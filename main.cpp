#include <msp430.h> 
#include "ui.h"
#include "clock.h"
#include "ipmiv2.h"
#include "sensors.h"
#include "ipmi_device_specific.h"

int main(void) {
	unsigned int cur_tick;
	unsigned int next_tick;
	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
    CSCTL0 = CSKEY; 						// Get access to the clock registers
    CSCTL3 &= ~(DIVM0 | DIVM1 | DIVM2);		// Run at 8 MHz.
    PM5CTL0 &= ~LOCKLPM5;

    next_tick = clock.ticks_per_second*5;
    ui.initialize();
    clock.initialize();
    ipmi.initialize();
    thisDevice.initialize();
    sensors.initialize();
    __enable_interrupt();
    while (1) {
    	// Reset the go-to-sleep register.
    	// Now if anyone interrupts us *while* we're in our loop, they'll clear this,
    	// and we won't go to sleep.
		asm("		MOV.B #0x18, r4");
		ui.process();
		ipmi.process();
		sensors.process();
/*		cur_tick = clock.ticks;
		// Dealing with the wraparound is a problem.
		// When we wrap around, next_tick will be *below*
		// cur_tick, but by a lot. We ignore those.
		if (cur_tick > next_tick) {
			if (cur_tick - next_tick < 0x8000) {
				ui.logprintln("MAIN> Time %u", cur_tick);
				next_tick = next_tick + clock.ticks_per_second*5;
			}
		}
*/
		asm("		OR.W r4, SR");
    	asm("		NOP");

    }


	return 0;
}
