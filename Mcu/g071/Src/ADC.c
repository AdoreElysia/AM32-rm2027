/*
 * ADC.c
 *
 *  Created on: May 20, 2020
 *      Author: Alka
 */
#include "ADC.h"
#include "dbg_trace.h"   /* RAM 黑匣子（在线调试用） */

#ifdef USE_ADC_BEMF
/* [0]电流 [1]电压 [2]温度 [3]VA [4]VB [5]VC [6]V0 —— 三相+V0 供软件过零用 */
#include "adc_bemf.h"
uint16_t ADCDataDMA[7];
#elif defined(USE_ADC_INPUT)
uint16_t ADCDataDMA[4];
#else
uint16_t ADCDataDMA[3];
#endif

extern uint16_t ADC_raw_temp;
extern uint16_t ADC_raw_volts;
extern uint16_t ADC_raw_current;
extern uint16_t ADC_raw_input;

#define ADC_DELAY_CALIB_ENABLE_CPU_CYCLES \
    (LL_ADC_DELAY_CALIB_ENABLE_ADC_CYCLES * 64)

void ADC_DMA_Callback()
{ // read dma buffer and set extern variables

#ifdef USE_ADC_INPUT
    ADC_raw_temp = ADCDataDMA[3];
    ADC_raw_volts = ADCDataDMA[1] / 2;
    ADC_raw_current = ADCDataDMA[2];
    ADC_raw_input = ADCDataDMA[0];

#else
    ADC_raw_temp = ADCDataDMA[2];
    ADC_raw_volts = ADCDataDMA[1];
    ADC_raw_current = ADCDataDMA[0];
#ifdef USE_ADC_BEMF
    /* ★ 2026-10-06：BEMF 采样点与 PWM 不同步 ⇒ 偶尔落在开关瞬间，采到反向尖刺
     *   （实测 burst 波形：低平台中间孤立的高尖刺，幅度达 ±300 码，而真实 BEMF 斜率
     *    只有约 30 码/ms）。尖刺会让软件过零在错误相位触发 ⇒ 换相乱 ⇒ 电机抖。
     *   这里做**限速滤波**：单个采样点相对上一拍变化超过 DBG_BEMF_SLEW_LIMIT 码就丢弃
     *   （保持上一拍的值），把尖刺挡在过零逻辑之外。
     *   注意：换相瞬间悬空相本来就会阶跃（flyback），所以这个限速顺带起了"消隐"作用。 */
    {
        static uint16_t fv[4] = { 2048, 2048, 2048, 2048 };   /* VA VB VC V0 的滤波后值 */
        const uint16_t raw[4] = { ADCDataDMA[3], ADCDataDMA[4], ADCDataDMA[5], ADCDataDMA[6] };

        for (uint8_t i = 0; i < 4u; i++) {
            int32_t d = (int32_t)raw[i] - (int32_t)fv[i];

            /* ★ 限速跟随（不是"丢弃"）：每拍最多朝 raw 移动 ±LIMIT 码。
             *   为什么不能直接丢弃：初值 2048 与真值差很远时会永远丢不完 ⇒ 滤波器锁死
             *   （第一版就踩了这个坑：55416/55416 全被丢，值卡在 2048）。
             *   限速跟随既能压掉尖刺（300 码的尖刺被限成 LIMIT），又一定能收敛。 */
            if (d > (int32_t)DBG_BEMF_SLEW_LIMIT) {
                fv[i] = (uint16_t)(fv[i] + DBG_BEMF_SLEW_LIMIT);
                dbg_trace_spike();          /* 记一次"被限速削掉的跳变"（黑匣子里看得到） */
            } else if (d < -(int32_t)DBG_BEMF_SLEW_LIMIT) {
                fv[i] = (uint16_t)(fv[i] - DBG_BEMF_SLEW_LIMIT);
                dbg_trace_spike();
            } else {
                fv[i] = raw[i];
            }
        }
        bemf_adc_va = fv[0];
        bemf_adc_vb = fv[1];
        bemf_adc_vc = fv[2];
        bemf_adc_v0 = fv[3];
    }
#endif
#endif
    dbg_trace_adc_sample();   /* 黑匣子：记一次采样（含"换相后连采"的 burst） */

    /* ★ 2026-10-06：重新武装 ADC。
     *   G0 的 ADC 在"单次模式 + 外部触发(EXTEN=1)"下，一个序列跑完会硬件清掉 ADSTART；
     *   不重新置位就再也不会响应触发 ⇒ 采样彻底停摆（实测：ADSTART=1 但一次都不转换）。
     *   这里的回调对每个序列跑一次（DMA 传完 7 个 halfword ⇒ TC），正好是重新武装的地方。 */
    LL_ADC_REG_StartConversion(ADC1);
}

void enableADC_DMA()
{ // enables channel

    NVIC_SetPriority(DMA1_Channel2_3_IRQn, 3);
    NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

    LL_DMA_ConfigAddresses(
        DMA1, LL_DMA_CHANNEL_2,
        LL_ADC_DMA_GetRegAddr(ADC1, LL_ADC_DMA_REG_REGULAR_DATA),
        (uint32_t)&ADCDataDMA, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    /* Set DMA transfer size */
#ifdef USE_ADC_BEMF
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, 7);
#elif defined(USE_ADC_INPUT)
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, 4);
#else
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, 3);

#endif
    /* Enable DMA transfer interruption: transfer complete */
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_2);

    /* Enable DMA transfer interruption: transfer error */
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_2);

    /*## Activation of DMA
     * #####################################################*/
    /* Enable the DMA transfer */
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
}

void activateADC(void)
{
    __IO uint32_t wait_loop_index = 0U;
    __IO uint32_t backup_setting_adc_dma_transfer = 0U;

    if (LL_ADC_IsEnabled(ADC1) == 0) {
        /* Enable ADC internal voltage regulator */
        LL_ADC_EnableInternalRegulator(ADC1);
        wait_loop_index = ((LL_ADC_DELAY_INTERNAL_REGUL_STAB_US * (SystemCoreClock / (100000 * 2))) / 10);
        while (wait_loop_index != 0) {
            wait_loop_index--;
        }
        backup_setting_adc_dma_transfer = LL_ADC_REG_GetDMATransfer(ADC1);
        LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_NONE);

        /* Run ADC self calibration */
        LL_ADC_StartCalibration(ADC1);

        /* Poll for ADC effectively calibrated */

        while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0) {
        }
        /* Restore ADC DMA transfer request after calibration */
        LL_ADC_REG_SetDMATransfer(ADC1, backup_setting_adc_dma_transfer);

        /* Delay between ADC end of calibration and ADC enable. */
        /* Note: Variable divided by 2 to compensate partially */
        /*       CPU processing cycles (depends on compilation optimization). */
        wait_loop_index = (ADC_DELAY_CALIB_ENABLE_CPU_CYCLES >> 1);
        while (wait_loop_index != 0) {
            wait_loop_index--;
        }
        /* Enable ADC */
        LL_ADC_Enable(ADC1);
        while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0) {
        }
        ADC->CCR |= ADC_CCR_TSEN;
    }
}
void ADC_Init(void)
{
    LL_ADC_REG_InitTypeDef ADC_REG_InitStruct = { 0 };
    LL_ADC_InitTypeDef ADC_InitStruct = { 0 };

    LL_GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    /* Peripheral clock enable */
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC);

    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    /**ADC1 GPIO Configuration
    PA4   ------> ADC1_IN4
    PA6   ------> ADC1_IN6
    */
    GPIO_InitStruct.Pin = VOLTAGE_ADC_PIN;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = CURRENT_ADC_PIN;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ADC1 DMA Init */

    /* ADC1 Init */
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_2, LL_DMAMUX_REQ_ADC1);

    LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_2,
        LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

    LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PRIORITY_HIGH);

    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MODE_CIRCULAR);

    LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PERIPH_NOINCREMENT);

    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MEMORY_INCREMENT);

    LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_PDATAALIGN_WORD);

    LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MDATAALIGN_HALFWORD);

    /* USER CODE BEGIN ADC1_Init 1 */

    /* USER CODE END ADC1_Init 1 */
    /** Configure Regular Channel
     */
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1),
        LL_ADC_CHANNEL_TEMPSENSOR);
    /** Configure the global features of the ADC (Clock, Resolution, Data
     * Alignment and number of conversion)
     */
    /* ★ 2026-10-06：触发源从"软件/1kHz 循环"改成 TIM1 TRGO2（OC4REF），
     *   ⇒ 每次 PWM 周期在 off-time 里自动采一轮，采样点与开关瞬间彻底同步。
     *   （原来 952 Hz 软件触发、相位相对 PWM 漂移 ⇒ 28% 的采样落在开关瞬间被污染。）*/
    /* 用 TIM1_CH4 的【比较事件】当触发源（EXTSEL=1）：
     *   它由 CNT==CCR4 产生，**不依赖 CC4E**（本板 CH4 的引脚是 PA11，与 B_HIN 共用，
     *   绝不能让 CH4 输出使能），也不依赖 MMS/TRGO 配置 ⇒ 最干净。
     *   CCR4 的位置 = 每个 PWM 周期的 off-time 里（见 peripherals.c）。 */
    ADC_REG_InitStruct.TriggerSource = LL_ADC_REG_TRIG_EXT_TIM1_CH4;
    #ifdef USE_ADC_BEMF
    ADC_REG_InitStruct.SequencerLength = LL_ADC_REG_SEQ_SCAN_ENABLE_7RANKS;
#else
    ADC_REG_InitStruct.SequencerLength = LL_ADC_REG_SEQ_SCAN_ENABLE_3RANKS;
#endif
    ADC_REG_InitStruct.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
    ADC_REG_InitStruct.ContinuousMode = LL_ADC_REG_CONV_SINGLE;
    ADC_REG_InitStruct.DMATransfer = LL_ADC_REG_DMA_TRANSFER_LIMITED;
    ADC_REG_InitStruct.Overrun = LL_ADC_REG_OVR_DATA_PRESERVED;
    LL_ADC_REG_Init(ADC1, &ADC_REG_InitStruct);
    // LL_ADC_REG_SetTriggerEdge(ADC1, LL_ADC_REG_TRIG_EXT_FALLING);
    LL_ADC_SetOverSamplingScope(ADC1, LL_ADC_OVS_DISABLE);
    LL_ADC_SetTriggerFrequencyMode(ADC1, LL_ADC_CLOCK_FREQ_MODE_LOW);
    LL_ADC_REG_SetSequencerConfigurable(ADC1, LL_ADC_REG_SEQ_CONFIGURABLE);
    LL_ADC_SetClock(ADC1, LL_ADC_CLOCK_ASYNC_DIV2);
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1,
        LL_ADC_SAMPLINGTIME_19CYCLES_5);
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_2,
        LL_ADC_SAMPLINGTIME_160CYCLES_5);
    LL_ADC_REG_SetTriggerEdge(ADC1, LL_ADC_REG_TRIG_EXT_RISING);
    LL_ADC_DisableIT_EOC(ADC1);
    LL_ADC_DisableIT_EOS(ADC1);

    ADC_InitStruct.Resolution = LL_ADC_RESOLUTION_12B;
    ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
    ADC_InitStruct.LowPowerMode = LL_ADC_LP_MODE_NONE;
    LL_ADC_Init(ADC1, &ADC_InitStruct);

    /* ★ 2026-10-06：序列顺序改成【BEMF 四路最前、慢通道在最后】。
     *   原因：ADC 由 TIM1 在 PWM off-time 内触发（见 peripherals.c 的 TRGO2=OC4），
     *   而采样点(孔径)在触发后逐个 rank 顺延 —— 4 个 BEMF rank 共约 4 µs，
     *   能整体落在 off-time 里；电流/电压/温度是慢量，放到后面（甚至落到 on-time）也无所谓。
     *   ⇒ DMA 缓冲区顺序随之变成 [0]VA [1]VB [2]VC [3]V0 [4]电流 [5]电压 [6]温度 */
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, CURRENT_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, CURRENT_ADC_CHANNEL, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, VOLTAGE_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, VOLTAGE_ADC_CHANNEL, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, LL_ADC_CHANNEL_TEMPSENSOR);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_TEMPSENSOR, LL_ADC_SAMPLINGTIME_COMMON_2);
#ifdef USE_ADC_BEMF
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_4, BEMF_ADC_PHASE_A_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_A_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_5, BEMF_ADC_PHASE_B_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_B_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_6, BEMF_ADC_PHASE_C_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_C_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_7, BEMF_ADC_NEUTRAL_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_NEUTRAL_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
#endif

}
