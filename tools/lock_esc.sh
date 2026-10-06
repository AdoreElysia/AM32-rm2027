#!/bin/bash
# lock_esc.sh —— 一键锁死电驱板（你自己随时可用，不必等 AI）
#
# 做法：停掉 F1 的 DShot（= 信号丢失）→ 复位电调 → 校验未解锁。
#   AM32 失联后会 allOff() 解除解锁、并保持锁定（Src/main.c:1998-2017）。
# 用法：sudo ./tools/lock_esc.sh
set -e
cd "$(dirname "$0")/.."

ELF=obj/AM32_RM2027_ESC_G071_2.21.elf
[ -f "$ELF" ] || { echo "❌ 找不到 $ELF"; exit 1; }
sym() { arm-none-eabi-nm "$ELF" | awk -v s="$1" '$3==s {print "0x"$1; exit}'; }
A_ARMED=$(sym armed); A_IS=$(sym inputSet); A_RUN=$(sym running)

echo "▶ 停 F1 的 DShot (J-Link halt)"
openocd -f interface/jlink.cfg -c "transport select swd" -f target/stm32f1x.cfg \
  -c "adapter speed 1000" -c "init" -c "halt" -c "shutdown" >/dev/null 2>&1 || true

echo "▶ 复位电调，让它以「无信号」状态干净启动 (ST-Link)"
openocd -f interface/stlink.cfg -f target/stm32g0x.cfg -c "adapter speed 500" \
  -c "init" -c "reset run" -c "sleep 2500" \
  -c "echo {校验 armed(应=0) inputSet(应=0) running(应=0):}" \
  -c "mdb $A_ARMED" -c "mdb $A_IS" -c "mdb $A_RUN" \
  -c "shutdown" 2>&1 | grep -E "^\{|^0x" || true

echo "🔒 电驱板已锁定：未解锁、门极全关（allOff），电机不会转。"
echo "   （电调会每 ~2 秒自我复位一次，这是无信号时的正常行为，LED 常红。）"
