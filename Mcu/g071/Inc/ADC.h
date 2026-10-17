/*
 * ADC.h
 *
 *  Created on: May 20, 2020
 *      Author: Alka
 */

#include "main.h"
#include "targets.h"

#ifndef ADC_H_
#define ADC_H_

extern volatile uint8_t adc_order;       /* 0..3：A/B/C/V0 在 rank1..4 的轮转序号 */
extern volatile uint8_t adc_order_auto;  /* 1=每 N 个窗口自动轮转 */
extern volatile uint8_t adc_order_pending; /* 待切换顺序：在 ADC-TC 中断里安全应用 */
extern volatile uint8_t adc_frame_order;   /* 本帧映射实际使用的顺序（win_push 标记用） */
void adc_apply_order(uint8_t o);

void ADC_DMA_Callback();
void enableADC_DMA();
void activateADC();
void ADC_Init(void);

void Configure_DMA();

void Configure_ADC();

void Activate_ADC();

#endif /* ADC_H_ */
