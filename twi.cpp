#include <msp430.h>
#include "twi.h"
#include "ui.h"

Twi twi;

Twi::twi_state_t Twi::twi_state = Twi::state_IDLE;
#pragma NOINIT
Twi::twi_result_t Twi::twi_result;
#pragma NOINIT
unsigned char Twi::slave_register[4];
#pragma NOINIT
unsigned char *Twi::buf;
#pragma NOINIT
unsigned char Twi::nbytes;
#pragma NOINIT
unsigned char Twi::slave_addr;
#pragma NOINIT
unsigned char Twi::slave_register_len;
#pragma NOINIT
Twi::twi_transaction_t Twi::twi_transaction;


void Twi::initialize() {
	UCB1CTLW0 = UCSWRST;
	P3SEL0 |= BIT1 | BIT2;
	P3SEL1 &= ~(BIT1 | BIT2);
	P3REN |= (BIT1 | BIT2);
	P3OUT |= (BIT1 | BIT2);
	UCB1CTLW0 = UCSWRST | UCSSEL_2 | UCMODE_3 | UCSYNC | UCMST;
	// 250 kHz.
	UCB1BRW = 4;
	UCB1CTLW0 &= ~UCSWRST;
}

void Twi::process() {
Twi_process_begin:
	switch(__even_in_range(twi_state, state_MAX)) {
	case state_IDLE: return;
	case state_BEGIN:
		if (UCB1STAT & UCBBUSY) {
			asm("		MOV.B #0x00, r4");
			return;
		}
		switch(__even_in_range(twi_transaction, transaction_MAX)) {
		case transaction_NONE: twi_state = state_IDLE; return;
		case transaction_WRITE:
			twi_result = result_OK;
			twi_state = state_WRITE;
			// Put in reset.
			UCB1CTLW0 |= UCSWRST;
			// Write transaction. Initialize address
			UCB1I2CSA = slave_addr;
			// Set DMA destination address.
			DMA2DA = (__SFR_FARPTR) (unsigned long) &UCB1TXBUF;
			// Set DMA source address.
			DMA2SA = (__SFR_FARPTR) (unsigned long) buf;
			// Set DMA size.
			DMA2SZ = nbytes;
			// Transmitter mode.
			UCB1CTLW0 |= UCTR;
			// Autostop after nbytes.
			UCB1CTLW1 = UCASTP_2;
			UCB1TBCNT = nbytes;
			// Pull out of reset.
			UCB1CTLW0 &= ~UCSWRST;
			// Enable byte counter interrupt.
			UCB1IE = UCBCNTIE | UCALIE | UCNACKIE;
			// Set DMA trigger.
			DMACTL1 = (DMACTL1 & 0xFF00) | 25;
			// Set DMA parameters.
			DMA2CTL = DMALEVEL | DMASRCBYTE | DMADSTBYTE | DMASRCINCR_3 | DMADSTINCR_0 | DMADT_0;
			// Begin transmission.
			UCB1CTLW0 |= UCTXSTT;
			// Enable DMA.
			DMA2CTL |= DMAEN;
			return;
		case transaction_READ:
			twi_result = result_OK;
			twi_state = state_READ;
			// Put in reset.
			UCB1CTLW0 |= UCSWRST;
			// Write transaction. Initialize address
			UCB1I2CSA = slave_addr;
			// Set DMA destination address.
			DMA2DA = (__SFR_FARPTR) (unsigned long) buf;
			// Set DMA source address.
			DMA2SA = (__SFR_FARPTR) (unsigned long) &UCB1RXBUF;
			// Set DMA size.
			DMA2SZ = nbytes;
			// Receiver mode.
			UCB1CTLW0 &= ~UCTR;
			// Autostop after nbytes.
			UCB1CTLW1 = UCASTP_2;
			UCB1TBCNT = nbytes;
			// Pull out of reset.
			UCB1CTLW0 &= ~UCSWRST;
			// DON'T enable byte counter interrupt.
			// Byte counter interrupt goes off too early. It'll disable DMA and we'll
			// never get the data. Instead, use the DMA interrupt.
			UCB1IE = UCNACKIE | UCALIE;
			// Set DMA trigger.
			DMACTL1 = (DMACTL1 & 0xFF00) | 24;
			// Set DMA parameters.
			DMA2CTL = DMALEVEL | DMASRCBYTE | DMADSTBYTE | DMASRCINCR_0 | DMADSTINCR_3 | DMADT_0 | DMAIE;
			// Begin transmission.
			UCB1CTLW0 |= UCTXSTT;
			// Enable DMA.
			DMA2CTL |= DMAEN;
			return;
		case transaction_REGISTER_READ:
			// Register reads don't use repeated starts,
			// because the automatic stop counter doesn't work with them.
			twi_result = result_OK;
			twi_state = state_REGISTER_READ;
			// Put in reset.
			UCB1CTLW0 |= UCSWRST;
			// Write transaction. Initialize address
			UCB1I2CSA = slave_addr;
			// Set DMA destination address.
			DMA2DA = (__SFR_FARPTR) (unsigned long) &UCB1TXBUF;
			// Set DMA source address.
			DMA2SA = (__SFR_FARPTR) (unsigned long) slave_register;
			// Set DMA size.
			DMA2SZ = slave_register_len;
			// Transmitter mode.
			UCB1CTLW0 |= UCTR;
			UCB1CTLW1 = UCASTP_2;
			UCB1TBCNT = slave_register_len;
			// Pull out of reset.
			UCB1CTLW0 &= ~UCSWRST;
			// Enable byte counter interrupt.
			UCB1IE = UCBCNTIE | UCNACKIE | UCALIE;
			// Set DMA trigger.
			DMACTL1 = (DMACTL1 & 0xFF00) | 25;
			// Set DMA parameters.
			DMA2CTL = DMALEVEL | DMASRCBYTE | DMADSTBYTE | DMASRCINCR_3 | DMADSTINCR_0 | DMADT_0;
			// Begin transmission.
			UCB1CTLW0 |= UCTXSTT;
			// Enable DMA.
			DMA2CTL |= DMAEN;
		default:
			__never_executed();
		}
		return;
	case state_READ:
		if (TWI_BUSY()) return;
		// Disable DMA, as it was never disabled before.
		DMA2CTL &= ~DMAEN;
		// Make sure we run through the main loop.
		asm("		MOV.B #0x00, r4");
		// Return to idle.
		twi_state = state_IDLE;
		return;
	case state_WRITE:
		if (TWI_BUSY()) return;
		asm("		MOV.B #0x00, r4");
		twi_state = state_IDLE;
		return;
	case state_REGISTER_READ:
		if (TWI_BUSY()) return;
		// Check the result.
		if (twi_result != result_OK) {
			asm("		MOV.B #0x00, r4");
			twi_state = state_IDLE;
			return;
		}
		// Swap the transaction.
		twi_transaction = transaction_READ;
		goto Twi_process_begin;
	default:
		__never_executed();
	}
}

void Twi::read_i2c(unsigned char slave_addr, unsigned char nbytes, unsigned char *buf) {
	if (!is_complete()) {
		ui.logputln("TWI> read attempted while busy");
		return;
	}
	Twi::slave_addr = slave_addr;
	Twi::nbytes = nbytes;
	Twi::buf = buf;
	twi_transaction = transaction_READ;
	twi_state = state_BEGIN;
}

void Twi::write_i2c(unsigned char slave_addr, unsigned char nbytes, unsigned char *buf) {
	if (!is_complete()) {
		ui.logputln("TWI> write attempted while busy");
		return;
	}
	Twi::slave_addr = slave_addr;
	Twi::nbytes = nbytes;
	Twi::buf = buf;
	twi_transaction = transaction_WRITE;
	twi_state = state_BEGIN;
}

void Twi::read_i2c_register(unsigned char slave_addr, unsigned long slave_register, unsigned char addr_nbytes, unsigned char nbytes, unsigned char *buf) {
	if (!is_complete()) {
		ui.logputln("TWI> register read attempted while busy");
		return;
	}
	Twi::slave_addr = slave_addr;
	Twi::nbytes = nbytes;
	switch(addr_nbytes) {
	case 0:
	case 1:
		Twi::slave_register[0] = slave_register & 0xFF;
		Twi::slave_register_len = 1;
		break;
	case 2:
		Twi::slave_register[0] = (slave_register & 0xFF00)>>8;
		Twi::slave_register[1] = (slave_register & 0xFF);
		Twi::slave_register_len = 2;
		break;
	case 3:
		Twi::slave_register[0] = (slave_register & 0xFF0000)>>16;
		Twi::slave_register[1] = (slave_register & 0xFF00)>>8;
		Twi::slave_register[2] = (slave_register & 0xFF);
		Twi::slave_register_len = 3;
		break;
	case 4:
	default:
		Twi::slave_register[0] = (slave_register & 0xFF000000)>>24;
		Twi::slave_register[1] = (slave_register & 0xFF0000)>>16;
		Twi::slave_register[2] = (slave_register & 0xFF00)>>8;
		Twi::slave_register[3] = (slave_register & 0xFF);
		Twi::slave_register_len = 4;
		break;
	}
	Twi::buf = buf;
	twi_transaction = transaction_REGISTER_READ;
	twi_state = state_BEGIN;
}

#pragma vector=USCI_B1_VECTOR
__interrupt void EUSCI_I2C_Handler(){
	switch(__even_in_range(UCB1IV, 0x1E)) {
	case 0x00: return;	// no interrupt
	case 0x02: 			// ALIFG
		Twi::twi_result = Twi::result_ARBITRATION_LOST;
		DMA2CTL &= ~DMAEN;
		UCB1IE = 0;
		asm("	mov.b	#0x00, r4");
		// And wake up.
		__bic_SR_register_on_exit(LPM0_bits);
		return;
	case 0x04:
		Twi::twi_result = Twi::result_NACK;
		UCB1CTLW0 |= UCTXSTP;
		DMA2CTL &= ~DMAEN;
		UCB1IE = 0;
		asm("	mov.b	#0x00, r4");
		// And wake up.
		__bic_SR_register_on_exit(LPM0_bits);
		return;  // NACKIFG
	case 0x06: 			// STTIFG
	case 0x08: 			// STPIFG
	case 0x0A:			// RXIFG3
	case 0x0C:			// TXIFG3
	case 0x0E:			// RXIFG2
	case 0x10:			// TXIFG2
	case 0x12:			// RXIFG1
	case 0x14:			// TXIFG1
	case 0x16:			// RXIFG0
	case 0x18:			// TXIFG0
		return;
	case 0x1A:			// BCNTIFG
		DMA2CTL &= ~DMAEN;
		UCB0IE = 0;
		asm("	mov.b	#0x00, r4");
		// And wake up.
		__bic_SR_register_on_exit(LPM0_bits);
		return;
	case 0x1C:			// SCLLOW
	case 0x1E:			// 9th bit.
		return;
	default:
		__never_executed();
	}
}

