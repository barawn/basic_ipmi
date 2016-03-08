/*
 * adc.h
 *
 *  Created on: Mar 8, 2016
 *      Author: barawn
 */

#ifndef ADC_H_
#define ADC_H_

#include <msp430.h>

/** \brief ADC support class.
 *
 * Simple functions for interacting with the ADC.
 *
 */

class ADC {
public:
	typedef struct adc_calibration {
		unsigned char tag;					//< Identifier tag of the ADC calibration block.
		unsigned char len;					//< Length of the ADC calibration block.
		unsigned int gain;					//< Inverse gain of the ADC, times 2^15. To correct ADC, multiply by gain, divide by 2^15.
		unsigned int offset;				//< Offset of the ADC. To correct ADC, add offset (after gain correction)
		unsigned int temp30_1v2;			//< Raw reading of temperature sensor at 30 C using 1.2V internal reference
		unsigned int temp85_1v2;			//< Raw reading of temperature sensor at 85 C using 1.2V internal reference.
		unsigned int temp30_2v0;			//< Raw reading of temperature sensor at 30 C using 2.0V internal reference.
		unsigned int temp85_2v0;			//< Raw reading of temperature sensor at 85 C using 2.0V internal reference.
		unsigned int temp30_2v5;			//< Raw reading of temperature sensor at 30 C using 2.5V internal reference.
		unsigned int temp85_2v5;			//< Raw reading of temperature sensor at 85 C using 2.5V internal reference.
	} adc_calibration_t;
	typedef struct ref_calibration {
		unsigned char tag;					//< Identifier tag of the REF calibration block.
		unsigned char len;					//< Length of the REF calibration block.
		unsigned int ref_1v2;				//< Ratio of 1.2V reference to 1.2V, times 2^15
		unsigned int ref_2v0;				//< Ratio of 2.0V reference to 2.0V, times 2^15
		unsigned int ref_2v5;				//< Ratio of 2.5V reference to 2.5V, times 2^15
	} ref_calibration_t;

	ADC() {}
	static void initialize();
	static inline bool complete() {
		return (ADC12IFGR0 & ADC12IFG1);
	}
	static inline void get_values(unsigned int *arr) {
		arr[0] = ADC12MEM0;
		arr[1] = ADC12MEM1;
	}
	static inline void convert() {
		ADC12IER0 |= ADC12IE1;
		ADC12CTL0 |= ADC12SC;
	}
	static adc_calibration_t *adc_calib;
	static ref_calibration_t *ref_calib;
};

extern ADC adc;

#endif /* ADC_H_ */
