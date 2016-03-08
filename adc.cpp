/*
 * adc.c
 *
 *  Created on: Mar 8, 2016
 *      Author: barawn
 */

#include "adc.h"
#include "platform.h"

ADC adc;

#pragma NOINIT
ADC::adc_calibration_t * ADC::adc_calib;
#pragma NOINIT
ADC::ref_calibration_t * ADC::ref_calib;

void ADC::initialize() {
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

	unsigned char *tmp;

	tmp = platform_tag_find(TLV_ADC12CAL);
	adc_calib = (adc_calibration_t *) tmp;

	tmp = platform_tag_find(TLV_REFCAL);
	ref_calib = (ref_calibration_t *) tmp;

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
	// Don't need to enable the reference. It gets enabled automatically during ADC
	// conversion.
	REFCTL0 = REFVSEL_1;
}

#pragma vector=ADC12_VECTOR
__interrupt void ADC12_Handler() {
	ADC12IER0 = 0;
	asm("	mov.b	#0x00, r4");
	__bic_SR_register_on_exit(LPM0_bits);
}
