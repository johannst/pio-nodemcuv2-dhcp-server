// Copyright (c) 2022 Johannes Stoelp
//
// dhcp protocol: https://datatracker.ietf.org/doc/html/rfc2131
// dhcp options : https://datatracker.ietf.org/doc/html/rfc2132

#ifndef DHCP_H
#define DHCP_H

#include "types.h"

#include <optional>

// -- Global constants.

constexpr u16 DHCP_SERVER_PORT = 67;
constexpr u16 DHCP_CLIENT_PORT = 68;

constexpr usize DHCP_MESSAGE_LEN = 576;
constexpr usize DHCP_MESSAGE_MIN_LEN = 243;

constexpr u32 DHCP_OPTION_COOKIE = 0x63538263;

enum class dhcp_operation : u8 {
    BOOTREQUEST = 1,
    BOOTREPLY,
};

enum class dhcp_option : u8 {
    PAD = 0,
    END = 255,

    // Vendor Extensions.
    SUBNET_MASK = 1,
    ROUTER = 3,
    DNS = 6,

    // IP Layer Parameters per Interface.
    BROADCAST_ADDR = 28,

    // DHCP Extensions.
    REQUESTED_IP = 50,
    IP_ADDRESS_LEASE_TIME,
    OPTION_OVERLOAD,
    DHCP_MESSAGE_TYPE,
    SERVER_IDENTIFIER,
    PARAMETER_REQUEST_LIST,
    MESSAGE,
    MAX_DHCP_MESSAGE_SIZE,
    RENEWAL_TIME_T1,
    REBINDING_TIME_T2,
    CLASS_ID,
    CLIENT_ID,
};

// -- DHCP message.

enum class dhcp_message_type : u8 {
    DHCP_DISCOVER = 1,
    DHCP_OFFER,
    DHCP_REQUEST,
    DHCP_DECLINE,
    DHCP_ACK,
    DHCP_NAK,
    DHCP_RELEAE,
};

struct dhcp_message {
    dhcp_operation op;
    u8 htype;
    u8 hlen;
    u8 hops;
    u32 xid;
    u16 secs;
    u16 flags;
    u32 ciaddr;
    u32 yiaddr;
    u32 siaddr;
    u32 giaddr;
    u8 chaddr[16];
    u8 sname[64];
    u8 file[128];
    u32 cookie;
    u8 options[312 - 4 /* cookie len */];
} __attribute__((packed));

static_assert(DHCP_MESSAGE_MIN_LEN == offsetof(dhcp_message, options) + 3 /* DHCP_MESSAGE_TYPE size */,
              "Declare min size as dhcp_message with the DHCP_MESSAGE_TYPE option.");

// -- DHCP Utilities.

struct option_view {
    const u8* data;
    usize len;
};

// Search for the option with tag 'search_tag' in the options 'opt' of length 'len'.
std::optional<struct option_view> get_option(const u8* opt, usize len, dhcp_option search_tag);

template<typename T>
constexpr T get_opt_val(const u8* opt_data) {
    T ret = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        ret = (ret << 8) | *opt_data++;
    }
    return ret;
}

template<typename T>
u8* put_opt_val(u8* opt_data, T val) {
    for (size_t i = sizeof(T); i; --i) {
        *opt_data++ = (val >> ((i - 1) * 8)) & 0xff;
    }
    return opt_data;
}

#endif
