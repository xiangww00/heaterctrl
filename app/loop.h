
#ifndef _LOOP_H_
#define _LOOP_H_

#include "parameter.h"

#define  HT_WARN_SENSOR_FAIL     0x01
#define  HT_WARN_NOWATER         0x02
#define  HT_WARN_CANNOT_HEAT     0x04
#define  HT_WARN_DISINF_TIMEOUT  0x08

#define WARN_FLAG_VALVE   0x40
#define WARN_FLAG_CH1     0x01
#define WARN_FLAG_CH2     0x02
#define WARN_FLAG_CH3     0x04

#define CLEAR_WARN_VALVE   0x00
#define CLEAR_WARN_CH1     0x01
#define CLEAR_WARN_CH2     0x02
#define CLEAR_WARN_CH3     0x03

typedef struct {
    void (*Heating) (int8_t set, int8_t *state);
    int8_t (*CheckTopState) (void);
    int8_t (*CheckBottomState) (void);

    HeatSet *Setting; 

    int8_t HStateMachine;

    int8_t HeatState;
    int8_t OnDuty;
    int8_t IsDisinfect;

    int8_t TopState;
    int8_t BottomState;

    int8_t TSensorFail;
    int8_t DisinfectTimeout; 
    int8_t HeatCount;
    int8_t IsMaxTemp;
    int8_t IsDisInfoTemp;

    int16_t MyTemperature;

    int16_t CheckTemp1;

    uint32_t WarningStatus;

    uint32_t HeatTimer1; 
    uint32_t HeatTimer2; 
    uint32_t HeatStartTime;
    uint32_t DisinfectStartTime;
} HeatControl;

typedef struct {
    void (*ValveCtrl) (int8_t set, int8_t *state);
    HeatControl *pHeatController[3];

    int8_t VStateMachine;
    int8_t ValveState;

    uint32_t ValveTimer;
} MainControl;


void MainLoop(void);
MainControl* GetController(void);
uint8_t Ctrl_GetlWarn(void);
uint32_t Ctrl_GetWarnStatus(void);
void Ctrl_CleanWarn(int8_t which);

#endif

