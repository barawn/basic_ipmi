/*
 * twi.h
 *
 *  Created on: Feb 29, 2016
 *      Author: barawn
 */

#ifndef TWI_H_
#define TWI_H_

//% \brief Generic I2C (two-wire interface) peripheral.
//%
//%
class Twi {
public:
	typedef enum twi_state {
		state_IDLE = 0,					// Nothing.
		state_BEGIN = 2,
		state_WRITE = 4,				// Writing only.
		state_READ = 6,					// Reading only.
		state_REGISTER_READ = 8,		// Writing register.
		state_MAX = state_REGISTER_READ
	} twi_state_t;
	typedef enum twi_result {
		result_OK = 0,
		result_NACK = 2,
		result_ARBITRATION_LOST = 4,
		result_MAX = result_ARBITRATION_LOST
	} twi_result_t;
	typedef enum twi_transaction {
		transaction_NONE = 0,
		transaction_WRITE = 2,
		transaction_READ = 4,
		transaction_REGISTER_READ = 6,
		transaction_MAX = transaction_REGISTER_READ
	} twi_transaction_t;

	Twi() {}
	static void initialize();
	static void process();
	static void read_i2c(unsigned char slave_addr, unsigned char nbytes, unsigned char *buf);
	static void write_i2c(unsigned char slave_addr, unsigned char nbytes, unsigned char *buf);
	static void read_i2c_register(unsigned char slave_addr, unsigned long slave_register, unsigned char addr_nbytes, unsigned char nbytes, unsigned char *buf);
	static bool is_complete() {
		return twi_state == state_IDLE;
	}
	static twi_result_t result() {
		return twi_result;
	}

	static unsigned char slave_register[4];
	static unsigned char slave_register_len;
	static unsigned char slave_addr;
	static twi_state_t twi_state;
	static twi_result_t twi_result;
	static twi_transaction_t twi_transaction;
	static unsigned char *buf;
	static unsigned char nbytes;
};

#define TWI_BUSY() (DMA2CTL & DMAEN)

extern Twi twi;

#endif /* TWI_H_ */
