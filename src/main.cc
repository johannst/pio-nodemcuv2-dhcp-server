// Copyright (c) 2022 Johannes Stoelp

#include <dhcp.h>
#include <lease_db.h>
#include <utils.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

/// -- WIFI access configuration.

static constexpr char STATION_SSID[] = "<SSID>";
static constexpr char STATION_WPA2[] = "<WPA2PW>";

/// -- WIFI client config.

static const IPAddress LOCAL_IP(10, 0, 0, 2);
static const IPAddress GATEWAY(10, 0, 0, 1);
static const IPAddress BROADCAST(10, 0, 0, 255);
static const IPAddress SUBNET(255, 255, 255, 0);
static const IPAddress DNS1(192, 168, 2, 1);

/// -- DHCP lease config.

static const IPAddress LEASE_START(10, 0, 0, 10);
static constexpr u32 LEASE_TIME_SECS = 8 * 60 * 60; /* 8h */

/// -- DHCP message buffer.

alignas(dhcp_message) static u8 MSG_BUFFER[DHCP_MESSAGE_LEN];

static_assert(sizeof(dhcp_message) <= sizeof(MSG_BUFFER), "UDP buffer must be big enough to hold dhcp_message!");

/// -- Lease DB.

static lease_db<16> LEASE_DB;

/// -- UDP io handler.

static WiFiUDP UDP;

/// -- Template specialization.

template<>
inline IPAddress get_opt_val(const u8* opt_data) {
    return IPAddress(opt_data[0], opt_data[1], opt_data[2], opt_data[3]);
}

template<>
inline u8* put_opt_val(u8* opt_data, IPAddress addr) {
    *opt_data++ = addr[0];
    *opt_data++ = addr[1];
    *opt_data++ = addr[2];
    *opt_data++ = addr[3];
    return opt_data;
}

#define LOG_UART(uart, fmt, ...)                  \
    do {                                          \
        if (uart) {                               \
            uart.printf(fmt "\r", ##__VA_ARGS__); \
        }                                         \
    } while (0)

#define LOG(fmt, ...) LOG_UART(Serial, fmt, ##__VA_ARGS__)

static void setup_station_wifi() {
    // Configure wifi in station mode.
    WiFi.mode(WIFI_STA);

    // Configure static IP.
    WiFi.config(LOCAL_IP, GATEWAY, SUBNET, DNS1);

    // Connect to SSID.
    WiFi.begin(STATION_SSID, STATION_WPA2);

    // Wait until wifi is connected.
    Serial.printf("Connecting to SSID = %s\n\r", STATION_SSID);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(500);
    }

    Serial.printf("\n\rConnected to %s\n\r", STATION_SSID);
    Serial.print("  local_ip : ");
    Serial.println(WiFi.localIP());
    Serial.print("  gateway  : ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("  subnet   : ");
    Serial.println(WiFi.subnetMask());
    Serial.print("  dns1     : ");
    Serial.println(WiFi.dnsIP());
    Serial.print("  broadcast: ");
    Serial.println(WiFi.broadcastIP());
}

void setup() {
    // Initialize serial port for logging.
    Serial.begin(115200);

    // Connect as client to wifi using global configuration.
    setup_station_wifi();

    // Start listening for udp messages.
    UDP.begin(DHCP_SERVER_PORT);

    pinMode(LED_BUILTIN, OUTPUT);
}

static void handle_dhcp_message(dhcp_message& msg, usize len);

void loop() {
    const usize npbytes = UDP.parsePacket();

    // Only handle UDP packets with valid size wrt dhcp messages.
    if (npbytes >= DHCP_MESSAGE_MIN_LEN && npbytes < sizeof(dhcp_message)) {
        const usize nrbytes = UDP.read(MSG_BUFFER, npbytes);

        // Type pun buffer to to dhcp_message (cpp).
        // Should be optimized out mainly due to alignment specification of buffer.
        dhcp_message msg;
        std::memcpy(&msg, MSG_BUFFER, sizeof(msg));

        handle_dhcp_message(msg, nrbytes);
    } else {
        if (npbytes > 0) {
            LOG("Ignored UDP message of size %d bytes\n", npbytes);
        }
        delay(500);
    }
}

// Try to unwrap an optional, return of optional doesn't hold a value.
#define TRY(expr)             \
    ({                        \
        auto optional = expr; \
        if (!optional)        \
            return;           \
        optional.value();     \
    })

// Get seconds since boot (absolute time value).
// We use this to maintain lease expiration times.
usize now_secs() {
    return millis() / 1000;
}

static void handle_dhcp_message(dhcp_message& msg, usize len) {
    // Sanity check dhcp message.
    if (msg.op != dhcp_operation::BOOTREQUEST || msg.cookie != DHCP_OPTION_COOKIE) {
        return;
    }

    // Length of the filled in client options.
    const usize opt_len = len - (msg.options - (const u8*)&msg);

    // Each dhcp message must contain the dhcp message type (state in the protocol).
    const auto msg_type = ({
        auto opt = TRY(get_option(msg.options, opt_len, dhcp_option::DHCP_MESSAGE_TYPE));
        from_raw<dhcp_message_type>(opt.data[0]);
    });

    // Remove expired leases.
    LEASE_DB.flush_expired(now_secs());

    // Compute client hash, using the CLIENT_ID option if available else use
    // the hardware address.
    u32 client_hash;
    if (const auto client_id = get_option(msg.options, opt_len, dhcp_option::CLIENT_ID)) {
        client_hash = hash(client_id->data, client_id->len);
    } else {
        client_hash = hash(msg.chaddr, msg.hlen);
    }

    // Extract the dhcp options requested by the client (using 16 was sufficient in my case).
    dhcp_option requested_param[16];
    usize requested_param_len = 0;
    if (const auto opt = get_option(msg.options, opt_len, dhcp_option::PARAMETER_REQUEST_LIST)) {
        requested_param_len = opt->len > sizeof(requested_param) ? sizeof(requested_param) : opt->len;

        for (usize i = 0; i < requested_param_len; ++i) {
            requested_param[i] = from_raw<dhcp_option>(opt->data[i]);
        }
    }

    usize lease_id;
    dhcp_message_type resp_msg;

    switch (msg_type) {
        case dhcp_message_type::DHCP_DISCOVER: {
            LOG("Received DHCP_DISCOVER client_hash=%x\n", client_hash);

            if (const auto lease = LEASE_DB.get_lease(client_hash)) {
                // We already have a lease for this client.
                lease_id = lease.value();
            } else {
                // Allocate a new lease for this client and reserve for a short
                // amount of time.
                lease_id = TRY(LEASE_DB.new_lease(client_hash, now_secs() + 15 /* secs */));
            }

            // DHCP message type answer.
            resp_msg = dhcp_message_type::DHCP_OFFER;
        } break;

        case dhcp_message_type::DHCP_REQUEST: {
            LOG("Received DHCP_REQUEST client_hash=%x\n", client_hash);

            // Get server identifier specified by client.
            const auto server_id = ({
                auto op = TRY(get_option(msg.options, opt_len, dhcp_option::SERVER_IDENTIFIER));
                get_opt_val<IPAddress>(op.data);
            });

            // Check if dhcp message was ment for us.
            if (server_id != LOCAL_IP) {
                return;
            }

            // Client is now requesting the offered lease, at that stage the
            // lease should have been allocated.
            lease_id = TRY(LEASE_DB.get_lease(client_hash));

            // Update the lease db with the proper lease expiration time
            // (absolute time).
            LEASE_DB.update_lease(client_hash, now_secs() + LEASE_TIME_SECS /* secs */);

            // DHCP message type answer.
            resp_msg = dhcp_message_type::DHCP_ACK;
        } break;

        default: {
            LOG("Received unexpected DHCP MESSAGE TYPE %d\n", into_raw(msg_type));
            return;
        }
    }

    // Craft response package.

    // Compute client id based on start address of dhcp range and lease idx.
    auto client_addr = LEASE_START;
    client_addr[3] = client_addr[3] + lease_id;

    // From rfc2131 Table 3:
    //
    // Field      DHCPOFFER   DHCPACK
    // -----      ---------   -------
    // 'op'       BOOTREPLY   BOOTREPLY
    // 'htype'    keep        keep
    // 'hlen'     keep        keep
    // 'hops'     0           0
    // 'xid'      keep        keep
    // 'secs'     0           0
    // 'ciaddr'   0           0
    // 'yiaddr'   IP address offered to client
    // 'siaddr'   IP address of next bootstrap server
    // 'flags'    keep        keep
    // 'giaddr'   keep        keep
    // 'chaddr'   keep        keep
    // 'sname'    Server host name or options
    // 'file'     Client boot file name or options

    msg.op = dhcp_operation::BOOTREPLY;
    msg.hops = 0;
    msg.secs = 0;
    msg.ciaddr = 0;
    msg.yiaddr = client_addr.v4();
    msg.siaddr = LOCAL_IP.v4();

    // From rfc2131 Table 3:
    //
    // Option                    DHCPOFFER   DHCPACK
    // ------                    ---------   -------
    // IP address lease time     MUST        MUST (DHCPREQUEST)
    // DHCP message type         DHCPOFFER   DHCPACK
    // Server identifier         MUST        MUST

    u8* optp = msg.options;

    // DHCP message type.
    *optp++ = into_raw(dhcp_option::DHCP_MESSAGE_TYPE);
    *optp++ = 1 /* len */;
    *optp++ = into_raw(resp_msg);

    // Server identifier.
    *optp++ = into_raw(dhcp_option::SERVER_IDENTIFIER);
    *optp++ = 4 /* len */;
    optp = put_opt_val(optp, LOCAL_IP);

    // Lease time.
    *optp++ = into_raw(dhcp_option::IP_ADDRESS_LEASE_TIME);
    *optp++ = 4 /* len */;
    optp = put_opt_val(optp, LEASE_TIME_SECS);

    // Renewal time.
    *optp++ = into_raw(dhcp_option::RENEWAL_TIME_T1);
    *optp++ = 4 /* len */;
    optp = put_opt_val(optp, LEASE_TIME_SECS / 2);

    // Rebind time.
    *optp++ = into_raw(dhcp_option::REBINDING_TIME_T2);
    *optp++ = 4 /* len */;
    optp = put_opt_val(optp, LEASE_TIME_SECS * 8 / 12);

    // Add options requested by client that we support.
    for (usize i = 0; i < requested_param_len; ++i) {
        auto opt = requested_param[i];
        switch (opt) {
            case dhcp_option::SUBNET_MASK: {
                // Subnet mask.
                *optp++ = into_raw(opt);
                *optp++ = 4 /* len */;
                optp = put_opt_val(optp, SUBNET);
            } break;

            case dhcp_option::ROUTER: {
                // Router address.
                *optp++ = into_raw(opt);
                *optp++ = 4 /* len */;
                optp = put_opt_val(optp, GATEWAY);
            } break;

            case dhcp_option::DNS: {
                // DNS address.
                *optp++ = into_raw(opt);
                *optp++ = 4 /* len */;
                optp = put_opt_val(optp, DNS1);
            } break;

            case dhcp_option::BROADCAST_ADDR: {
                // Broadcast address.
                *optp++ = into_raw(opt);
                *optp++ = 4 /* len */;
                optp = put_opt_val(optp, BROADCAST);
            } break;

            default:
                break;
        }
    }

    // End option end marker.
    *optp++ = into_raw(dhcp_option::END);

    // Send out dhcp message.
    UDP.beginPacket(BROADCAST, DHCP_CLIENT_PORT);
    UDP.write((const u8*)&msg, optp - (u8*)&msg);
    UDP.endPacket();
}
