/*
 * adc_zc.h —— 【环形缓冲 + 变号触发 + 回看单调】版过零检测器
 *
 * 为什么写它：
 *   现有的 adc_bemf.c 用"施密特滞回 + 边沿标志"判过零，为了避开换相瞬间的振铃，
 *   还额外挂了两个时间门槛（换相后屏蔽 3 帧、75µs 下界）。这套东西能跑，但门槛都是
 *   拍脑袋定的，而且互相耦合（改一个另一个就不对了）。
 *
 *   本模块换一条更本质的路：不靠时间门槛，只靠【波形形状】。
 *   物理事实是：
 *     · 真过零 —— 悬空相反电势是一条平缓斜坡穿过零点，斜率只有 20~40 码/点，
 *                 而且过零前后连着好几个点都在朝同一个方向走（严格单调）；
 *     · 换相跳变 —— 悬空相被切走，相邻两点差 500~800 码，一步就跨过去，
 *                 它会把"连续 N 点单调"这件事打断。
 *   ⇒ 用"变号 + 回看 N 点严格单调"就能把两者分开，不需要知道换相发生在哪一刻。
 *
 * 判据（三级）：
 *   第 1 关 变号：新样本和它前一个样本跨过 0（prev <= 0 < now，或 prev >= 0 > now）
 *   第 2 关 斜率上限（可选，默认关）：|now - prev| <= ADC_ZC_SLOPE_CAP
 *   第 3 关 回看单调：往回数 ADC_ZC_MONO_N 个样本（含最新那个）必须严格递增/递减
 *   三级全过 ⇒ 报一次过零，方向由第 1 关决定。
 *
 * ★ 参数是实测定的（用本板真实采样数据、11 个窗口、23 个真过零 / 46 次换相跳变）：
 *     MONO_N | 斜率上限 | 判出 | 命中真过零 | 换相跳变(假) | 漏报
 *        2   |   不限   |  67  |     22     |     45      |  1
 *        3   |   不限   |  32  |     22     |     10      |  1
 *        4   |   不限   |  22  |     22     |      0      |  1   ← 取这个
 *        5   |   不限   |  22  |     22     |      0      |  1
 *        8   |   不限   |  19  |     19     |      0      |  4   ← 太深，真过零开始漏
 *   ⇒ MONO_N = 4 时"单调"这一关已经足够，连斜率上限都不需要（默认关掉）。
 *   （唯一的 1 个漏报是过零正好落在缓冲区开头、历史不够 4 个点，属上电头 4 帧的盲区。）
 *
 * ★ 当前集成状态：只在 ADC-DMA 中断里【喂数据 + 计数】，不参与换相 ⇒ 电机行为不变。
 *   要用它接管换相，把 Mcu/g071/Src/stm32g0xx_it.c 里那段判据换成读本模块的返回值即可。
 */
#ifndef ADC_ZC_H
#define ADC_ZC_H

#include <stdint.h>

/* 需要连续多少个样本严格单调（含最新那个）。实测 4 最优，见上面表格。 */
#ifndef ADC_ZC_MONO_N
#define ADC_ZC_MONO_N 4
#endif

/* 环形缓冲区格数。必须 >= ADC_ZC_MONO_N；多留几格便于以后加大回看深度。 */
#ifndef ADC_ZC_BUF_LEN
#define ADC_ZC_BUF_LEN 8
#endif

/* 斜率上限（码/点）。0 = 关闭。
 * MONO_N>=4 时不需要它；只有把 MONO_N 调小（2~3）时才需要它兜底。 */
#ifndef ADC_ZC_SLOPE_CAP
#define ADC_ZC_SLOPE_CAP 0
#endif

/* ★★ 过零判据选择开关 ★★
 *   0 = 原方案：施密特滞回(getCompOutputLevel) + 边沿标志 + 换相屏蔽 + 75µs 下界
 *   1 = 新方案：本模块（环形缓冲 + 变号 + 回看 MONO_N 点严格单调）
 * 两套都保留 bemf_irq_armed 当"每步只认一次"的闸、下游时序完全一致
 * （见 Mcu/g071/Src/stm32g0xx_it.c 里那段）。改回 0 即可无痛回退。 */
#ifndef BEMF_ZC_USE_RINGBUF
#define BEMF_ZC_USE_RINGBUF 1
#endif

#define ADC_ZC_RISE 1u    /* 上升过零：prev <= 0 < now */
#define ADC_ZC_FALL 2u    /* 下降过零：prev >= 0 > now */

/* 清空缓冲区与历史。上电、换相方向反转、丢锁重来时调用。 */
void adc_zc_reset(void);

/* 压入一个样本。e = 悬空相码 − V0码（即 adc_bemf_get_delta()）。
 * t_cnt = 采样那一刻的 INTERVAL_TIMER 读数，只在报过零时被记录，便于对时。
 * 返回 0 = 本次没有过零；ADC_ZC_RISE / ADC_ZC_FALL = 报了过零。 */
uint8_t adc_zc_push(int16_t e, uint16_t t_cnt);

/* 事件环长度（2 的幂，掩码寻址）。64 条过零 ≈ 35ms ≈ 10 个电周期 ——
 * 刚好盖住开环"每 4~5 个电周期丢一步"那个已知毛病的周期，才看得出规律。 */
#ifndef ADC_ZC_EV_N
#define ADC_ZC_EV_N 64
#endif

/* ---- 观测量（全部 volatile，供 SWD 非侵入式读取）---- */
extern volatile uint32_t adc_zc_n_rise;      /* 上升过零累计 */
extern volatile uint32_t adc_zc_n_fall;      /* 下降过零累计 */
extern volatile uint32_t adc_zc_n_reject;    /* 变号了但被单调/斜率关掉的次数 */
extern volatile uint16_t adc_zc_ev_cnt[ADC_ZC_EV_N];  /* 最近 N 次过零的 INTERVAL_TIMER 读数 */
extern volatile uint16_t adc_zc_ev_gap[ADC_ZC_EV_N];  /* ★ 与【上一次过零】的差值（拍）—— 见下 */
extern volatile uint8_t  adc_zc_ev_kind[ADC_ZC_EV_N]; /* 与上面对应：ADC_ZC_RISE / ADC_ZC_FALL */
extern volatile uint8_t  adc_zc_ev_idx;      /* 上面那个小环的下一个写入位置 */

/* ★ 为什么要有 ev_gap（2026-10-10 加，上一轮踩过的坑）★
 *   ev_cnt 记的是 INTERVAL_TIMER->CNT 的【绝对值】，而那个定时器只有
 *   interruptRoutine() 会把它清零 ⇒ 【开环期间没人清】，它自由跑，
 *   绝对值毫无意义（上一轮读回来一串乱数就是这么来的）。
 *   ev_gap = 本次 t_cnt − 上次 t_cnt（16 位模减）：定时器自由跑时它就等于
 *   真实间隔（只要间隔 < 65536 拍 = 32.8ms，换相步才 ~1100 拍，安全）。
 *   ⇒ 开环看 ev_gap（间隔），闭环看 ev_cnt（绝对值），两个数组各管一种模式。
 *   间隔 × 0.5µs = 过零周期：目标转速下应恒 ≈ 一个换相步（实测约 550µs = 1100 拍）。
 *   若出现 2 倍值 ⇒ 中间漏认了一次过零（开环在交棒转速下有"每 4~5 个电周期丢一步"
 *   的已知毛病，这个数组能把丢步率直接量化出来）。 */

#endif /* ADC_ZC_H */
