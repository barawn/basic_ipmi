/*
 * strprintf.h
 *
 *  Created on: Feb 5, 2016
 *      Author: barawn
 */

#ifndef STRPRINTF_H_
#define STRPRINTF_H_

#include <stdarg.h>

bool isxdigit(char c);
unsigned char atox(char c);

char *strprintf(char *p, const char *format, ...);
unsigned int vstrprintf(char *p, unsigned int idx, unsigned int mask, const char *format, va_list a);

static inline unsigned int strputs(char *p, unsigned int idx, unsigned int mask, const char *str) {
	while (*str) {
		p[idx++] = *str++;
		idx &= mask;
	}
	return idx;
}

#endif /* STRPRINTF_H_ */
