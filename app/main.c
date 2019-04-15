
#define MAIN

#include <stdio.h>

#include "stm32f0xx.h"
#include "common.h"
#include "global.h"
#include "board.h"
#include "uart.h"
#include "rtc.h"
#include "adc.h"
#include "loop.h"
#include "parameter.h"
#include "ver.h"

const char build_date[] = __DATE__;
const char build_time[] = __TIME__;
const char compile_by[] = COMPILE_BY;
const char compile_at[] = COMPILE_AT;
const char compiler[] = COMPILER ;
const char version_chk[] = VERSION_CHECK ;


int main(void)
{
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);
    Periph_Clock_Enable();
    GPIO_Config();
    TIM16_Config();

    Usart_StdioInit();

    MainVersion = (MAIN_VERSION << 20) | BUILD_VERSION;
    DPRINTF(("Main Booting ......\n"));
    FPRINTF(("Firmware version %08x (%d.%d)\n", MainVersion, MainVersion >> 20, (MainVersion & 0xfffff)));
    FPRINTF(("%s version.\n", version_chk));
    FPRINTF(("Compiled by %s@%s.\n", compile_by, compile_at));
    DPRINTF(("%s\n", compiler));
    FPRINTF(("Build Time: %s %s \n\n", build_date, build_time));

    Usart_CommInit();

    RTC_Config();
    ADC_Config();

    Parameter_Load();

    MainLoop();//go to main loop and never come back

    EPRINTF(("You shouldn't see me here.\n"));
}


