/*
 * info.h
 *
 *  Created on: Feb 21, 2016
 *      Author: barawn
 */

#ifndef INFO_H_
#define INFO_H_

#include <msp430.h>
#include "sensors.h"

//% \brief Informational class.
//%
//% The Info class contains general information/parameters, including the IPMI address.
class Info {
public:
	Info info() {}
	static unsigned char ipmi_address;
	static char serial_number[8];
	static Sensors::sensor_calibration_t calibration;

	const unsigned char fw_major = 0x01;
	const unsigned char fw_minor = 0x00;

	static inline void unlock() {
		MPUCTL0_H = 0xA5;
		MPUSAM |= MPUSEGIWE;
	}
	static inline void lock() {
		MPUSAM &= ~MPUSEGIWE;
		MPUCTL0_H = 0x00;
	}
};

extern Info info;

#endif /* INFO_H_ */
