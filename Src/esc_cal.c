#include "esc_cal.h"
#include <string.h>
#include <stddef.h>
#include "eeprom.h"

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
volatile uint16_t cal_raw[6][4] = {{0}};

static uint8_t  cal_step   = 0;
static uint16_t cal_tick   = 0;
static uint32_t cal_acc[4] = {0,0,0,0};
static uint16_t cal_n      = 0;

void esc_cal_poll(void)
{
    if (cal_mode != CAL_MODE_ADC) { return; }
    extern void comStep(int);
    extern volatile uint16_t duty_cycle;
    extern uint16_t bemf_adc_va, bemf_adc_vb, bemf_adc_vc, bemf_adc_v0_hw;

    if (cal_state == 1u) { return; }       /* 已完成 */

    /* 每 100 次主循环 ≈ 几十 ms，切一个状态 */
    if (cal_tick < 200u) {
        cal_tick++;
        if (cal_tick > 40u) {              /* 状态稳定后再采 */
            cal_acc[0] += bemf_adc_va;
            cal_acc[1] += bemf_adc_vb;
            cal_acc[2] += bemf_adc_vc;
            cal_acc[3] += bemf_adc_v0_hw;
            cal_n++;
        }
        return;
    }
    /* 收尾当前状态 */
    if (cal_n) {
        cal_raw[cal_step][0] = (uint16_t)(cal_acc[0]/cal_n);
        cal_raw[cal_step][1] = (uint16_t)(cal_acc[1]/cal_n);
        cal_raw[cal_step][2] = (uint16_t)(cal_acc[2]/cal_n);
        cal_raw[cal_step][3] = (uint16_t)(cal_acc[3]/cal_n);
    }
    cal_acc[0]=cal_acc[1]=cal_acc[2]=cal_acc[3]=0; cal_n=0; cal_tick=0;
    cal_step++;
    if (cal_step >= 6u) {
        duty_cycle = 0;
        cal_mode = 0; cal_state = 1u;      /* 完成 */
        return;
    }
    comStep((int)cal_step + 1);
    duty_cycle = cal_duty;
}
