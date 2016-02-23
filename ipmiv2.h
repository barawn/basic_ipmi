/*
 * ipmiv2.h
 *
 *  Created on: Feb 17, 2016
 *      Author: barawn
 */

#ifndef IPMIV2_H_
#define IPMIV2_H_

#include <msp430.h>

class IPMI {
public:
	// state definitions
	typedef enum ipmi_rx_state {
		ipmi_RX_IDLE = 0,
		ipmi_RX_PAUSED = 2,
		ipmi_RX_RECEIVING = 4,
		ipmi_RX_PROCESSING = 6,
		ipmi_RX_TRANSMITTING = 8,
		ipmi_RX_STATE_MAX = 8
	} ipmi_rx_state_t;
	typedef enum ipmi_tx_state {
		ipmi_TX_IDLE = 0,
		ipmi_TX_STARTED = 2,
		ipmi_TX_ARBITRATION_LOST = 4,
		ipmi_TX_NACKED = 6,
		ipmi_TX_COMPLETE = 8,
		ipmi_TX_RETRY_WAIT = 10,
		ipmi_TX_TRANSMITTING = 12,
		ipmi_TX_STATE_MAX = 12
	} ipmi_tx_state_t;
	// IPMI structures
	typedef struct ipmi_header {
		unsigned char netfn_dstLUN;
		unsigned char check1;
		unsigned char srcSA;
		unsigned char rqSeq_srcLUN;
		unsigned char cmd;
	} ipmi_header_t;


	// IPMI constants
	const unsigned char IPMI_MIN_MESSAGE_LENGTH = 6;
	const unsigned char IPMI_MIN_RESPONSE_LENGTH = 7;

	const unsigned char IPMI_COMPLETION_OK = 0x00;
	const unsigned char IPMI_COMPLETION_INVALID = 0xC1;
	const unsigned char IPMI_COMPLETION_REQUEST_DATA_TRUNCATED = 0xC6;
	const unsigned char IPMI_COMPLETION_REQUEST_DATA_LENGTH_INVALID = 0xC7;
	const unsigned char IPMI_COMPLETION_PARAMETER_OUT_OF_RANGE = 0xC9;
	const unsigned char IPMI_COMPLETION_CANNOT_RETURN_NUMBER_OF_BYTES = 0xCA;
	const unsigned char IPMI_COMPLETION_SENSOR_DATA_RECORD_NOT_PRESENT = 0xCB;
	const unsigned char IPMI_COMPLETION_INVALID_DATA_FIELD = 0xCC;

	const unsigned char IPMI_APP_GET_DEVICE_ID 		   = 0x01;
	const unsigned char IPMI_APP_GET_SELF_TEST_RESULTS = 0x04;

	const unsigned char IPMI_SENSOR_GET_DEVICE_SDR_INFO = 0x20;
	const unsigned char IPMI_SENSOR_GET_DEVICE_SDR = 0x21;
	const unsigned char IPMI_SENSOR_RESERVE_DEVICE_SDR_REPOSITORY = 0x22;
	const unsigned char IPMI_SENSOR_GET_SENSOR_READING = 0x2D;

	static void initialize();
	static void process();
	static bool tx_process();
	static bool handle_message();

	static bool handle_oem_netfn();
	static bool handle_app_netfn();
	static bool handle_sensor_netfn();
	static bool handle_unknown_netfn();

	static bool validate_message(unsigned char len);
	static void respond(unsigned char len);

	static void fill_connection_header(ipmi_header_t *hdr,
									  unsigned char netFn_dstLUN,
									  unsigned char rqSeq_srcLUN,
									  unsigned char cmd);
	static void generate_check2(ipmi_header_t *hdr, unsigned char *check2);

	static ipmi_rx_state_t ipmi_rx_state;
	static ipmi_tx_state_t ipmi_tx_state;

	const unsigned int RX_BUFFER_SIZE = 128;
	static unsigned char rx_buffer[RX_BUFFER_SIZE];
	// This is an integer because it's copied from DMAxSZ, which is an int.
	// It will always be 128 or less.
	static unsigned int rx_buffer_remaining;

	const unsigned char TX_RETRY_MAX = 3;
	const unsigned char TX_BUFFER_SIZE = 32;
	// Maximum bytes per message. The extra byte in the buffer
	// is to allow us to do 16-bit copies.
	const unsigned char TX_BUFFER_MAX = 31;
	static unsigned char tx_retry_count;
	static unsigned int tx_retry_time;
	static unsigned char tx_slave;
	static const unsigned char *tx_address;
	static unsigned int tx_length;
	static unsigned char tx_buffer[TX_BUFFER_SIZE];

private:
	static void ipmi_rx_dma_init() {
		// DMA trigger is now UCB0RXIFG.
		DMACTL0 = (18 << 8) | (DMACTL0 & 0xFF);
		// Max buffer size.
		DMA1SZ = IPMI::RX_BUFFER_SIZE;
		DMA1SA = (__SFR_FARPTR) (unsigned long) &UCB0RXBUF;
		DMA1DA = (__SFR_FARPTR) (unsigned long) IPMI::rx_buffer;
		DMA1CTL = DMALEVEL | DMASRCBYTE | DMADSTBYTE | DMASRCINCR_0 | DMADSTINCR_3 | DMADT_0;
	}
	static void ipmi_rx_reset() {
		ipmi_rx_state = ipmi_RX_IDLE;
		UCB0CTLW0 |= UCSWRST;
		UCB0I2COA0 |= UCOAEN | UCGCEN;
		UCB0CTLW0 &= ~UCSWRST;
		UCB0IE = UCSTTIE;
	}
};

extern IPMI ipmi;

#endif /* IPMIV2_H_ */
