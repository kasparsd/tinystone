# Eddystone Beacons Using nRF24l01+ and ATTiny

Create [Eddystone Beacons](https://github.com/google/eddystone) for the [Physical Web](https://github.com/google/physical-web) using nRF24l01+ radio modules and simple ATTiny microcontrollers.

![Eddystone Beacon in iOS Notification Center](http://kaspars.net/wp-content/uploads/2016/01/eddystone-beacon-iphone-notification-center.jpg)

## Build

The `Makefile` currently includes configuration for [ATTiny44](http://www.atmel.com/devices/ATTINY44.aspx) and an [USBasp](http://www.fischl.de/usbasp/) programmer.

## Notes

### BLE Advertising Packet Format

BLE packet format:

	Preamble 1 byte | Access Address 4 bytes | Payload 2-39 bytes | CRC 3 bytes

where Payload:

	Header 2 bytes | Payload <= 37 bytes

where Header:

	PDU type 4 bits | RFU 2 bits | TxAdd 1 bit | RxAdd 1 bit | Length 6 bits | RFU 6 bits

and Payload:

	Broadcast Address 6 bytes | Broadcast Data 0 - 31 bytes


### BLE Advertising Packets

- Preamble is always `0xAA` for broadcast packets

- Address is always `0x8E89BED6` for broadcast packets

- PDU Types:

	- `ADV_IND` (`0b0000`) - Connectable undirected advertising event
	- `ADV_NONCONN_IND` (`0b0010`) - Non-connectable undirected advertising event
	- `ADV_SCAN_IND` (`0b0110`) - Scannable undirected advertising event


### Using nRF24l01+ for BLE

- In parallel to the SPI command word applied on the MOSI pin, the STATUS register is shifted serially out on the MISO pin.

- We need to use the _non-Enhanced_ ShockBurst packet format to mimic the BLE packets. Set the register `EN_AA = 0x00` and `ARC = 0` to disable Enhanced ShockBurst.

Here is the nRF24l01+ packet format -- notice the similarity with the BLE packet format above:

	Preamble 1 byte | Address 3-5 byte | Payload 1-32 byte | CRC 1-2 byte


### Eddystone-URL Packets

[Eddystone-URL](https://github.com/google/eddystone/tree/master/eddystone-url) defines the structure of the 32 byte payload inside the BLE advertising packet.


## Credits

[Kaspars Dambis](http://kaspars.net) based on the work by [Dmitry Grinberg](http://dmitry.gr) described in [Faking Bluetooth LE](http://dmitry.gr/index.php?r=05.Projects&proj=11.%20Bluetooth%20LE%20fakery).
