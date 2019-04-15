
#include <string.h>

#include "common.h"
#include "global.h"
#include "board.h"
#include "uart.h"



#define USART1_TX_DMA_FLAG_TC            DMA1_FLAG_TC2
#define USART1_TX_DMA_FLAG_GL            DMA1_FLAG_GL2
#define USART1_RX_DMA_FLAG_TC            DMA1_FLAG_TC3
#define USART1_RX_DMA_FLAG_GL            DMA1_FLAG_GL3


#define COMM_OUT_ENABLE   GPIOA->BSRR = GPIO_Pin_12
#define COMM_OUT_DISABLE  GPIOA->BRR = GPIO_Pin_12

static int Rx_Buf[USART_RX_SIZE / 4];
static int Tx_Buf[USART_TX_SIZE / 4];
static volatile int8_t Tx_Busy = 0;

int __io_putchar(int ch)
{
    USART2->TDR = (ch & (uint16_t)0x01FF);
    while ((USART2-> ISR & USART_ISR_TXE) == 0);
    return ch;
}

void Usart_StdioInit(void)
{

    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /*  -- BaudRate = 115200 */
    USART2->BRR = 48000000 / 115200;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    return ;
}

void Usart_CommInit(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /*  -- BaudRate = 115200 */
    USART1->BRR = 48000000 / 115200;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    USART1_RX_DMA_CHANNEL->CPAR = (uint32_t)(&(USART1->RDR));
    USART1_RX_DMA_CHANNEL->CMAR = (uint32_t)Rx_Buf;
    USART1_RX_DMA_CHANNEL->CCR = DMA_CCR_CIRC | DMA_CCR_MINC | DMA_CCR_PL_1 ;
    USART1_RX_DMA_CHANNEL->CNDTR = (uint16_t)USART_RX_SIZE;

    USART1->CR3 |= USART_CR3_DMAR;
    USART1_RX_DMA_CHANNEL->CCR |= DMA_CCR_EN;


    USART1_TX_DMA_CHANNEL ->CPAR = (uint32_t)(&(USART1->TDR));
    USART1_TX_DMA_CHANNEL ->CMAR = (uint32_t) Tx_Buf;
    USART1_TX_DMA_CHANNEL ->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PL_1 ;
    USART1_TX_DMA_CHANNEL ->CNDTR = (uint16_t)USART_TX_SIZE;


    DMA1->IFCR = USART1_TX_DMA_FLAG_TC | USART1_TX_DMA_FLAG_GL;
    USART1_TX_DMA_CHANNEL ->CCR |= DMA_IT_TC;

    USART1->CR3 |= USART_CR3_DMAT;

    NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
    NVIC_SetPriority(DMA1_Channel2_3_IRQn, 2);

    TIM15_Config();

    DPRINTF(("[%d] Communication Usart Init ok.\n", (int32_t)SysTicks));
    return ;
}

uint8_t *Usart_Comm_RxBuf(void)
{
    return (uint8_t *) Rx_Buf;
}

uint8_t *Usart_Comm_TxBuf(void)
{
    return (uint8_t *) Tx_Buf;
}

void DMA1_Channel2_3_IRQHandler(void)
{
    if ((DMA1->ISR) & USART1_TX_DMA_FLAG_TC) {
        USART1_TX_DMA_CHANNEL ->CCR &= (uint16_t)(~DMA_CCR_EN);
        USART1->CR3 &= (uint32_t)~USART_CR3_DMAT;
        DMA1->IFCR = USART1_TX_DMA_FLAG_TC | USART1_TX_DMA_FLAG_GL;
        TIM15->CR1 |= TIM_CR1_CEN;
    }
}

void Usart_Comm_TxDone(void)
{
    Tx_Busy = 0;
    COMM_OUT_DISABLE;
}

void Usart_Comm_Tx(char *send, int len)
{
    COMM_OUT_ENABLE;
    DelayUS(200);
    memcpy(Tx_Buf, send, len);

    USART1_TX_DMA_CHANNEL ->CNDTR = len;
    DMA1->IFCR = USART1_TX_DMA_FLAG_TC;

    USART1->CR3 |= USART_CR3_DMAT;
    USART1_TX_DMA_CHANNEL ->CCR |= DMA_CCR_EN;
    Tx_Busy = 1;
}

void Usart_Comm_ResetTx(void)
{
    USART1_TX_DMA_CHANNEL ->CCR &= (uint16_t)(~DMA_CCR_EN);
    USART1->CR3 &= (uint32_t)~USART_CR3_DMAT;
    DMA1->IFCR = USART1_TX_DMA_FLAG_TC | USART1_TX_DMA_FLAG_GL;
    Tx_Busy = 0;
    COMM_OUT_DISABLE;
}

int8_t Usart_Comm_TxBusy(void)
{
    return (Tx_Busy);
}

int8_t Usart_Comm_WaitTx(int32_t timeout)
{
    do {
        if (Tx_Busy == 0) return 0;
        DelayUS(1);
    } while (--timeout > 0);

    return -1;
}



