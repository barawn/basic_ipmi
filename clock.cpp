#include <msp430.h>
#include "clock.h"

Clock clock;

volatile unsigned int Clock::ticks = 0;

void Clock::initialize() {
	// Set up the watchdog timer, and set it running.
	// That's all we need to do. The system wakes up every watchdog tick (~32 ms) and polls processes.

	// Watchdog trips over every 32768 SMCLK ticks (e.g. 32768 us).
	WDTCTL = WDTPW | WDT_MDLY_32 | WDTTMSEL | WDTCNTCL;

	// This is sometimes called SFRIFG1, and sometimes IFG1, and they didn't do a compatibility define.
	SFRIFG1 &= ~WDTIFG;
	SFRIE1 |= WDTIE;
}

#pragma vector = WDT_VECTOR
__interrupt void
WDT_Handler() {
	clock.ticks++;
	// Reset the global "can I go to sleep" register.
	asm("	mov.b	#0x00, r4");
	__bic_SR_register_on_exit(LPM0_bits);
}
