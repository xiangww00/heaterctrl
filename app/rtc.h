
#ifndef _RTC_H_
#define _RTC_H_


void RTC_SetAlarmSecond(void) ;
void RTC_SetCurrentTime(uint8_t hh, uint8_t mm, uint8_t ss);
void RTC_SetCurrentDate(uint8_t weekday, uint8_t day, uint8_t mon, uint8_t year);
int32_t RTC_GetCurrentTimeStamp(void);
void RTC_Config(void);

#endif

