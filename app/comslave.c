

#include <string.h>

#include "common.h"
#include "global.h"
#include "board.h"
#include "uart.h"
#include "crc8.h"
#include "rtc.h"
#include "loop.h"
#include "comslave.h"

#define CMD_SIZE   128
#define SYNC_BYTE  0x16

typedef struct {
    uint8_t FunCode;
    eCode(*CmdFuc)(uint8_t *pucFrame, uint16_t len);
} CmdHandler;


typedef enum {
    RX_SYNC = 0,
    RX_WAIT,
    RX_DATA,
    RX_EXE,
    RX_RESP,
    RX_RST,
} RX_STATE;


static int32_t Cmd_iBuf[CMD_SIZE / 4 ];
static int8_t Rx_State = 0;
static int16_t RdPtr_Rx = 0;
static int16_t WrPtr_Rx = 0;
static int16_t Rx_len = 0;

static uint8_t *Usart_Rx_Buf = 0;
static uint8_t *Cmd_Buf = 0;
static uint8_t *UsartTxPtr = 0 ;

static void ComCmdSendFrame(uint8_t *pucFrame, uint16_t usLength);
static int32_t GetSetupData(HeatSet *setup, uint8_t *pucFrame);

static eCode ComCmdGetVersion(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetDateTime(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdSetDateTime(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetTemperature(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetState(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetSetupCH1(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetSetupCH2(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetSetupCH3(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdGetWarning(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdCleanWarning(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdCtrlHeater(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdCtrlDisinfect(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdSetSetupCH1(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdSetSetupCH2(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdSetSetupCH3(uint8_t *pucFrame, uint16_t len);
static eCode ComCmdSetupFlush(uint8_t *pucFrame, uint16_t len);

const CmdHandler InstCmdHandler[] = {
    { 0x01, ComCmdGetVersion },
    { 0x02, ComCmdGetDateTime},
    { 0x03, ComCmdSetDateTime},
    { 0x04, ComCmdGetTemperature},
    { 0x05, ComCmdGetState},
    { 0x06, ComCmdGetSetupCH1},
    { 0x07, ComCmdGetSetupCH2},
    { 0x08, ComCmdGetSetupCH3},
    { 0x09, ComCmdGetWarning},
    { 0x0a, ComCmdCleanWarning},
    { 0x0b, ComCmdCtrlHeater},
    { 0x0c, ComCmdCtrlDisinfect},
    { 0x0d, ComCmdSetSetupCH1},
    { 0x0e, ComCmdSetSetupCH2},
    { 0x0f, ComCmdSetSetupCH3},
    { 0x10, ComCmdSetupFlush},
};

static void SetSetupData(HeatSet *setup, uint8_t *pucFrame, uint16_t *len)
{
    uint16_t usLength = 2;

    pucFrame[usLength++] = (uint8_t)(setup->HeatEnable);
    pucFrame[usLength++] = (uint8_t)(setup->DisinfectEnable);
    pucFrame[usLength++] = (uint8_t)(setup->TempMax);
    pucFrame[usLength++] = (uint8_t)(setup->TempTarget);
    pucFrame[usLength++] = (uint8_t)(setup->TempThreshold);
    pucFrame[usLength++] = (uint8_t)(setup->TempDisinfect);
    pucFrame[usLength++] = (uint8_t)((setup->DisinfectPeriod) && 0xff);
    pucFrame[usLength++] = (uint8_t)(((setup->DisinfectPeriod) >> 8) && 0xff);
    pucFrame[usLength++] = (uint8_t)((setup->DisinfectTimeout) && 0xff);
    pucFrame[usLength++] = (uint8_t)(((setup->DisinfectTimeout) >> 8) && 0xff);
    pucFrame[usLength++] = (uint8_t)(setup->WorkStart.rtcTime.Hour);
    pucFrame[usLength++] = (uint8_t)(setup->WorkStart.rtcTime.Minute);
    pucFrame[usLength++] = (uint8_t)(setup->WorkStart.rtcTime.Second);
    pucFrame[usLength++] = (uint8_t)(setup->WorkEnd.rtcTime.Hour);
    pucFrame[usLength++] = (uint8_t)(setup->WorkEnd.rtcTime.Minute);
    pucFrame[usLength++] = (uint8_t)(setup->WorkEnd.rtcTime.Second);
    pucFrame[usLength++] = (uint8_t)(setup->DisinfectTime.rtcTime.Hour);
    pucFrame[usLength++] = (uint8_t)(setup->DisinfectTime.rtcTime.Minute);
    pucFrame[usLength++] = (uint8_t)(setup->DisinfectTime.rtcTime.Second);

    *len = usLength;
}

static int32_t GetSetupData(HeatSet *setup, uint8_t *pucFrame)
{
    int8_t  *pcPtr;
    int16_t DisinfectTimeout;
    int16_t DisinfectPeriod;

    pcPtr = (int8_t *) pucFrame;

    if (pcPtr[0] < 0 || pcPtr[0] > 100) return -1;  //TempMax
    if (pcPtr[1] < 0 || pcPtr[1] > 100) return -1;  //TempTarget
    if (pcPtr[2] <= 0 || pcPtr[2] > 50) return -1;  //TempThreshold
    if (pcPtr[3] < 0 || pcPtr [3] > 100) return -1;  //TempDisinfect

    DisinfectTimeout = (int16_t)((pucFrame[5] << 8) | pucFrame[4]);
    if (DisinfectTimeout > 300 || DisinfectTimeout  < 0) return -1;
    DisinfectPeriod = (int16_t)((pucFrame[7] << 8) | pucFrame[6]);
    if (DisinfectPeriod > 60 || DisinfectPeriod  < 0) return -1;

    if ((pucFrame[8] > 23) || (pucFrame[9] > 59) || (pucFrame[10] > 59)) return -1; //workstart
    if ((pucFrame[11] > 23) || (pucFrame[12] > 59) || (pucFrame[13] > 59)) return -1; //workend
    if ((pucFrame[14] > 23) || (pucFrame[15] > 59) || (pucFrame[16] > 59)) return -1; //Disinfect time

    setup->TempMax = pcPtr[0];
    setup->TempTarget = pcPtr[1];
    setup->TempThreshold = pcPtr[2];
    setup->TempDisinfect = pcPtr[3];
    setup->DisinfectTimeout = DisinfectTimeout ;
    setup->DisinfectPeriod = DisinfectPeriod;
    setup->WorkStart.rtcTime.Hour = pucFrame[8];
    setup->WorkStart.rtcTime.Minute = pucFrame[9];
    setup->WorkStart.rtcTime.Second = pucFrame[10];
    setup->WorkEnd.rtcTime.Hour = pucFrame[11];
    setup->WorkEnd.rtcTime.Minute = pucFrame[12];
    setup->WorkEnd.rtcTime.Second = pucFrame[13];
    setup->DisinfectTime.rtcTime.Hour = pucFrame[14];
    setup->DisinfectTime.rtcTime.Minute = pucFrame[15];
    setup->DisinfectTime.rtcTime.Second = pucFrame[16];

    Parameter_SetUpdate();

    return 0;
}

static eCode ComCmdSetSetupCH1(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    uint16_t usLength = 2;
    MainControl *pCtrl = GetController();
    HeatSet *setup = pCtrl->pHeatController[0]->Setting;

    if (len < 17) return ECODE_INVALID_PARAM;


    if (GetSetupData(setup, pucFrame + 2) < 0) return ECODE_INVALID_PARAM;

    pucFrame[usLength++] =  0x00;
    ComCmdSendFrame(pucFrame, usLength);

    return ECODE_SUCCESS;

}

static eCode ComCmdSetSetupCH2(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    uint16_t usLength = 2;
    MainControl *pCtrl = GetController();
    HeatSet *setup = pCtrl->pHeatController[1]->Setting;

    if (len < 17) return ECODE_INVALID_PARAM;

    if (GetSetupData(setup, pucFrame + 2) < 0) return ECODE_INVALID_PARAM;

    pucFrame[usLength++] =  0x00;
    ComCmdSendFrame(pucFrame, usLength);

    return ECODE_SUCCESS;
}

static eCode ComCmdSetSetupCH3(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    uint16_t usLength = 2;
    MainControl *pCtrl = GetController();
    HeatSet *setup = pCtrl->pHeatController[2]->Setting;

    if (len < 17) return ECODE_INVALID_PARAM;
    if (GetSetupData(setup, pucFrame + 2) < 0) return ECODE_INVALID_PARAM;

    pucFrame[usLength++] =  0x00;
    ComCmdSendFrame(pucFrame, usLength);

    return ECODE_SUCCESS;
}
static eCode ComCmdCtrlHeater(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    uint16_t usLength = 2;
    int8_t which, onoff;
    MainControl *pCtrl = GetController();

    which = (int8_t)pucFrame[2];
    onoff = (int8_t) pucFrame[3];

    if (which < 1 || which > 3) return ECODE_INVALID_PARAM;

    which--;

    HeatSet *setup = pCtrl->pHeatController[which]->Setting;

    onoff = (onoff) ? 1 : 0;
    setup-> HeatEnable = onoff;
    Parameter_SetUpdate();

    pucFrame[usLength++] =  0x00;
    ComCmdSendFrame(pucFrame, usLength);

    return ECODE_SUCCESS;
}

static eCode ComCmdCtrlDisinfect(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    uint16_t usLength = 2;
    int8_t which, onoff;
    MainControl *pCtrl = GetController();

    which = (int8_t)pucFrame[2];
    onoff = (int8_t) pucFrame[3];

    if (which < 1 || which > 3) return ECODE_INVALID_PARAM;

    which--;

    HeatSet *setup = pCtrl->pHeatController[which]->Setting;

    onoff = (onoff) ? 1 : 0;
    setup-> DisinfectEnable = onoff;
    Parameter_SetUpdate();

    pucFrame[usLength++] =  0x00;
    ComCmdSendFrame(pucFrame, usLength);

    return ECODE_SUCCESS;
}

static eCode ComCmdSetupFlush(uint8_t *pucFrame, uint16_t len)
{
    int32_t result;
    uint16_t usLength = 2;
    if (len != 1) return  ECODE_INVALID_PARAM;

    result = Parameter_Flush();

    if (result < 0) {
        return ECODE_EXE_ERROR;
    }

    pucFrame[usLength++] =  0x00;

    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}


static eCode ComCmdCleanWarning(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;

    if (len != 1) return  ECODE_INVALID_PARAM;

    if (*pucFrame & WARN_FLAG_VALVE) {
        Ctrl_CleanWarn(CLEAR_WARN_VALVE);
    }

    if (*pucFrame & WARN_FLAG_CH1) {
        Ctrl_CleanWarn(CLEAR_WARN_CH1);
    }

    if (*pucFrame & WARN_FLAG_CH2) {
        Ctrl_CleanWarn(CLEAR_WARN_CH2);
    }

    if (*pucFrame & WARN_FLAG_CH3) {
        Ctrl_CleanWarn(CLEAR_WARN_CH3);
    }

    pucFrame[usLength++] =  0x00;
    ComCmdSendFrame(pucFrame, usLength);

    return ECODE_SUCCESS;
}


static eCode ComCmdGetWarning(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);

    uint16_t usLength = 2;
    uint8_t warn;
    uint32_t status;
    warn = Ctrl_GetlWarn();
    status = Ctrl_GetWarnStatus();
    pucFrame[usLength++] = (uint8_t)(warn);
    pucFrame[usLength++] = (uint8_t)(status & 0xff);
    pucFrame[usLength++] = (uint8_t)((status >> 8) & 0xff);
    pucFrame[usLength++] = (uint8_t)((status >> 16) & 0xff);
    pucFrame[usLength++] = (uint8_t)((status >> 24) & 0xff);

    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}

static eCode ComCmdGetSetupCH1(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    MainControl *pCtrl;
    uint16_t usLength;

    pCtrl = GetController();
    HeatSet *setup = pCtrl->pHeatController[0]->Setting;

    SetSetupData(setup, pucFrame, &usLength);

    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}

static eCode ComCmdGetSetupCH2(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    MainControl *pCtrl;
    uint16_t usLength;

    pCtrl = GetController();
    HeatSet *setup = pCtrl->pHeatController[1]->Setting;

    SetSetupData(setup, pucFrame, &usLength);

    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}

static eCode ComCmdGetSetupCH3(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    MainControl *pCtrl;
    uint16_t usLength;

    pCtrl = GetController();
    HeatSet *setup = pCtrl->pHeatController[2]->Setting;

    SetSetupData(setup, pucFrame, &usLength);

    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}

static void ComCmdSendFrame(uint8_t *pucFrame, uint16_t usLength)
{
    uint8_t crc;
    crc = Get_CRC8(pucFrame, usLength);
    pucFrame[usLength++] =  crc;
    Usart_Comm_WaitTx(10000);
    Usart_Comm_Tx((char *)pucFrame, usLength);
    return ;
}

static eCode ComCmdGetState(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    MainControl *pCtrl;
    uint16_t usLength = 2;
    uint16_t state = 0;

    pCtrl = GetController();
    for (int i = 0; i < 3; i++) {
        if (pCtrl->pHeatController[i]->HeatState) {
            state  |= (1 << i);
        }
        if (pCtrl->pHeatController[i]->IsDisinfect) {
            state  |= (1 << (i + 3));
        }
        if (pCtrl->pHeatController[i]->OnDuty) {
            state  |= (1 << (i + 6));
        }
        if (pCtrl->pHeatController[i]->TopState) {
            state  |= (1 << (i + 9));
        }
        if (pCtrl->pHeatController[i]->BottomState) {
            state  |= (1 << (i + 12));
        }
    }
    pucFrame[usLength++] = (uint8_t)(state & 0xff);
    pucFrame[usLength++] = (uint8_t)((state >> 8) & 0xff);

    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;

}

static eCode ComCmdGetTemperature(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);

    uint16_t usLength = 2;

    MainControl *pCtrl = GetController();

    pucFrame[usLength++] = (uint8_t)(pCtrl->pHeatController[0]->MyTemperature & 0xff);
    pucFrame[usLength++] = (uint8_t)((pCtrl->pHeatController[0]->MyTemperature >> 8) & 0xff);
    pucFrame[usLength++] = (uint8_t)(pCtrl->pHeatController[1]->MyTemperature & 0xff);
    pucFrame[usLength++] = (uint8_t)((pCtrl->pHeatController[1]->MyTemperature >> 8) & 0xff);
    pucFrame[usLength++] = (uint8_t)(pCtrl->pHeatController[2]->MyTemperature & 0xff);
    pucFrame[usLength++] = (uint8_t)((pCtrl->pHeatController[2]->MyTemperature >> 8) & 0xff);

    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;

}

static eCode ComCmdSetDateTime(uint8_t *pucFrame, uint16_t len)
{
    uint16_t usLength = 2;

    if (len < 7) return  ECODE_INVALID_PARAM;

    if (pucFrame[2] > 99 || pucFrame[3] > 12 || pucFrame[4] > 31 || pucFrame[5] > 7 ||
        pucFrame[6] > 23 || pucFrame[7] > 59 || pucFrame[8] > 59) return ECODE_INVALID_PARAM ;

    RTC_SetCurrentDate(pucFrame[5], pucFrame[4], pucFrame[3], pucFrame[2]);
    RTC_SetCurrentTime(pucFrame[6], pucFrame[7], pucFrame[8]);

    pucFrame[usLength++] =  0x00;
    ComCmdSendFrame(pucFrame, usLength);

    return ECODE_SUCCESS;

}

static eCode ComCmdGetDateTime(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    RTC_TimeTypeDef RTC_TimeStructure;
    RTC_DateTypeDef RTC_DateStructure;
    uint16_t usLength = 2;

    RTC_GetTime(RTC_Format_BIN, &RTC_TimeStructure);
    RTC_GetDate(RTC_Format_BIN, &RTC_DateStructure);

    pucFrame[usLength++] =  RTC_DateStructure.RTC_Year;
    pucFrame[usLength++] =  RTC_DateStructure.RTC_Month;
    pucFrame[usLength++] =  RTC_DateStructure.RTC_Date;
    pucFrame[usLength++] =  RTC_DateStructure.RTC_WeekDay;
    pucFrame[usLength++] =  RTC_TimeStructure.RTC_Hours ;
    pucFrame[usLength++] =  RTC_TimeStructure.RTC_Minutes ;
    pucFrame[usLength++] =  RTC_TimeStructure.RTC_Seconds ;

    ComCmdSendFrame(pucFrame, usLength);

    return ECODE_SUCCESS;

}

static eCode ComCmdGetVersion(uint8_t *pucFrame, uint16_t len)
{
    UNUSED(len);
    uint16_t usLength = 2;
    pucFrame[usLength++] = (uint8_t)(MainVersion & 0xff);
    pucFrame[usLength++] = (uint8_t)((MainVersion >> 8) & 0xff);
    pucFrame[usLength++] = (uint8_t)((MainVersion >> 16) & 0xff);
    pucFrame[usLength++] = (uint8_t)((MainVersion >> 24) & 0xff);

    ComCmdSendFrame(pucFrame, usLength);
    return ECODE_SUCCESS;
}

static void ComEcho_ECode(uint8_t *pucFrame, eCode code)
{
    uint16_t usLength = 2;
    pucFrame[usLength++] = code ;
    ComCmdSendFrame(pucFrame, usLength);

    return ;
}



void ComSlave_Init(void)
{
    Usart_Rx_Buf  = Usart_Comm_RxBuf();
    UsartTxPtr  = Usart_Comm_TxBuf();
    Cmd_Buf  = (uint8_t *) Cmd_iBuf;

    DPRINTF(("[%d] Communication Slave init ok \n", (int32_t)SysTicks));
    return ;
}

static void ComSlave_Rx_Reset(void)
{
    WrPtr_Rx = (uint16_t) USART_RX_SIZE - ((uint16_t)(USART1_RX_DMA_CHANNEL -> CNDTR));
    RdPtr_Rx = WrPtr_Rx ;
    return ;
}

void ComSlave_Service(void)
{
    uint32_t i;
    int16_t fullness;
    int32_t dt, dcnt;

    static eCode ErrorCode;
    static int64_t rx_start = 0;
    static int16_t last_cnt = 0;

    static uint8_t *pucFrame;
    static uint16_t usLength;
    static uint8_t ucFunctionCode;

    WrPtr_Rx = USART_RX_SIZE - ((uint16_t)(USART1_RX_DMA_CHANNEL -> CNDTR));

    fullness = WrPtr_Rx - RdPtr_Rx;
    if (fullness < 0) fullness += USART_RX_SIZE ;

    switch (Rx_State) {
        case RX_SYNC:
            if (!fullness) break;
            rx_start  = SysTicks;
            last_cnt  = fullness;
            Rx_State = RX_WAIT;
            break;

        case RX_WAIT:
            dt = (int32_t)(SysTicks - rx_start);
            dcnt = fullness - last_cnt;

            if ((dt > 30)
                || ((dcnt > 0) && (dt > (dcnt  + 4)))
                || ((dcnt == 0) && (dt > 2))
               ) {
                Rx_State = RX_DATA;
            }
            break;

        case RX_DATA:
            Rx_len = (fullness > 255) ? 255 : fullness;

            if (RdPtr_Rx + Rx_len < USART_RX_SIZE) {
                memcpy(&Cmd_Buf[0], &Usart_Rx_Buf[RdPtr_Rx], Rx_len);
                RdPtr_Rx += Rx_len;
            } else {
                int16_t len1, len2;
                len1 = USART_RX_SIZE - RdPtr_Rx;
                len2 = Rx_len - len1;
                memcpy(&Cmd_Buf[0], &Usart_Rx_Buf[RdPtr_Rx], len1);
                memcpy(&Cmd_Buf[0 + len1], &Usart_Rx_Buf[0], len2);
                RdPtr_Rx = len2;
            }

            if (Cmd_Buf[0] != SYNC_BYTE) {
                EPRINTF(("Not Sync byte.\n"));
                Rx_State =  RX_RST;
                break;
            }

            pucFrame = &Cmd_Buf[0];

            if (Get_CRC8(Cmd_Buf, (int)Rx_len)) {
                EPRINTF(("CRC Error.\n"));
                ErrorCode = ECODE_CRC_ERROR   ;
                Rx_State =  RX_RESP;
                break;
            }

            usLength = Rx_len - 1 - 2 ;
            Rx_State =  RX_EXE;
            break;

        case RX_EXE:
            ucFunctionCode = pucFrame[1];

            ErrorCode = ECODE_NOT_SUPPORT_CMD   ;
            for (i = 0; i < sizeof(InstCmdHandler) / sizeof(CmdHandler); i++) {
                if (InstCmdHandler[i].FunCode == ucFunctionCode) {
                    ErrorCode =  InstCmdHandler[i].CmdFuc(pucFrame, usLength);
                    break;
                }
            }

            if (ErrorCode != ECODE_SUCCESS) {
                Rx_State =  RX_RESP;
                break;
            }

            Rx_State =  RX_SYNC;
            break;

        case RX_RESP:
            ComEcho_ECode(pucFrame, ErrorCode);
            Rx_State =  RX_SYNC;
            break;

        case RX_RST:
            ComSlave_Rx_Reset();
            Rx_State =  RX_SYNC;
            break;

    }
}


