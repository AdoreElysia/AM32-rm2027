
/* AM32- multi-purpose brushless controller firmware for the stm32f051 */

//===========================================================================
//=============================== Changelog =================================
//===========================================================================
/*
 * 1.54 Changelog;
 * --Added firmware name to targets and firmware version to main
 * --added two more dshot to beacons 1-3 currently working
 * --added KV option to firmware, low rpm power protection is based on KV
 * --start power now controls minimum idle power as well as startup strength.
 * --change default timing to 22.5
 * --Lowered default minimum idle setting to 1.5 percent duty cycle, slider
range from 1-2.
 * --Added dshot commands to save settings and reset ESC.
 *
 *1.56 Changelog.
 * -- added check to stall protection to wait until after 40 zero crosses to fix
high startup throttle hiccup.
 * -- added TIMER 1 update interrupt and PWM changes are done once per pwm
period
 * -- reduce commutation interval averaging length
 * -- reduce false positive filter level to 2 and eliminate threshold where
filter is stopped.
 * -- disable interrupt before sounds
 * -- disable TIM1 interrupt during stepper sinusoidal mode
 * -- add 28us delay for dshot300
 * -- report 0 rpm until the first 10 successful steps.
 * -- move serial ADC telemetry calculations and desync check to 10Khz
interrupt.
 *
 * 1.57
 * -- remove spurious commutations and rpm data at startup by polling for longer
interval on startup
 *
 * 1.58
 * -- move signal timeout to 10khz routine and set armed timeout to one quarter
second 2500 / 10000
 * 1.59
 * -- moved comp order definitions to target.h
 * -- fixed update version number if older than new version
 * -- cleanup, moved all input and output to IO.c
 * -- moved comparator functions to comparator.c
 * -- removed ALOT of useless variables
 * -- added siskin target
 * -- moved pwm changes to 10khz routine
 * -- moved basic functions to functions.c
 * -- moved peripherals setup to periherals.c
 * -- added crawler mode settings
 *
 * 1.60
 * -- added sine mode hysteresis
 * -- increased power in stall protection and lowered start rpm for crawlers
 * -- removed onehot125 from crawler mode
 * -- reduced maximum startup power from 400 to 350
 * -- change minimum duty cycle to DEAD_TIME
 * -- version and name moved to permanent spot in FLASH memory, thanks mikeller
 *
 * 1.61
 * -- moved duty cycle calculation to 10khz and added max change option.
 * -- decreased maximum interval change to 25%
 * -- reduce wait time on fast acceleration (fast_accel)
 * -- added check in interrupt for early zero cross
 *
 * 1.62
 * --moved control to 10khz loop
 * --changed condition for low rpm filter for duty cycle from || to &&
 * --introduced max deceleration and set it to 20ms to go from 100 to 0
 * --added configurable servo throttle ranges
 *
 *
 *1.63
 *-- increase time for zero cross error detection below 250us commutation
interval
 *-- increase max change a low rpm x10
 *-- set low limit of throttle ramp to a lower point and increase upper range
 *-- change desync event from full restart to just lower throttle.

 *1.64
 * --added startup check for continuous high signal, reboot to enter bootloader.
 *-- added brake on stop from eeprom
 *-- added stall protection from eeprom
 *-- added motor pole divider for sinusoidal and low rpm power protection
 *-- fixed dshot commands, added confirmation beeps and removed blocking
behavior
 *--
 *1.65
 *-- Added 32 millisecond telemetry output
 *-- added low voltage cutoff , divider value and cutoff voltage needs to be
added to eeprom
 *-- added beep to indicate cell count if low voltage active
 *-- added current reading on pa3 , conversion factor needs to be added to
eeprom
 *-- fixed servo input capture to only read positive pulse to handle higher
refresh rates.
 *-- disabled oneshot 125.
 *-- extended servo range to match full output range of receivers
 *-- added RC CAR style reverse, proportional brake on first reverse , double
tap to change direction
 *-- added brushed motor control mode
 *-- added settings to EEPROM version 1
 *-- add gimbal control option.
 *--
 *1.66
 *-- move idwg init to after input tune
 *-- remove reset after save command -- dshot
 *-- added wraith32 target
 *-- added average pulse check for signal detection
 *--
 *1.67
 *-- Rework file structure for multiple MCU support
 *-- Add g071 mcu
 *--
 *1.68
 *--increased allowed average pulse length to avoid double startup
 *1.69
 *--removed line re-enabling comparator after disabling.
 *1.70 fix dshot for Kiss FC
 *1.71 fix dshot for Ardupilot / Px4 FC
 *1.72 Fix telemetry output and add 1 second arming.
 *1.73 Fix false arming if no signal. Remove low rpm throttle protection below
300kv *1.74 Add Sine Mode range and drake brake strength adjustment *1.75
Disable brake on stop for PWM_ENABLE_BRIDGE Removed automatic brake on stop on
neutral for RC car proportional brake. Adjust sine speed and stall protection
speed to more closely match makefile fixes from Cruwaller Removed gd32 build,
until firmware is functional *1.76 Adjust g071 PWM frequency, and startup power
to be same frequency as f051. Reduce number of polling back emf checks for g071
 *1.77 increase PWM frequency range to 8-48khz
 *1.78 Fix bluejay tunes frequency and speed.
           Fix g071 Dead time
           Increment eeprom version
 *1.79 Add stick throttle calibration routine
           Add variable for telemetry interval
 *1.80 -Enable Comparator blanking for g071 on timer 1 channel 4
           -add hardware group F for Iflight Blitz
           -adjust parameters for pwm frequency
           -add sine mode power variable and eeprom setting
           -fix telemetry rpm during sine mode
           -fix sounds for extended pwm range
           -Add adjustable braking strength when driving
 *1.81 -Add current limiting PID loop
           -fix current sense scale
           -Increase brake power on maximum reverse ( car mode only)
           -Add HK and Blpwr targets
           -Change low kv motor throttle limit
           -add reverse speed threshold changeover based on motor kv
           -doubled filter length for motors under 900kv
*1.82  -Add speed control pid loop.
*1.83  -Add stall protection pid loop.
           -Improve sine mode transition.
           -decrease speed step re-entering sine mode
           -added fixed duty cycle and speed mode build option
           -added rpm_controlled by input signal ( to be added to config tool )
*1.84  -Change PID value to int for faster calculations
           -Enable two channel brushed motor control for dual motors
           -Add current limit max duty cycle
*1.85  -fix current limit not allowing full rpm on g071 or low pwm frequency
                -remove unused brake on stop conditional
*1.86  - create do-once in sine mode instead of setting pwm mode each time.
*1.87  - fix fixed mode max rpm limits
*1.88  - Fix stutter on sine mode re-entry due to position reset
*1.89  - Fix drive by rpm mode scaling.
           - Fix dshot px4 timings
*1.90  - Disable comp interrupts for brushed mode
           - Re-enter polling mode after prop strike or desync
           - add G071 "N" variant
           - add preliminary Extended Dshot
*1.91  - Reset average interval time on desync only after 100 zero crosses
*1.92  - Move g071 comparator blanking to TIM1 OC5
           - Increase ADC read frequency and current sense filtering
           - Add addressable LED strip for G071 targets
*1.93  - Optimization for build process
       - Add firmware file name to each target hex file
       -fix extended telemetry not activating dshot600
       -fix low voltage cuttoff timeout
*1.94  - Add selectable input types
*1.95  - reduce timeout to 0.5 seconds when armed
*1.96  - Improved erpm accuracy dshot and serial telemetry, thanks Dj-Uran
             - Fix PID loop integral.
                 - add overcurrent low voltage cuttoff to brushed mode.
*1.97    - enable input pullup
*1.98    - Dshot erpm rounding compensation.
*1.99    - Add max duty cycle change to individual targets ( will later become
an settings option)
                 - Fix dshot telemetry delay f4 and e230 mcu
*2.00    - Cleanup of target structure
*2.01    - Increase 10khztimer to 20khz, increase max duty cycle change.
*2.02	 - Increase startup power for inverted output targets.
*2.03    - Move chime from dshot direction change commands to save command.
*2.04    - Fix current protection, max duty cycle not increasing
                 - Fix double startup chime
                 - Change current averaging method for more precision
                 - Fix startup ramp speed adjustment
*2.05		 - Fix ramp tied to input frequency
*2.06    - fix input pullups
         - Remove half xfer insterrupt from servo routine
                                 - update running brake and brake on stop
*2.07    - Dead time change f4a
*2.08		 - Move zero crosss timing
*2.09    - filter out short zero crosses
*2.10    - Polling only below commutation intverval of 1500-2000us
				 - fix tune frequency again
*2.11    - RC-Car mode fix
*2.12    - Reduce Advance on hard braking
*2.13    - Remove Input capture filter for dshot2400
         - Change dshot 300 speed detection threshold 
*2.14    - Reduce G071 zero cross checks
         - Assign all mcu's duty cycle resolution 2000 steps
*2.15    - Enforce 1/2 commutation interval as minimum for g071
         - Revert timing change on braking
				 - Add per target over-ride option to max duty cycle change.
				 - todo fix signal detection
*2.16    - add L431 
				 - add variable auto timing
				 - add droneCAN
*/
#include "main.h"
#include "ADC.h"
#include "IO.h"
#include "common.h"
#include "comparator.h"
#ifdef USE_ADC_BEMF
#include "adc_bemf.h"   /* 软件过零：起步链在主循环里直接读 adc_bemf_get_delta() */
#include "adc_zc.h"     /* ★ 环形缓冲版过零检测器：交棒处要触发闭环波形捕获 */
#endif
#include "dshot.h"
#include "eeprom.h"
#include "esc_cal.h"
#include "functions.h"
#include "peripherals.h"
#include "phaseouts.h"
#include "serial_telemetry.h"
#include "kiss_telemetry.h"
#include "signal.h"
#include "sounds.h"
#include "targets.h"
#include <stdint.h>
#include <string.h>
#include <assert.h>

#ifndef NXP
#ifdef USE_LED_STRIP
#include "WS2812.h"
#endif
#endif

#ifdef USE_CRSF_INPUT
#include "crsf.h"
#endif

#if DRONECAN_SUPPORT
#include "DroneCAN/DroneCAN.h"
#endif

#include "version.h"

void zcfoundroutine(void);
void commutate(void);
extern volatile uint16_t duty_cycle;
/* 闭环换相的时间量（定义在后面，这里前置声明：olStartupTick 交棒时要用） */
extern volatile uint16_t lastzctime;
extern volatile uint16_t thiszctime;
extern volatile uint16_t waitTime;

/* RM2027 开环调试开关：未定义时按 0 处理，这样既能用 #ifdef，也能直接写进 if 条件里 */
#ifndef RM2027_OL_ARM_ONLY
#define RM2027_OL_ARM_ONLY 0
#endif

// firmware build options !! fixed speed and duty cycle modes are not to be used
// with sinusoidal startup !!

//#define FIXED_DUTY_MODE  // bypasses signal input and arming, uses a set duty
// cycle. For pumps, slot cars etc 
//#define FIXED_DUTY_MODE_POWER 100     //
// 0-100 percent not used in fixed speed mode

// #define FIXED_SPEED_MODE  // bypasses input signal and runs at a fixed rpm
// using the speed control loop PID 
//#define FIXED_SPEED_MODE_RPM  1000  //
// intended final rpm , ensure pole pair numbers are entered correctly in config
// tool.

// #define BRUSHED_MODE         // overrides all brushless config settings,
// enables two channels for brushed control 
//#define GIMBAL_MODE     // also
// sinusoidal_startup needs to be on, maps input to sinusoidal angle.

//===========================================================================
//=============================  Defaults =============================
//===========================================================================

uint8_t drive_by_rpm = 0;
uint32_t MAXIMUM_RPM_SPEED_CONTROL = 10000;
uint32_t MINIMUM_RPM_SPEED_CONTROL = 1000;

// assign speed control PID values values are x10000
/* ★★★ 2026-10-11 速度环参数（"改全"的一部分）★★★
 *   这段是外环：参考 = 油门映射出的目标换相周期，实际 = e_com_time，
 *   PID 输出累加进 input_override（占空比），见 main.c 的
 *       input_override += doPidCalculations(&speedPid, e_com_time, target_e_com_time);
 *   注意它是【累加】形式 ⇒ Kp 本身起的就是积分作用（Ki 保持 0 是对的）。
 *   改动：
 *     · Kd 100 -> 0：误差是带噪的 µs 量，微分没有收益、只会把抖动注入占空比。
 *     · output_limit 50000 -> 3000：每次调用最多改 0.3 个油门刻度
 *       （3000/10000）⇒ 主循环约 17kHz ⇒ 满量程约 0.4 秒 ⇒ 够快也不振荡。
 *       （原来 50000 折算每次 5 个刻度 ⇒ 满量程 20ms ⇒ 太猛、易振荡。） */
fastPID speedPid = { // commutation speed loop time
    .Kp = 10,
    .Ki = 0,
    .Kd = 0,
    .integral_limit = 10000,
    .output_limit = 3000
};

fastPID currentPid = { // 1khz loop time
    .Kp = 400,
    .Ki = 0,
    .Kd = 1000,
    .integral_limit = 20000,
    .output_limit = 100000
};

fastPID stallPid = { // 1khz loop time
    .Kp = 1,
    .Ki = 0,
    .Kd = 50,
    .integral_limit = 10000,
    .output_limit = 50000
};

EEprom_t eepromBuffer;
volatile uint32_t polling_mode_changeover;
volatile uint8_t ramp_divider;
volatile uint8_t max_ramp_startup = RAMP_SPEED_STARTUP;
volatile uint8_t max_ramp_low_rpm = RAMP_SPEED_LOW_RPM;
volatile uint8_t max_ramp_high_rpm = RAMP_SPEED_HIGH_RPM;
char send_esc_info_flag;
uint32_t eeprom_address = EEPROM_START_ADD; 
uint16_t prop_brake_duty_cycle = 0;
uint16_t ledcounter = 0;
uint16_t ramp_count;
uint32_t process_time = 0;
uint32_t start_process = 0;
uint16_t one_khz_loop_counter = 0;
uint16_t target_e_com_time_high;
uint16_t target_e_com_time_low;
volatile uint8_t compute_dshot_flag = 0;
uint8_t crsf_input_channel = 1;
uint8_t crsf_output_PWM_channel = 2;
uint8_t telemetry_interval_ms = 30;
uint8_t temp_advance;
uint16_t motor_kv = 2000;
uint8_t dead_time_override = DEAD_TIME;
uint16_t stall_protect_target_interval = TARGET_STALL_PROTECTION_INTERVAL;
uint16_t enter_sine_angle = 180;
char do_once_sinemode = 0;
uint8_t auto_advance_level;
volatile uint8_t zero_throttle_brake_active;
volatile uint8_t temp_comp_pwm;
uint8_t brake_countdown;

//============================= Servo Settings ==============================
uint16_t servo_low_threshold = 1100; // anything below this point considered 0
uint16_t servo_high_threshold = 1900; // anything above this point considered 2000 (max)
uint16_t servo_neutral = 1500;
uint8_t servo_dead_band = 100;

//========================= Battery Cuttoff Settings ========================
char LOW_VOLTAGE_CUTOFF = 0; // Turn Low Voltage CUTOFF on or off
uint16_t low_cell_volt_cutoff = 330; // 3.3volts per cell

//=========================== END EEPROM Defaults ===========================

const char filename[30] AM32_FLASH_SECTION(".file_name") = FILE_NAME;
_Static_assert(sizeof(FIRMWARE_NAME) <=13,"Firmware name too long");   // max 12 character firmware name plus NULL 

// move these to targets folder or peripherals for each mcu
uint16_t ADC_CCR = 30;
uint16_t current_angle = 90;
uint16_t desired_angle = 90;
char return_to_center = 0;
uint16_t target_e_com_time = 0;
int16_t Speed_pid_output;
char use_speed_control_loop = 0;
int32_t input_override = 0;
int16_t use_current_limit_adjust = 2000;
char use_current_limit = 0;
int32_t stall_protection_adjust = 0;
uint32_t MCU_Id = 0;
uint32_t REV_Id = 0;

uint16_t armed_timeout_count;
uint16_t reverse_speed_threshold = 1500;
#if DRONECAN_SUPPORT
uint32_t desync_happened = 0;
#else
uint8_t desync_happened = 0;
#endif
char maximum_throttle_change_ramp = 1;

char crawler_mode = 0; // no longer used //
uint16_t velocity_count = 0;
uint16_t velocity_count_threshold = 75;

char low_rpm_throttle_limit = 1;

uint16_t low_voltage_count = 0;
uint16_t telem_ms_count;

uint16_t VOLTAGE_DIVIDER = TARGET_VOLTAGE_DIVIDER; // 100k upper and 10k lower resistor in divider
uint16_t
    battery_voltage; // scale in volts * 10.  1260 is a battery voltage of 12.60
char cell_count = 0;
char brushed_direction_set = 0;

uint16_t tenkhzcounter = 0;
int32_t consumed_current = 0;
int32_t smoothed_raw_current = 0;
int16_t actual_current = 0;

char lowkv = 0;

uint16_t min_startup_duty = 120;
uint16_t sin_mode_min_s_d = 120;
char bemf_timeout = 10;

char startup_boost = 50;
char reversing_dead_band = 1;

uint16_t low_pin_count = 0;

uint8_t max_duty_cycle_change = 2;
char fast_accel = 1;
char fast_deccel = 0;
uint16_t last_duty_cycle = 0;
uint16_t duty_cycle_setpoint = 0;
char play_tone_flag = 0;

typedef enum { GPIO_PIN_RESET = 0U,
    GPIO_PIN_SET } GPIO_PinState;

uint16_t startup_max_duty_cycle = 200;
uint16_t minimum_duty_cycle = DEAD_TIME;
uint16_t stall_protect_minimum_duty = DEAD_TIME;
char desync_check = 0;
char low_kv_filter_level = 20;

uint16_t tim1_arr = TIM1_AUTORELOAD; // current auto reset value
uint16_t TIMER1_MAX_ARR = TIM1_AUTORELOAD; // maximum auto reset register value
uint16_t duty_cycle_maximum = 2000; // restricted by temperature or low rpm throttle protect
uint16_t low_rpm_level = 20; // thousand erpm used to set range for throttle resrictions
uint16_t high_rpm_level = 70; //
uint16_t throttle_max_at_low_rpm = 400;
uint16_t throttle_max_at_high_rpm = 2000;

uint16_t commutation_intervals[6] = { 0 };

/* ★★ 2026-10-11 实验埋点：闭环换相历史（诊断"越转越慢"）★★
 *   每次 commutate() 记一笔，按【时间顺序】存（只记一次即停，不循环覆盖）：
 *     ccm_ci[]   = 换相后的 commutation_interval（六步平均的实测步长，1 tick = 0.5us）
 *     ccm_wait[] = waitTime（换相后要等的 30 度延迟，= ci/2 - advance）
 *     ccm_step[] = 换相后的 step（1..6；若出现跳号/回退 ⇒ 换相乱套）
 *   1024 笔 x 2.64ms 约覆盖 2.7 秒；把 ccm_ci 逐笔累加就是真实时间轴。
 *   只写 RAM，不改变任何控制行为；ccm_idx 不清零 ⇒ 跨越"过流停机→重启"也能连起来看。 */
/* ★★ 2026-10-11 闭环速度环的油门上限（防累加漂移）★★
 *   实测依据：开环用 340（约 17% 占空比）就稳定跑到 1670 转/分、电流 0.6A；
 *   而闭环速度环是【增量累加】结构（input_override += error*Kp，Ki=Kd=0），
 *   只要'实际转速比目标慢一点'就每拍往上加、没有停止条件 ⇒ 实测 4 秒内
 *   duty_cycle_setpoint 从 333 漂到 505（+52%）、峰值电流触到 110（= 1.8A 保护线）
 *   ⇒ 再跑几秒必被掐断（长跑'掉速重启'的来源）。
 *   既然开环 17% 就够，就不允许闭环油门漂过 450（约 20% 占空比）。 */
#ifndef RM2027_CL_INPUT_CAP
#define RM2027_CL_INPUT_CAP 450
#endif

#define CCM_N 256          /* ★ 1024->256：腾 RAM 给窗口式采样 */
volatile uint16_t adc_frame_cnt;      /* 24kHz ADC 帧计数（ISR 自增）*/

/* ★★ 2026-10-11 过零时刻前移量（运行时可用 SWD 改，便于扫参数）★★
 *   用户要求"接着把偏置拉大" —— 不重编译也能扫：
 *       mwh <本变量地址> <拍数>        （1 帧 = 83.3 拍，0 = 不前移）
 *   含义：把上报的过零时刻往前挪 ⇒ 缩短"过零→换相"的等待 ⇒ 换相提前。
 *   ⚠ 风险（本分支实测）：超前角过大转矩变差 —— 提前约 12 度时曾实测转速
 *     从 2222 塌到 390。扫的时候要盯"转速是否反而下降"。
 *   默认 84（= 1 帧，即之前那一版）。 */
/* ★ 实测定值：扫过 0/84/168/336 拍 ⇒ 前移越多转速越低（168/336 拍时稳态
 *   只剩 643 转/分，而 0 拍时稳态中位 1.000ms）。原因是前移 = 换相提前 =
 *   相位超前 ⇒ 转矩下降。所以定为 0（不前移）。 */
volatile uint16_t bemf_zc_lead_ticks = 0;

/* ★★ 2026-10-11 波形捕获的触发时刻做成可设定（用户要求看不同时段的全速采样）★★
 *   原来只抓"交棒那一刻"。要对比"塌陷前 vs 塌陷后"的波形，必须能指定时刻：
 *       mww <zcw_trig_delay_frames 地址> <帧数>     1 帧 = 41.67us
 *         e.g. 1500ms => 36000 帧 ； 5000ms => 120000 帧
 *       mwb <zcw_arm_st 地址> 0                    跑下一轮前清零
 *       mwb <zcw_state 地址> 0                     同上（连同冻结状态一起清）
 *   触发后 21ms（ZCW_WIN=2 窗）的原始 delta 会被冻结，跑完用 SWD 读。 */
volatile uint32_t zcw_trig_delay_frames = 36000;   /* 默认交棒后 1500ms */
volatile uint32_t zcw_arm_frame = 0;
volatile uint8_t  zcw_arm_st = 0;
static   uint16_t ccm_last_frame;     /* 上一次换相时的帧计数 */
volatile uint16_t ccm_ci[CCM_N];      /* 最近 1024 笔：真实换相间隔（帧，41.67us/帧）*/
volatile uint8_t  ccm_step[CCM_N];
/* ★★★ 2026-10-11 串口【换相事件流】（分支 1011）★★★
 *   目的：芯片只有 16KB RAM，内存记录只能存 1 秒多，看不到"跑久了会不会变慢"。
 *   做法（用户提的方案）：中断里只把事件写进内存环形缓冲；主循环攒够一批再用
 *   DMA 一口气发出去 —— 采集与发送在时间上完全错开，采到的数据永不被自己干扰。
 *   帧格式（18 字节，小端，与 PC 端 evt-dump.py 一致）：
 *     AA 55 | seq u16 | ts u32 | delta i16 | step u8 | wait u16 | duty u16 | cur i16 | chk u8
 *   带宽：每次换相 18 字节、约 1000 次/秒 => 约 18KB/s，1Mbaud 下很轻松。 */
#define EVT_FRAME   49           /* ★ 2026-10-12 37->49：再加 6 个"影响输出"的量
                                  *   [19..20] input_override/10000（速度环输出）
                                  *   [21..22] startup_max_duty_cycle（AM32 起步天花板）
                                  *   [23..24] use_current_limit_adjust（电流环夹子）
                                  *   [25..26] commutation_interval（ci，拍）
                                  *   [27] 位0=阶段(1开环/0闭环) 位1=电流环使能
                                  *   [28..29] thiszctime（本次过零量到的间隔）
                                  *   [30..31] lastzctime
                                  *   [32..33] average_interval(低16位)
                                  *   [34..35] advance（相位前移量）
                                  *   [36..37] duty_cycle_setpoint（夹完之后最终设定值）
                                  *   [38..39] duty_cycle_maximum（第三个夹子）
                                  *   [40..41] bemf_timeout_happened（超时兜底计数）
                                  *   [42..43] adc_zc_n_reject（过零被拒计数,低16）
                                  *   [44..45] input（速度环输入）
                                  *   [46..47] stall_protection_adjust(低16)
                                  *   [48] crc8(前 48 字节) */
/* ★★ xien/n2b：事件环形缓冲 1024 -> 4096 字节 ★★
 *   窗口包 6156 B 一次性交给 DMA 期间，事件只能进这个环；1024 B 只装 20 个事件，
 *   1 Mbaud 下发一窗 61.6ms @1000 次/s ≈ 62 个事件 ⇒ 每发一窗丢 ~40 个。
 *   4096 B = 83 个事件 ≈ 83ms，覆盖 2 Mbaud 的一次窗口发送（30.8ms）绰绰有余。
 *   ⚠ RAM +3KB（本芯片 16KB，注意 targets.h 里 64KB 宏的坑）。 */
#define EVT_BUF_N   512    /* 事件流本分支已关，缓冲 4096->512 省 3.5KB */
#define EVT_BATCH   200          /* 攒 10 个事件发一次 */
volatile uint8_t  evt_buf[EVT_BUF_N];
volatile uint16_t evt_head;
volatile uint16_t evt_tail;
volatile uint32_t evt_total;     /* 打包过的事件数 */
volatile uint32_t evt_drop;      /* 缓冲满而丢弃的 */
/* ★ xien/n1 试验：事件流总开关（运行时 SWD 可写）
 *   0 = 完全不组事件包、不发事件帧（只看窗口流/卡顿）；
 *   1 = 恢复原有 49 B/换相事件流。
 *   编译默认值来自 targets.h 的 RM2027_EVT_STREAM（本分支=0）。 */
#ifndef RM2027_EVT_STREAM
#define RM2027_EVT_STREAM 1
#endif
volatile uint8_t  evt_enable = RM2027_EVT_STREAM;
volatile uint8_t  evt_tx[EVT_BATCH];

/* ★ 2026-10-20 两轮标定产物快照（供 SWD 一次读；v1=第一轮开环稳定段后，v2=第二轮）*/
volatile int16_t  v1_sg[7], v1_so[7], v1_fg[7], v1_fo[7];
volatile int16_t  v2_sg[7], v2_so[7], v2_fg[7], v2_fo[7];
volatile uint16_t v1_rkn[7][4], v2_rkn[7][4];
volatile int32_t  v1_rks[7][4], v2_rks[7][4];
volatile int16_t  fin_erpm, fin_duty, fin_cur;
volatile int16_t  v1_rank_g[4], v2_rank_g[4];
volatile int16_t  cl_duty_match;   /* ★ 第一次闭环稳态 duty，第二轮开环直接用它 */
volatile uint8_t  v1_ok, v2_ok, cal_snap_req;
volatile uint32_t adc_frame_total;   /* 24kHz 总帧数（u32，不会回绕）*/

/* ★ 2026-10-12 延时交棒计时：给编译期宏 RM2027_OL_RUN_MS 用
 *   ms10k_cnt 由 10kHz 中断自增（1 拍 = 0.1ms）；
 *   ol_hold_ms = 斜坡跑完那一刻的读数；两者之差就是"已纯开环跑了多少 0.1ms" */
volatile uint32_t ms10k_cnt = 0;
volatile uint32_t ol_hold_ms = 0;
volatile uint32_t ol_openloop_ms = 0;   /* 观测：本次纯开环已跑毫秒数 */   /* 24kHz 总帧数（u32，不会回绕）*/

volatile uint16_t ccm_head;           /* 下一个写入位置 */

/* ★★ 2026-10-11 诊断"卡顿"：每次【超时硬踹】时记下当时的占空比/电流/等了多久 ★★
 *   用户假说：低惯量转子对换相节奏不均极敏感，某个力矩缺口落在齿槽坡上就把
 *   转子抓住（图上卡 18ms、delta 平坦 = 转子几乎没转）。
 *   要区分两种可能、而且修法完全相反：
 *     · 电流大  ⇒ 线圈在使劲而转子不动 = 真堵转 ⇒ 低惯量/齿槽假说成立；
 *     · 电流小  ⇒ 驱动其实被切掉了 = 换相逻辑问题 ⇒ 与惯量无关。
 *   记录点：闭环超时兜底分支（强制换相那一处）之前。 */
#define TM_N 64
volatile uint16_t tm_duty[TM_N];   /* 卡住时的 duty_cycle（0..2000）*/
volatile uint16_t tm_cur[TM_N];    /* 卡住时的 actual_current */
volatile uint16_t tm_cnt[TM_N];    /* 卡住时 INTERVAL_TIMER_COUNT（等了多久）*/
volatile uint8_t  tm_idx;
volatile uint32_t tm_total;
volatile uint32_t ccm_total;          /* 累计笔数（>1024 说明已绕过一圈）*/

/* 粗时间线：每 64 笔记一次 ⇒ 128 笔覆盖 128x64 次换相。
 * 闭环约 1.3ms/步 ⇒ 约 10 秒，足够覆盖一整轮台架运行（含过流停机重启）。
 * 细记录只看得到"最后 1.3 秒"，这张粗表才看得到"整轮"有没有变慢。 */
#define CCM_SLOW_N   32   /* ★ 128->32：同上 */
#define CCM_SLOW_DIV 64
volatile uint16_t ccm_slow_ci[CCM_SLOW_N];
volatile uint16_t ccm_slow_duty[CCM_SLOW_N];  /* 该点占空比（0..2000，2000 = 100%）*/
volatile uint16_t ccm_slow_cur[CCM_SLOW_N];   /* 该点 actual_current（电调内部单位）*/
volatile uint32_t ccm_slow_iov[CCM_SLOW_N];   /* 该点速度环累加量 input_override */
volatile uint16_t ccm_slow_in[CCM_SLOW_N];    /* 该点 input（= iov/10000，速度环输出）*/
volatile uint16_t ccm_slow_sp[CCM_SLOW_N];    /* 该点 duty_cycle_setpoint（各上限截断前）*/
volatile uint16_t ccm_slow_head;
volatile uint32_t ccm_slow_total;
volatile uint32_t average_interval = 0;
uint32_t last_average_interval;
int e_com_time;

uint16_t ADC_smoothed_input = 0;
volatile int16_t degrees_celsius;
int16_t converted_degrees;
uint8_t temperature_offset;
#ifdef NXP	// raw temperature uses two 16-bit values
uint16_t ADC_raw_temp[2] = {0};
#else
uint16_t ADC_raw_temp;
#endif
uint16_t ADC_raw_volts;
uint16_t ADC_raw_current;
uint16_t ADC_raw_input;
uint16_t ADC_raw_ntc;
uint8_t PROCESS_ADC_FLAG = 0;
volatile char send_telemetry = 0;

#if ADC_CAPTURE_MODE
/* ================= ★ ADC 原始波形捕获（用户要求"看波形"）★ =================
 * 双缓冲窗口：一边采集、一边发送 ⇒ 采集不中断（窗口之间自然留出间隔）。
 * 每窗口 84 点 × 2 路(VA,VB) × 2 字节 = 336 字节 + 8 字节帧头。 */
volatile uint16_t adc_cap_buf[2][ADC_CAP_POINTS][2];
volatile uint16_t adc_cap_n     = 0;
volatile uint8_t  adc_cap_wr    = 0;
volatile uint8_t  adc_cap_ready = 0;
volatile uint32_t adc_cap_seq   = 0;   /* 已发出的窗口号 */
volatile uint32_t adc_cap_drop  = 0;   /* 因上一窗口未发完而丢弃的采样点数 */
volatile uint32_t adc_cap_pts   = 0;   /* 累计采集到的点数（用于对账） */

/* 由 ADC-DMA 中断（24kHz）每帧调用一次 */
void adc_capture_sample(uint16_t va, uint16_t vb)
{
    if (adc_cap_ready) { adc_cap_drop++; return; }   /* 上一窗口还没发完 ⇒ 丢 */
    adc_cap_buf[adc_cap_wr][adc_cap_n][0] = va;
    adc_cap_buf[adc_cap_wr][adc_cap_n][1] = vb;
    adc_cap_pts++;
    if (++adc_cap_n >= ADC_CAP_POINTS) {
        adc_cap_n = 0;
        adc_cap_ready = 1;
    }
}

/* ★★★ 2026-10-10 关键修复②：发送必须等 TXE ★★★
 *   LL_USART_TransmitData8() 只是往 TDR 寄存器写一个字节，【不等发完】。
 *   而 TDR 只有一个字节的坑 ⇒ 连写 344 字节时前面的全被后面的覆盖 ✗
 *   ⇒ 实测现象：串口上只收到约 2.1KB/s 的杂乱字节、完全找不到帧头 0xAA55。
 *   修法：每次写之前等 TXE（发送数据寄存器空）。 */
static void cap_putc(uint8_t b)
{
    while (LL_USART_IsActiveFlag_TXE(USART1) == 0) { }
    LL_USART_TransmitData8(USART1, b);
}

/* 主循环里调用：把写满的窗口发出去 */
void adc_capture_drain(void)
{
    uint8_t b;
    uint32_t sq;
    int k;

    if (!adc_cap_ready) { return; }

    /* ★★★ 2026-10-10 关键修复：先把【发送方向】打开 ★★★
     *   实测（SWD 读寄存器）：USART1->CR1 = 0x00000005 ⇒ UE=1、RE=1，但 **TE=0**
     *   —— 发送器被关着！而 adc_cap_seq 已经数到 53930 ⇒ 固件在拼命"发"，
     *   字节写进 TDR 却一个都没出去 ⇒ 串口上收不到任何数据。
     *   根因：serial_telemetry.c 初始化末尾调了
     *     LL_USART_SetTransferDirection(USART1, LL_USART_DIRECTION_RX);
     *   而 LL 的实现里设成 RX 会【清掉 TE】⇒ 之后谁写 TDR 都不发。
     *   ⇒ 这里在发之前显式打开 TX（本板是半双工，TE=1 时发送器独占这条线，
     *     正是我们要的：只往外倒数据）。 */
    LL_USART_SetTransferDirection(USART1, LL_USART_DIRECTION_TX);

    b = adc_cap_wr;
    adc_cap_wr ^= 1;              /* 立刻切到另一个缓冲，让 ISR 继续采 */
    adc_cap_ready = 0;

    cap_putc(0xAA);
    cap_putc(0x55);
    sq = adc_cap_seq++;
    for (k = 0; k < 4; k++) { cap_putc((uint8_t)((sq >> (k * 8)) & 0xFF)); }
    cap_putc((uint8_t)(ADC_CAP_POINTS & 0xFF));
    cap_putc((uint8_t)((ADC_CAP_POINTS >> 8) & 0xFF));
    for (k = 0; k < ADC_CAP_POINTS; k++) {
        uint16_t va = adc_cap_buf[b][k][0];
        uint16_t vb = adc_cap_buf[b][k][1];
        cap_putc((uint8_t)(va & 0xFF));
        cap_putc((uint8_t)(va >> 8));
        cap_putc((uint8_t)(vb & 0xFF));
        cap_putc((uint8_t)(vb >> 8));
    }
}
#endif
char telemetry_done = 0;
char prop_brake_active = 0;

volatile char dshot_telemetry = 0;

uint8_t last_dshot_command = 0;
char old_routine = 1;
uint16_t adjusted_input = 0;

#define TEMP30_CAL_VALUE ((uint16_t*)((uint32_t)0x1FFFF7B8))
#define TEMP110_CAL_VALUE ((uint16_t*)((uint32_t)0x1FFFF7C2))

uint16_t smoothedcurrent = 0;
const uint8_t numReadings = 50; // the readings from the analog input
uint8_t readIndex = 0; // the index of the current reading
uint32_t total = 0;
uint16_t readings[50];

uint8_t bemf_timeout_happened = 0;
/* ★★ 2026-10-14 丢过零滤波（RM2027_ZCL_FILTER）的运行时状态与观测 ★★
 *   全部放 RAM、走 SWD 读（tools/zcl-dump.py），不占串口带宽（p[] 49 字节已满）。
 *   逻辑见：超时分支（tenKhzRoutine 里"丢过零兜底"处）、interruptRoutine（清计数）、
 *   PeriodElapsedCallback（刷新 zcl_pred）。 */
volatile uint16_t zcl_pred = 0;      /* 超时阈值附加项：正常换相后=本步 waitTime；超时换相后=0 */
volatile uint8_t  zcl_miss = 0;      /* 连续丢失计数：真过零被接受时清 0，超时兜底时 +1 */
volatile uint16_t zcl_hold_n = 0;    /* 观测：丢 1~2 个（惯性按原节拍走）的步数累计 */
volatile uint16_t zcl_slow_n = 0;    /* 观测：连续丢 ≥3 后降频的步数累计 */
volatile uint8_t  zcl_miss_max = 0;  /* 观测：最长连续丢失纪录 */
uint8_t changeover_step = 5;
uint8_t filter_level = 5;
uint8_t running = 0;
uint16_t advance = 0;
uint8_t advancedivisor = 6;
volatile char rising = 1;

/* ★ 按上游 AM32 的写法：板级/编译期配置统一放在 targets.h（见 RM2027_ESC_G071 块里的 OL_* 宏），
 *   这里只保留【运行时状态】与【观测变量】。
 *   起步序列（对齐 → 斜坡 → 恒速）的推进逻辑见 olStepTick()。
 *   ★ 2026-10-08：这组变量改成【无条件编译】—— 闭环的"开环起步链"（见下）也复用它们。 */
uint32_t ol_steps = 0;                      /* 观测：累计换相步数 */
uint16_t ol_step_current[6];                /* 观测：六步各自的反推相电流（索引 = step-1，厘安）*/
uint8_t  ol_state = 0;                      /* 0 空闲 / 1 对齐 / 2 斜坡 / 3 恒速 */
uint16_t ol_input_cur = OL_INPUT_RUN;       /* 当前实际用的油门刻度（状态机维护）*/
uint16_t ol_step_div_cur = OL_STEP_DIV;     /* 当前实际步长（斜坡中变化；单位 20kHz 拍）*/
/* ★★★ 细粒度开环目标（xien/n1）★★★
 *   ol_ci_tgt   : 目标换相间隔，单位 0.5µs 拍（不是整数 step_div），可 SWD poke32 微调；
 *   ol_input_tgt: 目标运行油门刻度，可 SWD poke16 微调；
 *   ol_ci_cur   : 当前实际换相间隔（细粒度），用于生成 commutation_interval。 */
#ifndef XIEN_OL_CI_TGT
#define XIEN_OL_CI_TGT ((uint32_t)OL_STEP_DIV * 100UL)
#endif
#ifndef XIEN_OL_INPUT_TGT
#define XIEN_OL_INPUT_TGT OL_INPUT_RUN
#endif
volatile uint32_t ol_ci_tgt = XIEN_OL_CI_TGT;
volatile uint16_t ol_input_tgt = XIEN_OL_INPUT_TGT;
volatile uint32_t ol_ci_cur = 500000UL;     /* 起步 ci：OL_STEP_DIV_START*100，startMotor 会覆盖 */
volatile uint8_t  ol_tgt_armed = 0;         /* 目标首次到达时只武装一次捕获环 */
uint16_t ol_trip_count = 0;                 /* 观测：过流连续超限计数 */
uint32_t ol_trips = 0;                      /* 观测：累计过流跳闸次数 */
uint32_t ol_iphase_last = 0;                /* 观测：最近反推出来的相电流（厘安）*/
uint32_t ol_run_ticks = 0;                  /* 本次已驱动的拍数 */
/* ★ 2026-10-12 闭环自动停机（宏 RM2027_CL_RUN_MS）：交棒时刻的绝对帧号 + 计时中标志 */
volatile uint32_t cl_run_start = 0;         /* 交棒那一刻的 24kHz 绝对帧号 */
volatile uint8_t  cl_run_armed = 0;         /* 1 = 已交棒，自动停机计时中 */
volatile uint32_t cl_runs_stopped = 0;      /* 观测：因闭环到点而停机的次数 */
volatile uint8_t  cl_done_latch = 0;        /* ★ 1 = 闭环跑完已停机，必须先【收油】才能再跑
                                             *   （否则主循环的启动逻辑只看 running==0，
                                             *    油门还挂着就会立刻重启 ⇒ 无限循环重跑）*/
uint32_t ol_timeouts = 0;                   /* 观测：到时间上限自动停机的次数 */
volatile uint8_t ol_led_off_req = 0;        /* 请求切"待命红"（WS2812 阻塞操作，交主循环执行）*/
volatile uint8_t ol_led_on_req = 0;         /* 请求切"运行绿"：每次起步点一次（见 olStepTick）——
                                             * 否则第一轮跑完变红后，之后每轮都会一直亮红灯 */
volatile uint8_t ol_stop_reason = 0;        /* 0 无 / 1 到时间上限 / 2 过流跳闸 */
uint32_t ol_cool_ticks = 0;                 /* 冷却已过的拍数 */
/* ★★ 2026-10-09 【恒定斜率升频】的三个量（与开环分支 olStepTick 同一套做法）★★
 *   开环台架实测已证明：斜坡的【形状】必须是"频率随时间线性"，即 df/dt = 常数。
 *   原来的"每换相把步长按比例缩 1/2^shift"看起来合理，但它让换相频率【指数】上升
 *   ⇒ df/dt ∝ f² ⇒ 2222Hz 处的加速度需求约 254,000 Hz/s，
 *   而反电势随转速升高、力矩余量反而变小 ⇒ 末端必然丢步
 *   （现场表现：最后一段 14Hz → 81Hz → 2222Hz，用户说"最后一下突进跟不上"）。
 *   改成恒定斜率后末端需求降到约 3,500 Hz/s（小 72 倍），用户实测"好多了"。
 *   业界（Microchip AN857 / AVR928）说的就是 ramp start speed / end speed / slope。
 *   实现：olo_freq_q = 换相频率 × 256（步/秒）；每换相加 slope×65536/f_q
 *   （因为 df = slope × dt = slope / f），再由 div = 20000×256/f_q 反算步长。 */
uint32_t ol_freq_q     = 0;      /* 当前换相频率 x 256（步/秒）*/
uint32_t ol_freq_tgt_q = 0;      /* 斜坡终点频率 x 256 */
/* ★★ 2026-10-10 554 → 900 Hz/s ★★
 *   用户观察："0 到 1 秒转速就稳定了，不需要把转速抬那么高"。
 *   配合把 OL_STEP_DIV 从 11 降到 16（斜坡终点 1250Hz），时长 = (1250−5)/900 ≈ 1.4 秒，
 *   与"1 秒左右稳定"对得上（原来 (1818−5)/554 ≈ 3.3 秒，确实拖沓）。
 *   斜率不能随意加大：本项目踩过的坑是"按比例缩步长"让频率指数上升、末端 df/dt 达
 *   254,000 Hz/s ⇒ 转子跟不上 ⇒ 丢步。900 Hz/s 只有那个数的 0.35%，仍是安全区。*/
/* ★ 2026-10-10 退回 554（试过 900，随"降转速/缩短斜坡"那一轮一起改的；
 *   那一轮的三个改动（斜率、OL_STEP_DIV、OL_INPUT_START、CCR4）里
 *   真正致命的是 OL_INPUT_START=300 + CCR4：采样滑出导通期。其余留作记录。
 *   本值保持实测能起得来的 554。 */
/* ★★★ 2026-10-10 554 → 1800 Hz/s —— 用户要求：开环不用爬那么久，加速 1 秒即可稳定 ★★★
 *   原来 (1818-4)/554 ≈ 3.3 秒，太拖。改成 1800 Hz/s ⇒ 同样 4→1818Hz 只要约 1.0 秒 ✓
 *   安全性：本项目踩过的坑是按比例缩步长 ⇒ 频率指数上升 ⇒ 末端 df/dt 达 254,000 Hz/s
 *   ⇒ 转子跟不上丢步。1800 只有那个数的 0.7%，且 554 已实测可用，故在安全区内。
 *   转子能否跟上，看恒速段的 ol_meas_ci 偏差即可（±1% = 跟得很好）。
 *   ⚠ 顶端转速仍保持 OL_STEP_DIV=11（3030RPM）不变 —— 实测恒速段的稳定性随转速升高而变好：
 *     3030RPM 偏差仅 ±1%，而 303RPM 那版是 ±5%（低速齿槽力矩占比大）。 */
/* ★ 默认加速度（Hz/s）：普通分支=554，xien/n1 可在 targets.h 覆盖为 400 使升频更连续。
 *   运行时可经 SWD 写：tools/esc-dbg.sh poke16 ol_freq_slope <Hz/s>。 */
#ifndef XIEN_OL_FREQ_SLOPE
#define XIEN_OL_FREQ_SLOPE 554
#endif
#ifndef XIEN_OL_RUN_MS
#define XIEN_OL_RUN_MS     8000
#endif
uint16_t ol_freq_slope = XIEN_OL_FREQ_SLOPE;    /* 恒定斜率升频：df/dt = 本值 Hz/s */

/* ★★★ 2026-10-11 斜坡形态改成【指数升频】= 前缓后陡 ★★★
 *   用户要求："起步阶段改成曲线增速，前缓后陡，只是别太陡"。
 *   原来的形态：ol_freq_q += K/ol_freq_q ⇒ 积分出 f ∝ √n ⇒【前陡后缓】——
 *   起步瞬间频率涨得最快，而那时反电势≈0、力矩余量最小 ⇒ 转子跟不上就滑步
 *   ⇒ 现场就是"有电流、电机不转"。形态正好是反的。
 *   新形态：每次换相 f *= (1 + OL_RAMP_UP_Q/256) ⇒ f ∝ (1+r)ⁿ ⇒ 前缓后陡。
 *     · r 由本宏定：+1/256 ≈ +0.39%。默认 13 ⇒ +5.1%/换相步。
 *     · 全程步数 n = ln(f终/f始)/ln(1+r) = ln(1818/4)/ln(1.051) ≈ 123 步
 *     · 全程时间 ≈ (1/f始)·(1+r)/r ≈ 0.25s × 20.7 ≈ 5.2 秒
 *       ⇒ 加上后面的恒速热身 1.0s ≈ 6.2s，仍在 OL_STARTUP_TICKS_MAX(8s) 之内。
 *     · 大部分时间花在【低频段】= 起步最需要时间的阶段 ✓ 越往后越快 ✓
 *   ★ 只动"频率怎么涨"，占空比/油门/交棒判据一行没动。
 *   ⚠ 想更快就把本值调大（时间≈与 r 成反比）；想更缓就调小。 */
#ifndef OL_RAMP_UP_Q
#define OL_RAMP_UP_Q   13
#endif

/* ★★★ 2026-10-09 三步定位计数器（回答"交棒后为什么认不到过零"）★★★
 *   ① ol_adc_isr_n：ADC 完成中断进入次数 ⇒ 中断本身在不在跑
 *   ② ol_edge_n   ：其中"确实产生了边沿"的帧数 ⇒ 状态有没有在翻转
 *   ③ ol_noarm_n  ：有边沿、但 bemf_irq_armed=0 被【静默丢弃】的次数
 *      —— 原来的判据链是 `if (blanked) {...} else if (armed && edge) {...}`，
 *         【没有最后那个 else】⇒ "未武装"这种情况被无声丢掉、完全不可见。
 *         这很可能就是实测"交棒后连拒收都停了"的原因。
 *   配上已有的 ol_trace_n（接受）与 ol_trace_rej（窗外拒收），就能配平：
 *      边沿 = 接受 + 窗外拒收 + 未武装丢弃 */
volatile uint32_t ol_adc_isr_n = 0;
volatile uint32_t ol_edge_n    = 0;
volatile uint32_t ol_noarm_n   = 0;

volatile uint8_t ol_startup_arm = 0;        /* 1 = 闭环开环起步链在跑（ISR 也要读，故无条件定义）*/
/* ★ 台架过流切断用的计数（见 tenKhzRoutine 里的说明与 targets.h 的 RM2027_BENCH_OC_*） */
uint16_t ol_bench_oc_count = 0;

/* ★★★ 2026-10-11 停机原因持久埋点（重启不清，用 SWD 读）★★★
 *   为什么需要：能清 running 的路径有 7~8 处，其中多数**没有累积计数器**
 *   （比如开环 8 秒时限用的是 ol_startup_ticks，本分支没有观测量），
 *   而 ol_stop_reason 在重启时会被清零 ⇒ 事后读不出来上一轮为什么停。
 *   这里在每一处清 running 的地方写一个编号，只增不清 ⇒ 一轮跑完读它即可定位。
 *   编号：1=开环8s时限 2=过流跳闸 3=台架过流 4=油门回0 5=失步(非ADC_BEMF支路)
 *         6=闭环刹车/停转 7=低压截止 8=其它 */
volatile uint8_t ol_stop_site = 0;
uint32_t ol_bench_oc_trips = 0;
/* ↑ 这一组是"开环版 / 闭环开环起步链"共用的运行时状态。闭环也要用（起步链复用它们），
 *   所以下面的起步链代码【不能】再包在 #if RM2027_OL_ARM_ONLY 里。 */

/* ================== ★ 2026-10-09 过零埋点（无条件编译，ISR 要写） ==================
 * 台架现象："电机转、但慢且抖、没有梯形腰"，而 bemf_timeout_happened 以 ~26 次/秒
 * 上涨 ⇒ 换相主要靠 22.5ms 兜底硬踹 ⇒ 约 43 机械 RPM。
 * 要害是判"过零到底被认在哪一步的哪个位置" —— 这只能实测，猜不出来：
 *   正确：认到时距上次过零 ≈ 一个完整换相步（= 写入时的 CI）；
 *   假过零：远小于 CI（认在换相点/振铃上）；认晚：远大于 CI。
 * 写入点见 interruptRoutine()（只记"被接受"的），拒收点见 ADC-DMA 中断。
 * 全部是环形缓冲，用 SWD 直接 dump 这几段 RAM，不改变任何行为。 */
/* ★★ 2026-10-09：过零【发生】的时刻（由 ADC 中断写入，interruptRoutine 使用）。
 *   为什么必须是它而不是 INTERVAL_TIMER_COUNT：边沿标志被窗口拒掉后会一直挂着，
 *   等到某一帧窗口通过才被"收下"，那时的 CNT 是"收下时刻"、不是过零时刻。
 *   实测（埋点）靠 delta 分出两类（±5~±39 是本帧翻转、±374~±409 是滞后几百帧）
 *   才把这条钉死。用它做 thiszctime，CNT/CI 才能收敛到 1.0。 */
volatile uint16_t bemf_zc_time = 0;
/* ★ 2026-10-12 记录【这个过零发生在哪一步】—— 每步 waitTime 表要用它当索引 */
volatile uint8_t  bemf_zc_step = 0;

/* ★★★ 2026-10-12 每步 waitTime 表（由【开环实测】导出）★★★
 *   来源：在开环区（步长 0.917ms、1856 拍，转子跟着固定时基走、无反馈）实测每个 step 的
 *         "过零落在步内的位置 p_open"，则【过零 → 下一次换相】的理想延迟 = (1 − p_open)×步长。
 *   实测 p_open / 比例（= 这里的分母 1000）：
 *     step1 0.682 → 0.314    step2 0.636 → 0.359    step3 0.727 → 0.269
 *     step4 0.636 → 0.359    step5 0.952 → 0.047    step6 0.571 → 0.423
 *   ★ 关键：六个比例【全都小于 0.5】⇒ 开环实际是"过零后 0.05~0.42 步就换相"，
 *     远早于教科书 30°（= 0.5 步）。而闭环原来一律用 ci/2 = 0.5 ⇒ 换相整体偏晚
 *     ⇒ 相位偏 ⇒ 转矩掉 ⇒ 变慢。本表把闭环的相位"对齐到开环那套好的相位"。
 *   ⚠ 用比例（×ci）而不是绝对拍：步长随转速变，绝对拍不随动。
 *   ⚠ 方向风险：用户扫 advance 曾得"越提前越慢"，与本表方向相反 —— 先跑一次定方向。 */
/* ★★★ 2026-10-13 重标尝试【已撤回】：这张表不是"测量修正"，它是【换相提前量】★★★
 *   做法：用当前配置（V0=三相平均 + 三相增益归一）纯开环实测 u0 = 0.587/0.643/0.605/0.596/0.646/0.606，
 *        取 wt_num[s] = (1 − u0)×1000 = {413, 357, 395, 404, 354, 394}（= 保留老提前量、只补检测延迟 δ 的位移，
 *        代数上 1−u0_new ≡ ratio_old + (u0_old − u0_new)）。
 *   实测（evt_run_20261013_144428）：
 *        超时 0 ✅、六步散布 7%（史上最好）、A×ci 0.70 ✅
 *        但 **转速从 ~1450 掉到 420 转/分**、ci 与波形差 37% ❌
 *   ⇒ 判读：散布是【相对量】，步长从 ~2000 拍变 ~5000 拍 ⇒ "7%" 主要是"转得慢"的假象。
 *   ⇒ 更重要的结论：**开环实测的 u0 不能直接搬到闭环** —— 检测延迟 δ 在开环/闭环不同
 *      （斜率、屏蔽窗、接受窗都不同）。按代数推导转速本该不变，实际变了 ⇒ 该假设不成立。
 *   ⇒ 并且把这张表的性质搞清了：**它是 advance（转矩/相位的选择），不是测量校正。**
 *      旧表比例 0.05~0.42（大量提前）⇒ 1450 转/分；按 1−u0 重标（0.35~0.41，接近教科书 30°）
 *      ⇒ 提前量小 ⇒ 转矩掉 ⇒ 420 转/分。方向与源码注释里"越提前越慢"那句警告相反，见下。
 *   ⇒ 下一步方向（不再是改这张表）：让换相时刻**由转速推算的时序**决定，检测只用来**慢环校正转速/相位**
 *      —— 这样 δ 的抖动就不进相位；或先把 δ 测出来再从 waitTime 里减掉。
 *   恢复旧表（= 开环那套好相位的载体，转速的来源）： */
/* ★★★ 2026-10-15 六步常数化（test-12 第 2 步）：W ≡ 295‰ ★★★
 *   依据（今天的实测，evt_run_20261015_202541 / _203047 两份一致）：
 *     · 换相间隔六步 875/542/625/625/667/417 拍（极差 73%）；
 *       而【过零间隔六步只差 10.8%】（1463~1627 拍）⇒ 转子的每步物理时间是均匀的
 *       ⇒ "六步不均"不是转子的错，是【换相时刻】不均。
 *     · 换相时刻不均只可能来自本表：W 的六步值 47~423‰（相邻差 ±0.376×ci）。
 *       用表项的相邻差预测换相间隔：极差 68.8%、且六个间隔值的集合与实测一一对应 ✓
 *       （用户现场观察"卡顿/像变转向"的定量来源：每步"过零→换相"延迟在 47~423‰
 *         之间跳 ⇒ 相位六步间摆 ±19°E ⇒ 转矩一拍重一拍轻；step 序列无倒退，
 *         转向不可能反——数据已证。）
 *   数学：换相间隔(s) = 过零间隔 + W(s+1) − W(s) ⇒ W ≡ 常数 ⇔ 间隔 = 过零间隔（均匀）✓
 *   取 295‰ = 旧表均值（(314+359+269+359+47+423)/6 ≈ 295）⇒【总提前量水平不变】，
 *   只把六步的错配掰正。
 *   ★ 与 2026-10-12 那次"迭代到六步均匀"（见下方注释块）的区别：那次用
 *     ratio×(T̄/T_s) 迭代、"符号不自洽"而失败，并据此推断"六步不均是转子/磁路的
 *     固有不对称"——但那个推断【已被今天的过零间隔数据推翻】（转子侧是均匀的）。
 *     且那次很可能是被【索引错位一档】干扰：今天从事件流实测 wait/ci = wt_num[step−2]
 *     （§3 悬案的实测佐证）⇒ 迭代法调的那一项可能不是生效的那一项。
 *     常数表【天然免疫索引错位】（拿哪一项都一样）⇒ 这次的判读比那次干净。
 *   ★ 若常数化后六步仍不均 ⇒ 才轮到"转子/磁路固有不对称"的假说重新上台。 */
static const uint16_t wt_num[6] = { 285u, 285u, 285u, 285u, 285u, 285u };  /* ÷1000 */
/* ★★ 2026-10-12 试过"迭代到六步均匀"，失败并撤回 ★★
 *   想法：先按 ratio_s <- ratio_s × (T̄/T_s) 迭代一轮（T=实测六步时长），指望六步时长拉齐。
 *   实测（同一台架、同样 6 秒）：
 *     表A(开环导出) 六步 1.250/0.917/0.917/0.833/1.208/0.458  散布 85%   整体 1818 转/分
 *     表B(迭代后)   六步 2.542/0.583/1.792/1.208/1.833/0.583  散布 138%  整体 1143 转/分
 *   ⇒ 更差，而且符号不自洽（step6 被调长了 ✓，step1 本该缩短却更长 ✗）。
 *   ⇒ "六步不均匀"不是"我们等的时间不对"造成的，调 waitTime 的比例改不掉它；
 *     它更像转子/磁路的固有不对称（与 step5 那个"过零认不到 45%"的缺陷同源）。
 *   ⇒ 因此保留表 A（开环导出、已验证有效），不再迭代。*/

/* ★ 2026-10-09：本步内"|悬空相 − V0|"的最大值（每帧更新、每次过零清零）。
 *   用途：一眼看出反电势真实幅值 ⇒ 直接决定施密特滞回 BEMF_HYST_LSB 该取多少。
 *   实测它和"接受时 delta 只有 ±5~±11"配合，才能判定"信号在阈值上蹭"这件事。 */
volatile int16_t  bemf_dmax = 0;
/* ★★ 2026-10-13 闭环自动停机 + 锁定判据（分支 1013-g071-锁定判据+自动停机）★★
 *   自动停机：交棒后最多跑 RM2027_CL_RUN_MS 毫秒，到点无条件收油
 *   —— 用户要求"怕烧电机，开环闭环都短一点"；原来这条基线【没有】这道闸，
 *      所以我那次 12s 采集 = 闭环真转了 12.12s（而且转子是停的）⇒ 必须有。
 *   锁定判据：见 lock_frame() 的说明。 */

/* ★★★ 2026-10-13 锁定判据：A × ci ≈ 常数（磁链不变量）★★★
 *   用户原话："他会电机停转，空有换向……如果这部分的数据被算进了迭代里面，
 *   这一步是不是要进一步完善完善"——完全正确，必须先有"锁定检测"。
 *
 *   物理依据：反电势幅值 A ∝ 转速 ω，而换相周期 ci ∝ 1/ω ⇒ A × ci = 常数
 *   （就是"磁链"，也是 TI InstaSPIN-BLDC 积分法用的那个量）。
 *   实测分离度（六步幅值中位 × ci）：
 *     健康（跑②）：A ≈ 120 码、ci ≈ 2475 拍 ⇒ A×ci ≈ 297000
 *     失锁/停转（2026-10-13 那一跑）：A = 3 码、ci = 434 拍 ⇒ A×ci ≈ 1302（差 230 倍）
 *   ⇒ 判据：A×ci 跌破"健康峰值的 1/2"连续 6 步（= 一个电周期）⇒ 判失锁。
 *   峰值用【慢衰减峰保持】（不是低通！低通会被故障一起拖下去、永远不触发）。 */
volatile uint32_t lock_flux = 0;        /* 本步的 A×ci */
volatile uint32_t lock_flux_ref = 0;    /* 健康峰值（慢衰减）*/
volatile uint8_t  lock_lost = 0;        /* 1 = 已判失锁，交主循环处理停机 */
volatile uint16_t lock_bad_steps = 0;   /* 连续不合格的步数 */
volatile uint16_t lock_trips = 0;       /* 累计判失锁次数 */
volatile int16_t  lock_last_amp = 0;    /* 最近一步的幅值（诊断）*/
static int16_t lk_mx1, lk_mx2, lk_mn1, lk_mn2;
static uint32_t lk_buf[16];   /* 最近 16 步的 A×ci，取中位当基准（对偶发异常免疫）*/
static uint8_t  lk_bi = 0, lk_bn = 0;
static uint8_t lk_last_step = 0;

/* 每帧由 ADC 中断调用（传【原始】delta：锁定判据要的是真实反电势，与补偿无关）*/
void lock_frame(int16_t d_raw, uint8_t st)
{
    if (d_raw > lk_mx1) { lk_mx2 = lk_mx1; lk_mx1 = d_raw; }
    else if (d_raw > lk_mx2) { lk_mx2 = d_raw; }
    if (d_raw < lk_mn1) { lk_mn2 = lk_mn1; lk_mn1 = d_raw; }
    else if (d_raw < lk_mn2) { lk_mn2 = d_raw; }
    if (st < 1u || st > 6u) { return; }
    if (lk_last_step == 0u) { lk_last_step = st; return; }
    if (st == lk_last_step) { return; }
    {   /* ---- 结算刚结束的那一步 ---- */
        uint16_t a  = (uint16_t)(lk_mx2 - lk_mn2);
        uint32_t f  = (uint32_t)a * (uint32_t)commutation_interval;
        lock_last_amp = (int16_t)a;
        lock_flux = f;
        /* ★ 2026-10-13 修正①：峰值上升要限速。
         *   第一版直接 f > ref 就 ref = f ⇒ 一个瞬态就把基准抬到 4 倍
         *   （实测 lock_flux_ref = 950992，而健康值只有 ~25 万）⇒ 正常值被误判失锁、
         *   闭环刚稳 1.28s 就被停掉。原因：某个瞬间"step 窗口跨了不止一个电周期"
         *   （换相被漏记）⇒ A 正常但 ci 很大 ⇒ 乘积虚高。
         *   ⇒ 上升改成每步最多涨 1/64（一次瞬态只能推动 1.6%），下降仍走慢衰减。
         *   修正②：连续不合格从 6 步加到 10 步（一个电周期以上），更抗瞬时抖动。 */
        /* ★★★ 2026-10-13 修正③：基准改用【最近 16 步 A×ci 的中位数】★★★
         *   前两版用"峰保持"当基准，都被"step 窗口跨了不止一个电周期"（换相被漏记）
         *   的偶发异常抬飞：第一版抬到 95 万（健康 25 万）⇒ 误停；
         *   加限速后又缓慢爬到 128 万（当时 A=240 码明明很健康）⇒ 又误停。
         *   中位数对偶发异常天然免疫 ⇒ 一次异常最多占 16 个样本里的 1 个，动不了中位。
         *   （16 个数排序 ~120 次操作，每步只做一次 ≈ 1kHz ⇒ 开销可忽略。）*/
        lk_buf[lk_bi] = f;
        lk_bi = (uint8_t)((lk_bi + 1u) & 15u);
        if (lk_bn < 16u) { lk_bn++; }
        {
            uint32_t tmp[16];
            for (uint8_t _i = 0; _i < lk_bn; _i++) { tmp[_i] = lk_buf[_i]; }
            for (uint8_t _i = 0; _i < lk_bn; _i++) {
                for (uint8_t _j = (uint8_t)(_i + 1u); _j < lk_bn; _j++) {
                    if (tmp[_j] < tmp[_i]) { uint32_t _x = tmp[_i]; tmp[_i] = tmp[_j]; tmp[_j] = _x; }
                }
            }
            lock_flux_ref = tmp[lk_bn >> 1];
        }
        if (!ol_startup_arm && running && lock_flux_ref > 30000u) {
            if (f < (lock_flux_ref >> 1)) {
                if (++lock_bad_steps >= 10u) {
                    lock_lost = 1;
                    lock_trips++;
                    lock_bad_steps = 0;
                }
            } else if (lock_bad_steps > 0u) {
                lock_bad_steps--;
            }
        } else {
            lock_bad_steps = 0;
        }
    }
    lk_mx1 = lk_mx2 = -32768; lk_mn1 = lk_mn2 = 32767;
    lk_last_step = st;
}
/* ★ 奇偶偏置闸/计数（见 stm32g0xx_it.c 与 targets.h 的说明）*/
volatile uint16_t ci_clamp_rejects = 0;

#define OL_TRACE_LEN 32
volatile uint16_t OL_TRACE_CNT[OL_TRACE_LEN];  /* 认到过零时"距上次过零"的拍数（TIM2, 0.5µs） */
volatile int16_t  OL_TRACE_D[OL_TRACE_LEN];    /* 当时的"悬空相 − V0"（ADC 码） */
volatile int16_t  OL_TRACE_DMAX[OL_TRACE_LEN]; /* 本步内 |delta| 的最大值（= 反电势幅值）*/
volatile uint16_t OL_TRACE_CI[OL_TRACE_LEN];   /* 当时认为的一步有多长 */
volatile uint8_t  OL_TRACE_ST[OL_TRACE_LEN];   /* 当时的 step */
volatile uint8_t  ol_trace_idx = 0;
volatile uint16_t ol_trace_n = 0;              /* 累计"接受"次数 */
volatile uint16_t ol_trace_rej = 0;            /* 累计"有边沿但被时间窗拒掉"次数 */

/* ★★ 2026-10-09 悬空相波形捕捉（每 8 个 ADC 帧记一次，环形 64 点）★★
 *   为什么需要它：台架实测"闭环换相频率恒定、不跟转速变" ⇒ 说明闭环锁在了
 *   自己的换相动作上，而不是转子的反电势。要一次性区分两种可能 ——
 *     ① 硬件上悬空相根本没真悬空（相上仍是 PWM/被驱动）⇒ delta 是几百码的方波；
 *     ② 悬空相确实悬空、但反电势太小被噪声/振铃盖住 ⇒ delta 只有几十码；
 *   —— 只能把"悬空相 − V0"按时间还原成波形来看。这比再猜判据可靠。
 *   T 记的是记录时刻的 INTERVAL_TIMER 低 16 位，用它可以对上"换相点在哪"。 */
/* ★ 2026-10-09 换相时刻埋点（见 commutate()）：记录每次换相时的 delta/CNT/step/waitTime。
 *   用途：判断"换相落在反电势周期的哪个位置" —— 相位对错只能靠这条直接证据。 */
#define OL_CM_LEN 32
volatile int16_t  OL_CM_D[OL_CM_LEN];   /* 换相时刻的 "悬空相 − V0" */
volatile uint16_t OL_CM_T[OL_CM_LEN];   /* 换相时刻的 INTERVAL_TIMER 低16位 */
volatile uint8_t  OL_CM_S[OL_CM_LEN];   /* 换相后的 step */
volatile uint16_t OL_CM_W[OL_CM_LEN];   /* 当时的 waitTime */
volatile uint8_t  ol_cm_idx = 0;
volatile uint16_t ol_cm_n = 0;

#define OL_WF_LEN 64
volatile int16_t  OL_WF_D[OL_WF_LEN];   /* 悬空相 − V0（ADC 码） */
volatile uint16_t OL_WF_T[OL_WF_LEN];   /* 记录时刻（TIM2 低16位，0.5µs/拍） */
volatile uint8_t  OL_WF_S[OL_WF_LEN];   /* 当时的 step */
volatile uint8_t  ol_wf_idx = 0;
volatile uint8_t  ol_wf_div = 0;
#define OL_REJ_LEN 32
volatile uint16_t OL_REJ_CNT[OL_REJ_LEN];      /* 被拒时的位置（拍数） */
volatile uint16_t OL_REJ_CI[OL_REJ_LEN];       /* 被拒时的 CI */
volatile uint8_t  ol_rej_idx = 0;

/* ================== ★★ 2026-10-10 实验埋点：开环驱动期间"路径 B 的判据" ★★ ==================
 * 用户的要求（原话）："开环跑，但开环时候要用测出闭环的数据，以便我去排查是什么有问题"。
 *
 * 为什么原来的观测有盲区：
 *   路径 B 的【接受侧记录】(OL_TRACE_*) 写在 interruptRoutine() 里，而那个函数在
 *   开环驱动期间【根本不被调用】（换相由 olStartupTick() 独占）⇒ 开环时我们只拿得到
 *   "被拒"的记录（OL_REJ_*），拿不到"判成接受"的记录 ⇒ 判据到底怎么走的看不见。
 *
 * 这个实验把两件事彻底分开：
 *   换相时基：仍由【开环链】独占 —— 转速已知且正确，反电势一定出得来（开环已跑通）。
 *   路径 B  ：只"听"不驱动 —— 对每个边沿做一次完整的判据运算，把结果原样记下来。
 * 数据直接回答三个问题：
 *   ① 开环转得好好的时候，路径 B 到底认不认得到过零？→ ol_olp_n / ol_olp_acc
 *   ② 认到的那些，CNT 落在哪？开环下真过零应在【半个步长】处（= 0.5×CI）；
 *      若挤在 0 或 满步 附近 ⇒ 认的是换相振铃 / 迟到边沿 ⇒ 判据本身错。
 *   ③ 为什么接受/拒掉？同时记 waitTime / average_interval / delta ⇒
 *      一眼看出是哪个门槛在起作用（开环期间 average_interval 停滞不更新，这条尤其关键）。
 * 全部是环形缓冲 + SWD 直读，不改变任何行为。 */
#define OL_OLP_LEN 64
volatile uint16_t OL_OLP_CNT[OL_OLP_LEN];  /* 边沿出现时 INTERVAL_TIMER 读数（2MHz 拍） */
volatile uint16_t OL_OLP_CI[OL_OLP_LEN];   /* 当时的 commutation_interval（= 开环步长） */
volatile uint16_t OL_OLP_WT[OL_OLP_LEN];   /* 当时的 waitTime（换相点，开环期间是陈旧值） */
volatile uint16_t OL_OLP_AI[OL_OLP_LEN];   /* 当时的 average_interval（闭环统计量，开环期间停滞） */
volatile int16_t  OL_OLP_D[OL_OLP_LEN];    /* 当时的"悬空相 − V0"（ADC 码） */
volatile uint8_t  OL_OLP_OK[OL_OLP_LEN];   /* 路径 B 的结论：1=接受 0=被时间窗拒掉 */
volatile uint8_t  OL_OLP_ST[OL_OLP_LEN];   /* 当时的 step */
volatile uint8_t  ol_olp_idx   = 0;
volatile uint32_t ol_olp_n     = 0;        /* 开环驱动期间路径 B 判过多少次（有边沿、未被 blank） */
volatile uint32_t ol_olp_acc   = 0;        /* 其中判"接受"的次数 */
volatile uint32_t ol_olp_blank = 0;        /* 其中"换相后第一帧"被作废的次数 */
volatile uint16_t ol_olp_ci_last = 0;      /* 观测：最后一次记录时的步长（对上转速） */

#if !RM2027_OL_ARM_ONLY

/* ================== 闭环的"开环起步"（正统三段式，2026-10-08 加） ==================
 * 为什么要它：本板过零只能靠 ADC + 软件施密特（没有比较器通路，见 targets.h 的
 *   USE_ADC_BEMF 注释）。而施密特有个硬门槛 BEMF_HYST_LSB 码（≈相侧 50mV），
 *   对应电机 ~300~500 机械 RPM —— 低于这个转速，反电势跨不过门槛、状态不翻转、
 *   认不到过零。于是"起步就进闭环"会自锁：认不到过零 ⇒ 不缩短换相间隔 ⇒
 *   永远靠 22.5ms 超时硬踹 ⇒ 转速上不去 ⇒ 更认不到。实测（2026-10-08）：
 *   zc 恒 0、bemf_timeout_happened 猛涨、step 每 22.5ms 才走一步（≈7 RPM）。
 *
 * 正统做法（与上游 AM32 一致，只是把"比较器认过零"换成"ADC 认过零"）：
 *   ① 对齐：comStep(1) 把转子钉到已知位置
 *   ② 开环斜坡：固定时基硬踹，步长由 OL_STEP_DIV_START 逐次缩短到 OL_STEP_DIV
 *   ③ 认到 N 次过零 ⇒ 交棒给闭环（old_routine=0），此后由过零中断驱动换相
 * 三段里只有第 ③ 段需要反电势 —— 而那时电机已被踹到斜坡末端，反电势足够大、
 * 判据稳定可认。 */
#define OL_STARTUP_ALIGN   0   /* 阶段码：先对齐 */
#define OL_STARTUP_RAMP    1   /* 阶段码：开环斜坡 */
/* ★★★ 2026-10-09 "验证过再交棒"的参数 ★★★
 *   实测依据（三步定位计数器）：交棒【前】悬空相状态翻转正常（边沿 29→1714），
 *   交棒【瞬间】几乎停止（5 秒只涨 74 个）⇒ 反电势在交棒那一刻就塌了
 *   ⇒ 转子在交棒瞬间被打断节奏（用户观察："一交到闭环就剧烈抖、转速变慢"）。
 *   改法：斜坡到目标速度后【先不交】，进"验证段"——
 *     开环继续按目标节奏换相（节奏不断、反电势一直在），同时只"听"闭环报过零；
 *     每 OL_VERIFY_WIN 拍结算一次，数窗口内认到的过零数并与"应有的数"比；
 *     连续 OL_VERIFY_OK_NEED 个窗口合格 ⇒ 才真正把驱动交给闭环。 */
#ifndef OL_VERIFY_WIN
#define OL_VERIFY_WIN       2000   /* 校验窗口：20kHz 拍 ⇒ 2000 = 100ms */
#endif
/* ★★★ 2026-10-10 新增：斜坡跑到底后先【恒速热身】多久才开始验证交棒 ★★★
 *   用户指出："你开环压根没到匀速转的时候"。
 *   实测：斜坡(4000→110 拍、斜率 554Hz/s)只要 0.32 秒就走完，而验证窗一达标
 *   （3×100ms）立刻就交棒 ⇒ 开环全程约 0.5 秒，电机【从没在恒定转速上稳过】。
 *   TI SPRABZ3 的原话同样是 "the motor will gradually speed up and finally
 *   **run at a constant speed** in open loop commutation mode" —— 恒速段不是可选项：
 *     ① 转子稳下来（不在加速中途）⇒ 反电势波形干净；
 *     ② ol_meas_cross() 的"8 次平均实测周期"才有机会填满【恒速样本】
 *        （斜坡里步长动辄 200ms，8 个样本根本凑不齐 ⇒ 实测值可能是陈旧/中途值）；
 *     ③ 交棒才是"从恒速切到闭环"，而不是"在加速中途硬切"。
 *   取 30000 拍 = 1.5 秒：足够让 8 样本平均（每步 5.5ms ⇒ 8 步仅 44ms）反复刷新，
 *   也足够转子速度稳定；总开环 ≈ 0.2(对齐)+0.32(斜坡)+1.5(热身)+0.3(验证) ≈ 2.3 秒，
 *   远低于 8 秒的时长上限。 */
#ifndef OL_SOAK_TICKS
#define OL_SOAK_TICKS       20000   /* 1.0 秒（原 30000=1.5s）*/
#endif
#ifndef OL_VERIFY_OK_NEED
#define OL_VERIFY_OK_NEED   3      /* 连续多少个合格窗口才交棒 ⇒ 3 = 300ms */
#endif
/* ★★★ 2026-10-10 实验开关：开环跑，但不交棒 ★★★
 *   用户要求："开环跑，但开环时候要用测出闭环的数据，以便我去排查是什么有问题"。
 *   置 1 ⇒ 斜坡跑到底后【不交棒】，长期保持开环驱动：换相由开环链独占（转速已知、
 *          且已被开环台架证明是好的），路径 B 只"听"不驱动，持续把判据结果写进
 *          OL_OLP_* 环（见 Src/main.c 上面的说明）。这样用户能一边看示波器、
 *          我一边用 SWD 从容取数。
 *   置 0 ⇒ 恢复"验证过再交棒"的正常行为。
 *   ⚠ 这是实验开关，不是产品行为 —— 实验做完必须改回 0 再谈后续。 */
#ifndef OL_HOLD_OPENLOOP
/* ★★★ 2026-10-11 临时改回 1 —— 用户要求"切到开环版本再试一次" ★★★
 *   目的：把新的指数斜坡（前缓后陡）单独看清楚，不让闭环的毛病干扰判断。
 *   ⇒ 斜坡跑到底后【不交棒】，一直开环驱动到 8 秒上限。
 *   ⚠ 看完必须改回 0。
 *   0 = 正常行为：开环斜坡跑到底 + 验证窗口通过 ⇒ 交棒（ol_startup_arm=0），
 *       此后换相由 ADC-DMA 中断里的【环形缓冲版过零判据】驱动
 *       （Mcu/g071/Src/stm32g0xx_it.c 的 BEMF_ZC_USE_RINGBUF 分支）。
 *   1 = 只跑开环、不交棒（前几轮单独观察开环波形时用的实验态）。
 *   ⚠ 这一步是唯一"动到电机驱动行为"的改动。开环那一侧（对齐→斜坡→验证计数）
 *     一行没动，用的还是被台架反复验证过的那条链；换掉的只是"交棒之后怎么认过零"。
 *     开环安全网仍在：过流 OL_TRIP_CURRENT=8A、总时长 OL_RUN_LIMIT=8s。 */
#define OL_HOLD_OPENLOOP    0
#endif

/* ★★★ 2026-10-11 临时实验开关：跳过交棒的"验证窗口" ★★★
 *   为什么：交棒与否由 ol_verify_ok 决定，而那个计数走的是 olStartupTick() 里的
 *   【旧判据】（固定 8 码阈值 + 时间窗）—— 和我已经换成环形缓冲的新判据是两套。
 *   实测交棒因此变成"抛硬币"：同一固件前几轮 ver=3 能交棒、后几轮 ver=0 不交。
 *   而本次要抓的"交棒时电机反转"必须发生在【确定交棒】的前提下才能复现
 *   （三次都没交棒 ⇒ 捕获一次都没点火）。⇒ 临时把这道验证拿掉：
 *   "斜坡跑到底就交棒"。
 *   ⚠ 实验开关，不是产品行为 —— 抓完必须改回 0。
 *   ★ 去掉的只是"看不看过零计数"这一步；斜坡、对齐、交棒时的相位/周期初始化
 *     全部照旧 ⇒ 交棒后的行为与之前几轮可比。 */
#ifndef OL_SKIP_VERIFY
#define OL_SKIP_VERIFY      0
#endif
#if OL_SKIP_VERIFY
/* ★ 交棒条件改成"斜坡跑到底 + 恒速热身满"（ol_soak_ticks 由上面那段自增，
 *   封顶在 OL_SOAK_TICKS）——而不是简单置 1。
 *   为什么不能用"置 1"：那样开环只跑 3.4 秒（斜坡一到头就交）⇒ 开环整段看不见了，
 *   用户当场就发现"开环跑不动了"（他只看到电机进闭环后不对）。
 *   改成等热身满 ⇒ 开环完整跑「对齐 0.2s + 斜坡 3.2s + 恒速 1.0s」≈4.4s 之后再交，
 *   既看得见开环、交棒又是确定的。
 *   ★ 附带好处：热身跑满之后 ol_meas_ci 是从【整段恒速】量出来的 ⇒ 交棒时给闭环的
 *     周期初值更可信（之前验证窗和热身混在一起，样本未必够）。 */
#define OL_HOLD_DONE()  ((RM2027_NEVER_HANDOVER) ? 0 : \
                         ((ms10k_cnt - ol_hold_ms) >= (uint32_t)RM2027_OL_RUN_MS * 10u))

#define OL_VERIFY_PASS      (ol_soak_ticks >= OL_SOAK_TICKS && OL_HOLD_DONE())
#else
#define OL_VERIFY_PASS      (ol_verify_ok >= OL_VERIFY_OK_NEED)
#endif
#if OL_HOLD_OPENLOOP
/* ★★★ 2026-10-11：看波形用的 4000000UL(200s) 已改回 160000UL(8.0s) ★★★
 *   200 秒是【电机卸掉】时为了连续输出临时加的；用户接回电机后不能再用：
 *   开环在目标转速下有已知毛病（每 4~5 个电周期丢一步 ⇒ 丢步即制动 ⇒ 电流尖峰
 *   ⇒ 发热），连续跑 200 秒会烧电机/电调。
 *   8.0 秒 = 对齐 0.2s + 斜坡 5.2s + 稳速约 2.6s，够看完整条新曲线。
 *   真正停车的路径仍是"油门回 0"与过流跳闸。 */
#define OL_STARTUP_TICKS_MAX (XIEN_OL_RUN_MS * 20UL)   /* 20kHz 拍：XIEN_OL_RUN_MS ms */
#else
#define OL_STARTUP_TICKS_MAX (XIEN_OL_RUN_MS * 20UL)
#endif
#define OL_STARTUP_ZC_NEED 3    /* 旧判据（已被窗口校验取代，保留以防别处引用）*/
/* ★ 起步期间判过零用的"最小 |delta|"（ADC 码）。
 *   为什么不靠"抬阈值抗噪"：本板反电势本来就小 —— targets.h 记的实测是
 *   "3700RPM 时悬空相 delta ≈ ±32 码"，线性折算到新的交棒转速（2222RPM）只有约 ±19 码。
 *   把阈值抬到 20 就等于【交棒转速及以下永远认不到过零】⇒ 永远不交棒。
 *   所以阈值只取 8（高于噪声底 ±5，低于交棒转速的 ±19，留 2 倍余量）；
 *   抗噪改由两条【物理上更可靠】的判据承担（它们与幅度无关）：
 *     ① 换相后重新取样（ol_zc_reseed）—— 挡掉换相瞬间的振铃；
 *     ② 只认"落在本步中间段"的跨越 —— 真过零物理上就发生在换相后约半个步长处，
 *        而振铃/串扰紧跟在换相之后 ⇒ 用时间窗把两者分开。 */
#ifndef OL_ZC_MIN_DELTA
#define OL_ZC_MIN_DELTA 8
#endif

static uint8_t   ol_startup_stage = OL_STARTUP_ALIGN;
static uint32_t  ol_startup_ticks = 0; /* 本起步过程已跑的拍数（安全上限用） */
static uint16_t  ol_startup_trip = 0;  /* 过流连续超限计数 */
volatile uint8_t  ol_startup_zc = 0;   /* 本起步过程累计认到的过零数（观测+交棒判据）*/
static uint8_t    ol_zc_prev = 0;      /* 上一次采到的电平（判"跨越"用）*/
volatile int16_t  ol_zc_delta = 0;     /* 观测：起步期间最近一次"悬空相 − V0" */
volatile uint8_t  ol_zc_level = 0;     /* 观测：上式的符号（1=正 0=负） */
volatile uint8_t  ol_zc_inwin = 0;     /* 观测：本拍的跨越是否落在"本步中间段"时间窗内 */
volatile uint8_t  ol_stage_show = 0;   /* 观测：起步阶段 0=对齐 1=斜坡 */
static uint8_t    ol_zc_reseed = 0;    /* 换相后重新取样标志（见下面判过零处） */
static uint16_t   ol_verify_ticks = 0; /* 验证段：本窗口已过拍数 */
static uint32_t   ol_soak_ticks   = 0; /* ★ 恒速热身段已过拍数（见 OL_SOAK_TICKS）*/
static uint16_t   ol_verify_ok    = 0; /* 验证段：连续合格窗口数 */
volatile uint16_t ol_verify_last = 0;  /* 观测：上一窗口认到的过零数 */
volatile uint16_t ol_verify_ok_show = 0;/* 观测：当前连续合格窗口数 */
volatile uint16_t ol_steps_show = 0;   /* 观测：一个采样窗内的换相步数（估速率用） */

/* 起步期间：允许过零中断"认边沿"，但不许它直接驱动换相（换相由本链独占），
 * 也不许主循环那条 22.5ms 超时兜底插手（否则会和本链的时基打架）。 */
#define OL_STARTUP_ACTIVE() (ol_startup_arm != 0)

/* 给 ISR 用的"数一次过零"出口（ISR 在 stm32g0xx_it.c，不能直接写 static 变量） */
void ol_startup_zc_inc(void)
{
    if (ol_startup_zc < 250) {
        ol_startup_zc++;
    }
}

/* ★★★ 2026-10-10 开环期间的"转子周期"实测 —— 用户猜想 1 的修法 ★★★
 *   用户原话："有没有可能是开环切换到闭环的时候闭环没有数据所以导致切换的时候跟不上"。
 *   —— 对。原来交棒是把【命令值】硬塞给闭环：
 *        commutation_interval = ol_step_div_cur * 100;
 *      而开环期间闭环的测量/滤波链（interruptRoutine / PeriodElapsedCallback）是停着的
 *      ⇒ average_interval、commutation_intervals[] 全是陈旧值
 *      ⇒ 切过去的头几步没有历史，必然晃一阵才能收敛。
 *   改法：开环期间让"测量"一直跑（只测不换相）——累计【过零到过零】的真实间隔。
 *      反电势过零是跟着【转子】走的，所以量到的就是转子真实周期；
 *      ★ 如果它和命令值不一致，那本身就是"开环滑步"的直接证据。
 *   交棒时优先用实测值当初值 ⇒ 切过去即与转子同步，不用再收敛。 */
volatile uint16_t ol_meas_ci    = 0;    /* 实测转子周期（0.5µs 拍）；0 = 尚未量到 */
volatile uint16_t ol_meas_used  = 0;    /* 观测：本次交棒实际用了多少（0 = 退回命令值）*/
volatile uint16_t ol_meas_gap_used = 0;  /* 观测：交棒实际用的 gap（0 = 退回半步）*/
static   uint32_t ol_meas_sum   = 0;
static   uint16_t ol_meas_n     = 0;
static   uint16_t ol_meas_prev  = 0;    /* 上一次的 8 样本平均（做一致性比较用）*/
static   uint8_t  ol_meas_ok    = 0;    /* 连续一致的次数 */
volatile uint16_t ol_cross_prev = 0;    /* 上一次过零落在当前步内的位置 */
/* ★★★ 2026-10-10 相位连续交棒用：开环期间"过零 → 下一次换相"的实测间隔 ★★★
 *   开环实测：真过零落在 0.91 步处（拒绝环 CNT≈1000 / 步长 1100）
 *   ⇒ 开环的换相点离过零只有 0.09 步（转子滞后磁场约 24.6°）。
 *   而闭环原来一交棒就按"过零后 0.5 步（理想 30°）"换相 ⇒ 相位前跳 0.41 步
 *   ⇒ 转子当场失同步 ⇒ 边沿从 ~1400/秒 塌到 ~130/秒 ⇒ 被拖成慢速锁定。
 *   ⇒ 开环期间把实际 gap 量出来，交棒时用它当初始 waitTime。 */
volatile uint16_t ol_meas_gap_sum = 0;
volatile uint16_t ol_meas_gap_n   = 0;
volatile uint16_t ol_meas_gap     = 0;

/* 由 ISR 在每个"被接受的过零"处调用（开环期间）。
 * 定时器在每次换相时被 olStartupTick 清 0 ⇒
 *   本步周期 = ol_step_div_cur*100；间隔 = (本步周期 − 上次过零位置) + 本次位置。 */
void ol_meas_cross(uint16_t cnt)
{
    uint32_t step = (uint32_t)ol_step_div_cur * 100u;
    uint32_t iv;

    if (cnt >= step) {          /* 位置超过一步 ⇒ 数据不可信，丢弃 */
        return;
    }
    iv = step - (uint32_t)ol_cross_prev + (uint32_t)cnt;
    ol_cross_prev = cnt;

    /* 顺带量"过零 → 下一次换相"的间隔：开环换相发生在 timer 达到 step 时，
     * 所以从本次过零的 cnt 到下一次换相还有 (step - cnt) 拍。 */
    {
        uint32_t gap = step - (uint32_t)cnt;
        if (gap <= 0xFFFFu) {
            ol_meas_gap_sum = (uint16_t)((ol_meas_gap_sum + gap) / 2u);  /* 指数平均，够用 */
            if (ol_meas_gap_n < 1000u) { ol_meas_gap_n++; }
            if (ol_meas_gap_n >= 4u) { ol_meas_gap = ol_meas_gap_sum; }
        }
    }

    if (iv > 65535u) {          /* 超过 uint16 能表示的范围 ⇒ 丢弃 */
        return;
    }
    ol_meas_sum += iv;
    if (++ol_meas_n >= 8u) {    /* 8 次平均，够稳又够快 */
        uint16_t m = (uint16_t)(ol_meas_sum / (uint32_t)ol_meas_n);
        ol_meas_sum = 0;
        ol_meas_n   = 0;

        /* ★★★ 2026-10-10 抗噪闸 ★★★
         *   实测教训（本轮台架，母线 0V、转子根本没转）：噪声也能凑出 8 个"过零"、
         *   报出一个 5754 的"实测周期" —— 而命令值恰好是 5500，落在交棒的
         *   1/8~8 倍窗口里，任何幅度检查都防不住。若拿它当交棒初值就会把闭环带偏。
         *   ⇒ 只认【连续 3 次平均互相一致（±20%）】的结果：噪声是随机的，很难连续三次
         *     落在一起；而真在转时每 5.5ms 出一次平均（1.5 秒热身段有几十次），
         *     连续 3 次一致是常态。 */
        if (ol_meas_prev != 0
            && m + (m >> 2) >= ol_meas_prev
            && m - (m >> 2) <= ol_meas_prev) {
            if (ol_meas_ok < 200u) { ol_meas_ok++; }
        } else {
            ol_meas_ok = 0;
        }
        ol_meas_prev = m;
        if (ol_meas_ok >= 2u) {          /* 含本次共 3 次一致 ⇒ 才发布 */
            ol_meas_ci = m;
        }
    }
}

/* 由主循环每轮调用（与 olStepTick 同层）。返回时若已交棒，ol_startup_arm 已清 0。 */
static void olStartupTick(void)
{
    if (!OL_STARTUP_ACTIVE() || !running) {
        return;
    }
    /* ★ 收油门即停（2026-10-08 补）：原来用被覆盖过的 input 判断 ⇒ 起步期间
     *   油门回 0 也停不下来、一路跑到超时。改用原始油门 newinput。 */
    /* ★★ K4：自驱期间，在【使用点】把虚拟油门强制钉住 ★★
     *   为什么放这里：newinput 有多个异步写入者（dshot.c 的 EXTI 路径、signal.c 的模拟/伺服
     *   路径…），在主循环某处注入会被它们覆盖。而 olStartupTick() 只有这一个判断点
     *   ⇒ 在判断前一刻钉住，谁都盖不掉。实测（cal_trip 追踪）：命中此路径时
     *   newinput=0 而 cal_run=3，即"自驱中被别的写入者清掉了油门"。
     *   ⚠ 只钉油门；过流/失速/到点/失锁保护全不动。 */
    {   extern volatile uint8_t cal_run;
        if (cal_run != 0u) { newinput = 350; input = 350; }
    }
    if (newinput < 48) {
        {   /* ★ K4 追踪：这条路径不写 ol_stop_site，必须单独打点 */
            extern volatile uint16_t cal_trip[4];
            extern volatile uint8_t  cal_run;
            cal_trip[0]=0xBEEFu; cal_trip[1]=newinput; cal_trip[2]=cal_run; cal_trip[3]=input;
        }
        allOff();
        running = 0;
        ol_startup_arm = 0;
        return;
    }

    /* ---- 安全①：过流（起步时反电势几乎为 0，最容易吃到堵转电流） ---- */
#if OL_TRIP_CURRENT > 0
    if (duty_cycle > 0
        && ((uint32_t)actual_current * 2000) / (uint32_t)duty_cycle > OL_TRIP_CURRENT) {
        if (++ol_startup_trip > OL_TRIP_TICKS) {
            allOff();
            running = 0;
                ol_stop_site = 2;            /* 停机站点：起步链过流跳闸 */
            ol_startup_arm = 0;
            return;
        }
    } else {
        ol_startup_trip = 0;
    }
#endif

    /* ---- 安全②：总时长上限（斜坡跑完仍没交棒 ⇒ 放弃起步、停机，绝不无限硬踹） ----
     * 8.0 秒 = 160000 拍：对齐 0.2s + 斜坡 3.2s ⇒ 还剩 4.6 秒在交棒转速上等过零，
     * 而 2222RPM 下一步 750µs、认 3 次过零只要 2.3ms ⇒ 余量绰绰有余。
     * （原来写 200 秒是为了配合"斜坡要 13 秒"的；现在斜坡 3.2 秒就完，
     *   再留 200 秒只会把"起步失败"变成"电机一直硬踹不停"。） */
    /* ★★ 2026-10-11 修：这个时限只在【还没交棒】时计时 ★★
     *   原来是无条件 ++ol_startup_ticks，而 olStartupTick() 在 main.c:2767
     *   每轮主循环都被调用 ⇒ 交棒进了闭环它照样数 ⇒ 从电调启动算起满 8 秒
     *   就把正常运行的电机停掉（实测 ssite=1），油门还挂着 ⇒ 自动重启 ⇒
     *   每 8 秒一轮"停一下又转"。台架粗时间线（ccm_slow）在 7.4 秒处的
     *   突变就是这个。本意只是"起步失败别无限硬踹"，所以加上 ol_startup_arm。 */
    if (ol_startup_arm && (++ol_startup_ticks > OL_STARTUP_TICKS_MAX)) {
        allOff();
        running = 0;
        ol_stop_site = 1;                 /* 停机站点：开环 8 秒时限 */
        ol_startup_arm = 0;
        return;
    }

    /* 期望间隔：与 test-12 原链一致，直接使用当前细粒度 ci；
     * 起步时 ol_ci_cur = OL_STEP_DIV_START*100 = 500000，对齐后进入斜坡。 */
    commutation_interval = ol_ci_cur;

    /* ---- 阶段推进 ---- */
    if (ol_startup_stage == OL_STARTUP_ALIGN) {
        /* 对齐：门极在 startMotor() 里已钉在 comStep(1)，这里只计时 */
        if (INTERVAL_TIMER_COUNT >= commutation_interval) {
            SET_INTERVAL_TIMER_COUNT(0);
            ol_startup_stage = OL_STARTUP_RAMP;
            /* 进斜坡：开始武装过零判据（只用来"数"过零，换相仍由本链独占） */
            ol_input_cur = OL_INPUT_START;
            enableCompInterrupts();
        }
    } else {
        /* 开环斜坡：到点就换相一次，并把步长按 1/2^OL_RAMP_SHIFT 缩短 */
        if (INTERVAL_TIMER_COUNT >= commutation_interval) {
            SET_INTERVAL_TIMER_COUNT(0);
            commutate();
            ol_zc_reseed = 1;   /* ★ 换相后"重新取样"：避免切换悬空相造成的假跳变被当成过零 */
            ol_steps++;
            /* ★★★ 2026-10-11 切回【斜线（线性升频）】形态 —— 用户要求 ★★★
             *   df = slope x dt / f，定点（f_q = f x 256）：
             *       ol_freq_q += (ol_freq_slope x 65536) / ol_freq_q;
             *   积分出 f ∝ √n。实测形态（slope=554）：
             *     第 1 步 250ms（4.0 步/秒）→ 第 2 步直接跳到 142.5 步/秒（7ms/步）
             *     → 之后近似线性爬升 → 第 2965 步 / 3.27 秒 到 1818 步/秒。
             *   （指数版 OL_RAMP_UP_Q 的定义保留但不再使用。） */
            /* ★ 运行时目标：允许 SWD 改 ol_ci_tgt，实际 f 按 ol_freq_slope 平滑跟随，
             *   上升/下降都限速，不会阶跃。 */
            if (ol_ci_tgt < 100UL) { ol_ci_tgt = 100UL; }
            ol_freq_tgt_q = 512000000UL / ol_ci_tgt;
            {
                uint32_t inc = ((uint32_t)ol_freq_slope * 65536UL) / ol_freq_q;
                if (inc < 1UL) { inc = 1UL; }
                if (ol_freq_q < ol_freq_tgt_q) {
                    ol_freq_q += inc;
                    if (ol_freq_q > ol_freq_tgt_q) { ol_freq_q = ol_freq_tgt_q; }
                } else if (ol_freq_q > ol_freq_tgt_q) {
                    if (ol_freq_q > ol_freq_tgt_q + inc) { ol_freq_q -= inc; }
                    else { ol_freq_q = ol_freq_tgt_q; }
                }

                /* V/f：油门从 OL_INPUT_START 按频率比例插到 ol_input_tgt */
                {
                    uint32_t f0 = 512000000UL / ((uint32_t)OL_STEP_DIV_START * 100UL);
                    uint32_t ft = ol_freq_tgt_q;
                    if (ft > f0) {
                        uint32_t num = (ol_freq_q > f0) ? (ol_freq_q - f0) : 0u;
                        uint32_t den = ft - f0;
                        if (num > den) { num = den; }
                        {
                            int32_t in0 = (int32_t)OL_INPUT_START;
                            int32_t in1 = (int32_t)ol_input_tgt;
                            ol_input_cur = (uint16_t)(in0 + ((in1 - in0) * (int32_t)num) / (int32_t)den);
                        }
                    }
                }

                if (ol_freq_q >= ol_freq_tgt_q) {
                    ol_freq_q = ol_freq_tgt_q;
                    ol_ci_cur = ol_ci_tgt;                 /* 细粒度终点：1 拍分辨率 */
                    ol_step_div_cur = (uint16_t)(ol_ci_tgt / 100UL);
                    ol_input_cur = ol_input_tgt;
                    if (!ol_tgt_armed) {
                        ol_tgt_armed = 1;
                        ol_hold_ms = ms10k_cnt;
                        adc_zc_wave_arm(0xFFFFFFFFu);
                    }
                } else {
                    ol_ci_cur = 512000000UL / ol_freq_q;
                    ol_step_div_cur = (uint16_t)(ol_ci_cur / 100UL);
                }
            }
        }
    }

    /* ---- ★ 直接判过零（本函数跑在 20kHz 中断里，每个周期读一次）----
     * 为什么不用 ADC-DMA 中断那条路：它依赖 bemf_irq_armed / average_interval
     *   等一堆运行时门槛，起步期间这些量还没建立 ⇒ ol_startup_zc 恒 0 ⇒ 永远不交棒。
     * 这里直接读"悬空相 − V0"（adc_bemf_get_delta()），判它是否跨过阈值。
     * ★ 2026-10-09：ADC 中断里那份【重复计数】已删除 —— 原来两边都在数，
     *   同一个过零被记两次 ⇒ 计数翻倍 ⇒ 交棒时机不可控。现在只有这里一份。
     * ★ 2026-10-09：判据改成【极性无关】—— 只认"跨过阈值"这个事件本身，不判方向。
     *   物理上每一步有且只有一个过零；判方向的收益是零，风险却很大：
     *   rising 与实际边沿方向的对应关系一旦反了，就【一个都数不到】⇒ 永远不交棒，
     *   表现出来就只是"电机在慢慢爬坡"（正是用户说的"缓慢抖动"）。 */
    {
        if (ol_freq_q >= ol_freq_tgt_q) {
            /* 只有斜坡已经跑到终点（= 交棒转速 2222RPM）之后才开始数：
             * 之前的转速低、反电势小，数出来的都不是可靠的过零。 */
            int16_t d = adc_bemf_get_delta();
            ol_zc_delta = d;
            uint8_t lvl = (d > OL_ZC_MIN_DELTA) ? 1u
                        : ((d < -(int16_t)OL_ZC_MIN_DELTA) ? 0u : ol_zc_level);
            ol_zc_level = lvl;
            /* 只认"落在本步中间段"的跨越：真过零在换相后约半个步长处，
             * 振铃紧跟在换相之后 ⇒ 用时间窗把两者分开（与幅度无关，最可靠）。 */
            uint32_t cnt = INTERVAL_TIMER_COUNT;
            /* ★★★ 2026-10-10 窗口上界从 3/4 步放开到 整步 ★★★
             *   实测（恒速段、3030RPM、路径B 的 ol_meas_ci 偏差仅 ±1%，说明过零确实存在）：
             *   路径A 每 100ms 窗口只数到 0~5 次（交棒需要 ≥121）⇒ 交棒永不成立
             *   ⇒ 8 秒上限到、电调重启（数据里 arm 全程 1、div 在 8.5s 跳回 5000）。
             *   根因：路径A 的时间窗原为 [ci/4, 3ci/4]（0.25~0.75 步），
             *   而实测真过零落在 **0.75~0.9 步** —— 恰好被上界挡在外面。
             *   ★ 这在物理上是应该的：开环时转子必然滞后于磁场，过零就比"半步"晚；
             *     TI 那条"过零在换相后 30°(半步)"的前提是【已对齐的闭环】。
             *     窗口卡 3/4 上界等于要求"先闭环对齐才准交棒" ⇒ 死锁。
             *   下界仍留 1/8 步：避开换相后紧跟的振铃（那一段另有 ol_zc_reseed + 3 帧屏蔽）。 */
            uint8_t inwin = (cnt > (commutation_interval >> 3))
                         && (cnt < commutation_interval);
            ol_zc_inwin = inwin;
            if (ol_zc_reseed) {
                /* 刚换相：悬空相变了，delta 会跳变 ⇒ 只把基准电平对齐到新值，不计数 */
                ol_zc_reseed = 0;
                ol_zc_prev = lvl;
            } else if (inwin && lvl != ol_zc_prev) {
                if (ol_startup_zc < 250) {
                    ol_startup_zc++;
                }
                ol_zc_prev = lvl;
            }
        } else {
            /* 还没到交棒转速：清掉早期可能攒下的噪声计数，并丢弃换相残留 */
            ol_zc_reseed = 0;
            ol_startup_zc = 0;
            ol_zc_inwin = 0;
        }
    }

    ol_stage_show = ol_startup_stage;
    ol_steps_show = ol_steps;

    /* ---- 交棒 ----
     * 条件：斜坡已跑到底（ol_freq_q >= ol_freq_tgt_q ⇒ 转速就是交棒转速）
     *       且认到 OL_STARTUP_ZC_NEED 次过零（说明反电势确实出来了、转子在跟着转）。
     * 为什么必须"斜坡跑到底"：只认过零不看斜坡 ⇒ 起步早期（div 还很大、转子几乎没转）
     *   会被噪声骗去交棒，实测只走 3 步就交棒、之后闭环不换相、steps 冻住。 */
    /* ---- 交棒前先"验证"：开环继续驱动，只在旁边核对闭环认到的过零速率 ---- */
    ol_verify_ok_show = ol_verify_ok;
    if (ol_freq_q >= ol_freq_tgt_q) {
        /* ★★★ 2026-10-10 先恒速热身 OL_SOAK_TICKS 拍，再开始验证计数 ★★★
         *   用户指出："你开环压根没到匀速转的时候"。原来斜坡一跑到底、验证窗一达标
         *   （3×100ms）就交棒 ⇒ 开环全程约 0.5 秒，电机从没在恒定转速上稳过
         *   ⇒ 交棒等于"在加速中途硬切"。TI SPRABZ3 也明确要求开环先
         *   "run at a constant speed" 再切闭环。
         *   而且这一步是实测周期能用的前提：ol_meas_cross() 的 8 次平均需要【恒速样本】，
         *   斜坡里步长动辄 200ms，样本根本凑不齐。
         *   热身期间一切照旧（开环驱动 + 只测不换相的过零测量），只是不数验证窗。 */
        if (ol_soak_ticks < OL_SOAK_TICKS) {
            ol_soak_ticks++;              /* 恒速热身中：只跑、不判 */
        } else if (++ol_verify_ticks >= OL_VERIFY_WIN) {
            uint16_t expect = (uint16_t)(((uint32_t)OL_VERIFY_WIN * 100UL) / ol_ci_tgt);
            ol_verify_ticks = 0;
            if (ol_startup_zc >= (uint16_t)(expect - (expect / 3))) {
                if (ol_verify_ok < 250) { ol_verify_ok++; }
            } else {
                ol_verify_ok = 0;      /* 本窗口不合格 ⇒ 连续计数清零重来 */
            }
            ol_verify_last = ol_startup_zc;   /* 观测 */
            ol_startup_zc = 0;         /* 每窗口重新数 */
        }
    }

    /* ★★★ 2026-10-11 关键修复⑩：交棒块必须【锁存】—— 只执行一次 ★★★
     *   原条件 `!OL_HOLD_OPENLOOP && ol_freq_q >= ol_freq_tgt_q && OL_VERIFY_PASS`
     *   在交棒之后【仍然成立】：
     *     · ol_step_div_cur 停在 OL_STEP_DIV 不再变；
     *     · ol_verify_ok 只在开环 tick 里更新，而交棒后开环 tick 不再跑 ⇒ 它保持
     *       在达标值不降。
     *   ⇒ 这个块就在主循环里【每轮都跑一遍】（每秒几千次）⇒ 每次都把
     *     commutation_interval / waitTime / 六步环 / average_interval 重新初始化
     *     ⇒ 把闭环刚测出来的时序【覆盖掉】⇒ 换相时刻成了"初值 + 实测值"的混合物
     *     ⇒ 相位必乱、转子表现异常。
     *   实测铁证：闭环波形捕获的触发次数 = 8000 次（应为 1 次）—— 因为每次跑这段
     *   都会调 adc_zc_wave_arm() 把捕获重新武装。
     *   ⇒ 加 ol_startup_arm 作锁存（本块自己会清它）⇒ 保证只执行一次。 */
    if (ol_startup_arm && !OL_HOLD_OPENLOOP && ol_freq_q >= ol_freq_tgt_q && OL_VERIFY_PASS) {
        ol_startup_arm = 0;          /* 退出开环：此后换相交给过零中断闭环 */
        /* ★ 闭环自动停机的计时起点：以"交棒这一刻"为 0（见主循环里的到点检查）*/
        cl_run_start = adc_frame_total;
        cl_run_armed = 1;
        old_routine = 0;
        /* ★★★ 2026-10-11 交棒时给速度环一个干净的起点 ★★★
         *   input_override 是速度环的工作变量（单位 = 油门 x10000），而它全流程
         *   没有"用当前油门初始化"的地方 ⇒ 不初始化的话速度环从 0 爬起、交棒瞬间
         *   占空比会先掉下去。这里用交棒时刻的实际油门给它赋初值 ⇒ 交棒平滑。
         *   同时清掉 PID 的历史（integral/last_error/error）⇒ 不把开环段的陈旧误差
         *   带进闭环造成第一下猛冲。 */
        input_override = (int32_t)adjusted_input * 10000;
        speedPid.integral   = 0;
        speedPid.last_error = 0;
        speedPid.error      = 0;
        /* ★ 交棒这一刻【武装】闭环波形捕获：阈值 = 交棒实测步长的 2 倍。
         *   实测交棒的 16ms 窗口内一切正常 ⇒ 改由"ci 涨到 2 倍"来触发冻结，
         *   窗口才罩得住塌陷本身（见 adc_zc.h 的说明）。 */
        /* ★ 2026-10-11 触发门槛收紧：x2 -> x1.125
         *   实测教训：门槛设成 2 倍时，ci 在交棒后头几次换相内就翻倍了
         *   ⇒ 触发时塌陷【已经完成】⇒ 捕获窗口抓不到"交棒 + 停"的过程
         *   （窗口里已经是稳定慢态：delta 变号间隔 2.6ms）。
         *   收到 1.125 倍 ⇒ 只要 ci 略一上涨就冻结 ⇒ 窗口罩住交棒那一刻。 */
        /* ★★ 交棒时必须把闭环的时间基准初始化正确 ★★
         *   闭环的 waitTime 只在 PeriodElapsedCallback() 里算，而开环斜坡期间它
         *   根本没跑过 ⇒ 交棒瞬间 waitTime / lastzctime / thiszctime 全是陈旧值
         *   （waitTime 甚至是 0）⇒ 第一次过零立刻换相（步长≈0）⇒ commutation_interval
         *   被污染 ⇒ 之后换相时刻一直错（波形退化成"类似正弦、没有梯形斜线"）。
         *   ★ 口径提醒：thiszctime 是【两次过零之间的时间】= 一个完整换相步，
         *     不是半个 —— interruptRoutine() 每次过零都把 INTERVAL_TIMER 清零，
         *     所以下次读到的 CNT 就是整个步长；PeriodElapsedCallback() 里
         *     commutation_interval = (ci + (lastzctime+thiszctime)/2)/2 也是在
         *     朝"整步"收敛。原先这里写成 commutation_interval >> 1 是错的：
         *     它会让 ci 一次性被砍掉 25%，等于凭空命令电机加速 1/3 ⇒ 起步就丢步。 */
        /* ★★★ 2026-10-10：优先用【开环期间实测到的转子周期】做初值 ★★★
         *   原来一律用命令值 ol_step_div_cur*100。若开环时转子其实没完全跟上
         *   （滑步），命令值 ≠ 真实周期 ⇒ 切过去的瞬间就错，得靠滤波慢慢收敛。
         *   实测值来自 ol_meas_cross()（开环期间"只测不换相"累计的过零间隔）
         *   ⇒ 天然与转子同步 ⇒ 切过去即吻合，不再有"头几步跟丢"。
         *   合理性检查：只接受命令值 0.5~2 倍之间的结果，避免把噪声当周期。 */
        {
            uint32_t cmd_ci = (uint32_t)ol_step_div_cur * 100u;
            uint16_t m = ol_meas_ci;
            /* ★★★ 2026-10-10 窗口从 0.5~2 倍放宽到 1/8~8 倍 ★★★
             *   实测教训：开环滑步可能远超 2 倍 —— 实测 ol_meas_ci=10743 而命令值 2400
             *   （滑步 4.5 倍，转子 310RPM vs 磁场 1389RPM），于是实测值被我原来的
             *   `m <= cmd_ci<<1`(=4800) 挡掉、退回命令值 ⇒ 交棒后还得靠滤波收敛 1 秒。
             *   放宽到 1/8~8 倍即可覆盖真实滑步，同时仍能挡掉离谱噪声。 */
            if (m >= 200u && (uint32_t)m >= (cmd_ci >> 3) && (uint32_t)m <= (cmd_ci << 3)) {
                commutation_interval = (uint32_t)m;   /* 用实测（跟随转子）*/
                ol_meas_used = m;
            } else {
                commutation_interval = cmd_ci;        /* 退回命令值 */
                ol_meas_used = 0;
            }
        }
        thiszctime = (uint16_t)commutation_interval;
        lastzctime = thiszctime;

        /* ★★★ 2026-10-10 关键修复：交棒时把"六步环形缓冲"一并填上实测周期 ★★★
         *   用户提问："为什么开环频率这么高能跟上且转速稳定，但一闭环频率就降低？"
         *   —— 问对了。真相是【闭环自己把周期算错了】，不是电机跟不上：
         *     · 开环时转子确实跟着 1818Hz 跑（ol_meas_ci = 1089 ≈ 命令 1100，±1%）；
         *     · 但 commutation_interval 现在直接取自 commutation_intervals[] 的六步平均，
         *       而那 6 个槽【只在闭环的 PeriodElapsedCallback 里写】——
         *       开环期间那个函数不跑 ⇒ 交棒那一刻环里装的是【上一次运行的残留值】
         *       （上一次因母线被拉塌、停在 OL_CI_FLOOR_DIV 附近 ≈13000）
         *       ⇒ 六步平均立刻给出一个错约 9 倍的周期 ⇒ 闭环锁错 ⇒ 频率一交棒就掉。
         *   ⇒ 交棒时把 6 个槽全部填成刚定下的周期（实测优先），闭环第一拍就是对的。
         *   顺带把统计量 average_interval / last_average_interval 也对齐，
         *   免得 ISR 里 average_interval < 50 那条捷径把窗口判据短路掉。 */
        {
            uint8_t k;
            for (k = 0; k < 6u; k++) {
                commutation_intervals[k] = (uint16_t)commutation_interval;
            }
            average_interval = commutation_interval;
            last_average_interval = average_interval;
        }
        if (!eepromBuffer.auto_advance) {
            advance = (uint16_t)((commutation_interval * temp_advance) >> 6);
        } else {
            advance = (uint16_t)((commutation_interval * auto_advance_level) >> 6);
        }
        /* ★★★ 2026-10-10 相位连续交棒 ★★★
         *   原来这里一律用"过零后 0.5 步(理想 30°)"当 waitTime，并把定时器重设到半步处
         *   —— 等于在交棒瞬间把相位【往前跳】。而开环实测真过零落在 0.91 步处
         *   （拒绝环 CNT≈1000 / 步长 1100）⇒ 开环的换相点离过零只有 0.09 步
         *   ⇒ 交棒按 0.5 步排 ⇒ 相位前跳 0.41 步 ⇒ 转子当场失同步
         *   ⇒ 边沿从 ~1400/秒 塌到 ~130/秒 ⇒ 被慢速磁场拖住 ⇒ 稳定错误锁定（现场抖动）。
         *   改用开环期间实测的 gap(≈0.09 步)：相位接着开环走，不跳。
         *   合理性检查：gap 必须在 (0, ci/2] 之间，否则退回原式。 */
        if (ol_meas_gap > 0u && (uint32_t)ol_meas_gap <= (commutation_interval >> 1)) {
            waitTime = ol_meas_gap;                          /* 相位连续 */
            ol_meas_gap_used = ol_meas_gap;
        } else {
            waitTime = (uint16_t)((commutation_interval >> 1) - advance);
            ol_meas_gap_used = 0;
        }
        /* 定时器【不再重设】：它一直按开环的节奏在走，保持相位连续。
         * （原来 SET_INTERVAL_TIMER_COUNT(ci>>1) 会把相位强行挪到半步处。） */
        ol_meas_gap_used = waitTime;
#if RM2027_ZCL_FILTER
        /* ★ 2026-10-14 丢过零滤波：交棒后第一拍的超时阈值基准 = 本次相位连续的 waitTime。
         *   （之后每次正常换相由 PeriodElapsedCallback 刷新；超时换相后清 0。） */
        zcl_pred = waitTime;
#endif
        /* ★ 交棒后第一帧也屏蔽掉：交棒点落在一个步长的中途，这一帧的 delta 同样
         *   不能代表"新的悬空相"（悬空相还没换，但定时基准刚被重置），
         *   顺手丢弃它的边沿，避免又给正反馈送第一脚。 */
        bemf_blank = BEMF_BLANK_FRAMES;   /* 与 changeCompInput 一致：屏蔽 10 帧 */
        enableCompInterrupts();
    }
}
#endif /* !RM2027_OL_ARM_ONLY */

#if RM2027_OL_ARM_ONLY
/* 开环独占版：给闭环起步链提供空实现，主循环/ISR 不必分叉。
 * （主循环里两条路是 #if 分开的，这里只为消掉"定义了没用"的告警。） */
#undef  OL_STARTUP_ACTIVE
#define OL_STARTUP_ACTIVE() (0)
__attribute__((unused)) static void olStartupTick(void) { }
__attribute__((unused)) void ol_startup_zc_inc(void) { }
#endif

////Space Vector PWM ////////////////
// const int pwmSin[] ={128, 132, 136, 140, 143, 147, 151, 155, 159, 162, 166,
// 170, 174, 178, 181, 185, 189, 192, 196, 200, 203, 207, 211, 214, 218, 221,
// 225, 228, 232, 235, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248,
// 248, 249, 250, 250, 251, 252, 252, 253, 253, 253, 254, 254, 254, 255, 255,
// 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 253,
// 253, 253, 252, 252, 251, 250, 250, 249, 248, 248, 247, 246, 245, 244, 243,
// 242, 241, 240, 239, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248,
// 248, 249, 250, 250, 251, 252, 252, 253, 253, 253, 254, 254, 254, 255, 255,
// 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 253,
// 253, 253, 252, 252, 251, 250, 250, 249, 248, 248, 247, 246, 245, 244, 243,
// 242, 241, 240, 239, 238, 235, 232, 228, 225, 221, 218, 214, 211, 207, 203,
// 200, 196, 192, 189, 185, 181, 178, 174, 170, 166, 162, 159, 155, 151, 147,
// 143, 140, 136, 132, 128, 124, 120, 116, 113, 109, 105, 101, 97, 94, 90, 86,
// 82, 78, 75, 71, 67, 64, 60, 56, 53, 49, 45, 42, 38, 35, 31, 28, 24, 21, 18,
// 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 8, 7, 6, 6, 5, 4, 4, 3, 3, 3, 2, 2, 2,
// 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 5, 6, 6, 7, 8,
// 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9,
// 8, 8, 7, 6, 6, 5, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
// 1, 2, 2, 2, 3, 3, 3, 4, 4, 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 15, 16,
// 17, 18, 21, 24, 28, 31, 35, 38, 42, 45, 49, 53, 56, 60, 64, 67, 71, 75, 78,
// 82, 86, 90, 94, 97, 101, 105, 109, 113, 116, 120, 124};

////Sine Wave PWM ///////////////////
int16_t pwmSin[] = {
    180, 183, 186, 189, 193, 196, 199, 202, 205, 208, 211, 214, 217, 220, 224,
    227, 230, 233, 236, 239, 242, 245, 247, 250, 253, 256, 259, 262, 265, 267,
    270, 273, 275, 278, 281, 283, 286, 288, 291, 293, 296, 298, 300, 303, 305,
    307, 309, 312, 314, 316, 318, 320, 322, 324, 326, 327, 329, 331, 333, 334,
    336, 337, 339, 340, 342, 343, 344, 346, 347, 348, 349, 350, 351, 352, 353,
    354, 355, 355, 356, 357, 357, 358, 358, 359, 359, 359, 360, 360, 360, 360,
    360, 360, 360, 360, 360, 359, 359, 359, 358, 358, 357, 357, 356, 355, 355,
    354, 353, 352, 351, 350, 349, 348, 347, 346, 344, 343, 342, 340, 339, 337,
    336, 334, 333, 331, 329, 327, 326, 324, 322, 320, 318, 316, 314, 312, 309,
    307, 305, 303, 300, 298, 296, 293, 291, 288, 286, 283, 281, 278, 275, 273,
    270, 267, 265, 262, 259, 256, 253, 250, 247, 245, 242, 239, 236, 233, 230,
    227, 224, 220, 217, 214, 211, 208, 205, 202, 199, 196, 193, 189, 186, 183,
    180, 177, 174, 171, 167, 164, 161, 158, 155, 152, 149, 146, 143, 140, 136,
    133, 130, 127, 124, 121, 118, 115, 113, 110, 107, 104, 101, 98, 95, 93,
    90, 87, 85, 82, 79, 77, 74, 72, 69, 67, 64, 62, 60, 57, 55,
    53, 51, 48, 46, 44, 42, 40, 38, 36, 34, 33, 31, 29, 27, 26,
    24, 23, 21, 20, 18, 17, 16, 14, 13, 12, 11, 10, 9, 8, 7,
    6, 5, 5, 4, 3, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 5, 5,
    6, 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 18, 20, 21, 23,
    24, 26, 27, 29, 31, 33, 34, 36, 38, 40, 42, 44, 46, 48, 51,
    53, 55, 57, 60, 62, 64, 67, 69, 72, 74, 77, 79, 82, 85, 87,
    90, 93, 95, 98, 101, 104, 107, 110, 113, 115, 118, 121, 124, 127, 130,
    133, 136, 140, 143, 146, 149, 152, 155, 158, 161, 164, 167, 171, 174, 177
};

// int sin_divider = 2;
int16_t phase_A_position;
int16_t phase_B_position;
int16_t phase_C_position;
uint16_t step_delay = 100;
char stepper_sine = 0;
char forward = 1;
uint16_t gate_drive_offset = DEAD_TIME;

uint8_t stuckcounter = 0;
uint16_t k_erpm;
uint16_t e_rpm; // electrical revolution /100 so,  123 is 12300 erpm

uint16_t adjusted_duty_cycle;

uint8_t bad_count = 0;
uint8_t bad_count_threshold = CPU_FREQUENCY_MHZ / 24;
uint8_t dshotcommand;
uint16_t armed_count_threshold = 1000;

volatile char armed = 0;
uint16_t zero_input_count = 0;

uint16_t input = 0;
volatile uint16_t newinput = 0;
volatile char inputSet = 0;
char dshot = 0;
volatile char servoPwm = 0;
volatile uint32_t zero_crosses;

volatile uint8_t zcfound = 0;

volatile uint8_t bemfcounter;
uint8_t min_bemf_counts_up = TARGET_MIN_BEMF_COUNTS;
uint8_t min_bemf_counts_down = TARGET_MIN_BEMF_COUNTS;

volatile uint16_t lastzctime;
volatile uint16_t thiszctime;

volatile uint16_t duty_cycle = 0;
char step = 1;
volatile uint32_t commutation_interval = 12500;
volatile uint16_t waitTime = 0;
uint16_t signaltimeout = 0;
uint8_t ubAnalogWatchdogStatus = RESET;

#if defined(NEED_INPUT_READY) || defined(NXP)
volatile char input_ready = 0;
#endif

int32_t doPidCalculations(struct fastPID* pidnow, int actual, int target)
{

    pidnow->error = actual - target;
    pidnow->integral = pidnow->integral + pidnow->error * pidnow->Ki;
    if (pidnow->integral > pidnow->integral_limit) {
        pidnow->integral = pidnow->integral_limit;
    }
    if (pidnow->integral < -pidnow->integral_limit) {
        pidnow->integral = -pidnow->integral_limit;
    }

    pidnow->derivative = pidnow->Kd * (pidnow->error - pidnow->last_error);
    pidnow->last_error = pidnow->error;

    pidnow->pid_output = pidnow->error * pidnow->Kp + pidnow->integral + pidnow->derivative;

    if (pidnow->pid_output > pidnow->output_limit) {
        pidnow->pid_output = pidnow->output_limit;
    }
    if (pidnow->pid_output < -pidnow->output_limit) {
        pidnow->pid_output = -pidnow->output_limit;
    }
    return pidnow->pid_output;
}

void loadEEpromSettings()
{
    read_flash_bin(eepromBuffer.buffer, eeprom_address, sizeof(eepromBuffer.buffer));
    /* ★ 校准流程（宏控制，不依赖 SWD）：
     *   默认：先读 flash 校验 → 合法就用；不合法（或 XIEN_CAL_FORCE=1）→ 自动校准 → 存回 flash */
    esc_cal_load();
    /* ★★ flash 持久化（应用侧）：加载成功 ⇒ 立刻覆盖判据用的 RAM 表 ★★
     *   校准结果存在 flash 最后一页(0x0801F800+0x100)，而 app 区只到 0x0800F800
     *   ⇒ esc-lab.sh 的 `flash write_image erase <bin> 0x08001000` 只擦 app 扇区，
     *     【重烧固件不会丢掉校准结果】。 */
    if (esc_cal_state == 1u) {
        /* ★ 只有"看起来合理"的 flash 表才采用（防止历史上写入的退化表把补偿关掉）：
         *   6 个增益都在 [700,3500] 且不全为 1000。不合格 ⇒ 保留编译期/烘焙的默认表。*/
        uint8_t _ok = 1u, _diff = 0u;
        for (uint8_t i = 1u; i <= 6u; i++) {
            if (esc_cal.sg[i] < 700 || esc_cal.sg[i] > 3500) { _ok = 0u; }
            if (esc_cal.sg[i] != 1000) { _diff = 1u; }
        }
        if (_ok && _diff) {
            for (uint8_t i = 0; i < 7u; i++) { cal_sg[i] = esc_cal.sg[i]; cal_so[i] = esc_cal.so[i]; }
        }
    }
#if XIEN_CAL_ENABLE
    if (esc_cal_state != 1u || XIEN_CAL_FORCE) {
        esc_cal_auto();
        (void)esc_cal_save(&esc_cal);
    }
#endif
    if(eepromBuffer.eeprom_version < 3){ // eeprom versions less than 3 had a firmware name string in these bytes 
      eepromBuffer.max_ramp = 160;    // 0.1% per ms to 25% per ms 
      eepromBuffer.minimum_duty_cycle = 1; // 0.2% to 51 percent
      eepromBuffer.disable_stick_calibration = 0; // 
      eepromBuffer.absolute_voltage_cutoff = 10;  // voltage level 1 to 100 in 0.5v increments
      eepromBuffer.current_P = 100; // 0-255
      eepromBuffer.current_I = 0; // 0-255
      eepromBuffer.current_D = 100; // 0-255
      eepromBuffer.active_brake_power = 0; // 1-5 percent duty cycle
      eepromBuffer.brake_on_zero_throttle = 0;
      eepromBuffer.reserved_eeprom_3[0] = 0; //14-16  for crsf input
      eepromBuffer.reserved_eeprom_3[1] = 0;
      eepromBuffer.reserved_eeprom_3[2] = 0;
    }
    if(eepromBuffer.brake_on_zero_throttle > 9){ // byte 13 held a firmware name character (0x30 or similar) before eeprom version 4
      eepromBuffer.brake_on_zero_throttle = 0;
    }
    // eepromBuffer.advance_level can either be set to 0-3 with config tools less than 1.90 or 10-42 with 1.90 or above 
    if (eepromBuffer.advance_level > 42 || (eepromBuffer.advance_level < 10 && eepromBuffer.advance_level > 3)){
        temp_advance = 16;
    }
    if (eepromBuffer.advance_level < 4) {         // old format needs to be converted to 0-32 range
        temp_advance = (eepromBuffer.advance_level<<3);
        eepromBuffer.advance_level = temp_advance + 10;
    }
    if (eepromBuffer.advance_level < 43 && eepromBuffer.advance_level > 9 ) { // new format subtract 10 from advance
        temp_advance = eepromBuffer.advance_level - 10;
    }

    if (eepromBuffer.pwm_frequency < 145 && eepromBuffer.pwm_frequency > 7) {
      int divider = eepromBuffer.pwm_frequency * 100 / 6;
      TIMER1_MAX_ARR =   TIM1_AUTORELOAD * 400 / divider;
      SET_AUTO_RELOAD_PWM(TIMER1_MAX_ARR);
    } else {
      tim1_arr = TIM1_AUTORELOAD;
      SET_AUTO_RELOAD_PWM(tim1_arr);
    }
    if(eepromBuffer.minimum_duty_cycle < 51 && eepromBuffer.minimum_duty_cycle > 0){
    minimum_duty_cycle = eepromBuffer.minimum_duty_cycle * 10;
    }else{
    minimum_duty_cycle = 0;
    }
    if (eepromBuffer.startup_power < 151 && eepromBuffer.startup_power > 49) {
            min_startup_duty = minimum_duty_cycle + eepromBuffer.startup_power;
    } else {
        min_startup_duty = minimum_duty_cycle;
    }
    startup_max_duty_cycle = minimum_duty_cycle + 400;  

    motor_kv = (eepromBuffer.motor_kv * 40) + 20;
#ifdef THREE_CELL_MAX
		motor_kv =  motor_kv / 2;
#endif
#ifdef ONE_TWO_CELL_MAX
		motor_kv =  motor_kv / 16;
#endif
    setVolume(2);
    if (eepromBuffer.eeprom_version > 0) { // these commands weren't introduced until eeprom version 1.
#ifdef CUSTOM_RAMP

#else
        if (eepromBuffer.beep_volume > 11) {
            setVolume(5);
        } else {
            setVolume(eepromBuffer.beep_volume);
        }
#endif
        servo_low_threshold = (eepromBuffer.servo.low_threshold * 2) + 750; // anything below this point considered 0
        servo_high_threshold = (eepromBuffer.servo.high_threshold * 2) + 1750; // anything above this point considered 2000 (max)
        servo_neutral = (eepromBuffer.servo.neutral) + 1374;
        servo_dead_band = eepromBuffer.servo.dead_band;

        low_cell_volt_cutoff = eepromBuffer.low_cell_volt_cutoff + 250; // 2.5 to 3.5 volts per cell range
        
        
#ifndef HAS_HALL_SENSORS
        eepromBuffer.use_hall_sensors = 0;
#endif

        if (eepromBuffer.sine_mode_changeover_thottle_level < 5 || eepromBuffer.sine_mode_changeover_thottle_level > 25) { // sine mode changeover 5-25 percent throttle
            eepromBuffer.sine_mode_changeover_thottle_level = 5;
        }
        if (eepromBuffer.drag_brake_strength == 0 || eepromBuffer.drag_brake_strength > 10) { // drag brake 1-10
            eepromBuffer.drag_brake_strength = 10;
        }

        if (eepromBuffer.driving_brake_strength == 0 || eepromBuffer.driving_brake_strength > 9) { // motor brake 1-9
            eepromBuffer.driving_brake_strength = 10;
        }

        if(eepromBuffer.driving_brake_strength < 10){
            dead_time_override = DEAD_TIME + (150 - (eepromBuffer.driving_brake_strength * 10));
            if (dead_time_override > 200) {
                dead_time_override = 200;
            }
        min_startup_duty = min_startup_duty + dead_time_override;
        minimum_duty_cycle = minimum_duty_cycle + dead_time_override;
        throttle_max_at_low_rpm = throttle_max_at_low_rpm + dead_time_override;
        startup_max_duty_cycle = startup_max_duty_cycle + dead_time_override;
#ifdef STMICRO
        TIM1->BDTR |= dead_time_override;
#endif
#ifdef ARTERY
        TMR1->brk |= dead_time_override;
#endif
#ifdef GIGADEVICES
        TIMER_CCHP(TIMER0) |= dead_time_override;
#endif
#ifdef NXP
    	for (int submodule = 0; submodule <= 2; submodule++) {
    		FLEXPWM0->SM[submodule].DTCNT0 = PWM_DTCNT0_DTCNT0(dead_time_override);	//PWMA deadtime
    		FLEXPWM0->SM[submodule].DTCNT1 = PWM_DTCNT1_DTCNT1(dead_time_override);	//PWMB deadtime
    	}
#endif
#ifdef WCH
            TIM1->BDTR |= dead_time_override;
#endif
        }
        if (eepromBuffer.limits.temperature < 70 || eepromBuffer.limits.temperature > 140) {
            eepromBuffer.limits.temperature = 255;
        }

        if (eepromBuffer.limits.current > 0 && eepromBuffer.limits.current <= 100) {
            use_current_limit = 1;
        }
        
        currentPid.Kp = eepromBuffer.current_P*2;
        currentPid.Ki = eepromBuffer.current_I;
        currentPid.Kd = eepromBuffer.current_D*2;
        
        if (eepromBuffer.sine_mode_power == 0 || eepromBuffer.sine_mode_power > 10) {
            eepromBuffer.sine_mode_power = 5;
        }

        // unsinged int cant be less than 0
        if (eepromBuffer.input_type < 10) {
            switch (eepromBuffer.input_type) {
            case AUTO_IN:
                dshot = 0;
                servoPwm = 0;
                EDT_ARMED = 1;
                break;
            case DSHOT_IN:
                dshot = 1;
                EDT_ARMED = 1;
                break;
            case SERVO_IN:
                servoPwm = 1;
                break;
            case SERIAL_IN:
                break;
            case EDTARM_IN:
                EDT_ARM_ENABLE = 1;
                EDT_ARMED = 0;
                dshot = 1;
                break;
            };
        } else {
            dshot = 0;
            servoPwm = 0;
            EDT_ARMED = 1;
        }
        
        if(eepromBuffer.max_ramp < 10){
          ramp_divider = 9;
          max_ramp_startup = eepromBuffer.max_ramp;
          max_ramp_low_rpm = eepromBuffer.max_ramp;
          max_ramp_high_rpm = eepromBuffer.max_ramp;
        }else{
          ramp_divider = 0;
          if((eepromBuffer.max_ramp / 10) < max_ramp_startup){
            max_ramp_startup = eepromBuffer.max_ramp / 10;
          }
          if((eepromBuffer.max_ramp / 10) < max_ramp_low_rpm){
            max_ramp_low_rpm = eepromBuffer.max_ramp / 10;
          }
          if((eepromBuffer.max_ramp / 10) < max_ramp_high_rpm){
            max_ramp_high_rpm = eepromBuffer.max_ramp / 10;
          }
        }
        
        if (motor_kv <= 20) {
            low_rpm_throttle_limit = 0;
        }
        // guard divisions for an erased eeprom (motor_poles 0 or 0xff),
        // ARM hardware division returns 0 but it is UB in C
        uint8_t rpm_level_div = 0;
        if (eepromBuffer.motor_poles != 0) {
            rpm_level_div = 32 / eepromBuffer.motor_poles;
        }
        if (rpm_level_div != 0) {
            low_rpm_level = motor_kv / 100 / rpm_level_div;
            high_rpm_level = motor_kv / 12 / rpm_level_div;
        } else {
            low_rpm_level = 0;
            high_rpm_level = 0;
        }
    }
    reverse_speed_threshold = map(motor_kv, 300, 3000, 1000, 500);
    if (eepromBuffer.bi_direction){
      polling_mode_changeover = POLLING_MODE_THRESHOLD / 2;
    }else{
      polling_mode_changeover = POLLING_MODE_THRESHOLD;
    }
    temp_comp_pwm = eepromBuffer.comp_pwm;
}

void saveEEpromSettings()
{
    save_flash_nolib(eepromBuffer.buffer, sizeof(eepromBuffer.buffer), eeprom_address);
}

uint16_t getSmoothedCurrent()
{
    total = total - readings[readIndex];
    readings[readIndex] = ADC_raw_current;
    total = total + readings[readIndex];
    readIndex = readIndex + 1;
    if (readIndex >= numReadings) {
        readIndex = 0;
    }
    smoothedcurrent = total / numReadings;
    return smoothedcurrent;
}

void getBemfState()
{
    uint8_t current_state = 0;
#if defined(MCU_F031) || defined(MCU_G031)
    if (step == 1 || step == 4) {
        current_state = PHASE_C_EXTI_PORT->IDR & PHASE_C_EXTI_PIN;
    }
    if (step == 2 || step == 5) { //        in phase two or 5 read from phase A Pf1
        current_state = PHASE_A_EXTI_PORT->IDR & PHASE_A_EXTI_PIN;
    }
    if (step == 3 || step == 6) { // phase B pf0
        current_state = PHASE_B_EXTI_PORT->IDR & PHASE_B_EXTI_PIN;
    }
#else
    //Get current comparator output level
    current_state = !getCompOutputLevel(); // polarity reversed
#endif
    if (rising) {
        if (current_state) {
            bemfcounter++;
        } else {
            bad_count++;
            if (bad_count > bad_count_threshold) {
                bemfcounter = 0;
            }
        }
    } else {
        if (!current_state) {
            bemfcounter++;
        } else {
            bad_count++;
            if (bad_count > bad_count_threshold) {
                bemfcounter = 0;
            }
        }
    }
}

/* ---- 串口换相事件流 / 窗口流：统一发送仲裁 + proto2 + ACK/NAK/重传 ---- */
/* ★★ 2026-10-18 xien/n4-serial-ack ★★
 *   在 WIP(proto2 magic8+CRC16) 的包格式上重做发送侧，修掉 WIP 实测的
 *   "tx_busy 卡 1 / tx_stuck_n 8176 / CRC 11/11 全失败 / ~25KB 1e04ac00 洪流"：
 *     ① tx_busy 必须在 LL_DMA_EnableChannel() 【之前】置位。WIP 之后置位：
 *        DMA 短包/TC 中断先到 ⇒ tx_busy 永远卡 1（实锤）。
 *     ② 所有发送（窗口流 / 换相事件流 / KISS 遥测）只允许走 uart_tx_submit()
 *        一个入口。绝不允许各流自己 DisableChannel 改地址/长度 —— 那正是 WIP
 *        链路里"有结构假数据 / 整包腰斩"的来源（同一条 DMA1_CH3 被半途换源）。
 *     ③ 发送前统一钉 TE=1（UT1 半双工，TE=0 时 TXE 恒 1、DMA 假装传完）。
 *     ④ 看门狗只在"EN=1 且 CNDTR=0 持续 >5ms"或"超过按字节数算的时限"才强杀，
 *        不把"TC 中断还没跑"的正常收尾误判成卡死（WIP tx_stuck_n 爆炸的原因）。
 *
 *   物理链路：PB6 单脚 + HDSEL 半双工（serial_telemetry.c 确认没有 PB7/RX 脚）。
 *   ⇒ ESC 发送期间 PC 的命令必丢；所以每发完一个窗，等最后一 bit 移出后切 RX
 *      开 ack_wait_ms 的"收命令窗"，PC 只能在这个窗内回 ACK/NAK/SET。
 *
 *   下行窗口包 proto2（PC 侧 tools/win-proto2.py 解析）：
 *     [0..7] magic A5 5A A5 5A C3 3C 96 69
 *     [8] ver=0x02 [9] type=0x01 [10..11] len=512*12
 *     [12..13] seq u16 [14..17] ts u32 [18..19] n=512
 *     [20..]   payload 512×12B（VA,VB,VC,V0,电流,step,dbg）
 *     [..]     crc16 = CRC-16/CCITT-FALSE over [8 .. 20+len-1]
 *   上行命令帧（PC→ESC，7B）：
 *     C3 3C | len=3 | cmd | arg_lo | arg_hi | crc8(poly07 over len..arg)
 *     cmd: 01=ACK(seq) 02=NAK(seq) 03=RESEND(seq) 04=SET(win_every_n)
 *
 *   重传：stop-and-wait。win_pkt 保留最近一窗直到 ACK；收到 NAK/RESEND 或
 *   收窗超时 → 原样重发同一窗，最多 WIN_RETRY_MAX 次后放弃计数。重发永远
 *   优先于新窗，且与窗口流复用同一 tx_busy 仲裁 ⇒ 不会抢同一条 DMA 半途换源。 */
volatile uint8_t tx_busy = 0;          /* 1 = DMA1_CH3 正在发（所有流共用）*/
static volatile uint8_t  tx_kind = 0;
static volatile uint16_t tx_len = 0;
static volatile uint32_t tx_start_ms = 0;   /* ms10k_cnt（0.1ms）*/
static volatile uint32_t tx_zero_ms  = 0;

/* ---- 观测计数（全部 SWD 可读：tools/esc-dbg.sh peek <名>）---- */
volatile uint32_t tx_stuck_n     = 0;  /* EN=1/CNDTR=0 持续超时强杀次数 */
volatile uint32_t tx_timeout_n   = 0;  /* 超过按长度算的时限强杀次数 */
volatile uint32_t tx_te_fix_n    = 0;  /* 发送前发现 TE=0 并修回次数 */
volatile uint32_t tx_dma_err_n   = 0;  /* DMA TE3 错误次数 */
volatile uint32_t tx_retry_n     = 0;  /* 实际重发窗口次数 */
volatile uint32_t win_giveup_n   = 0;  /* 重发超上限放弃的窗数 */
volatile uint32_t pkt_crc_fail_n = 0;  /* 收到 NAK/RESEND（主机报坏包）次数 */
volatile uint32_t win_ack_n      = 0;  /* 收到合法 ACK 的窗数 */
volatile uint32_t rx_cmd_n       = 0;  /* 收到合法上行命令数 */
volatile uint32_t rx_crc_fail_n  = 0;  /* 上行帧 CRC/格式错次数 */
volatile uint32_t rx_resync_n    = 0;  /* 上行字节流重同步次数 */

#define UART_KIND_NONE   0u
#define UART_KIND_WINDOW 1u
#define UART_KIND_EVT    2u
#define UART_KIND_TELEM  3u

/* 上行命令 */
#define CMD_ACK     0x01u
#define CMD_NAK     0x02u
#define CMD_RESEND  0x03u
#define CMD_SET     0x04u
#define CMD_MAGIC0  0xC3u
#define CMD_MAGIC1  0x3Cu

/* ---- 窗口流协议状态 ---- */
#define WIN_RETRY_MAX 3u
volatile uint16_t ack_wait_ms = 80u;   /* 发窗后收命令窗长（SWD 可调）。8ms 时 PC 的 USB 延迟常错过 ⇒ 默认 15ms */
/* ★ 2026-10-19 默认改为 0（关闭 ACK 模式）★
 *   实测证据：本板【上行 UART 物理不存在】（只有 ESC_TX→CH340_RX）。
 *   win_ack_mode=1 时固件发一包就等 ACK ⇒ 永远等不到 ⇒ 超时+重传 ⇒
 *     win_seq +87/500ms 但 win_sent 只有 +5/500ms（吞吐塌到 1/5），
 *     且重传帧让 PC 侧解析 6940 字节却 0 个有效包（magic 命中 0）。
 *   关掉后同一场景：68 窗/5.72s、seq 严格递增、CRC 0 错 ✓
 *   ⇒ 这块板上 ACK 模式必然自毁，默认关。 */
volatile uint8_t  win_ack_mode = 0u;
/* ---- 临时调试计数（定位"收窗为何不开"）---- */
volatile uint32_t dbg_tc3win_n    = 0;  /* DMA-TC3 且 tx_kind==WINDOW 次数 */
volatile uint32_t dbg_tc3other_n  = 0;  /* DMA-TC3 但非 WINDOW 次数 */
volatile uint32_t dbg_uarttc_n    = 0;  /* USART TC 进 uart_usart_tc_isr 次数 */
volatile uint32_t dbg_listenend_n = 0;  /* win_listen_end 被调用次数 */

static volatile uint8_t  win_listening    = 0;
static volatile uint32_t win_listen_until = 0;
static volatile uint8_t  win_inflight     = 0;   /* 1 = 已发出、在等 ACK */
static volatile uint8_t  win_retry        = 0;
static volatile uint16_t win_inflight_seq = 0;
static volatile uint8_t  win_retx_pending = 0;
static volatile uint8_t  win_noack_streak = 0;
static volatile uint16_t win_skip         = 0;   /* 还要丢几个窗（win_every_n-1）*/

/* 上行接收状态机（USART1 RXNE 中断里用）。
 * 小环缓冲：一个收窗里 PC 可以连发多条（如 SET+ACK），main 一次排空再切回 TX。*/
#define RX_CMD_QN 4u
static volatile uint8_t  rx_idx = 0;
static volatile uint8_t  rx_buf[7];
static volatile uint8_t  rxq_head = 0, rxq_tail = 0;
static volatile uint8_t  rxq_cmd[RX_CMD_QN];
static volatile uint16_t rxq_arg[RX_CMD_QN];

/* 返回 1 = 可以发（所有流统一问这个）。忙由 TC3 中断清，看门狗兜底。*/
static uint8_t tx_ready(void)
{
    if (win_listening) { return 0; }        /* 半双工收窗内，谁都不许发 */
    if (tx_busy) {
        if (!LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_3)) {
            tx_busy = 0; tx_zero_ms = 0;    /* 正常收尾：TC3 中断已关通道 */
        } else if (LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_3) == 0u) {
            /* CNDTR=0 但 TC 中断还没跑（正常窗口只有几 µs）⇒ 给 5ms 宽限再强杀 */
            if (tx_zero_ms == 0u) { tx_zero_ms = ms10k_cnt | 1u; }
            else if ((uint32_t)(ms10k_cnt - tx_zero_ms) > 50u) {
                LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
                tx_busy = 0; tx_zero_ms = 0; tx_stuck_n++;
            }
        } else {
            tx_zero_ms = 0;
            /* 时限 = 字节数 × 0.7 个 0.1ms（1.5M ≈ 6.7µs/B）+ 10ms 裕量 */
            if ((uint32_t)(ms10k_cnt - tx_start_ms) >
                ((uint32_t)tx_len * 7u / 10u + 100u)) {
                LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
                tx_busy = 0; tx_timeout_n++;
            }
        }
    }
    return (uint8_t)(!tx_busy);
}

/* 统一发送入口：调用者必须先 tx_ready()==1 */
static void uart_tx_submit(uint8_t *p, uint16_t len, uint8_t kind)
{
    if ((USART1->CR1 & USART_CR1_TE) == 0u) { tx_te_fix_n++; }
    LL_USART_SetTransferDirection(USART1, LL_USART_DIRECTION_TX);  /* 半双工钉死 TE=1 */
    LL_USART_ClearFlag_TC(USART1);              /* 清上一次发送残留的 TC，否则窗口包 TC 提前触发 */
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
    LL_DMA_ClearFlag_GI3(DMA1);                 /* 清残留 TC/TE 标志 */
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_3, (uint32_t)p);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, len);
    LL_USART_EnableDMAReq_TX(USART1);
    tx_kind = kind; tx_len = len;
    tx_start_ms = ms10k_cnt; tx_zero_ms = 0;
    tx_busy = 1;                                /* ★ 必须先置位再使能（WIP 卡死根因）*/
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_3);
}

/* 给 Mcu/g071/Src/serial_telemetry.c 的 KISS 遥测统一入口 */
uint8_t uart_tx_dma_ready(void) { return tx_ready(); }
void    uart_tx_dma(uint8_t *p, uint16_t len)
{
    if (!tx_ready()) { return; }
    uart_tx_submit(p, len, UART_KIND_TELEM);
}

/* ★★★ 2026-10-20 文本遥测：直接打"油门 / 转速 / 母线电压 / 母线电流" ★★★
 *   为什么要它：调参时最想知道的就是这四个量，窗口流(proto2)是给离线波形分析用的，
 *   不方便一眼看数。打开后【窗口流自动让路】（两者抢同一个 UART）。
 *   格式：一行一行，921600，约 20Hz：
 *     THR=400 RPM=123456 VBUS=11600 IBUS=1500
 *       THR  = 解码后的油门 0..2047（= input）
 *       RPM  = 电转速 erpm（e_rpm 的单位是 100 erpm）
 *       VBUS = 母线电压 mV（battery_voltage 的单位是 10mV）
 *       IBUS = 母线电流 mA（actual_current 的单位是 10mA）
 *   关掉：poke telem_text_en 0
 */
volatile uint8_t telem_text_en = 0u;
volatile uint8_t  bench_run = 0u;   /* SWD 台架油门开关 */
volatile int16_t  bench_thr = 0;    /* SWD 台架油门值 */
static char     telem_text_buf[72];
static uint32_t telem_text_last = 0u;

static char *tt_u2s(char *q, uint32_t v)
{
    char t[12]; uint8_t n = 0u;
    if (v == 0u) { *q++ = '0'; return q; }
    while (v) { t[n++] = (char)('0' + (v % 10u)); v /= 10u; }
    while (n) { *q++ = t[--n]; }
    return q;
}
static char *tt_str(char *q, const char *s) { while (*s) { *q++ = *s++; } return q; }

static void telem_text_pump(void)
{
    if (telem_text_en == 0u) { return; }
    if (!tx_ready()) { return; }
    /* 20Hz（按 24kHz 帧计数，不受主循环快慢影响）*/
    if ((uint32_t)(adc_frame_total - telem_text_last) < (24u * 50u)) { return; }
    telem_text_last = adc_frame_total;

    char *q = telem_text_buf;
    q = tt_str(q, "THR=");  q = tt_u2s(q, (uint32_t)input);
    q = tt_str(q, " RPM="); q = tt_u2s(q, (uint32_t)e_rpm * 100u);
    q = tt_str(q, " VBUS=");q = tt_u2s(q, (uint32_t)battery_voltage * 10u);
    q = tt_str(q, " IBUS=");q = tt_u2s(q, (uint32_t)((int32_t)actual_current * 10));
    *q++ = '\r'; *q++ = '\n';
    uart_tx_submit((uint8_t *)telem_text_buf, (uint16_t)(q - telem_text_buf), UART_KIND_TELEM);
}

/* CRC8（poly 0x07, init 0）：上行命令帧与换相事件帧共用 */
static uint8_t evt_crc8(const uint8_t *p, uint8_t n)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (uint8_t b = 0; b < 8u; b++) {
            if (crc & 0x80u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc = (uint8_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* ---- IRQ 回调（Mcu/g071/Src/stm32g0xx_it.c 调用）---- */

/* DMA1_CH3 TC：一个包发完。窗口包则等最后一位移出后切 RX 开收窗。*/
void uart_tx_tc3_isr(void)
{
    tx_busy = 0; tx_zero_ms = 0;
    if (tx_kind == UART_KIND_WINDOW) {
        dbg_tc3win_n++;
        /* ★ 本板上行物理未接线 ⇒ 不等 ACK、不开收窗、发完立即释放。
         *   否则 win_inflight 会永久卡 1 ⇒ win_pump 永远提前返回 ⇒ 窗口流死掉。*/
        win_inflight = 0;
        win_retx_pending = 0;
    } else {
        dbg_tc3other_n++;
    }
}
/* DMA1_CH3 TE：DMA 错误。释放锁并计数，避免永久卡死。*/
void uart_tx_err_isr(void)
{
    tx_busy = 0; tx_zero_ms = 0; tx_dma_err_n++;
    win_inflight = 0; win_retx_pending = 0;
}

/* USART1 TC：最后一 bit 已移出，切半双工 RX。*/
void uart_usart_tc_isr(void)
{
    dbg_uarttc_n++;
    LL_USART_ClearFlag_TC(USART1);
    LL_USART_DisableIT_TC(USART1);
    (void)USART1->RDR;                                     /* 清 RXNE 残留 */
    USART1->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    LL_USART_DisableDMAReq_TX(USART1);
    LL_USART_SetTransferDirection(USART1, LL_USART_DIRECTION_RX);  /* RE=1, TE=0 */
    rx_idx = 0;
    LL_USART_EnableIT_RXNE(USART1);
    win_listening = 1;
    win_listen_until = ms10k_cnt + (uint32_t)ack_wait_ms * 10u;
}

/* 收窗结束：关 RXNE、切回 TX。main 上下文调用。*/
static void win_listen_end(void)
{
    if (!win_listening) { return; }
    dbg_listenend_n++;
    LL_USART_DisableIT_RXNE(USART1);
    (void)USART1->RDR;
    USART1->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    LL_USART_SetTransferDirection(USART1, LL_USART_DIRECTION_TX);
    win_listening = 0;
}

/* USART1 收错误（ORE 等）：清标志、重同步。*/
void uart_rx_err_isr(void)
{
    (void)USART1->RDR;
    USART1->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF;
    rx_idx = 0;
    rx_resync_n++;
}

/* USART1 RXNE：上行命令字节（半双工，只可能落在收窗内）*/
void uart_rx_isr(uint8_t b)
{
    if (rx_idx == 0u) {
        if (b == CMD_MAGIC0) { rx_buf[rx_idx++] = b; }
        else { rx_resync_n++; }
        return;
    }
    if (rx_idx == 1u) {
        if (b == CMD_MAGIC1) { rx_buf[rx_idx++] = b; }
        else if (b == CMD_MAGIC0) { rx_buf[0] = b; }   /* 仍是第 1 字节 */
        else { rx_idx = 0; rx_resync_n++; }
        return;
    }
    rx_buf[rx_idx++] = b;
    if (rx_idx >= 7u) {
        uint8_t crc = evt_crc8((const uint8_t *)rx_buf, 6u);
        if (crc == rx_buf[6] && rx_buf[2] == 3u) {
            uint8_t nh = (uint8_t)((rxq_head + 1u) & (RX_CMD_QN - 1u));
            if (nh != rxq_tail) {          /* 队列满就丢最新（host 会重发命令）*/
                rxq_cmd[rxq_head] = rx_buf[3];
                rxq_arg[rxq_head] = (uint16_t)(rx_buf[4] | ((uint16_t)rx_buf[5] << 8));
                rxq_head = nh;
            }
            rx_cmd_n++;
        } else {
            rx_crc_fail_n++;
        }
        rx_idx = 0;
    }
}

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, 不反转/不异或输出) 256 项表。
 * 窗口包 6144B 载荷一次算完；放在组包后单趟扫描，直接表查（比 WIP 的 nibble
 * 表快一倍）。实测约 0.5ms/窗，而一个窗周期 ~49ms ⇒ CPU 占用 ~1%。*/
static const uint16_t crc16_tab[256] = {
    0x0000u, 0x1021u, 0x2042u, 0x3063u, 0x4084u, 0x50A5u, 0x60C6u, 0x70E7u,
    0x8108u, 0x9129u, 0xA14Au, 0xB16Bu, 0xC18Cu, 0xD1ADu, 0xE1CEu, 0xF1EFu,
    0x1231u, 0x0210u, 0x3273u, 0x2252u, 0x52B5u, 0x4294u, 0x72F7u, 0x62D6u,
    0x9339u, 0x8318u, 0xB37Bu, 0xA35Au, 0xD3BDu, 0xC39Cu, 0xF3FFu, 0xE3DEu,
    0x2462u, 0x3443u, 0x0420u, 0x1401u, 0x64E6u, 0x74C7u, 0x44A4u, 0x5485u,
    0xA56Au, 0xB54Bu, 0x8528u, 0x9509u, 0xE5EEu, 0xF5CFu, 0xC5ACu, 0xD58Du,
    0x3653u, 0x2672u, 0x1611u, 0x0630u, 0x76D7u, 0x66F6u, 0x5695u, 0x46B4u,
    0xB75Bu, 0xA77Au, 0x9719u, 0x8738u, 0xF7DFu, 0xE7FEu, 0xD79Du, 0xC7BCu,
    0x48C4u, 0x58E5u, 0x6886u, 0x78A7u, 0x0840u, 0x1861u, 0x2802u, 0x3823u,
    0xC9CCu, 0xD9EDu, 0xE98Eu, 0xF9AFu, 0x8948u, 0x9969u, 0xA90Au, 0xB92Bu,
    0x5AF5u, 0x4AD4u, 0x7AB7u, 0x6A96u, 0x1A71u, 0x0A50u, 0x3A33u, 0x2A12u,
    0xDBFDu, 0xCBDCu, 0xFBBFu, 0xEB9Eu, 0x9B79u, 0x8B58u, 0xBB3Bu, 0xAB1Au,
    0x6CA6u, 0x7C87u, 0x4CE4u, 0x5CC5u, 0x2C22u, 0x3C03u, 0x0C60u, 0x1C41u,
    0xEDAEu, 0xFD8Fu, 0xCDECu, 0xDDCDu, 0xAD2Au, 0xBD0Bu, 0x8D68u, 0x9D49u,
    0x7E97u, 0x6EB6u, 0x5ED5u, 0x4EF4u, 0x3E13u, 0x2E32u, 0x1E51u, 0x0E70u,
    0xFF9Fu, 0xEFBEu, 0xDFDDu, 0xCFFCu, 0xBF1Bu, 0xAF3Au, 0x9F59u, 0x8F78u,
    0x9188u, 0x81A9u, 0xB1CAu, 0xA1EBu, 0xD10Cu, 0xC12Du, 0xF14Eu, 0xE16Fu,
    0x1080u, 0x00A1u, 0x30C2u, 0x20E3u, 0x5004u, 0x4025u, 0x7046u, 0x6067u,
    0x83B9u, 0x9398u, 0xA3FBu, 0xB3DAu, 0xC33Du, 0xD31Cu, 0xE37Fu, 0xF35Eu,
    0x02B1u, 0x1290u, 0x22F3u, 0x32D2u, 0x4235u, 0x5214u, 0x6277u, 0x7256u,
    0xB5EAu, 0xA5CBu, 0x95A8u, 0x8589u, 0xF56Eu, 0xE54Fu, 0xD52Cu, 0xC50Du,
    0x34E2u, 0x24C3u, 0x14A0u, 0x0481u, 0x7466u, 0x6447u, 0x5424u, 0x4405u,
    0xA7DBu, 0xB7FAu, 0x8799u, 0x97B8u, 0xE75Fu, 0xF77Eu, 0xC71Du, 0xD73Cu,
    0x26D3u, 0x36F2u, 0x0691u, 0x16B0u, 0x6657u, 0x7676u, 0x4615u, 0x5634u,
    0xD94Cu, 0xC96Du, 0xF90Eu, 0xE92Fu, 0x99C8u, 0x89E9u, 0xB98Au, 0xA9ABu,
    0x5844u, 0x4865u, 0x7806u, 0x6827u, 0x18C0u, 0x08E1u, 0x3882u, 0x28A3u,
    0xCB7Du, 0xDB5Cu, 0xEB3Fu, 0xFB1Eu, 0x8BF9u, 0x9BD8u, 0xABBBu, 0xBB9Au,
    0x4A75u, 0x5A54u, 0x6A37u, 0x7A16u, 0x0AF1u, 0x1AD0u, 0x2AB3u, 0x3A92u,
    0xFD2Eu, 0xED0Fu, 0xDD6Cu, 0xCD4Du, 0xBDAAu, 0xAD8Bu, 0x9DE8u, 0x8DC9u,
    0x7C26u, 0x6C07u, 0x5C64u, 0x4C45u, 0x3CA2u, 0x2C83u, 0x1CE0u, 0x0CC1u,
    0xEF1Fu, 0xFF3Eu, 0xCF5Du, 0xDF7Cu, 0xAF9Bu, 0xBFBAu, 0x8FD9u, 0x9FF8u,
    0x6E17u, 0x7E36u, 0x4E55u, 0x5E74u, 0x2E93u, 0x3EB2u, 0x0ED1u, 0x1EF0u
};
static uint16_t __attribute__((unused)) crc16_upd(uint16_t crc, uint8_t b)
{
    return (uint16_t)((crc << 8) ^ crc16_tab[(uint8_t)((crc >> 8) ^ b)]);
}

/* ★★★ 2026-10-11 串口【窗口式全速采样】（串口测试分支固化）★★★
 *   窗内：24kHz 全速采 512 帧（≈21.3ms），一帧不抽；窗间：整窗 DMA 发。
 *   proto2 包：magic8 + ver/type/len/seq/ts/n + 512×12B + crc16。 */
#define WIN_N        128   /* 512->128：窗 5.3ms，RAM 省 13.8KB（校准构建够用）*/
#define WIN_HDR      20
#define WIN_CRC      2
#define WIN_TOTAL    (WIN_HDR + WIN_N * 12 + WIN_CRC)
volatile int16_t  win_buf[WIN_N];       /* VA（四路电压版）*/
volatile int16_t  win_vb[WIN_N];
volatile int16_t  win_vc[WIN_N];
volatile int16_t  win_v0[WIN_N];
volatile int16_t  win_cur[WIN_N];       /* 每帧电流（ADC 原始码）*/
volatile uint8_t  win_step[WIN_N];
volatile uint8_t  win_dbg[WIN_N];       /* 未钳制的步内时间（ADC 帧，饱和 255）*/
volatile uint16_t win_n;
volatile uint8_t  win_state;            /* 0=采集中 1=采满待发 */
volatile uint32_t win_seq;
volatile uint32_t win_ts;
volatile uint32_t win_sent;             /* 已组包交给 DMA 的窗数 */
volatile uint8_t  win_enable = 1;       /* SWD 可写：0=停窗口流 */
/* 窗间抽窗：1=每窗都发；N=每 N 个采满的窗只发 1 个（窗内仍 512 全采样）。
 * SWD 写：tools/esc-dbg.sh poke16 win_every_n <N>；上电初值 = targets.h 宏。*/
volatile uint16_t win_every_n = RM2027_WIN_EVERY_N;
static uint8_t    win_pkt[2][WIN_TOTAL];
static volatile uint8_t win_buf_i = 0u, win_tx_i = 0u;   /* 最近一窗：stop-and-wait 重发缓冲 */

/* 中断里调用：窗口内全速采（只写内存，绝不发送）*/
void win_push(int16_t va, int16_t vb, int16_t vc, int16_t v0, int16_t cu, uint8_t st)
{
    if (win_state != 0u) { return; }        /* 采满待发/发送中 ⇒ 本窗不再采 */
    if (win_n == 0u) { win_ts = adc_frame_total; }
    uint16_t i = win_n;
    /* ★ ISR 直写包缓冲最终位置（ping-pong）：主循环零重排、零 CRC */
    {
        uint8_t *q = &win_pkt[win_buf_i][WIN_HDR + (uint16_t)i * 12u];
        extern volatile uint16_t adc_frame_cnt;
        uint16_t el = (uint16_t)(adc_frame_cnt - ccm_last_frame);
        (void)el;   /* ★ el 不再用于 dbg（dbg 改放油门）*/
        q[ 0]=(uint8_t)( va & 0xFF); q[ 1]=(uint8_t)((va>>8)&0xFF);
        q[ 2]=(uint8_t)( vb & 0xFF); q[ 3]=(uint8_t)((vb>>8)&0xFF);
        q[ 4]=(uint8_t)( vc & 0xFF); q[ 5]=(uint8_t)((vc>>8)&0xFF);
        q[ 6]=(uint8_t)( v0 & 0xFF); q[ 7]=(uint8_t)((v0>>8)&0xFF);
        q[ 8]=(uint8_t)( cu & 0xFF); q[ 9]=(uint8_t)((cu>>8)&0xFF);
        q[10]=(uint8_t)((st & 0x07u) | ((adc_frame_order & 0x03u) << 6));
        q[11]=(uint8_t)(input >> 3);   /* ★ dbg 字节改放「油门/8」：串口数据里就能分段 */
    }
    {
        static uint8_t last_st = 0;
        uint8_t stp = (uint8_t)(st & 0x07u);
        if (adc_order_auto && stp == 1u && last_st != 1u) {
            adc_order_pending = (uint8_t)((adc_order + 1u) & 3u);
        }
        last_st = stp;
    }
    i++;
    if (i >= WIN_N) {
        win_n = WIN_N;
        win_state = 1u;                     /* 采满 ⇒ 交给主循环 */
    } else {
        win_n = i;
    }
}

/* 主循环调用：窗口协议状态机（组包 / 收命令 / ACK / 重发 / 抽窗）。
 * ★ 抽窗必须在 TX 忙时也处理：否则采集被 41ms 的 TX 节流，
 *   win_every_n=2 也拿不到全速率采样。*/
void win_pump(void)
{
    { extern volatile uint8_t telem_text_en;
      if (telem_text_en != 0u) { return; } }   /* 文本遥测优先，窗口流让路 */
    /* (1) 排空上行命令队列（一个收窗内 PC 可连发 SET+ACK）*/
    {
        uint8_t got = 0;
        while (rxq_head != rxq_tail) {
        uint8_t  c = rxq_cmd[rxq_tail];
        uint16_t a = rxq_arg[rxq_tail];
        rxq_tail = (uint8_t)((rxq_tail + 1u) & (RX_CMD_QN - 1u));
        got = 1;
        switch (c) {
        case CMD_ACK:
            if (win_inflight && (uint16_t)a == win_inflight_seq) {
                win_inflight = 0; win_retx_pending = 0; win_retry = 0;
                win_ack_n++;
            }
            break;
        case CMD_NAK:
        case CMD_RESEND:
            pkt_crc_fail_n++;
            if (win_inflight) {
                if (win_retry < WIN_RETRY_MAX) { win_retx_pending = 1; }
                else { win_inflight = 0; win_giveup_n++; }
            }
            break;
        case CMD_SET:
            if (a < 1u) { a = 1u; }
            if (a > 1000u) { a = 1000u; }
            win_every_n = a;
            break;
        default:
            break;
        }
        }   /* while rxq */
        if (got) { win_noack_streak = 0; win_listen_end(); }
    }

    if (!win_enable) {
        win_listen_end();
        win_inflight = 0; win_retx_pending = 0;
        if (win_state == 1u) { win_n = 0u; win_state = 0u; }
        return;
    }

    /* (2) 收窗超时：没等到 ACK ⇒ 超时当 NAK，重发同一窗 */
    if (win_listening) {
        if ((int32_t)(ms10k_cnt - win_listen_until) >= 0) {
            win_listen_end();
            win_noack_streak++;
            if (win_inflight) {
                if (win_retry < WIN_RETRY_MAX) { win_retx_pending = 1; }
                else { win_inflight = 0; win_giveup_n++; }
            }
        }
        return;                             /* 收窗内不发窗口/事件 */
    }

    /* (3) ★ 已删除"连续多窗无 ACK ⇒ 降级并停开收窗"的逻辑 ★
     *   根因：降级后 uart_usart_tc_isr() 直接 return ⇒ 永不开收窗 ⇒ ESC 永远 TE=1 独占总线
     *   ⇒ PC 既收不到"可发许可"，一发言又和 ESC 对撞 ⇒ 永久死锁、必须重烧才能恢复。
     *   结论：收窗必须【永远】开；无 ACK 时只放松重传力度，绝不关闭监听。*/

    /* (4) 抽窗（win_every_n）：TX 忙也要处理，丢的是 win_buf 里的窗，不碰 win_pkt */
    if (win_state == 1u && win_skip > 0u) {
        win_skip--;
        win_n = 0u; win_state = 0u; win_seq++;
        return;
    }

    /* (5) 重发优先（win_pkt 仍是那一窗，未被覆盖）*/
    if (win_retx_pending) {
        if (!tx_ready()) { return; }
        win_retx_pending = 0; win_retry++; tx_retry_n++;
        uart_tx_submit(win_pkt[win_tx_i], WIN_TOTAL, UART_KIND_WINDOW);
        return;
    }

    /* (6) 有一窗在等 ACK ⇒ win_pkt 不能覆盖 */
    if (win_inflight) { return; }

    if (win_state != 1u) { return; }
    if (!tx_ready()) { return; }

    /* (7) 组包（proto2）+ CRC16 */
    {
        const uint16_t seq  = (uint16_t)win_seq;
        const uint32_t ts   = win_ts;
        uint16_t k = 0u;
        /* ★ ping-pong：发送刚采满的那块，采集立刻切到另一块 */
        win_tx_i = win_buf_i;
        win_buf_i = (uint8_t)(win_buf_i ^ 1u);
        win_state = 0u;
        win_pkt[win_tx_i][k++] = 0xA5; win_pkt[win_tx_i][k++] = 0x5A; win_pkt[win_tx_i][k++] = 0xA5; win_pkt[win_tx_i][k++] = 0x5A;
        win_pkt[win_tx_i][k++] = 0xC3; win_pkt[win_tx_i][k++] = 0x3C; win_pkt[win_tx_i][k++] = 0x96; win_pkt[win_tx_i][k++] = 0x69;
        win_pkt[win_tx_i][k++] = 0x02u;                                   /* ver = proto2 */
        win_pkt[win_tx_i][k++] = 0x01u;                                   /* type = window */
        win_pkt[win_tx_i][k++] = (uint8_t)((WIN_N * 12u) & 0xFF);         /* len u16 */
        win_pkt[win_tx_i][k++] = (uint8_t)(((WIN_N * 12u) >> 8) & 0xFF);
        win_pkt[win_tx_i][k++] = (uint8_t)(seq & 0xFF);
        win_pkt[win_tx_i][k++] = (uint8_t)(seq >> 8);
        win_pkt[win_tx_i][k++] = (uint8_t)(ts & 0xFF);
        win_pkt[win_tx_i][k++] = (uint8_t)((ts >> 8) & 0xFF);
        win_pkt[win_tx_i][k++] = (uint8_t)((ts >> 16) & 0xFF);
        win_pkt[win_tx_i][k++] = (uint8_t)((ts >> 24) & 0xFF);
        win_pkt[win_tx_i][k++] = (uint8_t)(WIN_N & 0xFF);
        win_pkt[win_tx_i][k++] = (uint8_t)(WIN_N >> 8);
        k = (uint16_t)(WIN_HDR + WIN_N * 12u);
        win_pkt[win_tx_i][k++] = 0u;   /* 无 CRC（靠 magic/len + UART 帧同步）*/
        win_pkt[win_tx_i][k++] = 0u;
    }

    win_inflight = 1;
    win_inflight_seq = (uint16_t)win_seq;
    win_retry = 0;
    win_retx_pending = 0;
    win_skip = (uint16_t)((win_every_n > 1u) ? (win_every_n - 1u) : 0u);
    win_seq++;
    win_sent++;
    win_n = 0u; win_state = 0u;             /* 先清计数再释放！否则 ISR 看到 state=0 会向 win_buf[512] 写 */
    uart_tx_submit(win_pkt[win_tx_i], WIN_TOTAL, UART_KIND_WINDOW);
}

/* 事件流发送入口：也走统一仲裁（事件流默认关，见 targets.h RM2027_EVT_STREAM）*/
static void evt_send(uint8_t *p, uint16_t len)
{
    if (!tx_ready()) { return; }
    uart_tx_submit(p, len, UART_KIND_EVT);
}

/* 在换相那一刻打包一个事件（中断上下文：只写内存，不发送）*/
static void evt_push(void)
{
    extern volatile uint8_t ol_startup_arm;   /* 定义在别处 */
    if (!evt_enable) { return; }              /* ★ 事件流关闭：直接不组包 */
    uint16_t nxt = (uint16_t)((evt_head + EVT_FRAME) & (EVT_BUF_N - 1u));
    if (nxt == evt_tail) { evt_drop++; return; }
    uint8_t *p = (uint8_t *)&evt_buf[evt_head];
    uint16_t seq  = (uint16_t)evt_total;
    uint32_t ts   = adc_frame_total;
    int16_t  dl   = adc_bemf_get_delta();
    uint8_t  st   = (uint8_t)step;
    uint16_t wt   = waitTime;
    uint16_t dt   = duty_cycle;
    int16_t  cr   = (int16_t)actual_current;
    p[0] = 0xA5; p[1] = 0x5A; p[2] = 0xA5; p[3] = 0x5A;   /* 4 字节独特帧头 */
    p[4] = (uint8_t)(seq & 0xFF);  p[5] = (uint8_t)(seq >> 8);
    p[6] = (uint8_t)(ts & 0xFF);   p[7] = (uint8_t)((ts >> 8) & 0xFF);
    p[8] = (uint8_t)((ts >> 16) & 0xFF); p[9] = (uint8_t)((ts >> 24) & 0xFF);
    p[10] = (uint8_t)(dl & 0xFF);  p[11] = (uint8_t)((dl >> 8) & 0xFF);
    p[12] = st;
    p[13] = (uint8_t)(wt & 0xFF);  p[14] = (uint8_t)(wt >> 8);
    p[15] = (uint8_t)(dt & 0xFF);  p[16] = (uint8_t)(dt >> 8);
    p[17] = (uint8_t)(cr & 0xFF);  p[18] = (uint8_t)((cr >> 8) & 0xFF);
    {
        extern int32_t  input_override;
        extern uint16_t startup_max_duty_cycle;
        extern int16_t  use_current_limit_adjust;
        extern char     use_current_limit;
        uint16_t iov = (uint16_t)(input_override / 10000);
        p[19] = (uint8_t)(iov & 0xFF);   p[20] = (uint8_t)(iov >> 8);
        p[21] = (uint8_t)(startup_max_duty_cycle & 0xFF);
        p[22] = (uint8_t)(startup_max_duty_cycle >> 8);
        p[23] = (uint8_t)((uint16_t)use_current_limit_adjust & 0xFF);
        p[24] = (uint8_t)(((uint16_t)use_current_limit_adjust >> 8) & 0xFF);
        p[25] = (uint8_t)(commutation_interval & 0xFF);
        p[26] = (uint8_t)((commutation_interval >> 8) & 0xFF);
        p[27] = (uint8_t)((ol_startup_arm ? 1u : 0u) | (use_current_limit ? 2u : 0u));
        /* ★ 2026-10-12 加"ci 的滤波原料"：thiszctime 是 6 步平均的输入，
         *   逐事件记下来就能重建那 6 个槽 ⇒ 判断 ci 是被谁推大的。*/
        p[28] = (uint8_t)(thiszctime & 0xFF);   p[29] = (uint8_t)(thiszctime >> 8);
        p[30] = (uint8_t)(lastzctime & 0xFF);   p[31] = (uint8_t)(lastzctime >> 8);
        p[32] = (uint8_t)(average_interval & 0xFF); p[33] = (uint8_t)((average_interval >> 8) & 0xFF);
        p[34] = (uint8_t)(advance & 0xFF);      p[35] = (uint8_t)(advance >> 8);
        {
            extern uint16_t duty_cycle_setpoint, duty_cycle_maximum, input;
            extern uint8_t  bemf_timeout_happened;
            extern volatile uint32_t adc_zc_n_reject;
            extern int32_t  stall_protection_adjust;
            p[36]=(uint8_t)(duty_cycle_setpoint&0xFF); p[37]=(uint8_t)(duty_cycle_setpoint>>8);
            p[38]=(uint8_t)(duty_cycle_maximum&0xFF);  p[39]=(uint8_t)(duty_cycle_maximum>>8);
            p[40]=(uint8_t)bemf_timeout_happened;
            /* ★ 2026-10-13：p[41] 原来是保留字节(恒 0)，改记【失步判据触发计数】的低 8 位。
             *   为什么要它：用户报"转起来正常一会、突然卡顿甚至停下"——那条分支
             *   （见下面 PeriodElapsedCallback 里的 desync_check 段）会 desync_happened++
             *   并且**把 last_duty_cycle 砍到 min_startup_duty/2**，正好解释"顿一下"。
             *   但 desync_happened 原来没进载荷 ⇒ 数不出来。记在 p[41] 就不用改帧长(仍 49)。
             *   ⚠ 解析侧同步改：cap-evt.py 现在把 j+40 当 8 位 timeout、j+41 当 desync。 */
            p[41]=(uint8_t)(desync_happened & 0xFF);
            p[42]=(uint8_t)(adc_zc_n_reject&0xFF);     p[43]=(uint8_t)((adc_zc_n_reject>>8)&0xFF);
            p[44]=(uint8_t)(input&0xFF);               p[45]=(uint8_t)(input>>8);
            p[46]=(uint8_t)((uint16_t)stall_protection_adjust&0xFF);
            p[47]=(uint8_t)(((uint16_t)stall_protection_adjust>>8)&0xFF);
            p[48] = evt_crc8(p, 48);
        }
    }
    evt_head = nxt;
    evt_total++;
}

/* 主循环调用：攒够一批就用 DMA 发（不在中断里做）*/
void evt_pump(void)
{
    /* ★ xien/n1：事件流关闭时丢弃已排队事件，彻底让出串口给窗口流 */
    if (!evt_enable) { evt_tail = evt_head; return; }
    /* ★ 与窗口流共用一条 DMA ⇒ 串口忙就等下一轮（原来是私有 busy，见 evt_send 上方说明）*/
    if (!tx_ready()) { return; }
    uint16_t avail = (uint16_t)((evt_head - evt_tail) & (EVT_BUF_N - 1u));
    if (avail < EVT_BATCH) { return; }
    for (uint16_t i = 0; i < EVT_BATCH; i++) {
        evt_tx[i] = evt_buf[(uint16_t)((evt_tail + i) & (EVT_BUF_N - 1u))];
    }
    evt_tail = (uint16_t)((evt_tail + EVT_BATCH) & (EVT_BUF_N - 1u));
    evt_send((uint8_t *)evt_tx, EVT_BATCH);
}

/* ccm_record：换相历史记录（诊断'闭环抖且慢'）
 *   为什么单列成函数、且必须在【离群值钳制之前】调用：
 *   commutate() 里有一道离群值剔除，把本步实测步长钳在 [ci/4, ci*2] 之间，
 *   剧烈抖动/停顿会被抹平。若记 commutation_interval（六步平均 + 钳制），
 *   读出来是光滑曲线、看不出抖 —— 而用户肉眼看到的抖正是被这段抹掉的部分。
 *   所以这里记【未钳制的原始实测步长】，粗时间线里同时记平滑值作对比。 */
static void ccm_record(uint32_t raw_step)
{
    (void)raw_step;
    uint16_t _ci = ccm_head;
    /* ★ 真实换相间隔 = 相邻两次换相之间过了多少个 ADC 帧（41.67us/帧）。
     *   这个量只依赖 ISR 里的帧计数，不依赖过零检测认没认到 ⇒ 可信。 */
    {
        uint16_t f = adc_frame_cnt;
        ccm_ci[_ci] = (uint16_t)(f - ccm_last_frame);
        ccm_last_frame = f;
    }
    evt_push();          /* ★ 串口事件流：每笔换相打包一个事件 */
    ccm_step[_ci] = (uint8_t)step;
    ccm_head = (uint16_t)((_ci + 1u) & (CCM_N - 1u));
    ccm_total++;
    if ((ccm_total & (CCM_SLOW_DIV - 1u)) == 0u) {
        uint16_t _si = ccm_slow_head;
        ccm_slow_ci[_si]   = (uint16_t)commutation_interval;
        ccm_slow_duty[_si] = duty_cycle;
        ccm_slow_cur[_si]  = (uint16_t)actual_current;
        ccm_slow_iov[_si]  = (uint32_t)input_override;
        ccm_slow_in[_si]   = input;
        ccm_slow_sp[_si]   = duty_cycle_setpoint;
        ccm_slow_head = (uint16_t)((_si + 1u) & (CCM_SLOW_N - 1u));
        ccm_slow_total++;
    }
}

void commutate()
{
    if (forward == 1) {
        step++;
        if (step > 6) {
            step = 1;
            desync_check = 1;
        }
        rising = step % 2;
    } else {
        step--;
        if (step < 1) {
            step = 6;
            desync_check = 1;
        }
        rising = !(step % 2);
    }
#ifdef INVERTED_EXTI
    rising = !rising;
#endif
    __disable_irq(); // don't let dshot interrupt
    if (!prop_brake_active) {
        comStep(step);
    }
    __enable_irq();
    changeCompInput();
#ifndef NO_POLLING_START
	if (average_interval > polling_mode_changeover + 500) {
      old_routine = 1;
   }
#endif
    bemfcounter = 0;
    zcfound = 0;
    /* ★★★ 2026-10-10 改存【本步实测步长】，不再存滤波后的 ci ★★★
     *   原来存的是 commutation_interval（滤波输出）⇒ 6 个槽其实是【同一个值】，
     *   下面那个"6 步平均"等于白算、没有任何额外平滑作用。
     *   参考 TI SPRABZ3《Sensorless Trapezoidal Control of BLDC Motors》的
     *   "Commutation Instants Computation"：30° 换相延迟应当基于【上一整转】的计时器 ——
     *       cmtn_delay = v_timer / 12   （v_timer = 6 个换相步的计数）
     *   ⇒ 把每一步的实测步长存进环里，再用六步和 ÷6 作为换相周期
     *     （= TI 的 v_timer/12 的两倍 = 一个完整换相步）。 */
    {
        uint32_t meas_step = (uint32_t)((lastzctime + thiszctime) >> 1);
        /* ★ 记本步的【过零间隔】(thiszctime = 两次过零之间的时间 = 一个换相步)。
         *   为 0 就代表这一步【没有过零事件】、是被超时兜底硬踹换相的
         *   —— 那正是"抖/失步"的直接证据（开环的换相由固定时基给，不会有 0）。 */
        ccm_record(thiszctime);
        /* ★★★ 2026-10-10 关键修复⑨：离群值剔除 —— 单次离谱的间隔不许进六步平均 ★★★
         *   台架实测（闭环交棒后约 10ms 处）：转子瞬时停顿一次 ⇒ 本步实测
         *     284 点 = 11.8ms = 23600 拍（正常步长是 1080 拍，这是它的 22 倍）。
         *   它一旦进环，六步和÷6 立刻把 ci 从 ~1330 抬到 ~12500
         *   ⇒ waitTime = ci/2 从 0.33ms 变成 3.25ms ⇒ 磁场反过来拖住转子
         *   ⇒ 实测间隔继续变长 ⇒ ci 一路爬到上限 13000
         *   ⇒【稳定地锁死在慢速（≈154~540 步/秒 vs 开环 1793），且无恢复路径】。
         *   这正是用户最早那句"开环频率这么高能跟上、一闭环频率就降"的成因。
         *   ⚠ 这一步的间隔本来就不可信：它是在"转子失步/停顿"那次扰动里量到的，
         *     不反映转子真实的转速。
         *   ⇒ 把每一步的实测值钳在【当前 ci 的 1/4 ~ 2 倍】之间。
         *     正常跟踪时每步只变千分之几，完全不受影响；真正的调速（能持续多步）
         *     也能在几十步内到位。而单次扰动最多只能把平均推 17%（2 倍÷6 步）。
         *   ★ 注意这里钳的是【存进环里的值】，不是 ci 本身 —— 不改变任何滤波/限速
         *     逻辑，只把明显不该信的样本挡在外面。 */
        /* ★★★ 2026-10-12 ci 合理性闸：只接受当前 ci 的 ±25%（原来是 ci/4 ~ 2ci）★★★
         *   实测（五跑补偿实验）：闭环一旦认到多余的跨零，本步间隔就被记小；而六步环
         *   允许到 ci/4 ⇒ ci 被拉小 ⇒ waitTime=比例×ci 变小 ⇒ 换相提前 ⇒ 间隔更短
         *   …… 一路往下冲（ci 从 0.68ms 掉到 0.19ms，而转子真实一步 ≈5.3ms）
         *   ⇒ 固件按 25 倍于转子的速率换相 ⇒ 转子跟不上 ⇒ 卡一下冲一下、电机只嗡。
         *   ⇒ 每格只收【当前 ci 的 ±25%】，越界丢弃并计数。
         *   ⚠ 起步/斜坡段 ci 每步只变约 6% ⇒ ±25% 不影响正常跟踪。 */
        uint32_t lo = commutation_interval - (commutation_interval >> 2);
        uint32_t hi = commutation_interval + (commutation_interval >> 2);
        if (lo < 64u) { lo = 64u; }
        if (meas_step < lo) { ci_clamp_rejects++; meas_step = lo; }
        if (meas_step > hi) { ci_clamp_rejects++; meas_step = hi; }
        commutation_intervals[step - 1] =
            (uint16_t)((meas_step > 0xFFFFu) ? 0xFFFFu : meas_step);
    }

    /* ★★★ 2026-10-09 决定性埋点：记录【换相那一刻】的 "悬空相 − V0" ★★★
     *   它直接回答"换相落在反电势周期的哪个位置" —— 这是判断闭环相位对不对的
     *   唯一直接证据（之前所有间接指标都被证明可以自洽、说明不了问题）：
     *     · 相位对：换相应落在 BEMF 平台的起点（过零后 30°）⇒ 此刻 delta 已在
     *       "刚刚离开零点、朝平台方向走"的位置（约满幅值的 1/2~1 之间）；
     *     · 相位错（如认在过零点上就换相）：此刻 delta ≈ 0 ⇒ 换相整整早了 30°电角度
     *       ⇒ 转矩不对、电流灌进错误角度 ⇒ 抖 + 电流大 + 转速上不去（实测 1.x A）。
     *   同时记下 CNT，用来和过零时刻对照（换相应在 waitTime 处）。 */
    OL_CM_D[ol_cm_idx] = adc_bemf_get_delta();
    OL_CM_T[ol_cm_idx] = (uint16_t)INTERVAL_TIMER_COUNT;
    OL_CM_S[ol_cm_idx] = (uint8_t)step;
    OL_CM_W[ol_cm_idx] = waitTime;
    ol_cm_idx = (uint8_t)((ol_cm_idx + 1) & (OL_CM_LEN - 1));
    if (ol_cm_n < 60000) {
        ol_cm_n++;
    }
    
#ifdef USE_PULSE_OUT
	if(step == 1 || step == 4  ){
    WRITE_REG(RPM_PULSE_PORT->ODR, READ_REG(RPM_PULSE_PORT->ODR) ^ RPM_PULSE_PIN);
	}
#endif
}

/*
 * @brief 	Called by the COM_TIMER interrupt handler after the set wait time
 * 			This computes how much to advance in a commutation step.
 * 			This disables the COM_TIMER interrupt.
 * 			Then it enables the comparator to generate its interrupt.
 */
void PeriodElapsedCallback()
{
    DISABLE_COM_TIMER_INT(); // disable interrupt
    commutate();
    /* ★★ 2026-10-09 抗"正反馈塌陷"的最后一道闸 ★★
     *   转子物理上不可能在一步（60°电角度）之内把转速提高 12% 以上 ⇒ 若滤波结果
     *   比这一步骤降超过 1/8，说明本步的 thiszctime 被假边沿带偏了，只允许按 1/8 下降。
     *   为什么必须有：一旦进入"假过零 → ci 变小 → waitTime 变小 → 假边沿来得更早"
     *   这条正反馈，闭环会把 ci 从 1500 一路拉到 62（实测 commutation_intervals =
     *   [70,62,62,66,77,77]，每步 0.75×），换相比 PWM 周期还快、六步波形全烂，
     *   而且它【不会】自恢复（实测一直持续到断电）。这是台架实测的现场表现
     *   ——"输出成了恒定高的 PWM、电机不转"。
     *   （正常跟踪时每步的变化只有千分之几，这个闸完全不影响正常调速。） */
    {
        /* ★★★ 2026-10-10 换相周期改用【上一整转(6 步)】的平均 —— 对齐 TI SPRABZ3 ★★★
         *   TI 原文："let T be the time that the rotor spent to complete the previous
         *   revolution... cmtn_delay = v_timer / 12"（v_timer = 6 个换相步的计数）
         *   ⇒ 他们的 30° 延迟基于 6 步平均；而我们原来只有 2 步滤波:
         *        ci = (ci + (last+this)/2) / 2
         *     再叠加"每次换相最多只许变 ±1/64"的限速 ⇒ 周期估计又慢又抖。
         *     实测：4.5 倍的误差要约 95 次换相（≈1 秒）才收敛 ——
         *     正是"交棒后要晃一秒才稳"的来源。
         *   ⇒ 用 commutation_intervals[] 的六步和 ÷6。环里存的是【实测步长】（见上面），
         *     所以这次才是真正的整转平均。
         *   注：环还没填满时（开机/刚解锁）退回原式，避免用零值把 ci 打到 0。 */
        uint32_t ci_sum = (uint32_t)commutation_intervals[0] + (uint32_t)commutation_intervals[1]
                        + (uint32_t)commutation_intervals[2] + (uint32_t)commutation_intervals[3]
                        + (uint32_t)commutation_intervals[4] + (uint32_t)commutation_intervals[5];
        uint32_t ci_new;
        if (ci_sum >= 6u) {
            ci_new = ci_sum / 6u;              /* 整转(6 步)平均 —— 对齐 TI 的做法 */
        } else {
            ci_new = ((commutation_interval) + ((lastzctime + thiszctime) >> 1)) >> 1;  /* 环未填满 ⇒ 退回原式 */
        }
        /* ★★★ 2026-10-09 关键修复⑦：闸门改成【双向】，不许换相频率"一步投降" ★★★
         *   台架实测（duty 351、电流 0.46~0.55A 已与开环同量级、提前量已为 0）：
         *     交棒后【一步】ci 就从 1500 跳到 ~7000（≈475RPM）。
         *   原因：交棒后的头 11.7ms 没认到过零，第一次测到的间隔就巨大，
         *   而滤波 ci=(ci+间隔)/2 一次就被拉大 4.7 倍 ⇒ 换相频率直接塌到 1/4.7
         *   ⇒ 转子被拖到低频 ⇒ 再也起不来。现场就是"占空比加大了但依旧跟不上步频"。
         *   而上一版的闸【只限制变小、不限制变大】（当时是为了"丢锁能自恢复"），
         *   恰恰挡不住这一下。
         *   ⇒ 改成双向，一步最多变 1/16（6.25%）。这样：
         *     · 单次坏测量最多让频率偏 6%，不会一步投降；
         *     · 频率会【咬住交棒转速】把转子往上拖，给电机时间跟上来
         *       （正是"先让转速上来且跟上换向，再交给闭环调"这条思路）；
         *     · 真要适应（转子确实只能跑更慢/更快）时，每步 6.25% 在 2222 步/秒下
         *       约 139 倍/秒的调节速度，几十毫秒内到位，不影响正常调速；
         *     · 丢锁后也能自己往上爬回来（放大方向不再被禁）。 */
        /* ★★ 2026-10-09：档位从 1/16 再放缓到 1/64（1.5%/步）★★
         *   1/16 实测（duty 351）不行：频率被硬摁在 2222RPM 而转子跟不上
         *   ⇒ 电流冲到 1.x A、剧烈抖动 ⇒ 电全灌进错误的角度变成热和抖。
         *   改成 1/64 后，闭环的【驱动形态】就接近已验证可用的开环
         *   （固定时基、六步规整），只是频率能以约 33 倍/秒 的速度慢慢跟着转子走 ——
         *   既不一步投降，也不硬摁着转子灌电流。这正是"跟不上就慢慢降、跟得上就慢慢增"
         *   那条思路的最小实现。 */
        uint32_t ci_lo = commutation_interval - (commutation_interval >> 6);
        uint32_t ci_hi = commutation_interval + (commutation_interval >> 6);
        if (ci_new < ci_lo) {
            ci_new = ci_lo;
        }
        if (ci_new > ci_hi) {
            ci_new = ci_hi;
        }
        /* ★★★ 2026-10-09 关键修复⑧：给换相周期加"地板"，永远不许比交棒转速更慢 ★★★
         *   实测（4 秒短测）：换相 33 次/秒（≈55RPM，几乎全是 22.5ms 兜底在踹）、
         *   过零只认到 4.5 次/秒。而 55RPM 时反电势只有 ~4 码（滞回阈值是 16 码）
         *   ⇒ 跨不过阈值 ⇒ 认不到过零 ⇒ 只剩兜底踹 ⇒ 转得更慢 ⇒ 反电势更小……
         *   【这是一个锁死的恶性循环】：一旦慢下去就永远回不来。
         *   而 2222RPM（= 交棒转速）时反电势有 ~400 码（实测），完全够认。
         *   ⇒ 把 ci 钉一个上限 = 交棒周期（OL_STEP_DIV×100）：
         *     转速永远不会掉到交棒转速以下 ⇒ 反电势一直够大 ⇒ 过零一直认得到
         *     ⇒ 闭环自持。这正是"咬住转速、别一步投降"的硬版本。
         *   ★★★ 2026-10-10 把它与"交棒转速"拆开 ★★★
         *   用户原话："你开环速度没拉上去" —— 因为原来地板和【开环斜坡终点/交棒转速】
         *   共用 OL_STEP_DIV：想让地板低就得让开环也慢，想让开环快地板就跟着高，怎么调都是错的。
         *   而这两件事本来就该分开：
         *     · 开环斜坡终点 = 交棒转速：要【高】（反电势强 ⇒ 交棒验证可靠）
         *     · 闭环 ci 地板：要【低】（低于转子在该占空比下能到的转速 ⇒ 不超速命令、不滑步）
         *   ⇒ 地板改用独立宏 OL_CI_FLOOR_DIV（见 targets.h）。
         *   地板只用防"越慢越认不到"的死循环，所以只要低到让闭环自由跟随即可。 */
        {
            uint32_t ci_cap = (uint32_t)OL_CI_FLOOR_DIV * 100u;
            if (ci_new > ci_cap) {
                ci_new = ci_cap;
            }
        }
        commutation_interval = ci_new;
    }
  	if (!eepromBuffer.auto_advance) {
	  advance = (commutation_interval * temp_advance) >> 6; // 60 divde 64 0.9375 degree increments
	} else {
	  advance = (commutation_interval * auto_advance_level) >> 6; // 60 divde 64 0.9375 degree increments
    }
    /* ★ 2026-10-12 每步 waitTime：改为查表（比例 × ci），不再一律 ci/2。
     *   （advance 已并入表里，这里不再减；auto_advance 逻辑保留但不参与。）
     *
     * ★★★ 2026-10-12 索引错位修正：bemf_zc_step - 1 → step - 1 ★★★
     *   原来用 bemf_zc_step-1。但本段代码跑在 commutate()【之后】，此刻
     *   bemf_zc_step 记的是【刚才那次】过零所在的步（= 上一个步）；
     *   而这里算出的 waitTime 是被【接下来那次】过零消费的（interruptRoutine()
     *   用 SET_AND_ENABLE_COM_INT(waitTime+1) 武装 COM 定时器）——
     *   接下来那次过零发生在【刚进入的 step】里。⇒ 索引用 step-1 才对，
     *   用 bemf_zc_step-1 会把整张表错位一整步。
     *
     *   实测证据（OL_CM_W 埋点，RM2027_CL_ANCHOR_DUTY=300 那一跑，最近 32 次换相）：
     *     按 OL_CM_S 归并 waitTime 得 114/1035/766/878/660/872 拍；
     *     用表 A 比例去除，六个值反推出的 ci 分别是 2426/2447/2439/2445/2454/2429
     *     ⇒ 六个独立值收敛到同一个 ci ≈ 2440 拍 = 1.22ms。
     *   而这个收敛【只在把 label 与表项错开一位时才成立】：
     *     label 1→wt_num[4]  2→wt_num[5]  3→wt_num[0]  4→wt_num[1]  5→wt_num[2]  6→wt_num[3]
     *   即实际生效的是 wt_num[s-3]（label s 的换相所消费的那次过零在步 s-2 里…
     *   再往上追一层就是本行），表本意是 wt_num[s-1]。错位一整步。
     *   后果：step5 本该用 0.047 却拿到 0.359（晚 0.31×ci ≈ 19° 电角度）；
     *        这与实测"步 5 最长(1.54ms)、步 6 最短(0.50ms)"的宽度畸变方向一致。 */
    {
        uint8_t _s = (step >= 1 && step <= 6) ? (uint8_t)(step - 1u) : 0u;
        waitTime = (uint16_t)(((uint32_t)commutation_interval * (uint32_t)wt_num[_s]) / 1000u);
    }
#if RM2027_ZCL_FILTER
    /* ★ 2026-10-14 丢过零滤波：正常换相后刷新阈值基准（在 waitTime 刚算好之后）。
     *   下次过零丢失时，超时阈值 = ci + zcl_pred + 余量
     *   ⇒ 触发点落在"虚拟过零 + 本步预计延迟" = 本该换相的时刻（保持原节拍）。 */
    zcl_pred = waitTime;
#endif
    if (!old_routine) {
        enableCompInterrupts(); // enable comp interrupt
    }
    if (zero_crosses < 10000) {
        zero_crosses++;
    }
}

/*
 * @brief 	Called by the comparator interrupt handler.
 * 			Disables the comparator interrupt.
 * 			Enables the COM_TIMER and sets it to generate an interrupt after the wait time.
 */
void interruptRoutine()
{
#if RM2027_OL_ARM_ONLY
    return; /* 开环调试：过零中断路径不参与换相（换相由 olStepTick 独占） */
#endif
//   if (average_interval > 125) {
//        if ((INTERVAL_TIMER_COUNT < 125) && (duty_cycle < 600) && (zero_crosses < 500)) { // should be impossible, desync?exit anyway
//           return;
//        }
//        stuckcounter++; // stuck at 100 interrupts before the main loop happens
//                        // again.
//        if (stuckcounter > 100) {
//            maskPhaseInterrupts();
//            zero_crosses = 0;
//            return;
//        }
//    }
        /* 滤波：本板是"ADC 版过零"，采样值每个 PWM 周期（41.7µs）才刷新一次 ⇒
         * 上游那个"连续读 filter_level 次"的循环在这里只会读到同一个值、没有滤波作用
         * 还白添延迟 ⇒ 改成只读一次。抗噪由三层负责：
         *   ① adc_bemf.c 的施密特滞回（BEMF_HYST_LSB，默认 8 码 ≈ 相侧 100mV）
         *   ② 调用点的门槛"距上次换相已过半个周期"（见 stm32g0xx_it.c 的 DMA 中断）
         *   ③ "每步只认一次"的武装标志（bemf_irq_armed）
         * （filter_level 这个变量在【轮询模式/起步】那条路径里仍在用，别删。）*/
#if defined(USE_ADC_BEMF)
        /* ★ 2026-10-09：这道"按 rising 判电平"的门对 ADC 版过零也去掉了。
         *   理由与 ADC 中断里改成极性无关是同一个：边沿检测已经保证"这一步确实
         *   发生过一次跨越"，再按同一个 rising 约定判一次电平只会引入【第二处
         *   极性耦合】—— 极性一错，前面认对了、这里又被挡回去 ⇒ 依然不换相。
         *   本板 A/B 相没有比较器通路可与上游约定对照（这正是当初要写 adc_bemf.c
         *   的原因），所以只保留"边沿 + 每步只认一次 + 时间窗"这三条与极性无关的判据。 */
#elif defined(MCU_F031) || defined(MCU_G031)
        if ((current_GPIO_PORT->IDR & current_GPIO_PIN) == !(rising)) {
            return;
        }
#else
        if (getCompOutputLevel() == rising) {
            return;
        }
#endif
    /* ★★★ 2026-10-09 埋点：记录"这一次过零被认在哪一步的哪个位置" ★★★
     *   为什么需要它：台架表现是"电机转、但慢且抖、没有梯形腰"，而
     *   bemf_timeout_happened 以 ~26 次/秒 上涨 ⇒ 换相主要靠 22.5ms 兜底硬踹
     *   ⇒ 约 43 机械 RPM。要判"是不是过零认在了错的位置"，只能实测：
     *     正确位置应 ≈ 一个完整换相步（= 此刻的 commutation_interval）；
     *     若远小于它 ⇒ 认在了换相点上（假过零）；
     *     若远大于它 ⇒ 认晚了 / 中间丢了几步。
     *   环形缓冲 + 累计计数，用 SWD 直接把这几段 RAM 读出来即可，不改行为。 */
    OL_TRACE_CNT[ol_trace_idx] = bemf_zc_time;                     /* 过零【发生】时距上次过零的拍数 */
    OL_TRACE_D[ol_trace_idx]   = adc_bemf_get_delta();             /* 当时的"悬空相 − V0" */
    OL_TRACE_CI[ol_trace_idx]  = (uint16_t)commutation_interval;   /* 当时认为的一步有多长 */
    OL_TRACE_ST[ol_trace_idx]  = (uint8_t)(step);                  /* 当时在第几拍换相 */
    OL_TRACE_DMAX[ol_trace_idx] = bemf_dmax;                       /* 本步 |delta| 峰值 */
    bemf_dmax = 0;                                                 /* 重新计本步 */
    ol_trace_idx = (uint8_t)((ol_trace_idx + 1) & (OL_TRACE_LEN - 1));
    if (ol_trace_n < 60000) {
        ol_trace_n++;
    }

#if RM2027_ZCL_FILTER
    /* ★ 2026-10-14 丢过零滤波：真过零被接受 ⇒ 连续丢失计数清零
     *   （退出降频/惯性模式，回到正常闭环；ci 由六步环 + ±1/64 限速自然收敛）。 */
    zcl_miss = 0;
#endif
    __disable_irq();
    maskPhaseInterrupts();
    lastzctime = thiszctime;
    /* ★★ 2026-10-09：用"过零【发生】的时刻"，不是"此刻"也不该用接受时刻 ★★
     *   bemf_zc_time 由 ADC 中断在"本帧检测到翻转"时写入，见 stm32g0xx_it.c 的
     *   "关键修复③"。用错它会系统地短量 thiszctime ⇒ CNT/CI 恒 0.6、ci 被带小
     *   ⇒ runaway ⇒ 永久丢锁（22.5ms 兜底硬踹 ⇒ ~43 RPM 慢抖）。 */
    thiszctime = bemf_zc_time;
    SET_INTERVAL_TIMER_COUNT(0);
    SET_AND_ENABLE_COM_INT(waitTime+1); // enable COM timer interrupt
    __enable_irq();
}

void startMotor()
{
#if !RM2027_OL_ARM_ONLY
    /* ★★ 2026-10-08 改：起步不再直接进闭环，改走"正统三段式"开环起步 ★★
     *   原先这里把 old_routine 清 0 直接进闭环 —— 但本板过零靠 ADC 软件施密特，
     *   有 BEMF_HYST_LSB 的硬门槛，静止/极低速时认不到过零 ⇒ 直接进闭环会自锁
     *   （实测：zc 恒 0、靠 22.5ms 超时硬踹、step 每 22.5ms 才一步 ≈7 RPM）。
     *   现在：起步先开环（对齐 → 斜坡，固定时基硬踹），等电机被踹到斜坡末端、
     *   过零能稳定认到之后再交棒给闭环。判据与交棒动作见 olStartupTick()。
     *   注意此间 old_routine 置 1 是【故意】的语义："不要走中断闭环换相"；
     *   但主循环那条 22.5ms 超时兜底必须让开（见其调用处加的条件），
     *   否则两套时基会打架。 */
    old_routine = 1;
#endif
    if (running == 0) {
        commutate();
        commutation_interval = 10000;
        SET_INTERVAL_TIMER_COUNT(5000);
        running = 1;
#if !RM2027_OL_ARM_ONLY
        /* 进入/重置开环起步链：对齐阶段先钉住门极并计时 */
        ol_startup_arm   = 1;
        ol_startup_stage = OL_STARTUP_ALIGN;
        ol_startup_zc    = 0;
        ol_startup_ticks = 0;
        ol_startup_trip  = 0;
        /* ★ 细粒度目标：ol_ci_tgt 是目标 ci 拍；起点仍 5000；
         *   f_q = 512000000 / ci（2MHz × 256）⇒ 目标频率可以精确到 1 拍。 */
        if (ol_ci_tgt < 100UL) { ol_ci_tgt = 100UL; }   /* 防 0/负目标 */
        ol_step_div_cur  = (OL_STEP_DIV_START > (uint16_t)(ol_ci_tgt / 100UL))
                           ? OL_STEP_DIV_START : (uint16_t)(ol_ci_tgt / 100UL);
        ol_ci_cur = (uint32_t)ol_step_div_cur * 100UL;
        ol_tgt_armed = 0;
        ol_freq_q     = 512000000UL / ol_ci_cur;
        ol_freq_tgt_q = 512000000UL / ol_ci_tgt;
        ol_input_cur     = OL_INPUT_START;
        commutation_interval = (uint32_t)OL_ALIGN_TICKS * 100;   /* 0.5µs 单位 ⇒ 对齐时长 */
        SET_INTERVAL_TIMER_COUNT(0);
        /* 先不武装过零：对齐期间门极不动、电压还没到悬空态，此时认的"边沿"都是假的。
         * 进斜坡后再武装（见 ol_startup_stage 切换处）。 */
        maskPhaseInterrupts();
#endif
    }
#if RM2027_OL_ARM_ONLY
    enableCompInterrupts();
#endif
}

void setInput()
{
    if (eepromBuffer.bi_direction) {
        if (dshot == 0) {
            if (eepromBuffer.rc_car_reverse) {
                if (newinput > (1000 + (servo_dead_band << 1))) {
                    if (forward == eepromBuffer.dir_reversed) {
                        adjusted_input = 0;
                        //               if (running) {
                        prop_brake_active = 1;
                        if (return_to_center) {
                            forward = 1 - eepromBuffer.dir_reversed;
                            prop_brake_active = 0;
                            return_to_center = 0;
                        }
                    }
                    if (prop_brake_active == 0) {
                        return_to_center = 0;
                        adjusted_input = map(newinput, 1000 + (servo_dead_band << 1), 2000, 47, 2047);
                    }
                }
                if (newinput < (1000 - (servo_dead_band << 1))) {
                    if (forward == (1 - eepromBuffer.dir_reversed)) {
                        adjusted_input = 0;
                        prop_brake_active = 1;
                        if (return_to_center) {
                            forward = eepromBuffer.dir_reversed;
                            prop_brake_active = 0;
                            return_to_center = 0;
                        }
                    }
                    if (prop_brake_active == 0) {
                        return_to_center = 0;
                        adjusted_input = map(newinput, 0, 1000 - (servo_dead_band << 1), 2047, 47);
                    }
                }
                if (newinput >= (1000 - (servo_dead_band << 1)) && newinput <= (1000 + (servo_dead_band << 1))) {
                    adjusted_input = 0;
                    if (prop_brake_active) {
                        prop_brake_active = 0;
                        return_to_center = 1;
                    }
                }
            } else {
                if (newinput > (1000 + (servo_dead_band << 1))) {
                    if (forward == eepromBuffer.dir_reversed) {
                        if (((commutation_interval > reverse_speed_threshold) && (duty_cycle < 200)) || stepper_sine) {
                            forward = 1 - eepromBuffer.dir_reversed;
                            zero_crosses = 0;
                            old_routine = 1;
                            maskPhaseInterrupts();
                            brushed_direction_set = 0;
                        } else {
                            newinput = 1000;
                        }
                    }
                    adjusted_input = map(newinput, 1000 + (servo_dead_band << 1), 2000, 47, 2047);
                }
                if (newinput < (1000 - (servo_dead_band << 1))) {
                    if (forward == (1 - eepromBuffer.dir_reversed)) {
                        if (((commutation_interval > reverse_speed_threshold) && (duty_cycle < 200)) || stepper_sine) {
                            zero_crosses = 0;
                            old_routine = 1;
                            forward = eepromBuffer.dir_reversed;
                            maskPhaseInterrupts();
                            brushed_direction_set = 0;
                        } else {
                            newinput = 1000;
                        }
                    }
                    adjusted_input = map(newinput, 0, 1000 - (servo_dead_band << 1), 2047, 47);
                }

                if (newinput >= (1000 - (servo_dead_band << 1)) && newinput <= (1000 + (servo_dead_band << 1))) {
                    adjusted_input = 0;
                    brushed_direction_set = 0;
                }
            }
        }
        if (dshot) {
                     if (eepromBuffer.rc_car_reverse) {
                         if (newinput > 1047) {
                         if (forward == eepromBuffer.dir_reversed) {
                         adjusted_input = 0;
                         prop_brake_active = 1;
                         if (return_to_center) {
                             forward = 1 - eepromBuffer.dir_reversed;
                             prop_brake_active = 0;
                             return_to_center = 0;
                         }
                     }
                     if (prop_brake_active == 0) {
                         return_to_center = 0;
                         adjusted_input = ((newinput - 1048) * 2 + 47) - reversing_dead_band;
                     }
                     }
                     if (newinput <= 1047 && newinput > 47) {
                     if (forward == (1 - eepromBuffer.dir_reversed)) {
                         adjusted_input = 0;
                         prop_brake_active = 1;
                         if (return_to_center) {
                             forward = eepromBuffer.dir_reversed;
                             prop_brake_active = 0;
                             return_to_center = 0;
                         }
                     }
                     if (prop_brake_active == 0) {
                         return_to_center = 0;
                         adjusted_input = ((newinput - 48) * 2 + 47) - reversing_dead_band;
                     }
                     }
                     if (newinput < 48) {
                     adjusted_input = 0;
                     if (prop_brake_active) {
                         prop_brake_active = 0;
                         return_to_center = 1;
                     }
                 }
                         } else {
            if (newinput > 1047) {

                if (forward == eepromBuffer.dir_reversed) {
                    if (((commutation_interval > reverse_speed_threshold) && (duty_cycle < 200)) || stepper_sine) {
                        forward = 1 - eepromBuffer.dir_reversed;
                        zero_crosses = 0;
                        old_routine = 1;
                        maskPhaseInterrupts();
                        brushed_direction_set = 0;
                     } else {
                        newinput = 0;
                    }
                }
                adjusted_input = ((newinput - 1048) * 2 + 47) - reversing_dead_band;
            }
            if (newinput <= 1047 && newinput > 47) {
                if (forward == (1 - eepromBuffer.dir_reversed)) {
                    if (((commutation_interval > reverse_speed_threshold) && (duty_cycle < 200)) || stepper_sine) {
                        zero_crosses = 0;
                        old_routine = 1;
                        forward = eepromBuffer.dir_reversed;
                        maskPhaseInterrupts();
                        brushed_direction_set = 0;
                     } else {
                        newinput = 0;
                    }
                }
                adjusted_input = ((newinput - 48) * 2 + 47) - reversing_dead_band;
            }
            if (newinput < 48) {
                adjusted_input = 0;
                brushed_direction_set = 0;
                }
            }
        }
    } else {
        adjusted_input = newinput;
    }
#if RM2027_OL_ARM_ONLY
    /* ★ 油门只当启停开关：数值不参与调速。
     *   未解锁时必须保持 0 —— 解锁判据就是 adjusted_input == 0（见 tenKhzRoutine）。
     *   用 ol_input_cur 而不是 OL_INPUT_RUN：起步序列（对齐/斜坡）阶段要临时给更大的油门。 */
    adjusted_input = (armed && newinput >= 48) ? ol_input_cur : 0;
#else
    /* ★ 闭环的"开环起步"阶段同理：这两段（对齐/斜坡）的油门要用 ol_input_cur
     *   （OL_INPUT_START），否则起步扭矩按用户油门算，解锁后刚给一点油就可能推不动。
     *   交棒后 ol_startup_arm 清 0，这里就恢复成"按用户油门"。 */
    if (OL_STARTUP_ACTIVE()) {
        adjusted_input = ol_input_cur;
    }
#endif
#ifndef BRUSHED_MODE
    if ((bemf_timeout_happened > bemf_timeout) && eepromBuffer.stuck_rotor_protection) {
        allOff();
        maskPhaseInterrupts();
        input = 0;
        bemf_timeout_happened = 102;
#ifdef USE_RGB_LED
        setIndividualRGBLed(1, 0, 0);
#endif
    } else {
#ifdef FIXED_DUTY_MODE
        input = FIXED_DUTY_MODE_POWER * 20 + 47;
#else
        if (eepromBuffer.use_sine_start) {
            if (adjusted_input < 30) { // dead band ?
                input = 0;
            }
            if (adjusted_input > 30 && adjusted_input < (eepromBuffer.sine_mode_changeover_thottle_level * 20)) {
                input = map(adjusted_input, 30,
                    (eepromBuffer.sine_mode_changeover_thottle_level * 20), 47, 160);
            }
            if (adjusted_input >= (eepromBuffer.sine_mode_changeover_thottle_level * 20)) {
                input = map(adjusted_input, (eepromBuffer.sine_mode_changeover_thottle_level * 20),
                    2047, 160, 2047);
            }
        } else {
            /* ★★★ 2026-10-11 关键修复：速度环接管之前，input_override 必须【跟随油门】★★★
             *   打开速度环后 input 来自 input_override（速度环的累加变量），而它原来
             *   只在交棒处被初始化 ⇒ 形成死锁：
             *     电机不转 ⇒ 到不了交棒 ⇒ input_override 停在 0
             *     ⇒ input=0（被夹到 48）⇒ 占空比≈1 ⇒ 电机更不转。
             *   实测铁证：duty=1、steps=0、卡在 div=5000 的对齐段、cur≈0。
             *   ⇒ 只要速度环还没接管（开环起步段 or 没在跑），就让 input_override
             *     等于当前油门（单位 x10000）⇒ 开环照常能转，等到交棒后 PID 再接管。 */
            if (ol_startup_arm || !running) {
                input_override = (int32_t)adjusted_input * 10000;
            }
            if (use_speed_control_loop) {
                if (drive_by_rpm) {
                    target_e_com_time = 60000000 / map(adjusted_input, 47, 2047, MINIMUM_RPM_SPEED_CONTROL, MAXIMUM_RPM_SPEED_CONTROL) / (eepromBuffer.motor_poles / 2);
                    if (adjusted_input < 47) { // dead band ?
                        input = 0;
                        speedPid.error = 0;
                        input_override = 0;
                    } else {
                        input = (uint16_t)(input_override / 10000); // speed control pid override
                        if (input > 2047) {
                            input = 2047;
                        }
                        if (input < 48) {
                            input = 48;
                        }
                    }
                } else {

                    input = (uint16_t)(input_override / 10000); // speed control pid override
                    if (input > 2047) {
                        input = 2047;
                    }
                    if (input < 48) {
                        input = 48;
                    }
                }
            } else {

                input = adjusted_input;
            }
        }
#endif
    }
#endif
#ifndef BRUSHED_MODE
if (!stepper_sine && armed) {
        if (input >= 47 + (80 * eepromBuffer.use_sine_start)) {
            /* ★ 2026-10-12：加 !cl_done_latch —— 闭环自动停机后必须【先收油】才能再跑。
             *   原来只判断 running==0，而油门还挂着 ⇒ 停了立刻又启动 ⇒
             *   实测 cl_runs_stopped 一路涨（"跑完 6 秒 → 停 → 又跑 6 秒"无限循环）。*/
            if (running == 0 && !cl_done_latch) {
                allOff();
#if RM2027_OL_ARM_ONLY
                if (!old_routine) {   /* 开环：换相由 olStepTick() 独占，不必走 startMotor() */
                    startMotor();
                }
#else
                /* ★ 闭环：从"停止"给油【无条件】走 startMotor()（2026-10-07 定位的
                 *   "解锁后推不动转子"根因）。
                 *   上游这里门控 `if (!old_routine)`，语义是"old_routine==1 时由老轮询换相
                 *   自己起步"。但本板没有比较器、老轮询起步不可靠；更要命的是解锁【必须】
                 *   先有一段零油门，而"已解锁 + 零油门 + 停止"会把 old_routine 钉成 1
                 *   （见下面零油门分支）⇒ 这道门控会让"解锁后的头一次给油"什么都不做：
                 *   现象是 armed=1、running=1、duty 有值，但电机不换相、推不动。
                 *   startMotor() 内部会先清 old_routine，故放在这里做首次 commutate()。 */
                startMotor();
#endif
                running = 1;
                last_duty_cycle = min_startup_duty;
            }

            if (eepromBuffer.use_sine_start) {
                duty_cycle_setpoint = map(input, 137, 2047, minimum_duty_cycle+40, 2000);
            } else {
                duty_cycle_setpoint = map(input, 47, 2047, minimum_duty_cycle, 2000);
            }

            if (!eepromBuffer.rc_car_reverse) {
                prop_brake_active = 0;
            }
        }

        if (input < 47 + (80 * eepromBuffer.use_sine_start)) {
            if (play_tone_flag != 0) {
                switch (play_tone_flag) {
									
                case 1:
                    playDefaultTone();
                    break;
                case 2:
                    playChangedTone();
                    break;
                case 3:
                    playBeaconTune3();
                    break;
                case 4:
                    playInputTune2();
                    break;
                case 5:
                    playDefaultTone();
                    break;
                }
                play_tone_flag = 0;
            }

            if (!eepromBuffer.comp_pwm) {
                duty_cycle_setpoint = 0;
                if (!running) {
                    old_routine = 1;
                    zero_crosses = 0;
                    if (eepromBuffer.brake_on_stop) {
                        fullBrake();
                    } else {
                        if (!prop_brake_active) {
                            allOff();
                        }
                    }
                }
                if (eepromBuffer.rc_car_reverse && prop_brake_active) {
#ifndef PWM_ENABLE_BRIDGE

                  if (dshot == 0) prop_brake_duty_cycle = (getAbsDif(1000, newinput) + 1000);
                    if (dshot)  {
                        if (newinput <= 1047 && newinput > 47) prop_brake_duty_cycle = ((newinput - 48) * 2 + 47) - reversing_dead_band;
                        if (newinput > 1047) prop_brake_duty_cycle = ((newinput - 1048) * 2 + 47) - reversing_dead_band;
                    }
                    if (prop_brake_duty_cycle >= (1999)) {

                        fullBrake();
                    } else {
                        proportionalBrake();
                    }
#endif
                }
            } else {
                if (!running) {

                    old_routine = 1;
                    zero_crosses = 0;
                    bad_count = 0;
                    if (eepromBuffer.brake_on_stop > 0) {
                        if (!eepromBuffer.use_sine_start) {
#ifndef PWM_ENABLE_BRIDGE
                          if(eepromBuffer.brake_on_stop == 1){
                             prop_brake_duty_cycle =  eepromBuffer.drag_brake_strength * 200;
                              if (prop_brake_duty_cycle >= (1999)) {
                                fullBrake();
                              } else {
                                proportionalBrake();
                                prop_brake_active = 1;
                              }
                           }
#else
                            // todo add proportional braking for pwm/enable style bridge.
#endif
                        }
                    } else {
                        allOff();
                    }
                    duty_cycle_setpoint = 0;
                }

                phase_A_position = ((step - 1) * 60) + enter_sine_angle;
                if (phase_A_position > 359) {
                    phase_A_position -= 360;
                }
                phase_B_position = phase_A_position + 119;
                if (phase_B_position > 359) {
                    phase_B_position -= 360;
                }
                phase_C_position = phase_A_position + 239;
                if (phase_C_position > 359) {
                    phase_C_position -= 360;
                }

                if (eepromBuffer.use_sine_start == 1) {
                    stepper_sine = 1;
                }
                duty_cycle_setpoint = 0;
            }
        }
        if (!prop_brake_active) {
            if (!RM2027_OL_ARM_ONLY && input >= 47 && (zero_crosses < (uint32_t)(30 >> eepromBuffer.stall_protection))) {
                if (duty_cycle_setpoint < min_startup_duty) {
                    duty_cycle_setpoint = min_startup_duty;
                }
                if (duty_cycle_setpoint > startup_max_duty_cycle) {
                    duty_cycle_setpoint = startup_max_duty_cycle;
                }
            }

            if (duty_cycle_setpoint > duty_cycle_maximum) {
                duty_cycle_setpoint = duty_cycle_maximum;
            }
            if (use_current_limit) {
                if (duty_cycle_setpoint > use_current_limit_adjust) {
                    duty_cycle_setpoint = use_current_limit_adjust;
                }
            }

            if (!RM2027_OL_ARM_ONLY && stall_protection_adjust > 0 && input > 47) {

                duty_cycle_setpoint = duty_cycle_setpoint + (uint16_t)(stall_protection_adjust/10000);
            }

#if RM2027_CL_ANCHOR_DUTY > 0 && !RM2027_OL_ARM_ONLY
            /* ★★ 2026-10-12 闭环锚定占空比（开关见 targets.h 的 RM2027_CL_ANCHOR_DUTY）★★
             *   放在上面所有夹子【之后】⇒ 这一轮占空比由它说了算（速度环只能空转）。
             *   加 !OL_STARTUP_ACTIVE()：起步段（对齐/斜坡/交棒）仍走原 V/f 斜坡，
             *   否则斜坡被钉死 ⇒ 交棒条件（OL_VERIFY_PASS）永远不满足。 */
            if (running && !OL_STARTUP_ACTIVE()) {
                duty_cycle_setpoint = RM2027_CL_ANCHOR_DUTY;
            }
#endif
        }
    }
#endif
}

#if RM2027_OL_ARM_ONLY
/* ================== RM2027 强制开环（DShot 仅解锁） ==================
 * 由 tenKhzRoutine()（TIM6，20kHz）调用 —— 只是"轮询频率"，不是时间基准。
 *
 * ★★ 时间基准与 AM32 原生的开环一致（这是本函数与上游对齐的关键）★★
 *   AM32 原生也有一段开环：它的兜底路径（主循环里）就是
 *       if (INTERVAL_TIMER_COUNT > 45000) { ... zcfoundroutine(); }   // 强制换相
 *   用的正是【INTERVAL_TIMER（TIM2，0.5µs/拍）+ commutation_interval】。
 *   本函数沿用同一套：等 INTERVAL_TIMER_COUNT >= commutation_interval 就换相，只是把
 *   固定阈值 45000 换成【随斜坡变化的期望间隔】。换相动作、时间量、计数器全用上游机制。
 *
 * 配置常量（OL_*）在 targets.h；运行时状态与观测量在文件上方的全局区。
 * 起步序列（ol_state）：0 空闲 → 1 对齐 → 2 斜坡 → 3 恒速
 *   1 对齐：不换相，门极钉在 comStep(1)，等满 OL_ALIGN_TICKS 对应的时间 —— 转子被拉到
 *           已知位置并停住；顺带把 step 复位成 1，所以每次起步的第一组门极都一样。
 *   2 斜坡：从 OL_STEP_DIV_START 起，每换相一次把步长按 1/2^OL_RAMP_SHIFT 缩短，
 *           到 OL_STEP_DIV 转恒速。
 *   3 恒速：保持 OL_STEP_DIV。 */
static void olStepTick(void)
{
    if (armed && running && input >= 47) {
        /* ---- 过流跳闸（见上方参数区说明）：先把相电流反推出来，再比阈值 ----
         * 相电流 ≈ 实测母线平均电流 ÷ 占空比；duty_cycle 是 0..2000 的口径。 */
#if OL_TRIP_CURRENT > 0   /* 0 = 关闭过流跳闸 ⇒ 用预处理器判断（编译期常量） */
        if (duty_cycle > 0 && (ol_iphase_last =
                ((uint32_t)actual_current * 2000) / (uint32_t)duty_cycle) > OL_TRIP_CURRENT) {
            if (++ol_trip_count > OL_TRIP_TICKS) {
                allOff();
                running = 0;
                ol_state = 0;
                ol_input_cur = 0; /* ★ 关键：不清零的话主循环会立刻把 running 又置 1 重启 */
                ol_step_div_cur = OL_STEP_DIV;
                ol_trip_count = 0;
                ol_trips++;
                ol_stop_reason = 2; /* 过流跳闸：保持锁存（故障，不能自动重跑） */
                ol_led_off_req = 1; /* 停机 ⇒ 灭灯（主循环执行），避免"绿灯亮着但已停"的错觉 */
                return;
            }
        } else {
            ol_trip_count = 0;
        }
#endif

        /* ---- 运行时间上限：到点自动停机（防止"忘了关"或"转子卡住还一直通电"）---- */
#if OL_RUN_LIMIT > 0      /* 0 = 不限制 ⇒ 用预处理器判断 */
        if (++ol_run_ticks >= OL_RUN_LIMIT) {
            allOff();
            running = 0;
            ol_state = 0;
            ol_input_cur = 0; /* 与跳闸同样锁存：要再跑必须重新解锁 */
            ol_step_div_cur = OL_STEP_DIV;
            ol_run_ticks = 0;
            ol_timeouts++;
            ol_stop_reason = 1; /* 到时间上限：冷却后自动解除锁存（否则被永久锁死） */
            ol_led_off_req = 1; /* 停机 ⇒ 灭灯（主循环执行） */
            return;
        }
#endif

        /* ★★ 时间基准：与 AM32 原生开环用同一套东西 ★★
         *   AM32 原生也有一段开环 —— 它的兜底路径（主循环里）就是：
         *        if (INTERVAL_TIMER_COUNT > 45000) { ... zcfoundroutine(); }   // 强制换相
         *   用的正是【INTERVAL_TIMER（TIM2，0.5µs/拍）+ commutation_interval】。
         *   这里唯一的不同：把固定阈值 45000 换成【随斜坡变化的期望间隔】。
         *   ⇒ 换相动作（commutate()）、时间量（commutation_interval）、计数器
         *     （INTERVAL_TIMER_COUNT）全部沿用上游机制，不再自己数 20kHz 拍。 */
        if (ol_state == 0) {
            /* 解锁后的第一拍：复位步序、门极钉住做"对齐"、把期望间隔设成对齐时长 */
            step = 1;
            comStep(1);
            /* ★ 起步就请求点绿灯（主循环执行）。为什么"每次起步"都要点，而不是只在
             *   "解锁序列"里点一次：停机/跳闸都会把灯设成暗红（见下方 ol_led_off_req），
             *   而绿灯只在主循环的解锁序列里点过一次 ⇒ 第一轮跑完变红之后，后面每一轮
             *   重跑都会一直亮红灯（2026-10-07 用户实测："转电机的时候 led 还是红灯"）。 */
            ol_led_on_req = 1;
            ol_input_cur = OL_INPUT_START;
            ol_step_div_cur = (OL_STEP_DIV_START > OL_STEP_DIV) ? OL_STEP_DIV_START : OL_STEP_DIV;
            commutation_interval = (uint32_t)OL_ALIGN_TICKS * 100;   /* 0.5µs 单位 ⇒ 200ms */
            SET_INTERVAL_TIMER_COUNT(0);
            ol_state = (OL_ALIGN_TICKS > 0) ? 1 : ((ol_step_div_cur > OL_STEP_DIV) ? 2 : 3);
            if (ol_state == 3) {
                ol_input_cur = OL_INPUT_RUN;
            }
        } else if (INTERVAL_TIMER_COUNT >= commutation_interval) {
            /* 期望间隔用完 ⇒ 到点了 */
            SET_INTERVAL_TIMER_COUNT(0);
            if (ol_state == 1) {
                /* 对齐结束（这一段只等时间、不换相）⇒ 转入斜坡 */
                ol_state = (ol_step_div_cur > OL_STEP_DIV) ? 2 : 3;
                if (ol_state == 3) {
                    ol_input_cur = OL_INPUT_RUN;
                }
            } else {
                ol_step_current[step - 1] = (uint16_t)ol_iphase_last; /* 观测：本步相电流 */
                commutate();
                ol_steps++;
                if (ol_state == 2) {
                    /* 斜坡：每换一次相，把步长按 1/2^OL_RAMP_SHIFT 缩短 */
                    uint16_t dec = (uint16_t)(ol_step_div_cur >> OL_RAMP_SHIFT);
                    if (dec == 0) {
                        dec = 1;
                    }
                    if ((uint32_t)ol_step_div_cur > (uint32_t)OL_STEP_DIV + dec) {
                        ol_step_div_cur -= dec;
                    } else {
                        ol_step_div_cur = OL_STEP_DIV;
                        ol_input_cur = OL_INPUT_RUN; /* 斜坡结束 ⇒ 回到运行油门 */
                        ol_state = 3;
                    }
                }
            }
            /* 期望间隔换算到 0.5µs 单位：既是下一步的时间基准，也让 erpm/e_com_time 遥测算得对 */
            commutation_interval = (uint32_t)ol_step_div_cur * 100;
        }
    } else {
        ol_run_ticks = 0; /* 不在驱动 ⇒ 运行计时清零 */
        if (running && input < 47) { /* 油门回 0 ⇒ 停机，并复位起步序列 */
            ol_stop_site = 4;            /* 停机站点：油门回 0 */
            allOff();
            running = 0;
            ol_state = 0;
            ol_input_cur = OL_INPUT_RUN;
            ol_step_div_cur = OL_STEP_DIV;
            ol_led_off_req = 1; /* 停机 ⇒ 点"待命红"（与跳闸/超时两条停机路径一致） */
        }
        /* 锁存解除条件之一：① !armed（彻底上锁）② newinput < 48（原始油门回到 0）。
         * 两者都成立时说明是"人为上锁/收油门"，可以恢复。
         * 注意：用 test_esc 那块 F103 测试板时 F1 爬到目标油门后一直保持 120，
         * ② 永远不会成立 ⇒ 所以"到时间上限"那一类停机改由下面的冷却逻辑自动解除。 */
        if (!armed || newinput < 48) {
            ol_input_cur = OL_INPUT_RUN;
            ol_step_div_cur = OL_STEP_DIV;
            ol_trip_count = 0;
            ol_stop_reason = 0;
            ol_cool_ticks = 0;
            cl_done_latch = 0;   /* ★ 收油/上锁 ⇒ 解除"闭环跑完"的锁存，下次给油可再跑 */
            /* ★★ 2026-10-12 必须同时清这个，否则"启动就停、电机一步都不转" ★★
             *   cl_run_armed 之前只在自动停机里被清 ⇒ 一旦成功交棒过一次，
             *   收油停机后它仍是 1、cl_run_start 还是上一轮的旧帧号 ⇒
             *   下次刚把 running 置 1，到点检查一算 (adc_frame_total - cl_run_start)
             *   远超 6 秒 ⇒ 立刻 allOff() ⇒ 无限"启动即停"，电机完全转不起来
             *   （实测：newinput=400、armed=1，但 running 恒为 0）。 */
            cl_run_armed = 0;
        }
        /* ★ "到时间上限"停机的自动重跑：冷却 OL_COOL_LIMIT 拍后解除锁存。
         *   这样"跑 N 秒 → 停 M 秒 → 又跑 N 秒"，既不会一直转、也不会被永久锁死。
         *   过流跳闸（ol_stop_reason == 2）不在此列：必须人为干预才恢复。 */
#if OL_COOL_LIMIT > 0    /* 0 = 不自动重跑 ⇒ 用预处理器判断 */
        if (ol_stop_reason == 1) {
            if (++ol_cool_ticks >= OL_COOL_LIMIT) {
                ol_input_cur = OL_INPUT_RUN;
                ol_step_div_cur = OL_STEP_DIV;
                ol_stop_reason = 0;
                ol_cool_ticks = 0;
            }
        }
#endif
        if (ol_stop_reason == 0) {
            ol_cool_ticks = 0;
        }
    }
}
#endif

void tenKhzRoutine()
{
    ms10k_cnt++;   /* ★ 0.1ms 计数：给"纯开环先跑多久"计时 */
    ol_openloop_ms = (ms10k_cnt - ol_hold_ms) / 10u;
 // 20khz as of 2.00 to be renamed
    duty_cycle = duty_cycle_setpoint;
    tenkhzcounter++;
    ledcounter++;
    ramp_count++;
    one_khz_loop_counter++;

#if RM2027_BENCH_OC_LIMIT > 0
    /* ★★ 2026-10-09 台架过流切断（护电机）★★
     *   背景：闭环换相相位不对时会把电全灌进错误角度 —— 实测 12V 下电流冲到
     *   1.x A（= 18W 灌在一个 1404 上），几秒就把电机烤热，用户只能手动断电护电机。
     *   而现有的过流跳闸阈值是 OL_TRIP_CURRENT = 800（厘安 = 8.0A），那是给飞行用的，
     *   台架电源只有 1.5A，根本来不及；而且它只在【起步链】里生效，闭环里没有。
     *   ⇒ 这里加一条【闭环也生效】的通用切断：actual_current（厘安）超过阈值
     *     持续 RM2027_BENCH_OC_TICKS 拍（20kHz，50ms = 1000 拍）就 allOff 停机。
     *   正常工作时开环实测 0.57~0.60A、切断阈值 1.0A ⇒ 不会误切，
     *   只在"相位不对、电流异常"时保护电机。 */
    if (running && actual_current > RM2027_BENCH_OC_LIMIT) {
        if (++ol_bench_oc_count > RM2027_BENCH_OC_TICKS) {
            allOff();
            running = 0;
            ol_stop_site = 3;                /* 停机站点：台架过流 */
            ol_startup_arm = 0;
            ol_bench_oc_count = 0;
            ol_bench_oc_trips++;
        }
    } else {
        ol_bench_oc_count = 0;
    }
#endif
    /* ★ 2026-10-20 修：DShot 输入下 zero_input_count 原本只在 USE_ADC_INPUT 分支里累加
     *   ⇒ 解锁条件 zero_input_count>30 永远不成立 ⇒ 油门回零也解不了锁。 */
    if (dshot) {
        if (input == 0) { if (zero_input_count < 60000u) { zero_input_count++; } }
        else            { zero_input_count = 0; }
    }
    if (!armed) {
        if (cell_count == 0) {
            if (inputSet) {
                if (adjusted_input == 0) {
                    armed_timeout_count++;
                    if (armed_timeout_count > LOOP_FREQUENCY_HZ) { // one second
                        if (zero_input_count > 30) {
                            armed = 1;
#ifdef USE_LED_STRIP
                            //	send_LED_RGB(0,0,0);
                            delayMicros(1000);
                            send_LED_RGB(0, 255, 0);
#endif
#ifdef USE_RGB_LED
                            setIndividualRGBLed(0,1,0);
#endif
                            if ((cell_count == 0) && eepromBuffer.low_voltage_cut_off == 1) {
                                cell_count = battery_voltage / 370;
                                for (int i = 0; i < cell_count; i++) {
                                    playInputTune();
                                    delayMillis(100);
                                    RELOAD_WATCHDOG_COUNTER();
                                }
                            } else {
#ifdef MCU_AT415
															play_tone_flag = 4;
#else
															playInputTune();
#endif
                            }
                            if (!servoPwm && !dshot) {
                                eepromBuffer.rc_car_reverse = 0;
                            }
                        } else {
                            inputSet = 0;
                            armed_timeout_count = 0;
                        }
                    }
                } else {
                    armed_timeout_count = 0;
                }
            }
        }
    }

    if (eepromBuffer.telemetry_on_interval) {
        telem_ms_count++;
        if (telem_ms_count > ((telemetry_interval_ms - 1 + eepromBuffer.telemetry_on_interval) * 20)) {
            // telemetry_on_interval = 1 is a boolean, but it can also be 2 or more to indicate an identifier
            // by making the interval just slightly different with an unique identifier, we can guarantee that many ESCs can communicate on just one signal
            // there will be some collisions but not as many as if two ESCs always tried to talk at once.
            send_telemetry = 1;
            telem_ms_count = 0;
        }
    }

#ifndef BRUSHED_MODE

    if (!stepper_sine) {
#ifndef CUSTOM_RAMP
        /* ★ 闭环的"开环起步"期间要跳过这段轮询换相：它只在 old_routine=1 时跑，
         *   而起步链故意把 old_routine 置 1（表示"先别走中断闭环"），
         *   若不跳过，轮询也会来插一脚、和起步链的固定时基打架。 */
        if (!RM2027_OL_ARM_ONLY && !OL_STARTUP_ACTIVE() && old_routine && running) {
	//				send_LED_RGB(255, 0, 0);
            maskPhaseInterrupts();
            getBemfState();
            if (!zcfound) {
                if (rising) {
                    if (bemfcounter > min_bemf_counts_up) {
                        zcfound = 1;
                        zcfoundroutine();
                    }
                } else {
                    if (bemfcounter > min_bemf_counts_down) {
                        zcfound = 1;
                        zcfoundroutine();
                    }
                }
            }
        }
#endif
#if RM2027_OL_ARM_ONLY
        olStepTick(); /* ★ 开环调试：本函数独占换相，上面的 BEMF 轮询换相已关掉 */
#else
        olStartupTick(); /* ★ 闭环的开环起步链：对齐 → 斜坡 → 认到过零交棒 */
        { extern void evt_pump(void); evt_pump(); }   /* ★ 串口事件流：主循环发送 */
        { extern void win_pump(void); win_pump(); }    /* ★ 窗口式全速采样 */
        { extern void telem_text_pump(void); telem_text_pump(); }   /* ★ 文本遥测 20Hz */
        { extern void cal_sg_guard(void); cal_sg_guard(); }        /* ★ cal_sg 越界硬保险 */
        esc_cal_poll();   /* ★ K2a 静态 ADC 校准状态机 */
        /* ★★ K4：自驱动校准跑（不依赖 F1 给油）★★
         *   用户要求："单独接电池+烧录线，烧录运行一下就能校准好"。
         *   做法：不再等 DShot 解锁，主循环自己调 startMotor() 走【正统三段式开环】，
         *   并把 adc_order_auto 打开 ⇒ order 逐电周期轮转；窗口包 q[10] 已带
         *   (step | order<<6) 标记 ⇒ PC 侧能按 order 分组比同一相。
         *   保护（沿用台架既有红线，不要在这里放宽）：
         *     · OL_RUN_LIMIT = 160000 拍 = 8s ⇒ 到点自动收油；
         *     · OL_TRIP_CURRENT = 8A（起步链）+ RM2027_BENCH_OC_LIMIT ≈1.8A（十 kHz 段）；
         *     · 跑完/异常停机后必须把 adc_order_auto 还原为 0 —— 判据不能吃轮转序。 */
        {
            extern volatile uint8_t cal_run;
            extern volatile uint8_t cal_compute_req;
            extern void rk_compute(void);
            extern volatile uint8_t adc_order_auto;
            extern volatile uint8_t adc_order_pending;
            /* ★★ 2026-10-20 两轮标定（用户新流程）★
             *   第一轮：开环跑 → 算参数（cal_run 1→2→3）
             *   然后：闭环在油门 400 跑 2s 测 e_rpm → 回写 ol_ci_tgt（cal_run=4）
             *   第二轮：用对齐后的工作点再跑一遍开环 → 算"精确参数"（cal_run 1→2→3）
             *   cal_pass: 0=第一轮 1=第二轮 */
            static uint32_t cl_rs = 0u, cl_rn = 0u, pause_t0 = 0u, snap_t0 = 0u;
            static uint8_t cal_pass = 0u;
            if (cal_compute_req) { cal_compute_req = 0u; rk_compute(); }   /* ★ K15 按需计算 */
            /* ★★ K17：交棒进闭环 ⇒ 必须【停掉 order 轮转】★★
             *   轮转是 rank 校准用的（开环段采数据需要），但闭环判据【不能吃轮转序】：
             *   项目 n3e 结论原话"闭环判据不能这样交替（会注入交替偏移）"。
             *   实测现象：交棒后 order 仍在转 ⇒ 每圈采样槽位不同 ⇒ 过零时刻逐圈跳变
             *   ⇒ 电机明显抖动。ol_startup_arm 由 1 变 0 就是交棒时刻。 */
            {
                extern volatile uint8_t ol_startup_arm;
                /* ★ 必须先"见过开环"再谈"交棒"：启动瞬间 ol_startup_arm 还是 0，
                 *   若无条件清轮转，会把还没开始的 rank 采集直接掐死
                 *   （实测：rk_k 全 1000 退化、rank 一个样本都没采到）。*/
                static uint8_t seen_open_loop = 0;
                if (ol_startup_arm != 0u) { seen_open_loop = 1u; }
                if (seen_open_loop && cal_run != 0u && ol_startup_arm == 0u) {
                    adc_order_auto    = 0u;
                    adc_order_pending = 0u;
                }
                if (cal_run == 0u) { seen_open_loop = 0u; }   /* 本轮结束，复位 */
            }
#if RM2027_CAL_AUTO
            /* ★★ K16：烧录/上电后【自动跑一轮校准】，跑完自己停，等下次烧录 ★★ */
            {
                static uint8_t  auto_started = 0;
                static uint32_t auto_t0      = 0;
                extern volatile uint32_t adc_frame_total;
                if (!auto_started && cal_run == 0u) {          /* 只在开机后触发一次 */
                    auto_started = 1u;
                    auto_t0 = adc_frame_total;
#if RM2027_CAL_AUTO_APPLY
                    { extern volatile uint8_t cal_apply; cal_apply = 1u; }
#endif
                    cal_run = 1u;                              /* 启动自驱校准 */
                } else if (cal_run != 0u &&
                           (uint32_t)(adc_frame_total - auto_t0) > (uint32_t)(RM2027_CAL_AUTO_MS * 24u)) {
                    /* 硬超时：不管电机状态如何，强制收尾 + 计算 + 停机 */
                    extern void allOff(void);
                    cal_run = 0u;                /* 先清标志，allOff 才会真的关断 */
                    adc_order_auto = 0u;
                    adc_order_pending = 0u;
                    allOff();
                    running = 0;
                    rk_compute();
                    auto_started = 1u;           /* 已完成，不再重启 */
                }
            }
#endif
            /* ★★ K18：自动对齐工作点（用户要求）★★
             *   问题：开环按固定 ol_ci_tgt 硬踹，而电机在某油门下的自然转速是另一回事
             *        （实测 ci=1700 ⇒ 11765 eRPM，但 400 油门闭环只有 9300 eRPM ⇒ 差 26%）
             *        ⇒ 交棒瞬间相位/负载角突跳，瞬态被采进校准数据 ⇒ 只对某个油门合适。
             *   做法：cal_run=4 ⇒ 先正常起步、交棒、闭环跑 2 秒取 e_rpm 平均
             *        ⇒ ci_tgt = 20,000,000 / eRPM（1 拍 = 0.5µs，6 步一电周期）
             *        ⇒ 写回 ol_ci_tgt ⇒ 停机 ⇒ 自动转 cal_run=1 跑校准。
             */
            if (cal_run == 4u) {
                static uint32_t rt_t0 = 0;
                static uint32_t rt_enter = 0;
                static uint32_t rt_sum = 0;
                static uint16_t rt_n = 0;
                extern volatile uint32_t ol_ci_tgt;
                if (rt_enter == 0u) { rt_enter = adc_frame_total; }
                if ((uint32_t)(adc_frame_total - rt_enter) > 144000u) {
                    /* ★ 6s 硬兜底：交棒/闭环没成功也不能一直转 */
                    allOff(); running = 0;
                    rt_enter = 0u; rt_t0 = 0u; rt_sum = 0u; rt_n = 0u;
                    cal_run = 1u;                            /* 跳过对齐，直接第二轮校准 */
                } else {
                    if (!running) { startMotor(); }
                    if (running && ol_startup_arm == 0u) {          /* 已在闭环 */
                        if (rt_t0 == 0u) { rt_t0 = adc_frame_total; rt_sum = 0u; rt_n = 0u; }
                        if ((uint32_t)(adc_frame_total - rt_t0) > 48000u) {   /* 2 秒 */
                            uint32_t avg = rt_n ? (rt_sum / (uint32_t)rt_n) : 0u;
                            uint32_t ci;
                            if (avg < 50u)   { avg = 50u; }          /* 500 eRPM 下限 */
                            ci = 200000UL / avg;   /* avg 单位=100eRPM ⇒ 20e6/(avg*100) */
                            if (ci < 200u)   { ci = 200u; }
                            if (ci > 20000u) { ci = 20000u; }
                            ol_ci_tgt = ci;
                            cal_run = 0u;                            /* 先清零，allOff 才真关断 */
                            allOff();
                            running = 0;
                            rt_enter = 0u; rt_t0 = 0u; rt_sum = 0u; rt_n = 0u;
                            cal_run = 1u;                            /* → 用新 ci 跑第二轮校准 */
                        } else {
                            rt_sum += (uint32_t)e_rpm;
                            if (rt_n < 60000u) { rt_n++; }
                        }
                    } else if (rt_t0 != 0u) {
                        rt_t0 = 0u; rt_sum = 0u; rt_n = 0u;          /* 回到开环 ⇒ 重新等交棒 */
                    }
                }
            }
            if (cal_run == 1u) {                        /* 请求：等正常状态机自己起步 */
                { extern void rk_reset(void); extern volatile uint8_t rk_arm;
                  rk_reset(); rk_arm = 1u; }           /* ★ K5：开始累计 rank 增益 */
                adc_order_auto = 1u;
                cal_run = 2u;
            } else if (cal_run == 2u) {
                /* ★ 点火：正常路径的起步在"输入处理"段里（靠 DShot 触发），自驱绕过了它
                 *   ⇒ 这里替它调一次 startMotor()。油门/解锁已由输入层虚拟油门保证，
                 *   复位也不再触发 ⇒ 点了火就能被正常状态机接手。 */
                if (!running) { startMotor(); }
                if (running) { cal_run = 3u; }
            } else if (cal_run == 3u && !running) {     /* 转起来后又停了（到点/过流/失锁）*/
                {   /* ★ 面包屑：把停机现场录下来，看是谁把它关掉的
                     *   （这些都是本文件的全局，直接用，别加 extern —— 类型/限定符会对不上）*/
                    extern volatile uint16_t cal_dbg[8];
                    cal_dbg[0]=newinput; cal_dbg[1]=input; cal_dbg[2]=armed;
                    cal_dbg[3]=signaltimeout; cal_dbg[4]=ol_startup_stage;
                    cal_dbg[5]=ol_startup_arm; cal_dbg[6]=duty_cycle; cal_dbg[7]=actual_current;
                }
                { extern void rk_compute(void); extern volatile uint8_t rk_arm;
                  rk_arm = 0u; rk_compute(); }         /* ★ K5：跑完在片上算出六步增益 */
                adc_order_auto = 0u;                    /* ★ 必须还原：判据不许吃轮转序 */
                adc_order_pending = 0u;
                /* ★ 2026-10-20 用户流程：S1 开环(5s 算 v1) → 闭环测自然转速 → 停机冷却 2.5s
                 *   → S2 开环(ci*=对齐值, 油门 400, 5s 算 v2) → 最后闭环 → 收工 */
                if (cal_pass == 0u) {
                    uint32_t avg = cl_rn ? (cl_rs / (uint32_t)cl_rn) : 0u;
                    uint32_t ci;
                    if (avg < 50u)   { avg = 50u; }
                    ci = 200000UL / avg;                 /* avg 单位=100eRPM */
                    if (ci < 200u)   { ci = 200u; }
                    if (ci > 12000u) { ci = 12000u; }
                    ol_ci_tgt = ci;                      /* S4：开环频率对齐闭环自然转速 */
                    fin_erpm  = (int16_t)e_rpm;
                    cl_duty_match = (int16_t)duty_cycle;
                    { extern void dbg_two_save(uint8_t, const volatile int16_t*, const volatile int16_t*, const volatile int16_t*);
                      extern volatile int16_t rk_k[5]; extern volatile int16_t fit_g[7], fit_o[7];
                      dbg_two_save(1u, (const volatile int16_t*)&rk_k[1], fit_g, fit_o); }   /* v1=第一轮停机时的结果 */   /* S4 开环占空比与闭环一致 */
                    cal_pass  = 1u;
                    cal_run   = 5u;                      /* 停机 + 冷却 2.5s */
                    pause_t0  = adc_frame_total;
                } else {
                    fin_erpm = (int16_t)e_rpm;
                    fin_duty = (int16_t)duty_cycle;
                    fin_cur  = (int16_t)actual_current;
                    { extern void dbg_two_save(uint8_t, const volatile int16_t*, const volatile int16_t*, const volatile int16_t*);
                      extern volatile int16_t rk_k[5]; extern volatile int16_t fit_g[7], fit_o[7];
                      dbg_two_save(2u, (const volatile int16_t*)&rk_k[1], fit_g, fit_o); }   /* v2=第二轮停机时的结果 */
                    cal_run  = 0u;                       /* 收工 */
                }
                cl_rs = 0u; cl_rn = 0u;
            }
            /* ★ S4：停机冷却 2.5s（60000 帧）后进第二轮开环 */
            if (cal_run == 5u) {
                if (pause_t0 == 0u) { pause_t0 = adc_frame_total; }
                if ((uint32_t)(adc_frame_total - pause_t0) > 60000u) {
                    pause_t0 = 0u;
                    cal_run  = 1u;
                }
            }
            /* ★ 开环稳定段 5s：ISR 只置请求，真正算参数在主循环里做（不阻塞中断）*/
            if (cal_run == 3u && snap_t0 == 0u) { snap_t0 = adc_frame_total; }
            if (cal_snap_req == 0u && snap_t0 != 0u &&
                ((cal_pass == 0u && v1_ok == 0u) || (cal_pass == 1u && v2_ok == 0u)) &&
                (uint32_t)(adc_frame_total - snap_t0) >= 90000u) {   /* 6.4s 稳定开环 */
                { extern volatile uint16_t cal_dbg[8]; cal_dbg[0] = 0x5A01u; }
                cal_snap_req = (cal_pass == 0u) ? 1u : 2u;
                snap_t0 = 0u;
            }
            /* ★ 闭环段（交棒后）累计 e_rpm：S3 用来算自然转速 */
            if (cal_run == 3u && running && ol_startup_arm == 0u) {
                cl_rs += (uint32_t)e_rpm;
                if (cl_rn < 60000u) { cl_rn++; }
            }
        }
        /* ★ 校准命令（SWD 可写）：1=存默认 2=重载 3=存当前 */
        {
            extern volatile uint8_t esc_cal_cmd;
            if (esc_cal_cmd == 1u) { esc_cal_defaults(); (void)esc_cal_save(&esc_cal); esc_cal_cmd = 0u; }
            else if (esc_cal_cmd == 2u) { esc_cal_load(); esc_cal_cmd = 0u; }
            else if (esc_cal_cmd == 3u) { (void)esc_cal_save(&esc_cal); esc_cal_cmd = 0u; }
        }
        /* ★★ 2026-10-12 固件级闭环自动停机（RM2027_CL_RUN_MS）★★
         *   为什么放主循环：交棒后 olStartupTick() 直接 return（ol_startup_arm=0），
         *   所以闭环段没有任何到点检查 ⇒ 这里用"绝对帧号差"当计时器（与 ADC 同步，
         *   不受主循环快慢影响）。到点无条件收油 ⇒ 脚本挂了/电脑断了也不会一直转。
         *   ol_stop_reason 用 1（与开环"到时间上限"同类：非锁存，冷却后可再跑）。*/
        if (RM2027_CL_RUN_MS > 0 && cl_run_armed && running
            && (uint32_t)(adc_frame_total - cl_run_start) >= (uint32_t)RM2027_CL_RUN_MS * 24u) {
            allOff();
            running = 0;
            ol_state = 0;
            ol_input_cur = 0;
            ol_step_div_cur = OL_STEP_DIV;
            ol_run_ticks = 0;
            ol_timeouts++;
            ol_stop_reason = 1;
            ol_led_off_req = 1;
            cl_run_armed = 0;
            cl_runs_stopped++;
            cl_done_latch = 1;      /* ★ 锁存：必须收油后才能再跑（见启动逻辑处的判断）*/
        }
#if RM2027_LOCK_DETECT
        /* ★★★ 2026-10-13 失锁 ⇒ 立刻停机 ★★★
         *   这是"电机停转、换相照跑"的唯一出口。之前没有任何机制能发现这种状态：
         *     · desync_check 只在"换相间隔变长 >50%"时判失锁 —— 而我们这种死法是
         *       换相自己越跑越快、间隔一路【变短】⇒ 完全在它判据之外；
         *     · stuck_rotor_protection 被显式关掉了；
         *     · bemf_timeout 兜底换相反而让"没认到过零也照换"变成正常行为。
         *   判据 = A×ci（磁链）跌破健康峰值的一半（见 lock_frame）⇒ 实测分离度 230 倍。 */
        if (lock_lost) {
            lock_lost = 0;
            allOff();
            running = 0;
            ol_state = 0;
            ol_input_cur = 0;
            ol_step_div_cur = OL_STEP_DIV;
            ol_run_ticks = 0;
            ol_timeouts++;
            ol_stop_reason = 3;          /* 3 = 失锁（新站点码）*/
            ol_led_off_req = 1;
            cl_run_armed = 0;
            cl_done_latch = 1;           /* 与自动停机同规：必须先收油才能再跑 */
        }
#endif
        /* ★★ 2026-10-12 解除锁存必须放在【主循环】里 ★★
         *   我第一版把它放进了 olStepTick()，而那个函数在正常构建（非 OL_ARM_ONLY）
         *   下【根本不会被调用】⇒ 锁存一粘就永远粘住 ⇒ 一次自动停机之后电机再也起不来：
         *   实测 cl_done_latch 恒为 1、给油 6 秒 ol_steps Δ=0、running 恒为 0、串口 0 事件。
         *   收油（原始油门 < 48）或未解锁时，解除"闭环跑完"锁存与到点计时。 */
        if (newinput < 48 || !armed) {
            cl_done_latch = 0;
            cl_run_armed = 0;
        }
#endif
        if (one_khz_loop_counter > PID_LOOP_DIVIDER) { // 1khz PID loop
            PROCESS_ADC_FLAG = 1; // set flag to do new adc read at lower priority
            one_khz_loop_counter = 0;
            if (use_current_limit && running) {
                use_current_limit_adjust -= (int16_t)(doPidCalculations(&currentPid, actual_current,
                                                          eepromBuffer.limits.current * 2 * 100)
                    / 10000);
                if (use_current_limit_adjust < minimum_duty_cycle) {
                    use_current_limit_adjust = minimum_duty_cycle;
                }
                if (use_current_limit_adjust > 2000) {
                    use_current_limit_adjust = 2000;
                }
            }
            if (eepromBuffer.stall_protection && running) { // this boosts throttle as the rpm gets lower, for crawlers
                                               // and rc cars only, do not use for multirotors.
                stall_protection_adjust += (doPidCalculations(&stallPid, commutation_interval,
                                               stall_protect_target_interval));
                if (stall_protection_adjust > 150 * 10000) {
                    stall_protection_adjust = 150 * 10000;
                }
                if (stall_protection_adjust <= 0) {
                    stall_protection_adjust = 0;
                }
            }
            /* ★★★ 2026-10-11 三个守卫，缺一不可 ★★★
             *   ① !ol_startup_arm —— 开环斜坡/交棒期间不许速度环插手（否则和固定时基打架）
             *   ② target 合理性闸 —— target_e_com_time 若为 0 或离谱（映射没生效/EEPROM 垃圾），
             *      PID 误差会恒为正 ⇒ input_override 一路顶到最大 ⇒ 满油门飞车。这里限死
             *      200~60000µs（5000 ~ 16.7 步/秒）之外一律不调。
             *   ③ running —— 原就有 */
            if (use_speed_control_loop && running && !ol_startup_arm &&
                target_e_com_time >= 200 && target_e_com_time <= 60000) {
                input_override += doPidCalculations(&speedPid, e_com_time, target_e_com_time);
#if RM2027_CL_INPUT_CAP > 0
                if (input_override > (int32_t)RM2027_CL_INPUT_CAP * 10000) {
                    input_override = (int32_t)RM2027_CL_INPUT_CAP * 10000;
                }
#endif
                if (input_override > 2047 * 10000) {
                    input_override = 2047 * 10000;
                }
                if (input_override < 0) {
                    input_override = 0;
                }
                if (zero_crosses < 100) {
                    speedPid.integral = 0;
                }
            }
        }
        if (ramp_count > ramp_divider) {
          ramp_count = 0;
#ifdef VOLTAGE_BASED_RAMP
            uint16_t voltage_based_max_change = map(battery_voltage, 800, 2200, 10, 1);
            if (average_interval > 200) {
                max_duty_cycle_change = voltage_based_max_change;
            } else {
                max_duty_cycle_change = voltage_based_max_change * 3;
            }
#else
            if (zero_crosses < 150 || last_duty_cycle < 150) {   
                max_duty_cycle_change = max_ramp_startup;
            } else {
                if (average_interval > 500) {
                    max_duty_cycle_change = max_ramp_low_rpm;
                } else {
                    max_duty_cycle_change = max_ramp_high_rpm;
                }
            }
          
#endif
#ifdef CUSTOM_RAMP
   //         max_duty_cycle_change = eepromBuffer[30];
#endif
            if ((duty_cycle - last_duty_cycle) > max_duty_cycle_change) {
                duty_cycle = last_duty_cycle + max_duty_cycle_change;

            }
            if ((last_duty_cycle - duty_cycle) > max_duty_cycle_change) {
                duty_cycle = last_duty_cycle - max_duty_cycle_change;
            }
            }else{
             duty_cycle = last_duty_cycle;
            }

        if ((armed && running) && input > 47) {
          if(zero_throttle_brake_active){
            zero_throttle_brake_active = 0;
            temp_comp_pwm = eepromBuffer.comp_pwm;
          }else{
            adjusted_duty_cycle = ((duty_cycle * tim1_arr) / 2000) + 1;
        }
        } else {
          if(running && input < 47){ // brake on zero throttle behavior while motor is still rotating
            if(eepromBuffer.brake_on_zero_throttle == 1){   // coast on 0 throttle
              temp_comp_pwm = 0;                            // tracks rpm until stopped 
              zero_throttle_brake_active = 1;
            }
              if(eepromBuffer.brake_on_zero_throttle == 2){   // motor brake on 0 throttle    
              temp_comp_pwm = 1;                             // tracks rpm until stopped
              zero_throttle_brake_active = 1;
            }
              if((eepromBuffer.brake_on_zero_throttle > 2) && (eepromBuffer.brake_on_zero_throttle < 10)){   // brake on 0 throttle after 2 + x seconds
              if(zero_throttle_brake_active == 0){
                brake_countdown = eepromBuffer.brake_on_zero_throttle - 2;  // brake countdown decremented in 10khz routine
                tenkhzcounter = 10000; 
              }
              if((brake_countdown == 0) && (zero_throttle_brake_active == 1)){
                zero_crosses = 0;                          // after countdown forces the brake on stop behavior 
                running = 0; ol_stop_site = 6;   /* 停机站点：零油门刹车 */
              }
              zero_throttle_brake_active = 1;
              }
              adjusted_duty_cycle = ((duty_cycle * tim1_arr) / 2000);
          } else{  // input less than 47 and not running, normal brake on stop behavior
            if (prop_brake_active) {
              adjusted_duty_cycle =  tim1_arr - ((prop_brake_duty_cycle * tim1_arr) / 2000);
            } else {
              if((eepromBuffer.brake_on_stop == 2) && armed){  // require arming for active brake
                comStep(2);
                adjusted_duty_cycle = DEAD_TIME + ((eepromBuffer.active_brake_power * tim1_arr) / 2000)* 10;
            }else{
                adjusted_duty_cycle = ((duty_cycle * tim1_arr) / 2000);
            }
            }
          }
        }
        last_duty_cycle = duty_cycle;
        SET_AUTO_RELOAD_PWM(tim1_arr);
        /* ★★★ 2026-10-19 K2a-3：静态校准期间【让出】三路 CCR 的所有权 ★★★
         *   本函数是 20kHz 的 PWM 中断，结尾这句 SET_DUTY_CYCLE_ALL 会把 TIM1->CCR1/2/3
         *   全部重写成 adjusted_duty_cycle。F1 收油(f1 idle)时 duty_cycle=0 ⇒ 它每 50µs 把三路
         *   CCR 清成 0 一次，于是 esc_cal_poll() 刚写进去的校准占空比【根本活不过一个中断周期】：
         *   门极全程关断、三相读数恒 ~0 码。
         *   —— 这才是"校准期间电机不驱动"的真正根因（不是 TIM1 MOE、也不是引脚 AF：
         *      实测 BDTR.MOE=1、AFRH=0x222、引脚在 AF 模式，但 CCR 被这里反复清零）。
         *   约定与 phaseouts.c 的 allOff() 里那条"校准期间让路"完全相同：校准模式 = 谁都不许动门极。 */
        if (cal_mode == 0u) {
            SET_DUTY_CYCLE_ALL(adjusted_duty_cycle);
        }
    }
#endif // ndef brushed_mode
#if defined(FIXED_DUTY_MODE) || defined(FIXED_SPEED_MODE)
    if (getInputPinState()) {
        signaltimeout++;
        if (signaltimeout > LOOP_FREQUENCY_HZ) {
            NVIC_SystemReset();
        }
    } else {
        signaltimeout = 0;
    }
#else
    /* ★★ K4：自驱动校准跑期间【冻结"信号超时"计时器】—— 这就是"DShot 依赖"的解耦点 ★★
     *   为什么必须在这里：signaltimeout 由本函数（20kHz 中断）无条件累加，
     *   而"掉信号"处理在超过 0.5s 时做 armed=0 / input=0 / allOff()。
     *   自驱没有 DShot ⇒ 主循环里压 0 没用（ISR 每 50µs 又加 1）
     *   ⇒ 0.5 秒后起步链自我关闭（实测：点火后 ~20-40ms 就掉，且无 ol_stop_site
     *     记录 —— 它不是"保护停机"，是"掉信号"）。
     *   ⚠ 只冻结这一个计时器：过流/失速/到点(OL_RUN_LIMIT)/失锁 全部照旧有效。 */
    { extern volatile uint8_t cal_run;
      if (cal_run == 0u) { signaltimeout++; } else { signaltimeout = 0; } }

#endif
}

void processDshot()
{
    if (compute_dshot_flag == 1) {
        computeDshotDMA();
        compute_dshot_flag = 0;
    }
    if (compute_dshot_flag == 2) {
      if(e_com_time > 65535){    // beyond dshot range
        make_dshot_package(65535);
      }else{
        make_dshot_package(e_com_time);
      }
        compute_dshot_flag = 0;
        return;
    }
    setInput();
}

void advanceincrement()
{
    if (!forward) {
        phase_A_position++;
        if (phase_A_position > 359) {
            phase_A_position = 0;
        }
        phase_B_position++;
        if (phase_B_position > 359) {
            phase_B_position = 0;
        }
        phase_C_position++;
        if (phase_C_position > 359) {
            phase_C_position = 0;
        }
    } else {
        phase_A_position--;
        if (phase_A_position < 0) {
            phase_A_position = 359;
        }
        phase_B_position--;
        if (phase_B_position < 0) {
            phase_B_position = 359;
        }
        phase_C_position--;
        if (phase_C_position < 0) {
            phase_C_position = 359;
        }
    }
#ifdef GIMBAL_MODE
    setPWMCompare1(((2 * pwmSin[phase_A_position]) + gate_drive_offset) * TIMER1_MAX_ARR / 2000);
    setPWMCompare2(((2 * pwmSin[phase_B_position]) + gate_drive_offset) * TIMER1_MAX_ARR / 2000);
    setPWMCompare3(((2 * pwmSin[phase_C_position]) + gate_drive_offset) * TIMER1_MAX_ARR / 2000);
#else
    setPWMCompare1(
        (((2 * pwmSin[phase_A_position] / SINE_DIVIDER) + gate_drive_offset) * TIMER1_MAX_ARR / 2000) * eepromBuffer.sine_mode_power / 10);
    setPWMCompare2(
        (((2 * pwmSin[phase_B_position] / SINE_DIVIDER) + gate_drive_offset) * TIMER1_MAX_ARR / 2000) * eepromBuffer.sine_mode_power / 10);
    setPWMCompare3(
        (((2 * pwmSin[phase_C_position] / SINE_DIVIDER) + gate_drive_offset) * TIMER1_MAX_ARR / 2000) * eepromBuffer.sine_mode_power / 10);
#endif
}

void zcfoundroutine()
{ // only used in polling mode, blocking routine.
#if RM2027_OL_ARM_ONLY
    return; /* 开环调试：此路径不参与换相（双保险，防止别处调用它换相） */
#endif
    thiszctime = INTERVAL_TIMER_COUNT;
    SET_INTERVAL_TIMER_COUNT(0);
    commutation_interval = (thiszctime + (3 * commutation_interval)) / 4;
    advance = (temp_advance * commutation_interval) >> 6; //   7.5 degree increments
    waitTime = commutation_interval / 2 - advance;
    while ((INTERVAL_TIMER_COUNT) < (waitTime)) {
        if (zero_crosses < 5) {
            break;
        }
    }
#ifdef MCU_GDE23
    TIMER_CAR(COM_TIMER) = waitTime;
#endif
#ifdef STMICRO
    COM_TIMER->ARR = waitTime;
#endif
#ifdef MCU_AT32
		COM_TIMER->pr = waitTime;
#endif
#ifdef NXP
//	COM_TIMER->MSR[0] = waitTime;
	COM_TIMER->MR[0] = waitTime;
#endif

    commutate();
    bemfcounter = 0;
    bad_count = 0;

    zero_crosses++;
#ifdef NO_POLLING_START     // changes to interrupt mode after 2 zero crosses, does not re-enter
       if (zero_crosses > 2) {
            old_routine = 0;
            enableCompInterrupts(); // enable interrupt
        }
#else
    if (eepromBuffer.stall_protection || eepromBuffer.rc_car_reverse) {
        if (zero_crosses >= 20 && commutation_interval <= 2000) {
            old_routine = 0;
            enableCompInterrupts(); // enable interrupt
        }
    } else {
       if (commutation_interval < polling_mode_changeover) {
            old_routine = 0;
            enableCompInterrupts(); // enable interrupt
        }
    }
 #endif
}
#ifdef BRUSHED_MODE
void runBrushedLoop()
{

    uint16_t brushed_duty_cycle = 0;

    if (brushed_direction_set == 0 && adjusted_input > 48) {
        if (forward) {
            allOff();
            delayMicros(10);
            twoChannelForward();
        } else {
            allOff();
            delayMicros(10);
            twoChannelReverse();
        }
        brushed_direction_set = 1;
    }

    brushed_duty_cycle = map(adjusted_input, 48, 2047, 0,
        (TIMER1_MAX_ARR - (TIMER1_MAX_ARR / 20)));

    if (degrees_celsius > eepromBuffer.limits.temperature) {
        duty_cycle_maximum = map(degrees_celsius, eepromBuffer.limits.temperature,
            eepromBuffer.limits.temperature + 20, TIMER1_MAX_ARR / 2, 1);
    } else {
        duty_cycle_maximum = TIMER1_MAX_ARR - 50;
    }
    if (brushed_duty_cycle > duty_cycle_maximum) {
        brushed_duty_cycle = duty_cycle_maximum;
    }

    if (use_current_limit) {
        use_current_limit_adjust -= (int16_t)(doPidCalculations(&currentPid, actual_current,
                                                  eepromBuffer.limits.current * 100)
            / 10000);
        if (use_current_limit_adjust < minimum_duty_cycle) {
            use_current_limit_adjust = minimum_duty_cycle;
        }

        if (brushed_duty_cycle > use_current_limit_adjust) {
            brushed_duty_cycle = use_current_limit_adjust;
        }
    }
    if ((brushed_duty_cycle > 0) && armed) {
        SET_DUTY_CYCLE_ALL(brushed_duty_cycle);
        //	  	TIM1->CCR1 = brushed_duty_cycle;
        //		TIM1->CCR2 = brushed_duty_cycle;
        //		TIM1->CCR3 = brushed_duty_cycle;

    } else {
        SET_DUTY_CYCLE_ALL(0);
        //		TIM1->CCR1 = 0;
        //// 		TIM1->CCR2 = 0; 		TIM1->CCR3 = 0;
        brushed_direction_set = 0;
    }
}
#endif


/*
  check device info from the bootloader, confirming pin code and eeprom location
 */
static void checkDeviceInfo(void)
{
#ifdef MCU_SITL
    // no bootloader device info page in SITL
    return;
#endif
#ifdef NXP
    uint32_t pflashBlockBase  = 0U;
    uint32_t pflashTotalSize  = 0U;
    uint32_t pflashSectorSize = 0U;

    //Get flash properties
    FLASH_API->flash_get_property(&s_flashDriver, kFLASH_PropertyPflashBlockBaseAddr, &pflashBlockBase);
    FLASH_API->flash_get_property(&s_flashDriver, kFLASH_PropertyPflashSectorSize, &pflashSectorSize);
    FLASH_API->flash_get_property(&s_flashDriver, kFLASH_PropertyPflashTotalSize, &pflashTotalSize);
#else
#define DEVINFO_MAGIC1 0x5925e3da
#define DEVINFO_MAGIC2 0x4eb863d9

    const struct devinfo {
        uint32_t magic1;
        uint32_t magic2;
        const uint8_t deviceInfo[9];
    } *devinfo = (struct devinfo *)(0x1000 - 32);
    if (devinfo->magic1 != DEVINFO_MAGIC1 ||
        devinfo->magic2 != DEVINFO_MAGIC2) {
        // bootloader does not support this feature, nothing to do
        return;
    }
    // change eeprom_address based on the code in the bootloaders device info
    switch (devinfo->deviceInfo[4]) {
        case 0x1f:
            eeprom_address = 0x08007c00;
            break;
        case 0x35:
            eeprom_address = 0x0800f800;
            break;
        case 0x2b:
            eeprom_address = 0x0801f800;
            break;
    }
#endif

    // TODO: check pin code and reboot to bootloader if incorrect

}

int main(void)
{

#ifdef NXP
    initCorePeripherals();
    checkDeviceInfo();
    loadEEpromSettings();
    enableCorePeripherals();
    initAfterJump();
#else
    initAfterJump();
    checkDeviceInfo();
    initCorePeripherals();
    enableCorePeripherals();
    loadEEpromSettings();
    { extern void v2_apply(void); v2_apply(); }   /* ★ 正常电调：V2 标定结果直接生效 */
#endif

    /* ★★ 上电就把六路门极显式关成"断开(coast)" —— 别依赖引脚的上电默认状态 ★★
     * 为什么必须显式做：本板三路低边接的是 TIM1 的【互补输出】(PA7=CH1N / PB0=CH2N /
     * PB1=CH3N)，而互补输出在 CCR=0 时电平是【高】⇒ 若不显式关闭，上电后低边 MOS
     * 会一直通着（2026-10-07 实测：停机态 C 相下管引脚 100% 高、C 相被短到地），
     * 用户一给母线电就听到持续啸叫；把六路门极强制成 GPIO 输出低后声音立刻消失。
     * allOff() 会把三相都设成"高边关 + 低边关 + 引脚 GPIO 输出低"。
     * 注：这一条也让"停机≠刹车"成为确定状态（本模式 brake_on_stop/brake_on_zero_throttle 都是 0）。 */
    allOff();

    if (VERSION_MAJOR != eepromBuffer.version.major || VERSION_MINOR != eepromBuffer.version.minor || EEPROM_VERSION > eepromBuffer.eeprom_version) {
        eepromBuffer.version.major = VERSION_MAJOR;
        eepromBuffer.version.minor = VERSION_MINOR;
        eepromBuffer.eeprom_version = EEPROM_VERSION;
        saveEEpromSettings();
    }
    
    if (eepromBuffer.dir_reversed == 1) {
        forward = 0;
    } else {
        forward = 1;
    }
    tim1_arr = TIMER1_MAX_ARR;
    if (!eepromBuffer.comp_pwm) {
        eepromBuffer.use_sine_start = 0; // sine start requires complementary pwm.
    }

    if (eepromBuffer.rc_car_reverse) { // overrides a whole lot of things!
        throttle_max_at_low_rpm = 1000;
        eepromBuffer.bi_direction = 1;
        eepromBuffer.use_sine_start = 0;
        low_rpm_throttle_limit = 1;
        eepromBuffer.variable_pwm = 0;
        eepromBuffer.brake_on_zero_throttle = 0;
        eepromBuffer.comp_pwm = 0;
        temp_comp_pwm = 0;
        eepromBuffer.stuck_rotor_protection = 0;
        minimum_duty_cycle = minimum_duty_cycle + 50;
        stall_protect_minimum_duty = stall_protect_minimum_duty + 50;
        min_startup_duty = min_startup_duty + 50;
    }

#ifdef MCU_F031
    GPIOF->BSRR = LL_GPIO_PIN_6; // uncomment to take bridge out of standby mode
                                 // and set oc level
    GPIOF->BRR = LL_GPIO_PIN_7; // out of standby mode
    GPIOA->BRR = LL_GPIO_PIN_11;
#endif
#ifdef MCU_G031
    GPIOA->BRR = LL_GPIO_PIN_11;
    GPIOA->BSRR = LL_GPIO_PIN_12;    // Pa12 attached to enable on dev board
#endif
#ifdef USE_LED_STRIP
    send_LED_RGB(125, 0, 0);
#endif
#ifdef USE_RGB_LED
     setIndividualRGBLed(1,0,0);
#endif

#ifdef USE_CRSF_INPUT
    inputSet = 1;
    playStartupTune();
    MX_IWDG_Init();
    LL_IWDG_ReloadCounter(IWDG);
#else
#if defined(FIXED_DUTY_MODE) || defined(FIXED_SPEED_MODE)
    MX_IWDG_Init();
    RELOAD_WATCHDOG_COUNTER();
    inputSet = 1;
    armed = 1;
    adjusted_input = 48;
    newinput = 48;
		comStep(2);
#ifdef FIXED_SPEED_MODE
    use_speed_control_loop = 1;
    eepromBuffer.use_sine_start = 0;
    target_e_com_time = 60000000 / FIXED_SPEED_MODE_RPM / (eepromBuffer.motor_poles / 2);
    input = 48;
#endif

#else
#ifdef BRUSHED_MODE
    // bi_direction = 1;
    commutation_interval = 5000;
    eepromBuffer.use_sine_start = 0;
    maskPhaseInterrupts();
    playBrushedStartupTune();
#else
 #ifdef MCU_AT415
    play_tone_flag = 5;
 #else
    playStartupTune();
	#endif
#endif
    zero_input_count = 0;
    MX_IWDG_Init();
    RELOAD_WATCHDOG_COUNTER();
#ifdef GIMBAL_MODE
    eepromBuffer.bi_direction = 1;
    eepromBuffer.use_sine_start = 1;
#endif

#ifdef USE_ADC_INPUT
    armed_count_threshold = 5000;
    inputSet = 1;

#else
    // checkForHighSignal();     // will reboot if signal line is high for 10ms
    receiveDshotDma();
    if (drive_by_rpm) {
        use_speed_control_loop = 1;
    }
#endif

#endif // end fixed duty mode ifdef
#endif // end crsf input

#ifdef MCU_F051
    MCU_Id = DBGMCU->IDCODE &= 0xFFF;
    REV_Id = DBGMCU->IDCODE >> 16;

    if (REV_Id >= 4096) {
        temperature_offset = 0;
    } else {
        temperature_offset = 230;
    }

#endif
#ifdef NEUTRONRC_G071
    setInputPullDown();
#else
    setInputPullUp();
#endif

#ifdef USE_STARTUP_BOOST
  min_startup_duty = min_startup_duty + 200 + ((eepromBuffer.pwm_frequency * 100)/24);
  minimum_duty_cycle = minimum_duty_cycle + 50 + ((eepromBuffer.pwm_frequency * 50 )/24);
  startup_max_duty_cycle = startup_max_duty_cycle + 400;
#endif

#if RM2027_BENCH_PIN_EEPROM
    /* ★ 台架模式：把依赖的外部状态在 RAM 里钉成确定值（不写回 EEPROM）。
     *   起因：本板 EEPROM 从第 17 字节起是 0xFF 擦除态 —— motor_poles、limits.current、
     *   input_type、stuck_rotor_protection 等都是垃圾值，不能指望它。
     *   ★ 2026-10-07 从 RM2027_OL_ARM_ONLY 里【拆出来】独立成一个开关：
     *     开环（OL）与"闭环"切换时，这段钉死参数必须【一直保留】——
     *     RM2027_OL_ARM_ONLY 一旦关掉（转闭环），若这段也跟着消失，
     *     motor_poles/limits 就变成擦除态的垃圾值，转速/限流全乱。 */
    eepromBuffer.input_type = DSHOT_IN;      /* ★★ 2026-10-08 补上（原来漏钉！）：不钉的话
                                              *   擦除态 0xFF 会走"else ⇒ dshot=0"分支，
                                              *   于是油门值永远进不了 newinput ⇒ input 恒 0
                                              *   ⇒ 起步链门槛(input>=47)过不去 ⇒ 电机不转。
                                              *   ★ 注意：loadEEpromSettings() 在本段【之前】就按
                                              *     擦除态算过 dshot/EDT_ARMED 了 ⇒ 只改 input_type
                                              *     没用，必须把 dshot / servoPwm / EDT_ARMED
                                              *     也一并钉死（与 DSHOT_IN 分支的赋值一致）。 */
    dshot = 1;
    servoPwm = 0;
    EDT_ARMED = 1;
    eepromBuffer.bi_direction = 0;           /* 输入映射走最朴素的那一条 */
    eepromBuffer.use_sine_start = 0;         /* 正弦启动会跟六步抢时序 */
    eepromBuffer.stall_protection = 0;       /* 失速 PID 会按 commutation_interval 猛加占空比 */
    eepromBuffer.stuck_rotor_protection = 0; /* 堵转保护会 allOff + 清零油门 */
    eepromBuffer.low_voltage_cut_off = 0;    /* 台架电源偏低时别被低压保护停掉 */
    eepromBuffer.variable_pwm = 0;           /* 否则 tim1_arr 随转速跳变、PWM 频率乱变 */
    eepromBuffer.brake_on_zero_throttle = 0;
    eepromBuffer.brake_on_stop = 0;
    /* ---- 本板电机参数（只写 RAM）：HSKRC 1404-4600KV，9N12P（9 槽 12 磁钢 ⇒ 6 对极）。
     *   motor_poles 存的是【磁极数 = 磁钢片数】，不是极对数 —— 代码内部再 /2 才得极对数：
     *   DroneCAN.c:1128 `pkt.rpm = (e_rpm * 200) / eepromBuffer.motor_poles` 只有取磁极数时才成立。
     *   motor_kv 存压缩字节 (KV-20)/40：114 → 4580KV、115 → 4620KV，最接近铭牌 4600。
     *   注：low/high_rpm_level 等派生量已在 loadEEpromSettings() 里按擦除态算过（现为 0 =
     *   低转速油门限制失效），这里不重算 —— 对 5% 占空比的台架测试无影响；要让它真正生效
     *   需把这两项写进 EEPROM（写之前先确认 rc_car_reverse 非 0，见下条）。 */
    eepromBuffer.motor_kv = 114;
    eepromBuffer.motor_poles = 12;
    /* ★★★ 2026-10-11 接通"外环速度环"（缺的就是这一环）★★★
     *   背景：实测闭环后转子稳定在 ~385 步/秒（波形自身周期就是 2.62ms ⇒ 是转子真的慢，
     *   不是判据漏检；判据 8/8 全中、相位 0.51 步理想、占空比恒定）。而开环靠固定时基
     *   硬拖能到 1000 步/秒 ⇒ 说明"占空比固定"时转子只能停在那个电压支撑得起的转速上。
     *   业界结构（MATLAB/Microchip）：
     *       外环 PI 速度环 → 占空比给定 ；过零 → +30° → 六步换相
     *   而 AM32 本来就有这个外环（speedPid + target_e_com_time）⇒ 只需接通：
     *     · drive_by_rpm = 1 会自动把 use_speed_control_loop 置 1（见 receiveDshotDma 处）
     *     · MIN/MAXIMUM_RPM_SPEED_CONTROL 决定"油门 → 目标转速"的映射：
     *         target_e_com_time = 60000000 / map(input,47,2047,MIN,MAX) / (poles/2)
     *       单位核对过：e_com_time = 6 个换相步(µs) = 一个电周期(µs)，与 target 同量纲 ✓
     *     · 台架油门约 400 ⇒ (400-47)/2000 = 17.6% ⇒ 要让工作点落在 1667 机械 RPM
     *       （= 开环的终速，在齿槽门槛 900RPM 之上）：
     *         800 + 0.176 x (5700-800) ≈ 1665 ⇒ 取 MIN=800 / MAX=5700 ✓ */
    drive_by_rpm = 1;
    /* ★ 注意顺序：main() 里第 3159 行有 `if (drive_by_rpm) use_speed_control_loop = 1;`
     *   而它在本行【之前】执行 ⇒ 只靠 drive_by_rpm 拉不起来 ⇒ 这里再显式置一次，
     *   不依赖执行顺序。 */
    use_speed_control_loop = 1;
    MINIMUM_RPM_SPEED_CONTROL  = 800;
    MAXIMUM_RPM_SPEED_CONTROL  = 5700;
    /* ---- 堵直通隐患：temp_comp_pwm 非 0 时 phaseXPWM() 会把低边栅极常开
     *   （Mcu/g071/Src/phaseouts.c:139 的 else 分支），与高边斩波叠加就是半桥直通。
     *   擦除态 eepromBuffer.comp_pwm = 0xFF（非 0），目前只靠 rc_car_reverse 那条路径
     *   （main.c:1890）把它清 0。这里两个都显式钉死，免得以后保存设置后关系被破坏。 */
    eepromBuffer.comp_pwm = 0;
    temp_comp_pwm = 0;
    /* ★★ 2026-10-09：把换相提前量钉成 0 ★★
     *   来源：擦除态 EEPROM 的 advance_level = 0xFF = 255 落在 loadEEpromSettings() 的
     *   `advance_level > 42` 那一支，被钉成 temp_advance = 16 ⇒
     *        advance = ci×16/64 = ci/4  ⇒  waitTime = ci/2 − ci/4 = ci/4
     *   即换相发生在【过零后 15°】，而教科书位置是 30°（waitTime = ci/2）。
     *   把换相点整整挪掉半个理想角，在低转速（实测只有 ~390RPM、反电势才 ~70 码）
     *   下换相缺口里会出现反转矩 ⇒ 转矩变差 ⇒ 转子带不住 ⇒ 转速从 2222 塌到 390。
     *   实测波形侧证：悬空相反电势幅值只有 ~70 码（390RPM 该有的大小），
     *   而 2222RPM 时是 ~400 码 —— 是"转速塌了"，不是"没信号"。
     *   advance_level = 10 ⇒ temp_advance = 0（见 loadEEpromSettings 的换算），
     *   这里直接写 temp_advance 更直白，不受 EEPROM 分支影响。 */
    temp_advance = 0;
    eepromBuffer.advance_level = 10;
    /* ★ 还有一处旁路必须一起关掉：换成 auto_advance 后 advance 用的是
     *   auto_advance_level，而它是 main 循环里按【占空比】算的
     *       auto_advance_level = map(duty_cycle, 100, 2000, 13, 23)
     *   ⇒ duty=251 时得 13，于是 advance = ci×13/64 ≈ 0.20ci —— 与实测
     *   advance/ci = 0.203 完全对上（这就是上一版 temp_advance=0 没生效的原因）。
     *   擦除态 EEPROM 的 auto_advance = 0xFF ≠ 0 ⇒ 默认就走这条路。钉成 0，
     *   让换相点真正回到 waitTime = ci/2 的教科书位置。 */
    eepromBuffer.auto_advance = 0;
    minimum_duty_cycle = 0;                  /* 清掉 eeprom / dead_time_override 留下的占空比地板 */
    min_startup_duty = 0;                    /* ★ 2026-10-07 撤回 350：用户实测"插着 ST-Link
                                              * 母线被拉塌、拔掉正常"，而开环版（已知良性）里
                                              * 唯一与之前"母线稳 12V"那批会话不同的、行为性的
                                              * 改动就是这一行 ⇒ 先撤回到 0，把变量逐个排除。
                                              * （闭环要用时再加回 350 并单独验证。）*/
    /* ★★★ 2026-10-11 上限 400 -> 700（用户要求"提高开环占空比"）★★★
     *   这一句是【起步期占空比的硬上限】：main.c:2338 会
     *       if (duty_cycle_setpoint > startup_max_duty_cycle) duty_cycle_setpoint = ...;
     *   所以把 OL_INPUT_START/RUN 从 400 提到 600 也没用 —— 实测占空比正好卡在 400。
     *   改成 700：输入 600 ⇒ 占空比约 553（≈ 原来 351 的 1.6 倍），
     *   目的是让低频段（反电势≈0）有足够电压把转子带起来。
     *   安全网仍在：过流 OL_TRIP_CURRENT=8A、总时长 8.0s。 */
    startup_max_duty_cycle = 400;      /* 回归档值（曾为试高占空比临时放到 700）*/
    tim1_arr = TIMER1_MAX_ARR;
#if RM2027_OL_ARM_ONLY
    old_routine = 1;                         /* 开环独占版：换相由 olStepTick 独占 */
#else
    /* ★ 2026-10-08 改：闭环不再在这里把 old_routine 钉成 0。
     *   原来钉 0 是为了让主循环 `if (!old_routine) startMotor();` 能起步，
     *   但那样起步就直接进中断闭环了（本板认不到低速过零 ⇒ 自锁）。
     *   现在起步由 startMotor() 里置 1 + 走开环起步链（olStartupTick）来完成，
     *   起步链跑完才把它清 0 交给闭环。这里保持"不参与起步"的初值即可，
     *   真正的起步门控在 startMotor() 里无条件执行（见那里的说明）。 */
    old_routine = 1;
#endif
    zero_crosses = 0;
#endif

    while (1) {
#if ADC_CAPTURE_MODE
        adc_capture_drain();     /* ★ 把装满的 ADC 窗口发到串口 */
#endif
e_com_time = ((commutation_intervals[0] + commutation_intervals[1] + commutation_intervals[2] + commutation_intervals[3] + commutation_intervals[4] + commutation_intervals[5]) + 4) >> 1; // COMMUTATION INTERVAL IS 0.5US INCREMENTS 

#if defined(FIXED_DUTY_MODE) || defined(FIXED_SPEED_MODE)
        setInput();
#endif

#ifdef NEED_INPUT_READY
 #ifdef MCU_F031
    if (input_ready) {
    setInput(); 
    input_ready = 0;
    }
#else
    if (input_ready) {
     processDshot();
     input_ready = 0;
     }
#endif
#endif
        /* ★★ K4：台架【虚拟油门】——在输入层注入，下游状态机一行都不用改 ★★
         *   为什么必须在这一层：自驱跳过了 DShot，而"信号存在"是整条链的前提 ——
         *   没有 DShot 时 input 恒 0（到不了 input>47 的起步条件），
         *   且 signaltimeout 必然超时 ⇒ 每 ~2s 触发"掉信号复位"(NVIC_SystemReset)
         *   ⇒ 自驱刚起来就被清掉（实测：ol_ci_cur 回初值、ol_stop_site=0 = 被复位）。
         *   所以这里只假装"我一直在给油、信号一直在"，让【解锁/起步/保护原样跑】。
         *   ⚠ 不绕过任何保护：过流、失速、到点(OL_RUN_LIMIT=8s)、失锁、掉信号 全部照旧。 */
        {
            /* ★ 2026-10-20：两轮标定的"算参数"放在主循环（ISR 里只置 cal_snap_req），
             *   避免在 20kHz 中断里跑 rk_compute 造成时序抖动。*/
            if (cal_snap_req != 0u) {
                uint8_t w = cal_snap_req; cal_snap_req = 0u;
                { extern void rk_compute(void);
                  extern volatile int16_t rk_k[5];
                  extern volatile int16_t rank_g[4];
                  for (uint8_t i=0u;i<4u;i++) { rank_g[i] = rk_k[i+1u]; }   /* ★ 现成的 4 个 rank 槽增益 */
                  if (w == 1u) { for (uint8_t i=0u;i<4u;i++) { v1_rank_g[i]=rank_g[i]; } }
                  else         { for (uint8_t i=0u;i<4u;i++) { v2_rank_g[i]=rank_g[i]; } }
                  rk_compute(); }
                { extern volatile uint16_t cal_dbg[8]; cal_dbg[1] = 0x5A02u; }
                if (w == 1u) {
                    for (uint8_t i = 0u; i < 7u; i++) { v1_fg[i]=fit_g[i]; v1_fo[i]=fit_o[i]; }
                    for (uint8_t sx=0u; sx<7u; sx++) for (uint8_t ox=0u; ox<4u; ox++) { v1_rkn[sx][ox]=rk_n[sx][ox]; v1_rks[sx][ox]=rk_sum[sx][ox]; }
                    v1_ok = 1u;
                } else {
                    for (uint8_t i = 0u; i < 7u; i++) { v2_fg[i]=fit_g[i]; v2_fo[i]=fit_o[i]; }
                    for (uint8_t sx=0u; sx<7u; sx++) for (uint8_t ox=0u; ox<4u; ox++) { v2_rkn[sx][ox]=rk_n[sx][ox]; v2_rks[sx][ox]=rk_sum[sx][ox]; }
                    v2_ok = 1u;
                }
            }
            extern volatile uint8_t cal_run;
            {
                extern volatile uint8_t bench_run;
                extern volatile int16_t bench_thr;
                if (bench_run != 0u) {
                    input         = (int16_t)bench_thr;
                    newinput      = (int16_t)bench_thr;
                    armed         = 1;
                    signaltimeout = 0;
                    if (!running) { startMotor(); }
                }
            }
            if (cal_run != 0u) {   /* ★ 从 cal_run=1 起就置油门：避免 startMotor 第一次跑在 input=0 */
                input         = (cl_duty_match > 0) ? (int16_t)(cl_duty_match * 115 / 100 + 47) : 400;      /* ★ 统一油门 400：开环段与闭环(400)同工作点 */
                newinput      = (cl_duty_match > 0) ? (int16_t)(cl_duty_match * 115 / 100 + 47) : 400;      /* ★ 必须一起钉！olStartupTick() 判"收油门即停"
                                           *   用的是【原始油门 newinput】（main.c:1332），
                                           *   只钉 input ⇒ 起步链第一个 tick 就 allOff。 */
                armed         = 1;        /* 虚拟解锁 */
                signaltimeout = 0;        /* 虚拟"信号一直在" ⇒ 不触发上锁/复位 */
            }
        }
if(zero_crosses < 5){
    if(eepromBuffer.bi_direction){
     min_bemf_counts_up = TARGET_MIN_BEMF_COUNTS + 1;
     min_bemf_counts_down = TARGET_MIN_BEMF_COUNTS + 1;
   }else{
     min_bemf_counts_up = TARGET_MIN_BEMF_COUNTS * 2;
     min_bemf_counts_down = TARGET_MIN_BEMF_COUNTS * 2;
   }
}else{
	  min_bemf_counts_up = TARGET_MIN_BEMF_COUNTS;
	  min_bemf_counts_down = TARGET_MIN_BEMF_COUNTS;
}

       RELOAD_WATCHDOG_COUNTER();

        if (eepromBuffer.variable_pwm == 1) {      // uses range defined by pwm frequency setting
            tim1_arr = map(commutation_interval, 96, 200, TIMER1_MAX_ARR / 2,
                TIMER1_MAX_ARR);
        }
        if (eepromBuffer.variable_pwm == 2) {      // uses automatic range   
          if(average_interval < 250 && average_interval > 100){
            tim1_arr = average_interval * (CPU_FREQUENCY_MHZ/9);
          }
          if(average_interval < 100 && average_interval > 0){
            tim1_arr = 100 * (CPU_FREQUENCY_MHZ/9);
         }
          if((average_interval >= 250) || (average_interval == 0)){
              tim1_arr = 250 * (CPU_FREQUENCY_MHZ/9);
          } 
        }
        if (signaltimeout > (LOOP_FREQUENCY_HZ >> 1)) { // half second timeout when armed;
            if (armed) {
                allOff();
                armed = 0;
                input = 0;
                inputSet = 0;
                zero_input_count = 0;
                SET_DUTY_CYCLE_ALL(0);
                resetInputCaptureTimer();
                for (int i = 0; i < 64; i++) {
                    dma_buffer[i] = 0;
                }
#ifdef USE_LED_STRIP
                /* ★ 改回"待命红"：原来"掉信号就上锁"时这一步是靠下面的 NVIC_SystemReset() 顺手
                 *   把 LED 复位掉的；台架模式不复位，所以要自己设 —— 否则 armed 已清零、门极
                 *   也全关，绿灯（已解锁指示）却还亮着，看上去像"没上锁"。
                 *   颜色约定（与本固件开机时的指示一致）：**暗红 = 未解锁/已上锁待命，绿 = 已解锁**。
                 *   不要设成全灭：全灭分不清"上锁"和"没电"，实测会误导。
                 *   只在这个 1→0 的跳变处发一次，不在下面"每轮都执行"的 2 秒分支里重复发。 */
                send_LED_RGB(125, 0, 0);
#endif
                /* ★★ 这个复位【必须保留】，不能省！（2026-10-07 实测定位的严重缺陷）
                 *   为什么：掉信号后这里只做了 resetInputCaptureTimer() + 清 dma_buffer，
                 *   而这【不足以重新武装输入捕获的 DMA】—— 实测 DMA1_CH1->CNDTR 会停在 0
                 *   （传输计数已用完），从此再也收不到任何 DShot 帧 ⇒ ESC 变成"聋子"：
                 *   现象就是"怎么都解锁不了、只能复位电驱板""必须断电重上电才能再收到信号"。
                 *   上游正是靠这次 NVIC_SystemReset() 把输入捕获 + DMA 完整重新初始化。
                 *   （曾为"保住调试器预置的 RAM 参数"把它改成不复位 —— 那是错的、也是本次
                 *     一系列怪现象的根因；现在参数都写进固件默认值了，复位不再有害。） */
                NVIC_SystemReset();
            }
            if (signaltimeout > LOOP_FREQUENCY_HZ << 1) { // 2 second when not armed
                allOff();
                armed = 0;
                input = 0;
                inputSet = 0;
                zero_input_count = 0;
                SET_DUTY_CYCLE_ALL(0);
                resetInputCaptureTimer();
                for (int i = 0; i < 64; i++) {
                    dma_buffer[i] = 0;
                }
                /* 同上：这里也必须复位（未解锁状态下信号丢失超过 2 秒）。
                 * 原理与上面 0.5 秒分支完全一样：不复位 ⇒ 输入捕获 DMA 停在 CNDTR=0，
                 * 之后永远收不到 DShot ⇒ "只能复位电驱板才能解锁"。 */
                NVIC_SystemReset();
            }
        }
#if RM2027_OL_ARM_ONLY
        /* 起步/停机请求的灯色，在这里真正执行（仅开环模式用；闭环走 AM32 原生灯效）。
         * 为什么不在 olStepTick() 里直接调：WS2812 是阻塞位操作（约 30µs），
         * 而 olStepTick 跑在 20kHz（周期 50µs）的 TIM6 中断里，会打乱时序。 */
        if (ol_led_on_req) {
            ol_led_on_req = 0;
#ifdef USE_LED_STRIP
            send_LED_RGB(0, 255, 0); /* 绿 = 正在驱动（与解锁序列里那次绿灯同一约定） */
#endif
        }
        if (ol_led_off_req) {
            ol_led_off_req = 0;
#ifdef USE_LED_STRIP
            send_LED_RGB(125, 0, 0); /* 暗红 = 已停机/上锁待命（约定同开机指示） */
#endif
        }
#endif
#ifdef USE_CUSTOM_LED
        if ((input >= 47) && (input < 1947)) {
            if (ledcounter > (2000 >> forward)) {
                GPIOB->BSRR = LL_GPIO_PIN_3;
            } else {
                GPIOB->BRR = LL_GPIO_PIN_3;
            }
            if (ledcounter > (4000 >> forward)) {
                ledcounter = 0;
            }
        }
        if (input > 1947) {
            GPIOB->BSRR = LL_GPIO_PIN_3;
        }
        if (input < 47) {
            GPIOB->BRR = LL_GPIO_PIN_3;
        }
#endif

        if (tenkhzcounter > LOOP_FREQUENCY_HZ) { // 1s sample interval 10000
            consumed_current += (actual_current << 16) / 360;
            tenkhzcounter = 0;
            if(brake_countdown > 0){
              brake_countdown--;
            }
        }

#ifndef BRUSHED_MODE

        if ((zero_crosses > 1000) || (adjusted_input == 0)) {
            bemf_timeout_happened = 0;
        }
        if (zero_crosses > 100 && adjusted_input < 200) {
            bemf_timeout_happened = 0;
        }
        if (eepromBuffer.use_sine_start && adjusted_input < 160) {
            bemf_timeout_happened = 0;
        }

        if (crawler_mode) {
            if (adjusted_input < 400) {
                bemf_timeout_happened = 0;
            }
        } else {
            if (adjusted_input < 150) { // startup duty cycle should be low enough to not burn motor
                bemf_timeout = 100;
            } else {
                bemf_timeout = 10;
            }
        }
#endif
        average_interval = e_com_time / 3;
        if (!RM2027_OL_ARM_ONLY && desync_check && zero_crosses > 10) {
            /* ★ 失步判据必须只在"换相间隔【变长】"时才成立（2026-10-07 实测修正）。
             *   上游用的是 |Δ| > 50%（两个方向都算）。但本板闭环起步的加速斜坡，靠的就是
             *   换相间隔【连续大幅缩短】—— PeriodElapsedCallback 的
             *       ci = (ci + ((lastzctime+thiszctime)>>1)) >> 1
             *   本来就是"每转把 ci 往过零时刻拉近一半" ⇒ 健康加速天然满足 |Δ| > 50%
             *   ⇒ 被误判成失步 ⇒ running=0 复位 ⇒ 电机"冲到 2272eRPM 又被拽回去重起"
             *   ⇒ 现象就是【在转、但很抖、永远跑不稳】。
             *   实测：duty≈290、e_rpm=2272 处被复位，掉到 38 后重新爬（2026-10-07）。
             *   物理上真正的失步 = 转子跟不上换相 ⇒ 换相间隔变长 ⇒ 只认这个方向即可。
             *   （这样也顺带排掉"首次跨越 average_interval<2000 那条界"时的假触发。） */
            if ((average_interval > last_average_interval) &&
                (getAbsDif(last_average_interval, average_interval) > (average_interval >> 1)) &&
                (average_interval < 2000)) { // throttle resitricted before zc 20.
#if defined(USE_ADC_BEMF)
                /* ★★★ 2026-10-09 关键修复⑤：本板绝不能在这条分支里退回轮询态 ★★★
                 *   实测证据（12V 台架，停机瞬间读 RAM）：
                 *     old_routine = 1、bemf_irq_armed = 0、bemf_to 以 ~25 次/秒 上涨
                 *   机理：一旦 old_routine=1，PeriodElapsedCallback() 里那句
                 *     `if (!old_routine) enableCompInterrupts();`
                 *   就不再执行 ⇒ 过零判据【永远不再武装】⇒ 再也认不到过零
                 *   ⇒ 只剩 22.5ms 兜底硬踹。现场表现是"闭环每约 11 步就断一次、
                 *   大部分时间靠硬踹维持"⇒ 转得慢。
                 *   为什么这条分支会【被频繁误触发】：判据是
                 *     `average_interval 变大 且 |Δ|>50%`，
                 *   而闭环从 ci=1500 收敛到真实步长（实测 ~2500 拍）的过程中
                 *   average_interval 本来就会上下跳 ⇒ 每次跳都被判成"失步"。
                 *   另外 `commutation_interval > 1000`（>500µs/步 = "慢"）在收敛期
                 *   也天然成立 ⇒ 会把 running 清零、把电机停掉。
                 *   上游这套语义是给"有比较器、能退回轮询起步"的板子用的；
                 *   本板三相里只有 C 相能进比较器（见 Inc/targets.h 的 USE_ADC_BEMF
                 *   注释），轮询起步不可靠 ⇒ 必须留在中断闭环。
                 *   本板的真实故障保护仍在别处：起步链过流跳闸、bemf_timeout 兜底、
                 *   stuck_rotor_protection、以及 F1 台架侧的限时运行。
                 *   ⇒ 这里只记数、并把过零判据重新武装好，不改变运行状态。 */
                zero_crosses = 0;
                desync_happened++;
                enableCompInterrupts();
#if !RM2027_NO_DESYNC_DUTYCUT
                /* ★ 2026-10-13 可切开关：这一句是"打顿"的疑似执行机构 ——
                 *   本板这条分支【不回滚 running】（见上面注释，那是关键修复⑤），
                 *   只把应用的占空比砍到"最小起步占空比的一半"一拍。而这条判据自己在
                 *   注释里承认会误触发（"ci 收敛期 average_interval 本来就会上下跳 ⇒
                 *   每次跳都被判成失步"）⇒ 一次误判 = 一次转矩掉一下 = 用户手上的"卡顿"。
                 *   RM2027_NO_DESYNC_DUTYCUT=1（默认）⇒ 只计数 + 重新武装，不动占空比。 */
                last_duty_cycle = min_startup_duty / 2;
#endif
#else
                zero_crosses = 0;
                desync_happened++;
                if ((!eepromBuffer.bi_direction && (input > 47)) || commutation_interval > 1000) {
                    running = 0;
                }
                old_routine = 1;
                if (zero_crosses > 100) {
                    average_interval = 5000;
                }
                last_duty_cycle = min_startup_duty / 2;
#endif
            }
            desync_check = 0;
            //	}
            last_average_interval = average_interval;
        }

#if !defined(MCU_G031) && !defined(NEED_INPUT_READY)
#ifdef NXP
	if (dshot_telemetry && (commutation_interval > DSHOT_PRIORITY_THRESHOLD)) {
		NVIC_SetPriority(IC_DMA_IRQ_NAME, 0);
		NVIC_SetPriority(COM_TIMER_IRQ, 1);
		NVIC_SetPriority(COMP0_IRQ, 1);
		NVIC_SetPriority(COMP1_IRQ, 1);
	} else {
		NVIC_SetPriority(IC_DMA_IRQ_NAME, 1);
		NVIC_SetPriority(COM_TIMER_IRQ, 0);
		NVIC_SetPriority(COMP0_IRQ, 0);
		NVIC_SetPriority(COMP1_IRQ, 0);
	}
#else
        if (dshot_telemetry && (commutation_interval > DSHOT_PRIORITY_THRESHOLD)) {
             NVIC_SetPriority(IC_DMA_IRQ_NAME, 0);
             NVIC_SetPriority(COM_TIMER_IRQ, 1);
             NVIC_SetPriority(COMPARATOR_IRQ, 1);
         } else {
             NVIC_SetPriority(IC_DMA_IRQ_NAME, 1);
             NVIC_SetPriority(COM_TIMER_IRQ, 0);
             NVIC_SetPriority(COMPARATOR_IRQ, 0);
         }
#endif
#endif
#if ADC_CAPTURE_MODE
        send_telemetry = 0;      /* ★ 捕获模式：停发 KISS 遥测，串口专供 ADC 原始流 */
#endif
        if (send_telemetry) {
#ifdef USE_SERIAL_TELEMETRY
            makeTelemPackage((int8_t)degrees_celsius, battery_voltage, actual_current,
                (uint16_t)(consumed_current >> 16), e_rpm);
            send_telem_DMA(10);
            send_telemetry = 0;
#endif
        } else if(send_esc_info_flag ) {
           makeInfoPacket();
           send_telem_DMA(49);
           send_esc_info_flag = 0;
        }
        if (PROCESS_ADC_FLAG == 1) { // for adc and telemetry set adc counter at 1khz loop rate
#ifndef USE_ADC_BEMF
          /* 非 ADC-BEMF 的板子：本循环自己"先取上一轮结果、再踢一脚"软件触发。
           * ADC-BEMF 板子这两件事都不做 —— 结果由 DMA 完成中断在每个 PWM 周期搬一次，
           * 触发由 TIM1_CH4 硬件给（见 ADC.c）。这里再搬一次反而可能读到"搬一半"的数组
           * （四路 BEMF 必须来自同一次扫描）。 */
          ADC_DMA_Callback(); // common to all, Call ADC_DMA callback to get raw data
#endif
#ifdef NO_CURRENT_SENSE
          ADC_raw_current = 0;
#endif
#ifdef NO_VOLTAGE_SENSE
          ADC_raw_volts = 0;
#endif          
#if defined(STMICRO)
#ifndef USE_ADC_BEMF
            LL_ADC_REG_StartConversion(ADC1);
#endif
#ifdef USE_ADC_1_2
          LL_ADC_REG_StartConversion(ADC2);
#endif          
            converted_degrees = __LL_ADC_CALC_TEMPERATURE(3300, ADC_raw_temp, LL_ADC_RESOLUTION_12B);
#endif
#ifdef MCU_GDE23
            // converted_degrees = (1.43 - ADC_raw_temp * 3.3 / 4096) * 1000 / 4.3 + 25;
            converted_degrees = ((int32_t)(357.5581395348837f * (1 << 16)) - ADC_raw_temp * (int32_t)(0.18736373546511628f * (1 << 16))) >> 16;
            adc_software_trigger_enable(ADC_REGULAR_CHANNEL);
#endif
#ifdef ARTERY            
            adc_ordinary_software_trigger_enable(ADC1, TRUE);
    #ifdef USE_NTC
            converted_degrees = getNTCDegrees(ADC_raw_ntc);
    #else     
            converted_degrees = getConvertedDegrees(ADC_raw_temp);
    #endif
#endif
#ifdef NXP
            //Convert temperature data to actual temperature in degrees Celsius
            converted_degrees = computeTemperature(ADC_raw_temp[0], ADC_raw_temp[1]);
            //Start ADC conversion
            startADCConversion();
#endif
#ifdef WCH
            startADCConversion( );
            converted_degrees = getConvertedDegrees(ADC_raw_temp);
#endif
            degrees_celsius = converted_degrees;
#ifdef NXP
            //MCXA has 16-bit ADC data
            battery_voltage = ((7 * battery_voltage) + ((ADC_raw_volts * 3300 / 65535 * VOLTAGE_DIVIDER) / 100)) / 8;
            smoothed_raw_current = getSmoothedCurrent();
            //Actual current is in 10mA, so 1 = 10mA
            actual_current = (((smoothed_raw_current * 3300 / 65535) - CURRENT_OFFSET) * 100) / (MILLIVOLT_PER_AMP);
#else
            battery_voltage = ((7 * battery_voltage) + ((ADC_raw_volts * 3300 / 4095 * VOLTAGE_DIVIDER) / 100)) >> 3;
            smoothed_raw_current = getSmoothedCurrent();
            actual_current = ((smoothed_raw_current * 3300 / 41) - (CURRENT_OFFSET * 100)) / (MILLIVOLT_PER_AMP);
#endif
            if (actual_current < 0) {
                actual_current = 0;
            }
            if (eepromBuffer.low_voltage_cut_off == 1) {  
                if (battery_voltage < (cell_count * low_cell_volt_cutoff)) {
                  low_voltage_count++;
                } else {
                  if(!LOW_VOLTAGE_CUTOFF){  // if set low cutoff has happened, require power cycle to reset
                    low_voltage_count = 0;
                  }
                }
            }
            if (eepromBuffer.low_voltage_cut_off == 2 ){   // absolute cut off
              if (battery_voltage <  (eepromBuffer.absolute_voltage_cutoff * 50)) {
                low_voltage_count++;    
                } else {
                  if(!LOW_VOLTAGE_CUTOFF){
                    low_voltage_count = 0;
                  }
                }
            }
            if (low_voltage_count > (10000 - (stepper_sine * 9900))) {      // 10 second wait before cut-off for low voltage
              LOW_VOLTAGE_CUTOFF = 1;
              input = 0;
              allOff();
              maskPhaseInterrupts();
              running = 0;
                ol_stop_site = 7;            /* 停机站点：低压截止 */
              zero_input_count = 0;
              armed = 0;
             }
           
            PROCESS_ADC_FLAG = 0;
#ifdef USE_ADC_INPUT
            if (ADC_raw_input < 10) {
                zero_input_count++;
            } else {
                zero_input_count = 0;
            }
#endif
        }
#ifdef USE_ADC_INPUT
        signaltimeout = 0;
        ADC_smoothed_input = (((10 * ADC_smoothed_input) + ADC_raw_input) / 11);
        newinput = ADC_smoothed_input / 2;
        if (newinput > 2000) {
            newinput = 2000;
        }
        input_ready = 1;       
#endif
        stuckcounter = 0;
        if (stepper_sine == 0) {

            e_rpm = running * (600000 / e_com_time); // in tens of rpm
            k_erpm = e_rpm / 10; // ecom time is time for one electrical revolution in microseconds

            if (low_rpm_throttle_limit) { // some hardware doesn't need this, its on
                                          // by default to keep hardware / motors
                                          // protected but can slow down the response
                                          // in the very low end a little.
                duty_cycle_maximum = map(k_erpm, low_rpm_level, high_rpm_level, throttle_max_at_low_rpm,
                    throttle_max_at_high_rpm); // for more performance lower the
                                               // high_rpm_level, set to a
                                               // consvervative number in source.
            }else{
							duty_cycle_maximum = 2000;
						}

            if (degrees_celsius > eepromBuffer.limits.temperature) {
              duty_cycle_maximum = map(degrees_celsius, eepromBuffer.limits.temperature - 10, eepromBuffer.limits.temperature + 10,
                throttle_max_at_high_rpm / 2, 1);
            }
            if (zero_crosses < 100 && commutation_interval > 500) {
              filter_level = 12;
            } else {
              filter_level = map(average_interval, 100, 500, 3, 12);
            }
            if (commutation_interval < 50) {
              filter_level = 2;
            }

            if (eepromBuffer.auto_advance) {
              auto_advance_level = map(duty_cycle, 100, 2000, 13, 23);
            }

            /**************** old routine*********************/
#ifdef CUSTOM_RAMP
            if (old_routine && running) {
                maskPhaseInterrupts();
                getBemfState();
                if (!zcfound) {
                    if (rising) {
                        if (bemfcounter > min_bemf_counts_up) {
                            zcfound = 1;
                            zcfoundroutine();
                        }
                    } else {
                        if (bemfcounter > min_bemf_counts_down) {
                            zcfound = 1;
                            zcfoundroutine();
                        }
                    }
                }
            }
#endif
            /* ★ 起步链（olStartupTick）期间把这条 22.5ms 超时兜底让开：
             *   起步链本身在按固定时基换相，兜底再来"硬踹"会打乱它的节奏；
             *   而兜底原本要解决的就是"没人换相"，起步链在跑时这个问题不存在。 */
            /* ★★ 2026-10-11 关键：兜底阈值从【固定 22.5ms】改成【1.5 倍当前步长】★★
             *   实测（交棒前后的原始 delta 波形，zcw 捕获）：闭环正常时是 1ms/步
             *   （和开环一样快），但每走 4~8ms 就【卡住约 18ms】——step 冻结、
             *   delta 平在 +50 不振荡（转子几乎没转）⇒ 这段没有穿零、判据无过零可认
             *   ⇒ 只能等满 45000 拍（22.5ms）被硬踹出来。平均下来约 3ms/步，
             *   听到的就是"抖 + 慢"。
             *   而开环那边的同一机制早就改成"随斜坡变化的期望间隔"了
             *   （见本文件 2539-2542 的注释），只有闭环这边还留着固定的 45000。
             *   ⇒ 对齐成 1.5xci：认不到过零时最多等到 1.5 步就强制换一次，
             *     卡顿从 18ms 缩到约 3.75ms；认到过零时照旧用真过零定相位。
             *   注意：这里必须仍走 commutate() 而不是 zcfoundroutine()
             *   （后者会把这个"超时时刻"当成过零时刻写进 thiszctime ⇒ 污染 ci）。 */
#if RM2027_ZCL_FILTER
            /* ★ 2026-10-14 丢过零滤波：阈值从"死等 1.5×ci"改成 "ci + zcl_pred + ci>>5"。
             *   zcl_pred = 正常换相后记下的本步 waitTime（虚拟过零→换相的预计延迟）、
             *   超时换相后清 0 ⇒ 触发点 = 本该换相的时刻 ⇒ 丢过零那一步的换相间隔 ≈ ci
             *   （原节拍），不再被拉长到 1.05~1.45×ci。余量 ci>>5 防抢在正常过零前触发。 */
            if (!RM2027_OL_ARM_ONLY && !OL_STARTUP_ACTIVE() &&
                INTERVAL_TIMER_COUNT > ((uint32_t)commutation_interval + (uint32_t)zcl_pred
                                        + ((uint32_t)commutation_interval >> RM2027_ZCL_MARGIN_SHIFT))) {
#else
            if (!RM2027_OL_ARM_ONLY && !OL_STARTUP_ACTIVE() &&
                INTERVAL_TIMER_COUNT > (commutation_interval + (commutation_interval >> 1))) {
#endif
              zero_throttle_brake_active = 0;   // reset zero throttle brake on back emf timeout (rotation stop)
              if(running){
                bemf_timeout_happened++;
                
                temp_comp_pwm = eepromBuffer.comp_pwm;
                maskPhaseInterrupts();
#if RM2027_OL_ARM_ONLY
                old_routine = 1;   /* 开环：本分支不参与换相（换相由 olStepTick 独占） */
#else
                /* ★★ 闭环（本板 = ADC 中断闭环）绝不能退回轮询态、也不能把过零中断留在屏蔽态。
                 *   为什么：maskPhaseInterrupts() 之后，唯一会重新武装过零判据的地方是
                 *   PeriodElapsedCallback()（main.c 里 `if (!old_routine) enableCompInterrupts();`），
                 *   而那个函数要靠 interruptRoutine()（= 认到过零）才能被调用 ⇒ 死锁：
                 *     屏蔽 ⇒ 认不到过零 ⇒ 没人重新武装 ⇒ 只能再等下一次超时硬踹一次。
                 *   实测（2026-10-07）：old_routine 恒 1、bemf_irq_armed 恒 0、
                 *   bemf_timeout_happened 以 ~20 次/秒 稳定上涨 ⇒ 换相完全由本分支硬踹驱动
                 *   ⇒ 20 步/秒（约 33 RPM）"缓慢蹭着转 + 一直抖"、母线约 1 A、e_rpm 恒 8。
                 *   上游这里写 old_routine=1 是给"有比较器、能退回轮询"的板子留的语义；
                 *   本板三相里只有 C 相能进比较器（见 Inc/targets.h 的 USE_ADC_BEMF 注释），
                 *   轮询起步不可靠 ⇒ 必须留在中断模式。 */
                old_routine = 0;
#endif
                if (input < 48) {
                    running = 0;
                    commutation_interval = 5000;
                }
                zero_crosses = 0;
#if RM2027_OL_ARM_ONLY
                zcfoundroutine();
#else
                /* ★★ 闭环：这里只做"强制换相一次"，【不能】走 zcfoundroutine()。
                 *   为什么：zcfoundroutine() 会写 `thiszctime = INTERVAL_TIMER_COUNT`
                 *   （本次超时时刻 ≈45000），而这个【假的过零时刻】随后被
                 *   interruptRoutine() 复制进 lastzctime，再被 PeriodElapsedCallback 的
                 *      ci = (ci + ((lastzctime + thiszctime) >> 1)) >> 1
                 *   平滑进 commutation_interval ⇒ ci 被不断【拉大】⇒ 加速斜坡起不来
                 *   ⇒ 只能继续靠本分支每 20~50ms 硬踹一次（自锁）。
                 *   实测（2026-10-07）：ci 在 21000~37000 徘徊不降、e_rpm 恒 6~8、
                 *   bemf_timeout_happened 以 ~20 次/秒 稳定上涨；而 lastzctime 与
                 *   thiszctime 的配对（如 45005/18643）证明过零本身是认到了的
                 *   （interruptRoutine 确实被调用）⇒ 卡的就是这里的时间污染。 */
                /* 诊断：超时硬踹现场（见 tm_* 变量定义处）*/
                if (tm_idx < TM_N) {
                    tm_duty[tm_idx] = duty_cycle;
                    tm_cur[tm_idx]  = (uint16_t)actual_current;
                    tm_cnt[tm_idx]  = (uint16_t)INTERVAL_TIMER_COUNT;
                    tm_idx++;
                }
                tm_total++;
                commutate();
                SET_INTERVAL_TIMER_COUNT(0);
#if RM2027_ZCL_FILTER
                /* ★ 2026-10-14 丢过零滤波：连续丢失计数 + 限速降频 + 基准复位。
                 *   丢 1~2 个：只计数（惯性按原节拍走，不降频）；
                 *   连续丢 > RM2027_ZCL_SLOW_AFTER 个（即第 3 个起）：每步把周期拉长
                 *     ci>>RM2027_ZCL_SLOW_SHIFT（≈1.56%/步，降速限幅），等重新捕获过零；
                 *   真过零一被认到（interruptRoutine 里 zcl_miss 清 0）即恢复。
                 *   zcl_pred = 0：基准从"虚拟过零"变成"换相点本身"⇒ 下一步若还丢，
                 *   阈值 = ci + 0 + 余量 ⇒ 换相间隔≈ci，节拍保持原频率附近。 */
                if (zcl_miss < 255u) { zcl_miss++; }
                if (zcl_miss > zcl_miss_max) { zcl_miss_max = zcl_miss; }
                if (zcl_miss > RM2027_ZCL_SLOW_AFTER) {
                    uint32_t zcl_inc = (uint32_t)commutation_interval >> RM2027_ZCL_SLOW_SHIFT;
                    if (zcl_inc < 1u) { zcl_inc = 1u; }
                    commutation_interval += zcl_inc;
                    if (zcl_slow_n < 60000u) { zcl_slow_n++; }
                } else {
                    if (zcl_hold_n < 60000u) { zcl_hold_n++; }
                }
                zcl_pred = 0;
#endif
#endif
#if !RM2027_OL_ARM_ONLY
                /* 硬踹完立刻把过零判据交还给中断闭环，否则下一次过零照样认不出来。
                 * 安全：zcfoundroutine() 开头刚做过 SET_INTERVAL_TIMER_COUNT(0)
                 * （见其函数体），而中断端的门槛是 INTERVAL_TIMER->CNT > average_interval/2
                 * ⇒ 刚踹完不会立刻误认过零。 */
                enableCompInterrupts();
#endif
              }
            }
        } else { // stepper sine

#ifdef GIMBAL_MODE
            step_delay = 300;
            maskPhaseInterrupts();
            allpwm();
            if (newinput > 1000) {
                desired_angle = map(newinput, 1000, 2000, 180, 360);
            } else {
                desired_angle = map(newinput, 0, 1000, 0, 180);
            }
            if (current_angle > desired_angle) {
                forward = 1;
                advanceincrement();
                delayMicros(step_delay);
                current_angle--;
            }
            if (current_angle < desired_angle) {
                forward = 0;
                advanceincrement();
                delayMicros(step_delay);
                current_angle++;
            }
#else

            if (input > 48 && armed) {
                PROCESS_ADC_FLAG = 1;
                if (input > 48 && input < 137) { // sine wave stepper

                    if (do_once_sinemode) {
                        // disable commutation interrupt in case set
                        DISABLE_COM_TIMER_INT();
                        maskPhaseInterrupts();
                        SET_DUTY_CYCLE_ALL(0);
                        allpwm();
                        do_once_sinemode = 0;
                    }
                    advanceincrement();
                    step_delay = map(input, 48, 120, 7000 / eepromBuffer.motor_poles, 810 / eepromBuffer.motor_poles);
                    delayMicros(step_delay);
                    e_rpm = 600 / step_delay; // in hundreds so 33 e_rpm is 3300 actual erpm
                    e_com_time = step_delay * 360; 

                } else {
                    do_once_sinemode = 1;
                    advanceincrement();
                    if (input > 200) {
                        phase_A_position = 0;
                        step_delay = 80;
                    }

                    delayMicros(step_delay);
                    if (phase_A_position == 0) {
                        stepper_sine = 0;
                        running = 1;
                        old_routine = 1;
                        commutation_interval = 9000;
                        average_interval = 9000;
                        last_average_interval = average_interval;
                        SET_INTERVAL_TIMER_COUNT(9000);
                        zero_crosses = 20;
                        prop_brake_active = 0;
                        step = changeover_step;
                        // comStep(step);// rising bemf on a same as position 0.
                        if (eepromBuffer.stall_protection) {
                            last_duty_cycle = stall_protect_minimum_duty;
                        }
                        commutate();
                        generatePwmTimerEvent();
                    }
                }

            } else {
                running = 0;
                do_once_sinemode = 1;
                if (eepromBuffer.brake_on_stop == 1) {
#ifndef PWM_ENABLE_BRIDGE
                    prop_brake_duty_cycle =  eepromBuffer.drag_brake_strength * 200;
                    adjusted_duty_cycle =  tim1_arr - ((prop_brake_duty_cycle * tim1_arr) / 2000);
                    if(adjusted_duty_cycle < 100){
                      fullBrake();
                    }else{
                      proportionalBrake();
                      SET_DUTY_CYCLE_ALL(adjusted_duty_cycle);
                      prop_brake_active = 1;
                    } 
#else
                    // todo add braking for PWM /enable style bridges.
#endif
                } else if (eepromBuffer.brake_on_stop == 2){
                  comStep(2);
                  SET_DUTY_CYCLE_ALL(DEAD_TIME + ((eepromBuffer.active_brake_power * tim1_arr) / 2000)* 10);
                }else{
                   SET_DUTY_CYCLE_ALL(0);
                   allOff();
                }
                e_rpm = 0;
            }

#endif // gimbal mode
        } // stepper/sine mode end

#ifdef BRUSHED_MODE
        runBrushedLoop();
#endif
#if DRONECAN_SUPPORT
	DroneCAN_update();
#endif
    }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t* file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line
       number, tex: printf("Wrong parameters value: file %s on line %d\r\n", file,
       line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
