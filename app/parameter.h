
#ifndef _PARAMETER_H_
#define _PARAMETER_H_

#define PARA_MAKER ((uint32_t) 0xb00bface)

typedef struct {
    uint8_t Second;
    uint8_t Minute;
    uint8_t Hour;
    uint8_t Res;
} RTCTimeDef;

typedef union {
    RTCTimeDef rtcTime;
    int32_t Timer;
} TimeDef;

typedef struct {
    int8_t TempMax;
    int8_t TempTarget;
    int8_t TempThreshold;
    int8_t TempDisinfect;

    int16_t DisinfectTimeout;
    int16_t DisinfectPeriod;

    int8_t HeatEnable;
    int8_t DisinfectEnable;

    TimeDef WorkStart;
    TimeDef WorkEnd;
    TimeDef DisinfectTime;

} HeatSet;

struct parameter_store 
{
    uint32_t Marker;
    HeatSet hCtrl[3];

    uint32_t pading[2];
} ;

extern  struct parameter_store  myParameter;

void Parameter_Load(void);
int32_t Parameter_Store(void);
void Parameter_SetUpdate(void);
void Parameter_Service(void);
int32_t Parameter_Flush(void);

#endif

