#ifndef RTC_H
#define RTC_H

#include "lib.h"
#include "types.h"

typedef struct virtual_rtc{
	volatile float counter;	//Keeps track of the ticks
	float freq;				//Frequency of virtual rtc
}virtual_rtc_t;

void tick();
void change_to_virtual_rtc(int pid);

void rtc_init();
int32_t rtc_open();
int32_t rtc_read(void* buf, int32_t nbytes);
int32_t rtc_write(const void* buf, int32_t nbytes);
int32_t rtc_close(int32_t fd);

void change_RTC_freq(uint32_t freq);

#endif // RTC_H
