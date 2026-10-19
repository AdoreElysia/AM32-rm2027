#ifndef ESC_CAL_H
#define ESC_CAL_H
#include <stdint.h>

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
void    esc_cal_load(void);
uint8_t esc_cal_save(const esc_cal_t *c);
#endif
