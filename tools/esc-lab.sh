#!/bin/bash
# ============================================================================
# esc-lab.sh —— 电调实验【唯一入口】（菜单）
#
#   [1] 编译当前分支 → 固件改名为分支名 → 烧录
#   [2] 1 + 额外拓展任务（采集 / 调试）—— 与串口采集有关的全在这里
#
# ★ 为什么做成菜单：
#   tasks.json 只留【一个】任务调本脚本，以后不用每次新建/手敲同一堆命令。
#   分支不再手选：一律用【当前所在分支】，固件名就是分支名（和 fw-burn.sh 同一套约定）。
#
# ★ 额外拓展任务菜单（选项 2 会进来）：
#   [1] 串口：换相事件流（长趋势，时间覆盖 100%）
#   [2] 串口：窗口式全速采样（逐帧波形，窗口内有间隔）
#   [3] 跑前：清捕获状态 + 设窗口触发点（SWD）
#   [4] 跑后：dump 窗口波形 + 画图（SWD，电机须停）
#   [5] 电调 RAM 写读验证（首次烧 36K 固件前必跑一次）
#   [6] 电调 状态快照（SWD，电机须停）
#   [7] F1 给油（电机会转！）   [8] F1 收油
#   [9] 一条龙：跑前arm → 给油 → 采 N 秒 → 收油 → dump
#   [0] 返回 / 退出
#
# 无交互用法（供 tasks.json / 脚本调用）： esc-lab.sh 1 | esc-lab.sh 2
# ============================================================================
set -u

REPO="/home/lpl/桌面/RM2027/AM32-rm2027"
WORK="/home/lpl/桌面/RM2027/rm2027-drone/work"
FW_DIR="$REPO/firmware"
BIN="$REPO/obj/AM32_RM2027_ESC_G071_2.21.bin"
APP_ADDR="0x08001000"
MAX_SIZE=59392                    # app 区上限（再多就碰 EEPROM）
SERIAL="/dev/ttyUSB0"
ESC_DBG="$REPO/tools/esc-dbg.sh"

cd "$REPO" || exit 1

hr() { echo "=================================================="; }
pause() { read -rp "   (回车继续)" _; }

# ---------------------------------------------------------------- 核心：编译+烧录
core_build_flash() {
    local BRANCH COMMIT SZ MAKE_LOG OUT
    BRANCH=$(git rev-parse --abbrev-ref HEAD)
    COMMIT=$(git rev-parse --short HEAD)
    echo "→ 分支：$BRANCH    提交：$COMMIT"

    echo "---- 编译 ----"
    rm -f "$BIN"
    MAKE_LOG=$(mktemp)
    if ! make RM2027_ESC_G071 ARM_SDK_PREFIX=/usr/bin/arm-none-eabi- >"$MAKE_LOG" 2>&1; then
        echo "❌❌ 编译失败 —— 已中止，没有烧录任何东西"
        grep -iE "error|错误" "$MAKE_LOG" | head -12
        rm -f "$MAKE_LOG"; return 1
    fi
    [ -f "$BIN" ] || { echo "❌❌ 编译没有产出 $BIN —— 已中止"; tail -12 "$MAKE_LOG"; rm -f "$MAKE_LOG"; return 1; }
    rm -f "$MAKE_LOG"

    SZ=$(stat -c%s "$BIN")
    echo "✅ 编译通过：$SZ 字节"
    if [ "$SZ" -gt "$MAX_SIZE" ]; then
        echo "❌ bin $SZ > app 上限 $MAX_SIZE —— 会越界写到 EEPROM，拒绝烧录"; return 1
    fi

    mkdir -p "$FW_DIR"
    cp "$BIN" "$FW_DIR/$BRANCH.bin"
    echo "→ 已固化：firmware/$BRANCH.bin"

    # 清孤儿固件（分支已不存在的）
    local BRANCHES DEL=0
    BRANCHES=$(git branch --format='%(refname:short)')
    for f in "$FW_DIR"/*.bin; do
        [ -e "$f" ] || continue
        bn=$(basename "$f" .bin)
        if ! echo "$BRANCHES" | grep -qx "$bn"; then
            echo "  🗑 删除孤儿固件：$bn.bin"; rm -f "$f"; DEL=$((DEL + 1))
        fi
    done
    echo "→ 归档 $(ls -1 "$FW_DIR"/*.bin 2>/dev/null | wc -l) 个；本次删孤儿 $DEL 个"

    echo
    echo "---- 烧录 ----"
    echo "⚠ 请先【开 12V】，并确认 F1 正在发 0 油门（烧完电调会复位运行）"
    read -rp "   准备好后按回车继续（Ctrl-C 取消）" _
    OUT=$(timeout 300 openocd -f interface/stlink.cfg -f target/stm32g0x.cfg \
        -c "adapter speed 500" -c "init" -c "reset halt" \
        -c "flash write_image erase $BIN $APP_ADDR" \
        -c "verify_image $BIN $APP_ADDR" \
        -c "reset run" -c "shutdown" 2>&1)
    echo "$OUT" | grep -aiE "wrote|verified|Error" | sed 's/^/  /'
    if ! echo "$OUT" | grep -qai "verified"; then
        echo "❌ 烧录或校验失败（ST-Link 连接？）"; return 1
    fi
    echo "✅ 烧录完成并校验通过（只写 app 段 $APP_ADDR）"
    return 0
}

# ---------------------------------------------------------------- F1 油门
f1_throttle() {   # $1 = 0 给油 / 1 收油
    timeout 25 openocd -f interface/jlink.cfg -c "transport select swd" -f target/stm32f1x.cfg \
        -c "adapter speed 500" -c "init" -c "halt" -c "mwb 0x20000274 $1" -c "resume" -c "shutdown" >/dev/null 2>&1
}

# ---------------------------------------------------------------- 一条龙
one_shot() {
    local SECS=${1:-8} TRIG=${2:-36000}
    echo "== 一条龙：arm → 给油 → 采 ${SECS}s → 收油 → dump =="
    [ -x "$ESC_DBG" ] && "$ESC_DBG" arm "$TRIG"
    ( timeout $((SECS + 15)) python3 "$WORK/evt-dump.py" "$SERIAL" "$SECS" >/tmp/evt_run.log 2>&1 ) &
    local L=$!
    sleep 3
    echo "→ 给油"; f1_throttle 0
    sleep "$SECS"
    echo "→ 收油"; f1_throttle 1
    wait "$L" 2>/dev/null
    sleep 2
    echo "→ dump 窗口波形"; python3 "$WORK/zcw-dump.py" | tail -4
    tail -12 /tmp/evt_run.log | sed 's/^/  /'
}

# ---------------------------------------------------------------- 额外拓展任务
extra_menu() {
    while :; do
        echo
        hr; echo "  额外拓展任务（采集 / 调试）"; hr
        echo "  [1] 串口：换相事件流（长趋势，覆盖 100%）"
        echo "  [2] 串口：窗口式全速采样（逐帧波形）"
        echo "  [3] 跑前：清捕获状态 + 设窗口触发点（SWD）"
        echo "  [4] 跑后：dump 窗口波形 + 画图（SWD）"
        echo "  [5] 电调 RAM 写读验证（首次烧 36K 固件前必跑）"
        echo "  [6] 电调 状态快照（SWD）"
        echo "  [7] F1 给油（电机会转！）"
        echo "  [8] F1 收油"
        echo "  [9] 一条龙：arm → 给油 → 采 N 秒 → 收油 → dump"
        echo "  [0] 返回"
        read -rp "选择: " c
        case "${c:-x}" in
          1) read -rp "秒数 [8]: " s; python3 "$WORK/evt-dump.py" "$SERIAL" "${s:-8}" ;;
          2) read -rp "秒数 [8]: " s; python3 "$WORK/win-dump.py" "$SERIAL" "${s:-8}" ;;
          3) read -rp "触发点帧数 [36000=交棒后1500ms]: " f; "$ESC_DBG" arm "${f:-36000}" ;;
          4) python3 "$WORK/zcw-dump.py" | tail -4 ;;
          5) "$ESC_DBG" ramcheck ;;
          6) "$ESC_DBG" snap ;;
          7) echo "⚠ 即将给油，电机会转"; f1_throttle 0; echo "已给油" ;;
          8) f1_throttle 1; echo "已收油" ;;
          9) read -rp "采多少秒 [8]: " s; read -rp "触发点帧数 [36000]: " f; one_shot "${s:-8}" "${f:-36000}" ;;
          0) return ;;
          *) echo "无效" ;;
        esac
    done
}

# ---------------------------------------------------------------- 主菜单
main_menu() {
    echo
    hr; echo "  RM2027 电调实验  [$(git rev-parse --abbrev-ref HEAD)]"; hr
    echo "  [1] 编译当前分支 → 固件改名 → 烧录"
    echo "  [2] 1 + 额外拓展任务（采集 / 调试）"
    echo "  [0] 退出"
    read -rp "选择: " c
    case "${c:-x}" in
      1) core_build_flash ;;
      2) core_build_flash && extra_menu ;;
      0) exit 0 ;;
      *) echo "无效" ;;
    esac
}

# 无交互：esc-lab.sh 1 / 2
if [ $# -ge 1 ]; then
    case "$1" in
      1) core_build_flash ;;
      2) core_build_flash && extra_menu ;;
      *) echo "用法: esc-lab.sh [1|2]"; exit 1 ;;
    esac
else
    while :; do main_menu; done
fi
