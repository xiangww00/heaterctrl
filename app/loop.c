
#include "stm32f0xx.h"
#include "common.h"
#include "global.h"
#include "board.h"
#include "uart.h"
#include "rtc.h"
#include "adc.h"
#include "comslave.h"
#include "loop.h"

#define TEMP_AVG                5
#define EV_TIMEOUT              60
#define EV_STOP_DELAY           3
#define HEATING_TIMEOUT         8
#define HEATING_PAUSE_TIME      16
#define HEATING_RETRY           3
#define HEATING_MAXTEMP_TIME    1

#define HEAT_ON                 1
#define HEAT_OFF                0

typedef enum {
    HT_STA_IDLE = 0,
    HT_STA_ONDUTY,
    HT_STA_HTING,
    HT_STA_DO_DISINF,
    HT_STA_START_DISINF,
    HT_STA_WAIT_HTING,
    HT_STA_WARNING,
} HT_STATE;

typedef enum {
    EV_STA_IDLE,
    EV_STA_START,
    EV_STA_ON,
    EV_STA_STOP,
    EV_STA_WARNING,
} EV_STATE;

static HeatControl gHController[3] = {
    {
        .Heating = Heating_Ctrl1,
        .CheckTopState = Check1Top,
        .CheckBottomState = Check1Bottom,

        .Setting = &myParameter.hCtrl[0],

        .HStateMachine = HT_STA_IDLE ,

        .HeatState = 0,
        .OnDuty = 0,
        .IsDisinfect = 0,
        .TSensorFail = 0,
        .DisinfectTimeout = 0,
        .TopState = 0,
        .BottomState = 0,
        .WarningStatus = 0,

        .MyTemperature = 0,

        .HeatStartTime = 0,
        .DisinfectStartTime = 0,

    },
    {
        .Heating = Heating_Ctrl2,
        .CheckTopState = Check2Top,
        .CheckBottomState = Check2Bottom,

        .Setting = &myParameter.hCtrl[1],

        .HStateMachine = HT_STA_IDLE ,

        .HeatState = 0,
        .OnDuty = 0,
        .IsDisinfect = 0,
        .TSensorFail = 0,
        .DisinfectTimeout = 0,
        .TopState = 0,
        .BottomState = 0,
        .WarningStatus = 0,

        .MyTemperature = 0,

        .HeatStartTime = 0,
        .DisinfectStartTime = 0,

    },
    {
        .Heating = Heating_Ctrl3,
        .CheckTopState = Check3Top,
        .CheckBottomState = Check3Bottom,

        .Setting = &myParameter.hCtrl[2],

        .HStateMachine = HT_STA_IDLE ,

        .HeatState = 0,
        .OnDuty = 0,
        .IsDisinfect = 0,
        .TSensorFail = 0,
        .DisinfectTimeout = 0,
        .TopState = 0,
        .BottomState = 0,
        .WarningStatus = 0,

        .MyTemperature = 0,

        .HeatStartTime = 0,
        .DisinfectStartTime = 0,

    },
};

static MainControl gMainController = {
    .ValveCtrl = EValve_Ctrl,
    .pHeatController[0] = &gHController[0],
    .pHeatController[1] = &gHController[1],
    .pHeatController[2] = &gHController[2],

    .ValveState = 0,
    .ValveTimer = 0,

    .VStateMachine = EV_STA_IDLE,

};


MainControl *GetController(void)
{
    return (&(gMainController));
}


static void Controller_Init(void)
{
    MainControl *pMCtrl = GetController();

    for (int i = 0; i < 3; i++) {
        pMCtrl-> pHeatController[i]->Heating(0, &(pMCtrl->pHeatController[i]->HeatState));
    }
    pMCtrl->ValveCtrl(0, &pMCtrl->ValveState);
}

uint8_t Ctrl_GetlWarn(void)
{
    uint8_t warning = 0;
    MainControl *pMCtrl = GetController();
    if (pMCtrl->VStateMachine == EV_STA_WARNING)
        warning |= 0x40;

    if (pMCtrl->pHeatController[0]->HStateMachine == HT_STA_WARNING)
        warning |= 0x01;

    if (pMCtrl->pHeatController[1]->HStateMachine == HT_STA_WARNING)
        warning |= 0x02;

    if (pMCtrl->pHeatController[2]->HStateMachine == HT_STA_WARNING)
        warning |= 0x04;

    return warning;

}

void Ctrl_CleanWarn(int8_t which)
{
    HeatControl *pCurHCtrl;
    MainControl *pMCtrl = GetController();
    if (which == 0) {
        if (pMCtrl-> VStateMachine == EV_STA_WARNING) {
            pMCtrl-> VStateMachine =  EV_STA_IDLE;
        }
    } else {
        if (which < 4 && which > 0) {
            which--;
            pCurHCtrl = pMCtrl->pHeatController[which];
            if (pCurHCtrl->HStateMachine == HT_STA_WARNING) {
                pCurHCtrl->WarningStatus = 0;
                pCurHCtrl->HStateMachine = HT_STA_IDLE;
            }
        }
    }

    return ;
}

uint32_t Ctrl_GetWarnStatus(void)
{
    uint32_t WarnStatus = 0;
    MainControl *pMCtrl = GetController();

    WarnStatus |= (pMCtrl->pHeatController[0]->WarningStatus) ;
    WarnStatus |= (pMCtrl->pHeatController[1]->WarningStatus << 8) ;
    WarnStatus |= (pMCtrl->pHeatController[2]->WarningStatus << 16) ;

    return WarnStatus;
}


void MainLoop(void)
{
    int i, j;

    int64_t lCheckTempTime = 0;
    int64_t lCheckLevelTime = 0;
    int32_t iTempAvg[3];
    int32_t iTempResult;
    int8_t cTempCnt = 0;
    int8_t cTempCheck[3];
    int8_t cTempRelease[3];
    int16_t sTemperature[3][TEMP_AVG] = { 0 };

    int8_t cTopCheck[3];
    int8_t cTopRelease[3];
    int8_t cBottomCheck[3];
    int8_t cBottomRelease[3];

    int32_t iCurRTCTimer;

    MainControl *pMCtrl = GetController();
    HeatControl *pCurHCtrl;

    Controller_Init();
    DelayUS(1000);

    ComSlave_Init();
    RTC_SetAlarmSecond();
    IWDG_Config();


    while (1) {

        if (gEventUpdate) {
            DisableMIRQ;
            gEventUpdate = 0;
            EnableMIRQ;

            iCurRTCTimer = RTC_GetCurrentTimeStamp();

            for (i = 0; i < 3; i++) {
                pCurHCtrl = pMCtrl->pHeatController[i];
                if (
                    (pCurHCtrl->Setting->DisinfectEnable)
                    && (pCurHCtrl->Setting->DisinfectTime.Timer == iCurRTCTimer)
                    && (pCurHCtrl->HStateMachine != HT_STA_WARNING)) {
                    pCurHCtrl->HStateMachine = HT_STA_START_DISINF;
                    DPRINTF(("Start Disinfection.\n"));
                }
            }

            DPRINTF(("[%d] %06x %d    ", (int32_t) SysTicks, iCurRTCTimer, P1Hz));
            DPRINTF(("Temp: %d %d %d \n", gHController[0].MyTemperature, gHController[1].MyTemperature, gHController[2].MyTemperature));
            DPRINTF(("TempFail: %d %d %d   ", gHController[0].TSensorFail, gHController[1].TSensorFail, gHController[2].TSensorFail));
            DPRINTF(("TopState: %d %d %d   ", gHController[0].TopState, gHController[1].TopState, gHController[2].TopState));
            DPRINTF(("BottomState: %d %d %d   ", gHController[0].BottomState, gHController[1].BottomState, gHController[2].BottomState));
            DPRINTF(("State: %d %d %d %d   ", pMCtrl->VStateMachine, gHController[0].HStateMachine, gHController[1].HStateMachine, gHController[2].HStateMachine));
            DPRINTF(("WarnState: 0x%x 0x%x 0x%x \n", gHController[0].WarningStatus, gHController[1].WarningStatus, gHController[2].WarningStatus));

        }

        switch (pMCtrl->VStateMachine) {
            case EV_STA_IDLE:
                if (
                    (pMCtrl->pHeatController[0]->TopState == 0)
                    || (pMCtrl->pHeatController[1]->TopState == 0)
                    || (pMCtrl->pHeatController[2]->TopState == 0)
                ) {
                    pMCtrl->VStateMachine = EV_STA_START;
                    break;
                }
                break;

            case EV_STA_START:
                pMCtrl->ValveCtrl(1, &pMCtrl->ValveState);
                pMCtrl->ValveTimer = P1Hz + EV_TIMEOUT * 60;
                pMCtrl->VStateMachine = EV_STA_ON;
                break;

            case EV_STA_ON:
                if (P1Hz >= pMCtrl->ValveTimer) {
                    DPRINTF(("EV timeout and go to warning state.\n"));
                    pMCtrl->VStateMachine = EV_STA_WARNING;
                    break;
                }
                if (
                    (pMCtrl->pHeatController[0]->TopState)
                    && (pMCtrl->pHeatController[1]->TopState)
                    && (pMCtrl->pHeatController[2]->TopState)
                ) {
                    pMCtrl->ValveCtrl(0, &pMCtrl->ValveState);
                    pMCtrl->ValveTimer = P1Hz + EV_STOP_DELAY * 60;
                    pMCtrl->VStateMachine = EV_STA_STOP;
                }
                break;

            case EV_STA_WARNING:
                if (pMCtrl->ValveState) {
                    pMCtrl->ValveCtrl(0, &pMCtrl->ValveState);
                }
                break;

            case EV_STA_STOP:
                if (P1Hz > pMCtrl->ValveTimer) {
                    pMCtrl->VStateMachine = EV_STA_IDLE;
                }
                break;
        }

        for (i = 0; i < 3; i++) {
            pCurHCtrl = pMCtrl->pHeatController[i];

            if (pCurHCtrl->TSensorFail) {
                pCurHCtrl->WarningStatus |= HT_WARN_SENSOR_FAIL    ;
                if (pCurHCtrl->HStateMachine  != HT_STA_WARNING) {
                    pCurHCtrl->HStateMachine  = HT_STA_WARNING;
                    DPRINTF(("[%d] Goto Warning State. %d \n", (int32_t)SysTicks, HT_WARN_SENSOR_FAIL));
                }
            }

            if (pCurHCtrl-> BottomState) {
                pCurHCtrl->WarningStatus |= HT_WARN_NOWATER ;
                if (pCurHCtrl->HStateMachine  != HT_STA_WARNING) {
                    pCurHCtrl->HStateMachine  = HT_STA_WARNING;
                    DPRINTF(("[%d] Goto Warning State. %d \n", (int32_t)SysTicks, HT_WARN_NOWATER));
                }
            }

            switch (pCurHCtrl->HStateMachine) {
                case HT_STA_IDLE :
                    if (
                        (pCurHCtrl->Setting->HeatEnable)
                        && (iCurRTCTimer >= pCurHCtrl->Setting->WorkStart.Timer)
                        && (iCurRTCTimer < pCurHCtrl->Setting->WorkEnd.Timer)
                    ) {
                        pCurHCtrl->HStateMachine  = HT_STA_ONDUTY;
                        pCurHCtrl->OnDuty = 1;
                        break;
                    }

                    break;

                case HT_STA_ONDUTY:
                    if (
                        (pCurHCtrl-> MyTemperature < (pCurHCtrl->Setting->TempTarget - pCurHCtrl->Setting->TempThreshold))
                        && (pCurHCtrl->TopState)
                        && (pCurHCtrl->BottomState)
                    ) {
                        pCurHCtrl->Heating(HEAT_ON, &(pCurHCtrl->HeatState));
                        pCurHCtrl->HeatStartTime  = P1Hz;
                        pCurHCtrl->HeatCount = 0;
                        pCurHCtrl->IsMaxTemp = 0;
                        pCurHCtrl->HeatTimer1 =  P1Hz + HEATING_TIMEOUT * 60;
                        pCurHCtrl->CheckTemp1 = pCurHCtrl->MyTemperature ;
                        pCurHCtrl->HStateMachine  = HT_STA_HTING;
                        break;
                    }

                    if (pCurHCtrl->Setting->HeatEnable == 0) {
                        pCurHCtrl->OnDuty = 0;
                        pCurHCtrl->HStateMachine  = HT_STA_IDLE;
                        break;
                    }

                    break;

                case HT_STA_HTING:
                    if (pCurHCtrl->HeatState == 0) {
                        pCurHCtrl->HStateMachine  = HT_STA_ONDUTY;
                        break;
                    } else {
                        if (pCurHCtrl->MyTemperature >= pCurHCtrl->Setting->TempTarget) {
                            pCurHCtrl->Heating(HEAT_OFF, &(pCurHCtrl->HeatState));
                            pCurHCtrl->HStateMachine  = HT_STA_ONDUTY;
                            break;
                        }

                        if (pCurHCtrl->MyTemperature >= pCurHCtrl->Setting->TempMax) {
                            if (pCurHCtrl->IsMaxTemp == 0)  {
                                pCurHCtrl->IsMaxTemp = 1;
                                pCurHCtrl->HeatTimer2 =  P1Hz + HEATING_MAXTEMP_TIME * 60;
                            } else {
                                if (P1Hz >= pCurHCtrl->HeatTimer2)  {
                                    pCurHCtrl->IsMaxTemp = 0;
                                    pCurHCtrl->Heating(HEAT_OFF, &(pCurHCtrl->HeatState));
                                    pCurHCtrl->HStateMachine  = HT_STA_ONDUTY;
                                    break;
                                }
                            }
                        } else {
                            if (pCurHCtrl->IsMaxTemp)  {
                                pCurHCtrl->IsMaxTemp = 0;
                            }
                        }


                        if ((P1Hz  >= pCurHCtrl-> HeatTimer1)
                            && (pCurHCtrl->MyTemperature <= (2 + pCurHCtrl->CheckTemp1))) {
                            pCurHCtrl->HeatCount++;
                            if (pCurHCtrl->HeatCount >= HEATING_RETRY) {
                                pCurHCtrl->Heating(HEAT_OFF, &(pCurHCtrl->HeatState));
                                pCurHCtrl->WarningStatus |= HT_WARN_CANNOT_HEAT;
                                pCurHCtrl->HStateMachine  = HT_STA_WARNING;
                                DPRINTF(("[%d] Goto Warning State. %d \n", (int32_t)SysTicks, HT_WARN_CANNOT_HEAT));
                                break;
                            } else {
                                pCurHCtrl->Heating(HEAT_OFF, &(pCurHCtrl->HeatState));
                                pCurHCtrl->HeatTimer1 =  P1Hz + HEATING_PAUSE_TIME * 60;
                                pCurHCtrl->HStateMachine  = HT_STA_WAIT_HTING;
                                break;
                            }

                        }
                    }

                    if (pCurHCtrl->Setting->HeatEnable == 0) {
                        pCurHCtrl->Heating(HEAT_OFF, &(pCurHCtrl->HeatState));
                        pCurHCtrl->OnDuty = 0;
                        pCurHCtrl->HStateMachine  = HT_STA_IDLE;
                        break;
                    }
                    break;

                case HT_STA_WARNING:
                    if (pCurHCtrl->HeatState) {
                        pCurHCtrl->Heating(HEAT_OFF, &(pCurHCtrl->HeatState));
                    }
                    if ((pCurHCtrl->Setting->HeatEnable == 0) && (pCurHCtrl->OnDuty)) {
                        pCurHCtrl->OnDuty = 0;
                    }

                    break;

                case HT_STA_WAIT_HTING:
                    if (P1Hz  >= pCurHCtrl-> HeatTimer1) {
                        pCurHCtrl->IsMaxTemp = 0;
                        pCurHCtrl->HeatTimer1 =  P1Hz + HEATING_TIMEOUT * 60;
                        pCurHCtrl->CheckTemp1 = pCurHCtrl->MyTemperature ;
                        pCurHCtrl->HStateMachine  = HT_STA_HTING;
                        break;
                    }

                    if (pCurHCtrl->Setting->HeatEnable == 0) {
                        pCurHCtrl->OnDuty = 0;
                        pCurHCtrl->HStateMachine  = HT_STA_IDLE;
                        break;
                    }

                    break;

                case  HT_STA_START_DISINF:
                    if (pCurHCtrl->HeatState == 0) {
                        pCurHCtrl->Heating(HEAT_ON, &(pCurHCtrl->HeatState));
                    }
                    pCurHCtrl->IsDisinfect = 1;
                    pCurHCtrl->IsDisInfoTemp = 0;
                    pCurHCtrl->DisinfectStartTime = P1Hz;
                    pCurHCtrl->HStateMachine  = HT_STA_DO_DISINF;
                    break;

                case  HT_STA_DO_DISINF:
                    if (pCurHCtrl->Setting->DisinfectEnable == 0) {
                        pCurHCtrl->IsDisinfect = 0;
                        pCurHCtrl->HStateMachine  = HT_STA_IDLE;
                        break;
                    }

                    if ((pCurHCtrl->Setting->HeatEnable == 0) && (pCurHCtrl->OnDuty)) {
                        pCurHCtrl->OnDuty = 0;
                    }

                    if (pCurHCtrl->MyTemperature >= pCurHCtrl->Setting->TempDisinfect) {
                        if (pCurHCtrl->IsDisInfoTemp == 0) {
                            pCurHCtrl->IsDisInfoTemp = 1;
                            pCurHCtrl->HeatTimer1 =  P1Hz + (pCurHCtrl->Setting->DisinfectPeriod) * 60;
                        } else {
                            if (P1Hz >= pCurHCtrl->HeatTimer1) {
                                pCurHCtrl->Heating(HEAT_OFF, &(pCurHCtrl->HeatState));
                                pCurHCtrl->IsDisinfect = 0;
                                pCurHCtrl->IsDisInfoTemp = 0;
                                pCurHCtrl->HStateMachine  = HT_STA_IDLE;
                                break;
                            }
                        }
                    }

                    if (P1Hz - pCurHCtrl->DisinfectStartTime > (uint32_t)((uint32_t)pCurHCtrl->Setting->DisinfectTimeout) * 60) {
                        pCurHCtrl->Heating(HEAT_OFF, &(pCurHCtrl->HeatState));
                        pCurHCtrl->IsDisinfect = 0;
                        pCurHCtrl->WarningStatus |= HT_WARN_CANNOT_HEAT;
                        pCurHCtrl->HStateMachine  = HT_STA_WARNING;
                        DPRINTF(("[%d] Goto Warning State. %d \n", (int32_t)SysTicks, HT_WARN_CANNOT_HEAT));
                        break;
                    }
                    break;
            }
        }

        if (SysTicks > lCheckTempTime) {
            iTempResult = ADC_GetTempture(&sTemperature[0][cTempCnt], &sTemperature[1][cTempCnt], &sTemperature[2][cTempCnt]);
            cTempCnt ++;
            cTempCnt  = cTempCnt % TEMP_AVG;

            for (i = 0; i < 3; i ++) {

                if (iTempResult & 0xff) {
                    cTempRelease[i] = 0;
                    if (pMCtrl->pHeatController[i]->TSensorFail == 0)
                        cTempCheck[i] ++;
                    if (cTempCheck[i] >= 4) {
                        pMCtrl->pHeatController[i]->TSensorFail = 1;
                    }
                } else {
                    cTempCheck[i] = 0;
                    if (pMCtrl->pHeatController[i]->TSensorFail) {
                        if (++cTempRelease[i] > 16) {
                            pMCtrl->pHeatController[i]->TSensorFail = 0;
                        }
                    }
                }

                iTempResult >>= 8;
                iTempAvg[i] = 0;
                for (j = 0; j < TEMP_AVG; j++) {
                    iTempAvg[i] += (int32_t) sTemperature[i][j];
                }
                iTempAvg[i] = iTempAvg[i] / TEMP_AVG;
                pMCtrl->pHeatController[i]->MyTemperature = (int16_t)iTempAvg[i];
            }

            lCheckTempTime  += (int64_t) 100;
        }


        if (SysTicks > lCheckLevelTime) {
            lCheckLevelTime  += (int64_t)20;

            for (i = 0; i < 3; i++) {

                if (pMCtrl->pHeatController[i]->CheckTopState()) {
                    cTopRelease[i] = 0;
                    if (pMCtrl->pHeatController[i]->TopState == 0) {
                        if (++cTopCheck[i] > 8) {
                            pMCtrl->pHeatController[i]->TopState = 1;
                        }
                    }
                } else {
                    cTopCheck[i] = 0;
                    if (pMCtrl->pHeatController[i]->TopState) {
                        if (++cTopRelease[i] > 16) {
                            pMCtrl->pHeatController[i]->TopState = 0;
                        }
                    }
                }

                if (pMCtrl->pHeatController[i]->CheckBottomState()) {
                    cBottomRelease[i] = 0;
                    if (pMCtrl->pHeatController[i]->BottomState == 0) {
                        if (++cBottomCheck[i] > 8) {
                            pMCtrl->pHeatController[i]->BottomState = 1;
                        }
                    }
                } else {
                    cBottomCheck[i] = 0;
                    if (pMCtrl->pHeatController[i]->BottomState) {
                        if (++cBottomRelease[i] > 16) {
                            pMCtrl->pHeatController[i]->BottomState = 0;
                        }
                    }
                }
            }
        }


        IWDG_Reload();
        ComSlave_Service();
        Parameter_Service();
    }

}

