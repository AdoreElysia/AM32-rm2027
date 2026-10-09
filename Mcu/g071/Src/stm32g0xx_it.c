/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    stm32g0xx_it.c
 * @brief   Interrupt Service Routines.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes
 * ------------------------------------------------------------------*/
#include "stm32g0xx_it.h"

#include "main.h"
/* Private includes
 * ----------------------------------------------------------*/
#include "ADC.h"
#include "IO.h"
#include "WS2812.h"
#include "targets.h"
#include "comparator.h"
#include "common.h"
#ifdef USE_ADC_BEMF
#include "adc_bemf.h"   /* ★ 本板过零检测的武装标志 bemf_irq_armed（替代比较器 EXTI）*/
#endif

/* USER CODE END Includes */

/* Private typedef
 * -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define
 * ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro
 * -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables
 * ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes
 * -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code
 * ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables
 * --------------------------------------------------------*/

/* USER CODE BEGIN EV */
extern void transfercomplete();
extern void PeriodElapsedCallback();
extern void interruptRoutine();
extern void tenKhzRoutine();
extern void processDshot();

extern volatile char send_telemetry;
uint16_t interrupt_time = 0;
extern volatile char servoPwm;
extern volatile char dshot_telemetry;
extern volatile char armed;
extern volatile char out_put;
extern volatile uint8_t compute_dshot_flag;

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0+ Processor Interruption and Exception Handlers */
/******************************************************************************/
/**
 * @brief This function handles Non maskable interrupt.
 */
void NMI_Handler(void)
{
    /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

    /* USER CODE END NonMaskableInt_IRQn 0 */
    /* USER CODE BEGIN NonMaskableInt_IRQn 1 */

    /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
 * @brief This function handles Hard fault interrupt.
 */
void HardFault_Handler(void)
{
    /* USER CODE BEGIN HardFault_IRQn 0 */

    /* USER CODE END HardFault_IRQn 0 */
    while (1) {
        /* USER CODE BEGIN W1_HardFault_IRQn 0 */
        /* USER CODE END W1_HardFault_IRQn 0 */
    }
}

/**
 * @brief This function handles System service call via SWI instruction.
 */
void SVC_Handler(void)
{
    /* USER CODE BEGIN SVC_IRQn 0 */

    /* USER CODE END SVC_IRQn 0 */
    /* USER CODE BEGIN SVC_IRQn 1 */

    /* USER CODE END SVC_IRQn 1 */
}

/**
 * @brief This function handles Pendable request for system service.
 */
void PendSV_Handler(void)
{
    /* USER CODE BEGIN PendSV_IRQn 0 */

    /* USER CODE END PendSV_IRQn 0 */
    /* USER CODE BEGIN PendSV_IRQn 1 */

    /* USER CODE END PendSV_IRQn 1 */
}

/**
 * @brief This function handles System tick timer.
 */
void SysTick_Handler(void)
{
    /* USER CODE BEGIN SysTick_IRQn 0 */

    /* USER CODE END SysTick_IRQn 0 */

    /* USER CODE BEGIN SysTick_IRQn 1 */

    /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32G0xx Peripheral Interrupt Handlers */
/* Add here the Interrupt Handlers for the used peripherals. */
/* For the available peripheral interrupt handler names, */
/* please refer to the startup file (startup_stm32g0xx.s). */
/******************************************************************************/

/**
 * @brief This function handles DMA1 channel 1 interrupt.
 */
void DMA1_Channel1_IRQHandler(void)
{
    if (armed && dshot_telemetry) {
        DMA1->IFCR |= DMA_IFCR_CGIF1;
        DMA1_Channel1->CCR = 0x00;
        if (out_put) {
            receiveDshotDma();
            compute_dshot_flag = 2;
        } else {
            sendDshotDma();
            compute_dshot_flag = 1;
        }
        EXTI->SWIER1 |= LL_EXTI_LINE_15;

        return;
    }
    /* USER CODE BEGIN DMA1_Channel1_IRQn 0 */
    if (LL_DMA_IsActiveFlag_HT1(DMA1)) {
        if (servoPwm) {
            LL_TIM_IC_SetPolarity(IC_TIMER_REGISTER, IC_TIMER_CHANNEL,
                LL_TIM_IC_POLARITY_FALLING);
            LL_DMA_ClearFlag_HT1(DMA1);
        }
    }
    if (LL_DMA_IsActiveFlag_TC1(DMA1) == 1) {
        DMA1->IFCR |= DMA_IFCR_CGIF1;
        DMA1_Channel1->CCR = 0x00;
        transfercomplete();
        EXTI->SWIER1 |= LL_EXTI_LINE_15;
    } else if (LL_DMA_IsActiveFlag_TE1(DMA1) == 1) {
        LL_DMA_ClearFlag_GI1(DMA1);
    }
}

/**
 * @brief This function handles DMA1 channel 2 and channel 3 interrupts.
 */
void DMA1_Channel2_3_IRQHandler(void)
{
    if (LL_DMA_IsActiveFlag_TC2(DMA1) == 1) {
        LL_DMA_ClearFlag_GI2(DMA1);
        ADC_DMA_Callback();
#ifdef USE_ADC_BEMF
        /* ★★ 重新"武装"下一次硬件触发 —— 本板 ADC 采样能连续跑起来的关键一行 ★★
         *   G0 这个 ADC（和 L4/G4 同款 IP）在 EXTEN≠0（外部触发）时的规矩：
         *     ① 触发检测必须先由软件写 ADSTART=1 才"武装"；
         *        ADSTART=0 期间到达的触发会被【直接丢弃】（不报错、不留痕）。
         *     ② 而 ADSTART 在【序列结束时会由硬件自动清零】⇒ 只武装一次的话，
         *        只有第一帧采样能跑，之后 24kHz 的 TIM1_CH4 触发全被丢掉。
         *   ⇒ 就在"一帧 DMA 搬完"这里重新武装（此刻 ADSTART 已由硬件清零，无竞争）。
         *   实测证据：武装一次后 EOS 置位、DMA 搬满 7 个字；400ms 内不再有第二帧。
         *   注：这样每帧的开销只是 1 次寄存器写；采样时刻仍由 TIM1_CH4（CCR4）决定。 */
        LL_ADC_REG_StartConversion(ADC1);
        /* ★★ 过零判据（2026-10-08 改成【边沿检测】）★★
         *   为什么改：原先是拿"电平"跟 `!rising` 比、并用
         *   `INTERVAL_TIMER->CNT > average_interval>>1` 当时间门槛。
         *   把数量算出来就露馅了 —— average_interval 的单位是【拍】，
         *   而 e_com_time = 3×单个换相步、average_interval = e_com_time/3
         *   ⇒ average_interval 恰好 = 1 个换相步 ⇒ `>> 1` 的门槛 = 【0.5 个换相步】。
         *   而真过零（悬空相 BEMF 穿过 V0）物理上就发生在【换相后约半个步长】——
         *   于是门槛正好压在真过零那一刻：采样点落前一点就"被判成噪声丢掉"，
         *   落后一点才认 ⇒ 同一步时认时不认 ⇒ 换相相位前后乱掉。
         *   （实测波形 C 相缺 0V / B 相缺 12V，就是换相相位错的典型相貌；
         *     开环之所以正常，是因为开环下 interruptRoutine() 整个短路、
         *     这段判据根本不参与 —— 见 main.c 的 #if RM2027_OL_ARM_ONLY。）
         *   改法（等价上游比较器 EXTI 的语义）：不在"电平"上做时间门槛，而是
         *   **只在状态真的翻转（边沿）那一帧报过零**。这样无论采样点落在过零的
         *   哪一侧，翻转事件都一定被报出一次 ⇒ 不再受 41.7µs 采样相位影响。
         *   三条判据：
         *     ① bemf_irq_armed —— 换相后武装、认到一次即清 ⇒【每步只认一次】。
         *     ② 边沿标志与 rising 一致（rising 步期望上升沿）—— 极性沿用
         *        main.c getBemfState() 的上游约定。
         *     ③ 上界（R3-c 那次的结论保留）：迟到超过 2 个步长的"过零"一定是假的，
         *        丢掉但不解除武装，交给 22.5ms 超时兜底。下界【不再需要】——
         *        换相瞬间的振铃由施密特滞回（BEMF_HYST_LSB）挡，
         *        且 changeCompInput() 已在换相时清掉上一相的边沿残留。 */
        {
            uint8_t edge_rise = 0, edge_fall = 0;

            /* 读边沿 + 就地清掉同向标志（同一个过零点只报一次） */
            adc_bemf_take_edges(&edge_rise, &edge_fall, 0, 0);

            if (bemf_irq_armed
                && ((rising && edge_rise) || (!rising && edge_fall))
                && ((average_interval < 50)
                    || (INTERVAL_TIMER->CNT < ((uint32_t)average_interval << 1)))) {
                /* 消费掉本次边沿：同向清掉、反向也清（换相后重新开始记） */
                adc_bemf_take_edges(0, 0, 1, 1);
                bemf_irq_armed = 0;
#ifdef USE_ADC_BEMF
                /* ★★ 闭环的"开环起步"期间（ol_startup_arm=1）：这里【什么都不做】，
                 *   只把武装标志放回去 —— 换相由 olStartupTick() 的固定时基独占，
                 *   两套时基同时动手会把换相节奏打乱。
                 *   ★ 2026-10-09：原来这里还在调 ol_startup_zc_inc() 数过零，
                 *     而 olStartupTick() 里【也在数】⇒ 同一个过零被记两次 ⇒
                 *     计数翻倍、交棒时机不可控。计数现在只在 olStartupTick() 一处做
                 *     （那里还能顺便用"跨越必须落在本步中间段"的时间窗滤掉振铃，
                 *     比这里靠 average_interval 的门槛更可靠）。 */
                extern volatile uint8_t ol_startup_arm;
                if (ol_startup_arm) {
                    bemf_irq_armed = 1;
                } else {
                    interruptRoutine();
                }
#else
                interruptRoutine();
#endif
            }
        }
#endif
    }
    if (LL_DMA_IsActiveFlag_TE2(DMA1) == 1) {
        LL_DMA_ClearFlag_TE2(DMA1);
    }

    if (LL_DMA_IsActiveFlag_TC3(DMA1)) {
        send_telemetry = 0;
        LL_DMA_ClearFlag_GI3(DMA1);
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
        /* Call function Transmission complete Callback */
    } else if (LL_DMA_IsActiveFlag_TE3(DMA1)) {
        LL_DMA_ClearFlag_GI3(DMA1);
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
        /* Call Error function */
        // USART_TransferError_Callback();
    }
}

/**
 * @brief This function handles ADC1, COMP1 and COMP2 interrupts (COMP
 * interrupts through EXTI lines 17 and 18).
 */
void ADC1_COMP_IRQHandler(void)
{
  if (LL_EXTI_IsActiveFallingFlag_0_31(LL_EXTI_LINE_18)) {
    if((INTERVAL_TIMER->CNT) > (average_interval >> 1)){
      LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_18);
      interruptRoutine();
    }else{
      if(getCompOutputLevel() == rising){
          LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_18);
          return;
      }
    }
    return;
  }

  if (LL_EXTI_IsActiveRisingFlag_0_31(LL_EXTI_LINE_18)) {
    if((INTERVAL_TIMER->CNT) > (average_interval >> 1)){
      LL_EXTI_ClearRisingFlag_0_31(LL_EXTI_LINE_18);
      interruptRoutine();
    }else{
      if(getCompOutputLevel() == rising){
          LL_EXTI_ClearRisingFlag_0_31(LL_EXTI_LINE_18);
          return;
      }
    }
    return;
  }
  if (LL_EXTI_IsActiveFallingFlag_0_31(LL_EXTI_LINE_17)) {
    if((INTERVAL_TIMER->CNT) > (average_interval >> 1)){
      LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_17);
      interruptRoutine();
    }else{
      if(getCompOutputLevel() == rising){
          LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_17);
          return;
      }
    }
    return;
  }

  if (LL_EXTI_IsActiveRisingFlag_0_31(LL_EXTI_LINE_17)) {
    if((INTERVAL_TIMER->CNT) > (average_interval >> 1)){
      LL_EXTI_ClearRisingFlag_0_31(LL_EXTI_LINE_17);
      interruptRoutine();
    }else{
      if(getCompOutputLevel() == rising){
          LL_EXTI_ClearRisingFlag_0_31(LL_EXTI_LINE_17);
          return;
      }
    }
    return;
  }
}

/**
 * @brief This function handles TIM3 global interrupt.
 */
void TIM3_IRQHandler(void)
{
    /* USER CODE BEGIN TIM3_IRQn 0 */
    if (LL_TIM_IsActiveFlag_CC1(TIM3) == 1) {
        LL_TIM_ClearFlag_CC1(TIM3);
    }

    if (LL_TIM_IsActiveFlag_UPDATE(TIM3) == 1) {
        LL_TIM_ClearFlag_UPDATE(TIM3);
        // update_interupt++;
    }
    /* USER CODE END TIM3_IRQn 0 */
    /* USER CODE BEGIN TIM3_IRQn 1 */

    /* USER CODE END TIM3_IRQn 1 */
}

void TIM16_IRQHandler(void)
{
    /* USER CODE BEGIN TIM3_IRQn 0 */
    if (LL_TIM_IsActiveFlag_CC1(TIM16) == 1) {
        LL_TIM_ClearFlag_CC1(TIM16);
    }

    if (LL_TIM_IsActiveFlag_UPDATE(TIM16) == 1) {
        LL_TIM_ClearFlag_UPDATE(TIM16);
        // update_interupt++;
    }
    /* USER CODE END TIM3_IRQn 0 */
    /* USER CODE BEGIN TIM3_IRQn 1 */

    /* USER CODE END TIM3_IRQn 1 */
}

void TIM6_DAC_LPTIM1_IRQHandler(void)
{
    /* USER CODE BEGIN TIM6_DAC_LPTIM1_IRQn 0 */
    if (LL_TIM_IsActiveFlag_UPDATE(TIM6) == 1) {
        LL_TIM_ClearFlag_UPDATE(TIM6);
        tenKhzRoutine();
    }
    /* USER CODE END TIM6_DAC_LPTIM1_IRQn 0 */

    /* USER CODE BEGIN TIM6_DAC_LPTIM1_IRQn 1 */

    /* USER CODE END TIM6_DAC_LPTIM1_IRQn 1 */
}

/**
 * @brief This function handles TIM14 global interrupt.
 */
void TIM14_IRQHandler(void)
{
    interrupt_time = UTILITY_TIMER->CNT;
    PeriodElapsedCallback();
    LL_TIM_ClearFlag_UPDATE(TIM14);
    interrupt_time = ((uint16_t)UTILITY_TIMER->CNT) - interrupt_time;
}

/* USER CODE BEGIN 1 */
void DMA1_Ch4_7_DMAMUX1_OVR_IRQHandler(void)
{
    /* USER CODE BEGIN DMA1_Ch4_7_DMAMUX1_OVR_IRQn 0 */
    if (LL_DMA_IsActiveFlag_HT6(DMA1)) {
    }
    if (LL_DMA_IsActiveFlag_TC6(DMA1) == 1) {
        LL_DMA_ClearFlag_GI6(DMA1);

        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_6);
        LL_TIM_DisableAllOutputs(TIM16);
        LL_TIM_DisableCounter(TIM16);
        dma_busy = 0;
    } else if (LL_DMA_IsActiveFlag_TE6(DMA1) == 1) {
        LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_6);
        LL_TIM_DisableAllOutputs(TIM16);
        LL_TIM_DisableCounter(TIM16);
        dma_busy = 0;
        LL_DMA_ClearFlag_GI6(DMA1);
    }
}

void EXTI4_15_IRQHandler(void)
{
    LL_EXTI_ClearRisingFlag_0_31(LL_EXTI_LINE_15);
    processDshot();
}
