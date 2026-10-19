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
#define CAL_DUTY_DEF   120u /* 6% 占空比：只用来把"高侧相"抬到已知比例，避免大电流 */

extern volatile uint8_t  cal_mode;      /* 0=空闲 1=ADC静态 */
extern volatile uint8_t  cal_state;     /* 0=空闲 1=完成 2=失败 */
extern volatile uint16_t cal_bus_mv;    /* PC 传入的实测母线电压(mV) */
extern volatile uint16_t cal_duty;      /* 校准用占空比（0..2000）*/
extern volatile uint16_t cal_raw[6][4];   /* 低档 duty */
extern volatile uint16_t cal_raw2[6][4];  /* 高档 duty */ /* [step-1][VA,VB,VC,V0] 中位 */
void esc_cal_poll(void);                /* 主循环调用 */
