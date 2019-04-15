
#include "stm32f0xx.h"
#include "common.h"
#include "global.h"
#include "rtc.h"


#define RTC_FLAG_BKP  0xabcdface

void RTC_Config(void)
{
    RTC_InitTypeDef RTC_InitStructure;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;

    if (RTC_ReadBackupRegister(RTC_BKP_DR0) != RTC_FLAG_BKP) {

        RCC->BDCR |= RCC_BDCR_RTCSEL_LSI;
        RCC->BDCR |= RCC_BDCR_RTCEN;

        RTC_WaitForSynchro();
        RTC_InitStructure.RTC_AsynchPrediv = 99;
        RTC_InitStructure.RTC_SynchPrediv =  399; /* (40KHz / 100) - 1 = 399*/
        RTC_InitStructure.RTC_HourFormat = RTC_HourFormat_24;
        RTC_Init(&RTC_InitStructure);

        RTC_SetCurrentTime(1, 0, 0); //1:0:0
        RTC_SetCurrentDate(1, 1, 4, 19); //2019-4-1,mon

        DPRINTF(("[%d] RTC First launch, init and config.... ok.\n", (int32_t)SysTicks));
        RTC_WriteBackupRegister(RTC_BKP_DR0, RTC_FLAG_BKP);
    } else {
        RTC_WaitForSynchro();
        DPRINTF(("[%d] RTC already runs.\n", (int32_t)SysTicks));
    }

    EXTI->PR = ((uint32_t)0x00020000);;
    EXTI->IMR  |= ((uint32_t)0x00020000);
    EXTI->RTSR |= ((uint32_t)0x00020000);

    NVIC_EnableIRQ(RTC_IRQn);
    NVIC_SetPriority(RTC_IRQn, 5);

    return ;
}

void RTC_SetCurrentTime(uint8_t hh, uint8_t mm, uint8_t ss)
{
    RTC_TimeTypeDef RTC_TimeStructure;

    RTC_TimeStructure.RTC_H12     = RTC_H12_AM;
    RTC_TimeStructure.RTC_Hours   = hh;
    RTC_TimeStructure.RTC_Minutes = mm;
    RTC_TimeStructure.RTC_Seconds = ss;

    RTC_SetTime(RTC_Format_BIN, &RTC_TimeStructure);

}

void RTC_SetCurrentDate(uint8_t weekday, uint8_t day, uint8_t mon, uint8_t year)
{
    RTC_DateTypeDef RTC_DateStructure;

    RTC_DateStructure.RTC_WeekDay = weekday;
    RTC_DateStructure.RTC_Month = mon;
    RTC_DateStructure.RTC_Date = day;
    RTC_DateStructure.RTC_Year = year;

    RTC_SetDate(RTC_Format_BIN, &RTC_DateStructure);

}

void RTC_SetAlarmSecond(void)
{
    RTC_AlarmTypeDef  RTC_AlarmStructure;

    RTC_AlarmCmd(RTC_Alarm_A, DISABLE);

    RTC_AlarmStructure.RTC_AlarmTime.RTC_H12 = RTC_H12_AM;
    RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours = 0;
    RTC_AlarmStructure.RTC_AlarmTime.RTC_Minutes = 0;
    RTC_AlarmStructure.RTC_AlarmTime.RTC_Seconds = 0;
    RTC_AlarmStructure.RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_Date;
    RTC_AlarmStructure.RTC_AlarmDateWeekDay = RTC_Weekday_Monday;
    RTC_AlarmStructure.RTC_AlarmMask = RTC_AlarmMask_All ;//RTC_AlarmMask_DateWeekDay;

    RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_A, &RTC_AlarmStructure);

    RTC_ITConfig(RTC_IT_ALRA, ENABLE);
    RTC_AlarmCmd(RTC_Alarm_A, ENABLE);
    return ;
}

int32_t RTC_GetCurrentTimeStamp(void)
{
    int32_t time;
    uint32_t tmpreg = (uint32_t)RTC->TR ;
    time = (tmpreg & 0xf) + (((tmpreg >> 4) & 0x7) * 10);
    time |= (((tmpreg >> 8) & 0xf) + (((tmpreg >> 12) & 0x7) * 10)) << 8;
    time |= (((tmpreg >> 16) & 0xf) + (((tmpreg >> 20) & 0x3) * 10)) << 16;
    return time;
}


void RTC_IRQHandler(void)
{
    if (RTC_GetITStatus(RTC_IT_ALRA) != RESET) {
        gEventUpdate = 1;
        EXTI->PR = ((uint32_t)0x00020000);;
        RTC_ClearITPendingBit(RTC_IT_ALRA);
    }
}

