#include <msp430.h>
#include <math.h>
#include "sensors.h"
#include "ui.h"
#include "clock.h"
#include "info.h"
#include "adc.h"

// I2C Sensor Objects:
// 1: LTC4222 at 0x4F.
//    Status at 0xD2
//    Status at 0xD6
//    Source1 at 0xD8/0xD9
//    Source2 at 0xDA/0xDB
//    ADIN1 at 0xDC/0xDD
//    ADIN2 at 0xDE/0xDF
//    Sense1 at 0xE0/0xE1
//    Sense2 at 0xE2/0xE3
// 2: EMC1412 at 0x4C.
//    Temp1 at 0x00/0x29
//    Temp2 at 0x01/0x10

///// WORK IN PROGRESS

typedef struct i2c_register {
	unsigned int address;
	unsigned int reg;
} i2c_register_t;

i2c_register_t i2c_sequence[] = {
		{ .address = 0x4F, .reg = 0xD2 },
		{ .address = 0x4F, .reg = 0xD6 },
		{ .address = 0x4F, .reg = 0xD8 },
		{ .address = 0x4F, .reg = 0xD9 },
		{ .address = 0x4F, .reg = 0xDA },
		{ .address = 0x4F, .reg = 0xDB },
		{ .address = 0x4F, .reg = 0xDC },
		{ .address = 0x4F, .reg = 0xDD },
		{ .address = 0x4F, .reg = 0xDE },
		{ .address = 0x4F, .reg = 0xDF },
		{ .address = 0x4F, .reg = 0xE0 },
		{ .address = 0x4F, .reg = 0xE1 },
		{ .address = 0x4F, .reg = 0xE2 },
		{ .address = 0x4F, .reg = 0xE3 },
		{ .address = 0x4C, .reg = 0x00 },
		{ .address = 0x4C, .reg = 0x10 },
		{ .address = 0x4C, .reg = 0x01 },
		{ .address = 0x4C, .reg = 0x29 }
};

unsigned char i2c_sequence_buffer[sizeof(i2c_sequence)/sizeof(i2c_register_t)];
#pragma NOINIT
unsigned char i2c_sequence_count;

//// END WORK IN PROGRESS

// Overview of the sensor state machine:
// 1: Write
//
// 1: When ADC conversion complete, read out raw values and calibrate.
//    Begin


Sensors sensors;

Sensors::sensor_state_t Sensors::state = Sensors::sensor_CONVERT_ADC;

#pragma PERSISTENT
unsigned int Sensors::raw_values[Sensors::MAX_SENSORS] = { 0 , 0 };
#pragma PERSISTENT
int Sensors::cal_values[Sensors::MAX_SENSORS] = { 0 , 0 };
#pragma NOINIT
unsigned int Sensors::tick_wait;

const char *Sensors::sensor_names[Sensors::MAX_SENSORS] = {
		"MSP TEMP",
		"MSP VOLT"
};

// Calibration:
// 1855 - 1579 = 276 counts per 55 degrees
// So 5500/276: after 19 loops, leaves 256. 256 > 138, so 20 centidegrees per count.
// so it's 19 and 256/276ths.
// offset30 is 1579.
// Let's see how this works:
// 1543 - 1579 = -36
// -36 * 20 = -720
// Add 3000 = 2280 or 22.8 degrees.

unsigned int degrees_per_count;

// ADC temperature slope/offsets.
unsigned int centidegrees_per_count;
unsigned int minus25_offset;
// Voltage conversions:
// The VCC/2 conversion is from 2.0V, so it's 2000 mV/4096.
// We derive the calibration as:
// CAL_ADC20VREF_FACTOR * 2000
// this is now divided by 2^27,
// so we downshift by 2^10.
// e.g. if the factor is 0x7BBB, the calibration multiplier would be 61864.
// Then if we get 3699, we would multiply by 61864, and divide by 2^16.
// This leaves 3491, or 3.491 mV.

/** \brief Sensor calibration.
 *
 * Sensor calibration consists of reading in the ADC calibrations
 * and computing a value that can be used to convert the sensors
 * into physical units. Hardware multiplication can help, but we
 * have to deal with divides ourselves.
 *
 */
void Sensors::calibrate() {
	unsigned int delta;
	unsigned int tmp;
	unsigned int centidegrees_per_count;
	unsigned long l;

	// Calibration is in info segment.
	info.unlock();

	// Find the difference between 85C and 30C in ADC counts.
	delta = adc.adc_calib->temp85_2v0;
	delta -= adc.adc_calib->temp30_2v0;
	// Now divide 5500 centidegrees by that value.
	// The loop is just long division: how many 'deltas'
	// fit into 5500.
	centidegrees_per_count = 0;
	tmp = 55*100;
	while (tmp > delta) {
		tmp -= delta;
		centidegrees_per_count++;
	}

	// These are NOT the SDR values.
	// The sensor reading returns (raw - uc_temp_b)>>2.
	// M is returned as uc_temp_m << 2.
	// B is returned as 3000 (fixed).

	// These are in info segment.
	info.calibration.uc_temp_m = centidegrees_per_count;
	info.calibration.uc_temp_b = adc.adc_calib->temp30_2v0;
	// Find millivolts/count.
	// ref_2v0 is (vref/2.0)*2^15. Nominal conversion is (2000/2^12).
	// So multiplying them together gets (mV/count)*2^27.
	// Then divide by 2^10, to get (mV/count)*2^17.
	// Then take raw * l >> 2^16, which gives you the proper value
	// since the voltage is divided by 2.
	// I should also probably add offset correction here.
	l = ((unsigned long)adc.ref_calib->ref_2v0) * 2000;
	l >>= 10;
	// l's range should be restricted to 16 bits here.
	// It was restricted to 15 bits, then multiplied by an 11
	// bit number (26 bit range) and then downshifted by 10.
	info.calibration.uc_volt_m = (unsigned int) l;
	info.calibration.uc_volt_b = 0;
	// Note that the *other* voltages convert by using the
	// calibrated UC voltage, divided by 2^12.
	// So they multiply by cal_values[1] and divide by 2^12.
	// This is because they use AVCC as their reference voltage,
	// since their range is too high for anything else.
	info.lock();
}

/** \brief Sensor initialization.
 *
 */
void Sensors::initialize() {
	// ADC initialization is done in ADC::initialize()
	adc.initialize();
	adc.convert();
}

/** \brief Sensor processing function
 *
 */
void Sensors::process() {
	unsigned long tmp;

	switch (__even_in_range(state, sensor_STATE_MAX)) {
	case sensor_CONVERT_ADC:
		if (!adc.complete()) return;
		// Fill raw_values[0], raw_values[1].
		adc.get_values(raw_values);
		// cal_values here are calibrated to allow for easy IPMI access.
		// The temperature sensor stays in raw counts.
		cal_values[0] = raw_values[0] - info.calibration.uc_temp_b;
		// Voltage sensor gets translated to millivolts.
		tmp = raw_values[1] - info.calibration.uc_volt_b;
		tmp = raw_values[1] * ((unsigned long) info.calibration.uc_volt_m);
		cal_values[1] = tmp >> 16;

		tick_wait = clock.ticks + 5*clock.ticks_per_second;
		state = sensor_FINISH;
		return;
	case sensor_FINISH:
		if (clock.time_has_passed(tick_wait)) {
			adc.convert();
			state = sensor_CONVERT_ADC;
		}
		return;
	default:
		__never_executed();
	}
}
