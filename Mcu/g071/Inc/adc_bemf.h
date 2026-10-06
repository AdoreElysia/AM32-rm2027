/*
 * adc_bemf.h —— 软件过零检测（ADC 版 BEMF）
 *
 * 仅在本板（RM2027 自研电调）使用：定义 USE_ADC_BEMF 时生效。
 * 与 comparator.h 提供【完全同名】的接口，故 Src/main.c 的换相状态机无需改动。
 */
#pragma once

#include <stdint.h>

/* BEMF 限速滤波阈值（ADC 码）：单个采样点相对上一拍变化超过它就判为尖刺丢弃。
 * 真实 BEMF 斜率约 30 码/ms（1 kHz 采样），取 100 码留 3 倍余量；
 * 而 PWM 不同步造成的尖刺幅度可达 ±300 码 ⇒ 能挡掉。 */
#define DBG_BEMF_SLEW_LIMIT   60
extern volatile uint16_t bemf_adc_va;   /* PA5 = VA_SENSE */
extern volatile uint16_t bemf_adc_vb;   /* PA4 = VB_SENSE */
extern volatile uint16_t bemf_adc_vc;   /* PA2 = VC_SENSE */
extern volatile uint16_t bemf_adc_v0;   /* PA3 = V0_SENSE  */

void     adc_bemf_set_hysteresis(uint16_t hyst_lsb);
uint16_t adc_bemf_get_hysteresis(void);
int16_t  adc_bemf_get_delta(void);      /* 当前悬空相 − V0（有符号） */
uint8_t  adc_bemf_get_state(void);      /* 滞回后的稳定状态（只读，无副作用） */

/* ★ AWD：把"相电压 vs 阈值"的比较交给 ADC 硬件，只在跨越时中断 */
void adc_bemf_config_watchdog(uint32_t awd_channel, uint16_t center, uint16_t hyst);
void adc_bemf_config_watchdog_dir(uint32_t awd_channel, uint16_t center, uint16_t hyst,
    uint8_t expect_rising);   /* 单边窗口：中断语义 = "已越过阈值" */
void adc_bemf_watchdog_for_phase(void);  /* 按当前 step 选悬空相并重设窗口 */

/* 最近一次设定的 AWD 窗口中心（ADC 码）—— 给调试黑匣子记录用 */
extern volatile uint16_t adc_bemf_last_center;
