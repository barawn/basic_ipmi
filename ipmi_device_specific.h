/*
 * ipmi_device_specific.h
 *
 *  Created on: Feb 18, 2016
 *      Author: barawn
 */

#ifndef IPMI_DEVICE_SPECIFIC_H_
#define IPMI_DEVICE_SPECIFIC_H_

#include "ipmiv2.h"

#define IANA_ENTERPRISE_ID_OHIO_STATE (100)
#define IANA_ENTERPRISE_ID_UNIVERSITY_OF_HAWAII (2160)

#define IPMI_SENSOR_DEVICE 			0x01
#define IPMI_SDR_REPOSITORY_DEVICE 	0x02
#define IPMI_SEL_DEVICE				0x04
#define IPMI_FRU_INVENTORY_DEVICE	0x08
#define IPMI_IPMB_EVENT_RECEIVER	0x10
#define IPMI_IPMB_EVENT_GENERATOR	0x20
#define IPMI_BRIDGE					0x40
#define IPMI_CHASSIS_DEVICE			0x80

// Encode 4 ASCII chars in 3 bytes.
#define IPMI_6BIT_ASCII_QUAD( a , b , c, d ) \
	 ( ( ((b-0x20) & 0x3 )<<6 )|(   (a-0x20) & 0x3F)     )		\
	,( ( ((c-0x20) & 0xF )<<4 )|(  ((b-0x20) & 0x3C)>>2) )		\
	,( ( ((d-0x20) & 0x3F)<<2 )|(  ((c-0x20) & 0x30)>>4) )

class IPMI_Device {
public:
	IPMI_Device() {}

	static void initialize();

	typedef struct ipmi_device_id {
		unsigned char id;
		unsigned char revision;
		unsigned char fw_major;
		unsigned char fw_minor;
		unsigned char ipmi;
		unsigned char capabilities;
		unsigned char manufacturer[3];
		unsigned char product[2];
	} ipmi_device_id_t;

	typedef struct ipmi_sdr_header {
		unsigned char record_id_lsb;
		unsigned char record_id_msb;
		unsigned char sdr_version;
		unsigned char record_type;
		unsigned char record_length;
	} ipmi_sdr_header_t;

	typedef struct ipmi_sensor_threshold_masks {
		unsigned char lower_lsb;
		unsigned char lower_msb;
		unsigned char upper_lsb;
		unsigned char upper_msb;
		unsigned char settable_lsb;
		unsigned char settable_msb;
	} ipmi_sensor_threshold_masks_t;

	typedef struct ipmi_sensor_thresholds {
		unsigned char upper_nonrecoverable;
		unsigned char upper_critical;
		unsigned char upper_noncritical;
		unsigned char lower_nonrecoverable;
		unsigned char lower_critical;
		unsigned char lower_noncritical;
		unsigned char positive_hysteresis;
		unsigned char negative_hysteresis;
	} ipmi_sensor_thresholds_t;

	typedef struct ipmi_sensor_description {
		unsigned char units[3];
		unsigned char linearization;
		unsigned char m;
		unsigned char tolerance;
		unsigned char b;
		unsigned char accuracy;
		unsigned char accuracy_exp;
		unsigned char rexp_bexp;
	} ipmi_sensor_description_t;

	typedef struct ipmi_mc_locator_record {
		IPMI_Device::ipmi_sdr_header_t hdr;
		unsigned char key[2];
		unsigned char power_state;
		unsigned char capabilities;
		unsigned char reserved[3];
		unsigned char entity_id;
		unsigned char entity_instance;
		unsigned char oem;
		unsigned char id_type_length;
		unsigned char id[8];
	} ipmi_mc_locator_record_t;

	typedef struct ipmi_sensor_record {
		IPMI_Device::ipmi_sdr_header_t hdr;
		unsigned char key[3];
		unsigned char entity_id;
		unsigned char entity_instance;
		unsigned char sensor_initialization;
		unsigned char sensor_capabilities;
		unsigned char sensor_type;
		unsigned char event_reading_type_code;
		ipmi_sensor_threshold_masks_t threshold_masks;
		ipmi_sensor_description_t description;
		unsigned char analog_flags;
		unsigned char nominal;
		unsigned char nominal_max;
		unsigned char nominal_min;
		unsigned char sensor_max;
		unsigned char sensor_min;
		ipmi_sensor_thresholds_t thresholds;
		unsigned char reserved[2];
		unsigned char oem;
		unsigned char id_type_length;
		unsigned char id[8];
	} ipmi_sensor_record_t;

	static unsigned char *copy_device_id(unsigned char *target);
	static unsigned char *copy_sdr(unsigned int sdr,
							unsigned char offset,
							unsigned char bytes,
							unsigned char *target);
	static unsigned char *copy_sdr_info(unsigned char operation,
										unsigned char *target);
	static unsigned char *reserve_device_sdr_repository(unsigned char *target);
	static unsigned char *copy_sensor_reading(unsigned char number, unsigned char *target);
private:
	const unsigned char DEVICE_ID_LENGTH = 18;
	const unsigned char NUM_SDRS = 3;
	const unsigned char SDR_FLAGS = 1;
	static ipmi_device_id_t device_id;
	static unsigned char *sdrs[NUM_SDRS];
};

extern IPMI_Device thisDevice;

#endif /* IPMI_DEVICE_SPECIFIC_H_ */
