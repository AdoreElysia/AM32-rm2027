/*
 * ADC.c
 *
 *  Created on: May 20, 2020
 *      Author: Alka
 */
#include "ADC.h"

#ifdef USE_ADC_BEMF
/* ★ 扫描顺序（rank1..7）= [0]VA [1]VB [2]VC [3]V0 [4]电流 [5]电压 [6]温度
 *   为什么 BEMF 排最前：采样时刻被 TIM1_CH4 钉死在"高边开通后 ADC_TRIGGER_CCR4 拍"，
 *   而整条扫描必须排在同一个导通期内（见 ADC_Init 里 rank 顺序的说明）。
 *   —— 顺序改了，ADC_DMA_Callback() 的下标必须跟着改，两处是一体。 */
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
#ifdef USE_ADC_BEMF
    /* 顺序见文件头注释：VA VB VC V0 电流 电压 温度 */
    bemf_adc_va = ADCDataDMA[0];
    bemf_adc_vb = ADCDataDMA[1];
    bemf_adc_vc = ADCDataDMA[2];
    bemf_adc_v0 = ADCDataDMA[3];
    ADC_raw_current = ADCDataDMA[4];
    ADC_raw_volts = ADCDataDMA[5];
    ADC_raw_temp = ADCDataDMA[6];
#else
    ADC_raw_temp = ADCDataDMA[2];
    ADC_raw_volts = ADCDataDMA[1];
    ADC_raw_current = ADCDataDMA[0];
#endif
#endif
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

#ifdef USE_ADC_BEMF
        /* ★ 首次"武装"硬件触发（必须在 ADEN=1 之后才能写 ADSTART）。
         *   之后每一帧都在 DMA 完成中断里重新武装 —— 原因见 stm32g0xx_it.c
         *   DMA1_Channel2_3_IRQHandler 里的说明：本 ADC 的 ADSTART 每帧结束会被
         *   硬件自动清零，不重新写就等于"只有第一帧"。
         *   注：这里写的时候 ADC 尚未开始转换（ADSTART=0、ADDIS=0）✓ */
        LL_ADC_REG_StartConversion(ADC1);
#endif
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
#ifdef USE_ADC_BEMF
    /* ★ 触发源：TIM1_CH4（硬件、与 PWM 同频同步）—— 取代原来的 1kHz 软件触发。
     *   CH4 只当"采样时刻发生器"，不接引脚：peripherals.c 里把它设成 PWM2 模式，
     *   于是 OC4REF 的上升沿正好落在 CNT == CCR4 ⇒ CCR4 的数值就是
     *   "高边开通(CNT=0)后多久开始采样"（板级常量 ADC_TRIGGER_CCR4，targets.h）。
     *   ⇒ 采样点固定压在导通期内、离开关沿足够远；而且每个 PWM 周期都刷一次
     *   （24kHz），闭环判过零要的就是这个刷新率 —— 1kHz 在 3700RPM 下
     *     （换相窗口仅 450µs）连一个点都落不进去。 */
    ADC_REG_InitStruct.TriggerSource = LL_ADC_REG_TRIG_EXT_TIM1_CH4;
#else
    ADC_REG_InitStruct.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
#endif
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
    /* 注（实测结论，别再被这行误导）：这行的意图是"触发频率低 ⇒ 开 CFGR2.LFTRIG"，
     * 但传进去的 LL_ADC_CLOCK_FREQ_MODE_LOW 是【CCR 里的 LFMEN 位】、不是 CFGR2 的
     * LFTRIG 位，于是 MODIFY_REG 反而把 LFTRIG 清成 0。上机实测：LFTRIG=0 时
     * TIM1_CH4 触发照样能用（真正的门槛是 ADSTART 必须先被软件置 1，见 activateADC()
     * 与 stm32g0xx_it.c 的 DMA 完成中断），所以这里【保持原样不动】—— 改它没有收益、
     * 只会多一个变量。 */
    LL_ADC_SetTriggerFrequencyMode(ADC1, LL_ADC_CLOCK_FREQ_MODE_LOW);
    LL_ADC_REG_SetSequencerConfigurable(ADC1, LL_ADC_REG_SEQ_CONFIGURABLE);
    /* ★ ADC 时钟必须钉死，因为一帧 7 通道的总耗时要能塞进 PWM 导通期里。
     *   实测：改之前 ADC 时钟 ≈ 64MHz（RCC ADCSEL 复位态选的源 + 预分频=1）
     *        ⇒ 一帧只要 5.8µs，但 64MHz 超了 G0 数据手册的 ADC 上限 35MHz@VDD≥2.7V。
     *   现在：异步时钟 + 预分频 ÷2 = 32MHz ✓ 在规格内。
     *        一帧 = 6 个 32 周期通道 + 温度 173 周期 = 365 周期 ⇒ 11.6µs。
     *        ⇒ 触发后 2.3~4.3µs 采完 4 路 BEMF；电流落在 4.3~4.8µs（导通期前半，
     *          台架 17.6% 占空比的导通期是 7.3µs ⇒ 余量充足）；
     *          温度通道尾巴伸到关断期——它是最慢的量，无所谓。
     *   ⚠ 两个易错点（改的时候别踩）：
     *     ① LL_ADC_SetClock() 写的是 CFGR2.CKMODE = bit[31:30]，参数只能取
     *        LL_ADC_CLOCK_SYNC_PCLK_* 或 LL_ADC_CLOCK_ASYNC；而"÷2/÷4…"是 CCR.PRESC，
     *        归 LL_ADC_SetCommonClock() 管。原来把 LL_ADC_CLOCK_ASYNC_DIV2 喂给
     *        SetClock() ⇒ CKMODE 没变、PRESC 也没设，时钟频率全看 RCC 复位值。
     *     ② ADCSEL 若被改（RCC_CCIPR bit[31:30]），这里算好的帧时长就整体变。 */
    LL_ADC_SetClock(ADC1, LL_ADC_CLOCK_ASYNC);
    LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_CLOCK_ASYNC_DIV2);
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1,
        LL_ADC_SAMPLINGTIME_19CYCLES_5);
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_2,
        LL_ADC_SAMPLINGTIME_160CYCLES_5);
    LL_ADC_DisableIT_EOC(ADC1);
    LL_ADC_DisableIT_EOS(ADC1);

    ADC_InitStruct.Resolution = LL_ADC_RESOLUTION_12B;
    ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
    ADC_InitStruct.LowPowerMode = LL_ADC_LP_MODE_NONE;
    LL_ADC_Init(ADC1, &ADC_InitStruct);

#ifdef USE_ADC_BEMF
    /* ★ 扫描顺序（rank1..7）：VA VB VC V0 → 电流 → 电压 → 温度
     *
     * 为什么重排（原来 BEMF 压在 rank4~7、前面还夹着最慢的温度通道）：
     *   温度通道要 160.5 个采样周期（≈5.4µs），排在中间会把 4 路 BEMF 推到触发后
     *   7~11µs；而采样时刻被钉死在"高边开通后 ADC_TRIGGER_CCR4 拍"，台架占空比
     *   17.6% 时导通期只有 7.3µs ⇒ 原来那 4 路正好撞上下降沿的振铃，读数是废的。
     * 现在：
     *   ① 4 路 BEMF 在 rank1~4，触发后 ~4µs 内全部采完（彼此相差 <1µs，可视为同时）；
     *   ② 电流顺延到 ~5.6µs —— 导通期靠后的位置，正是母线采样电阻（U5）该采的时刻。
     * 四路必须"同一状态"：过零判据比较的是【悬空相 − V0】，两个通道得在同一时刻采。
     *
     * 源阻抗（按网表实算）：相通道 = R1∥R2∥R12 = 47k∥10k∥4.7k ≈ 3.0k
     * （早先注释写的 8.3k 漏了 R12 那只 4.7k 到 GND 的电阻）⇒ 19.5 周期绰绰有余，
     * 采样时间保持不动（COMMON_1 是全部通道共用的一个设置）。 */
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, BEMF_ADC_PHASE_A_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_A_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, BEMF_ADC_PHASE_B_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_B_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, BEMF_ADC_PHASE_C_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_C_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_4, BEMF_ADC_NEUTRAL_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_NEUTRAL_CH, LL_ADC_SAMPLINGTIME_COMMON_1);

    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_5, CURRENT_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, CURRENT_ADC_CHANNEL,
        LL_ADC_SAMPLINGTIME_COMMON_1);

    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_6, VOLTAGE_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, VOLTAGE_ADC_CHANNEL,
        LL_ADC_SAMPLINGTIME_COMMON_1);

    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_7,
        LL_ADC_CHANNEL_TEMPSENSOR);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_TEMPSENSOR,
        LL_ADC_SAMPLINGTIME_COMMON_2);
#else
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, CURRENT_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, CURRENT_ADC_CHANNEL,
        LL_ADC_SAMPLINGTIME_COMMON_1);

    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, VOLTAGE_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, VOLTAGE_ADC_CHANNEL,
        LL_ADC_SAMPLINGTIME_COMMON_1);

    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3,
        LL_ADC_CHANNEL_TEMPSENSOR);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_TEMPSENSOR,
        LL_ADC_SAMPLINGTIME_COMMON_2);
#endif
}
