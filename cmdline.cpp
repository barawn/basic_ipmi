#include <msp430.h>
#include "cmdline.h"
#include "ui.h"
#include "sensors.h"
#include "info.h"
#include "strprintf.h"

CmdLine cmdline(ui.cmd_buffer);

const char CmdLine::table[CmdLine::COMMAND_MAX/2-1][CmdLine::length+1] = {
		"help ",
		"versi",
		"calib",
		"set  ",
		"info ",
};

const CmdLine::set_argument_t CmdLine::settables[CmdLine::SET_MAX/2] = {
		{ "addre", &info.ipmi_address },
		{ "seria", info.serial_number }
};

const char CmdLine::unknown_command_string[] = "Unknown command!\n\r";
const char CmdLine::ver_string[] = "Version: testing\n\r";
const char CmdLine::help_string[] = "Commands: help, version, calibrate, set, info\n\r";
const char CmdLine::unknown_settable_string[] = "Set arguments: address, serial\n\r";

void CmdLine::interpret() {

	command = COMMAND_NONE;
	if (buffer[0] == 0x0) return;
	ui.logprintln("CMD> rec'd '%s'", buffer);
	do {
		unsigned int idx;
		idx = (((unsigned int) command) >> 1) - 1;
		if (compare(table[idx], buffer)) break;
		command = (command_t) ((unsigned int) command + 2);
	} while (command != COMMAND_MAX);
}

bool CmdLine::compare(const char *templ, const char *test) {
	unsigned int i;
	for (i=0;i<length;i++) {
		// End of command?
		if (templ[i] == ' ') return true;
		// End of command buffer?
		if (test[i] == 0x0) return true;
		if (templ[i] != test[i]) return false;
	}
	return true;
}

bool CmdLine::handle() {
	switch(__even_in_range(command, COMMAND_MAX)) {
	case COMMAND_NONE:
		if (!UART_BUSY()) return true;
		else return false;
	case COMMAND_HELP:
		return handle_help();
	case COMMAND_VERSION:
		return handle_version();
	case COMMAND_CALIBRATE:
		return handle_calibrate();
	case COMMAND_SET:
		return handle_set();
	case COMMAND_INFO:
		return handle_info();
	// Sleaze.
	case COMMAND_MAX:
		if (UART_BUSY()) return false;
		UART_STRPUT(unknown_command_string);
		command = COMMAND_NONE;
		return false;
	default:
		__never_executed();
	}
}

bool CmdLine::handle_info() {
	static unsigned int state = 0;
	if (UART_BUSY()) return false;

	switch(__even_in_range(state, 6)) {
	case 0: UART_STRPUT("Device Information:\n\r");
			state = 2;
			return false;
	case 2: ui.strput("Serial Number: ");
			ui.strnput(info.serial_number, 8);
			ui.println("\n\r");
			state = 4;
			return false;
	case 4: ui.println("IPMI Address: %X\n\r", info.ipmi_address);
			state = 6;
			return false;
	case 6: ui.println("Firmware: %u.%u\n\r", info.fw_major, info.fw_minor);
			state = 0;
			command = COMMAND_NONE;
			return false;
	default:
			__never_executed();
	}
}

bool CmdLine::handle_set() {
	unsigned int idx;
	unsigned int swval;
	char *arg;
	char *val;
	char *p;

	arg = 0x0;
	p = buffer;
	if (UART_BUSY()) return false;
	while (*p != ' ' && *p != 0x0) p++;
	if (*p == 0x0) idx = SET_MAX;
	else {
		// Terminate the original command.
		*p++ = 0x0;
		arg = p;
		for (idx=0;idx<SET_MAX/2;idx++) {
			if (compare(settables[idx].name, arg)) {
				p = arg;
				// Find the value.
				while (*p != ' ' && *p != 0x0) p++;
				if (*p == 0x0) {
					// Buffer ended before value.
					idx = SET_MAX/2;
					break;
				}
				// Terminate the argument.
				*p++ = 0x0;
				val = p;
				break;
			}
		}
	}
handle_set_switch:
	swval = idx << 1;
	switch(__even_in_range(swval, SET_MAX)) {
	// All settable 8 bit objects go here.
	case SET_ADDRESS:
		if (!isxdigit(val[0]) || !isxdigit(val[1])) {
			idx = SET_MAX/2;
			goto handle_set_switch;
		} else {
			unsigned char tmpval;
			unsigned char *settable;
			tmpval = atox(val[0]);
			tmpval <<= 4;
			tmpval |= atox(val[1]);

			info.unlock();
			settable = (unsigned char *) settables[idx].address;
			*settable = tmpval;
			info.lock();
			ui.println("Set %s [%u] to %X\n\r", arg, idx, tmpval);
			command = COMMAND_NONE;
			return false;
		}
	case SET_SERIAL:
		for (unsigned int i=0;i<8;i++) {
			if (val[i] == 0x0) {
				idx = SET_MAX/2;
				goto handle_set_switch;
			}
		}
		info.unlock();
		p = (char *) settables[idx].address;
		for (unsigned int i=0;i<8;i++) {
			p[i] = val[i];
		}
		info.lock();
		ui.print("Set %s [%u] to ", arg, idx);
		ui.strnput((char *) settables[idx].address, 8);
		ui.println("\n\r");
		command = COMMAND_NONE;
		return false;
	case SET_MAX:
		UART_STRPUT(unknown_settable_string);
		command = COMMAND_NONE;
		return false;
	default:
		__never_executed();
	}
}

bool CmdLine::handle_calibrate() {
	if (UART_BUSY()) return false;
	sensors.calibrate();
	ui.println("Calibration:\n\rTemp: m %u b %u\n\rVolt: m %u b %u\n\r",
				info.calibration.uc_temp_m,
				info.calibration.uc_temp_b,
				info.calibration.uc_volt_m,
				info.calibration.uc_volt_b);
	command = COMMAND_NONE;
	return false;
}

bool CmdLine::handle_help() {
	if (UART_BUSY()) return false;
	UART_STRPUT(help_string);
	command = COMMAND_NONE;
	return false;
}

bool CmdLine::handle_version() {
	if (UART_BUSY()) return false;
	UART_STRPUT(ver_string);
	command = COMMAND_NONE;
	return false;
}
