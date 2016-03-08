/*
 * platform.h
 *
 *  Created on: Feb 29, 2016
 *      Author: barawn
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

#define UART0_NONE
#define UART1_8N1_38400_SMCLK_1MHZ
#include "msp430_uart_defs.h"

inline void platform_ui_uart_init() {
	msp430_eusci_uart1_init();

	P3SEL0 |= BIT4 | BIT5;
}

inline void platform_ui_uart_interrupt_enable() {
	UCA1IE |= UCRXIE;
}

inline unsigned char *platform_tag_find(unsigned char tag) {
	unsigned char *p;
	p = (unsigned char *) TLV_START;
	while (*p != tag) {
		unsigned char len;
		p++;
		len = *p++;
		p += len;
	}
	return p;
}

#define UI_UART_VECTOR USCI_A1_VECTOR
#define UI_UART_IV	   UCA1IV
#define UI_UART_DMA_RX_TRIGGER 16
#define UI_UART_DMA_TX_TRIGGER 17
#define UI_UART_TXBUF UCA1TXBUF
#define UI_UART_RXBUF UCA1RXBUF

#endif /* PLATFORM_H_ */
