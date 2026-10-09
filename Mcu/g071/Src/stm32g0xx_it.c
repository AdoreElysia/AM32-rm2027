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

            /* ★★★ 2026-10-09 关键修复：每帧采样后必须主动调一次 getCompOutputLevel() ★★★
             *   它是【唯一】会更新施密特状态(bemf_state)、并置边沿标志的地方。
             *   原来闭环里根本没人调它 ⇒ 边沿恒为 0 ⇒ 下面 take_edges() 永远读到 0
             *   ⇒ 认不到过零 ⇒ interruptRoutine() 永不执行；而 interruptRoutine()
             *   恰恰是闭环里唯一会调 getCompOutputLevel() 的路径 ⇒【互为前提、死锁】。
             *   实测证据（非侵入式读 RAM）：交棒成功后 old_routine=0、ci=1500、zc=3，
             *   但 steps 冻在 100、zero_crosses=0、bemf_irq_armed=1 一直在空等、
             *   edge_rise=edge_fall=0 —— 而 zcdelta 高达 427 码（信号很强，
             *   是被判据卡住，不是没信号）。
             *   喂数据的位置就在这里：上一行 ADC_DMA_Callback() 刚把 7 个通道搬进 RAM。 */
            (void)getCompOutputLevel();

            /* 读边沿 + 就地清掉同向标志（同一个过零点只报一次） */
            adc_bemf_take_edges(&edge_rise, &edge_fall, 0, 0);

            /* ★★★ 2026-10-09 关键修复②：丢弃"换相后第一帧"的边沿 ★★★
             *   换相把悬空相换掉 ⇒ 下一帧采样到的 delta 必然跳变 ⇒ 必然置出一个
             *   假边沿。changeCompInput() 虽然清了边沿标志，但清的是【换相那一刻】，
             *   而跳变要到【下一帧采样】才被 getCompOutputLevel() 看到 ⇒ 又冒一个新边沿。
             *   实测后果（12V 台架）：交棒瞬间 ci=1500 一切正常，紧接着
             *   commutation_intervals 变成 [70,62,62,66,77,77]，ci 按 0.75× 一路衰减到 62
             *   （≈31µs，比 PWM 周期 41.6µs 还短）⇒ 换相比 PWM 还快 ⇒ 六步波形被搅烂，
             *   现场表现就是用户看到的"输出成了恒定高的 PWM、电机不转"。
             *   这一帧只用来"重新落定"滞回状态（上一行 getCompOutputLevel 已经做了），
             *   边沿整个作废。
             * ★ 注意这里不能用 return —— 本函数后面还有 TC3/USART 的处理。 */
            if (adc_bemf_blank_take()) {
                adc_bemf_take_edges(0, 0, 1, 1);
            }
            /* ★ 判据一：极性无关 —— 只问"这一步里状态翻转过没有"。
             *   物理上六步换相每一步的悬空相恰好过一次零 ⇒ 恰好一个边沿，
             *   所以"哪个方向"不携带任何信息，只带来"约定一旦反了就一个都认不到"的风险
             *   （本板 A/B 相没有比较器可与上游约定对照，这个风险是实打实的）。
             * ★ 判据二：时间窗 [1/2 步, 2 步]。
             *   ★★ 下界为什么是 1/2 步（2026-10-09 从 1/4 改回来）★★
             *   时间轴（timer 在每次过零时清零）：过零在 CNT≈0，换相发生在
             *   waitTime = ci/2 − advance ≈ 0.28·ci，真过零在下一次 CNT≈ci。
             *   ⇒ 换相点(0.28ci) 落在窗内的话，紧跟着的假边沿就会被收下（正反馈）。
             *   下界必须【高于换相点】—— ci/2 正好夹在换相点 0.28ci 与真过零 ci 中间，
             *   这就是上游 AM32 用 `CNT > average_interval>>1` 的道理。
             *   上一轮把它改成 >>2（理由是"它压在真过零上"）是误判：真过零在 ci 处，
             *   不在 ci/2 处 —— 那个判断是在 ci 已被假边沿拉小、waitTime 也缩到 ci/2
             *   附近时做的，属于"病了之后量出来的体温"。
             * ★★ 上界【取消】（2026-10-09）★★
             *   原来有 `CNT < average_interval << 1`，本意是"迟到太多的必是假的"。
             *   但埋点实测证明它造成了【永久死锁】：一次 runaway 把 ci 拉到 300 之后，
             *   真过零出现在 CNT≈42500（rejection 记录里一排 42503/42587/42684…，
             *   比值 141），远超 2×300=600 ⇒ 全被拒 ⇒ 而 average_interval 只在
             *   PeriodElapsedCallback 里更新、那个函数又【必须以认到过零为前提】
             *   ⇒ 不认 ⇒ 窗口不更新 ⇒ 永远认不到，只能靠 22.5ms 兜底硬踹
             *   （bemf_timeout_happened 以 ~26 次/秒 上涨 ⇒ 26 步/秒 ≈ 43 机械 RPM，
             *    现场就是"慢且抖、没有梯形腰"）。
             *   去掉上界后：丢锁时那个迟到的真过零照样被收下 ⇒
             *   ci_new = (300 + (300+42500)/2)/2 ≈ 9000（本闸只限变小、不限变大）
             *   ⇒ 闭环自己重新锁到一个低转速、再随占空比爬升 ⇒ 能自恢复。
             *   真正要防的假过零是【太早】那种（换相振铃），那个由下界管。 */
            else if (bemf_irq_armed && (edge_rise || edge_fall)) {
                uint32_t zc_cnt = INTERVAL_TIMER->CNT;
                if ((average_interval < 50)
                    || (zc_cnt > ((uint32_t)average_interval >> 1))) {
                /* 消费掉本次边沿：两个方向都清（换相后重新开始记） */
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
                } else {
                    /* ★ 2026-10-09 埋点：有边沿、但被时间窗拒掉 —— 记下"当时在哪"，
                     *   用来判断被拒的是"换相振铃"还是"真过零被窗口卡了"。
                     *   配合 OL_TRACE_*（接受侧）一起 dump，就能把判据一次看清。 */
                    extern volatile uint16_t OL_REJ_CNT[32], OL_REJ_CI[32];
                    extern volatile uint8_t ol_rej_idx;
                    extern volatile uint16_t ol_trace_rej;
                    OL_REJ_CNT[ol_rej_idx] = (uint16_t)zc_cnt;
                    OL_REJ_CI[ol_rej_idx]  = (uint16_t)average_interval;
                    ol_rej_idx = (uint8_t)((ol_rej_idx + 1) & 31);
                    if (ol_trace_rej < 60000) {
                        ol_trace_rej++;
                    }
                }
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
