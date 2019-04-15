
#include <stdlib.h>

#include "stm32f0xx.h"
#include "common.h"
#include "global.h"
#include "adc.h"


static uint16_t RegularConvData_Tab[5];
static uint16_t AdcTemp_Tab[5][5];

const uint16_t *const vref_cal = (uint16_t *) 0x1ffff7ba;
const uint16_t *const ts_cal = (uint16_t *) 0x1ffff7b8;

#define Temp_Rx 3300

const uint16_t NTC_Tbl[101] = {
    31770, 30253, 28816, 27453, 26161, 24935, 23772, 22668,
    21621, 20626, 19682, 18785, 17933, 17123, 16353, 15621,
    14925, 14264, 13634, 13035, 12465, 11923, 11406, 10915,
    10446, 10000, 9575, 9170, 8784, 8416, 8064, 7730,
    7410, 7106, 6815, 6538, 6273, 6020, 5778, 5548,
    5327, 5117, 4915, 4723, 4539, 4363, 4195, 4034,
    3880, 3733, 3592, 3457, 3328, 3204, 3086, 2972,
    2863, 2759, 2659, 2564, 2472, 2384, 2299, 2218,
    2141, 2066, 1994, 1926, 1860, 1796, 1735, 1677,
    1621, 1567, 1515, 1465, 1417, 1371, 1326, 1284,
    1243, 1203, 1165, 1128, 1093, 1059, 1027, 996,
    965,  936,  908,  881,  855,  830,  805,  782,
    759,  737,  715,  695,  674,
};

static int16_t GetTempbyR(uint16_t  R)
{
    int16_t tmax, tmin, t0;
    int8_t eq = 0;
    if (R  <= NTC_Tbl[100]) return 100;
    if (R >= NTC_Tbl[0]) return 0;

    tmax = 100;
    tmin = 0;

    do {
        t0 = (tmax + tmin) / 2;
        if (R > NTC_Tbl[t0]) {
            tmax = t0;
        } else if (R < NTC_Tbl[t0]) {
            tmin = t0;
        } else {
            eq = 1;
            tmin = t0;
            break;
        }
    } while ((tmax - tmin) > 1);

    if (!eq) {
        if (R < ((NTC_Tbl[tmin] + NTC_Tbl[tmax]) / 2))
            tmin ++;
    }

    return tmin;
}

static void ADC_Calibration(void)
{
    int32_t timeout = 0x1000;
    if ((ADC1->CR & ADC_CR_ADEN) != 0) {
        ADC1->CR |= ADC_CR_ADDIS;
    }

    while ((ADC1->CR & ADC_CR_ADEN) != 0) {
        if (--timeout <= 0) break;
    }

    ADC1->CFGR1 &= ~ADC_CFGR1_DMAEN;
    ADC1->CR |= ADC_CR_ADCAL;

    timeout = 0x10000;
    while ((ADC1->CR & ADC_CR_ADCAL) != 0) {
        if (--timeout <= 0) break;
    }
}

void ADC_Config(void)
{
    int32_t timeout;
    RCC->APB2ENR |= RCC_APB2ENR_ADCEN;

    ADC1->CFGR1 |= ADC_CFGR1_CONT |  ADC_CFGR1_SCANDIR ;
    ADC1->CHSELR = ADC_CHSELR_CHSEL5 | ADC_CHSELR_CHSEL6 | ADC_CHSELR_CHSEL7 | \
                   ADC_CHSELR_CHSEL16 | ADC_CHSELR_CHSEL17 ;

    ADC1->SMPR = 0x06;
    ADC->CCR  = ADC_CCR_TSEN | ADC_CCR_VREFEN;

    ADC_Calibration();

    ADC1->CFGR1 |= ADC_CFGR1_DMACFG;
    ADC1->CFGR1 |= ADC_CFGR1_DMAEN;


    DMA1_Channel1->CPAR = (uint32_t)(&(ADC1->DR));
    DMA1_Channel1->CMAR = (uint32_t)RegularConvData_Tab;

    DMA1_Channel1->CCR =  DMA_CCR_MINC | DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0 | \
                          DMA_CCR_CIRC | DMA_CCR_PL_1 ;

    DMA1_Channel1->CNDTR = 5;

#if 0
    DMA1_Channel1->CCR |=  DMA_IT_TC;
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    NVIC_SetPriority(DMA1_Channel1_IRQn, 2);
#endif

    DMA1_Channel1->CCR |= DMA_CCR_EN;

    ADC1->CR |= ADC_CR_ADEN;

    timeout = 0x20000;
    while (!((ADC1->ISR & ADC_ISR_ADRDY))) {
        if (--timeout <= 0) {
            EPRINTF(("ADC Config timeout.\n"));
            break;
        }
    }


    ADC1->CR |= ADC_CR_ADSTART;


    DPRINTF(("[%d] ADC init ... ok.\n", (int32_t)SysTicks));

    return ;
}


#if 0
void DMA1_Channel1_IRQHandler(void)
{
    if (DMA1->ISR & DMA_ISR_TCIF1) {
        DMA1->IFCR = DMA_ISR_TCIF1;
    }
}

void ADC_Test(void)
{
    while ((DMA1->ISR & DMA_ISR_TCIF1)  == 0);

    DMA1->IFCR = DMA_ISR_TCIF1;

    for (int i = 0; i < 5; i++) {
        printf("%d,", RegularConvData_Tab[i]);
    }
    printf("%d,%d\n", *vref_cal, *ts_cal);
}
#endif

int32_t ADC_GetTempture(int16_t *t1, int16_t *t2, int16_t *t3)
{
    int i, j, k;
    uint16_t tmp;

    int32_t vdda, u0;
    int32_t r[3];
    int32_t ret = 0;


    for (i = 0; i < 5; i++) {
        while ((DMA1->ISR & DMA_ISR_TCIF1)  == 0);
        DMA1->IFCR = DMA_ISR_TCIF1;
        for (j = 0; j < 5; j++) {
            AdcTemp_Tab[j][i] = RegularConvData_Tab[j];
        }
    }

    for (i = 0; i < 5 ; i++) {
        for (j = 0; j < 4; j++) {
            for (k = j + 1; k < 5; k++) {
                if (AdcTemp_Tab[i][j] > AdcTemp_Tab[i][k]) {
                    tmp = AdcTemp_Tab[i][k];
                    AdcTemp_Tab[i][k] = AdcTemp_Tab[i][j];
                    AdcTemp_Tab[i][j] = tmp;
                }
            }
        }
    }

    vdda = 4095; // ((int32_t)AdcTemp_Tab[0][2]) * 4095 /((int32_t) (*vref_cal));
    for (i = 0; i < 3; i++) {
        u0 = (int32_t)AdcTemp_Tab[i + 2][2];
        if (u0 < 20)
            ret |= (2 << (i * 8));

        if (abs(vdda - u0) <  20) {
            r[i] = 100000000;
            ret |= (1 << (i * 8));
        } else {
            r[i] =  u0 * Temp_Rx / (vdda - u0);
        }
    }

#if 0
    for (int i = 0; i < 5; i++) {
        DPRINTF(("%04d ", AdcTemp_Tab[i][2]));
    }
    DPRINTF(("%d,%d\n", *vref_cal, *ts_cal));

    DPRINTF(("[Ref =%d] [Temp.=%d] ", AdcTemp_Tab[0][2], AdcTemp_Tab[1][2]));
    for (i = 0; i < 3; i++) {
        DPRINTF(("[n=%d ADC=%d] ", i, AdcTemp_Tab[i + 2][2]));
    }
    DPRINTF((" \n"));
#endif

    if (t1)
        *t1 = GetTempbyR(r[0]);

    if (t2)
        *t2 = GetTempbyR(r[1]);

    if (t3)
        *t3 = GetTempbyR(r[2]);
    return ret ;
}

