#!/bin/bash
# bench_run.sh —— 电调台架联调一次的完整流程（在线调试用）
#
#   ① 清零黑匣子（计数器 + 环形缓冲头）
#   ② 复位 F1（J-Link）—— 重开它开头那段"0 油门"窗口
#   ③ 紧接着复位电调（ST-Link）—— 让它在 0 油门窗口里解锁（绿灯）
#   ④ 等 N 秒（默认 8s：F1 在 5s 后爬到油门 400）
#   ⑤ 读一遍电调状态（armed/running/input…，地址从 ELF 现取，不写死）
#   ⑥ 冻结黑匣子并解码分析
#
# 用法（ST-Link 设备节点属 root，所以要 sudo）：
#     sudo ./tools/bench_run.sh          # 等 8 秒
#     sudo ./tools/bench_run.sh 12       # 等 12 秒
#
# ⚠️ 跑完**不要给电调断电**（RAM 断电就没了）；脚本自己会 --freeze 再读。
set -e
cd "$(dirname "$0")/.."

SECS="${1:-8}"
ELF=obj/AM32_RM2027_ESC_G071_2.21.elf
NM=arm-none-eabi-nm

[ -f "$ELF" ] || { echo "❌ 找不到 $ELF（先 make RM2027_ESC_G071）"; exit 1; }

# ---- 从 ELF 取符号地址（写死地址会在改动固件后失效，这里每次现取）----
sym() { $NM "$ELF" | awk -v s="$1" '$3==s {print "0x"$1; exit}'; }
A_TRACE=$(sym g_trace)
A_ARMED=$(sym armed)
A_INPUTSET=$(sym inputSet)
A_RUNNING=$(sym running)
A_INPUT=$(sym input)
A_NEWINPUT=$(sym newinput)
A_OLDR=$(sym old_routine)
A_ZC=$(sym zero_crosses)
[ -n "$A_TRACE" ] && [ -n "$A_ARMED" ] || { echo "❌ 符号解析失败"; exit 1; }

# ---- ★ 开测前先确认母线电压（避免电源没开白跑一轮）----
A_VOLTS=$(sym ADC_raw_volts)
if [ -n "$A_VOLTS" ]; then
  VCODE=$(openocd -f interface/stlink.cfg -f target/stm32g0x.cfg -c "adapter speed 500" \
    -c "init" -c "mdh $A_VOLTS" -c "shutdown" 2>/dev/null | awk '/^0x/{print $2}')
  if [ -n "$VCODE" ]; then
    V=$((16#$VCODE))
    if [ "$V" -lt 600 ]; then
      printf "⚠️  母线电压只有 %.2f V（漏电基线）—— 电源没开或跳保护。\n" "$(python3 -c "print($V/112.8)")"
      printf "    请先把可调电源打开（8~12 V、限流 1~2 A）再测；本轮已中止。\n"
      exit 1
    fi
    printf "· 母线电压 %.2f V ✓\n" "$(python3 -c "print($V/112.8)")"
  fi
fi

echo "▶ ① 清零黑匣子 (g_trace @ $A_TRACE)"
# ⚠️ 全程**不 halt**：AM32 开了 IWDG，调试器一停住 CPU 看门狗就会复位芯片
#    （表现为 PC 跑到奇怪地址/回到 bootloader）。RAM 在运行时可以直接读写。
openocd -f interface/stlink.cfg -f target/stm32g0x.cfg -c "adapter speed 500" \
  -c "init" \
  -c "mww $A_TRACE 0" -c "mww $((A_TRACE+4)) 0" -c "mww $((A_TRACE+8)) 0" \
  -c "mww $((A_TRACE+12)) 0" -c "mww $((A_TRACE+16)) 0" -c "mww $((A_TRACE+20)) 0" \
  -c "mww $((A_TRACE+24)) 0" -c "mww $((A_TRACE+28)) 0" -c "mwh $((A_TRACE+40)) 0" \
  -c "mwb $((A_TRACE+39)) 1" -c "shutdown" >/dev/null 2>&1

echo "▶ ② 复位 F1 (J-Link)：重开 0 油门窗口"
openocd -f interface/jlink.cfg -c "transport select swd" -f target/stm32f1x.cfg \
  -c "adapter speed 1000" -c "init" -c "reset run" -c "shutdown" >/dev/null 2>&1
echo "   ✓"

echo "▶ ③ 复位电调 (ST-Link) 并等 ${SECS}s（不 halt，直接读 RAM）"
openocd -f interface/stlink.cfg -f target/stm32g0x.cfg -c "adapter speed 500" \
  -c "init" -c "reset run" -c "sleep $((SECS * 1000))" \
  -c "echo {armed(1=绿灯) inputSet running old_routine step zero_crosses:}" \
  -c "mdb $A_ARMED" -c "mdb $A_INPUTSET" -c "mdb $A_RUNNING" -c "mdb $A_OLDR" -c "mdb $A_STEP" -c "mdw $A_ZC 1" \
  -c "echo {input newinput:}" -c "mdh $A_INPUT" -c "mdh $A_NEWINPUT" \
  -c "shutdown" 2>&1 | grep -E "^\{|^0x" || true

# ★ ④ 软停车：把 F1 的目标油门写 0 —— 只改 RAM，不复位任何东西 ⇒ 黑匣子数据保留
#   （教训：先"锁板"再读会把数据抹掉 —— AM32 启动会清 .bss，复位一次黑匣子就空了）
F1ELF=../rm2027-drone/test_esc/build/Debug/test_esc.elf
if [ -f "$F1ELF" ]; then
  G=$(arm-none-eabi-nm "$F1ELF" | awk '$3=="g_dbg" {print $1}')
  A_F1TT=$(printf '0x%x' $((0x$G + 118)))   # app_debug_t 里 throttle_target 的偏移（sizeof=120，它在最后）
  echo "▶ ④ 软停车：把 F1 目标油门写 0（@ $A_F1TT，不复位）"
  openocd -f interface/jlink.cfg -c "transport select swd" -f target/stm32f1x.cfg \
    -c "adapter speed 1000" -c "init" -c "mwh $A_F1TT 0" -c "shutdown" >/dev/null 2>&1
  sleep 1
else
  echo "▶ ④ ⚠️ 找不到 F1 的 ELF（$F1ELF），跳过软停车 —— 请在读完后自己关电源"
fi

echo "▶ ⑤ 冻结并读取黑匣子"
python3 tools/dbg_trace_read.py --freeze

# ★ ⑥ 最后才锁板（这一步会复位电调 ⇒ 会清掉黑匣子，所以必须放在读之后）
echo "▶ ⑥ 锁电调板：停掉 F1 的 DShot → 电调失联自动解除解锁"
openocd -f interface/jlink.cfg -c "transport select swd" -f target/stm32f1x.cfg \
  -c "adapter speed 1000" -c "init" -c "halt" -c "shutdown" >/dev/null 2>&1
sleep 1
openocd -f interface/stlink.cfg -f target/stm32g0x.cfg -c "adapter speed 500" \
  -c "init" -c "reset run" -c "sleep 2500" \
  -c "echo {锁定校验 armed(应=0) inputSet(应=0) running(应=0):}" \
  -c "mdb $A_ARMED" -c "mdb $A_INPUTSET" -c "mdb $A_RUNNING" \
  -c "shutdown" 2>&1 | grep -E "^\{|^0x" || true
echo "   🔒 电驱板已锁定（未解锁、门极全关），电机不会转"

echo "▶ 完成。重点看上面【自动判读】与【换相间隔抖动】两节。"
echo "   （要再测一次：直接再跑本脚本；要手动解锁测：先补齐 F1 信号再复位电调。）"
