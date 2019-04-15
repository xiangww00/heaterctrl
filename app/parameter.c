

#include "stm32f0xx.h"
#include "common.h"
#include "global.h"
#include "parameter.h"

#define FLASH_PARAMETER_START   ((uint32_t)0x0800f000)
#define FLASH_PARAMETER_PAGE    (1)
#define FLASH_PAGE_SIZE         (1024)

struct parameter_store  myParameter;
static int8_t gFlashNeedUpdate = 0;

void Flash_Load(uint32_t *buf, int32_t dwsize)
{
    uint32_t *addr;
    addr = (uint32_t *) FLASH_PARAMETER_START ;

    for (int i = 0; i < dwsize; i++) {
        *buf ++ = *addr++;
    }
    return ;
}

int32_t Flash_Store(uint32_t *buf, int32_t dwsize)
{
    int i;
    int err = 0;
    uint32_t address;
    uint32_t Data;

    address =  FLASH_PARAMETER_START ;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    for (i = 0; i < FLASH_PARAMETER_PAGE; i++) {
        if (FLASH_ErasePage(FLASH_PARAMETER_START + (FLASH_PAGE_SIZE * i)) != FLASH_COMPLETE) {
            err = -1;
            goto exit;
        }
    }

    for (i = 0; i < dwsize; i++) {
        if (FLASH_ProgramWord(address, buf[i]) == FLASH_COMPLETE) {
            address = address + 4;
        } else {
            err = -2;
            goto exit;
        }
    }

    address =  FLASH_PARAMETER_START ;
    for (i = 0; i < dwsize; i++) {
        Data = *(uint32_t *)address;
        if (Data != buf[i]) {
            err = -3;
            goto exit;
        }
        address += 4;
    }

exit:
    FLASH_Lock();
    return err;
}


void Parameter_SetDefault(void)
{
    int i;
    myParameter.Marker = PARA_MAKER ;

    for (i = 0; i < 3; i++) {
        myParameter.hCtrl[i].TempTarget = 95;
        myParameter.hCtrl[i].TempMax = 98;
        myParameter.hCtrl[i].TempThreshold = 5;
        myParameter.hCtrl[i].TempDisinfect = 95;
        myParameter.hCtrl[i].DisinfectPeriod = 3;
        myParameter.hCtrl[i].DisinfectTimeout = 60;
        myParameter.hCtrl[i].HeatEnable = 0;
        myParameter.hCtrl[i].DisinfectEnable = 0;

        myParameter.hCtrl[i].WorkStart.rtcTime.Res = 0;
        myParameter.hCtrl[i].WorkStart.rtcTime.Hour = 8;
        myParameter.hCtrl[i].WorkStart.rtcTime.Minute = 0;
        myParameter.hCtrl[i].WorkStart.rtcTime.Second = 0;

        myParameter.hCtrl[i].WorkEnd.rtcTime.Res = 0;
        myParameter.hCtrl[i].WorkEnd.rtcTime.Hour = 18;
        myParameter.hCtrl[i].WorkEnd.rtcTime.Minute = 0;
        myParameter.hCtrl[i].WorkEnd.rtcTime.Second = 0;

        myParameter.hCtrl[i].DisinfectTime.rtcTime.Res = 0;
        myParameter.hCtrl[i].DisinfectTime.rtcTime.Hour = 4;
        myParameter.hCtrl[i].DisinfectTime.rtcTime.Minute = 50;
        myParameter.hCtrl[i].DisinfectTime.rtcTime.Second = 0;

    }

}

int8_t Parameter_CheckValid(void)
{
    int i;
    int8_t ret = 0;
    for (i = 0; i < 3; i++) {
        if ((myParameter.hCtrl[i].TempTarget > 100) || (myParameter.hCtrl[i].TempTarget < 0)) {
            myParameter.hCtrl[i].TempTarget = 95;
            ret = -1;
        }

        if ((myParameter.hCtrl[i].TempMax > 100) || (myParameter.hCtrl[i].TempMax < 0)) {
            myParameter.hCtrl[i].TempMax = 98;
            ret = -1;
        }

        if ((myParameter.hCtrl[i].TempThreshold > 50) || (myParameter.hCtrl[i].TempThreshold <= 0)) {
            myParameter.hCtrl[i].TempThreshold = 5;
            ret = -1;
        }

        if ((myParameter.hCtrl[i].TempDisinfect > 100) || (myParameter.hCtrl[i].TempDisinfect < 0)) {
            myParameter.hCtrl[i].TempDisinfect = 95;
            ret = -1;
        }

        if ((myParameter.hCtrl[i].DisinfectPeriod > 60) || (myParameter.hCtrl[i].DisinfectPeriod < 0)) {
            myParameter.hCtrl[i].DisinfectPeriod = 3;
            ret = -1;
        }

        if ((myParameter.hCtrl[i].DisinfectTimeout > 300) || (myParameter.hCtrl[i].DisinfectTimeout < 0)) {
            myParameter.hCtrl[i].DisinfectTimeout = 60;
            ret = -1;
        }

        if ((myParameter.hCtrl[i].HeatEnable > 1) || (myParameter.hCtrl[i].HeatEnable < 0)) {
            myParameter.hCtrl[i].HeatEnable = 0;
            ret = -1;
        }

        if ((myParameter.hCtrl[i].DisinfectEnable > 1) || (myParameter.hCtrl[i].DisinfectEnable < 0)) {
            myParameter.hCtrl[i].DisinfectEnable = 0;
            ret = -1;
        }

        if (myParameter.hCtrl[i].WorkStart.rtcTime.Res != 0) {
            myParameter.hCtrl[i].WorkStart.rtcTime.Res = 0;
            ret = -1;
        }

        if (myParameter.hCtrl[i].WorkStart.rtcTime.Hour >= 24) {
            myParameter.hCtrl[i].WorkStart.rtcTime.Hour = 8;
            ret = -1;
        }

        if (myParameter.hCtrl[i].WorkStart.rtcTime.Minute >= 59) {
            myParameter.hCtrl[i].WorkStart.rtcTime.Minute = 0;
            ret = -1;
        }

        if (myParameter.hCtrl[i].WorkStart.rtcTime.Second >= 59) {
            myParameter.hCtrl[i].WorkStart.rtcTime.Second = 0;
            ret = -1;
        }

        if (myParameter.hCtrl[i].WorkEnd.rtcTime.Res != 0) {
            myParameter.hCtrl[i].WorkEnd.rtcTime.Res = 0;
            ret = -1;
        }

        if (myParameter.hCtrl[i].WorkEnd.rtcTime.Hour >= 24) {
            myParameter.hCtrl[i].WorkEnd.rtcTime.Hour = 8;
            ret = -1;
        }

        if (myParameter.hCtrl[i].WorkEnd.rtcTime.Minute >= 59) {
            myParameter.hCtrl[i].WorkEnd.rtcTime.Minute = 0;
            ret = -1;
        }

        if (myParameter.hCtrl[i].WorkEnd.rtcTime.Second >= 59) {
            myParameter.hCtrl[i].WorkEnd.rtcTime.Second = 0;
            ret = -1;
        }

        if (myParameter.hCtrl[i].DisinfectTime.rtcTime.Res != 0) {
            myParameter.hCtrl[i].DisinfectTime.rtcTime.Res = 0;
            ret = -1;
        }

        if (myParameter.hCtrl[i].DisinfectTime.rtcTime.Hour >= 24) {
            myParameter.hCtrl[i].DisinfectTime.rtcTime.Hour = 8;
            ret = -1;
        }

        if (myParameter.hCtrl[i].DisinfectTime.rtcTime.Minute >= 59) {
            myParameter.hCtrl[i].DisinfectTime.rtcTime.Minute = 0;
            ret = -1;
        }

        if (myParameter.hCtrl[i].DisinfectTime.rtcTime.Second >= 59) {
            myParameter.hCtrl[i].DisinfectTime.rtcTime.Second = 0;
            ret = -1;
        }

    }

    return ret;
}



int32_t Parameter_Store(void)
{
    int32_t result;

    result = Flash_Store((uint32_t *)&myParameter, (sizeof(myParameter) + 3) / 4);
    if (result < 0) {
        DPRINTF(("Flash writting error. %d \n", result));
    }

    return result;

}

void Parameter_Load(void)
{
    int8_t need2store = 0;

    Flash_Load((uint32_t *)&myParameter, (sizeof(myParameter) + 3) / 4) ;
    if (myParameter.Marker != PARA_MAKER) {
        EPRINTF(("Parameter table was first load, do init...\n"));
        Parameter_SetDefault();
        need2store  = 1;
    }

    if (Parameter_CheckValid() < 0) {
        need2store = 1;
    }

    if (need2store) {
        Parameter_Store();
    }

    DPRINTF(("Load flash ok.\n"));
    for (int i = 0; i < 3; i++) {
        DPRINTF(("Control Temp: %d %d \n", i, myParameter.hCtrl[i].TempTarget));
        DPRINTF(("Disinfect time:%x \n", myParameter.hCtrl[i].DisinfectTime.Timer));
    }
    return ;
}

void Parameter_SetUpdate(void)
{
    gFlashNeedUpdate  = 1;
}

int32_t Parameter_Flush(void)
{
    int32_t result = Parameter_Store();
    DPRINTF(("[%d] Flush setup data into flash. \n", (int32_t) SysTicks));
    if (result == 0) {
        gFlashNeedUpdate  = 0;
    }
    return result;
}

/*
 * We should use a new algorithm to write the parameter into flash later.
 */
void Parameter_Service(void)
{
    static int64_t lTime2Check = 0;

    if (SysTicks < lTime2Check) {
        return ;
    }

    lTime2Check  = SysTicks + (int64_t)(30 * 60 * 1000);

    if (gFlashNeedUpdate) {
        Parameter_Flush();
    }

    return  ;

}
