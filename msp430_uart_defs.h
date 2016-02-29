/*
 * msp430_uart_defs.h
 *
 *  Created on: Feb 9, 2016
 *      Author: barawn
 */

#ifndef MSP430_UART_DEFS_H_
#define MSP430_UART_DEFS_H_

// Big giant table of UART defines. These get added to as I need them.
#ifndef UART0_NONE
#if defined(UART0_8N1_9600_SMCLK_1MHZ)
// 9600 bps, 8N1, SMCLK clock source, 1 MHz
#define UART0_CTLW0 (UCSSEL_2)
#define UART0_BRW (6)
#define UART0_MCTLW ((UCOS16) | (UCBRF_8) | (UCBRS5))
#elif defined(UART0_8N1_38400_SMCLK_1MHZ)
// 38400 bps, 8N1, SMCLK clock source, 1 MHz
#define UART0_CTLW0 (UCSSEL_2)
#define UART0_BRW (1)
#define UART0_MCTLW ((UCOS16) | (UCBRF_10))
#elif defined(UART0_8N1_115200_SMCLK_8MHZ)
// 115200 bps, 8N1, SMCLK clock source, 8 MHz
#define UART0_CTLW0 (UCSSEL2)
#define UART0_BRW (4)
#define UART0_MCTLW ((UCOS16) | (UCBRF_5) | ((UCBRS6) | (UCBRS4) | (UCBRS2) | (UCBRS0)))
#else
#error UART0 parameters not defined or unknown! Check msp430_uart_defs.h for options or to add a new one.
#endif
#endif

#ifndef UART1_NONE
#if defined(UART1_8N1_9600_SMCLK_1MHZ)
// 9600 bps, 8N1, SMCLK clock source, 1 MHz
#define UART1_CTLW0 (UCSSEL_2)
#define UART1_BRW (6)
#define UART1_MCTLW ((UCOS16) | (UCBRF_8) | (UCBRS5))
#elif defined(UART1_8N1_38400_SMCLK_1MHZ)
// 38400 bps, 8N1, SMCLK clock source, 1 MHz
#define UART1_CTLW0 (UCSSEL_2)
#define UART1_BRW (1)
#define UART1_MCTLW ((UCOS16) | (UCBRF_10))
#elif defined(UART1_8N1_115200_SMCLK_8MHZ)
// 115200 bps, 8N1, SMCLK clock source, 8 MHz
#define UART1_CTLW0 (UCSSEL2)
#define UART1_BRW (4)
#define UART1_MCTLW ((UCOS16) | (UCBRF_5) | ((UCBRS6) | (UCBRS4) | (UCBRS2) | (UCBRS0)))
#else
#error UART1 parameters not defined or unknown! Check msp430_uart_defs.h for options or to add a new one.
#endif
#endif

static inline void msp430_eusci_uart0_init() {
#ifndef UART0_NONE
	UCA0CTLW0 |= UCSWRST;
	UCA0CTLW0 = UART0_CTLW0 | UCSWRST;
	UCA0BRW = UART0_BRW;
	UCA0MCTLW = UART0_MCTLW;
	UCA0CTLW0 &= ~UCSWRST;
#endif
}

static inline void msp430_eusci_uart1_init() {
#ifndef UART1_NONE
	UCA1CTLW0 |= UCSWRST;
	UCA1CTLW0 = UART1_CTLW0 | UCSWRST;
	UCA1BRW = UART1_BRW;
	UCA1MCTLW = UART1_MCTLW;
	UCA1CTLW0 &= ~UCSWRST;
#endif
}

#endif /* MSP430_UART_DEFS_H_ */
