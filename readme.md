# Eddystone Beacons Using nRF24l01+ and ATTiny

BLE packet format:

	Preamble 1 byte | Access Address 4 bytes | Payload 2-39 bytes | CRC 3 bytes

where Payload:

	Header 2 bytes | Payload <= 37 bytes

where Header:

	PDU type 4 bits | RFU 2 bits | TxAdd 1 bit | RxAdd 1 bit | Length 6 bits | RFU 6 bits

where Payload:

	Broadcast Address 6 bytes | Broadcast Data 0 - 31 bytes


# Notes on BLE Advertising Packets

- Preamble is always `0xAA` for broadcast packets

- Address is always `0x8E89BED6` for broadcast packets

- PDU Types:

	- `ADV_IND` (`0b0000`) - Connectable undirected advertising event
	- `ADV_NONCONN_IND` (`0b0010`) - Non-connectable undirected advertising event
	- `ADV_SCAN_IND` (`0b0110`) - Scannable undirected advertising event


## Notes on nRF24l01+

- In parallel to the SPI command word applied on the MOSI pin, the STATUS register is shifted serially out on the MISO pin.

- We need to use the _non-Enhanced_ ShockBurst packet format to mimic the BLE packets. Set the register `EN_AA = 0x00` and `ARC = 0` to disable Enhanced ShockBurst.

		Preamble 1 byte | Address 3-5 byte | Payload 1-32 byte | CRC 1-2 byte