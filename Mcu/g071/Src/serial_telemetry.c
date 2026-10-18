/*
 * serial_telemetry.c
 *
 *  Created on: May 13, 2020
 *      Author: Alka
 */

#include "serial_telemetry.h"
#include "kiss_telemetry.h"
#include "targets.h"
#include "common.h"

void telem_UART_Init()
{
    LL_USART_InitTypeDef USART_InitStruct = { 0 };

    LL_GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    /* Peripheral clock enable */
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

    GPIO_InitStruct.Pin = LL_GPIO_PIN_6;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
    GPIO_InitStruct.Alternate = LL_GPIO_AF_0;
    LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    NVIC_SetPriority(USART1_IRQn, 3);
    NVIC_EnableIRQ(USART1_IRQn);

    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_3, LL_DMAMUX_REQ_USART1_TX);

    LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_3,
        LL_DMA_DIRECTION_MEMORY_TO_PERIPH);

    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PRIORITY_LOW);

    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MODE_NORMAL);

    LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PERIPH_NOINCREMENT);

    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MEMORY_INCREMENT);

    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PDATAALIGN_BYTE);

    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MDATAALIGN_BYTE);

    USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;
#if ADC_CAPTURE_MODE
    USART_InitStruct.BaudRate = ADC_CAP_BAUD;   /* ★ 捕获模式：1 Mbaud */
#else
    USART_InitStruct.BaudRate = ADC_CAP_BAUD;  /* ★★ xien/n2b：统一走 ADC_CAP_BAUD（默认 2 Mbaud）
                                                *   原来硬编码 1000000 会让 targets.h 里的波特率宏失效。 */
#endif
    USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
    USART_InitStruct.Parity = LL_USART_PARITY_NONE;
    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
    LL_USART_Init(USART1, &USART_InitStruct);
    LL_USART_SetTXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);
    LL_USART_SetRXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);
    LL_USART_DisableFIFO(USART1);
    LL_USART_ConfigHalfDuplexMode(USART1);

    LL_USART_Enable(USART1);
    while ((!(LL_USART_IsActiveFlag_TEACK(USART1))) || (!(LL_USART_IsActiveFlag_REACK(USART1)))) {
    }

    LL_DMA_ConfigAddresses(
        DMA1, LL_DMA_CHANNEL_3, (uint32_t)aTxBuffer,
        LL_USART_DMA_GetRegAddr(USART1, LL_USART_DMA_REG_DATA_TRANSMIT),
        LL_DMA_GetDataTransferDirection(DMA1, LL_DMA_CHANNEL_3));
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, sizeof(aTxBuffer));

    /* (5) Enable DMA transfer complete/error interrupts  */
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_3);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_3);
}

/* ★★ 2026-10-18 xien/n4-serial-ack：KISS 遥测不再自己抢 DMA1_CH3 ★★
 *   旧实现三个确定性 bug（WIP 台架 raw 实测）：
 *     ① 不看 tx_busy，和窗口流抢同一条 DMA，半途 DisableChannel 改地址/长度；
 *     ② 不重设 memory address（CMAR 被窗口流改过）；
 *     ③ 启动 DMA 后立刻 SetTransferDirection(RX) 把 TE 清掉，正在传的包静默丢。
 *   现在统一走 main.c 的仲裁入口 uart_tx_dma()：忙（包括半双工收窗）就丢这一帧遥测，绝不抢。 */
extern uint8_t uart_tx_dma_ready(void);
extern void    uart_tx_dma(uint8_t *p, uint16_t len);

void send_telem_DMA(uint8_t bytes)
{
    if (!uart_tx_dma_ready()) { return; }   /* 窗口流/收窗在用 ⇒ 丢这一帧遥测 */
    uart_tx_dma(aTxBuffer, bytes);
}
