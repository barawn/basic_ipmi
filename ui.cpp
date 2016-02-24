#include <msp430.h>
#include "ui.h"
#include "cmdline.h"
#include "clock.h"

#define UART0_8N1_38400_SMCLK_1MHZ
#include "msp430_uart_defs.h"
#include "strprintf.h"

const char UI::prompt[] = "TISC> ";
const char UI::init[] = "\x1B[H\x1B[40m\x1B[J\x1B[46m\x1B[K\x1B[38CLog\x1B[12H\x1B[K\x1B[35CCommands\x1B[24H\x1B[K\x1B[40m\x1B[13;23r\x1B[23H";
const char UI::probe[] = "\x1B[0c";
const char UI::statusbar_begin[] = "\x1B7\x1B[24H\x1B[46m";
const char UI::statusbar_end[] = "\x1B[40m\x1B8";
const char UI::log_begin[] = "\x1B[s\x1B[2;11r\x1B[11H";
const char UI::log_end[] = "\x1B[13;23r\x1B[u";
const char UI::backspace[] = "\x08 \x08";
const char UI::newline[] = "\n\r";

char UI::cmd_buffer[64];
char UI::rx_buffer[64];
char UI::log_line_buffer[82];
// Push the log buffer into FRAM so we don't worry about the size.
#pragma PERSISTENT
char UI::log_buffer[256] = { 0 };

unsigned char UI::log_wr;
unsigned char UI::log_rd;
unsigned char UI::cmd_wr;
unsigned char UI::cmd_rd;
unsigned char UI::rx_wr;
unsigned char UI::rx_rd;

UI ui;

void UI::initialize() {
	// Let's set up the UART.
	msp430_eusci_uart0_init();
	P2SEL1 |= BIT0 | BIT1;
    // Set up transmitter in DMA ch#0.
    // Trigger # is 15. Block transfer mode. Level sensitive. Destination doesn't increment.
    // Source does increment. Bytes on both source and destination.
	DMACTL0 = (DMACTL0 & 0xFF00) | 15;
    DMA0DA = (__SFR_FARPTR) (unsigned long) &UCA0TXBUF;
    // Add DMAIE later.
    DMA0CTL = DMADSTINCR_0 | DMASRCINCR_3 | DMADT_0 | DMALEVEL | DMADSTBYTE | DMASRCBYTE; // | DMAIE;
    log_rd = 0;
    log_wr = 0;
    cmd_rd = 0;
    cmd_wr = 0;
    rx_wr = 0;
    rx_rd = 0;
    echo_seen = false;
	// Enable interrupts.
	UCA0IE |= UCRXIE;
	DMA0CTL |= DMAIE;
}

void UI::process() {
	unsigned char nbytes;
	unsigned int cur_tick;

UI_begin_process:
	switch(__even_in_range(state, 34)) {
	case ui_NO_TERMINAL:
		// Probe periodically.
		cur_tick = clock.ticks;
		if (cur_tick > next_probe) {
			if (cur_tick - next_probe < 0x8000) {
				if (UART_BUSY()) return;
				UART_STRPUT(probe);
				next_probe = next_probe + probe_period*clock.ticks_per_second;
			}
		}
		if (!echo_seen) {
			nbytes = uart_available();
			while (nbytes) {
				char c;
				c = rx_buffer[rx_rd];
				rx_rd++;
				rx_rd = rx_rd % 64;
				nbytes--;
				// Chomp everything.
				vt100_parse(c);
				// Terminate if an echo was seen.
				if (echo_seen) {
					ui.logprintln("UI> saw term at %u", clock.ticks);
					nbytes = 0;
				}
			}
		}
		if (UART_BUSY()) return;
		if (!echo_seen) return;
		state = ui_SETUP_TERMINAL;
	case ui_SETUP_TERMINAL:
		UART_STRPUT(init);
		state = ui_SETUP_TERMINAL_WAIT;
		return;
	case ui_SETUP_TERMINAL_WAIT:
		if (UART_BUSY()) return;
		state = ui_PROMPT;
	case ui_PROMPT:
		UART_STRPUT(prompt);
		state = ui_PROMPT_WAIT;
		return;
	case ui_PROMPT_WAIT:
		if (UART_BUSY()) return;
		state = ui_IDLE;
	case ui_IDLE:
		// Probe.
		cur_tick = clock.ticks;
		if (cur_tick > next_probe) {
			if (!echo_seen) {
				// Since we only get to IDLE when echo_seen is set once, this means we sent out a new echo,
				// and never got a response. Switch to ui_NO_TERMINAL.
				ui.logprintln("UI> lost term at %u", clock.ticks);
				state = ui_NO_TERMINAL;
				goto UI_begin_process;
			}
			if (cur_tick - next_probe < 0x8000) {
				if (UART_BUSY()) return;
				echo_seen = false;
				UART_STRPUT(probe);
				next_probe = next_probe + probe_period*clock.ticks_per_second;
			}
		}
		// Do we have characters to process?
		nbytes = uart_available();
		while (nbytes) {
			char c;
			c = rx_buffer[rx_rd];
			// First check vt100 parsing.
			// If it swallows it, then we're done.
			if (!vt100_parse(c)) {
				// Check for special characters.
				if (c == 0x08) {
					// Ignore it if we're all the way back
					// at the beginning.
					if (cmd_wr) {
						if (UART_BUSY()) return;
						cmd_buffer[cmd_wr] = 0x0;
						if (cmd_rd == cmd_wr) cmd_rd--;
						cmd_wr--;
						UART_STRPUT(backspace);
					}
				} else if (c == '\r') {
					// Are we all caught up echoing?
					if (cmd_rd != cmd_wr) {
						unsigned int to_echo = cmd_wr - cmd_rd;
						// No. Check to see if the UART's available.
						if (UART_BUSY()) return;
						// It is. Now output the remainder.
						UART_STRNPUT(cmd_buffer+cmd_rd, to_echo);
						cmd_rd = cmd_wr;
					}
					// We're exiting out of this, so we need
					// to increment the pointer ourselves.
					rx_rd++;
					rx_rd = rx_rd % 64;
					cmd_buffer[cmd_wr] = 0x0;
					cmd_wr = 0;
					cmd_rd = 0;
					cmdline.interpret();
					state = ui_PROCESSING_WAIT;
					// Loop to processing.
					goto UI_begin_process;
				} else if (cmd_wr < 64) {
					cmd_buffer[cmd_wr] = c;
					cmd_wr++;
				}
			}
			rx_rd++;
			rx_rd = rx_rd % 64;
			nbytes--;
		}
		// We can't do anything if we're busy outputting
		// something. So wait in that case.
		if (UART_BUSY()) return;
		else if (cmd_wr > cmd_rd) {
			if (cmd_wr > cmd_rd) {
				unsigned int to_echo = cmd_wr - cmd_rd;
				UART_STRNPUT(cmd_buffer+cmd_rd, to_echo);
				cmd_rd = cmd_wr;
			}
		} else if (!log_empty()) {
			// Log's not empty. Print it out.
			UART_STRPUT(log_begin);
			state = ui_WAIT_LOG_OUT;
			return;
		}
		return;
	case ui_PROCESSING_WAIT:
		if (UART_BUSY()) return;
		UART_STRPUT(newline);
		state = ui_PROCESSING;
	case ui_PROCESSING:
		if (cmdline.handle()) {
			state = ui_PROMPT;
			goto UI_begin_process;
		}
		return;
	case ui_BEGIN_LOG_OUT:
		UART_STRPUT(log_begin);
		state = ui_WAIT_LOG_OUT;
		return;
	case ui_WAIT_LOG_OUT:
		if (UART_BUSY()) return;
		else {
			unsigned int transmit_cnt;
			unsigned int tmp;
			unsigned char c;
			if (log_empty()) {
				// ?!?!
				state = ui_END_LOG_OUT_WAIT;
				UART_STRPUT(log_end);
				return;
			}
			// Figure out how much to transmit
			log_line_buffer[0] = '\n';
			log_line_buffer[1] = '\r';
			transmit_cnt = 2;
			tmp = log_rd;
			do {
				c = log_buffer[log_rd];
				// FIXME make this conform to the log size
				log_rd++;
				// Terminator? If so, we're done.
				if (c == 0x0) break;
				// Nope. Add it.
				log_line_buffer[transmit_cnt++] = c;
				// Are we at max? If so, truncate and bail.
				if (transmit_cnt == transmit_max) break;
				// Out of characters? If so, we're done. Something weird happened.
				if (log_rd == log_wr) break;
			} while (1);
			UART_STRNPUT(log_line_buffer, transmit_cnt);
			state = ui_END_LOG_OUT;
			return;
		}
	case ui_END_LOG_OUT:
		if (UART_BUSY()) return;
		UART_STRPUT(log_end);
		state = ui_END_LOG_OUT_WAIT;
		return;
	case ui_END_LOG_OUT_WAIT:
		if (UART_BUSY()) return;
		state = ui_IDLE;
		return;
	case ui_BEGIN_STATUS_OUT:
		return;
	case ui_WAIT_STATUS_OUT:
		return;
	case ui_END_STATUS_OUT:
		return;
	case ui_END_STATUS_OUT_WAIT:
		return;
	}
}

bool UI::vt100_parse(char c) {
	switch (__even_in_range(vt100_state, vt100_STATE_MAX)) {
	case vt100_STATE_IDLE:
		if (c != 0x1B) return false;
		vt100_state = vt100_STATE_ESCAPE;
		return true;
	case vt100_STATE_ESCAPE:
		// Always swallow the character after an escape.
		if (c != '[') vt100_state = vt100_STATE_IDLE;
		else vt100_state = vt100_STATE_CSI;
		return true;
	case vt100_STATE_CSI:
		// Swallow all characters in the middle.
		if ((c < 0x40) || (c>0x7E)) return true;
		// OK, we've hit our last character.
		echo_seen = true;
		vt100_state = vt100_STATE_IDLE;
		return true;
	}
}

void UI::logputln(const char *str) {
	log_free();
	log_wr=strputs(log_buffer, log_wr, 0xFF, str);
	log_buffer[log_wr] = 0x0;
	// FIXME match with actual log buffer length
	log_wr++;
}

void UI::logprintln(const char *format, ...) {
	log_free();
	va_list a;
	va_start(a, format);
	log_wr=vstrprintf(log_buffer, log_wr, 0xFF, format, a);
	va_end(a);
	log_buffer[log_wr] = 0x0;
	// FIXME match with actual log buffer length
	log_wr++;
}

// Print to the output buffer.
void UI::println(const char *format, ...) {
	va_list a;
	va_start(a, format);
	unsigned int len;
	len = vstrprintf(log_line_buffer, 0, 0xFF, format, a);
	va_end(a);
	UART_STRNPUT(log_line_buffer, len);
}

#pragma vector=USCI_A0_VECTOR
__interrupt void
EUSCI_A0_Interrupt_Handler() {
	switch ( __even_in_range(UCA0IV, 8)) {
	case 0x00: break;
	case 0x02: // UCRXIFG
		UI::rx_buffer[UI::rx_wr] = UCA0RXBUF;
		UI::rx_wr++;
		UI::rx_wr = UI::rx_wr % 64;
		asm("	mov.b	#0x00, r4");
		__bic_SR_register_on_exit(LPM0_bits);
		break;
	case 0x04: // UCTXIFG
		break;
	case 0x06: // UCSTTIFG
		break;
	case 0x08: // UCTXCPTIFG
		break;
	default:
		__never_executed();
	}
}

#pragma vector=DMA_VECTOR
__interrupt void DMA_Handler() {
	switch ( __even_in_range(DMAIV, 16)) {
	case 0x00: return;
	case 0x02: asm("	MOV.B #0x00, r4"); __bic_SR_register_on_exit(LPM0_bits); return;
	case 0x04: return;
	case 0x06: return;
	case 0x08: return;
	case 0x0A: return;
	case 0x0C: return;
	case 0x0E: return;
	case 0x10: return;
	default: __never_executed();
	}
}
