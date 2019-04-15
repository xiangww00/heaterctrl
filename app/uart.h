
#ifndef _UART_H_
#define _UART_H_

#define USART_RX_SIZE  256
#define USART_TX_SIZE  128

#define USART1_TX_DMA_CHANNEL            DMA1_Channel2
#define USART1_RX_DMA_CHANNEL            DMA1_Channel3

int __io_putchar(int ch);
void Usart_StdioInit(void);

void Usart_CommInit(void);
void Usart_Comm_TxDone(void);
void Usart_Comm_ResetTx(void);
void Usart_Comm_Tx(char *send, int len);
int8_t Usart_Comm_TxBusy(void);
int8_t Usart_Comm_WaitTx(int32_t timeout);
uint8_t *Usart_Comm_RxBuf(void);
uint8_t *Usart_Comm_TxBuf(void);


#endif

