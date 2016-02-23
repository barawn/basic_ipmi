#include <msp430.h>
#include <math.h>
#include "sensors.h"
#include "ui.h"
#include "clock.h"
#include "info.h"

Sensors sensors;

Sensors::sensor_state_t Sensors::state = Sensors::sensor_CONVERT_ADC;

#pragma PERSISTENT
unsigned int Sensors::raw_values[Sensors::MAX_SENSORS] = { 0 , 0 };
#pragma PERSISTENT
int Sensors::cal_values[Sensors::MAX_SENSORS] = { 0 , 0 };
#pragma NOINIT
unsigned int Sensors::tick_wait;

typedef struct adc12_cal {
	unsigned char tag;
	unsigned char len;
	unsigned int gain;
	unsigned int offset;
	unsigned int temp30_1v2;
	unsigned int temp85_1v2;
	unsigned int temp30_2v0;
	unsigned int temp85_2v0;
	unsigned int temp30_2v5;
	unsigned int temp85_2v5;
} adc12_cal_t;

typedef struct ref_cal {
	unsigned char tag;
	unsigned char len;
	unsigned int ref_1v2;
	unsigned int ref_2v0;
	unsigned int ref_2v5;
} ref_cal_t;

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

unsigned char *tag_find(unsigned char tag) {
	unsigned char *p;
	p = (unsigned char *) TLV_START;
	while (*p != tag) {
		unsigned char len;
		p++;
		len = *p++;
		p += len;
	}
	return p;
}

void Sensors::calibrate() {
	adc12_cal_t *adccal;
	ref_cal_t *refcal;
	unsigned int delta;
	unsigned int tmp;
	unsigned int centidegrees_per_count;
	unsigned long l;

	// Calibration is in info segment.
	info.unlock();

	adccal = (adc12_cal_t *) tag_find(TLV_ADC12CAL);
	delta = adccal->temp85_2v0;
	delta -= adccal->temp30_2v0;
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
	info.calibration.uc_temp_b = adccal->temp30_2v0;
	refcal = (ref_cal_t *) tag_find(TLV_REFCAL);

	// Find millivolts/count.
	// ref_2v0 is (vref/2.0)*2^15. Nominal conversion is (2000/2^12).
	// So multiplying them together gets (mV/count)*2^27.
	// Then divide by 2^10, to get (mV/count)*2^17.
	// Then take raw * l >> 2^16, which gives you the proper value
	// since the voltage is divided by 2.
	// I should also probably add offset correction here.
	l = ((unsigned long)refcal->ref_2v0) * 2000;
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

void Sensors::initialize() {
	// Lots of initializing to do.
	// Our channels are:
	// A13
	// A14
	// A15
	// A30
	// A31
	// Mux ON resistance is 4k max, capacitance is 15 pF.
	// (4k * 9.7 * 15 pF) = ~500 ns.
	// MODOSC is up to 5.4 MHz max. 4 ADC12CLK cycles is still fast enough for the rails.
	// A13/A14/A15 are set up for 4 ADC12CLK cycles.
	// The temperature sensor needs 30 us, so that's what we set up for.
	// ADC12CLK is SMCLK, so we select 192 ADC12CLK cycles.

	// Just work with A30/A31 for now.

	// ADC INITIALIZATION
	ADC12CTL0 = ADC12ON | ADC12SHT1_0 | ADC12SHT0_7 | ADC12MSC;
	ADC12CTL1 = ADC12CONSEQ_1 | ADC12SHP;
	ADC12CTL3 = ADC12TCMAP | ADC12BATMAP;
	// Set up our sequence.
	ADC12MCTL0 = ADC12VRSEL_1 | ADC12INCH_30;
	ADC12MCTL1 = ADC12VRSEL_1 | ADC12INCH_31 | ADC12EOS;
	// Set up our reference.
	while (REFCTL0 & REFGENBUSY);
	REFCTL0 = REFON | REFVSEL_1;
	ADC12IER0 |= ADC12IE1;
	ADC12CTL0 |= ADC12ENC | ADC12SC;
}

void Sensors::process() {
	unsigned int cur_tick;
	unsigned long tmp;

	switch (__even_in_range(state, sensor_STATE_MAX)) {
	case sensor_CONVERT_ADC:
		if (!(ADC12IFGR0 & ADC12IFG1)) return;
		raw_values[0] = ADC12MEM0;
		raw_values[1] = ADC12MEM1;
		// cal_values here are calibrated to allow for easy IPMI access.
		// The temperature sensor stays in raw counts.
		cal_values[0] = raw_values[0] - info.calibration.uc_temp_b;
		// Voltage sensor gets translated to millivolts.
		tmp = raw_values[1] - info.calibration.uc_volt_b;
		tmp = raw_values[1] * ((unsigned long) info.calibration.uc_volt_m);
		cal_values[1] = tmp >> 16;
		ADC12IER0 |= ADC12IE1;

		state = sensor_FINISH;
		tick_wait = clock.ticks + 5*clock.ticks_per_second;
		return;
	case sensor_FINISH:
		cur_tick = clock.ticks;
		if (cur_tick > tick_wait) {
			if (cur_tick - tick_wait < 0x8000) {
				ADC12CTL0 |= ADC12SC;
				state = sensor_CONVERT_ADC;
			}
		}
		return;
	default:
		__never_executed();
	}
}

#pragma vector=ADC12_VECTOR
__interrupt void ADC12_Handler() {
	ADC12IER0 = 0;
	asm("	mov.b	#0x00, r4");
	__bic_SR_register_on_exit(LPM0_bits);
}
