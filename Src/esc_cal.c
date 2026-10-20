#include "esc_cal.h"
#include <string.h>
#include <stddef.h>
#include "eeprom.h"
#include "main.h"
#include "targets.h"   /* K6：要用 RM2027_SG_NUM / RM2027_SO_OFF 做初值 */    /* ★ K2a-3：要用 TIM1 寄存器（BDTR.MOE）与 LL_TIM_EnableAllOutputs */

esc_cal_t        esc_cal;
volatile uint8_t esc_cal_state = 0;

static uint16_t cal_crc16(const uint8_t *p, uint16_t n)
{
    uint16_t c = 0xFFFFu;
    for (uint16_t i = 0; i < n; i++) {
        c ^= (uint16_t)((uint16_t)p[i] << 8);
        for (uint8_t b = 0; b < 8u; b++) {
            c = (c & 0x8000u) ? (uint16_t)((c << 1) ^ 0x1021u) : (uint16_t)(c << 1);
        }
    }
    return c;
}

void esc_cal_defaults(void)
{
    memset(&esc_cal, 0, sizeof(esc_cal));
    esc_cal.magic = ESC_CAL_MAGIC;
    esc_cal.ver   = ESC_CAL_VER;
    for (uint8_t i = 0; i < 4u; i++) { esc_cal.ch_g[i] = 1000; }
    for (uint8_t i = 0; i < 7u; i++) { esc_cal.sg[i]   = 1000; }
    for (uint8_t i = 0; i < 6u; i++) { esc_cal.wt[i]   = 500;  }
    esc_cal.crc = cal_crc16((const uint8_t *)&esc_cal, (uint16_t)offsetof(esc_cal_t, crc));
    esc_cal_state = 0;
}

void esc_cal_load(void)
{
    read_flash_bin((uint8_t *)&esc_cal, ESC_CAL_ADDR + ESC_CAL_OFF, sizeof(esc_cal));
    uint16_t c = cal_crc16((const uint8_t *)&esc_cal, (uint16_t)offsetof(esc_cal_t, crc));
    if (esc_cal.magic != ESC_CAL_MAGIC || esc_cal.ver != ESC_CAL_VER || esc_cal.crc != c) {
        esc_cal_defaults();
    } else {
        esc_cal_state = 1;
    }
}

/* 写前先整页读出备份 → 改校准区 → 页对齐擦写 → 读回校验 */
uint8_t esc_cal_save(const esc_cal_t *c)
{
    static uint8_t page[2048];
    esc_cal_t tmp = *c;
    tmp.magic = ESC_CAL_MAGIC;
    tmp.ver   = ESC_CAL_VER;
    tmp.crc   = cal_crc16((const uint8_t *)&tmp, (uint16_t)offsetof(esc_cal_t, crc));

    read_flash_bin(page, ESC_CAL_ADDR, (int)sizeof(page));
    memcpy(&page[ESC_CAL_OFF], &tmp, sizeof(tmp));
    save_flash_nolib(page, (int)sizeof(page), ESC_CAL_ADDR);

    {
        esc_cal_t chk;
        read_flash_bin((uint8_t *)&chk, ESC_CAL_ADDR + ESC_CAL_OFF, sizeof(chk));
        if (memcmp(&chk, &tmp, sizeof(chk)) != 0) { esc_cal_state = 2; return 1; }
    }
    esc_cal = tmp;
    esc_cal_state = 1;
    return 0;
}

/* SWD 可写命令：0 空闲 / 1 存默认 / 2 重载 / 3 存当前 */
volatile uint8_t esc_cal_cmd = 0;

/* ★ 自动校准入口（K2/K3 实现）：当前先填默认值，保证流程可跑通 */
void esc_cal_auto(void)
{
    esc_cal_defaults();
}

/* ================= K2a：静态 ADC 校准 ================= */
volatile uint8_t  cal_mode   = 0;
volatile uint8_t  cal_state  = 0;
volatile uint16_t cal_bus_mv = 12000;
volatile uint16_t cal_duty   = CAL_DUTY_DEF;
volatile uint16_t cal_duty_hi = CAL_DUTY_HI;
static   uint16_t cal_ccr4_save = 0;   /* 校准前 TIM1->CCR4 的值，结束要还回去 */

/* ★ K3：rank 偏差校准
 *   ⚠ 必须【三个驱动状态各测一遍】：step1 只有 A 是 PWM 相，B 被低边钳到 0、C 悬空，
 *     它们没有真正的"两电平"，算出来的 k 是假的（第一版就踩了这个）。
 *     所以让 A/B/C 轮流当 PWM 相：sel0=step1(A PWM)、sel1=step2(C PWM)、sel2=step4(B PWM)。 */
volatile uint16_t cal_rank[3][4][2][4] = {{{{0}}}};  /* [sel][order][0=低/1=高][VA,VB,VC,V0] */
volatile uint8_t  cal_rank_prog = 0;   /* 进度：已完成的驻留期数(0..24) */
volatile uint8_t  cal_run       = 0;   /* K4 自驱动校准跑：0空闲/1请求/2跑中 */
/* ★ K4 调试面包屑：跑中→停 的那一刻把现场录下来（事后读，避免 SWD 采样不到毫秒级）
 *   [0]newinput [1]input [2]armed [3]signaltimeout [4]ol_startup_stage
 *   [5]ol_startup_arm [6]duty_cycle [7]actual_current  */
volatile uint16_t cal_dbg[8] = {0};
/* ★ K4 追踪：olStartupTick() 的 newinput<48 分支（唯一不写 ol_stop_site 的停机路径）
 *   [0]=0xBEEF 标记 [1]=当时的 newinput [2]=当时的 cal_run [3]=当时的 input */
volatile uint16_t cal_trip[4] = {0};
static const uint8_t rk_step[3] = { 1u, 2u, 3u };
static   uint8_t  rk_sel = 0;
static   uint8_t  rk_ord = 0;
static   uint8_t  rk_hi  = 0;
extern volatile uint8_t adc_order_pending; /* ADC-TC 中断里生效的 rank 顺序请求 */
volatile uint16_t cal_raw[6][4] = {{0}};
volatile uint16_t cal_raw2[6][4] = {{0}};

static uint8_t  cal_step   = 0;
static uint16_t cal_tick   = 0;
static uint32_t cal_acc[4] = {0,0,0,0};
static uint16_t cal_n      = 0;

extern void comStep(int);
extern void setPWMCompare1(uint16_t);
extern void setPWMCompare2(uint16_t);
extern void setPWMCompare3(uint16_t);
extern uint16_t bemf_adc_va, bemf_adc_vb, bemf_adc_vc, bemf_adc_v0_hw;

/* ★ K2a-5：把"下发一个驱动状态"抽成函数。
 *   为什么必须抽出来：原来驱动只在【每个驻留期结束时】下发，
 *   于是【第一个驻留期全程没有任何驱动】也被采样了 —— 那一格是空闲底噪
 *   （实测：表里第 1 步低档 = 53/51/51/47，正是 allOff 状态的三相读数）。
 *   现在开局立刻下发第一个状态（step1/低档），每个状态都被采到，一格不废。 */
static void cal_drive(uint8_t step, uint8_t hi)
{
    uint16_t d = hi ? cal_duty_hi : cal_duty;   /* ★ K2a-4：两档各自独立，不再 duty×2 */

    /* ★ TIM1 主输出使能（BDTR.MOE）+ 计数器运行：校准是 SWD 触发的旁路路径，
     *   MOE=0 时 CCxE 全开也不出波形（三相恒 ~0 码）。每步兜一道，幂等。 */
    LL_TIM_EnableCounter(TIM1);
    LL_TIM_EnableAllOutputs(TIM1);

    /* ★ 先把三路 CCR 全清零，再 comStep() 切引脚：
     *   comStep() 会把"高侧相"引脚从 GPIO 切回 AF；若此刻 CCR 还留着上一步的值，
     *   切回去的瞬间就会按残留占空比开通（可能撞上另一个还在导通的相）。 */
    setPWMCompare1(0);
    setPWMCompare2(0);
    setPWMCompare3(0);

    comStep((int)step);   /* 相 → 引脚模式：高侧 AF、低侧 GPIO 高(on)、悬空相两脚 GPIO 低 */

    /* ★★★ 相 → TIM1 通道映射（本板）★★★
     *   依据 Inc/targets.h 的 HARDWARE_GROUP_G0_RM2027 与 Src/main.c 的"六路门极"注释：
     *       A 相 = CH3 / CH3N → PA10 / PB1 → 【CCR3】
     *       B 相 = CH2 / CH2N → PA9  / PB0 → 【CCR2】
     *       C 相 = CH1 / CH1N → PA8  / PA7 → 【CCR1】
     *   ⚠ 别写成"A→CCR1"——CCR1 是 C 相。K2a-2 就是按 A→CCR1/C→CCR3 写的，后果是：
     *     该步"高侧相"的引脚虽然在 AF 模式，但它的 CCR 恒为 0 ⇒ 高侧一直关、
     *     互补低侧一直开，三相都量不到电压（实测 0~2 码）。
     *     —— 这不是 MOE/引脚 AF 的问题（实测 BDTR.MOE=1、AFRH=0x222 一直是对的）。
     *   step→高侧相 由 comStep() 定：1/6=A、2/3=C、4/5=B。 */
    switch (step) {
    case 1: case 6: setPWMCompare3(d); break;   /* 高侧 PWM = A 相 = CH3 */
    case 2: case 3: setPWMCompare1(d); break;   /* 高侧 PWM = C 相 = CH1 */
    default:        setPWMCompare2(d); break;   /* 高侧 PWM = B 相 = CH2 */
    }
}

void esc_cal_poll(void)
{

    /* ★ K2a-3 ⓪：cal_mode 的 0→1 上升沿 = 新一轮校准开始 ⇒ 必须复位状态机。
     *   原来 cal_step / cal_tick / cal_acc / cal_n 都是 static 且【从不清零】，
     *   于是第二次、第三次启动校准（PC 脚本每次都会先 poke cal_state=0, cal_mode=1）
     *   一进来 cal_step 就已经 ≥12 ⇒ 立刻"完成"：既不驱动、也不采样，
     *   cal_raw/cal_raw2 里全是上一轮的残留值 —— 看上去"秒完成 + 数据不变"。
     *   首次上电跑（cal_step 初始 0）才会真的跑一轮，所以这个 bug 只在"重跑"时暴露。 */
    static uint8_t cal_last_mode = 0;
    if (cal_mode != cal_last_mode) {
        cal_last_mode = cal_mode;
        if (cal_mode == CAL_MODE_ADC || cal_mode == CAL_MODE_RANK) {
            /* ---- 两个模式共用的进入动作 ---- */
            cal_tick = 0;
            cal_n    = 0;
            cal_acc[0] = cal_acc[1] = cal_acc[2] = cal_acc[3] = 0;
            cal_state  = 0;
            LL_TIM_EnableCounter(TIM1);
            LL_TIM_EnableAllOutputs(TIM1);
            /* ★ K2a-4：换上校准专用触发点（跑完必须还原）。
             *   默认 ADC_TRIGGER_CCR4 是按"正常运行占空比（~17%）"调的；
             *   校准高档导通窗短得多，必须把触发点提前，否则 rank3/4（VC/V0）落在关断窗。*/
            cal_ccr4_save = (uint16_t)(TIM1->CCR4 & 0xFFFFu);
            TIM1->CCR4    = CAL_CCR4;

            if (cal_mode == CAL_MODE_ADC) {
                cal_step = 0;
                /* ★ K2a-5：开局就下发第一个状态，否则第一个驻留期采到的是空闲底噪 */
                cal_drive(1u, 0u);
            } else {
                /* ★ K3：rank 偏差 —— 三个驱动状态 × 4 order × 2 档 = 24 个驻留期。
                 *   每个驱动状态内信号真值恒定 ⇒ 同一通道跨 4 个 order 的差
                 *   【纯粹是 rank 效应】，不掺负载角/转速。 */
                rk_sel = 0;
                rk_ord = 0;
                rk_hi  = 0;
                cal_rank_prog = 0;
                adc_order_pending = 0;
                cal_drive(rk_step[0], 0u);  /* 从 sel0/order0/低档开始 */
            }
        }
    }
    /* ★★ K3：rank 偏差校准状态机（不转电机，逐个 order 驻留平均）★★
     *   每个 order 把四个通道摆到不同的 rank 上；跑完 4 个 order，每个通道
     *   恰好把 rank1~4 各占一次 ⇒ cal_rank[order][ch] 就是"ch 在某个 rank 上的读数"。 */
    if (cal_mode == CAL_MODE_RANK) {
        if (cal_tick < CAL_RANK_DWELL) {
            cal_tick++;
            if (cal_tick > 60u) {      /* 前 60 拍丢掉：等 order 切换在中断里生效 */
                cal_acc[0] += bemf_adc_va; cal_acc[1] += bemf_adc_vb;
                cal_acc[2] += bemf_adc_vc; cal_acc[3] += bemf_adc_v0_hw;
                cal_n++;
            }
            return;
        }
        if (cal_n) {
            for (uint8_t j = 0; j < 4u; j++) {
                cal_rank[rk_sel][rk_ord][rk_hi][j] = (uint16_t)(cal_acc[j] / cal_n);
            }
            cal_rank_prog++;
        }
        cal_acc[0]=cal_acc[1]=cal_acc[2]=cal_acc[3]=0; cal_n=0; cal_tick=0;
        /* 推进顺序：同 sel 内先切低/高档，一档采完换 order，4 个 order 采完换 sel */
        rk_hi++;
        if (rk_hi >= 2u) {
            rk_hi = 0;
            rk_ord++;
            if (rk_ord >= 4u) {
                rk_ord = 0;
                rk_sel++;
                if (rk_sel >= 3u) {          /* 3 sel × 4 order × 2 档 = 24，采完 */
                    adc_order_pending = 0;   /* 还原 order0 */
                    TIM1->CCR4 = cal_ccr4_save; /* 还原正常采样触发点 */
                    cal_mode = 0; cal_state = 1u;
                    return;
                }
                adc_order_pending = 0;       /* 换 sel：order 回到 0 */
            } else {
                adc_order_pending = rk_ord;  /* 换 order：交给 ADC-TC 中断下一帧生效 */
            }
        }
        cal_drive(rk_step[rk_sel], rk_hi);   /* 保持 sel 的换相状态，只切档 */
        return;                              /* 真值只随"档"变，与 rank 无关 */
    }

    if (cal_mode != CAL_MODE_ADC) { return; }
    if (cal_state == 1u) { return; }

    /* cal_step 0..11 = 6 个换相状态 × 2 档占空比（低档/高档）*/
    if (cal_tick < 240u) {
        cal_tick++;
        if (cal_tick > 60u) {
            cal_acc[0] += bemf_adc_va; cal_acc[1] += bemf_adc_vb;
            cal_acc[2] += bemf_adc_vc; cal_acc[3] += bemf_adc_v0_hw;
            cal_n++;
        }
        return;
    }
    if (cal_n) {
        uint16_t v[4] = { (uint16_t)(cal_acc[0]/cal_n), (uint16_t)(cal_acc[1]/cal_n),
                          (uint16_t)(cal_acc[2]/cal_n), (uint16_t)(cal_acc[3]/cal_n) };
        uint8_t ph = (uint8_t)(cal_step >> 1);
        uint8_t hi = (uint8_t)(cal_step & 1u);
        for (uint8_t j = 0; j < 4u; j++) {
            if (hi) { cal_raw2[ph][j] = v[j]; } else { cal_raw[ph][j] = v[j]; }
        }
    }
    cal_acc[0]=cal_acc[1]=cal_acc[2]=cal_acc[3]=0; cal_n=0; cal_tick=0;
    cal_step++;
    if (cal_step >= 12u) {
        TIM1->CCR4 = cal_ccr4_save;   /* ★ K2a-4：还原正常采样触发点 */
        cal_mode = 0; cal_state = 1u; return;
    }

    /* 切到下一个状态（step = 相1..6，hi = 档）*/
    cal_drive((uint8_t)((cal_step >> 1) + 1u), (uint8_t)(cal_step & 1u));
}

/* ================= K5：rank 增益自测（片上，用户要求）=================
 * 事实（实跑实测）：采样槽位(rank)对"幅值"的影响是 rank1=0.670 → rank4=1.441（2.15×），
 *   而现有 RM2027_SG_NUM 那张 3 周期增益表就是它换了个写法
 *   （按 rank 因子算 A=1.52/B=1.16，表里是 1.41/1.15 —— 只差 8%/1%）。
 * 做法（全部在片上，每帧只多 2 次比较）：
 *   · 每个 ADC 帧把 (悬空相 − V0) 的 min/max 记进 [step][order] 格；pp=max−min 即该格幅值；
 *   · 跑完一轮：每步用 4 个 order 的均值归一 ⇒ 得到该格所在 rank 的相对因子 k[rank]；
 *   · 六步增益 g[s] = k[rank4] / k[rank(悬空相在 order0 的槽位)]，再归一（最小=1000）。
 *     依据：order0 下 悬空相 A/B/C 分别在 rank1/2/3，而参考 V0 恒在 rank4。
 *   ⚠ 这是"增益"；每步残留的"偏置"归六步 offset（RM2027_SO_OFF）单独管 —— 分工不混。
 */
/* ★★ K6：运行期可改的六步表 ★★
 *   初值 = 编译期宏（= 现状，能正常跑）；校准跑完后由 rk_compute() 覆盖。
 *   判据(adc_bemf.c) 读这两个 RAM 表 ⇒ 校准结果立刻生效，不需要重编译。
 *   ⚠ 增益管 rank 的 3 周期（A最大/B中/C基准）；偏置管每步残留 DC —— 分工不混。 */
volatile int16_t  cal_sg[7] = RM2027_SG_NUM;
volatile int16_t  cal_so[7] = RM2027_SO_OFF;
volatile int16_t  rk_so[7];
volatile int32_t  rk_sum[7][4];
volatile uint32_t rk_sq[7][4];   /* Σ(dv>>2)² —— 稳健幅值用（抗尖峰）*/
volatile uint16_t rk_n[7][4];
volatile uint8_t  cal_tables_ready = 0;
/* ★ 1=把算出的表应用到判据；0=保持编译期宏（= V2.9 行为）。默认 0 更安全。*/
volatile uint8_t  cal_apply = 0;
/* ★ K15：按需触发计算。为什么需要：电机卡顿时 running 反复通断，
 *   ol_run_ticks 被反复清零 ⇒ OL_RUN_LIMIT 永远到不了 ⇒ 状态机的 3→0 不收尾
 *   ⇒ rk_compute() 永远不跑（实测 f_cnt 都满了、fit_ready 还是 0）。
 *   PC 侧 poke 本变量为 1 即可立刻算一次。*/
volatile uint8_t  cal_compute_req = 0;

/* ★★ K12：按 V2.9 口径在【rank 修正后】的数据上重拟合 ★★
 *   V2.9 的做法：每步中段直线拟合 → 外推到步两端 e0_s/e1_s
 *     A_s = e1−e0（幅值）、m_s = (e0+e1)/2（中点）
 *     g_s = meanA/|A_s|（幅值归一）、o_s = −g_s·m_s（过零居中）
 *   这里的关键改动：拟合用的不是原始 d，而是 d×1000/K_rank[该帧所在槽位]
 *     ⇒ 抠掉 rank 槽位差，剩下的起伏才是真实相间不对称（= 需重标部分）。
 *   累加量：Σt, Σt², Σd, Σt·d（t = 步内采样序号），跑完解最小二乘。
 */
volatile int32_t  fit_n[7];
volatile int64_t  fit_t[7], fit_tt[7], fit_td[7];
volatile int32_t  fit_d[7];
volatile int16_t  fit_g[7], fit_o[7];     /* 重拟合出来的增益/偏置（诊断+候选）*/
volatile int32_t  fit_A[7], fit_M[7];     /* 拟合出的幅值 A_s / 中点 M_s（诊断真值）*/
/* ★ K13：单次换相步的长度统计（拟合把"每步实例"混在一起算，A=e1−e0 必须用
 *   【单实例长度】而不是总样本数 —— 否则幅值被放大几十倍，fit_A 会撞到上万码）*/
volatile int32_t  fit_Lsum[7];
volatile uint16_t fit_Lcnt[7];
volatile uint8_t  fit_ready = 0;
static   uint16_t fit_tc = 0;             /* 当前步内序号 */

/* ★★ K7：rank 增益改成【4 路通道各自的增益】★★
 *   模型：pp(步,order) = G_相 × K_rank × S_步
 *   每个通道在 4 个 order 下轮流占 rank1~4 ⇒ 对 order 取平均就把 K_rank 消掉，
 *   剩下正比于 G_相 × S_步。三相在同一转速下 S 相同 ⇒ 比值即通道增益。
 *   V0 的增益用恒等式定标：V0 是三相星形中点 ⇒ G_V0·v0 ≈ (G_A·va+G_B·vb+G_C·vc)/3。
 */
volatile int16_t  ch_gain[4];          /* [A,B,C,V0] ×1000 —— 4 路通道增益 */
volatile int16_t  rk_k[5];             /* [1..4] rank 因子 ×1000（rank1..rank4）*/
volatile int32_t  rk_csum[4];          /* 通道读数和（A,B,C,V0）*/
volatile uint32_t rk_cn;

static uint8_t rk_rank_of(uint8_t st, uint8_t o);   /* K12: 前置声明 */

/* ★★ K14：单实例 + 中段裁剪 + MAD 剔除（学 V2.9 的 u∈[0.15,0.95]，并补上抗离群）★★
 *   为什么必须按实例：跨实例把几百条斜坡混在一起拟合，实例间的整体漂移会把斜率稀释
 *   （实测 fit_A 只有 49 码，真实幅值是几百码）。
 *   为什么必须裁两端：换相后第 1 点有 ±200~600 码尖峰（仓库实测记录），末尾靠近下次换相也有振铃。
 *   为什么要 MAD：只裁两端挡不住偶发尖峰，一个 ±600 就能把均值拉歪。
 */
#define RK_INST_MAX  96
static int16_t inst_buf[RK_INST_MAX];
static uint8_t inst_n  = 0;
static uint8_t inst_st = 0;
volatile int32_t  fA_sum[7], fM_sum[7];
volatile uint16_t f_cnt[7];

static void rk_inst_close(void)
{
    uint8_t n = inst_n; uint8_t st = inst_st;
    inst_n = 0;
    if (st < 1u || st > 6u || n < 12u) { return; }
    uint8_t i0 = (uint8_t)((uint16_t)n * 15u / 100u);
    uint8_t i1 = (uint8_t)((uint16_t)n * 95u / 100u);
    if (i1 <= i0 + 3u) { return; }
    uint8_t m = (uint8_t)(i1 - i0);
    int16_t tmp[RK_INST_MAX];
    uint8_t i, j;
    for (i = 0; i < m; i++) { tmp[i] = inst_buf[i0 + i]; }
    for (i = 1; i < m; i++) { int16_t v = tmp[i]; j = i; while (j > 0u && tmp[j-1] > v) { tmp[j] = tmp[j-1]; j--; } tmp[j] = v; }
    int16_t med = tmp[m / 2u];
    int16_t dev[RK_INST_MAX];
    for (i = 0; i < m; i++) { int16_t x = (int16_t)(inst_buf[i0+i] - med); dev[i] = (x < 0) ? (int16_t)(-x) : x; }
    for (i = 1; i < m; i++) { int16_t v = dev[i]; j = i; while (j > 0u && dev[j-1] > v) { dev[j] = dev[j-1]; j--; } dev[j] = v; }
    int16_t mad = dev[m / 2u];
    int16_t thr = (int16_t)(mad * 3);
    if (thr < 8) { thr = 8; }
    uint8_t a = (uint8_t)(m / 3u), b = (uint8_t)(m - m / 3u);
    int32_t s1 = 0, s3 = 0; uint8_t c1 = 0, c3 = 0;
    for (i = 0; i < m; i++) {
        int16_t x = inst_buf[i0 + i];
        int16_t dv2 = (int16_t)(x - med); if (dv2 < 0) { dv2 = (int16_t)(-dv2); }
        if (dv2 > thr) { continue; }
        if (i < a)          { s1 += x; c1++; }
        else if (i >= b)    { s3 += x; c3++; }
    }
    if (c1 && c3) {
        int32_t A = s3 / (int32_t)c3 - s1 / (int32_t)c1;
        if (A < 0) { A = -A; }
        fA_sum[st] += A;
        fM_sum[st] += med;
        if (f_cnt[st] < 60000u) { f_cnt[st]++; }
    }
}
volatile int16_t  rk_min[7][4];
volatile int16_t  rk_max[7][4];
volatile uint16_t rk_pp[7][4];
volatile uint16_t rk_gain[7];
volatile uint8_t  rk_arm  = 0;
volatile uint8_t  rk_done = 0;

void rk_reset(void)
{
    for (uint8_t s = 0; s < 7u; s++) {
        rk_gain[s] = 1000u;
        for (uint8_t o = 0; o < 4u; o++) {
            rk_min[s][o] = 32767; rk_max[s][o] = -32768; rk_pp[s][o] = 0u;
            rk_sum[s][o] = 0; rk_sq[s][o] = 0u; rk_n[s][o] = 0u;
        }
    }
    rk_done = 0;
    rk_csum[0]=rk_csum[1]=rk_csum[2]=rk_csum[3]=0; rk_cn=0;
    for (uint8_t i=0;i<7u;i++){ fit_n[i]=0; fit_t[i]=0; fit_tt[i]=0; fit_td[i]=0; fit_d[i]=0; fit_g[i]=0; fit_o[i]=0; fit_Lsum[i]=0; fit_Lcnt[i]=0; fA_sum[i]=0; fM_sum[i]=0; f_cnt[i]=0; }
    inst_n=0; inst_st=0;
    fit_tc = 0; fit_ready = 0;
    for (uint8_t i=0;i<4u;i++) ch_gain[i]=1000;
}

void rk_sample(uint8_t st, uint8_t order, int16_t dv,
               uint16_t va, uint16_t vb, uint16_t vc, uint16_t v0)
{
    if (rk_arm == 0u || st < 1u || st > 6u) { return; }
    /* ★ 必须丢掉换相后的前几帧：换相尖峰（±200~600 码）会把 min/max 撑大，
     *   而尖峰大小随 order 变 ⇒ 会把 rank 图案带歪（第一版就是这么错的：
     *   片上算出 C 最大/A 最小，离线丢边界后是 A 最大，正好相反）。 */
    static uint8_t last_st = 0;
    static uint8_t skip = 0;
    if (st != last_st) {
        /* ★ 片上自己推进 adc_order：不能依赖 win_push()（那要 PC 在收窗口流才会跑），
         *   否则不接采集时 order 恒为 0 ⇒ 24 个格子只有 order0 有数据 ⇒ rank 因子全 1.0
         *   （实测踩过）。每电周期（step 回到 1）前进一格。 */
        if (st == 1u && rk_arm) {
            extern volatile uint8_t adc_order, adc_order_pending;
            adc_order_pending = (uint8_t)((adc_order + 1u) & 3u);
        }
        if (last_st >= 1u && last_st <= 6u && fit_tc > 0u) {   /* K13：记下上一实例的长度 */
            fit_Lsum[last_st] += (int32_t)fit_tc;
            if (fit_Lcnt[last_st] < 60000u) { fit_Lcnt[last_st]++; }
        }
        last_st = st; skip = RK_SKIP_AFTER_COMM;
        fit_tc  = 0;   /* K12：步内序号从 0 开始 */
    }
    if (skip) { skip--; return; }
    uint8_t o = (uint8_t)(order & 3u);
    if (dv < rk_min[st][o]) { rk_min[st][o] = dv; }   /* 仅留作参考/诊断 */
    if (dv > rk_max[st][o]) { rk_max[st][o] = dv; }
    /* ★ 幅值用【方差】而不是 min/max：换相尖峰会把 max 撑大、把 B/C 只有 6% 的差异翻掉
     *   （实测两轮 B/C 相对大小翻转）。方差对少量尖峰不敏感 ⇒ 稳。
     *   dv>>2 缩放到 ±~500 后平方，配合 n 上限 4096 避免 uint32 溢出。*/
    if (rk_n[st][o] < 4096u) {
        int32_t x = dv >> 2;
        rk_sum[st][o] += x;
        rk_sq[st][o]  += (uint32_t)(x * x);
        rk_n[st][o]++;
    }
    rk_csum[0]+=(int32_t)va; rk_csum[1]+=(int32_t)vb;   /* K7：通道增益用 */
    rk_csum[2]+=(int32_t)vc; rk_csum[3]+=(int32_t)v0; rk_cn++;
    /* ★ K14：单实例收集（rank 修正后），实例结束时按"中段 15%~95% + MAD 剔除 + 中位"处理 */
    {   extern volatile uint8_t ol_startup_arm;
        if (ol_startup_arm == 0u) { return; }      /* 只取开环段 */
        if (st != inst_st) {                       /* 上一实例结束 ⇒ 收尾 */
            rk_inst_close();
            inst_st = st; inst_n = 0;
        }
        if (inst_n < RK_INST_MAX) {
            uint8_t r = rk_rank_of(st, o);
            int32_t kr = (r >= 1u && r <= 4u && rk_k[r] > 0) ? (int32_t)rk_k[r] : 1000;
            int32_t dc = (int32_t)(((int64_t)dv * 1000) / kr);
            if (dc >  32000) { dc =  32000; }
            if (dc < -32000) { dc = -32000; }
            inst_buf[inst_n++] = (int16_t)dc;
        }
    }
}

/* order o 下，步 st 的悬空相占哪个 rank（1..4）*/
static uint8_t rk_rank_of(uint8_t st, uint8_t o)
{
    /* 悬空相: 1/4=C 2/5=A 3/6=B ；RANK_OF: A=[1,4,3,2] B=[2,1,4,3] C=[3,2,1,4] */
    static const uint8_t A_[4] = {1u,4u,3u,2u};
    static const uint8_t B_[4] = {2u,1u,4u,3u};
    static const uint8_t C_[4] = {3u,2u,1u,4u};
    const uint8_t *t = (st == 2u || st == 5u) ? A_ : ((st == 3u || st == 6u) ? B_ : C_);
    return t[o & 3u];
}

void rk_compute(void)
{
    uint32_t rsum[5] = {0,0,0,0,0};
    uint16_t rcnt[5] = {0,0,0,0,0};
    uint32_t k[5]    = {0,0,0,0,0};      /* rank 因子 ×1000 */
    uint32_t graw[7];
    uint32_t gmin;

    for (uint8_t st = 1u; st <= 6u; st++) {
        uint32_t sum = 0u; uint8_t n = 0u;
        for (uint8_t o = 0; o < 4u; o++) {
            /* 幅值 ∝ sqrt(var)，var = E[x²] − E[x]²（x = dv>>2）*/
            uint16_t pp = 0u;
            if (rk_n[st][o] >= 32u) {
                int32_t n2 = (int32_t)rk_n[st][o];
                int32_t mean = rk_sum[st][o] / n2;
                int32_t var  = (int32_t)(rk_sq[st][o] / (uint32_t)n2) - mean * mean;
                if (var > 0) {
                    /* 整数 sqrt（牛顿迭代，够用）*/
                    uint32_t r = (uint32_t)var, x = r, y = (x + 1u) / 2u;
                    while (y < x) { x = y; y = (x + r / x) / 2u; }
                    pp = (uint16_t)x;
                }
            }
            rk_pp[st][o] = pp;
            if (pp > 0u) { sum += pp; n++; }
        }
        if (n < 2u || sum == 0u) { continue; }
        uint32_t mu = sum / n;                        /* 该步 4 序的均值 */
        for (uint8_t o = 0; o < 4u; o++) {
            if (rk_pp[st][o] == 0u) { continue; }
            uint32_t khat = (uint32_t)rk_pp[st][o] * 1000u / mu;   /* 相对因子 ×1000 */
            uint8_t r = rk_rank_of(st, o);
            if (r >= 1u && r <= 4u) { rsum[r] += khat; rcnt[r]++; }
        }
    }
    for (uint8_t r = 1u; r <= 4u; r++) { k[r] = rcnt[r] ? (rsum[r] / rcnt[r]) : 1000u; }
    for (uint8_t r = 1u; r <= 4u; r++) { rk_k[r] = (int16_t)((k[r] > 32000u) ? 32000u : k[r]); }
    if (k[4] == 0u) { k[4] = 1000u; }                 /* V0 在 rank4 */

    graw[0] = 1000u;
    gmin = 0xFFFFFFFFu;
    for (uint8_t st = 1u; st <= 6u; st++) {
        uint8_t rf = rk_rank_of(st, 0u);              /* 运行用 order0 */
        uint32_t kf = (k[rf] == 0u) ? 1000u : k[rf];
        graw[st] = (uint32_t)k[4] * 1000u / kf;       /* 修正系数（V0/悬空相）*/
        if (graw[st] < gmin) { gmin = graw[st]; }
    }
    if (gmin == 0u || gmin == 0xFFFFFFFFu) { return; }
    for (uint8_t st = 1u; st <= 6u; st++) {
        uint32_t g = graw[st] * 1000u / gmin;         /* 归一：最小 = 1000 */
        rk_gain[st] = (g > 65535u) ? 65535u : (uint16_t)g;
    }
    /* ================= K7：先求【4 路通道各自的增益】，再推每步乘数 =================
     * 模型：pp(步,order) = G_相 × K_rank × S_步
     *   · 每个通道在 4 个 order 下轮流占 rank1~4 ⇒ 对 order 取平均把 K_rank 消掉；
     *   · 三相在同一转速下幅值相同 ⇒ 三者之比就是【通道增益】；
     *   · V0 是三相星形中点 ⇒ G_V0·v0 ≈ (G_A·va+G_B·vb+G_C·vc)/3 定标。
     * 然后判据仍用"每步一个乘数"的接口：sg[step] = G_该步悬空相（×1000）。
     */
    {
        int32_t amp[3] = {0,0,0};
        uint16_t cnt[3] = {0,0,0};
        for (uint8_t st = 1u; st <= 6u; st++) {
            uint8_t f = (st == 2u || st == 5u) ? 0u : ((st == 3u || st == 6u) ? 1u : 2u);
            for (uint8_t o = 0; o < 4u; o++) {
                if (rk_pp[st][o] == 0u) { continue; }
                uint8_t r = rk_rank_of(st, o);
                uint32_t kr = (r <= 4u && k[r] != 0u) ? k[r] : 1000u;
                amp[f] += (int32_t)((uint32_t)rk_pp[st][o] * 1000u / kr);   /* 去 rank 后的幅值 */
                cnt[f]++;
            }
        }
        int32_t m3 = 0; uint8_t n3 = 0;
        for (uint8_t i = 0; i < 3u; i++) { if (cnt[i]) { amp[i] /= (int32_t)cnt[i]; m3 += amp[i]; n3++; } }
        if (n3 == 0u || m3 == 0) { rk_done = 1u; return; }
        /* ★ 通道增益【一律当 1，不校】：
         *   实测通道间只有 ±0.9%（静态两点法，落在 deadband 内），
         *   把它们算成 1000/1100 这类值是估计噪声，乘进去会污染后面的六步不对称。
         *   这里只保留 rank 因子（rk_k）作为真正的校准量。*/
        (void)amp;
        ch_gain[0] = 1000; ch_gain[1] = 1000; ch_gain[2] = 1000; ch_gain[3] = 1000;
        /* V0 增益：G_V0 = (G_A·mva + G_B·mvb + G_C·mvc) / (3·mv0) */
        /* V0 同样不校（与三相同增益）—— 通道一致性的问题归静态校准，不在这里做 */

        /* ---- 第二步：用通道增益归一后的数据，算每步偏置 ---- */
        for (uint8_t st = 1u; st <= 6u; st++) {
            uint8_t f = (st == 2u || st == 5u) ? 0u : ((st == 3u || st == 6u) ? 1u : 2u);
            (void)f;   /* 通道增益按 1 处理 ⇒ 偏置不依赖相 */
            /* ★ 只用 rank 因子（通道增益按 1 处理）
             *   sg = K_rank4 / K_rank[该步悬空相在 order0 占的槽位] */
            uint8_t rf = rk_rank_of(st, 0u);
            int32_t kf = (rf <= 4u && k[rf] != 0u) ? (int32_t)k[rf] : 1000;
            int32_t kv = (k[4] != 0u) ? (int32_t)k[4] : 1000;
            int32_t g  = (kv * 1000) / kf;                 /* 修正乘数 ×1000 */
            if (g < 500)  { g = 500; }
            if (g > 4000) { g = 4000; }
            rk_gain[st] = (int16_t)g;
            if (cal_apply) { cal_sg[st] = (int16_t)g; }
            int32_t so = 0;
            if (rk_n[st][0] > 0u) {
                int32_t mraw = rk_sum[st][0] / (int32_t)rk_n[st][0];   /* 原始 d 的均值（order0）*/
                so = -(int32_t)((mraw * g) / 1000);        /* 归一后应归零 ⇒ 反向补偿 */
                if (so >  4095) { so =  4095; }
                if (so < -4095) { so = -4095; }
            }
            rk_so[st] = (int16_t)so;                       /* 只记录，供诊断 */
            /* ★ 不应用 offset：V2.9 的 SO_OFF 就是全 0（它靠"过零居中"拟合出来的值也没启用）。
             *   实测：应用我这版 offset（整步均值口径）后转速在 4800~15000 来回跳 = 卡顿
             *   ⇒ gain 只改幅值、不移动过零点；offset 会直接移动过零点，六步各加不同值
             *     = 给每步注入不同定时偏差。先学 V2.9：只用 gain、offset 保持 0。*/
            if (cal_apply) { cal_so[st] = 0; }
        }
        /* ★★ K12：V2.9 口径重拟合（数据已按 rank 因子修正过）★★
         *   每步 min² 拟合 → e0=b、e1=b+k(n−1) → A=|e1−e0|、M=(e0+e1)/2
         *   g_s = meanA/A_s ； o_s = −M_s·g_s/1000   （与 cal-anchor.py 同口径）*/
        {
            int32_t As[7] = {0}, Ms[7] = {0};
            int32_t sumA = 0; uint8_t nA = 0;
            for (uint8_t st = 1u; st <= 6u; st++) {
                if (f_cnt[st] == 0u) { continue; }
                int32_t A = fA_sum[st] / (int32_t)f_cnt[st];   /* 实例级 A 的平均 */
                int32_t M = fM_sum[st] / (int32_t)f_cnt[st];   /* 实例级中位的平均 */
                As[st] = A; Ms[st] = M;
                fit_A[st] = A; fit_M[st] = M;                  /* 诊断真值 */
                if (A > 0) { sumA += A; nA++; }
            }
            if (nA > 0u) {
                int32_t mA = sumA / (int32_t)nA;
                for (uint8_t st = 1u; st <= 6u; st++) {
                    if (As[st] <= 0) { fit_g[st] = 1000; fit_o[st] = 0; continue; }
                    int32_t g = mA * 1000 / As[st];
                    if (g < 500)  { g = 500; }
                    if (g > 4000) { g = 4000; }
                    int32_t o = -(Ms[st] * g) / 1000;
                    if (o >  4095) { o =  4095; }
                    if (o < -4095) { o = -4095; }
                    fit_g[st] = (int16_t)g;
                    fit_o[st] = (int16_t)o;
                }
                fit_ready = 1u;
                /* ★★ 合成最终运行时表：rank(可移植) × fit_g(残余) ★★
                 *   cal_sg 此刻存的是 rank 那一层（K10 算的 kv*1000/kf），
                 *   乘上残余增益才是"对原始 d 的完整修正"；偏置直接用 fit_o。*/
                /* ★ 只有拟合真的出结果（fit_ready）才应用；否则保持原表（V2.9 宏）。
                 *   事故：fit_ready=0 时也应用 ⇒ 把 cal_sg 覆盖成全 1000（等于不做补偿）。*/
                if (cal_apply && fit_ready) {
                    for (uint8_t st = 1u; st <= 6u; st++) {
                        int32_t gtot = ((int32_t)cal_sg[st] * (int32_t)fit_g[st]) / 1000;
                        if (gtot < 500)  { gtot = 500; }
                        if (gtot > 4000) { gtot = 4000; }
                        cal_sg[st] = (int16_t)gtot;
                        cal_so[st] = fit_o[st];
                    }
                    cal_tables_ready = 1u;
                    /* ★★ flash 持久化（保存侧）：把结果写回最后一页 ★★
                     *   开机 esc_cal_load() 会读回来并应用 ⇒ 断电/重烧都不丢。
                     *   sg/so = 判据用的两张表；ch_g = rank 因子（留作溯源）。*/
                    /* ★ 合理性闸门：不合格的结果【绝不写 flash】（否则会把好表永久冲掉）。
                     *   判据：① 6 个增益都在 [700,3500]；② 不能全等于 1000（退化）。
                     *   事故：一次没采到数据的校准把全 1000 写进 flash，开机又被加载应用
                     *   ⇒ 增益补偿被彻底关掉。*/
                    {
                        uint8_t ok = 1u, diff = 0u;
                        for (uint8_t i = 1u; i <= 6u; i++) {
                            if (cal_sg[i] < 700 || cal_sg[i] > 3500) { ok = 0u; }
                            if (cal_sg[i] != 1000) { diff = 1u; }
                        }
                        if (ok && diff) {
                            for (uint8_t i = 1u; i <= 6u; i++) { esc_cal.sg[i] = cal_sg[i]; esc_cal.so[i] = cal_so[i]; }
                            for (uint8_t i = 0u; i < 4u; i++)  { esc_cal.ch_g[i] = rk_k[i + 1u]; }
                            (void)esc_cal_save(&esc_cal);
                        }
                    }
                }
            }
        }
        rk_so[0] = 0; rk_gain[0] = 0;
        if (cal_apply) { cal_sg[0] = 0; cal_so[0] = 0; }
        cal_tables_ready = 1u;
    }
    rk_done = 1u;
}

/* ★★★ 2026-10-20 硬保险：cal_sg 永远不许出现越界值 ★★★
 *   事故：flash 加载路径把一张【全 0】的表当有效表应用 ⇒ cal_sg 全 0
 *   ⇒ 判据 d ≡ 0 ⇒ 认不到过零 ⇒ 永不交棒 ⇒ 一直开环（油门不改变转速）。
 *   这里做最后一道闸：只要有一个值不在 [700,3500]，整张表回落到编译期宏。
 *   调用点：开机加载之后、rk_compute 应用之后、以及主循环里（便宜，7 次比较）。
 */
void cal_sg_guard(void)
{
    static const int16_t dflt[7] = RM2027_SG_NUM;
    for (uint8_t i = 1u; i <= 6u; i++) {
        if (cal_sg[i] < 700 || cal_sg[i] > 3500) {
            for (uint8_t j = 0; j < 7u; j++) { cal_sg[j] = dflt[j]; cal_so[j] = 0; }
            cal_tables_ready = 0u;
            return;
        }
    }
}
