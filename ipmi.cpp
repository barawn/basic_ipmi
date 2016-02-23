#include <msp430.h>
#include "ui.h"

/*
 * The IPMI interface makes use of two separate state machines: an RX and a TX
 * state machine. Because IPMI only works as a master transmitter and a slave
 * receiver, and also allows processing to be single threaded, we make this
 * as simple as possible.
 *
 * This allows DMA-controlled interaction on *both* sides, for the most part.
 * To transmit:
 * 1: Make sure busy is free.
 * 2: Put eUSCI into soft reset
 * 3: Enable master mode, transmitter mode, disable I2C own address,
 *    and set up DMA.
 * 4: Take eUSCI out of soft reset.
 * 5: Set UCTXSTT.
 *
 * The only outcomes of a transmit operation are NACKIFG, ALIFG, or BCNTIFG,
 * which result in NACKED, ARBITRATION_LOST, and COMPLETED, respectively.
 *
 *
 */


#define IPMI_LUN(x) ( (x) & 0x3 )
#define IPMI_SET_LUN(x,y) ( ((x) & ~0x3) | (y) )
#define IPMI_SEQ(x) ( (x) & 0xFC)
#define IPMI_SET_SEQ(x,y) ( ((x) & ~0xFC) | (y))

class IPMI {
public:
	typedef enum ipmi_rx_state {
		ipmi_RX_IDLE = 0,
		ipmi_RX_PAUSED = 2,
		ipmi_RX_RECEIVING = 4,
		ipmi_RX_STATE_MAX = 4
	} ipmi_rx_state_t;
	typedef enum ipmi_tx_state {
		ipmi_TX_IDLE = 0,
		ipmi_TX_PREPARED = 2,
		ipmi_TX_STARTED = 4,
		ipmi_TX_ARBITRATION_LOST = 6,
		ipmi_TX_NACKED = 8,
		ipmi_TX_COMPLETE = 10,
		ipmi_TX_WAITING = 12,
		ipmi_TX_STATE_MAX = 12
	} ipmi_tx_state_t;

	// IPMI header structure.
	typedef struct ipmi_header {
		unsigned char netfn_responder_lun;
		unsigned char check1;
		unsigned char sender_address;
		unsigned char rqseq_sender_lun;
	} ipmi_header_t;

	// IPMI response structure.
	// In FRAM, long IPMI responses (SDRs) are
	// stored with header space, the data prefilled,
	// and a partial check computed. Then, the only
	// task needed for the checksum is to
	// subtract sender_address, rqseq_sender_lun
	// and completion.
	typedef struct ipmi_response {
		unsigned char data_length;
		unsigned char raw_check;
		ipmi_header_t header;
		unsigned char cmd;
		unsigned char completion;
		unsigned char *data;
	} ipmi_response_t;

	static void ipmi_enable_receiver() {
		UCB0CTLW0 |= UCSWRST;
		UCB0CTLW0 &= ~(UCMST | UCTR);
		UCB0I2COA0 |= UCOAEN;
		UCB0IE = STTIE;
		UCB0CTLW0 &= ~UCSWRST;
	}

	static void prepare_fixed_response(ipmi_response_t *rsp, ipmi_header_t *req);
	static void send_response();

	const unsigned char RETRY_MAX = 3;
	const unsigned int RX_BUFFER_SIZE = 128;
	// The TX buffer is so small because these are only for algorithmic packets.
	// Stuff like descriptors are completely stored in FRAM, and only the headers
	// and checksum are filled in on a per-message basis.
	//
	// Algorithmic packets include:
	// Get Device SDR Info
	// Reserve SDR Repository
	// Get Device ID
	// Get Self Test Results
	// Get Sensor Reading
	// Unknown Command
	// Total length for any of these messages is pretty short. The Get Device ID
	// is the longest, at 6 (header) + 11 (device ID) + 1 (checksum) = 18.
	// So 32 is healthily longer.
	const unsigned int TX_BUFFER_SIZE = 32;
	static ipmi_state_t ipmi_rx_state;
	static ipmi_state_t ipmi_tx_state;
	static unsigned int tick_wait = 0;
	static unsigned char rx_buffer[RX_BUFFER_SIZE];
	static unsigned char tx_buffer[TX_BUFFER_SIZE];
	static unsigned char rx_wr;
	static unsigned char rx_rd;
	static unsigned char ipmi_address;
	static unsigned char *ipmi_tx_address;
	static unsigned char *ipmi_tx_length;
	static unsigned char ipmi_tx_slave;
	static unsigned char ipmi_tx_retry_count;
};

static void IPMI::process() {
	unsigned int cur_tick;
	switch(__even_in_range(ipmi_tx_state, ipmi_TX_STATE_MAX)) {
	case ipmi_TX_PREPARED: break;
	case ipmi_TX_IDLE: break;
	case ipmi_TX_STARTED: return;
	case ipmi_TX_WAITING:
		cur_tick = clock.ticks();
		if (cur_tick > tick_wait) {
			if (cur_tick - tick_wait < 0x8000) {
				ipmi_tx_state = ipmi_TX_PREPARED;
				break;
			}
		}
		return;
	case ipmi_TX_COMPLETE:
		__disable_interrupt();
		if (UCB0STATW & UCBBUSY) {
			__enable_interrupt();
			return;
		}
		UCB0IE = 0;
		__enable_interrupt();
		ipmi_rx_state = ipmi_RX_IDLE;
		ipmi_enable_receiver();
		return;
	case ipmi_TX_ARBITRATION_LOST:
	case ipmi_TX_NACKED:
		ui.logprintln("IPMI> tx fail %u (%u/%u)", (unsigned int) ipmi_tx_state, ++ipmi_tx_retry_count, RETRY_MAX);
		if (ipmi_tx_retry_count != RETRY_MAX) {
			// Retry.
			ipmi_tx_state = ipmi_TX_WAITING;
			// Wait 100 millis.
			tick_wait = clock.ticks() + clock.ticks_per_second/10;
		}
		return;
		}
	}
	// If we're paused, and the TX state machine is idle, that means we have something to do.
	if (ipmi_rx_state == ipmi_RX_PAUSED) {

	}
	if (ipmi_tx_state == ipmi_TX_PREPARED) {
		if (UCB0STATW & UCBBUSY) return;
	}
}

static void IPMI::prepare_fixed_response(ipmi_response_t *rsp, ipmi_header_t *req) {
	unsigned char tmp1, tmp2;
	tmp1 = IPMI_LUN(req->rqseq_sender_lun);
	IPMI_SET_LUN(rsp->header.netfn_responder_lun, tmp1);
	tmp1 = IPMI_SEQ(req->rqseq_sender_lun);
	IPMI_SET_SEQ(rsp->header.rqseq_sender_lun, tmp1);
	rsp->header.sender_address = IPMI::ipmi_address;
	// compute check 1
	tmp2 = 0;
	tmp2 = tmp2 - req->sender_address;
	tmp2 = tmp2 - rsp->header.netfn_responder_lun;
	rsp->header.check1 = tmp2;
	// compute check 2
	tmp2 = rsp->raw_check;
	tmp2 = tmp2 - rsp->header.sender_address;
	tmp2 = tmp2 - rsp->header.rqseq_sender_lun;
	rsp->data[rsp->data_length - 1] = tmp2;
	ipmi_tx_address = (unsigned char *) &(rsp->header);
	// data_length doesn't include header (4), or cmd/completion (2)
	ipmi_tx_length = rsp->data_length + 6;
	ipmi_tx_state = ipmi_TX_PREPARED;
}

static void IPMI::send_response() {
	// Check RX state. Is it paused? We don't send out unsolicited messages, so it should always be paused.
	if (ipmi_rx_state != IPMI::ipmi_RX_PAUSED) {
		ui.logprintln("IPMI> responding when RX state = %u?", (unsigned int) ipmi_rx_state);
		return;
	}
	// OK, RX state is paused.
	ipmi_tx_state = IPMI::ipmi_TX_STARTED;
	ipmi_tx_retry_count = 0;
	DMA1CTL &= ~DMAEN;
	DMACTL0_H = 19;
	DMA1SA = (__SFR_FARPTR) ((unsigned long) IPMI::ipmi_tx_address);
	DMA1DA = (__SFR_FARPTR) ((unsigned long) &UCB0TXBUF);
	DMA1SZ = ipmi_tx_length;
	UCB0I2CSA = ipmi_tx_slave;
	// Don't know if interrupts need to be disabled here. Start interrupt is
	// disabled anyway when we go into SWRST.
	__disable_interrupt();
	UCB0CTLW0 |= UCSWRST;
	// Master transmitter, with automatic STOP generation.
	UCB0CTLW0 |= UCMST | UCTR | UCASTP_2;
	// Byte counter length.
	UCB0TBCNT = ipmi_tx_length;
	// *Disable* own address. If we're trying to transmit, we will not respond as a slave.
	UCB0I2COA0 &= ~UCOAEN;
	// Pull out of soft reset.
	UCB0CTLW0 &= ~UCSWRST;
	UCB0CTLW0 |= UCTXSTT;
	UCB0IE = UCALIE | UCNACKIE | UCBCNTIE;
	__enable_interrupt();
	DMA1CTL = DMAEN | DMALEVEL | DMASRCBYTE | DMADSTBYTE | DMASRCINCR | DMADT_0;
	// OK, so at this point we could just go to sleep and wait for the transmission to complete.
}

IPMI::ipmi_rx_state_t IPMI::ipmi_rx_state = IPMI::ipmi_RX_IDLE;
IPMI::ipmi_tx_state_t IPMI::ipmi_tx_state = IPMI::ipmi_TX_IDLE;
// FIXME change this.
unsigned char IPMI::ipmi_address = 0x00;

// The IPMI ISR routines are really primarily for RX:
// TX actually goes beginning -> completion with no
// interaction whatsoever, ending with either ALIFG,
// NACKIFG, or BCNTIFG.
#pragma VECTOR=USCI_B0_VECTOR
__interrupt void IPMI_Handler() {
	switch(__even_in_range(UCB0IV, 0x1E)) {
	case 0x00: 			// no interrupt
		return;
	case 0x02:			// ALIFG
		// Arbitration lost.
		// Set receive side interrupt enable.
		UCB0IE = UCSTTIE;
		// Set TX state to arbitration lost.
		IPMI::ipmi_tx_state = IPMI::ipmi_TX_ARBITRATION_LOST;
		//
		// DMA doesn't need to be disabled, because UCMST got cleared, and
		// so UCTXIFG won't go off. If UCSTTIFG goes off, nothing will happen
		// because we by definition here are in PAUSED.
		//
		// Clear ALIFG flag.
		UCB0IFG &= ~UCALIFG;
		// Kill the burst count interrupt enable.
		UCB0IE &= ~UCBCNTIE;
		// Make sure we stay awake.
		asm("	mov.b	#0x00, r4");
		// And wake up.
		__bic_SR_register_on_exit(LPM0_bits);
		return;
	case 0x04:			// NACKIFG
		// Set TX state to nacked.
		IPMI::ipmi_tx_state = IPMI::ipmi_TX_NACKED;
		// Clear NACKIFG.
		UCB0IFG &= ~UCNACKIFG;
		// Kill the burst count interrupt enable.
		UCB0IE &= ~UCBCNTIE;
		// Kill DMA.
		DMA1CTL &= ~DMAEN;
		// Send STOP.
		UCB0CTLW0 |= UCTXSTP;
		// Make sure we stay awake.
		asm("	mov.b	#0x00, r4");
		// Wake up.
		__bic_SR_register_on_exit(LPM0_bits);
		// Send STOP.
		UCB0CTL1 |= UCTXSTP;
		return;
	case 0x06:			// STTIFG
		// Check RX state.
		if (IPMI::ipmi_rx_state != IPMI::ipmi_RX_IDLE) {
			if (IPMI::ipmi_rx_state != IPMI::ipmi_RX_PAUSED) {
				// Got another START. If we're not IDLE, or PAUSED, we're RECEIVING.
				// So we have to act like we received a STOP.
				// Shut off DMA.
				DMA1CTL &= ~DMAEN;
				// Figure out number of bytes that were transferred.
				IPMI::rx_wr = IPMI::RX_BUFFER_SIZE;
				IPMI::rx_wr = IPMI::rx_wr - DMA1SZ;
				if (IPMI::rx_wr) {
					IPMI::ipmi_rx_state = IPMI::ipmi_RX_PAUSED;
					// NACK everything else we get.
					UCB0CTL1 |= UCTXNACK;
					UCB0IE = UCSTPIE | UCSTTIE | UCRXIE;
					UCB0STAT &= ~UCGC;
					asm("	mov.b	#0x00, r4");
					// And wake up.
					__bic_SR_register_on_exit(LPM0_bits);
					return;
				}
				// No data.
				// Processing continues. This happens if you get stupidity like
				// a start + address, then restart + address.
				IPMI::ipmi_rx_state = IPMI::ipmi_RX_IDLE;
			} else return;
		}
		if (UCB0STAT & UCGC) {
			// Respond to stop, start, and receive.
			UCB0IE = UCSTPIE | UCSTTIE | UCRXIE;
			// State doesn't change. Don't touch DMA either
			// until we're ready to really accept.
			return;
		}
		// Not a general call, this is real.
		UCB0IE = UCSTPIE | UCSTTIE;
		// DMA trigger is now UCB0RXIFG.
		DMACTL0_H = 18;
		DMA1SA = (__SFR_FARPTR) (unsigned long) &UCB0RXBUF;
		DMA1DA = (__SFR_FARPTR) (unsigned long) IPMI::rx_buffer;
		DMA1CTL = DMAEN | DMALEVEL | DMASRCBYTE | DMADSTBYTE | DMADSTINCR | DMADT_0;
		// STPIFG will fire when the message is completely received.
		IPMI::ipmi_rx_state = IPMI::ipmi_RX_RECEIVING;
		return;
	case 0x08:			// STPIFG
		if (IPMI::ipmi_rx_state == IPMI::ipmi_RX_RECEIVING) {
			// Shut off DMA.
			DMA1CTL &= ~DMAEN;
			// Figure out number of bytes that were transferred.
			IPMI::rx_wr = IPMI::RX_BUFFER_SIZE;
			IPMI::rx_wr = IPMI::rx_wr - DMA1SZ;
			if (IPMI::rx_wr) {
				IPMI::ipmi_rx_state = IPMI::ipmi_RX_PAUSED;
				// Kill the I2C receiver.
				UCB0CTLW0 |= UCSWRST;
				UCB0I2COA0 &= ~UCOAEN;
				UCB0IE = 0;
				UCB0CTLW0 &= ~UCSWRST;
				asm("	mov.b	#0x00, r4");
				// And wake up.
				__bic_SR_register_on_exit(LPM0_bits);
				return;
			}
			// No data.
			IPMI::ipmi_rx_state = IPMI::ipmi_RX_IDLE;
		}
		UCB0IE = UCSTTIE;
		// Don't need to do anything.
		return;
	case 0x0A:			// RXIFG3
		return;
	case 0x0C:			// TXIFG3
		return;
	case 0x0E:			// RXIFG2
		return;
	case 0x10:			// TXIFG2
		return;
	case 0x12:			// RXIFG1
		return;
	case 0x14:			// TXIFG1
		return;
	case 0x16:			// RXIFG0
		// General call?
		if (UCB0STAT & UCGC) {
			if (UCB0RXBUF == IPMI::ipmi_address) {
				UCB0IE = UCSTPIE | UCSTTIE;
				DMACTL0_H = 18;
				DMA1SA = (__SFR_FARPTR) (unsigned long) &UCB0RXBUF;
				DMA1DA = (__SFR_FARPTR) (unsigned long) IPMI::rx_buffer;
				DMA1CTL = DMAEN | DMALEVEL | DMASRCBYTE | DMADSTBYTE | DMADSTINCR | DMADT_0;
				IPMI::ipmi_rx_state = IPMI::ipmi_RX_RECEIVING;
				return;
			}
		}
		UCB0CTL1 |= UCTXNACK;
		asm("		TST.B &UCB0RXBUF");
		return;
	case 0x18:			// TXIFG0
		return;
	case 0x1A:			// BCNTIFG
		// TX completed successfully.
		IPMI::ipmi_tx_state = IPMI::ipmi_TX_COMPLETE;
		// Don't have to do anything with DMA, it should be done.
		// This requires both DMA1SZ and UCB0TBCNT to be set
		// identically. Note that address bytes are excluded.
		UCB0IE &= ~BCNTIE;
		asm("	mov.b	#0x00, r4");
		__bic_SR_register_on_exit(LPM0_bits);
		return;
	case 0x1C:			// SCLLOW timeout
		return;
	case 0x1E:			// 9th bit
		return;
	}
}
