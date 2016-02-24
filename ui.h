/*
 * ui.h
 *
 *  Created on: Feb 10, 2016
 *      Author: barawn
 */

#ifndef UI_H_
#define UI_H_

class UI {
public:
	// State definitions.
	typedef enum UI_state {
		ui_NO_TERMINAL = 0,			//< No terminal detected. Do nothing except send out probes and look for responses.
									//< Exits to: SETUP when terminal detected.
		ui_SETUP_TERMINAL = 2,		//< Print the terminal setup string.
									//< Exits to: SETUP_TERMINAL_WAIT immediately.
		ui_SETUP_TERMINAL_WAIT = 4, //< Wait for terminal string to be sent.
									//< Exits to: PROMPT when not transmitting.
		ui_PROMPT = 6,				//< Prints the prompt string.
									//< Exits to: PROMPT_WAIT immediately.
		ui_PROMPT_WAIT = 8,			//< Wait for prompt string to finish.
									//< Exits to: IDLE when not transmitting.
		ui_IDLE = 10,				//< Doing nothing. Echoing characters.
									//< Exits to: PROCESSING_WAIT on a \n
									//<           BEGIN_LOG_OUT    on log_available() and not transmitting
									//<           BEGIN_STATUS_OUT on ticks() > 30+prev_ticks and not transmitting
									//<
		ui_PROCESSING_WAIT = 12,	//< Command fully received (got a '\n'). Waiting for TX completion.
									//< Exits to: PROCESSING when not transmitting
		ui_PROCESSING = 14,			//< Handling the command.
									//< Exits to: PROMPT when processing completed.
		ui_BEGIN_LOG_OUT = 16,		//< Switching to log window.
									//< Exits to: WAIT_LOG_OUT immediately.
		ui_WAIT_LOG_OUT = 18,		//< Log window now active. Print log output line.
									//< Exits to: END_LOG_OUT when log_nchars == 0 and not transmitting
		ui_END_LOG_OUT = 20,		//< Switch back to main window.
									//< Exits to END_LOG_OUT_WAIT immediately.
		ui_END_LOG_OUT_WAIT = 22,	//< Switch back completed.
									//< Exits to IDLE when not transmitting
		ui_BEGIN_STATUS_OUT = 24,	//< Switching to status bar.
									//< Exits to: WAIT_STATUS_OUT immediately.
		ui_WAIT_STATUS_OUT = 26,	//< Status bar now active. Print status line.
									//< Exits to: END_STATUS_OUT immediately.
		ui_END_STATUS_OUT = 28,		//< Switch back to main window when not transmitting.
									//< Exits to: END_STATUS_OUT_WAIT after switch back prints.
		ui_END_STATUS_OUT_WAIT = 30,//< Switch back completed.
									//< Exits to IDLE when not transmitting.
		ui_STATE_MAX = 30
	} UI_state_t;
	typedef enum vt100_state {
		vt100_STATE_IDLE = 0,
		vt100_STATE_ESCAPE = 2,
		vt100_STATE_CSI = 4,
		vt100_STATE_MAX = 4
	} vt100_state_t;

	UI() {}
	static void initialize();
	static void process();
	static void logputln(const char *str);
	static void logprintln(const char *format, ...);
	static void println(const char *format, ...);
	static void print(const char *format, ...);
	static void strput(const char *s);
	static void strnput(const char *s, unsigned char n);

	static bool echo_seen;
	// Hardware receive buffer. Stored when system is doing something else. Commands are max 64 chars,
	// so this is fine. Any automated interaction should be working via 'expect'.
	static char rx_buffer[64];
	// Buffer to copy commands to.
	static char cmd_buffer[64];
	// Log that everyone else prints to.
	static char log_buffer[256];
	// Transmit buffer.
	static char log_line_buffer[82];
	// Pointers for hardware receive buffer.
	static unsigned char rx_rd;
	static unsigned char rx_wr;
	// Pointers for log.
	static unsigned char log_rd;
	static unsigned char log_wr;
	// Pointers for command echoing.
	static unsigned char cmd_rd;
	static unsigned char cmd_wr;
	// Pointer for transmit buffer.
	static unsigned char line_wr;
private:
	static UI_state_t state;
	static vt100_state_t vt100_state;
	static unsigned int next_probe;
	const unsigned char log_min_free = 81;
	const unsigned char log_max_used = 174;
	const unsigned char transmit_max = 82;
	// send every 2 seconds
	const unsigned int probe_period = 2;
	static const char prompt[];
	static const char init[];
	static const char probe[];
	static const char statusbar_begin[];
	static const char statusbar_end[];
	static const char log_begin[];
	static const char log_end[];
	static const char backspace[];
	static const char newline[];
	static const char unknown_command[];

	static bool vt100_parse(char c);

	static unsigned char uart_available() {
		unsigned char tmp;
		tmp = (unsigned char) (rx_wr - rx_rd);
		tmp = tmp % 64;
		return tmp;
	}
	static bool log_empty() {
		return (log_wr == log_rd);
	}
	static unsigned char log_used() {
		// FIXME make this conform to the log size
		// if log_wr-log_rd, we can write 254 characters, right? cuz we can't add a character
		// when log_wr = 255.
		// if log_wr = 1, and log_rd = 0, we can write 253 characters.
		return (log_wr - log_rd);
	}
	static void log_free() {
		while (log_used() > log_max_used) {
			while (log_buffer[log_rd] != 0x0 && (log_rd != log_wr))
				// FIXME make this conform to the log size
				log_rd++;
			if (log_rd == log_wr) return;
			// FIXME make this conform to the log size
			log_rd++;
		}
	}
};

// There's no easy way to make these real inlinable functions, unfortunately.

#define UART_STRPUT(str) 							   \
		DMA0SA = (__SFR_FARPTR) (unsigned long) (str); \
		DMA0SZ = sizeof(str)-1;						   \
		DMA0CTL |= DMAEN
#define UART_STRNPUT(str, len)						   \
		DMA0SA = (__SFR_FARPTR) (unsigned long) (str); \
		DMA0SZ = len;								   \
		DMA0CTL |= DMAEN
#define UART_BUSY() (DMA0CTL & DMAEN)

extern UI ui;

#endif /* UI_H_ */
