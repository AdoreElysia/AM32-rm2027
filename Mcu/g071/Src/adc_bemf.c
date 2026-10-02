/*
 * adc_bemf.c —— 软件过零检测（ADC 版 BEMF，带施密特滞回）
 *
 * 为什么需要它：
 *   本板主控 STM32G071GBU6N 上，三相电压只有 VC 落在比较器输入脚
 *   （PA2 = COMP2_INM，V0 在 PA3 = COMP2_INP），而 VB(PA4) / VA(PA5) 【没有 COMP 功能】
 *   ⇒ 三相比比较的硬件方案物理不可行 ⇒ 过零必须由软件判。
 *   （这正是硬件设计方所说"本该硬件做、要靠软件实现"的那一项。）
 *
 * 设计要点：
 *   1. 接口与 comparator.c 完全同名 ⇒ main.c 一行不改。
 *   2. getCompOutputLevel() 内部是【施密特滞回】比较器：越过 +H 才置 1、越过 −H 才置 0，
 *      中间保持 ⇒ 零点附近的噪声不会来回翻转（这是软件过零最关键的一招）。
 *   3. 触发源不再是比较器 EXTI，而是定时器/ADC-DMA 中断（见 stm32g0xx_it.c）。
 */
#include "adc_bemf.h"
#include "comparator.h"   /* 只为了 step / rising 等 extern 声明 */
#include "common.h"
#include "targets.h"

#ifdef USE_ADC_BEMF

/* ---- ADC 扫描结果（DMA 目标；上电初值取中点，避免未采样时乱跳） ---- */
volatile uint16_t bemf_adc_va = 2048;
volatile uint16_t bemf_adc_vb = 2048;
volatile uint16_t bemf_adc_vc = 2048;
volatile uint16_t bemf_adc_v0 = 2048;

/* ---- 施密特滞回阈值（ADC 码，12 位） ----
 * 1 LSB ≈ 3.3V/4096 = 0.806 mV；相电压衰减 10/57 = 0.1754
 * ⇒ 8 LSB(ADC) ≈ 6.4 mV ⇒ 折算到相电压侧 ≈ 37 mV
 * 初值 8，上机按波形再调（低速往下调、噪声大往上调）。
 */
static uint16_t bemf_hyst  = 8;
static uint8_t  bemf_state = 0;      /* 滞回后的稳定状态 */
static uint8_t  active_phase = 0;    /* 0=A(PA5)  1=B(PA4)  2=C(PA2) */

void adc_bemf_set_hysteresis(uint16_t hyst_lsb) { bemf_hyst = hyst_lsb; }
uint16_t adc_bemf_get_hysteresis(void)          { return bemf_hyst; }

/* 选当前【悬空】相 —— 判据与 comparator.c 的 step 映射完全一致：
 *   step 1/4 → C 悬空 ; 2/5 → A 悬空 ; 3/6 → B 悬空
 */
void changeCompInput(void)
{
    if (step == 1 || step == 4) {
        active_phase = 2;

    } else if (step == 2 || step == 5) {
        active_phase = 0;

    } else if (step == 3 || step == 6) {
        active_phase = 1;
    }
}

int16_t adc_bemf_get_delta(void)
{
    int16_t vph;

    switch (active_phase) {
    case 0:  vph = (int16_t)bemf_adc_va; break;

    case 1:  vph = (int16_t)bemf_adc_vb; break;

    default: vph = (int16_t)bemf_adc_vc; break;
    }

    return (int16_t)(vph - (int16_t)bemf_adc_v0);
}

/* ★ 施密特滞回比较器（取代 active_COMP->CSR >> 30 & 1）
 * 注意极性：Src/main.c 里用的是 `!getCompOutputLevel()`，此处沿用 comparator.c 的约定。
 */
uint8_t getCompOutputLevel(void)
{
    int16_t d = adc_bemf_get_delta();

    if (d > (int16_t)bemf_hyst) {
        bemf_state = 1;

    } else if (d < -(int16_t)bemf_hyst) {
        bemf_state = 0;
    }

    /* 落在 ±hyst 之间 ⇒ 保持原状态（这就是滞回） */
    return bemf_state;
}

/* ★★ AWD（模拟看门狗）：把阈值比较放进 ADC 硬件
 *   center = 悬空相的过零阈值 = ADC_raw_volts / 2（六步换相下 V_C 过零 ⟺ V_C = VBAT/2）
 *   hyst   = 滞回半宽，天然构成施密特（上下沿两个阈值）
 *   ⇒ 只有真正跨越窗口才中断；采样一万次里没跨越就一次都不打扰 CPU。
 */
void adc_bemf_config_watchdog(uint32_t awd_channel, uint16_t center, uint16_t hyst)
{
    uint32_t low  = (center > hyst) ? (uint32_t)(center - hyst) : 0u;
    uint32_t high = (uint32_t)center + hyst;

    if (high > 4095u) { high = 4095u; }

    LL_ADC_SetAnalogWDMonitChannels(ADC1, LL_ADC_AWD1, awd_channel);
    LL_ADC_ConfigAnalogWDThresholds(ADC1, LL_ADC_AWD1, high, low);
    LL_ADC_ClearFlag_AWD1(ADC1);
    LL_ADC_EnableIT_AWD1(ADC1);
}

/* 按当前 step 选悬空相 ⇒ 换监视通道 + 重算阈值（每次换相调用一次） */
void adc_bemf_watchdog_for_phase(void)
{
    extern uint16_t ADC_raw_volts;
    uint32_t center = (uint32_t)ADC_raw_volts / 2u;   /* 与 ADC 同一码域 */

    switch (active_phase) {
    case 0:  adc_bemf_config_watchdog(BEMF_AWD_PHASE_A_CH, center, bemf_hyst); break;

    case 1:  adc_bemf_config_watchdog(BEMF_AWD_PHASE_B_CH, center, bemf_hyst); break;

    default: adc_bemf_config_watchdog(BEMF_AWD_PHASE_C_CH, center, bemf_hyst); break;
    }
}

/* 触发源改为定时器 / ADC-DMA 中断 ⇒ 不再需要 EXTI 使能/屏蔽 */
void enableCompInterrupts(void) { }
void maskPhaseInterrupts(void) { }

#endif /* USE_ADC_BEMF */
