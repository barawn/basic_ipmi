#include "info.h"

Info info;

#pragma DATA_SECTION(".infoB")
unsigned char Info::ipmi_address;
#pragma DATA_SECTION(".infoB")
char Info::serial_number[8];
#pragma DATA_SECTION(".infoC")
Sensors::sensor_calibration_t Info::calibration;

