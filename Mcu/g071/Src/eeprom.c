/*
 * bootloader.c
 *
 *  Created on: Mar. 25, 2020
 *      Author: Alka
 *
 */

#include "eeprom.h"

#include <string.h>

#define page_size 0x800 // 2 kb for g071
uint32_t FLASH_FKEY1 = 0x45670123;
uint32_t FLASH_FKEY2 = 0xCDEF89AB;

void save_flash_nolib(uint8_t* data, int length, uint32_t add)
{
    /* ★ 2026-10-20：原来这里用 2KB 栈上 VLA（整页 2048B 时吃满栈 ⇒ 写页损坏）。
     *   现在直接从 data 读 32 位字写 flash，不再拷贝，栈开销 ≈ 0。*/
    uint32_t data_length = (uint32_t)length;

    // unlock flash

    while ((FLASH->SR & FLASH_SR_BSY1) != 0) {
        /*  add time-out*/
    }
    if ((FLASH->CR & FLASH_CR_LOCK) != 0) {
        FLASH->KEYR = FLASH_FKEY1;
        FLASH->KEYR = FLASH_FKEY2;
    }

    // erase page if address even divisable by 1024
    if ((add % 2048) == 0) {
        FLASH->CR |= FLASH_CR_PER;
        /* ★★★ 2026-10-20 修 bug ★★★
         *   原来写的是 `FLASH->CR |= (add / 2048) << 3;`
         *   add=0x0801F800 ⇒ 65600<<3 = 524800 ⇒ 溢出到 PNB 位域之外（PNB 只有 6 位，
         *   掩码 0x3F<<3）⇒ 【页号错误/擦除根本没执行】⇒ 之后往没擦的 flash 里写必然失败
         *   ⇒ esc_cal_save() 读回不一致、esc_cal_state=2。
         *   正确：页号要相对 flash 基址算，并掩进 PNB 位域。
         *   本芯片 128KB / 2KB 页 ⇒ 页号 0..63，0x0801F800 ⇒ 63（最后一页）。*/
        {
            uint32_t pnb = (add - 0x08000000u) / 2048u;
            FLASH->CR = (FLASH->CR & ~FLASH_CR_PNB_Msk) |
                        ((pnb << FLASH_CR_PNB_Pos) & FLASH_CR_PNB_Msk);
        }
        FLASH->CR |= FLASH_CR_STRT;
        while ((FLASH->SR & FLASH_SR_BSY1) != 0) {
            /*  add time-out */
        }
        if ((FLASH->SR & FLASH_SR_EOP) != 0) {
            FLASH->SR = FLASH_SR_EOP;
        } else {
            /* error */
        }
        FLASH->CR &= ~FLASH_CR_PER;
    }

    volatile uint32_t write_cnt = 0;
    while (write_cnt < data_length) {
        uint32_t w0 = *(uint32_t*)(data + write_cnt);
        uint32_t w1 = *(uint32_t*)(data + write_cnt + 4);
        FLASH->CR |= FLASH_CR_PG; /* (1) */
        *(__IO uint32_t*)(add + write_cnt) = w0;
        *(__IO uint32_t*)(add + write_cnt + 4) = w1;
        while ((FLASH->SR & FLASH_SR_BSY1) != 0) { /*  add time-out  */ }
        if ((FLASH->SR & FLASH_SR_EOP) != 0) {
            FLASH->SR = FLASH_SR_EOP;
        } else {
            /*  error  */
        }
        FLASH->CR &= ~FLASH_CR_PG;
        write_cnt += 8;
    }
    SET_BIT(FLASH->CR, FLASH_CR_LOCK);
}

void read_flash_bin(uint8_t* data, uint32_t add, int out_buff_len)
{
    // volatile uint32_t read_data;
    for (int i = 0; i < out_buff_len; i++) {
        data[i] = *(uint8_t*)(add + i);
    }
}
