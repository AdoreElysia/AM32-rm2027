/*
 * adc_zc.c —— 【环形缓冲 + 变号触发 + 回看单调】版过零检测器（实现）
 * 判据与参数依据见 adc_zc.h 顶部说明。
 */
#include "adc_zc.h"

#if ADC_ZC_BUF_LEN < ADC_ZC_MONO_N
#error "ADC_ZC_BUF_LEN 必须 >= ADC_ZC_MONO_N"
#endif
#if ADC_ZC_MONO_N < 2
#error "ADC_ZC_MONO_N 至少要是 2（否则没有相邻两点可比）"
#endif

/* ---- 环形缓冲区 ----
 * 写指针一直往前推，读的时候按"往回数 back 个"取值。
 * 只存 int16：本板悬空相 − V0 的摆幅约 ±1200 码，int16 绰绰有余。 */
static int16_t zc_buf[ADC_ZC_BUF_LEN];
static uint8_t zc_head  = 0;   /* 下一个要写入的位置 */
static uint8_t zc_count = 0;   /* 已写入的有效样本数（封顶到 BUF_LEN） */

volatile uint32_t adc_zc_n_rise   = 0;
volatile uint32_t adc_zc_n_fall   = 0;
volatile uint32_t adc_zc_n_reject = 0;
volatile uint16_t adc_zc_ev_cnt[ADC_ZC_EV_N]  = { 0 };
volatile uint8_t  adc_zc_ev_kind[ADC_ZC_EV_N] = { 0 };
volatile uint8_t  adc_zc_ev_idx      = 0;

void adc_zc_reset(void)
{
    for (uint8_t i = 0; i < ADC_ZC_BUF_LEN; i++) {
        zc_buf[i] = 0;
    }
    zc_head  = 0;
    zc_count = 0;
}

/* 取"往回数 back 个"的样本：back=0 是最新那个，back=1 是前一个 …… */
static int16_t zc_at(uint8_t back)
{
    uint8_t i = (uint8_t)((zc_head + ADC_ZC_BUF_LEN - 1u - back) % ADC_ZC_BUF_LEN);
    return zc_buf[i];
}

uint8_t adc_zc_push(int16_t e, uint16_t t_cnt)
{
    /* ---- 入队 ---- */
    zc_buf[zc_head] = e;
    zc_head = (uint8_t)((zc_head + 1u) % ADC_ZC_BUF_LEN);
    if (zc_count < ADC_ZC_BUF_LEN) {
        zc_count++;
    }
    if (zc_count < ADC_ZC_MONO_N) {
        return 0;   /* 历史还不够长，单调判不了（上电头 MONO_N-1 帧的盲区） */
    }

    int16_t now  = zc_at(0);
    int16_t prev = zc_at(1);

    /* ---- 第 1 关：变号 ----
     * 只认"跨过 0"这一件事，方向由它决定。
     * 用 <=0 / >=0 把"恰好等于 0"归到前一侧，避免 0 值被两边都算一遍。 */
    uint8_t rise = (prev <= 0 && now > 0);
    uint8_t fall = (prev >= 0 && now < 0);
    if (!rise && !fall) {
        return 0;
    }

    /* ---- 第 2 关：斜率上限（默认关）----
     * MONO_N>=4 时不需要；K 调小到 2~3 时靠它才压得住换相跳变。 */
#if ADC_ZC_SLOPE_CAP > 0
    int16_t dd = (int16_t)(now - prev);
    if (dd < 0) {
        dd = (int16_t)(-dd);
    }
    if (dd > (int16_t)ADC_ZC_SLOPE_CAP) {
        adc_zc_n_reject++;
        return 0;
    }
#endif

    /* ---- 第 3 关：回看 MONO_N 个样本必须严格单调 ----
     * 由旧到新逐对比较：b 从 MONO_N-1 数到 1，比 (b) 与 (b-1)。
     * 覆盖的就是"最新 MONO_N 个样本"之间的全部相邻对。
     * 换相跳变让某一对反向（甚至只是相等）⇒ 整关不过 ⇒ 不会被误认成过零。 */
    for (uint8_t b = (uint8_t)(ADC_ZC_MONO_N - 1u); b > 0u; b--) {
        int16_t older = zc_at(b);
        int16_t newer = zc_at((uint8_t)(b - 1u));
        if (rise && !(older < newer)) {
            adc_zc_n_reject++;
            return 0;
        }
        if (fall && !(older > newer)) {
            adc_zc_n_reject++;
            return 0;
        }
    }

    /* ---- 过零成立 ----
     * ev_cnt = 本次过零时的 INTERVAL_TIMER 读数。该定时器每次换相都被清零
     * ⇒ 这个值恒等于【换相 → 过零】的延迟（拍）。语义与实测值见 adc_zc.h。 */
    adc_zc_ev_cnt[adc_zc_ev_idx]  = t_cnt;
    adc_zc_ev_kind[adc_zc_ev_idx] = rise ? ADC_ZC_RISE : ADC_ZC_FALL;
    adc_zc_ev_idx = (uint8_t)((adc_zc_ev_idx + 1u) & (ADC_ZC_EV_N - 1u));

    if (rise) {
        adc_zc_n_rise++;
        return ADC_ZC_RISE;
    }
    adc_zc_n_fall++;
    return ADC_ZC_FALL;
}

/* ================= 闭环波形捕获（见 adc_zc.h 顶部说明） ================= */

volatile int16_t  zcw_vph[ZCW_N] = { 0 };
volatile int16_t  zcw_v0[ZCW_N]  = { 0 };
volatile uint8_t  zcw_step[ZCW_N] = { 0 };
volatile uint8_t  zcw_zc[ZCW_N]  = { 0 };
volatile uint16_t zcw_duty[ZCW_N] = { 0 };
volatile uint8_t  zcw_fwd[ZCW_N]   = { 0 };
volatile uint8_t  zcw_brake[ZCW_N] = { 0 };
volatile uint16_t zcw_ain[ZCW_N]   = { 0 };
volatile uint16_t zcw_idx      = 0;
volatile uint8_t  zcw_state    = 0;   /* 0=空转 1=已触发 2=冻结 */
volatile uint16_t zcw_trig_idx = 0;
volatile uint32_t zcw_n_trig   = 0;
static uint16_t   zcw_stop_at  = 0;
volatile uint32_t zcw_ci_trig  = 0;   /* ci 超过它就触发冻结（0=未武装）*/

void adc_zc_wave_push(int16_t vph, int16_t v0, uint16_t duty, uint8_t step, uint8_t zc_kind,
                      uint8_t fwd, uint8_t brake, uint16_t ain)
{
    /* ★ 已冻结 ⇒ 立刻返回，绝不能再写 ★
     *   上一版漏了这一句：state 到 2 之后仍在写，缓冲被后续空转数据一路覆盖
     *   ⇒ dump 出来只剩噪声。实测线索：冻结后两次读 zcw_idx 分别是 1024 和 134，
     *   还在涨 —— 冻结态下 index 不该动，这本身就说明在写。 */
    if (zcw_state == 2u) {
        return;
    }

    zcw_vph[zcw_idx]  = vph;
    zcw_v0[zcw_idx]   = v0;
    zcw_step[zcw_idx] = step;
    zcw_zc[zcw_idx]   = zc_kind;
    zcw_duty[zcw_idx] = duty;
    zcw_fwd[zcw_idx]   = fwd;
    zcw_brake[zcw_idx] = brake;
    zcw_ain[zcw_idx]   = ain;
    zcw_idx = (uint16_t)((zcw_idx + 1u) % ZCW_N);

    if (zcw_state == 1u && zcw_idx == zcw_stop_at) {
        zcw_state = 2u;          /* 填够触发后 ZCW_POST 个点 ⇒ 冻结 */
    }
}

/* 交棒那一刻调用：武装"ci 外扩触发"，并让缓冲从此刻起空转（state=0 仍会写） */
void adc_zc_wave_arm(uint32_t ci_trig)
{
    zcw_ci_trig = ci_trig;
    zcw_state   = 0u;      /* 空转：缓冲继续滚，等 ci 涨上去才冻结 */
}

/* 触发冻结（交棒处已不再直接调用；现在由 ISR 在 ci 超阈值时调） */
void adc_zc_wave_trigger(void)
{
    zcw_trig_idx = zcw_idx;
    zcw_stop_at  = (uint16_t)((zcw_idx + ZCW_POST) % ZCW_N);
    zcw_state    = 1u;
    zcw_n_trig++;
}
