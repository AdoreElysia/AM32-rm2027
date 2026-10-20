#ifndef ESC_CAL_H
#define ESC_CAL_H
#include <stdint.h>

/* ★ 校准开关（宏，不依赖 SWD）*/
#ifndef XIEN_CAL_ENABLE
#define XIEN_CAL_ENABLE  1u   /* 1 = 编入校准功能；0 = 纯读 flash/默认 */
#endif
#ifndef XIEN_CAL_FORCE
#define XIEN_CAL_FORCE   0u   /* 1 = 开��强制重新校准（忽略 flash 里的旧值）*/
#endif

#define ESC_CAL_MAGIC 0x4C414345u   /* "ECAL" */
#define ESC_CAL_VER   1u
#define ESC_CAL_ADDR  0x0801F800u   /* flash 最后一页（2KB，与 EEPROM 同页）*/
#define ESC_CAL_OFF   0x100u        /* 校准区从页内 256 字节开始（EEPROM 占 0..191）*/

typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t flags;
    int16_t  ch_g[4];   /* A/B/C/V0 端到端增益 ×1000（含 rank 因子）*/
    int16_t  ch_o[4];   /* 通道偏置（码）*/
    int16_t  sg[7];     /* 六步增益 ×1000 */
    int16_t  so[7];     /* 六步偏移（码）*/
    uint16_t wt[6];     /* 时间补偿 ‰ */
    uint16_t crc;       /* 前 sizeof-2 字节的 CRC16 */
} esc_cal_t;

extern esc_cal_t        esc_cal;
extern volatile uint8_t esc_cal_state;   /* 0=默认 1=flash 加载成功 2=写失败 */

void    esc_cal_defaults(void);
void    esc_cal_auto(void);   /* 自动校准（K2/K3 实现；当前=填默认）*/
void    esc_cal_load(void);
uint8_t esc_cal_save(const esc_cal_t *c);
#endif

/* ---- K2a：静态 ADC 校准（PC 脚本驱动）---- */
#define CAL_MODE_ADC   1u   /* 6 个静态换相状态，采 4 通道原始值 */

/* ★★★ K2a-4：两档 duty 的【角色】必须分清（2026-10-19，实测踩过）★★★
 *   采样落在【导通窗口】时节点电压就是 Vbus，与占空比无关；
 *   落在【关断窗口】时节点≈0（体二极管/低边钳位）。所以：
 *     · 低档 CAL_DUTY_LO —— 必须 **< CCR4**，让整轮扫描落在【关断窗口】⇒ 0V 参考点；
 *     · 高档 CAL_DUTY_HI —— 必须 **> CCR4 + 扫描时长**，让 rank1~4 全落在【导通窗口】⇒ ≈Vbus。
 *   ⚠ 千万不要"两档一起抬高"：两档都进导通窗口 ⇒ 两点读数都≈Vbus ⇒ 差分为 0 ⇒ 增益算不出来。
 *   ⚠ 扫描时长：四个电压通道在 rank1~4，rank4 约在触发后 80~180 拍才采完
 *     （实测：CCR4=160/duty=240 ⇒ 只剩 80 拍 ⇒ rank3(VC)/rank4(V0) 掉进关断窗口，
 *      表现为"C 相读数只有 A/B 的 1%、V0 恒 0"；把 CCR4 改到 60 立刻三相全对）。
 *     ⇒ 要满足 duty ≥ CCR4 + ~180。
 *   注：把 ADC 从 7 路缩到 4 路【不能】解决这个 —— 四个电压通道本来就是 rank1~4，
 *       砍掉 cur/bus/temp 只省总时长，rank3/4 该多晚还是多晚。 */
#define CAL_CCR4       100u  /* 校准专用 ADC 触发点（拍）；结束恢复 ADC_TRIGGER_CCR4 */
#define CAL_DUTY_LO     40u  /* 低档：< CAL_CCR4 ⇒ 采到"关断参考点"（≈0 码）*/
#define CAL_DUTY_HI    320u  /* 高档：> CAL_CCR4 + 180 ⇒ rank1~4 全在导通窗内（≈Vbus）*/
#define CAL_DUTY_DEF   CAL_DUTY_LO   /* 兼容旧名（脚本仍可 poke cal_duty 覆盖低档）*/

extern volatile uint8_t  cal_mode;      /* 0=空闲 1=ADC静态 */
extern volatile uint8_t  cal_state;     /* 0=空闲 1=完成 2=失败 */
extern volatile uint16_t cal_bus_mv;    /* PC 传入的实测母线电压(mV) */
extern volatile uint16_t cal_duty;      /* 低档占空比（0..2000）*/
extern volatile uint16_t cal_duty_hi;   /* 高档占空比（0..2000）*/
extern volatile uint16_t cal_raw[6][4];   /* 低档 duty（关断参考点）*/
extern volatile uint16_t cal_raw2[6][4];  /* 高档 duty（导通点）*/ /* [step-1][VA,VB,VC,V0] 中位 */
void esc_cal_poll(void);                /* 主循环调用 */

/* ================= K3：rank 偏差校准（不转电机）=================
 * 要解决什么：四个电压通道排在 rank1..4，各 rank 的采样时刻/建立不同
 *   ⇒【同一个物理通道】换个 rank 排位，读数就变。这就是历史上一改 ADC 顺序
 *   A/C 幅值就翻转（0.668 → 2.048）的那个效应。
 *
 * 为什么它必然表现为【六步 3 周期】误差：
 *   order0 下 悬空相 A=rank1、B=rank2、C=rank3，而参考 V0 恒在 rank4：
 *     步  1  2  3  4  5  6
 *     悬空相 C  A  B  C  A  B
 *     所在直 3  1  2  3  1  2
 *   ⇒ 若 R1≠R2≠R3，六步上就出现 1≡4、2≡5、3≡6 的图案。
 *   证据：现有 RM2027_SG_NUM = {0,1152,1629,1325,1152,1629,1325} 正是严格 3 周期！
 *   ⇒ 结论：那张"六步增益表"的 3 周期部分【就是 rank 偏差换了个写法】；
 *     做完 rank 校准后若 3 周期性消失，就说明不必再单独做增益校准，只留 offset。
 *
 * 怎么做：固定【一个】静态驱动状态（step1 高档：A=PWM/B=LOW/C=悬空），
 *   让 adc_order 依次 0/1/2/3 各驻留 CAL_RANK_DWELL 拍，每个 order 采 4 通道。
 *   这段时间里信号真值恒定 ⇒ 同一通道跨 4 个 order 的差【纯粹是 rank 效应】。
 *   注意：必须【不转电机】——转起来会有负载角/转速变化混进来。
 *   ⚠ 还要【两个电平】才能把每个 rank 的增益和偏置分开（单一个电平只能看到混合量）：
 *     低档(<CCR4，采样落在关断窗) ≈ 0V、高档(>CCR4+180，落在导通窗) ≈ Vbus。
 *   ⚠ 还要【三个驱动状态各测一遍】：step1 里只有 A 是 PWM 相，B 被低边钳到 0、C 悬空，
 *     它们没有真正的两电平，k 是假的（第一版踩过）。所以让 A/B/C 轮流当 PWM 相：
 *       sel0 = step1(A PWM)  sel1 = step2(C PWM)  sel2 = step4(B PWM)
 *     每个 sel 内扫 4 个 order × 低/高两档 ⇒ 3×4×2 = 24 个驻留期。
 *
 * 通道 ↔ rank 对照（由 ADC.c 的 adc_apply_order 决定）：
 *   order0: R1=A R2=B R3=C R4=V0      order2: R1=C R2=V0 R3=A R4=B
 *   order1: R1=B R2=C R3=V0 R4=A      order3: R1=V0 R2=A R3=B R4=C
 *   ⇒ A 相分别在 rank [1,4,3,2]（order0..3），B=[2,1,4,3]，C=[3,2,1,4]，V0=[4,3,2,1]。
 */
#define CAL_MODE_RANK   2u
#define CAL_RANK_DWELL  240u   /* 每个 (sel,order,档) 驻留多少主循环拍 */
extern volatile uint16_t cal_rank[3][4][2][4]; /* [sel][order][0=低/1=高][VA,VB,VC,V0] 均值 */
extern volatile uint8_t  cal_rank_prog;        /* 进度：已完成的驻留期数（0..24，24=完成）*/

/* ★★ K4：自驱动校准跑（不依赖 F1 给油）★★
 *   0 = 空闲/跑完   1 = 请求起转   2 = 跑中
 *   由 PC 侧 poke 成 1；固件自己调 startMotor() 走三段式开环，
 *   并打开 adc_order_auto 让 order 逐电周期轮转（窗口包自带 order 标记）。
 *   停机（到点/过流/失锁）后自动还原 adc_order_auto=0 并回到 0。 */
extern volatile uint8_t  cal_run;
extern volatile uint16_t cal_dbg[8];   /* K4 停机现场面包屑 */
extern volatile uint16_t cal_trip[4];  /* K4: newinput<48 停机路径现场 */

/* ★★ K5：rank 增益自测（片上）★★
 *   rk_pp[step][order] = 该格 (悬空相−V0) 的 pp=max−min
 *   rk_gain[1..6]      = 求出的六步增益 ×1000（新 SG_NUM 候选；旧 SG_NUM = {0,1152,1629,1325,...}） */
#define RK_SKIP_AFTER_COMM  4u   /* 换相后跳过几帧（避尖峰），与离线 --drop 3 对齐 */
extern volatile uint16_t rk_pp[7][4];
extern volatile uint16_t rk_gain[7];
extern volatile uint8_t  rk_arm, rk_done;
/* ★ K6：运行期六步表（初值=编译期宏；校准完由 rk_compute 覆盖）*/
extern volatile int16_t  cal_sg[7];
extern volatile int16_t  cal_so[7];
extern volatile int16_t  rk_so[7];
extern volatile int32_t  rk_sum[7][4];
extern volatile uint16_t rk_n[7][4];
extern volatile int16_t  rank_g[4];   /* ★ 4 个采样顺序增益 ×1000（order0=1.000）*/
void rank_g_compute(void);
/* ★ 调试：把两轮产物直接写进校准页空闲区（v1@0x180, v2@0x1C0），供 SWD 回读 */
void v2_apply(void);   /* ★ 把 V2 标定结果应用到判据（正常电调）*/
void dbg_two_save(uint8_t which, const volatile int16_t *rank4, const volatile int16_t *fg, const volatile int16_t *fo);
extern volatile uint8_t  cal_tables_ready;
extern volatile uint8_t  cal_apply;
void cal_sg_guard(void);   /* cal_sg 越界 ⇒ 回落编译期表（硬保险）*/
extern volatile uint8_t  cal_compute_req;   /* 1=立刻算一次 rk_compute()（不等跑结束）*/
/* ★ K12：按 V2.9 口径在 rank 修正后的数据上重拟合的结果 */
extern volatile int16_t  fit_g[7], fit_o[7];
extern volatile uint8_t  fit_ready;
extern volatile int32_t  fit_A[7], fit_M[7];
extern volatile int32_t  fit_Lsum[7];
extern volatile uint16_t fit_Lcnt[7];   /* 1=应用算出的表；0=保持 V2.9 宏（默认）*/
/* ★ K7：4 路通道增益 ×1000（A,B,C,V0）—— 这是 rank 校准的最终产物 */
extern volatile int16_t  ch_gain[4];
extern volatile int16_t  rk_k[5];   /* rank 因子 rank1..4 ×1000 */
extern volatile int32_t  rk_csum[4];
extern volatile uint32_t rk_cn;
extern volatile uint32_t rk_sq[7][4];
void rk_reset(void);
void rk_sample(uint8_t st, uint8_t order, int16_t dv, uint16_t va, uint16_t vb, uint16_t vc, uint16_t v0);
void rk_compute(void);
