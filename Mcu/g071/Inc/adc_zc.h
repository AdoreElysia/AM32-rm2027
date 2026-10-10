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

/* ★★★ 闭环波形捕获（2026-10-10 加，用户要求"看闭环的过零点"）★★★
 *   为什么不能用原来的 ADC_CAPTURE_MODE：它把 ADC 序列改成固定的 V0/VB 两路，
 *   delta 就不再是"动态悬空相 − V0" ⇒ 闭环判据拿到的是垃圾数据
 *   ⇒【开捕获就没有闭环，两者互斥】。
 *   这里改成：正常 ADC 序列【一行不改】，把每帧 delta 原样写进一块 RAM 环形缓冲，
 *   交棒那一刻触发冻结。跑完用 SWD 非侵入式 dump ⇒ 不动控制回路、不占串口。
 *
 *   冻结后缓冲里的内容 = 触发前 ZCW_N−ZCW_POST 个样本 + 触发后 ZCW_POST 个样本
 *   （环形缓冲天然给出这个窗口；从 zcw_idx 处开始按环序读就是时间顺序）。
 */
#ifndef ZCW_N
#define ZCW_N    1024      /* 全速 24kHz ⇒ 1024 点 = 42.7ms */
#endif
#ifndef ZCW_POST
#define ZCW_POST 768       /* 触发后保留多少点（触发前 256 点 ≈ 10.7ms）*/
#endif

/* ★★ 2026-10-10 改版：分别记 vph / v0 / step，不再只记 delta ★★
 *   为什么改：台架实测"交棒瞬间 delta 的直流偏置整体抬了约 +500 码"。
 *   但 delta = vph − v0，光看差值无法区分是哪一边动了 —— 三种可能的病根
 *   需要完全不同的修法：
 *     ① vph 跳高   ⇒ 采到的"悬空相"其实是被驱动相（rank1 指错相）；
 *     ② v0  塌低   ⇒ 虚拟中性点被续流拉偏（采样点滑出导通期）；
 *     ③ 两边都在动 ⇒ 采样时刻相对换相漂移。
 *   ⇒ 分三路记下来，一跑就能定位。 */
extern volatile int16_t  zcw_vph[ZCW_N];  /* 该帧的悬空相码（rank1）*/
extern volatile int16_t  zcw_v0[ZCW_N];   /* 该帧的 V0 码（rank2）*/
extern volatile uint8_t  zcw_step[ZCW_N]; /* 该帧的 step（1..6）—— 用来核 rank1 对不对 */
extern volatile uint8_t  zcw_zc[ZCW_N];   /* 该帧判据的结论：0=无 1=上升过零 2=下降过零 */
extern volatile uint16_t zcw_idx;         /* 环形写指针；冻结后从这里按环序读=最旧 */
extern volatile uint8_t  zcw_state;       /* 0=空转 1=已触发正在填 2=已冻结 */
extern volatile uint16_t zcw_trig_idx;    /* 触发时刻的 idx */
extern volatile uint32_t zcw_n_trig;      /* 触发过几次（多次起步时用来看最后一次）*/

void adc_zc_wave_push(int16_t vph, int16_t v0, uint8_t step, uint8_t zc_kind);
void adc_zc_wave_trigger(void);                     /* 交棒时调 */

/* 事件环长度（2 的幂，掩码寻址）。64 条过零 ≈ 35ms ≈ 10 个电周期 ——
 * 刚好盖住开环"每 4~5 个电周期丢一步"那个已知毛病的周期，才看得出规律。 */
#ifndef ADC_ZC_EV_N
#define ADC_ZC_EV_N 64
#endif

/* ---- 观测量（全部 volatile，供 SWD 非侵入式读取）---- */
extern volatile uint32_t adc_zc_n_rise;      /* 上升过零累计 */
extern volatile uint32_t adc_zc_n_fall;      /* 下降过零累计 */
extern volatile uint32_t adc_zc_n_reject;    /* 变号了但被单调/斜率关掉的次数 */
extern volatile uint16_t adc_zc_ev_cnt[ADC_ZC_EV_N];
extern volatile uint8_t  adc_zc_ev_kind[ADC_ZC_EV_N]; /* 对应方向：ADC_ZC_RISE / ADC_ZC_FALL */
extern volatile uint8_t  adc_zc_ev_idx;      /* 上面那个小环的下一个写入位置 */

/* ★★ ev_cnt[] 的语义（2026-10-10 实测确认，别再猜）★★
 *   记的是每次过零时的 INTERVAL_TIMER->CNT ⇒ 因为该定时器【每次换相都被清零】
 *   （开环换相也清，不只是 interruptRoutine 清 —— 数据证据：64 条读数全在
 *   840~990 且不单调），所以这个值恒等于：
 *
 *        【换相 → 过零】的延迟（拍，1 拍 = 0.5µs）
 *
 *   它就是相位关系的直接测量：
 *     · 理想（TI 的六步无感）应该是 0.50 个换相步；
 *     · 台架实测开环下是 0.81 个换相步（中位 890 拍，全部落在 796~968）
 *       —— 这正体现"开环时转子必然滞后于磁场"。
 *   ⇒ 看 ev_cnt 就能判断过零认在了步内的什么位置：偏小=认早了（可能是伪过零），
 *     偏大=认晚了（中间可能漏了一次）。
 *
 *   ⚠ 曾经这里还有一个 ev_gap[]（记"与上一次过零的差值"），已删：
 *     换相时 adc_zc_reset() 会让每个事件都变成"复位后的第一个" ⇒ 差值恒为 0，
 *     结构性不可能有值。绝对 CNT 才是有效量。
 */

#endif /* ADC_ZC_H */
