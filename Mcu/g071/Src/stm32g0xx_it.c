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
#include "adc_zc.h"     /* ★ 环形缓冲版过零检测器（变号 + 回看单调）*/
#endif

/* USER CODE END Includes */

/* ★★ 2026-10-10 实验埋点：开环驱动期间把"路径 B 的判据结果"记下来 ★★
 *   用户要求："开环跑，但开环时候要用测出闭环的数据"。
 *   本宏只在 ol_startup_arm=1（开环链在驱动换相）时生效 —— 那时路径 B 只"听"不驱动，
 *   所以把它的结论原样落盘不改变任何行为。变量说明见 Src/main.c 的 OL_OLP_* 注释。
 *   为什么用宏而不是函数：这是中断上下文，函数调用要多压一整套寄存器；
 *   而且两份调用点（接受侧 / 拒收侧）需要完全一致的取数次序。 */
#define OL_OLP_RECORD(okflag)                                                  \
    do {                                                                       \
        extern volatile uint8_t  ol_startup_arm;                               \
        if (ol_startup_arm) {                                                  \
            extern volatile uint8_t  ol_olp_idx;                               \
            extern volatile uint16_t OL_OLP_CNT[64], OL_OLP_CI[64];            \
            extern volatile uint16_t OL_OLP_WT[64], OL_OLP_AI[64];             \
            extern volatile uint16_t ol_olp_ci_last;                           \
            extern volatile int16_t  OL_OLP_D[64];                             \
            extern volatile uint8_t  OL_OLP_OK[64], OL_OLP_ST[64];             \
            extern volatile uint32_t ol_olp_n, ol_olp_acc;                     \
            uint8_t _oi = ol_olp_idx;                                          \
            OL_OLP_CNT[_oi] = (uint16_t)(zc_cnt);                              \
            OL_OLP_CI[_oi]  = (uint16_t)commutation_interval;                  \
            OL_OLP_WT[_oi]  = waitTime;                                        \
            OL_OLP_AI[_oi]  = (uint16_t)average_interval;                      \
            OL_OLP_D[_oi]   = adc_bemf_get_delta();                            \
            OL_OLP_OK[_oi]  = (okflag);                                        \
            OL_OLP_ST[_oi]  = (uint8_t)(step);                                 \
            ol_olp_ci_last  = (uint16_t)commutation_interval;                  \
            ol_olp_idx = (uint8_t)((_oi + 1) & 63);                            \
            ol_olp_n++;                                                        \
            if (okflag) { ol_olp_acc++; }                                      \
        }                                                                      \
    } while (0)

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
            /* ★★★ 2026-10-09 关键修复③：必须在【本帧】判定"翻转是否发生在这一帧" ★★★
             *   原来的次序是"读边沿（不清）→ 判时间窗" ⇒ 被窗口拒掉的边沿会【一直挂着】，
             *   等到后面某一帧窗口通过才被收下 ⇒ interruptRoutine() 里
             *   `thiszctime = INTERVAL_TIMER_COUNT` 量到的是【收下的时刻】，
             *   比真过零晚了不知多久。
             *   埋点铁证（12V 台架）：接受记录里的 delta 分成清清楚楚的两类 ——
             *     小的 ±5~±39  （正好刚跨过阈值 ⇒ 翻转就发生在记录的那一帧）；
             *     大的 ±374~±409（= 完整反电势幅值 ⇒ 翻转【早就】发生了，
             *                     记下的是"被收下"的时刻，滞后了几百个采样）。
             *   下游后果：CNT/CI 恒在 0.6 附近、永不收敛到 1.0 ⇒ ci 被滤波一路带小
             *   （之前所有"runaway / 永久丢锁 / 43RPM 慢抖"都是这个测量错误的下游）。
             *   修法：把次序倒过来 —— 先清旧标志 → 再用本帧新采样更新滞回状态 →
             *   再读。这样"读到的边沿"必定属于本帧 ⇒ 此刻的 CNT 就是过零时刻
             *   （误差 ≤ 1 个采样周期 41.6µs）。
             *   被拒的边沿也一并清掉（丢掉）：真落在下界之前的多半是换相振铃；
             *   万一是真过零被卡住，下一次过零 / 22.5ms 兜底会把它带回来（可自恢复）。
             *
             * ★ 2026-10-09 关键修复②（保留）：换相后第一帧的边沿作废。
             *   换相换了悬空相 ⇒ 下一帧 delta 必跳变 ⇒ 必置出假边沿；
             *   这一帧只用来"重新落定"滞回状态（getCompOutputLevel 已做）。 */
            uint8_t blanked = adc_bemf_blank_take();
#if BEMF_ZC_USE_RINGBUF
            /* ★★★ 2026-10-10：过零判据换成"环形缓冲 + 变号 + 回看单调"（用户指定）★★★
             *   用户要的结构：缓冲里存样本 → 检测到变号 → 回看前面 N 点是否严格单调。
             *   为什么它比原来那套可靠：原来靠两个【时间门槛】（换相后屏蔽 3 帧、
             *   75µs 下界）去躲换相振铃，门槛都是拍脑袋定的、还互相耦合；新判据不用
             *   时间信息，只用波形形状 —— 真过零是平缓斜坡（20~40 码/点）且连续多点
             *   同向，而换相跳变一步差 500~800 码，会把"连续 MONO_N 点单调"直接打断。
             *   实测（本板真实采样数据、23 个真过零 / 46 次换相跳变）：
             *     MONO_N=4 ⇒ 22 判出 = 22 真过零、0 误报，拒收 45 次全部落在换相跳变上。
             *   ★ 下游一行不动（bemf_irq_armed / bemf_zc_time / interruptRoutine /
             *     计时器清零），只换"认不认到过零"这一个判据 ⇒ 换相时序与原来一致。
             *   ★ 仍然调 getCompOutputLevel()：轮询路径 getBemfState() 要用 bemf_state。
             *   ★ 注意：这里只推一次 adc_zc_push —— 别再在别处重复推，会破坏缓冲区。 */
            adc_bemf_take_edges(0, 0, 1, 1);   /* 清残留 */
            (void)getCompOutputLevel();        /* 只更新 bemf_state，不再用于判据 */
            adc_bemf_take_edges(0, 0, 1, 1);   /* 旧判据的边沿读出来丢掉 */
            {
                extern volatile uint16_t bemf_adc_vph;   /* rank1 = 当前悬空相 */
                extern volatile uint16_t bemf_adc_v0;    /* rank2 = V0 */
                int16_t dzc = adc_bemf_get_delta();
                uint8_t zc  = adc_zc_push(dzc, (uint16_t)INTERVAL_TIMER->CNT);
                /* 顺手把这一帧写进闭环波形环形缓冲（交棒时冻结，供 SWD dump 画图）
                 * ★ vph / v0 / step 分开记 —— 这样才分得清交棒时的偏置跳变
                 *   是 vph 动、v0 动，还是 rank1 指错了相（见 adc_zc.h 的说明）。 */
                adc_zc_wave_push((int16_t)bemf_adc_vph, (int16_t)bemf_adc_v0,
                                 (uint16_t)TIM1->CCR1, (uint8_t)step, zc);
                if (zc) {
                    edge_rise = 1;   /* 方向不参与判据（本板 rising 的极性约定无可靠对照）*/
                }
                /* ★ 闭环波形捕获的触发：ci 涨到交棒实测值的 2 倍 ⇒ 塌陷开始，冻结。
                 *   只在"已武装且还没触发"时判一次；触发后 state 变 1/2 就不再进。 */
                {
                    extern volatile uint32_t commutation_interval;
                    if (zcw_ci_trig != 0u && zcw_state == 0u &&
                        commutation_interval > zcw_ci_trig) {
                        adc_zc_wave_trigger();
                    }
                }
            }
#else
            adc_bemf_take_edges(0, 0, 1, 1);      /* 清掉上一帧的残留 */
            (void)getCompOutputLevel();           /* 用本帧新采样更新滞回状态 */
            adc_bemf_take_edges(&edge_rise, &edge_fall, 1, 1);  /* 读本帧翻转并清掉 */
#endif

            /* ★ 2026-10-09 观测：记住本步内 |delta| 的峰值（= 反电势幅值）。
             *   它和"接受时 delta 只有 ±5~±11"一起，才能证明信号是不是在阈值上蹭
             *   —— 直接决定施密特滞回 BEMF_HYST_LSB 该取多大。每次过零清零。
             * ★ 注意位置：必须放在下面 `if (blanked) ... else if (...)` 之前，
             *   不能夹在两者之间 —— 否则会把 if/else 链断开（编译报
             *   "'else' without a previous 'if'"）。 */
            {
                extern volatile int16_t bemf_dmax;
                int16_t dd = adc_bemf_get_delta();
                int16_t ad = (dd < 0) ? (int16_t)(-dd) : dd;
                if (ad > bemf_dmax) {
                    bemf_dmax = ad;
                }
            }

            /* ★★★ 2026-10-10 新增：环形缓冲版过零检测器（用户指定的结构）★★★
             *   结构：环形缓冲区存样本 → 检测到变号 → 回看前面 N 点是否严格单调。
             *   现在它是【真正的判据】—— 见上面那段（BEMF_ZC_USE_RINGBUF）。
             *   这里不再单独推一次，避免同一帧推两次破坏缓冲区。 */
            /* ★ 2026-10-09 悬空相波形捕捉：每 8 帧记一次（≈3kHz 采样，64 点 ≈21ms）。
             *   用途见 Src/main.c 里 OL_WF_* 的说明 —— 把"悬空相 − V0"还原成波形，
             *   一次性判定"硬件没真悬空"还是"反电势太小被盖住"。 */
            {
                extern int16_t  OL_WF_D[64];
                extern uint16_t OL_WF_T[64];
                extern uint8_t  OL_WF_S[64];
                extern uint8_t  ol_wf_idx, ol_wf_div;
                /* step 已在包含的头里声明（非 volatile），不要在这里重复限定符 */
                /* ★★★ 2026-10-10 由 8 改成 64 —— 用户要求"把 ADC 采样的波形画出来" ★★★
             *   原来每 8 帧记一点 = 3kHz 采样、64 点只覆盖 21ms；
             *   而转子实测只有约 300 RPM ⇒ 电频率约 30Hz(周期 33ms)
             *   ⇒ 21ms 连一个电周期都不到，画不出转子的形状。
             *   改成每 64 帧一点 ⇒ 375Hz 采样、64 点覆盖 170ms ⇒ 可包住约 5 个电周期 ✓ */
            if (++ol_wf_div >= 64) {
                    ol_wf_div = 0;
                    OL_WF_D[ol_wf_idx] = adc_bemf_get_delta();
                    OL_WF_T[ol_wf_idx] = (uint16_t)INTERVAL_TIMER->CNT;
                    OL_WF_S[ol_wf_idx] = (uint8_t)step;
                    ol_wf_idx = (uint8_t)((ol_wf_idx + 1) & 63);
                }
            }

            /* ★ 三步定位计数器（见 Src/main.c 里的说明）。放在 if(blanked) 之前，
             *   是一个独立的花括号块 ⇒ 不会破坏下面的 if/else 判据链。 */
            {
                extern volatile uint32_t ol_adc_isr_n, ol_edge_n, ol_noarm_n;
                ol_adc_isr_n++;
                if (!blanked && (edge_rise || edge_fall)) {
                    ol_edge_n++;
                    if (!bemf_irq_armed) {
                        ol_noarm_n++;
                    }
                }
            }

            if (blanked) {
                edge_rise = edge_fall = 0;
                /* ★ 2026-10-10：开环期间"换相后第一帧被作废"也要计数 ——
                 *   配平用：边沿帧数 = 接受 + 窗外拒收 + 未武装丢弃 + 本项。 */
                {
                    extern volatile uint8_t ol_startup_arm;
                    extern volatile uint32_t ol_olp_blank;
                    if (ol_startup_arm) { ol_olp_blank++; }
                }
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
                /* ★★★ 2026-10-09 关键修复④：下界改成"换相点之后" ★★★
                 *   原来用 average_interval>>1（6 步滑动平均的一半），两个毛病：
                 *     ① average_interval 滞后、且与当前这一步的 waitTime 无关；
                 *     ② 它算出来的门槛可能落在换相点【之前】，于是换相瞬间的电平突跳
                 *        被收下（埋点铁证：某次接受记录 过零时delta = -595，
                 *        —— 滞回 16 码不可能在 |d|=595 时翻转，那只能是突跳），
                 *        该条位置 CNT/CI = 0.22 也正贴着换相点。
                 *   物理事实：真过零必然发生在【换相之后】—— 换相点在
                 *   waitTime = ci/2 − advance，真过零在它之后约半步处。
                 *   ⇒ 下界直接取 waitTime + 1/16 步余量（≈2 个采样周期，盖住振铃尾巴），
                 *     把换相点及其突跳整体排除。这样收到的每一条都必然是"换相后
                 *     真的穿过了零点"。 */
                extern volatile uint16_t waitTime;             /* 定义在 Src/main.c */
                extern volatile uint32_t commutation_interval; /* 定义在 Src/main.c */
                /* ★★★ 2026-10-10 下界从"与 CI 成正比"改成【固定小常数】—— 治自锁 ★★★
                 *   实测（接受环/拒绝环）：
                 *     · 真转子周期 meas_ci = 1100（开环实测）⇒ 转子真在发 1818 个过零/秒；
                 *     · 闭环 CI 却漂到 ~8500 ⇒ 按 1/8 速度换相 ⇒ 剧烈抖动；
                 *     · 拒绝环里被拒的 CNT ≈ 900~1070 —— 正是【真过零】的位置！
                 *   原因：原下界 zc_lo = waitTime + CI>>4 ≈ 0.5625×CI 与 CI 成正比，
                 *   而 CI 来自"被接受的过零" ⇒ CI 一旦偏大，下界就高到把所有真过零
                 *   判成"太早"拒掉 ⇒ 拒掉后计时器不重置 ⇒ 下次接受的间隔更长 ⇒ CI 再涨
                 *   ⇒ 自锁（实测 0.8 秒内 CI 从 1204 飙到 9358）。
                 *   修法：下界改成固定 OL_ZC_MIN_TICKS(150 拍 = 75µs)，只挡换相振铃
                 *   （实测 33~73µs），不再随 CI 变化 ⇒ 自锁结构被拆掉。 */
                uint32_t zc_lo = (uint32_t)OL_ZC_MIN_TICKS;
                if ((average_interval < 50) || (zc_cnt > zc_lo)) {
                /* 边沿已经在"读的时候"清掉了（见上），这里只把过零【发生】的时刻
                 * 交给 interruptRoutine()：它必须用 bemf_zc_time，不能用
                 * INTERVAL_TIMER_COUNT —— 后者是"此刻"，而此刻就是过零时刻
                 * （同一帧内），但那样写容易在后续改动里又被拉长成"接受时刻"。 */
                extern volatile uint16_t bemf_zc_time;   /* 定义在 Src/main.c */
                bemf_irq_armed = 0;
                /* ★★ 2026-10-09 关键修复⑥：别让"丢锁很久"的间隔回绕成小数值 ★★
                 *   INTERVAL_TIMER 是 2MHz 的 32 位计数器（可数 2147s），
                 *   而 thiszctime / bemf_zc_time 都是 uint16 ⇒ 超过 65535 拍
                 *   （32.7ms）就会回绕。丢锁时真实间隔是几百毫秒，回绕后变成一个
                 *   【很小】的数 ⇒ PeriodElapsedCallback 的滤波
                 *      ci = (ci + (last+this)/2)/2
                 *   会把 ci 当成"转得飞快"往小里拉 ⇒ commutation_interval 越跑越偏
                 *   ⇒ 永远回不到正轨（实测：接受记录里 CNT 恒在 2300~3100 这种
                 *   小值，而实际接受速率只有 ~5 次/秒 —— 两者矛盾就是回绕的铁证）。
                 *   钳到 0xFFFF：语义变成"间隔至少 32.7ms"，ci 会被拉大而不是拉小，
                 *   闭环就能自己从丢锁里往回爬（变大方向不受速率闸限制）。 */
                bemf_zc_time = (zc_cnt > 0xFFFFu) ? (uint16_t)0xFFFFu : (uint16_t)zc_cnt;

                /* ★★ 2026-10-10 实验埋点（用户要求："开环跑，但用闭环的数据"）★★
                 *   开环驱动期间（ol_startup_arm=1）路径 B 不驱动换相，但这里
                 *   照样把"判成接受"这条记录写下来 —— 原来只有 interruptRoutine()
                 *   里才有接受侧记录，而它在开环期间不被调用 ⇒ 开环时有边沿、
                 *   判成接受的次数完全不可见。见 Src/main.c 的 OL_OLP_* 说明。 */
                OL_OLP_RECORD(1);

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
                extern void ol_meas_cross(uint16_t cnt);
                if (ol_startup_arm) {
                    /* ★ 2026-10-10：开环期间【只测不换相】——把每个被接受的过零
                     *   交给 main.c 的 ol_meas_cross() 累计"过零到过零"的真实间隔
                     *   （= 转子周期，因为反电势过零跟着转子走）。
                     *   交棒时用这个实测值做闭环初值 ⇒ 不再"切过去没数据、跟不上"。
                     *   这里仍然不调 interruptRoutine()：换相仍由开环链独占。 */
                    ol_meas_cross((zc_cnt > 0xFFFFu) ? (uint16_t)0xFFFFu : (uint16_t)zc_cnt);
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
                    /* ★ 2026-10-10：开环驱动期间的拒收也要进实验环（OK=0），
                     *   这样 ol_olp_n 才等于"开环期间路径 B 遇到的全部边沿"。 */
                    OL_OLP_RECORD(0);
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
