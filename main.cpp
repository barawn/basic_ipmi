#include <msp430.h> 
#include "ui.h"
#include "clock.h"
#include "ipmiv2.h"
#include "sensors.h"
#include "ipmi_device_specific.h"
#include "twi.h"

unsigned char i2c_buf[2];

void main(void) {
	unsigned int next_tick;
	enum state {
		state_IDLE = 0,
		state_WRITE = 2,
		state_WRITE_WAIT = 4,
		state_READ = 6,
		state_READ_WAIT = 8
	};
	enum state cur_state = state_WRITE;

	WDTCTL = WDTPW | WDTHOLD;	// Stop watchdog timer
    CSCTL0 = CSKEY; 						// Get access to the clock registers
    CSCTL3 &= ~(DIVM0 | DIVM1 | DIVM2);		// Run at 8 MHz.
    PM5CTL0 &= ~LOCKLPM5;

    next_tick = clock.ticks_per_second*5;
    ui.initialize();
    clock.initialize();
    ipmi.initialize();
    twi.initialize();
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
		twi.process();
		sensors.process();
		asm("		OR.W r4, SR");
    	asm("		NOP");

    }
}
