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
#include "adc_zc.h"       /* ★ 环形缓冲版过零检测器（换相时要让它复位，见 changeCompInput）*/

#ifdef USE_ADC_BEMF

/* ---- ADC 扫描结果（DMA 目标；上电初值取中点，避免未采样时乱跳） ----
 * ★ 2026-10-10 重排后只有两路与判据有关：
 *   bemf_adc_vph = rank1 = 【当前悬空相】（通道由 changeCompInput 每次换相动态改写）
 *   bemf_adc_v0  = rank2 = V0
 *   另两相不再采样 —— 判据只用"悬空相 − V0"，它们没人读（原来是采了不用，
 *   白占 40 拍采样窗口，而窗口正是 duty 下限的瓶颈）。 */
volatile uint16_t bemf_adc_va  = 2048;
volatile uint16_t bemf_adc_vb  = 2048;
volatile uint16_t bemf_adc_vc  = 2048;
volatile uint16_t bemf_adc_vph = 2048;
volatile uint16_t bemf_adc_v0  = 2048;
volatile uint16_t bemf_adc_v0_hw = 2048;   /* ★ 硬件 V0 原值（日志用） */

/* 由 ADC.c 提供：动态改写 ADC 序列的 rank1 通道 */
extern void adc_set_first_rank(uint32_t channel);

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

/* ★★ 过零的【边沿】标志（等价上游比较器 EXTI 的边沿语义）★★
 *   为什么必须有它：本板没有比较器，过零只能在 ADC-DMA 完成中断里"看电平"判。
 *   而真过零（悬空相 BEMF 穿过中点）发生在【换相后约半个步长】，本板工作点上
 *   往往正好压在 ISR 那条时间门槛的边界上 ⇒ 用"电平"判会出现"这一步认到、
 *   下一步认不到"，换相相位前后乱掉（实测波形 C 相缺 0V / B 相缺 12V 就是这个）。
 *   改成边沿检测后：只要状态真的翻转了一次，就一定会被报出一次，
 *   与采样周期多长无关 —— 这正是上游 EXTI 锁存边沿的等价物。
 *   bemf_rising_edge：本次调用是否发生了 0→1 翻转。
 */
static uint8_t  bemf_edge_rise = 0;
static uint8_t  bemf_edge_fall = 0;

/* ★★ 2026-10-09："换相后第一帧采样"的屏蔽标志。
 *   换相会把悬空相换掉 ⇒ 下一帧采样的 delta 必然跳变 ⇒ 必然置出一个假边沿。
 *   置 1 表示"下一帧的边沿整个丢弃，只用来重新落定滞回状态"。
 *   由 changeCompInput()（每次换相）与交棒处设置；ADC-DMA 中断里消费并清 0。 */
volatile uint8_t bemf_blank = 0;

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
    /* ★ 换相（切到新的悬空相）⇒ 把上一相的边沿标志清掉。
     *   否则"上一步末尾残留的翻转"会被下一步当成自己的过零报出去。
     *   这一步等价于上游比较器切输入时重新 arm EXTI。 */
    bemf_edge_rise = 0;
    bemf_edge_fall = 0;
    /* ★★ 2026-10-09：再"屏蔽"掉紧随换相之后的那一帧采样。
     *   为什么只清边沿标志不够：清标志发生在换相那一刻，而"悬空相被换掉"
     *   引起的电平突变要到【下一帧 ADC 采样】才会被 getCompOutputLevel() 看到
     *   —— 于是那一帧又置出一个新边沿，被当成过零收下。
     *   实测后果（12V 台架，非侵入式读 RAM）：交棒瞬间 ci=1500 一切正常，
     *   紧接着 commutation_intervals 变成 [70,62,62,66,77,77]、ci 一路
     *   0.75× 衰减到 62（≈31µs，比 PWM 周期 41.6µs 还短）⇒ 换相比 PWM 还快
     *   ⇒ 六步波形被搅烂，现场表现就是"输出成了恒定高的 PWM、电机不转"。
     *   把这一帧的边沿整个丢弃，就等于把"换相振铃"从过零判据里摘出去。 */
    /* ★ 2026-10-10：从"屏蔽 1 帧"改成"屏蔽 BEMF_BLANK_FRAMES 帧"（用户要的 10）。
     *   原因见 targets.h 里该宏的说明：1 帧只有 41.7µs，盖不住 33~73µs 的换相振铃，
     *   尾巴会被当成过零收下 ⇒ 闭环锁在自己的换相动作上 ⇒ 拖转子、滑步、抖。 */
    bemf_blank = BEMF_BLANK_FRAMES;

    /* ★★★ 2026-10-10：把"新的悬空相"写到 ADC 序列的 rank1 ★★★
     *   这样每帧采到的 ADCDataDMA[0] 就恒等于"当前悬空相"，rank2 恒为 V0
     *   ⇒ 判据要的两路只占 2 档 40 拍（原来是 4 档 80 拍）
     *   ⇒ 采样窗口的需求从 CCR4+80 降到 CCR4+40 ⇒ duty 下限从约 13% 降到约 9%。
     *   映射与 comStep 一致：step 1/4 → C 悬空，2/5 → A 悬空，3/6 → B 悬空。 */
    if (step == 1 || step == 4) {
        active_phase = 2;
/* ★ 2026-10-12 四路电压版：rank1~4 固定为 VA/VB/VC/V0，
         *   这里【必须停掉】动态改写，否则 rank1 会被改成悬空相、三路全乱。
         *   悬空相改由 ADC_DMA_Callback 按 step 在软件里选。 */
#if 0
        adc_set_first_rank(BEMF_ADC_PHASE_C_CH);
#endif
    } else if (step == 2 || step == 5) {
        active_phase = 0;
/* ★ 2026-10-12 四路电压版：rank1~4 固定为 VA/VB/VC/V0，
         *   这里【必须停掉】动态改写，否则 rank1 会被改成悬空相、三路全乱。
         *   悬空相改由 ADC_DMA_Callback 按 step 在软件里选。 */
#if 0
        adc_set_first_rank(BEMF_ADC_PHASE_A_CH);
#endif
    } else if (step == 3 || step == 6) {
        active_phase = 1;
/* ★ 2026-10-12 四路电压版：rank1~4 固定为 VA/VB/VC/V0，
         *   这里【必须停掉】动态改写，否则 rank1 会被改成悬空相、三路全乱。
         *   悬空相改由 ADC_DMA_Callback 按 step 在软件里选。 */
#if 0
        adc_set_first_rank(BEMF_ADC_PHASE_B_CH);
#endif
    }
    /* ★★★ 2026-10-10 关键修复③ ★★★
     *   捕获模式(ADC_CAPTURE_MODE=1)下必须【停掉上面这个动态改写】：
     *   它每次换相都把 ADC 的 rank1 改成"当前悬空相"，于是
     *     ① 我写的固定 rank1(=VA/CH5) 被覆盖掉；
     *     ② 实测 CHSELR 变成 rank1=rank2=CH4(VB) —— 两个通道采同一个节点
     *        ⇒ 抓出来的"VA"其实是 VB ⇒ 两条波形几乎重合、看着像"错位"。
     *   （用户一眼看出"这不可能只是错位" —— 对，这个 bug 是他抓出来的。） */

#if BEMF_ZC_USE_RINGBUF
    /* ★★★ 2026-10-10：换相时让环形缓冲过零检测器复位 ★★★
     *   目的：让【回看窗口不跨换相】。
     *
     *   为什么必需：固件用的 delta 是"动态选中的悬空相 − V0"，每一步都换相
     *   ⇒ 每一次换相的跳变都是"伪过零"的候选。若回看窗口横跨换相，
     *   "旧相的斜坡 + 换相跳变"能凑出"连续 MONO_N 点单调" ⇒ 换相后 40~90µs
     *   被判成一次过零。实测台架数据（64 条过零记录）：6 条延迟只有 18~92µs，
     *   而真过零的中位延迟是 876 拍 = 438µs ⇒ 那 6 条全是伪过零。
     *
     *   ⚠ 这个工况【离线验证覆盖不到】：离线用的是固定相 VB−V0，每转只有 B 悬空
     *     的那 2 步有有效反电势，换相跳变只有 2 种情形；固件是每步都换相。
     *     所以离线测出的"22/22 命中、0 误报"不能直接外推到固件。
     *
     *   复位后要重新攒够 MONO_N=4 个本相样本才允许判 ⇒ 换相后最早判定点在
     *   4 帧 = 167µs = 334 拍 ⇒ 上面那 6 条（37~183 拍）整类被排除，
     *   而真过零（中位 876 拍）完全不受影响。
     *
     *   ★ 这比"换相后屏蔽 3 帧"更本质：那个是拍脑袋定的时间常数（125µs），
     *     这个是结构性的（由 MONO_N 决定），改 MONO_N 时不用再同步改屏蔽帧数。
     *   ★ 注意：这是【另一个】bemf_blank（上面那行）之外的独立机制，
     *     两者都留着 —— 一个管旧判据、一个管新判据，互不干扰。 */
    adc_zc_reset();
#endif
}

int16_t adc_bemf_get_delta(void)
{
    /* ★ 2026-10-10 重排后不再需要按 active_phase 选路：
     *   rank1 已经被 changeCompInput() 动态设成"当前悬空相"，所以
     *   ADCDataDMA[0] 就是它 ⇒ 直接相减即可（也省掉一次 switch 的开销，
     *   这个函数跑在 24kHz 的中断里）。 */
    return (int16_t)((int16_t)bemf_adc_vph - (int16_t)bemf_adc_v0);
}

/* ★ 2026-10-10 记录（A 相读不到的那一段已结案，别再从"软件绕过"这个方向想）：
 *   症状：闭环判据在 step 2/5（A 相悬空）完全没有边沿，六步里缺两步。
 *   一度以为是 PA5 引脚坏了、并写过一版"用虚拟中性点反推 A 相电压"的补丁
 *   （VA = 3·V0 − VB − VC，数学上是成立的），但那是误判 —— 真正的原因有两个，
 *   都不是固件问题：
 *     ① 我读的符号地址过期：`bemf_adc_va` 在 .bss 里随每次构建漂移
 *        （加埋点前是 0x200003cc，加完之后变成 0x200003d2），我按旧地址读到的
 *        其实是 `bemf_adc_v0` ⇒ 得出"PA5 恒为 0"的错误结论。
 *        **教训：任何 SWD 读变量前，必须从【当前烧进去的那份】ELF 现取地址。**
 *     ② 板子本身确有一处硬件不良，硬件同学修复后 A 相恢复正常。
 *   ⇒ 补丁已删除，恢复下面的直接读法。实测修复后 step 2/5 与其他四步一样有边沿
 *     （step2: 12 次记录/8 次接受，step5: 9/7）。 */

/* ★ 极性开关（运行时可改，不用重编译）：上机第一次跑闭环时，正确极性是"50/50 猜"的
 *   ——本板 A/B 相没有比较器可比对，只能与"照抄比较器约定"的代码对照。
 *   若一上电就出现"不转 / 反转 / 一冲就停"，在调试器里把本变量改成 1（或改回 0）再试一次。
 *   它同时作用于两条读法：轮询（main.c getBemfState）与中断（stm32g0xx_it.c），
 *   因为两者都经过 getCompOutputLevel()。 */
volatile uint8_t bemf_polarity_invert = 0;

/* ★ 施密特滞回比较器（取代 active_COMP->CSR >> 30 & 1）
 * 注意极性：Src/main.c 里用的是 `!getCompOutputLevel()`，此处沿用 comparator.c 的约定。
 */
/* ★★ 2026-10-09 【软件滑动平均】—— 提信噪比，治"闭环丢步" ★★
 *   背景（闭环第一次真正交棒成功后实测）：
 *     · bemf_dmax = 22 码 —— 悬空相 |delta| 峰值只有 22 码（占 ADC 满量程 0.5%）
 *     · 项目笔记实测噪声底 ±5 码 ⇒ 信噪比只有 4~6 倍
 *     · 后果：接受的过零 delta 全是 ±9~13（刚好越过阈值）、拒收是接受的 4.4 倍、
 *       而且认到的时刻 CNT/CI 在 0.6~36 之间乱跳
 *     ⇒ 换相时刻忽早忽晚 ⇒ 磁场时而超前时而滞后 ⇒ 转子被拽一下松一下
 *     ⇒【丢步】（用户判断："我感觉闭环也是丢步" —— 是对的）
 *   为什么不用 ADC 硬件过采样：G0 的过采样作用在整条扫描序列上，4× 就要把
 *     7 通道扫描从 ~2.5µs 拉到 10µs，而 17.6% 占空比下导通期只有 7.3µs ⇒ 装不下。
 *   ⇒ 改在软件层做：对 delta 取 4 点滑动平均，噪声按 √4 = 2 倍下降，
 *     代价是 ~83µs 相位滞后（1818 步/秒 下约占一步的 15% = 9° 电角度，可接受）。
 *   为什么是 4 点而不是更多：8 点能把噪声降到 1.8 码，但滞后 167µs（约 18° 电角度），
 *     会把过零判据的时间窗挤掉 —— 先用 4 点，实测不够再调 OL_BEMF_AVG_N。 */
#ifndef OL_BEMF_AVG_N
#define OL_BEMF_AVG_N 1   /* 上轮加的滑均：信号有 1025 码，不需要降噪；留着反而白添 83µs 滞后 ⇒ 关掉 */
#endif
static int16_t  bemf_avg_buf[OL_BEMF_AVG_N];
static uint8_t  bemf_avg_idx = 0;
static uint8_t  bemf_avg_filled = 0;
static int32_t  bemf_avg_sum = 0;

uint8_t getCompOutputLevel(void)
{
    int16_t d_raw = adc_bemf_get_delta();
    /* ---- 4 点滑动平均（环缓冲；未填满前用已有样本的平均，避免开机假值） ---- */
    bemf_avg_sum -= bemf_avg_buf[bemf_avg_idx];
    bemf_avg_buf[bemf_avg_idx] = d_raw;
    bemf_avg_sum += d_raw;
    bemf_avg_idx = (uint8_t)((bemf_avg_idx + 1) % OL_BEMF_AVG_N);
    if (!bemf_avg_filled && bemf_avg_idx == 0) {
        bemf_avg_filled = 1;
    }
    int16_t d = (int16_t)(bemf_avg_sum / OL_BEMF_AVG_N);
    uint8_t prev_level = bemf_state;

    if (d > (int16_t)bemf_hyst) {
        bemf_state = 1;

    } else if (d < -(int16_t)bemf_hyst) {
        bemf_state = 0;
    }

    /* 落在 ±hyst 之间 ⇒ 保持原状态（这就是滞回）。
     * ★ 顺便记录边沿：状态真的翻了一次 ⇒ 置对应标志，供 ISR 判过零用。
     *   注意比较的是"翻转前的电平"，与 bemf_polarity_invert 无关 —— 极性开关
     *   只影响"对外报出的电平"，不影响"状态有没有翻"这件事。 */
    if (bemf_state && !prev_level) {
        bemf_edge_rise = 1;
    } else if (!bemf_state && prev_level) {
        bemf_edge_fall = 1;
    }

    return bemf_polarity_invert ? (uint8_t)!bemf_state : bemf_state;
}

/* ★ 取出并清掉"自上次调用以来"的边沿标志（边沿检测的读出口）。
 *   clear_rise / clear_fall 为 1 表示"一并清零该方向的标志"：
 *   ISR 用「读到上升沿就清上升沿」的方式调用，于是同一个过零点只会被报一次，
 *   等价于上游 EXTI 的"边沿锁存 + 清标志"。 */
void adc_bemf_take_edges(uint8_t* out_rise, uint8_t* out_fall,
                        uint8_t clear_rise, uint8_t clear_fall)
{
    if (out_rise) {
        *out_rise = bemf_edge_rise;
    }
    if (out_fall) {
        *out_fall = bemf_edge_fall;
    }
    if (clear_rise) {
        bemf_edge_rise = 0;
    }
    if (clear_fall) {
        bemf_edge_fall = 0;
    }
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

/* 取走并清掉"屏蔽下一帧"标志（ADC-DMA 中断里用） */
/* ★ 2026-10-10 语义改成【倒计数】：每个 ADC 帧减 1，减到 0 之前都算"屏蔽中"。
 *   调用点（ADC-DMA 中断）不用改 —— 它每帧调一次、返回非 0 就丢掉本帧边沿，
 *   正好等价于"屏蔽 N 帧"。 */
uint8_t adc_bemf_blank_take(void)
{
    if (bemf_blank == 0) {
        return 0;
    }
    bemf_blank--;
    return 1;
}

#endif /* USE_ADC_BEMF */
