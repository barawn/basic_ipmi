/*
 * vt100.h
 *
 *  Created on: Feb 4, 2016
 *      Author: barawn
 */

#ifndef VT100_H_
#define VT100_H_

#define VT100_STRINGIFY2(X) #X
#define VT100_STRINGIFY(X) VT100_STRINGIFY2(X)

#define VT52_IDENT "\x1BZ"

#define VT100_ESCAPE 0x1B
#define VT100_IDENT "\x1B[0c"
#define VT100_RESET "\x1Bc"
#define VT100_CLEAR "\x1B[2J"
#define VT100_SAVECURSOR "\x1B[7"
#define VT100_RESTORECURSOR "\x1B[8"
#define VT100_SCROLLDOWN "\x1BD"
#define VT100_SCROLLREGION(top,bottom) "\x1B[" VT100_STRINGIFY(top) ";" VT100_STRINGIFY(bottom) "r"
#define VT100_REPEAT(num) "\x1B[" VT100_STRINGIFY(num) "b"
#define VT100_HOME "\x1B[H"


extern const char VT100_DefaultScroll[];

#endif /* VT100_H_ */
