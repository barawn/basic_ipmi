/*
 * clock.h
 *
 *  Created on: Feb 11, 2016
 *      Author: barawn
 */

#ifndef CLOCK_H_
#define CLOCK_H_

class Clock {
public:
	Clock() {}
	static unsigned int ticks;
	const unsigned int ticks_per_second = 30;
	void initialize();
};

extern Clock clock;

#endif /* CLOCK_H_ */
