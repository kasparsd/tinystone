#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#define F_CPU	8000000
#include <util/delay.h>

// @todo replace these with something nice
// Clear Bit
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
// Set Bit
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#define PIN_CE	PA1 // CE
#define PIN_nCS	PA2 // CSN
#define PIN_IRQ PA3 // IRQ
#define PIN_SCK PA4 // SCK
#define PIN_DO  PA5 // USI DO or MISO
#define PIN_LED PA7 // status LED

#define MY_MAC_0	0xEF
#define MY_MAC_1	0xFF
#define MY_MAC_2	0xC0
#define MY_MAC_3	0xAA
#define MY_MAC_4	0x18
#define MY_MAC_5	0x00

void btLeCrc(const uint8_t* data, uint8_t len, uint8_t* dst){
	uint8_t v, t, d;

	while(len--){
		d = *data++;
		for(v = 0; v < 8; v++, d >>= 1){
			t = dst[0] >> 7;
			dst[0] <<= 1;

			if(dst[1] & 0x80)
				dst[0] |= 1;

			dst[1] <<= 1;

			if(dst[2] & 0x80)
				dst[1] |= 1;

			dst[2] <<= 1;

			if(t != (d & 1)){
				dst[2] ^= 0x5B;
				dst[1] ^= 0x06;
			}
		}
	}
}

uint8_t swapbits(uint8_t a){
	uint8_t v = 0;

	if(a & 0x80) v |= 0x01;
	if(a & 0x40) v |= 0x02;
	if(a & 0x20) v |= 0x04;
	if(a & 0x10) v |= 0x08;
	if(a & 0x08) v |= 0x10;
	if(a & 0x04) v |= 0x20;
	if(a & 0x02) v |= 0x40;
	if(a & 0x01) v |= 0x80;

	return v;
}

void btLeWhiten(uint8_t* data, uint8_t len, uint8_t whitenCoeff){
	uint8_t  m;

	while(len--){
		for(m = 1; m; m <<= 1){
			if(whitenCoeff & 0x80){
				whitenCoeff ^= 0x11;
				(*data) ^= m;
			}
			whitenCoeff <<= 1;
		}
		data++;
	}
}

static inline uint8_t btLeWhitenStart(uint8_t chan){
	//the value we actually use is what BT'd use left shifted one...makes our life easier
	return swapbits(chan) | 2;
}

void btLePacketEncode(uint8_t* packet, uint8_t len, uint8_t chan){
	//length is of packet, including crc. pre-populate crc in packet with initial crc value!
	uint8_t i, dataLen = len - 3;

	btLeCrc(packet, dataLen, packet + dataLen);

	for(i = 0; i < 3; i++, dataLen++)
		packet[dataLen] = swapbits(packet[dataLen]);

	btLeWhiten(packet, len, btLeWhitenStart(chan));

	for(i = 0; i < len; i++)
		packet[i] = swapbits(packet[i]);
}

uint8_t spi_byte(uint8_t byte){
  USIDR = byte; // write byte to the USI register
  USISR = (1<<USIOIF); // clear the USI counter overflow flag

  while (!(USISR & (1<<USIOIF))){ // while no clock overflow
	USICR = (1<<USIWM0)|(1<<USICS1)|(1<<USICLK)|(1<<USITC);
	//USICR |= (1<<USITC); // toggle clock pin (send bit)
  }

  return USIDR; // return USI data received
}

void nrf_cmd(uint8_t cmd, uint8_t data)
{
	cbi(PORTA, PIN_nCS);
	spi_byte(cmd + 0x20); // 0x20 padding is needed to write to STATUS register.
	spi_byte(data);
	sbi(PORTA, PIN_nCS);
}

void nrf_simplebyte(uint8_t cmd)
{
	cbi(PORTA, PIN_nCS);
	spi_byte(cmd);
	sbi(PORTA, PIN_nCS);
}

void nrf_manybytes(uint8_t* data, uint8_t len)
{
	cbi(PORTA, PIN_nCS);
	do{
		spi_byte(*data++);
	}while(--len);
	sbi(PORTA, PIN_nCS);
}

void ports_setup(void)
{
	// Configure USI: three wire mode | external positive edge| software clock strobe
	// USICR |= (1<<USIWM0)|(1<<USICS1)|(1<<USICLK);

	// Set SCN, CE, LED, SCK and USI DO pins as output
	DDRA |= (1<<PIN_nCS)|(1<<PIN_CE)|(1<<PIN_LED)|(1<<PIN_SCK)|(1<<PIN_DO);

	TCCR0A = (1<<CS00); // clock select

	cbi(PORTA, PIN_CE); // Clear CE, turn off RX and TX
	sbi(PORTA, PIN_nCS); // Set CSN
}

int main(void)
{
	//static const uint8_t chLe[] = {37,38,39}; // 2402 MHz, 2426 MHz, and 2480 MHz
	static const uint8_t chRf[] = {2,26,80}; // F0 = 2400 + RF_CH [MHz]

	uint8_t i, L, ch = 0;
	uint8_t buf[32];
	uint8_t deb[32];

	ports_setup();

	// nRF24L01+ must be in a standby or power down mode before writing to the configuration registers.

	// CONFIG: MASK_RX_DR=0, MASK_TX_DS=0, MASK_MAX_RT=1, EN_CRC=0, EN_CRC=0, PWR_UP=1, PRIM_RX=0
	// Reflect TX_DS as active low interrupt on the IRQ pin
	nrf_cmd(0x00, 0x12); // 0x12

	// EN_AA: no auto-acknowledge
	nrf_cmd(0x01, 0x00);

	// EN_RXADDR: RX on pipe 0
	nrf_cmd(0x02, 0x00);

	// SETUP_AW: use a 4 byte address (default is 5 bytes)
	nrf_cmd(0x03, 0x02);

	// SETUP_RETR: no auto-retransmit (ARC=0)
	nrf_cmd(0x04, 0x00);

	// RF_SETUP: 1MBps at 0dBm
	nrf_cmd(0x06, 0x06);

	// STATUS: Clear TX_DS and MAX_RT
	nrf_cmd(0x07, 0x3E);

	//nrf_cmd(0x11, 0x20);	// RX_PW_P0: always RX 32 bytes
	//nrf_cmd(0x11, 0x00);	// RX_PW_P0: set RX_PW_P0=0 (not used)
	//nrf_cmd(0x1C, 0x00);	// DYNPD: no dynamic payloads
	//nrf_cmd(0x1D, 0x00);	// FEATURE: no features

	// Set TX address always, 0x8E89BED6 or "Bed6" for advertising packets
	buf[0] = 0x10 + 0x20; // 0x20 padding to specify the write register
	buf[1] = swapbits(0x8E);
	buf[2] = swapbits(0x89);
	buf[3] = swapbits(0xBE);
	buf[4] = swapbits(0xD6);
	nrf_manybytes(buf, 5);

	// Set RX address, not used
	buf[0] = 0x0A + 0x20; // 0x20 padding to specify the write register
	nrf_manybytes(buf, 5);

	while(1){

		PORTA |= (1<<PIN_LED); // Turn on LED

		L = 0;

		// Create our ADV_NONCONN_IND packet:
		// 2 byte header + 6 byte address + 21 byte payload

		buf[L++] = 0x40; // PDU type, ADV_NONCONN_IND.
		buf[L++] = 11; // 17 bytes of payload minus 6 bytes for MAC

		buf[L++] = MY_MAC_0;
		buf[L++] = MY_MAC_1;
		buf[L++] = MY_MAC_2;
		buf[L++] = MY_MAC_3;
		buf[L++] = MY_MAC_4;
		buf[L++] = MY_MAC_5;

		// LE-only , limited discovery
		buf[L++] = 2; // chunk size: 2
		buf[L++] = 0x01; // chunk type: device flags
		buf[L++] = 0x05; // flags (LE-only, limited discovery mode)

		buf[L++] = 7;
		buf[L++] = 0x08;
		buf[L++] = 'n';
		buf[L++] = 'R';
		buf[L++] = 'F';
		buf[L++] = ' ';
		buf[L++] = 'L';
		buf[L++] = 'E';

		// ... insert more data here
		//buf[L++] = 2; // length of custom data, including type byte
		//buf[L++] = 0xff; // TYPE_CUSTOMDATA
		//buf[L++] = 8;

		buf[L++] = 0x55; // CRC start value: 0x555555
		buf[L++] = 0x55;
		buf[L++] = 0x55;

		// Channel hopping
		//if (++ch == sizeof(chRf))
		//	ch = 0;

		btLePacketEncode(buf, L, chRf[ch]);

		nrf_cmd(0x05, chRf[ch]); // set the channel, loop through chRf
		nrf_cmd(0x07, 0x6E); // clear flags

		// Clear RX, TX FIFO before writing the payload
		nrf_simplebyte(0xE1); // FLUSH_RX TX_FIFO (0b11100001)
		nrf_simplebyte(0xE2); // FLUSH_RX RX_FIFO (0b11100010)

		// Write payload to the module
		cbi(PORTA, PIN_nCS);
		spi_byte(0xA0); // W_TX_PAYLOAD
		for(i = 0 ; i < L ; i++)
			spi_byte(buf[i]);
		sbi(PORTA, PIN_nCS);

		// Toggle CE to send the buffer
		sbi(PORTA, PIN_CE);	// Set CE
		_delay_us(20); // Min 10us for TX to be sent
		cbi(PORTA, PIN_CE); // Clear CE

		PORTA &= ~(1<<PIN_LED); // Turn off LED
		_delay_ms(100);

	}

	return 0;
}
