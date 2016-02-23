#include <stdlib.h>
#include <string.h>
#include "ipmi_device_specific.h"
#include "info.h"
#include "sensors.h"

IPMI_Device thisDevice;

/*
 *
 * These are standard responses. Nothing here should have to be
 * changed.
 *
 */

//< \brief Get Device ID response.
unsigned char *IPMI_Device::copy_device_id(unsigned char *target) {
	*target++ = IPMI::IPMI_COMPLETION_OK;

	memcpy(target, &device_id, sizeof(device_id));
	target += sizeof(device_id);
	return target;
}

//< \brief Get SDR Info response.
unsigned char *IPMI_Device::copy_sdr_info(unsigned char operation,
										  unsigned char *target) {
	*target++ = IPMI::IPMI_COMPLETION_OK;
	// If 'operation' = 1, we return the total count (all LUNs).
	// We only have 1 LUN, so it's always the same.
	*target++ = NUM_SDRS;
	*target++ = SDR_FLAGS;
	return target;
}

//< \brief Reserve Device SDR Repository response.
unsigned char *IPMI_Device::reserve_device_sdr_repository(unsigned char *target) {
	*target++ = IPMI::IPMI_COMPLETION_OK;
	*target++ = 0x01;
	*target++ = 0x00;
	return target;
}

//< \brief Get Device SDR response.
unsigned char *IPMI_Device::copy_sdr(unsigned int sdr,
									 unsigned char offset,
									 unsigned char bytes,
									 unsigned char *target) {
	ipmi_sdr_header_t *hdr;
	const unsigned char *this_sdr;
	unsigned char to_copy;
	unsigned char *p;

	p = target + 1;
	*target = IPMI::IPMI_COMPLETION_OK;
	// We need 3 bytes for next record ID + completion code
	const unsigned char COPY_MAX = IPMI::TX_BUFFER_MAX - IPMI::IPMI_MIN_MESSAGE_LENGTH - 3;
	// Does the SDR exist?
	if (sdr >= NUM_SDRS) {
		// No.
		*target = IPMI::IPMI_COMPLETION_SENSOR_DATA_RECORD_NOT_PRESENT;
		return p;
	}
	// Yes.
	hdr = (ipmi_sdr_header_t *) sdrs[sdr];
	this_sdr = sdrs[sdr];

	// Check if the offset goes too far. Record length is number
	// of following bytes.
	if (offset >= hdr->record_length + sizeof(ipmi_sdr_header_t)) {
		*target = IPMI::IPMI_COMPLETION_PARAMETER_OUT_OF_RANGE;
		return p;
	}
	// It's a valid offset.
	if (bytes == 0xFF) {
		// Read Entire Record.
		bytes = hdr->record_length + sizeof(ipmi_sdr_header_t) - offset;
	} else {
		if (offset + bytes > hdr->record_length + sizeof(ipmi_sdr_header_t)) {
			*target = IPMI::IPMI_COMPLETION_REQUEST_DATA_TRUNCATED;
			bytes = hdr->record_length + sizeof(ipmi_sdr_header_t) - offset;
		}
	}
	if (bytes > COPY_MAX) {
		*target = IPMI::IPMI_COMPLETION_CANNOT_RETURN_NUMBER_OF_BYTES;
		return p;
	}
	if (sdr == NUM_SDRS-1) {
		*p++ = 0xFF;
		*p++ = 0xFF;
	} else {
		unsigned int id;
		id = hdr->record_id_lsb + (hdr->record_id_msb << 8);
		id++;
		*p++ = id & 0xFF;
		*p++ = (id >> 8) & 0xFF;
	}
	this_sdr += offset;
	memcpy(p, this_sdr, bytes);
	p += bytes;
	return p;
}

/*
 *
 * Device-Specific data structures and functions.
 *
 */

//% \brief Get Sensor Reading response.
unsigned char *IPMI_Device::copy_sensor_reading(unsigned char number, unsigned char *target) {
	int tmp;

	if (number > NUM_SDRS-1) {
		*target++ = IPMI::IPMI_COMPLETION_SENSOR_DATA_RECORD_NOT_PRESENT;
		return target;
	}

	*target++ = IPMI::IPMI_COMPLETION_OK;
	switch(__even_in_range(number<<1, (NUM_SDRS-1)<<1)) {
	case 0:
		// Temperature sensor.
		tmp = sensors.cal_values[0];
		// Divide by 4.
		tmp = tmp >> 2;
		// Bound range.
		if (tmp < -128) tmp = -128;
		if (tmp > 127) tmp = 127;
		*target++ = tmp & 0xFF;
		break;
	case 2:
		// Voltage sensor.
		tmp = sensors.cal_values[1];
		// Subtract nominal.
		tmp -= 3300;
		// Divide by 4.
		tmp = tmp >> 2;
		// Bound range.
		if (tmp < -128) tmp = -128;
		if (tmp > 127) tmp = 127;
		*target++ = tmp & 0xFF;
		break;
	}
	// State.
	*target++ = 0x40;
	// Thresholds.
	*target++ = 0x00;
	return target;
}

// Primary thing we need to do is loop through the SDRs and assign our IPMI address to them.
void IPMI_Device::initialize() {
	unsigned int i;
	ipmi_sensor_record_t *sensor;
	ipmi_mc_locator_record_t *mc;

	mc = (ipmi_mc_locator_record_t *) sdrs[0];
	mc->key[0] = info.ipmi_address;
	for (i=1;i<NUM_SDRS;i++) {
		sensor = (ipmi_sensor_record_t *) sdrs[i];
		sensor->key[0] = info.ipmi_address;
		sensor->key[1] = 0;
		sensor->key[2] = i-1;
	}
	// Copy calibration.
	sensor = (ipmi_sensor_record_t *) sdrs[1];
	sensor->description.m = info.calibration.uc_temp_m << 2;
}

#pragma PERSISTENT
IPMI_Device::ipmi_device_id_t IPMI_Device::device_id = {
		.id = 0x01,
		.revision = (1<<7) | 0x00,
		.ipmi = 0x51,
		.capabilities = IPMI_SENSOR_DEVICE,
		.manufacturer = { (IANA_ENTERPRISE_ID_OHIO_STATE>> 0) & 0xFF,
						  (IANA_ENTERPRISE_ID_OHIO_STATE>> 8) & 0xFF,
						  (IANA_ENTERPRISE_ID_OHIO_STATE>>16) & 0xFF },
		.product = { 0x00, 0x80 }
};

#pragma PERSISTENT
IPMI_Device::ipmi_mc_locator_record_t mc_locator_record = {
		.hdr = { 0x00, 0x00, 0x51, 0x12, sizeof(IPMI_Device::ipmi_mc_locator_record_t)-sizeof(IPMI_Device::ipmi_sdr_header_t)},
		.capabilities = IPMI_SENSOR_DEVICE,
		.entity_id = 0x11,
		.entity_instance = 0x00,
		.id_type_length = 0xC8,
		.id = { 'T','I','S','C',' ','V','2',' ' },
};
// Divide temp by 4, and scale up 'b' by 4.
// Result is 10^-2, and b = 30, with exponent 2.
#pragma PERSISTENT
IPMI_Device::ipmi_sensor_record_t mc_temp_sensor = {
		.hdr = { 0x01, 0x00, 0x51, 0x01, (sizeof(IPMI_Device::ipmi_sensor_record_t) - sizeof(IPMI_Device::ipmi_sdr_header_t)) },
		.entity_id = 0x11,
		.entity_instance = 0x00,
		.sensor_initialization = 0x1,
		.sensor_capabilities = 0x03,
		.sensor_type = 0x01,
		.event_reading_type_code = 0x01,
		.description = { .units = { 0x40, 0x01, 0x00 }, .b = 30, .rexp_bexp = 0xE2 },
		.id_type_length = 0xC8,
		.id = { 'M', 'S', 'P', '_', 'T', 'E', 'M', 'P' },
};
// Our units are millivolts. To range down to below 3V, though, we want:
// 3.3V - 512 mV + 512 mV , so a total range of 1024 mV, with 256 values.
// So m = 4, and we divide our inputs by 4.
// Result is 10^-3. b = 33, with exponent 2.
#pragma PERSISTENT
IPMI_Device::ipmi_sensor_record_t mc_volt_sensor = {
		.hdr = { 0x02, 0x00, 0x51, 0x01, (sizeof(IPMI_Device::ipmi_sensor_record_t) - sizeof(IPMI_Device::ipmi_sdr_header_t)) },
		.entity_id = 0x11,
		.entity_instance = 0x00,
		.sensor_initialization = 0x1,
		.sensor_capabilities = 0x03,
		.sensor_type = 0x02,
		.event_reading_type_code = 0x01,
		.description = { .units = { 0x40, 0x04, 0x00 }, .m = 4, .b = 33, .rexp_bexp = 0xD2 },
		.id_type_length = 0xC8,
		.id = { 'M', 'S', 'P', '_', 'V', 'O', 'L', 'T' },
};

#pragma PERSISTENT
unsigned char *IPMI_Device::sdrs[IPMI_Device::NUM_SDRS] = {
		(unsigned char *) &mc_locator_record,
		(unsigned char *) &mc_temp_sensor,
		(unsigned char *) &mc_volt_sensor
};

