/*
 * zcdetector.h
 *
 *  Created on: 27 сент. 2015 г.
 *      Author: complynx
 */

#ifndef APP_INCLUDE_ZCDETECTOR_H_
#define APP_INCLUDE_ZCDETECTOR_H_


typedef __INT_FAST8_TYPE__ proc_word;
typedef __UINT_FAST8_TYPE__ proc_uword;
#define PROC_WORD_SIZE (sizeof(__INT_FAST8_TYPE__))
#define ZCDETECTOR

#define times_size 6
typedef struct {
	u32 times[times_size];
	u32 detector_shift;
	u32 halfperiod;
	size_t current_time;
	proc_word last_overflow;
	proc_word tick_tock;
	proc_word pin_num;
	u16 pin_internal_bit;
	u32 pin_internal_gpio;
} ZeroCrossCalculator;

extern ZeroCrossCalculator zeroCrossCalculator;
void ZCD_tick();

#endif /* APP_INCLUDE_ZCDETECTOR_H_ */
