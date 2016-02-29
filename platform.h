/*
 * platform.h
 *
 *  Created on: Feb 29, 2016
 *      Author: barawn
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

#define UART1_NONE
#define UART0_8N1_38400_SMCLK_1MHZ
#include "msp430_uart_defs.h"

inline void platform_ui_uart_init() {
	msp430_eusci_uart0_init();
	P2SEL1 |= BIT0 | BIT1;
}

inline void platform_ui_uart_interrupt_enable() {
	UCA0IE |= UCRXIE;
}

#define UI_UART_VECTOR USCI_A0_VECTOR
#define UI_UART_IV	   UCA0IV

#endif /* PLATFORM_H_ */
