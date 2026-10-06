/*
 * dbg_trace.c —— RAM 黑匣子实现（见 Inc/dbg_trace.h 的设计说明）
 *
 * 记录函数全部是「读几个变量 + 存一条」，无循环、无 LL 调用、无阻塞，
 * 目的是**不改变被观测对象的实时性**（否则观测就失真了）。
 */
#include "dbg_trace.h"
#include "common.h"
#include "targets.h"

#ifdef USE_ADC_BEMF
#include "adc_bemf.h"
#endif

volatile dbg_trace_t g_trace;

/* 这些是 main.c 里的状态量（只读，不改） */
extern uint16_t tenkhzcounter;              /* 10 kHz 计数（100 µs 一拍） */
extern char step;
extern volatile char rising;
extern uint8_t running;
extern volatile char armed;

static uint8_t s_seq;

/* 默认打开记录：g_trace 在 .bss 里，不显式置位的话 enable 是 0（默认不记录）。
 * 调试时也可以反过来用：把 enable 写 0 就"冻结"当前内容，便于稳定读回。 */
void dbg_trace_init(void)
{
    g_trace.enable = 1u;
}

static void push_event(uint8_t type)
{
    volatile dbg_ev_t *e;
    uint16_t h;

    if (g_trace.enable == 0u) {
        return;
    }

    h = g_trace.head;
    e = &g_trace.ring[h];

    e->t100     = (uint32_t)tenkhzcounter;
#ifdef INTERVAL_TIMER
    e->interval = (uint16_t)INTERVAL_TIMER->CNT;
#else
    e->interval = 0u;
#endif
    e->avg_int  = (uint16_t)average_interval;
    e->ccr      = (uint16_t)TIM1->CCR1;
#ifdef USE_ADC_BEMF
    e->center   = adc_bemf_last_center;
    e->delta    = adc_bemf_get_delta();
    e->va       = bemf_adc_va;
    e->vb       = bemf_adc_vb;
    e->vc       = bemf_adc_vc;
    e->v0       = bemf_adc_v0;
#else
    e->center   = 0u;
    e->delta    = 0;
    e->va = e->vb = e->vc = e->v0 = 0u;
#endif
    e->type     = type;
    e->step     = (uint8_t)step;
    e->flags    = (uint8_t)((running ? DBG_F_RUNNING : 0u)
                          | (armed ? DBG_F_ARMED : 0u)
                          | (rising ? DBG_F_RISING : 0u));
    e->seq      = s_seq++;

    g_trace.last_step   = (uint8_t)step;
    g_trace.last_center = e->center;

    g_trace.head = (uint16_t)((h + 1u) % DBG_RING_N);
}

/* 换相入口（在 interruptRoutine 最开始调用 —— 此时 step 还是「换相前」的值） */
void dbg_trace_commutation(uint8_t step_before)
{
    /* step 序列检查：正常每步 +1（或反向 -1），只允许 1..6 */
    if ((step_before < 1) || (step_before > 6)) {
        g_trace.step_err++;
    } else {
        uint8_t prev = g_trace.last_step;

        if ((prev >= 1) && (prev <= 6)) {
            int8_t d = (int8_t)step_before - (int8_t)prev;

            if ((d != 1) && (d != -1) && (d != 5) && (d != -5)) {
                g_trace.step_err++;      /* 重复同一步 / 跳步 */
            }
        }
    }
    g_trace.last_step = step_before;

    g_trace.commutations++;

    /* 每次换相后连采一段「悬空相 − V0」，相当于简易示波器 */
    g_trace.burst_arm = DBG_BURST_N;

    push_event(DBG_EV_COMMUT);
}

/* 过零入口（zcfoundroutine 最开始） */
void dbg_trace_zerocross(void)
{
    g_trace.zc_events++;
    push_event(DBG_EV_ZC);
}

/* 边沿被「太早」判据丢弃 */
void dbg_trace_too_early(void)
{
    g_trace.zc_too_early++;
}

/* AWD 中断（软件过零的触发源） */
void dbg_trace_awd_irq(void)
{
    g_trace.awd_irq++;
}

/* BEMF 限速滤波丢掉一个尖刺 */
void dbg_trace_spike(void)
{
    g_trace.spike_rejected++;
}

/* 10 kHz 拍（算 ADC 实际采样率用） */
void dbg_trace_tenkhz(void)
{
    g_trace.tenkhz_ticks++;
}

/* ADC 扫描完成回调里调用：填 burst */
void dbg_trace_adc_sample(void)
{
    g_trace.adc_cb++;

    if (g_trace.enable == 0u) {
        return;
    }

#ifdef USE_ADC_BEMF
    g_trace.last_delta = adc_bemf_get_delta();
    g_trace.last_center = adc_bemf_last_center;
    g_trace.last_hyst = adc_bemf_get_hysteresis();

    /* 长窗口：每 DBG_LONG_DECIM 个采样取一点（自由滚动，永远是最新窗口） */
    if (++g_trace.long_div >= DBG_LONG_DECIM) {
        uint16_t h = g_trace.long_head;

        g_trace.long_div = 0;
        g_trace.long_delta[h] = g_trace.last_delta;
        g_trace.long_step[h] = (uint8_t)step;
        h = (uint16_t)((h + 1u) % DBG_LONG_N);
        if (h == 0u) {
            g_trace.long_wraps++;
        }
        g_trace.long_head = h;
    }

    if (g_trace.burst_arm != 0u) {
        uint8_t slot = g_trace.burst_slot;
        uint8_t k    = (uint8_t)(DBG_BURST_N - g_trace.burst_arm);

        g_trace.burst[slot][k] = g_trace.last_delta;

        if (--g_trace.burst_arm == 0u) {
            g_trace.burst_slot  = (uint8_t)((slot + 1u) % DBG_BURST_CNT);
            g_trace.burst_count++;
        }
    }
#endif
}
