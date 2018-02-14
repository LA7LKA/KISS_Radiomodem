#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "device.h"
#include "util/FIFO.h"
#include "util/time.h"
#include "hardware/AFSK.h"
#include "hardware/Serial.h"
#include "protocol/AX25.h"
#include "hardware/softuart.h"

#if SERIAL_PROTOCOL == PROTOCOL_KISS
    #include "protocol/KISS.h"
#endif

#if SERIAL_PROTOCOL == PROTOCOL_SIMPLE_SERIAL
    #include "protocol/SimpleSerial.h"
#endif

Serial serial;
Afsk modem;
AX25Ctx AX25;

#if SERIAL_PROTOCOL == PROTOCOL_KISS
    static void ax25_callback(struct AX25Ctx *ctx) {
        kiss_messageCallback(ctx);
    }
#endif

#if SERIAL_PROTOCOL == PROTOCOL_SIMPLE_SERIAL
    static uint8_t serialBuffer[AX25_MAX_FRAME_LEN+1];
    static int sbyte;
    static size_t serialLen = 0;
    static bool sertx = false;

    static void ax25_callback(struct AX25Msg *msg) {
        ss_messageCallback(msg);
    }
#endif

void init(void) {
    sei();

    AFSK_init(&modem);
    ax25_init(&AX25, &modem.fd, ax25_callback);

    serial_init(&serial);    
    stdout = &serial.uart0;
    stdin  = &serial.uart0;

    #if SERIAL_PROTOCOL == PROTOCOL_KISS
        kiss_init(&AX25, &modem, &serial);
    #endif

    #if SERIAL_PROTOCOL == PROTOCOL_SIMPLE_SERIAL
        ss_init(&AX25);
    #endif
}

int main (void) {

	char c;
	static const char pstring[] PROGMEM = 
		"adapted for Atmel AVR and this demo by Martin Thomas\r\n";
	unsigned short cnt = 0;
#if (F_CPU > 4000000UL)
#define CNTHALLO (unsigned int)(0xFFFF)
#else 
#define CNTHALLO (unsigned int)(0xFFFF/3)
#endif

	softuart_init();
	softuart_turn_rx_on(); /* redundant - on by default */
	
	sei();

	softuart_puts_P( "\r\nSoftuart Demo-Application\r\n" );    // "implicit" PSTR
	softuart_puts_p( PSTR("generic softuart driver code by Colin Gittins\r\n") ); // explicit PSTR
	softuart_puts_p( pstring ); // pstring defined with PROGMEM
	softuart_puts( "--\r\n" );  // string "from RAM"


    init();
	DDRC |= _BV(1);
	PORTC |= _BV(1);

    #if SERIAL_PROTOCOL == PROTOCOL_KISS
        while (true) {
            ax25_poll(&AX25);
            
            if (serial_available(0)) {
                char sbyte = uart0_getchar_nowait();
                kiss_serialCallback(sbyte);
            }
        }
    #endif

    #if SERIAL_PROTOCOL == PROTOCOL_SIMPLE_SERIAL
        ticks_t start = timer_clock();
        while (1) {    
            ax25_poll(&AX25);

            if (!sertx && serial_available(0)) {
                sbyte = uart0_getchar_nowait();

                #if SERIAL_DEBUG
                    if ((serialLen < AX25_MAX_FRAME_LEN) && (sbyte != 10)) {
                        serialBuffer[serialLen] = sbyte;
                        serialLen++;
                    } else {
                        sertx = true;
                    }
                #else
                    if (serialLen < AX25_MAX_FRAME_LEN-1) {
                        serialBuffer[serialLen] = sbyte;
                        serialLen++;
                    } else {
                        serialBuffer[serialLen] = sbyte;
                        serialLen++;
                        sertx = true;
                    }

                    start = timer_clock();
                #endif
            } else {
                if (!SERIAL_DEBUG && serialLen > 0 && timer_clock() - start > ms_to_ticks(TX_MAXWAIT)) {
                    sertx = true;
                }
            }

            if (sertx) {
                ss_serialCallback(serialBuffer, serialLen, &AX25);
                sertx = false;
                serialLen = 0;
            }

        }
    #endif

    return(0);
}
