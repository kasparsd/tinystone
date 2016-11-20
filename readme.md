# Eddystone Beacons Using nRF24l01+ and ATTiny

Create [Eddystone Beacons](https://github.com/google/eddystone) for the [Physical Web](https://github.com/google/physical-web) using nRF24l01+ radio modules and simple ATTiny microcontrollers. Broadcast up 12 byte encoded URLs.

![Eddystone Beacon in iOS Notification Center](http://kaspars.net/wp-content/uploads/2016/01/eddystone-beacon-iphone-notification-center.jpg)

## Build

The `Makefile` currently includes configuration for [ATTiny44](http://www.atmel.com/devices/ATTINY44.aspx) and an [USBasp](http://www.fischl.de/usbasp/) programmer.

## Notes

### BLE Service Advertisements

BLE service advertisement packets have the following format ([here is a great overview](https://developer.mbed.org/teams/Bluetooth-Low-Energy/code/BLE_EddystoneBeacon_Service/)):

	Preamble 1 byte | Access Address 4 bytes | Payload 2-39 bytes | CRC 3 bytes

where Payload:

	Header 2 bytes | MAC Address 6 bytes | Data <= 31 bytes

The non-connectable undirected advertising packets use the following values:

- Preamble is `0xAA`
- Access address is `0x8E89BED6`
- PDU header:
    - PDU Type `ADV_NONCONN_IND` or `0b0010`
    - Payload length
- PDU MAC Address
- PDU Data
    - Advertisement Flags (3 bytes)
    - Advertisement Services Header (4 bytes)
    - Advertisement Frames (the actual Eddystone frame)
- CRC


### Using nRF24l01+ for BLE

Turns out the nRF24l01+ uses the same modulation scheme and frequencies as BLE. The ShockBurst packet format is also very similar to the BLE advertising packet format:

	Preamble 1 byte | Address 3-5 byte | Payload 1-32 byte | CRC 1-2 byte

By using a 4 byte address and disabling the automatic CRC we're able to send a BLE advertising packet, however the 32 byte payload is reduced by 18 bytes:

- 2 byte PDU Header
- 6 byte MAC Address
- 3 byte Advertisement Service Flags
- 3 byte CRC

which leaves us with 18 bytes for the actual Eddystone frame payload.


### Eddystone

[Eddystone](https://github.com/google/eddystone) defines the structure of the Eddystone frames. For example, the [Eddystone-URL](https://github.com/google/eddystone/tree/master/eddystone-url) frame contains the following:

	Service Data Length 1 byte | Service Data Type 1 byte | Eddystone Service UUID 2 bytes | Frame Type 1 byte | TX Power 1 byte | URL Scheme 1 byte | Encoded URL 1-17 bytes

where

- Eddystone Service UUID is `0xFEAA`
- Frame type for Eddystone-URL is `0x10`
- TX power is the received power at 0 meters in dBm and the value ranges from -100 dBm to +20 dBm to a resolution of 1 dBm
- [URL Scheme](https://github.com/google/eddystone/tree/master/eddystone-url#url-scheme-prefix) is `0x03` for `https://`, for example
- [Encoded URL](https://github.com/google/eddystone/tree/master/eddystone-url#eddystone-url-http-url-encoding)

With 6 bytes used for the frame header we're left with only 12 bytes for the actual encoded URL.


### Notes on nRF24l01+ SPI Commands

- Every new command must be started by a high to low transition on CSN. In parallel to the SPI command word applied on the MOSI pin, the STATUS register is shifted serially out on the MISO pin.

- We use the regular or _non-Enhanced_ ShockBurst packet format which allows disabling the automatic CRC. Set the register `EN_AA = 0x00` and `ARC = 0` to disable Enhanced ShockBurst. In addition, the nRF24L01+ air data rate must be set to 1Mbps or 250kbps. Set `EN_CRC = 0` to disable the automatic CRC.


## Credits

[Kaspars Dambis](http://kaspars.net) based on the work by [Dmitry Grinberg](http://dmitry.gr) described in [Faking Bluetooth LE](http://dmitry.gr/index.php?r=05.Projects&proj=11.%20Bluetooth%20LE%20fakery).
