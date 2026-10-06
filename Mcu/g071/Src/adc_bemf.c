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

volatile uint16_t adc_bemf_last_center = 0;   /* 最近一次设定的 AWD 窗口中心 */

uint8_t adc_bemf_get_state(void) { return bemf_state; }

void adc_bemf_set_hysteresis(uint16_t hyst_lsb) { bemf_hyst = hyst_lsb; }
uint16_t adc_bemf_get_hysteresis(void)          { return bemf_hyst; }

/* 选当前【悬空】相 —— 判据与 comparator.c 的 step 映射完全一致：
 *   step 1/4 → C 悬空 ; 2/5 → A 悬空 ; 3/6 → B 悬空
 *
 * ★ 2026-10-06 修：这里必须**同时**把 AWD 窗口按新 step 重新配好。
 *   原来只改了 active_phase，`adc_bemf_watchdog_for_phase()` 全工程无人调用
 *   ⇒ AWD 从来没被配置过 ⇒ 过零永远检测不到 ⇒ 只能开环转 ⇒ 电机抖动。
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

    adc_bemf_watchdog_for_phase();   /* ★ 按新悬空相重设窗口（并清标志） */
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

/* ★★ AWD（模拟看门狗）：把"相电压 vs 阈值"的比较交给 ADC 硬件，只在跨越时中断
 *
 * 【为什么用单边窗口 instead of 中心±hyst 的窄带】
 *   STM32 的 AWD 只在【转换值落在窗口之外】时置标志/中断。
 *   如果把窗口设成 [center-h, center+h]（很小的窄带），那么相电压绝大部分时间都在
 *   窄带之外 ⇒ 每次转换都会中断（等于"一直响"），根本不能当"过零事件"用。
 *   正确做法是**按本次换相后预期的过零方向**只留一侧：
 *     - 预期上升过零（rising=1）：窗口 = [0, center-h]
 *       ⇒ 在相电压爬过阈值之前一直落在窗口内（不中断）；一旦越过 ⇒ 落在窗外 ⇒ 中断 ✅
 *     - 预期下降过零（rising=0）：窗口 = [center+h, 4095]
 *       ⇒ 同理，越过阈值（往下）才中断 ✅
 *   ⇒ 中断语义 = "悬空相已越过 VBAT/2" = 真正的过零点。
 *
 * 【滞回】窗口边缘本身就是阈值，center ± hyst 给出两个不同方向的触发点 ✓
 */
void adc_bemf_config_watchdog_dir(uint32_t awd_channel, uint16_t center, uint16_t hyst,
    uint8_t expect_rising)
{
    uint32_t low;
    uint32_t high;

    if (expect_rising) {
        high = (center > hyst) ? (uint32_t)(center - hyst) : 0u;
        low  = 0u;
    } else {
        low  = (uint32_t)center + hyst;
        high = 4095u;
        if (low > 4095u) { low = 4095u; }
    }

    LL_ADC_SetAnalogWDMonitChannels(ADC1, LL_ADC_AWD1, awd_channel);
    LL_ADC_ConfigAnalogWDThresholds(ADC1, LL_ADC_AWD1, high, low);
    LL_ADC_ClearFlag_AWD1(ADC1);

    /* ★★ 这里【故意不开中断】：只配窗口。
     *   中断的开关交给 enableCompInterrupts()/maskPhaseInterrupts()（AM32 状态机
     *   靠它们"只在寻找过零的时间窗内开中断"）。
     *   上一版在这里 EnableIT ⇒ 轮询模式和 AWD 中断模式同时活着
     *   ⇒ 同一个过零被两条路径各换一次相 ⇒ step 一次跳 2 步
     *   ⇒ 示波器上每相只被 PWM 1 步（三相都是 1 高 5 低）。 */
}

/* 兼容旧接口：等价于"未指定方向"时按上升沿处理（旧行为＝双边窄带，仅留作对照） */
void adc_bemf_config_watchdog(uint32_t awd_channel, uint16_t center, uint16_t hyst)
{
    adc_bemf_config_watchdog_dir(awd_channel, center, hyst, (uint8_t)(rising ? 1u : 0u));
}

/* 按当前 step 选悬空相 ⇒ 换监视通道 + 重算阈值（每次换相调用一次）
 *
 * ★ 2026-10-06 修：过零阈值中心原来写的是 `ADC_raw_volts / 2`，**量纲算错了**。
 *   两条分压链比例不同：
 *     相电压通道：10K/(47K+10K) = 0.17544 ⇒ 码值 = V_相 × 0.17544 × 4095/3.3
 *     电池通道  ：4.7K/(47K+4.7K) = 0.090909 ⇒ 码值 = V_BAT × 0.090909 × 4095/3.3
 *   六步换相里悬空相过零 ⟺ V_相 = V_BAT/2，代入：
 *     阈值码 = (V_BAT/2) × 0.17544 × k = V_BAT × 0.087719 × k
 *     电池码 =  V_BAT    × 0.090909 × k
 *     ⇒ 阈值码 = 电池码 × (0.087719/0.090909) = 电池码 × 0.9649
 *   而原来的 `/2` 只有应有值的 51.8% ⇒ 8V 时窗口中心落在相压 2.07V 而不是 4V
 *   ⇒ AWD 在远未过零时就报"过零" ⇒ 换相超前 ⇒ 抖动。
 */
void adc_bemf_watchdog_for_phase(void)
{
    extern uint16_t ADC_raw_volts;

    /* ★ 过零阈值中心的正确口径（2026-10-06 更正）：
     *   相电压采样网络不是简单 47K/10K，而是
     *     相线 ─R1(47K)─ 采样点 ─R12(4.7K)─ GND，  采样点 ─R2(10K)─ V0(星形)
     *   （依据：ESC-测量地图与预期电压.md §4.2 的勘误 + 网表）
     *   ⇒ 共模增益 = 4.7/(47+4.7) = 0.0909，与 VBAT_SENSE 同比例
     *   ⇒ 六步换相里悬空相过零 ⇺ 相位采样码 = 电池码 / 2
     *     （推导：过零时相位采样码 = 0.0455·VBAT，电池通道码 = 0.0909·VBAT）
     *   所以这里就用 /2 —— 原实现是对的，曾误改成 ×0.965 已改回。 */
    uint32_t center = (uint32_t)ADC_raw_volts / 2u;

    adc_bemf_last_center = (uint16_t)center;

    switch (active_phase) {
    case 0:  adc_bemf_config_watchdog_dir(BEMF_AWD_PHASE_A_CH, (uint16_t)center, bemf_hyst, (uint8_t)(rising ? 1u : 0u)); break;

    case 1:  adc_bemf_config_watchdog_dir(BEMF_AWD_PHASE_B_CH, (uint16_t)center, bemf_hyst, (uint8_t)(rising ? 1u : 0u)); break;

    default: adc_bemf_config_watchdog_dir(BEMF_AWD_PHASE_C_CH, (uint16_t)center, bemf_hyst, (uint8_t)(rising ? 1u : 0u)); break;
    }
}

/* ★ 2026-10-06 修：原来这两个是空实现，但 AM32 的换相状态机**依赖它们**
 *   来"只在寻找过零的时间窗内开中断"（interruptRoutine 里换相后立刻 mask，
 *   等 30° 电角度到点后再 enable）。空实现 ⇒ AWD 一直开着 ⇒ 过零事件会连续触发。
 *   这里映射成 AWD 中断的关/开（顺带清标志，避免打断风暴）。 */
void enableCompInterrupts(void)
{
    LL_ADC_ClearFlag_AWD1(ADC1);
    LL_ADC_EnableIT_AWD1(ADC1);
}

void maskPhaseInterrupts(void)
{
    LL_ADC_DisableIT_AWD1(ADC1);
    LL_ADC_ClearFlag_AWD1(ADC1);   /* 标志不清掉会立刻再进中断 */
}

#endif /* USE_ADC_BEMF */
