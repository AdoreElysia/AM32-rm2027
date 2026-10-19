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
