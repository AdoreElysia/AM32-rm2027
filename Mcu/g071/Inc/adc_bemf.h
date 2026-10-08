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

/* ★ 过零极性开关：运行时改（调试器写 1/0）即可试两种极性，不用重编译。
 *   首次上闭环若"不转/反转/一冲就停"，先翻它。 */
extern volatile uint8_t bemf_polarity_invert;

/* ★ 过零的"边沿标志"读出口（见 adc_bemf.c 里 adc_bemf_take_edges 的说明）。
 *   ISR 用它判"本帧是否发生了电平翻转"，而不是拿电平去比 —— 后者会随采样相位
 *   时认时不认（本板工作点上真过零恰好在时间门槛边界附近）。 */
void adc_bemf_take_edges(uint8_t* out_rise, uint8_t* out_fall,
                        uint8_t clear_rise, uint8_t clear_fall);

void     adc_bemf_set_hysteresis(uint16_t hyst_lsb);
uint16_t adc_bemf_get_hysteresis(void);
int16_t  adc_bemf_get_delta(void);      /* 当前悬空相 − V0（有符号） */

/* ★ AWD：把"相电压 vs 阈值"的比较交给 ADC 硬件，只在跨越时中断 */
void adc_bemf_config_watchdog(uint32_t awd_channel, uint16_t center, uint16_t hyst);
void adc_bemf_watchdog_for_phase(void);  /* 按当前 step 选悬空相并重设窗口 */
