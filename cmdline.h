/*
 * command.h
 *
 *  Created on: Feb 11, 2016
 *      Author: barawn
 */

#ifndef CMDLINE_H_
#define CMDLINE_H_

class CmdLine {
public:
	CmdLine(char *buf) : buffer(buf), command(COMMAND_NONE) {}
	void interpret();
	bool handle();
private:
	// COMMAND-LINE-SPECIFIC STUFF
	typedef enum enum_command {
		COMMAND_NONE = 0,			//< No command processing.
		COMMAND_HELP = 2,			//< Help.
		COMMAND_VERSION = 4,		//< Print out firmware version string
		COMMAND_CALIBRATE = 6,		//< Run the sensor calibration.
		COMMAND_SET = 8,
		COMMAND_MAX = 10
	} command_t;

	typedef enum enum_argument {
		SET_ADDRESS = 0,
		SET_SERIAL = 2,
		SET_MAX = 4
	} argument_t;

	bool handle_help();
	bool handle_version();
	bool handle_calibrate();
	bool handle_set();
	static const char unknown_command_string[];
	static const char help_string[];
	static const char ver_string[];
	static const char unknown_settable_string[];

	// GENERIC STUFF. One day I'll believe in subclassing.
	const char length = 5;
	char *const buffer;
	bool compare(const char *templ, const char *test);
	command_t command;
	static const char table[COMMAND_MAX/2-1][length+1];
	typedef struct set_argument {
		char name[length+1];
		void *address;
	} set_argument_t;
	static const set_argument_t settables[SET_MAX/2];
};

extern CmdLine cmdline;

#endif /* CMDLINE_H_ */
