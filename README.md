# pio-nodemcuv2-dhcp-server

This repository implements a minimal [dhcp server][dhcp] on the [nodemcu v2]
board using the [arduino] framework and the [PlatformIO][pio] build
environment.


## DHCP protocol

All details of the dhcp protocol required for this implementation can be found
in the following two RFCs
- [rfc2131]: Dynamic Host Configuration Protocol
- [rfc2132]: DHCP Options and BOOTP Vendor Extensions

## Run

Install PlatformIO core following the [installation guide][pio-install].

```shell
# Build the project.
pio run

# Run the test (only added host native tests).
pio test

# ... Attach nodemcu v2 board via USB.

# Build and flash the embedded software.
pio run -e nodemcuv2 -t upload

# Connect to the serial monitor.
pio device monitor -b 115200
```

> Check the [Makefile](Makefile) for reference.

## Configuration

The wifi client and dhcp server are currently configured with the following
global variables in [main.cc](src/main.cc).

```cpp
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
```

## Why all this?

My ultimate goal was to setup a **guest wifi** to isolate my home network while
offering wifi to my guests.
For that I planned to use of the baked in guest wifi feature of the router I
got from my ISP.

In my home network I am running [pi-hole] for blocking ads and trackers, which
works by blocking those services on the DNS level. For this to function, all
the devices in my home network need to use the pi-hole node as DNS server.
This is typically done by letting the dhcp server distribute the DNS server to
use (so it does not need to be configured on each device).
Because the built-in dhcp server in the ISP router doesn't allow to configure
the DNS server I disabled that one and run the optional dhcp server which comes
with pi-hole.

With that setup as starting point, one problem arises when using the guest wifi
on the ISP router. Guest devices connected to the guest wifi don't see a dhcp
server and hence don't get an IP address assigned. In turn, the setup is
useless as I don't want guests to statically configure an IP address :^)

Therefore the plan was to write a minimal dhcp server running on a [nodemcu v2]
which would then linger in the guest wifi and distribute IP addresses to my
guests.

> Why a nodemcu v2? Because it has an wireless antenna and I had one laying
> around collecting dust.

So all in all my home setup would look something like the following.

```
   Home     +-----------+     Guest     +---------------+
   Wifi (((-| ISP modem |-))) Wifi  (((-| nodemcuv2     |
            |  & router |               | (dhcp server) |
            +-----+-----+               +---------------+
                  |
          +-------+-------+
          | pihole        |
          | (dhcp server) |
          +---------------+
```

## End of the story ...

... I read the RFCs, developed & tested the dhcp server in my home network and
then learned that the guests are isolated and I couldn't disable that isolation
in the ISP router.

At the end I couldn't deploy the guest wifi as planned but I had fun
implementing the dhcp server and I guess learned something :^)

For now I switched over to offering guest wifi by running an access point on
the raspberry pi spanning a new sub-net and then doing NAT and iptables
isolation to protect my home network. It's not ideal but it's sufficient for
now.

```
   Home     +-----------+ 
   Wifi (((-| ISP modem |
            |  & router |
            +-----+-----+
                  |
          +-------+-------+----------+    Guest
          | pihole        | NAT      |-))) Wifi
          | (dhcp server) | iptables |
          +---------------+----------+
```

## License

This project is licensed under the [MIT](LICENSE) license.

[dhcp]: https://en.wikipedia.org/wiki/Dynamic_Host_Configuration_Protocol
[rfc2131]: https://datatracker.ietf.org/doc/html/rfc2131
[rfc2132]: https://datatracker.ietf.org/doc/html/rfc2132
[pio]: https://platformio.org
[pio-install]: https://docs.platformio.org/en/latest//core/installation.html
[arduino]: https://docs.platformio.org/en/latest/frameworks/arduino.html
[pi-hole]: https://pi-hole.net
[nodemcu v2]: https://www.az-delivery.de/en/products/nodemcu
