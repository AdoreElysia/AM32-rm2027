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
 * 换算（按网表实算，别再套"47K∥10K"那个旧口径）：
 *   1 LSB ≈ 3.3V/4096 = 0.806 mV；相电压→采样脚的传递系数 0.0637
 *   ⇒ H LSB ⇒ 相侧 ≈ H × 0.806 / 0.0637 ≈ H × 12.7 mV
 *   ⇒ 8 LSB ≈ 100 mV 相侧（闭环在 3700RPM 时悬空相的 delta ≈ ±32 码 ⇒ 余量 4 倍）
 * 低速起步时 delta 会小到几个码，届时把 BEMF_HYST_LSB 调到 2~4（按噪声定）。
 */
#ifndef BEMF_HYST_LSB
#define BEMF_HYST_LSB 8
#endif
static uint16_t bemf_hyst  = BEMF_HYST_LSB;
static uint8_t  bemf_state = 0;      /* 滞回后的稳定状态 */
static uint8_t  active_phase = 0;    /* 0=A(PA5)  1=B(PA4)  2=C(PA2) */

/* ★ 过零检测的"武装"标志（取代比较器的 EXTI 使能/屏蔽）
 *   comparator.c 是用 EXTI 中断来报"悬空相过零"的；本板没有比较器通路，
 *   改成在【ADC-DMA 完成中断】里判（见 Mcu/g071/Src/stm32g0xx_it.c）。
 *   语义与上游一致：换相后 PeriodElapsedCallback()/commutate() 调
 *   enableCompInterrupts() 武装；一旦认到过零就由检测端清零，
 *   直到下一次换相再武装 ⇒【每步只认一次过零】(否则同一个过零点会被反复触发)。
 */
volatile uint8_t bemf_irq_armed = 0;

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

/* ★ 用软件"武装标志"取代比较器的 EXTI 使能/屏蔽（接口名保持与 comparator.c 相同，
 *   main.c 一行不用改）：
 *     enableCompInterrupts() —— 换相后调用（上游：PeriodElapsedCallback() 里）⇒ 允许认过零
 *     maskPhaseInterrupts()  —— 认到过零/停机等场合调用 ⇒ 禁止认过零
 *   真正判过零的地方是 ADC-DMA 完成中断（每帧一次，见 stm32g0xx_it.c）：
 *     条件 = bemf_irq_armed && !zcfound && 距上次换相已过半个周期
 *            && getCompOutputLevel() == !rising   ⇒ 认过零 ⇒ 清标志 + interruptRoutine()
 *   （极性沿用 main.c getBemfState() 的上游约定：rising 步期望 getCompOutputLevel()==0。
 *    若上机出现"换相方向反/不转"，第一个要试的就是把这个 `!rising` 反过来。） */
void enableCompInterrupts(void) { bemf_irq_armed = 1; }
void maskPhaseInterrupts(void) { bemf_irq_armed = 0; }

#endif /* USE_ADC_BEMF */
