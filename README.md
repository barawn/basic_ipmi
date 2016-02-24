# basic_ipmi

Basic framework for a simple IPMI MC.

# Objects

* UI - user interface, for over serial port
* IPMI - basic IPMI interface
* IPMI_Device - device-specific portion of IPMI (sensors, etc.)
* Sensor - actually performs sensor measurements
* Info - contains global, settable information
* CmdLine - command line processing in the UI
* Clock - the global timekeeping module (for delays, etc.)

# Serial Output

There are 2 ways to output data to a user: either via the console, or via the log.
Print to the console only if you're in a command-line function.
Print to the log everywhere else (and you can print to the log in a command-line function, too).

## Printing to the Console

First, check if the UART is currently busy, using UART_BUSY(). If it isn't, *you must yield to other
tasks*, which means *returning* from your function. "Keeping your place" in the function can be done
with state variables and a switch statement.

Next, to print using a (very restricted!!) printf-like syntax, use ui.println(format, ...). To print a
constant string, use UART_STRPUT(str). Note that UART_STRPUT(str) gets the length from sizeof(str),
so if you're not just printing a constant string array (instead, like a substring) use
UART_STRNPUT(str, len).

After *EVERY* UART call (ui.println(), UART_STRPUT(), UART_STRNPUT()) you *must* check UART_BUSY()
again! There is a strprintf() function if you want to programmatically build up a string before
sending it out.

Note that ui.println()'s buffer is 80 characters. So don't exceed that.

## Printing to the Log

Use ui.logprintln() for printf-like syntax, and ui.logputln() to print a constant string.
Good practice is to prefix lines with a brief indicator of who put the line in the log.

Note that rapid logprintln()'s don't cause a problem, but will cause log lines to be dropped,
so you may see missing lines in the output.

# Adding New Commands

To add new commands to the firmware, 

1) Add an enum entry to the command_t enum in CmdLine.h.
2) Add a corresponding entry into the CmdLine::table (5 characters: 'calibrate', 'caliba', etc. all match 'calib')
3) Add a handler function in CmdLine.cpp and add an entry in CmdLine::handle. It should return a bool.

In the handler function, returning 'false' means that handling is not complete, and the function
will be called again at the next wakeup.

Note that if you print to the UART as your last step, and you don't care when it's done,
just set command = COMMAND_NONE and return false. The COMMAND_NONE handler waits for the UART
to complete and returns true.

So for instance, if you need to write multiple lines, it would be:

bool handle_help() {
	static unsigned state = STATE_BEGIN;
	switch(__even_in_range(state, 4)) {
	case 0: if (UART_BUSY()) return false;
	        UART_STRPUT("Help line 1.\n\r");
	        state = 2;
	        return false;
	case 2: if (UART_BUSY()) return false;
			UART_STRPUT("Help line 2.\n\r");
			state = 4;
			return false;
	case 4: if (UART_BUSY()) return false;
			UART_STRPUT("Help line 3.\n\r");
			command = COMMAND_NONE;
			state = 0;
			return false;
	}
}

# Adding new sensors

To add a new sensor to the firmware,

1) Increment MAX_SENSORS in sensors.h
2) If the sensor is an onboard ADC, all you need to do is add additional ADC12MCTLx entries
   into the Sensors::initialize() function. Remove the ADC12EOS from the previous sequence,
   and put it at the end of your last ADC12MCTLx entry.
   
   Then in Sensors::process(), in the sensor_CONVERT_ADC state, read out the ADC,
   and do whatever manipulation is needed to put it into an easy-to-convert measurement.
   Store the raw value in raw_values, and the manipulated value in cal_values.
   
   If you need calibration, add calibration into the Info module.

3) If the sensor is NOT an onboard ADC, you will need to add states before SENSOR_FINISH
   to do whatever is needed to read from the sensor. Also move the state assignment to
   SENSOR_FINISH from the previous state, as well as the "tick_wait" assignment. Those
   should go in whatever your last state is.
   
   Make sure to put the raw read value into raw_values, and a value that's easy to convert
   into a physical measurement in cal_values.

4) Increment NUM_SDRS in ipmi_device_specific.h. Add an ipmi_sensor_record_t entry in
   ipmi_device_specific.cpp. Add any initialization (non-static calibrations?) to
   IPMI_Device::initialize(). Add a case to the switch in IPMI_Device::copy_sensor_reading
   filling in the sensor reading.
   
Keep in mind sensor readings are only 8 bits.
   