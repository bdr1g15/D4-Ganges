#include "../comms.h"

// Include the uart init() , transmit() and send_string() for printing data / for debuggig purpose
#define BAUD 9600                                   // define baud
#define BAUDRATE ((F_CPU)/(BAUD*16UL)-1)            // set baud rate value for UBRR

void Retrieve_data(uint8_t* type, uint16_t* data);
uint16_t Decode_data(uint8_t* type, uint16_t* data, uint16_t totalpacket);
#if ENCRYPTION_ENABLED
uint16_t Decrypt_data(uint16_t packet);
#endif

void init_uart1();
void uart_transmit(char data);
void send_string(char *str);