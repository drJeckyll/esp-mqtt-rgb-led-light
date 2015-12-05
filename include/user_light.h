#ifndef __USER_LIGHT_H__
#define __USER_LIGHT_H__

#include "pwm.h"

struct light_saved_param {
	uint32  pwm_period;
	uint32  pwm_duty[PWM_CHANNEL];
};

void user_light_init(void);
uint32 user_light_get_duty(uint8 channel);
void user_light_set_duty(uint32 duty, uint8 channel);
uint32 user_light_get_period(void);
void user_light_set_period(uint32 period);

#endif
