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
	//< Current time.
	static volatile unsigned int ticks;
	//< How many ticks there are in a second.
	const unsigned int ticks_per_second = 30;
	//< Initialize the clock.
	static void initialize();
	//< Use to determine how much time has passed.
	static inline bool time_has_passed(unsigned int target) {
		unsigned int cur_time;
		cur_time = ticks;
		return ((int) (cur_time - target) > 0);
	}
};

extern Clock clock;

#endif /* CLOCK_H_ */
