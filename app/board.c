
#include "stm32f0xx.h"
#include "common.h"
#include "global.h"
#include "board.h"
#include "uart.h"

void  Delay(uint32_t nCount)
{
    do {
        __asm volatile("nop");
    } while (nCount --);
}

void DelayUS(uint32_t usec)
{
    usec = usec * 48;
    do {
        __asm volatile("nop");
    } while (usec --);
    return;
}

void DelayMS(uint32_t msec)
{
    int64_t toms;
    if (msec < 3) {
        DelayUS(msec * 1000);
        return ;
    } else {
        toms = (int64_t) msec + SysTicks;
        while (SysTicks < toms);
    }
}


void TIM16_Config(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;

    TIM16->CR1 =  TIM_CR1_ARPE ;
    TIM16->ARR = 10000 - 1;
    TIM16->PSC =  4799;  //48M / 4800 = 10K
    TIM16->EGR = 0x01;

    NVIC_EnableIRQ(TIM16_IRQn);
    NVIC_SetPriority(TIM16_IRQn, 3);
    TIM16->DIER |= TIM_IT_Update ;
    TIM16->CNT = 0x00;

    TIM16->CR1 |= TIM_CR1_CEN;

}

void TIM16_IRQHandler(void)
{
    if (TIM16->SR & TIM_IT_Update) {
        P1Hz ++;
        TIM16->SR = (uint16_t)~TIM_IT_Update;
    }
}



void TIM15_Config(void)
{

    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;

    TIM15->CR1 = TIM_CR1_OPM | TIM_CR1_ARPE ;
    TIM15->ARR = 1200 - 1;
    TIM15->PSC =  11;  //48M / 12 = 4M
    TIM15->EGR = 0x01;


    NVIC_EnableIRQ(TIM15_IRQn);
    NVIC_SetPriority(TIM15_IRQn, 2);

    TIM15->DIER |= TIM_IT_Update ;
    TIM15->CNT = 0x00;

    return ;
}


void TIM15_IRQHandler(void)
{
    if (TIM15->SR & TIM_IT_Update) {
        Usart_Comm_TxDone();
        TIM15->SR = (uint16_t)~TIM_IT_Update;
    }
}

void Periph_Clock_Enable(void)
{
    RCC->AHBENR |= (RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_DMA1EN) ;
    RCC->CSR |= RCC_CSR_LSION;
    Delay(1000);
    return ;
}


void GPIO_Config(void)
{
    GPIOA->MODER = 0x2928fca0;
    GPIOB->MODER = 0x00001550;

    GPIOA->OSPEEDR = 0x0d3cfcf0;
    GPIOB->OSPEEDR = 0x55555555;

    GPIOA->PUPDR = 0x24140050;
    GPIOB->PUPDR = 0x55502aa0;

    GPIOA->AFR[0] = 0x00001100;
    GPIOA->AFR[1] = 0x00000110;

    return ;

}

void IWDG_Config(void)
{
    IWDG->KR = ((uint16_t)0x5555);
    IWDG->PR = ((uint8_t)0x04);      //Prescaler 64
    IWDG->RLR = 40000 / 128;
    IWDG->KR = ((uint16_t)0xAAAA);   //Reload
    IWDG->KR = ((uint16_t)0xCCCC);   //Start
}

void IWDG_Reload(void)
{
    IWDG->KR = ((uint16_t)0xAAAA);   //Reload
}

void Heating_Ctrl1(int8_t set, int8_t *state)
{
    if (set) {
        GPIOB->BSRR = GPIO_Pin_2;
        if (state)
            *state = 1;
    } else {
        GPIOB->BRR = GPIO_Pin_2;
        if (state)
            *state = 0;
    }
}

void Heating_Ctrl2(int8_t set, int8_t *state)
{
    if (set) {
        GPIOB->BSRR = GPIO_Pin_3;
        if (state)
            *state = 1;
    } else {
        GPIOB->BRR = GPIO_Pin_3;
        if (state)
            *state = 0;
    }
}

void Heating_Ctrl3(int8_t set, int8_t *state)
{
    if (set) {
        GPIOB->BSRR = GPIO_Pin_4;
        if (state)
            *state = 1;
    } else {
        GPIOB->BRR = GPIO_Pin_4;
        if (state)
            *state = 0;
    }
}

void EValve_Ctrl(int8_t set, int8_t *state)
{
    if (set) {
        GPIOB->BSRR = GPIO_Pin_5;
        GPIOB->BSRR = GPIO_Pin_6;
        if (state)
            *state = 1;
    } else {
        GPIOB->BRR = GPIO_Pin_5;
        GPIOB->BRR = GPIO_Pin_6;
        if (state)
            *state = 0;
    }
}

int8_t Check1Top(void)
{
    return ((GPIOB->IDR & GPIO_Pin_10) ? 0 : 1);
}

int8_t Check1Bottom(void)
{
    return ((GPIOB->IDR & GPIO_Pin_11) ? 0 : 1);
}

int8_t Check2Top(void)
{
    return ((GPIOB->IDR & GPIO_Pin_12) ? 0 : 1);
}

int8_t Check2Bottom(void)
{
    return ((GPIOB->IDR & GPIO_Pin_13) ? 0 : 1);
}

int8_t Check3Top(void)
{
    return ((GPIOB->IDR & GPIO_Pin_14) ? 0 : 1);
}

int8_t Check3Bottom(void)
{
    return ((GPIOB->IDR & GPIO_Pin_15) ? 0 : 1);
}


