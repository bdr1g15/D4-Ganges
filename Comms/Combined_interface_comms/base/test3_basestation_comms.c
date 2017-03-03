// This code is now going to involve taking the copy of basestation_comms.c and adding the adc bits from base_station_controller.c
//  date: 1/3/17
// Arthur: Mohammed Ibrahim and Joel
// D4-Ganges!!!!!
// This code is a aprt of test 1 I mentioned in my pink book
#include <avr/io.h>
#include <stdio.h>// for sprintf()
#include <avr/interrupt.h>
#include <util/delay.h>
#include <math.h>

#include "rfm12.h"
#include "test3_basestation_comms.h"

volatile uint8_t tx_encryption_key;

int main(void)
{
	// Initialise rfm12, adc, uart and interrupts
	rfm12_init();
	adc_init();
	init_uart1();
	sei();

	tx_encryption_key = 5;

	// Hold received data
	uint8_t receivedpackettype;
	uint16_t receiveddata;

	// Send test data
	uint16_t testdata;
	testdata = 0;

	while (1)
	{
		rfm12_tick();
		#if	ENABLE_CONTROLS
			Send_data(OP_ROLL, testdata);
			testdata++;
			if (testdata == 1024) testdata = 0;
			_delay_ms(50);

			char txString[100];
			sprintf(txString, "Transmitting: packet type %u and data %u\n", OP_ROLL, testdata);
			send_string(txString);
		#endif

		if (rfm12_rx_status() == STATUS_COMPLETE)
		{
			// Get the received packet type and data
			receivedpackettype = rfm12_rx_type();
			receiveddata = *rfm12_rx_buffer();

			// Decrypt (if enabled) and extract 10-bit data and packet type from the received packet 
			Retrieve_data(&receivedpackettype, &receiveddata);

		#if UPLINK_TEST
			// Send data to UART
			char rxString[100];
			sprintf(rxString, "Received: packet type %u and data %u\n", receivedpackettype, receiveddata);
			send_string(rxString);
		#endif
		}
	}
	
}

void Retrieve_data(uint8_t* type, uint16_t* data)
{
	// Combine packet type and data into a single 16-bit int
	uint16_t totalpacket;
	totalpacket = *type;
	totalpacket = (totalpacket << DATA_BIT_SIZE) + *data;

#if ENCRYPTION_ENABLED
	// Decrypt the received packet
	totalpacket = Decrypt_data(totalpacket);
#endif // ENCRYPTION_ENABLED


	// Split the decrypted packet into the data and the packet type
	Decode_data(type, data, totalpacket);
}

void Decode_data(uint8_t* type, uint16_t* data, uint16_t totalpacket)
{
	// Get 10-bit data from the 16 bit packet
	*data = totalpacket & (uint16_t)1023;

	// Get packet type
	*type = (totalpacket >> DATA_BIT_SIZE);
}

uint16_t Decrypt_data(uint16_t packet)
{
	// Retrieve the encryption key
	uint8_t rx_encryption_key;
	rx_encryption_key = (packet >> (DATA_BIT_SIZE + COMMAND_BIT_SIZE));

	// Retrieve bits that are shifted out when the left shift is done
	uint8_t rotated_out_bits;
	rotated_out_bits = (packet >> (DATA_BIT_SIZE + COMMAND_BIT_SIZE - rx_encryption_key));

	// Get completely rotated bits by adding the shifted out bits to the
	// original packet left-shifted by the required number of bits.
	// It is & with a sequence of 1s to remove the encryption key from the overall packet
	uint16_t decrypted_packet;
	decrypted_packet = ((packet << rx_encryption_key) & ((uint16_t)pow(2, DATA_BIT_SIZE + COMMAND_BIT_SIZE) - 1)) + rotated_out_bits;

	return decrypted_packet;
}

void Send_data(uint8_t type, uint16_t data)
{
	// Combine packet type and data into a single 16-bit int
	uint16_t totalpacket;
	totalpacket = type;
	totalpacket = (totalpacket << DATA_BIT_SIZE) + data;

	// Encrypt data
	#if ENABLE_ENCRYPTION
		totalpacket = Encrypt_data(totalpacket);
	#endif

	// Split 16-bit packet into two 8-bit ints - packet type and data
	uint8_t datapacket;
	Encode_data(&type, &datapacket, totalpacket);

	// Send packet to the buffer for transmission
	rfm12_tx(sizeof(datapacket), type, &datapacket);
}

void Encode_data(uint8_t* type, uint8_t* data, uint16_t totalpacket)
{
	// Data is equal to the 8 LSBs
	*data = totalpacket;

	// Type, encryption key and 2 bits of data are held in the 8 MSBs
	*type = (totalpacket >> DATA_BIT_SIZE);
}

#if ENABLE_ENCRYPTION
uint16_t Encrypt_data(uint16_t packet)
{
	// Retrieve bits that are shifted out when the right shift is done
	uint8_t rotated_out_bits;
	rotated_out_bits = (packet & ((uint8_t) pow(2, tx_encryption_key) - 1));

	// Get completely rotated bits by adding the shifted out bits to the
	// original packet right-shifted by the required number of bits.
	uint16_t encrypted_packet;
	encrypted_packet = (packet >> tx_encryption_key) + (rotated_out_bits << (COMMAND_BIT_SIZE + DATA_BIT_SIZE - tx_encryption_key));

	// Add on the encryption key to the MSBs of the packet
	encrypted_packet = encrypted_packet + (tx_encryption_key << (COMMAND_BIT_SIZE + DATA_BIT_SIZE));

	// Adjust encryption key for next transmission
	tx_encryption_key = (tx_encryption_key < 3) ? tx_encryption_key + 5 : tx_encryption_key - 3;
	if (tx_encryption_key == 0) tx_encryption_key = 5;

	return encrypted_packet;
}
#endif

// include adc function
uint16_t adc_read(int n)//[1]
{
	ADMUX = n;// represents PA2
			  // start conversion
	ADCSRA |= _BV(ADSC);
	// wait for conversion to complete
	//while(!(ADCSRA & _BV(ADIF))){};
	while (ADCSRA & _BV(ADSC));
	ADC = (ADCH << 8) | ADCL;// [1]
	return ADC;
}

void send_string(char *str)
{
	int i;
	for (i = 0; str[i]; i++) uart_transmit(str[i]);
}

void init_uart1()// initialize UART
{
	//1. set the baud rate, lets configure to 9600;
	// set the baud rate registers Ref: [1],[2]
	UBRR0H = BAUDRATE >> 8;// UBRRnH is 8 bits left
	UBRR0L = BAUDRATE;

	//2. setting up data packet: 8 bits ,no parity 1 stop bit
	// setting 8 bits got to UCSCR register Ref:[3], pg 185 of data sheet

	UCSR0C = _BV(UCSZ00) | _BV(UCSZ01); // 8 bits, USBS1 = 0 for 1 stop bit

										// note: havnt set up the stop bit in Ref [2] slides
										// 3. from Ref[2] we now enable Transmission and receive n UCSRnB register
	UCSR0B = _BV(TXEN0) | _BV(RXEN0);

}

// transmit data function
void uart_transmit(char data)
{
	while (!(UCSR0A &  _BV(UDRE0))); //  data register enable bit is 1 if tx buffer is empy
									 // if its 1 we load data onto UDR- Uart Data Register(buffer)
	UDR0 = data;
}

// Include adc_init()
void adc_init()//[1]
{

	// In ADCSRA Enable ADC (set ADEN) and prescaler of 64
	ADCSRA |= _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1);
}