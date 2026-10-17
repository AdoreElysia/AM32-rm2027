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

/* ★ 动态改写 rank1 的通道（换相时由 adc_bemf.c 的 changeCompInput() 调用）。
 *   为什么只改 rank1：rank2 固定是 V0；新悬空相放到最前，判据两路就都排在
 *   rank1/rank2 ⇒ 只需 2 档 40 拍就能采完（详 ADC_DMA_Callback 里的说明）。
 *   改写撞上正在转换的那一帧也不怕：换相后的第一帧本来就被 bemf_blank 丢掉。 */
void adc_set_first_rank(uint32_t channel)
{
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, channel);
}

void ADC_DMA_Callback()
{ // read dma buffer and set extern variables

#ifdef USE_ADC_INPUT
    ADC_raw_temp = ADCDataDMA[3];
    ADC_raw_volts = ADCDataDMA[1] / 2;
    ADC_raw_current = ADCDataDMA[2];
    ADC_raw_input = ADCDataDMA[0];

#else
#ifdef USE_ADC_BEMF
    /* ★★★ 2026-10-10 扫描顺序重排为 5 档：[0]悬空相 [1]V0 [2]电流 [3]电压 [4]温度 ★★★
     *   为什么把「悬空相 + V0」放最前：过零判据只用这两路（见 adc_bemf_get_delta），
     *   而约束是「采样必须整条落在导通期内：CCR4 + 扫描拍数 < duty×2665」。
     *   原来四路 BEMF 排在 rank1~4 ⇒ 要 4×20 = 80 拍才采完 ⇒ 需求 CCR4+80；
     *   现在只用两路 ⇒ 2×20 = 40 拍 ⇒ 需求 CCR4+40（200+40=240）。
     *   ⇒ duty 下限从约 13% 降到约 9%，交棒后那段低占空比（谷底 200 ⇒ 267 拍）
     *     `240 ≤ 267` 也够 ⇒ 不会再"采不到过零 → 闭环靠超时硬踹"。
     *
     *   rank1 的具体通道由 changeCompInput() 在每次换相时动态改写
     *   （A 悬空→CH5、B 悬空→CH4、C 悬空→CH2），这样 [0] 永远是"当前悬空相"。
     *   另两相不再采 —— 判据用不到它们（原来的 va/vb/vc 只有被选中的那个会被读）。
     *   ⚠ 数组下标与 rank 是一体，改顺序必须两处一起改。 */
#if ADC_CAPTURE_MODE
    /* 捕获模式：7 档由下面 SetSequencerRanks 排定 ——
     *   rank1 = ADC_CAP_CH1（默认 V0/PA3）   rank2 = ADC_CAP_CH2（默认 VB/PA4）
     *   rank3 = VC   rank4 = VA   rank5 = 电流   rank6 = 电压   rank7 = 温度
     * ⚠ 这两行把 [0]/[1] 当成 vph/v0，但默认配置下 [0] 是 V0、[1] 是 VB
     *   ⇒ delta = V0 − VB，【符号与正常模式相反】⇒ 判出来的 rise/fall 标签是反的；
     *     而且 rank1 固定为 V0（不随换相改成悬空相）⇒ 只有 B 悬空那 2 步是有效反电势。
     *   本模式下闭环判据整体无意义，只用于看波形。
     *   （2026-10-10 核实：旧注释"rank1=VA … rank4=V0"是 ae28847 改默认抓取通道之前的排法。）*/
    bemf_adc_vph    = ADCDataDMA[0];   /* = V0（默认配置）*/
    bemf_adc_v0     = ADCDataDMA[1];   /* = VB（默认配置）*/
    ADC_raw_current = ADCDataDMA[4];
    ADC_raw_volts   = ADCDataDMA[5];
    ADC_raw_temp    = ADCDataDMA[6];
    {
        extern void adc_capture_sample(uint16_t va, uint16_t vb);
        adc_capture_sample(ADCDataDMA[0], ADCDataDMA[1]);   /* ★ 抓 A、B 两路 */
    }
#else
    /* ★ 2026-10-12 四路电压版：rank1~4 = VA/VB/VC/V0，悬空相【软件选】 */
#if RM2027_ADC_AC_SWAP
    /* ★ rank1=VC / rank3=VA ⇒ 槽位映射反过来，保证 va/vb/vc 仍是"引脚"语义 */
    bemf_adc_vc = ADCDataDMA[0];
    bemf_adc_vb = ADCDataDMA[1];
    bemf_adc_va = ADCDataDMA[2];
#else
    bemf_adc_va = ADCDataDMA[0];
    bemf_adc_vb = ADCDataDMA[1];
    bemf_adc_vc = ADCDataDMA[2];
#endif
    bemf_adc_v0_hw = ADCDataDMA[3];    /* ★ 硬件 V0 原值：永远记它（日志语义不变） */
    bemf_adc_v0    = bemf_adc_v0_hw;
#if RM2027_V0_SOFTMEAN
    /* ★★★ 2026-10-13：V0 改用【三相平均】★★★
     *   原理图上 V0 就是"三只电阻从三相接到一个公共节点" ⇒ 它**本来就应该等于三相平均**。
     *   实测（一切为了校准/03 §六）：把 A/B/C 的增益归一之后，硬件 V0 与三相平均的
     *   **六步均值只差 −1.0 码**（未归一前 −19.1 码）⇒ 两者在均值上已经对上了；
     *   但**逐步还有 ±20 码的摆动**（极差 38 码 = 6%），那不是增益误差、是 V0 节点本身的
     *   逐步失真。取三相平均 ⇒ 这个摆动按定义归零（自参照）。
     *   ⚠ 必须放在上面三相增益校正【之后】（用的是校正后的 va/vb/vc）。
     *   ⚠ 历史教训：`归档/1013-g071-V0软件均值` 上线跑败过（超时 42240、散布 18%），
     *     但那次没有三相增益归一，而且是把多件事一起改的 ⇒ 本轮只改这一件。 */
    bemf_adc_v0 = (uint16_t)(((uint32_t)bemf_adc_va + (uint32_t)bemf_adc_vb
                              + (uint32_t)bemf_adc_vc) / 3u);
#endif
#if RM2027_ADC_GAIN_CAL
    /* ★★★ 2026-10-13 ADC 通道增益归一 ★★★
     *   在这里做（而不是在 adc_bemf_get_delta 里）的理由：这三行是【唯一】的入口，
     *   校正之后 bemf_adc_vph / delta / 窗口载荷 / 所有埋点全自动拿到校正后的值，
     *   不会出现"判据用校正值、记录用原始值"这种半拉子状态。
     *   只乘三相、**不动 V0**（V0 是参考）。系数见 Inc/targets.h 的 RM2027_ADCGC_*。
     *   实现是"乘 + 右移 10"，在 24kHz 的 DMA 中断里只多 3 次乘法，不占采样预算。 */
    bemf_adc_va = (uint16_t)(((uint32_t)bemf_adc_va * RM2027_ADCGC_A) >> 10);
    bemf_adc_vb = (uint16_t)(((uint32_t)bemf_adc_vb * RM2027_ADCGC_B) >> 10);
    bemf_adc_vc = (uint16_t)(((uint32_t)bemf_adc_vc * RM2027_ADCGC_C) >> 10);
#endif
    /* 悬空相映射与 changeCompInput 一致：step 1/4→C, 2/5→A, 3/6→B */
    {
        extern volatile uint8_t step;
        uint8_t st = step;
        if (st == 1u || st == 4u)      { bemf_adc_vph = bemf_adc_vc; }
        else if (st == 2u || st == 5u) { bemf_adc_vph = bemf_adc_va; }
        else                           { bemf_adc_vph = bemf_adc_vb; }
    }
    ADC_raw_current = ADCDataDMA[4];   /* rank5 */
    ADC_raw_volts   = ADCDataDMA[5];   /* rank6 */
    ADC_raw_temp    = ADCDataDMA[6];   /* rank7 */
#endif
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
#if ADC_CAPTURE_MODE
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, 7);
#else
    /* 7 档：VA/VB/VC/V0 / 电流 / 电压 / 温度（与 rank 顺序一体，改一处必须改另一处） */
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, 7);
#endif
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
#if ADC_CAPTURE_MODE
    /* 捕获模式：7 档 VA/VB/VC/V0/电流/电压/温度 */
    ADC_REG_InitStruct.SequencerLength = LL_ADC_REG_SEQ_SCAN_ENABLE_7RANKS;
#else
    /* ★ 2026-10-12 四路电压版：7 档 VA/VB/VC/V0 / 电流 / 电压 / 温度
     *   （原来 5 档是"悬空相+V0+电流+电压+温度"，rank 顺序改了这里必须跟着改）*/
    ADC_REG_InitStruct.SequencerLength = LL_ADC_REG_SEQ_SCAN_ENABLE_7RANKS;
#endif
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
    /* ★★ COMMON_1 的采样时间是"BEMF 四路能不能塞进导通期"的直接决定项（2026-10-07 实测）：
     *   19.5 周期时实测四路 BEMF（rank1~4）要 ~3.6µs ≈ 230 拍，判据是
     *       CCR4 + 230 < 导通期长度(= duty×ARR)
     *   ⇒ 占空比低于约 (CCR4+230)/2665 ≈ 14% 时，扫描尾巴（VC/V0）就落进关断期：
     *     导通期一结束，被驱动的两相被拉低、星点塌下来，于是悬空相和 V0 都读成 0
     *     ⇒ delta = 悬空相 − V0 ≈ 0 ⇒ 永远判不出过零 ⇒ 闭环锁不上。
     *     实测证据（CCR4=150、duty 17.6% 时）：vb(先采) 能读到 1053≈VBAT，
     *     而 vc、v0（后采）恒为 0；把触发提前到 CCR4=60 后立刻自洽：
     *     vb=717 vc=1051 v0=588 ⇒ (4+717+1051)/3 = 590 ≈ v0 ✓ ⇒ 采样正确。
     *   改小到 7.5 周期 ⇒ 四路 BEMF 只需 ~2.5µs（160 拍），低占空比余量大幅改善。
     *   7.5 周期 @32MHz = 234ns；相采样网络源阻抗 ~3.0k、采样电容 ~5pF
     *   ⇒ τ≈15ns ⇒ 234ns ≈ 15τ（12 位只需 ~8.3τ）⇒ 精度足够。
     *   （COMMON_1 也管电流/电压通道：电压分压源阻 ~4.3k ⇒ τ≈21ns ⇒ 11τ ✓；
     *     温度通道在 COMMON_2（160.5 周期）不受影响。） */
    /* ★ 2026-10-13 加长采样时间：改的是【SMP1 字段的值】，不是通道选择位。
     *   ⚠ LL 的语义坑（上一版就栽在这）：SetSamplingTimeCommonChannels(COMMON_1, <值>)
     *     才是写采样时间；SetChannelSamplingTime(ch, <值>) 只写"该通道选 SMP1 还是 SMP2"
     *     的选择位（掩码 (ch<<8) & 0x1F00），把时间值传进去会被掩成 0 ⇒ 空操作。
     *     判定方法：读 ADC1->SMPR（0x40012414），SMP1[2:0]/SMP2[6:4] 才是时间，
     *     SMPSEL[21:8] 是选择位。加长前读回 0x00100072 = SMP1 7.5 / SMP2 160.5(温度)。 */
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1,
        RM2027_ADC_SMP_BEMF);
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
#if ADC_CAPTURE_MODE
    /* ★ 捕获模式：固定 7 档。rank1/rank2 = 用户要抓的那两路（ADC_CAP_CH1/CH2），
     *   其余档用来占位（保持 VBAT/电流在固定位置，免得别的逻辑读到垃圾）★ */
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, ADC_CAP_CH1);
    LL_ADC_SetChannelSamplingTime(ADC1, ADC_CAP_CH1, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, ADC_CAP_CH2);
    LL_ADC_SetChannelSamplingTime(ADC1, ADC_CAP_CH2, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, BEMF_ADC_PHASE_C_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_C_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_4, BEMF_ADC_PHASE_A_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_A_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_5, CURRENT_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, CURRENT_ADC_CHANNEL, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_6, VOLTAGE_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, VOLTAGE_ADC_CHANNEL, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_7, LL_ADC_CHANNEL_TEMPSENSOR);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_TEMPSENSOR, LL_ADC_SAMPLINGTIME_COMMON_2);
#else
    /* ★★★ 2026-10-12 【四路电压版】rank1~4 = VA / VB / VC / V0 ★★★
     *   为什么改：诊断要"同一帧看见三相 + 中性点"才能判
     *     ① 采样到底坏没坏、② V0 ≈ (VA+VB+VC)/3 是否成立（代码里定的自检）、
     *     ③ "悬空相"选对了没有。
     *   代价（必须记住）：BEMF 从 2 档 40 拍变回 4 档 80 拍 ⇒ 采样需求
     *     CCR4(200)+80 = 280 拍 ⇒ duty 下限由约 9% 回到约 10.5%
     *     （约束见 targets.h：CCR4 + 扫描拍数 < duty/2000×2665）。
     *   delta 不再依赖"动态改写 rank1"，改由 ADC_DMA_Callback 按 step 在软件里选悬空相。 */
#if ADC_STATIC_TEST == 1
    /* ★ 静态模式 1：同一个母线电压通道复制到 rank1/2/3/4/6，测试顺序/settling。
     *   rank5=电流、rank7=温度；真值相同，差异只来自 ADC 采样顺序/建立。 */
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, VOLTAGE_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, VOLTAGE_ADC_CHANNEL, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, VOLTAGE_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, VOLTAGE_ADC_CHANNEL, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, VOLTAGE_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, VOLTAGE_ADC_CHANNEL, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_4, VOLTAGE_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, VOLTAGE_ADC_CHANNEL, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_5, CURRENT_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, CURRENT_ADC_CHANNEL, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_6, VOLTAGE_ADC_CHANNEL);
    LL_ADC_SetChannelSamplingTime(ADC1, VOLTAGE_ADC_CHANNEL, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_7, LL_ADC_CHANNEL_TEMPSENSOR);
    LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_TEMPSENSOR, LL_ADC_SAMPLINGTIME_COMMON_2);
#elif ADC_STATIC_TEST == 2
    /* ★ 静态模式 2：同一相连续采两次（VA/VA, VB/VB, VC/VC），rank7=V0。
     *   用来测“第二次采样是否明显更稳 / 建立不足”。 */
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, BEMF_ADC_PHASE_A_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_A_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, BEMF_ADC_PHASE_A_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_A_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, BEMF_ADC_PHASE_B_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_B_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_4, BEMF_ADC_PHASE_B_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_B_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_5, BEMF_ADC_PHASE_C_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_C_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_6, BEMF_ADC_PHASE_C_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_C_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_7, BEMF_ADC_NEUTRAL_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_NEUTRAL_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
#else
#if RM2027_ADC_AC_SWAP
    /* ★ 采样顺序交换校准：rank1 与 rank3 对调（rank1=VC、rank3=VA）。见 targets.h 的说明。*/
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, BEMF_ADC_PHASE_C_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_C_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, BEMF_ADC_PHASE_B_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_B_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, BEMF_ADC_PHASE_A_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_A_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
#else
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, BEMF_ADC_PHASE_A_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_A_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, BEMF_ADC_PHASE_B_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_B_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_3, BEMF_ADC_PHASE_C_CH);
    LL_ADC_SetChannelSamplingTime(ADC1, BEMF_ADC_PHASE_C_CH, LL_ADC_SAMPLINGTIME_COMMON_1);
#endif
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
#endif
#endif
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
