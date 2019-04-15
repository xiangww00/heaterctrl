
#ifndef __BOARD_HH_
#define __BOARD_HH_

void Delay(uint32_t nCount);
void DelayUS(uint32_t usec);
void DelayMS(uint32_t msec);

void Periph_Clock_Enable(void);
void TIM15_Config(void);
void TIM16_Config(void);
void GPIO_Config(void);
void IWDG_Config(void);
void IWDG_Reload(void);


void Heating_Ctrl1(int8_t set, int8_t *state);
void Heating_Ctrl2(int8_t set, int8_t *state);
void Heating_Ctrl3(int8_t set, int8_t *state);
void EValve_Ctrl(int8_t set, int8_t *state);

int8_t Check1Top(void);
int8_t Check2Top(void);
int8_t Check3Top(void);
int8_t Check1Bottom(void);
int8_t Check2Bottom(void);
int8_t Check3Bottom(void);

#endif

