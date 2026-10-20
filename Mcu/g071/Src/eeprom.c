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
    uint32_t data_to_FLASH[length / 4];
    memset(data_to_FLASH, 0, length / 4);
    for (int i = 0; i < length / 4; i++) {
        data_to_FLASH[i] = data[i * 4 + 3] << 24 | data[i * 4 + 2] << 16 | data[i * 4 + 1] << 8 | data[i * 4]; // make 16 bit
    }
    volatile uint32_t data_length = length / 4;

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

    volatile uint32_t write_cnt = 0, index = 0;
    while (index < data_length) {
        FLASH->CR |= FLASH_CR_PG; /* (1) */
        *(__IO uint32_t*)(add + write_cnt) = data_to_FLASH[index];
        *(__IO uint32_t*)(add + write_cnt + 4) = data_to_FLASH[index + 1];
        while ((FLASH->SR & FLASH_SR_BSY1) != 0) { /*  add time-out  */
        }
        if ((FLASH->SR & FLASH_SR_EOP) != 0) {
            FLASH->SR = FLASH_SR_EOP;
        } else {
            /*  error  */
        }
        FLASH->CR &= ~FLASH_CR_PG;
        write_cnt += 8;
        index += 2;
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
