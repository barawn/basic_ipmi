#include <msp430.h>
#include "ipmiv2.h"
#include "ui.h"
#include "clock.h"
#include "info.h"
#include "ipmi_device_specific.h"

IPMI ipmi;

IPMI::ipmi_rx_state_t IPMI::ipmi_rx_state = ipmi_RX_IDLE;
unsigned char IPMI::rx_buffer[IPMI::RX_BUFFER_SIZE];
unsigned int IPMI::rx_buffer_remaining = 0;

IPMI::ipmi_tx_state_t IPMI::ipmi_tx_state = ipmi_TX_IDLE;
unsigned char IPMI::tx_retry_count = 0;
unsigned int IPMI::tx_retry_time = 0;
unsigned char IPMI::tx_slave = 0;
unsigned int IPMI::tx_length = 0;

#pragma DATA_ALIGN(2)
unsigned char IPMI::tx_buffer[IPMI::TX_BUFFER_SIZE];

void IPMI::initialize() {
	UCB0CTLW0 |= UCSWRST;
	UCB0CTLW0 = UCMODE_3 | UCSSEL_2 | UCMM | UCSWRST;
	UCB0CTLW1 = UCASTP_1;
	// 100 kHz transmit rate.
	UCB0BRW = 10;
	UCB0I2COA0 = (info.ipmi_address >> 1) | UCOAEN | UCGCEN;
	// DMA initially gets set up for receiving.
	ipmi_rx_dma_init();

	// Assign pins to eUSCI.
	P1SEL0 &= ~(BIT6 | BIT7);
	P1SEL1 |= (BIT6 | BIT7);

	UCB0CTLW0 &= ~UCSWRST;
	// Activate start interrupt.
	UCB0IE |= UCSTTIE;
}

//< \brief Respond to an IPMI request.
//<
//< The standard response to an IPMI request fills
//< the TX buffer based on RX buffer responses.
//< The data content of the message is presumed to be filled already.
void IPMI::respond(unsigned char len) {
	ipmi_header_t *rq;
	ipmi_header_t *rsp;
	unsigned char netFn_dstLUN;
	unsigned char rqSeq_srcLUN;
	unsigned char tmp;
	unsigned char i;

	rq = (ipmi_header_t *) rx_buffer;
	rsp = (ipmi_header_t *) tx_buffer;

	// len doesn't include check2.
	len++;

	netFn_dstLUN = (rq->netfn_dstLUN & 0xFC) + 0x4;
	netFn_dstLUN |= rq->rqSeq_srcLUN & 0x3;
	rqSeq_srcLUN = (rq->rqSeq_srcLUN & 0xFC) | (rq->netfn_dstLUN & 0x3);
	tx_slave = rq->srcSA;
	rsp->netfn_dstLUN = netFn_dstLUN;
	rsp->rqSeq_srcLUN = rqSeq_srcLUN;
	rsp->cmd = rq->cmd;
	rsp->srcSA = info.ipmi_address;
	tmp = 0;
	tmp -= tx_slave;
	tmp -= netFn_dstLUN;
	rsp->check1 = tmp;
	// Check2 goes from byte 2 to byte before last (len-2)
	// e.g. if a min 7 byte message, go from byte 2 to byte 5.
	tmp = 0;
	for (i=2;i<len-1;i++) {
		tmp -= tx_buffer[i];
	}
	tx_buffer[len-1] = tmp;
	ipmi_tx_state = ipmi_TX_STARTED;
	ipmi_rx_state = ipmi_RX_TRANSMITTING;
	tx_length = len;
}

bool IPMI::handle_app_netfn() {
	ipmi_header_t *rq;
	unsigned char *data;
	unsigned int len;

	rq = (ipmi_header_t *) rx_buffer;
	data = tx_buffer + sizeof(ipmi_header_t);
	if (rq->cmd == IPMI_APP_GET_DEVICE_ID) {
		ui.logputln("IPMI> GET_DEVICE_ID");
		data = thisDevice.copy_device_id(data);
		len = data - tx_buffer;
		respond(len);
		return true;
	}
	if (rq->cmd == IPMI_APP_GET_SELF_TEST_RESULTS) {
		ui.logputln("IPMI> GET_SELF_TEST_RESULTS");
		*data++ = IPMI_COMPLETION_OK;
		*data++ = 0x56;
		*data++ = 0x00;
		len = data - tx_buffer;
		respond(len);
		return true;
	}
	return handle_unknown_netfn();
}

bool IPMI::handle_sensor_netfn() {
	ipmi_header_t *rq;
	unsigned char *data;
	unsigned char *rqdata;
	unsigned int len;
	unsigned char rx_length;

	rq = (ipmi_header_t *) rx_buffer;
	rqdata = rx_buffer + sizeof(ipmi_header_t);
	data = tx_buffer + sizeof(ipmi_header_t);
	rx_length = RX_BUFFER_SIZE - rx_buffer_remaining;

	if (rq->cmd == IPMI_SENSOR_GET_DEVICE_SDR_INFO) {
		unsigned char operation;

		if (rx_length - IPMI_MIN_MESSAGE_LENGTH) operation = rqdata[0];
		else operation = 0;
		ui.logprintln("IPMI> GET_DEVICE_SDR_INFO %X", operation);
		data = thisDevice.copy_sdr_info(operation, data);
		len = data - tx_buffer;
		respond(len);
		return true;
	}
	if (rq->cmd == IPMI_SENSOR_RESERVE_DEVICE_SDR_REPOSITORY) {
		ui.logputln("IPMI> RESERVE_DEVICE_SDR_REPOSITORY");
		data = thisDevice.reserve_device_sdr_repository(data);
		len = data - tx_buffer;
		respond(len);
		return true;
	}
	if (rq->cmd == IPMI_SENSOR_GET_DEVICE_SDR) {
		unsigned int sdr;
		unsigned char offset;
		unsigned char bytes;

		if (rx_length - IPMI_MIN_MESSAGE_LENGTH < 6) {
			*data++ = IPMI_COMPLETION_INVALID_DATA_FIELD;
			len = data - tx_buffer;
			respond(len);
			return true;
		}
		sdr = rqdata[2] + (rqdata[3] << 8);
		offset = rqdata[4];
		bytes = rqdata[5];
		ui.logprintln("IPMI> GET_DEVICE_SDR %u %u %u", sdr, offset, bytes);
		data = thisDevice.copy_sdr(sdr, offset, bytes, data);
		len = data - tx_buffer;
		respond(len);
		return true;
	}
	if (rq->cmd == IPMI_SENSOR_GET_SENSOR_READING) {
		unsigned int sensor;
		if (!(rx_length - IPMI_MIN_MESSAGE_LENGTH)) {
			*data++ = IPMI_COMPLETION_INVALID_DATA_FIELD;
			len = data - tx_buffer;
			respond(len);
			return true;
		}
		sensor = rqdata[0];
		ui.logprintln("IPMI> GET_SENSOR_READING %u", sensor);
		data = thisDevice.copy_sensor_reading(sensor, data);
		len = data - tx_buffer;
		respond(len);
		return true;
	}
	return handle_unknown_netfn();
}

bool IPMI::handle_oem_netfn() {
	return handle_unknown_netfn();
}

bool IPMI::handle_unknown_netfn() {
	ipmi_header_t *rq;
	unsigned char *data;
	unsigned int len;

	rq = (ipmi_header_t *) rx_buffer;
	data = tx_buffer + sizeof(ipmi_header_t);
	*data++ = IPMI_COMPLETION_INVALID;
	len = data - tx_buffer;
	respond(len);
	return true;
}

bool IPMI::handle_message() {
	ipmi_header_t *p;
	unsigned char netfn;
	unsigned char lun;

	p = (ipmi_header_t *) rx_buffer;
	netfn = (p->netfn_dstLUN & 0xFC) >> 2;
	lun = (p->netfn_dstLUN & 0x3);
	if (netfn & 0x1) {
		// This is a response. Maybe handle these
		// later or something, if I decide to do
		// something like get system times or something.
		return false;
	}
	if (lun) {
		return handle_unknown_netfn();
	}
	switch (__even_in_range(netfn, 0x3E)) {
	case 0x04:
		return handle_sensor_netfn();
	case 0x06:
		return handle_app_netfn();
	case 0x30:
		return handle_oem_netfn();
	case 0x00:
	case 0x02:
	case 0x08:
	case 0x0A:
	case 0x0C:
	case 0x0E:
	case 0x10:
	case 0x12:
	case 0x14:
	case 0x16:
	case 0x18:
	case 0x1A:
	case 0x1C:
	case 0x1E:
	case 0x20:
	case 0x22:
	case 0x24:
	case 0x26:
	case 0x28:
	case 0x2A:
	case 0x2C:
	case 0x2E:
	case 0x32:
	case 0x34:
	case 0x36:
	case 0x38:
	case 0x3A:
	case 0x3C:
	case 0x3E:
		return handle_unknown_netfn();
	default:
		__never_executed();
	}
}

bool IPMI::validate_message(unsigned char len) {
	unsigned char check;
	ipmi_header_t *p;
	unsigned char *data;
	unsigned char *p2;

	if (len < IPMI_MIN_MESSAGE_LENGTH) return false;
	p = (ipmi_header_t *) rx_buffer;
	data = rx_buffer + sizeof(ipmi_header_t);
	check = info.ipmi_address + p->netfn_dstLUN + p->check1;
	if (check) {
		// HACK TO SUPPORT GE BMC's BROADCAST MODE
		// GE's BMC screws up the transmit pointer when
		// doing broadcast mode, so it ends up overwriting
		// the netFn/LUN with the check of the first two
		// bytes (0 and rsSA).
		// Then rqSA gets duplicated (again, due to pointer screwup).
		if (len > IPMI_MIN_MESSAGE_LENGTH) return false;

		check = info.ipmi_address + p->netfn_dstLUN;
		if (check) return false;
		if (p->check1 != p->srcSA) return false;
		check = p->check1 + p->srcSA + p->rqSeq_srcLUN + p->cmd + *data;
		if (check) return false;
		// OK, it's fine. Fix the screwup.
		p->netfn_dstLUN = 0x18;
		return true;
	}
	check = p->srcSA + p->rqSeq_srcLUN + p->cmd;
	// Get pointer to end of data. Add 1 because
	// IPMI_MIN_MESSAGE_LENGTH includes the check2 byte.
	// So if the message length is 6, this is
	// data + 6 - 6 + 1, or data + 1 (check byte).
	p2 = data + len - IPMI_MIN_MESSAGE_LENGTH + 1;
	do {
		check += *data++;
	} while (data != p2);
	if (check) return false;
	return true;
}

bool IPMI::tx_process() {
	unsigned int cur_tick;

	switch(__even_in_range(ipmi_tx_state, ipmi_TX_STATE_MAX)) {
	case ipmi_TX_IDLE: return false;
	case ipmi_TX_STARTED:
ipmi_TX_STARTED_process:
		if (tx_length == 0) {
			ui.logputln("IPMI> zero length msg??");
			ipmi_tx_state = ipmi_TX_IDLE;
			return false;
		}
		ipmi_tx_state = ipmi_TX_TRANSMITTING;
		// TX begin. Address is in ipmi_tx_address,
		// and length is in ipmi_tx_length.
		DMA1SA = (__SFR_FARPTR) (unsigned long) tx_buffer;
		DMA1DA = (__SFR_FARPTR) (unsigned long) &UCB0TXBUF;
		DMA1SZ = tx_length;
		DMACTL0 = (19 << 8) | (DMACTL0 & 0xFF);
		DMA1CTL = DMALEVEL | DMASRCBYTE | DMADSTBYTE | DMASRCINCR_3 | DMADSTINCR_0 | DMADT_0;
		// Put eUSCI_B0 in reset, switch to master mode and transmit mode.
		UCB0CTLW0 |= UCSWRST;
		UCB0CTLW0 |= (UCMST | UCTR);
		UCB0TBCNT = tx_length;
		UCB0CTLW0 &= ~UCSWRST;
		UCB0IE = UCBCNTIFG | UCALIFG | UCNACKIFG;
		// Issue start.
		UCB0I2CSA = (tx_slave>>1);
		UCB0CTLW0 |= UCTXSTT;
		// Start DMA. We need to check this!!
		DMA1CTL |= DMAEN;

		return true;
	case ipmi_TX_ARBITRATION_LOST:
	case ipmi_TX_NACKED:
		ui.logprintln("IPMI> tx %u/%u fail %X", ++tx_retry_count, TX_RETRY_MAX, ipmi_tx_state);
		if (tx_retry_count == TX_RETRY_MAX) {
			// Abandoning attempt.
			ipmi_tx_state = ipmi_TX_COMPLETE;
			goto ipmi_TX_COMPLETE_process;
		}
		tx_retry_time = clock.ticks + 2;
		ipmi_tx_state = ipmi_TX_RETRY_WAIT;
		return true;
	case ipmi_TX_RETRY_WAIT:
		cur_tick = clock.ticks;
		if (cur_tick > tx_retry_time) {
			if (cur_tick - tx_retry_time < 0x8000) {
				ui.logprintln("IPMI> retry at %u", tx_retry_time);
				ipmi_tx_state = ipmi_TX_STARTED;
				goto ipmi_TX_STARTED_process;
			}
		}
		return true;
	case ipmi_TX_COMPLETE:
	ipmi_TX_COMPLETE_process:
		if (UCB0STATW & UCBBUSY) return true;
		ipmi_rx_dma_init();
		return false;
	case ipmi_TX_TRANSMITTING:
		return true;
	}
}

void IPMI::process() {
	unsigned int len;

	switch(__even_in_range(ipmi_rx_state, ipmi_RX_STATE_MAX)) {
	case ipmi_RX_IDLE:
	case ipmi_RX_RECEIVING:
		return;
	case ipmi_RX_PAUSED:
		// We have a message to process.
		len = RX_BUFFER_SIZE - rx_buffer_remaining;
		if (!validate_message(len)) {
			ipmi_rx_reset();
			return;
		} else {
			ipmi_header_t *p;
			p = (ipmi_header_t *) rx_buffer;
			ipmi_rx_state = ipmi_RX_PROCESSING;
		}
	case ipmi_RX_PROCESSING:
		if (!handle_message()) {
			ipmi_rx_reset();
			return;
		}
		if (ipmi_rx_state != ipmi_RX_TRANSMITTING) return;
		tx_retry_count = 0;
	case ipmi_RX_TRANSMITTING:
		if (!tx_process())
			ipmi_rx_reset();
	}
}

// Cleanups from v1:
// ipmi_address is the 8-bit address. It only gets programmed once, so a shift
// at that point is easy. Compares happen all the time.
// Instead of an rx_buffer_len we use rx_buffer_remaining. Reduces ISR instructions.
// (We can't use the byte counter since general calls screw it up).
//
// Note we have to use UCB0RXBUF_L to avoid register usage.
#pragma vector=USCI_B0_VECTOR
__interrupt void IPMI_Handler() {
	switch(__even_in_range(UCB0IV, 0x1E)) {
	case 0x00: return;	// no interrupt
	case 0x02: 			// ALIFG
		IPMI::ipmi_tx_state = IPMI::ipmi_TX_ARBITRATION_LOST;
		DMA1CTL &= ~DMAEN;
		UCB0IE = 0;
		asm("	mov.b	#0x00, r4");
		// And wake up.
		__bic_SR_register_on_exit(LPM0_bits);
		return;
	case 0x04:
		IPMI::ipmi_tx_state = IPMI::ipmi_TX_NACKED;
		UCB0CTLW0 |= UCTXSTP;
		DMA1CTL &= ~DMAEN;
		UCB0IE = 0;
		asm("	mov.b	#0x00, r4");
		// And wake up.
		__bic_SR_register_on_exit(LPM0_bits);
		return;  // NACKIFG
	case 0x06: 			// STTIFG
		if (IPMI::ipmi_rx_state != IPMI::ipmi_RX_IDLE) {
			// Repeated Start.
			if (DMA1SZ != IPMI::RX_BUFFER_SIZE) {
				// We have data, but we can't disable ourself yet.
				IPMI::ipmi_rx_state = IPMI::ipmi_RX_PAUSED;
				// We have to grab this before disabling DMA.
				IPMI::rx_buffer_remaining = DMA1SZ;
				DMA1CTL &= ~DMAEN;
				UCB0CTLW0 |= UCTXNACK;
				UCB0IE = UCSTPIE | UCSTTIE | UCRXIE;
				UCB0STAT &= ~UCGC;
				return;
			}
			DMA1CTL &= ~DMAEN;
			// No data. Continue.
			IPMI::ipmi_rx_state = IPMI::ipmi_RX_IDLE;
		}
		// Is it a general call?
		if (UCB0STAT & UCGC) {
			// Respond to stop, start, and receive.
			UCB0IE = UCSTPIE | UCSTTIE | UCRXIE;
			// State doesn't change. Don't touch DMA either
			// until we're ready to really accept.
			return;
		}
		// No, so initialize DMA.
		UCB0IE = UCSTPIE | UCSTTIE;
		// DMA is set up so that the receiver can do it as quickly as possible.
		DMA1CTL |= DMAEN;
		// STPIFG will fire when the message is completely received.
		IPMI::ipmi_rx_state = IPMI::ipmi_RX_RECEIVING;
		return;
	case 0x08: 			// STPIFG
		// Received STOP. Process message.
		if (IPMI::ipmi_rx_state == IPMI::ipmi_RX_RECEIVING) {
			if (DMA1SZ != IPMI::RX_BUFFER_SIZE) {
				IPMI::ipmi_rx_state = IPMI::ipmi_RX_PAUSED;
				IPMI::rx_buffer_remaining = DMA1SZ;
				DMA1CTL &= ~DMAEN;
				// Shut off receiver. SWRST resets IE.
				UCB0CTLW0 |= UCSWRST;
				UCB0I2COA0 &= ~(UCOAEN | UCGCEN);
				UCB0CTLW0 &= ~UCSWRST;
				asm("	mov.b	#0x00, r4");
				// And wake up.
				__bic_SR_register_on_exit(LPM0_bits);
				return;
			}
			// No data, so revert to STTIFG.
			IPMI::ipmi_rx_state = IPMI::ipmi_RX_IDLE;
		}
		if (IPMI::ipmi_rx_state == IPMI::ipmi_RX_PAUSED) {
			// We hit a repeated start.
			UCB0CTLW0 |= UCSWRST;
			UCB0I2COA0 &= ~(UCOAEN | UCGCEN);
			UCB0CTLW0 &= ~UCSWRST;
			asm("	mov.b	#0x00, r4");
			// And wake up.
			__bic_SR_register_on_exit(LPM0_bits);
			return;
		}
		// If we weren't receiving, we just need to reset back to STTIFG.
		UCB0IE = UCSTTIE;
		return;
	case 0x0A:			// RXIFG3
	case 0x0C:			// TXIFG3
	case 0x0E:			// RXIFG2
	case 0x10:			// TXIFG2
	case 0x12:			// RXIFG1
	case 0x14:			// TXIFG1
		return;
	case 0x16:			// RXIFG0
		// If it was a general call, we need to check our address.
		if (UCB0STATW & UCGC) {
			if (UCB0RXBUF_L == info.ipmi_address) {
				// Yes, it's ours. Enable start/stop interrupts, and set our state to receiving.
				UCB0IE = UCSTPIE | UCSTTIE;
				DMA1CTL |= DMAEN;
				IPMI::ipmi_rx_state = IPMI::ipmi_RX_RECEIVING;
				return;
			}
			UCB0STATW &= ~UCGC;
		}
		// Otherwise, just nacknacknack
		UCB0CTLW0 |= UCTXNACK;
		return;
	case 0x18:			// TXIFG0
		return;
	case 0x1A:			// BCNTIFG
		IPMI::ipmi_tx_state = IPMI::ipmi_TX_COMPLETE;
		UCB0CTLW0 |= UCTXSTP;
		DMA1CTL &= ~DMAEN;
		UCB0IE = 0;
		asm("	mov.b	#0x00, r4");
		// And wake up.
		__bic_SR_register_on_exit(LPM0_bits);
		return;
	case 0x1C:			// SCLLOW
		return;
	case 0x1E:			// 9th bit.
		return;
	default:
		__never_executed();
	}
}
