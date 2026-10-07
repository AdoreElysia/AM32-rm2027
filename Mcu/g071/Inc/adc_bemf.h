/*
 * adc_bemf.h —— 软件过零检测（ADC 版 BEMF）
 *
 * 仅在本板（RM2027 自研电调）使用：定义 USE_ADC_BEMF 时生效。
 * 与 comparator.h 提供【完全同名】的接口，故 Src/main.c 的换相状态机无需改动。
 */
#pragma once

#include <stdint.h>

/* ADC 原始码（由 ADC DMA 扫描序列填入） */
extern volatile uint16_t bemf_adc_va;   /* PA5 = VA_SENSE */
extern volatile uint16_t bemf_adc_vb;   /* PA4 = VB_SENSE */
extern volatile uint16_t bemf_adc_vc;   /* PA2 = VC_SENSE */
extern volatile uint16_t bemf_adc_v0;   /* PA3 = V0_SENSE  */

/* ★ 过零检测的"武装"标志（取代比较器的 EXTI）：换相后置 1、认到过零清 0。
 *   ADC-DMA 完成中断里读它（Mcu/g071/Src/stm32g0xx_it.c）。 */
extern volatile uint8_t bemf_irq_armed;

void     adc_bemf_set_hysteresis(uint16_t hyst_lsb);
uint16_t adc_bemf_get_hysteresis(void);
int16_t  adc_bemf_get_delta(void);      /* 当前悬空相 − V0（有符号） */

/* ★ AWD：把"相电压 vs 阈值"的比较交给 ADC 硬件，只在跨越时中断 */
void adc_bemf_config_watchdog(uint32_t awd_channel, uint16_t center, uint16_t hyst);
void adc_bemf_watchdog_for_phase(void);  /* 按当前 step 选悬空相并重设窗口 */
