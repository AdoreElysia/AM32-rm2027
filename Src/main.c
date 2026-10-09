
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
#endif
#include "dshot.h"
#include "eeprom.h"
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
fastPID speedPid = { // commutation speed loop time
    .Kp = 10,
    .Ki = 0,
    .Kd = 100,
    .integral_limit = 10000,
    .output_limit = 50000
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
uint16_t ol_trip_count = 0;                 /* 观测：过流连续超限计数 */
uint32_t ol_trips = 0;                      /* 观测：累计过流跳闸次数 */
uint32_t ol_iphase_last = 0;                /* 观测：最近反推出来的相电流（厘安）*/
uint32_t ol_run_ticks = 0;                  /* 本次已驱动的拍数 */
uint32_t ol_timeouts = 0;                   /* 观测：到时间上限自动停机的次数 */
volatile uint8_t ol_led_off_req = 0;        /* 请求切"待命红"（WS2812 阻塞操作，交主循环执行）*/
volatile uint8_t ol_led_on_req = 0;         /* 请求切"运行绿"：每次起步点一次（见 olStepTick）——
                                             * 否则第一轮跑完变红后，之后每轮都会一直亮红灯 */
volatile uint8_t ol_stop_reason = 0;        /* 0 无 / 1 到时间上限 / 2 过流跳闸 */
uint32_t ol_cool_ticks = 0;                 /* 冷却已过的拍数 */
volatile uint8_t ol_startup_arm = 0;        /* 1 = 闭环开环起步链在跑（ISR 也要读，故无条件定义）*/
/* ★ 台架过流切断用的计数（见 tenKhzRoutine 里的说明与 targets.h 的 RM2027_BENCH_OC_*） */
uint16_t ol_bench_oc_count = 0;
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

/* ★ 2026-10-09：本步内"|悬空相 − V0|"的最大值（每帧更新、每次过零清零）。
 *   用途：一眼看出反电势真实幅值 ⇒ 直接决定施密特滞回 BEMF_HYST_LSB 该取多少。
 *   实测它和"接受时 delta 只有 ±5~±11"配合，才能判定"信号在阈值上蹭"这件事。 */
volatile int16_t  bemf_dmax = 0;

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
#define OL_STARTUP_ZC_NEED 3    /* 认到 3 次过零就交棒给闭环 */
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

/* 由主循环每轮调用（与 olStepTick 同层）。返回时若已交棒，ol_startup_arm 已清 0。 */
static void olStartupTick(void)
{
    if (!OL_STARTUP_ACTIVE() || !running) {
        return;
    }
    /* ★ 收油门即停（2026-10-08 补）：原来用被覆盖过的 input 判断 ⇒ 起步期间
     *   油门回 0 也停不下来、一路跑到超时。改用原始油门 newinput。 */
    if (newinput < 48) {
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
    if (++ol_startup_ticks > 160000UL) {
        allOff();
        running = 0;
        ol_startup_arm = 0;
        return;
    }

    /* 期望间隔换算成 0.5µs 单位（TIM2 的拍），既当时间基准又给遥测算 eRPM */
    commutation_interval = (uint32_t)ol_step_div_cur * 100;

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
            if (ol_step_div_cur > OL_STEP_DIV) {
                uint16_t dec = (uint16_t)(ol_step_div_cur >> OL_RAMP_SHIFT);
                if (dec == 0) {
                    dec = 1;
                }
                if ((uint32_t)ol_step_div_cur > (uint32_t)OL_STEP_DIV + dec) {
                    ol_step_div_cur -= dec;
                } else {
                    ol_step_div_cur = OL_STEP_DIV;
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
        if (ol_step_div_cur <= OL_STEP_DIV) {
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
            uint8_t inwin = (cnt > (commutation_interval >> 2))
                         && (cnt < (commutation_interval - (commutation_interval >> 2)));
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
     * 条件：斜坡已跑到底（ol_step_div_cur <= OL_STEP_DIV ⇒ 转速就是交棒转速）
     *       且认到 OL_STARTUP_ZC_NEED 次过零（说明反电势确实出来了、转子在跟着转）。
     * 为什么必须"斜坡跑到底"：只认过零不看斜坡 ⇒ 起步早期（div 还很大、转子几乎没转）
     *   会被噪声骗去交棒，实测只走 3 步就交棒、之后闭环不换相、steps 冻住。 */
    if (ol_step_div_cur <= OL_STEP_DIV && ol_startup_zc >= OL_STARTUP_ZC_NEED) {
        ol_startup_arm = 0;          /* 退出开环：此后换相交给过零中断闭环 */
        old_routine = 0;
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
        commutation_interval = (uint32_t)ol_step_div_cur * 100;
        thiszctime = (uint16_t)commutation_interval;
        lastzctime = thiszctime;
        if (!eepromBuffer.auto_advance) {
            advance = (uint16_t)((commutation_interval * temp_advance) >> 6);
        } else {
            advance = (uint16_t)((commutation_interval * auto_advance_level) >> 6);
        }
        waitTime = (uint16_t)((commutation_interval >> 1) - advance);
        /* ★ 定时器不要清零：交棒点落在一个步长的【中途】，清 0 会让第一次过零
         *   只量到"半个步长"、同样污染 commutation_interval。摆到半步处，
         *   使得到第一次过零时量到的正好是整步。 */
        SET_INTERVAL_TIMER_COUNT(commutation_interval >> 1);
        /* ★ 交棒后第一帧也屏蔽掉：交棒点落在一个步长的中途，这一帧的 delta 同样
         *   不能代表"新的悬空相"（悬空相还没换，但定时基准刚被重置），
         *   顺手丢弃它的边沿，避免又给正反馈送第一脚。 */
        bemf_blank = 1;
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
    commutation_intervals[step - 1] = commutation_interval; // just used to calulate average

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
        uint32_t ci_new = ((commutation_interval) + ((lastzctime + thiszctime) >> 1)) >> 1;
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
        commutation_interval = ci_new;
    }
  	if (!eepromBuffer.auto_advance) {
	  advance = (commutation_interval * temp_advance) >> 6; // 60 divde 64 0.9375 degree increments
	} else {
	  advance = (commutation_interval * auto_advance_level) >> 6; // 60 divde 64 0.9375 degree increments
    }
    waitTime = (commutation_interval >> 1) - advance;
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
        ol_step_div_cur  = (OL_STEP_DIV_START > OL_STEP_DIV) ? OL_STEP_DIV_START : OL_STEP_DIV;
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
            if (running == 0) {
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
{ // 20khz as of 2.00 to be renamed
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
            ol_startup_arm = 0;
            ol_bench_oc_count = 0;
            ol_bench_oc_trips++;
        }
    } else {
        ol_bench_oc_count = 0;
    }
#endif
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
            if (use_speed_control_loop && running) {
                input_override += doPidCalculations(&speedPid, e_com_time, target_e_com_time);
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
                running = 0;                               // stops tracking rpm
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
        SET_DUTY_CYCLE_ALL(adjusted_duty_cycle);
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
    signaltimeout++;

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
    startup_max_duty_cycle = 400;
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
                last_duty_cycle = min_startup_duty / 2;
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
            if (!RM2027_OL_ARM_ONLY && !OL_STARTUP_ACTIVE() && INTERVAL_TIMER_COUNT > 45000) {
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
                commutate();
                SET_INTERVAL_TIMER_COUNT(0);
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
