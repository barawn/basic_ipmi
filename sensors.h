/*
 * sensors.h
 *
 *  Created on: Feb 19, 2016
 *      Author: barawn
 */

#ifndef SENSORS_H_
#define SENSORS_H_

// Internal sensors:
//   uC temperature
//   1/2 AVCC
//   2.5V
//   PCI 2.5V
//   S6 1.2V
//
// How to use:
// 1: switch on 2.0 V reference
// 2: measure 1/2 AVCC, temperature, S6 1.2V
// 3: switch to AVCC reference
// 4: measure 2.5V, PCI 2.5V
//
// Maybe convert 2.5V measurements to calibrated reference, who knows.
class Sensors {
public:
	typedef enum sensor_state {
		sensor_CONVERT_ADC = 0,
		sensor_FINISH = 2,
		sensor_STATE_MAX = sensor_FINISH
	} sensor_state_t;

	typedef struct sensor_calibration {
		unsigned int uc_temp_m;
		unsigned int uc_temp_b;
		unsigned int uc_volt_m;
		unsigned int uc_volt_b;
	} sensor_calibration_t;

	const unsigned int MAX_SENSORS = 2;
	static const char *sensor_names[MAX_SENSORS];
	static unsigned int raw_values[MAX_SENSORS];
	static int cal_values[MAX_SENSORS];
	static unsigned int tick_wait;

	Sensors() {}
	static void initialize();
	static void process();
	static void calibrate();

	static sensor_state_t state;
};

extern Sensors sensors;

#endif /* SENSORS_H_ */
