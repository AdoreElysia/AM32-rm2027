/*
 * dbg_trace.h —— 在线调试用「RAM 黑匣子」
 *
 * 目的：电机抖动时，光看单点变量看不出因果。这里在 RAM 里留下一段
 *       高分辨率的**事件时间线**，跑完之后用调试器一口气读出来分析：
 *         - 换相事件（每次 interruptRoutine）：间隔、占空、当时的三相/V0 码值
 *         - 过零事件（每次 zcfoundroutine）：悬空相−V0 的差值、是否过早被丢弃
 *         - 各类中断计数（AWD / ADC 回调 / 10kHz 拍）—— 用来发现「中断风暴」这类问题
 *         - burst：每次换相后连采 N 个 ADC 回调的「悬空相−V0」波形（相当于简易示波器）
 *
 * 【怎么用】ST-Link 连着电调，跑一段之后在 PC 上执行：
 *     python3 tools/dbg_trace_read.py            # 自动读地址 + dump + 解码
 * 详见 tools/dbg_trace_read.py 顶部说明。
 *
 * ⚠️ 三条纪律：
 *   1. 记录函数只做「存数」，不做计算、不阻塞、不调用 LL —— 保证不改变原有实时性。
 *   2. 结构体**不能加 __attribute__((packed))**：Cortex-M0+ 不支持非对齐访问，
 *      记录是数组、步长必须 4 的倍数，否则 uint32 成员会跑到非对齐地址 ⇒ HardFault。
 *      所以这里用显式 pad 把步长凑到 28 字节。
 *   3. Python 那边的解码格式必须和这里的字段顺序/步长**逐条对齐**（见 dbg_trace_read.py）。
 */
#pragma once
#include <stdint.h>

#define DBG_RING_N      192u    /* 事件环形缓冲条数（192 × 28B ≈ 5.3 KB） */
#define DBG_BURST_N     32u     /* 每次换相后连采多少个点 */
#define DBG_BURST_CNT   16u     /* 保留最近多少次换相的 burst */

/* ★ 长窗口（自由滚动、降采样）：用来一屏看到【多个连续步长】
 *   之前 burst 只有 32 点：952 Hz 时是 34 ms，改成 12.7 kHz 后只剩 2.5 ms ⇒ 看不到一个步长。
 *   这里每 DBG_LONG_DECIM 个 ADC 采样取一个点 ⇒ 12.7kHz/16 ≈ 794 Hz，
 *   256 点 ≈ 322 ms ⇒ 启动阶段（每步 50~110 ms）能覆盖 3~6 个步长。 */
#define DBG_LONG_N      256u    /* 长窗口点数 */
#define DBG_LONG_DECIM  16u     /* 降采样比 */

/* 事件类型 */
#define DBG_EV_COMMUT   1u      /* 换相（interruptRoutine 入口） */
#define DBG_EV_ZC       2u      /* 过零（zcfoundroutine 入口） */

/* flags 位 */
#define DBG_F_RUNNING   (1u << 0)
#define DBG_F_ARMED     (1u << 1)
#define DBG_F_BEMF      (1u << 2)   /* getCompOutputLevel() 的滞回状态 */
#define DBG_F_RISING    (1u << 3)
#define DBG_F_AWDFLAG   (1u << 4)   /* 进 ISR 时 AWD 标志还挂着（可能没被清） */

typedef struct {
    uint32_t t100;      /* 10 kHz 计数（×100 µs）—— 粗时基 */
    uint16_t interval;  /* INTERVAL_TIMER(TIM2@2MHz) 当前计数 → ×0.5 µs */
    uint16_t avg_int;   /* average_interval 当前值 */
    uint16_t ccr;       /* TIM1->CCR1（当前占空） */
    uint16_t center;    /* 当前 AWD 窗口中心（ADC 码） */
    int16_t  delta;     /* 悬空相 − V0（ADC 码，有符号） */
    uint16_t va, vb, vc, v0;
    uint8_t  type;      /* DBG_EV_* */
    uint8_t  step;
    uint8_t  flags;
    uint8_t  seq;       /* 低 8 位序号：用来发现漏记 */
    uint8_t  pad[2];    /* 显式补齐到 28 字节（保持 4 字节对齐，勿删） */
} dbg_ev_t;

typedef struct {
    /* ---- 计数（诊断中断风暴 / 采样率） ---- */
    volatile uint32_t commutations;   /* interruptRoutine 调用次数 */
    volatile uint32_t zc_events;      /* zcfoundroutine 调用次数 */
    volatile uint32_t zc_too_early;   /* 被「太早」判据丢弃的边沿次数 */
    volatile uint32_t awd_irq;        /* AWD 中断次数 */
    volatile uint32_t adc_cb;         /* ADC_DMA_Callback 次数（= 采样序列次数） */
    volatile uint32_t tenkhz_ticks;   /* 10 kHz 拍数（用于算 ADC 实际采样率） */
    volatile uint32_t step_err;       /* step 序列异常次数（重复/跳变） */
    volatile uint32_t spike_rejected; /* BEMF 采样被限速滤波判为尖刺并丢弃的次数 */

    /* ---- 最近状态快照（单点看） ---- */
    volatile uint16_t last_center;
    volatile uint16_t last_hyst;
    volatile int16_t  last_delta;
    volatile uint8_t  last_step;
    volatile uint8_t  enable;         /* 1=记录（调试器可直接改成 0 停录） */

    /* ---- 环形缓冲 ---- */
    volatile uint16_t head;
    volatile dbg_ev_t ring[DBG_RING_N];

    /* ---- burst：每次换相后连采「悬空相 − V0」 ---- */
    volatile uint8_t  burst_arm;      /* 还要采几个点 */
    volatile uint8_t  burst_slot;     /* 当前写第几个 burst（环形） */
    volatile uint32_t burst_count;    /* 已完成的 burst 数 */
    volatile int16_t  burst[DBG_BURST_CNT][DBG_BURST_N];

    /* ---- 长窗口（多步长示波器）---- */
    volatile int16_t  long_delta[DBG_LONG_N];   /* 悬空相 − V0（ADC 码） */
    volatile uint8_t  long_step[DBG_LONG_N];    /* 该点属于哪个 step（1..6，0=未记录） */
    volatile uint16_t long_head;                /* 下一个写入位置 */
    volatile uint8_t  long_div;                 /* 降采样计数 */
    volatile uint32_t long_wraps;               /* 绕了几圈（判断数据新旧） */
} dbg_trace_t;

extern volatile dbg_trace_t g_trace;

/* 初始化：打开记录（在 ADC 初始化之后调用） */
void dbg_trace_init(void);

/* 记录入口（都做成「几句话存完就返回」） */
void dbg_trace_commutation(uint8_t step_before);
void dbg_trace_zerocross(void);
void dbg_trace_too_early(void);
void dbg_trace_adc_sample(void);
void dbg_trace_awd_irq(void);
void dbg_trace_spike(void);      /* 限速滤波丢掉一个尖刺 */
void dbg_trace_tenkhz(void);
