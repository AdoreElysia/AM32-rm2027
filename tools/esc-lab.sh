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
#   [1] 跑前：清捕获状态 + 设窗口触发点（SWD）
#   [2] 串口：窗口式全速采样（逐帧波形）
#   [3] 跑后：dump 窗口波形 + 画图（SWD，电机须停）
#   [4] F1 给油（电机会转！）   [5] F1 收油
#   [6] 一条龙：arm → 给油 → 采 N 秒 → 收油 → dump
#   [0] 返回 / 退出
#
# 无交互用法（供 tasks.json / 脚本调用）： esc-lab.sh 1 | esc-lab.sh 2
# ============================================================================
set -u

# ★ xien/n2：路径不再硬编码用户名，统一由 tools/_env.sh 解析（可用环境变量覆盖）
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/_env.sh"
REPO="${RM2027_REPO}"
WORK="${RM2027_WORK}"
OUTD="${RM2027_DATA}"
FW_DIR="$REPO/firmware"
BIN="$REPO/obj/AM32_RM2027_ESC_G071_2.21.bin"
APP_ADDR="0x08001000"
MAX_SIZE=59392                    # app 区上限（再多就碰 EEPROM）
SERIAL="${ESC_SERIAL}"
ESC_DBG="$REPO/tools/esc-dbg.sh"

cd "$REPO" || exit 1

hr() { echo "=================================================="; }
pause() { read -rp "   (回车继续)" _; }

# ---------------------------------------------------------------- 核心：编译+烧录
# ★ xien/n2 收尾：分支名稳定映射成文件名（分支含 "/" 时不再 cp 到不存在的子目录），
#   并支持"烧录已有固件"传参：esc-lab.sh flash [<分支名>|<bin路径>]
safe_fw_name() { printf '%s' "${1:-}" | sed 's#/#__#g'; }

# ★ 产物 meta：记录 branch/commit/bin hash，给"安全复用已有产物"用
meta_path() { printf '%s/%s.bin.meta' "$FW_DIR" "$(safe_fw_name "${1:-}")"; }

write_meta() {
    local BRANCH="$1" BINFILE="$2" META
    META="$(meta_path "$BRANCH")"
    {
        echo "branch=$BRANCH"
        echo "commit=$(git rev-parse HEAD)"
        echo "sha256=$(sha256sum "$BINFILE" | awk '{print $1}')"
        echo "size=$(stat -c%s "$BINFILE")"
        echo "built=$(date -Iseconds)"
    } > "$META"
    echo "→ 已写产物 meta：${META#$REPO/}"
}

# 只在满足全部条件时复用 firmware/<分支>.bin，否则返回 1 让调用者重新编译：
#   ① branch / commit 与当前 HEAD 完全一致；
#   ② Inc Src Mcu/g071 Makefile 没有未提交改动（含 staged/untracked）；
#   ③ 归档 bin 的 sha256 与 meta 一致且大小不超限。
try_reuse_current() {
    local BRANCH SAFE ARCH META MB MC MS CS SZ
    BRANCH=$(git rev-parse --abbrev-ref HEAD)
    SAFE=$(safe_fw_name "$BRANCH")
    ARCH="$FW_DIR/$SAFE.bin"; META="$ARCH.meta"
    [ -f "$ARCH" ] && [ -f "$META" ] || return 1
    [ -f "$FW_DIR/$SAFE.bin.meta" ] || return 1
    MB=$(sed -n 's/^branch=//p' "$META" | head -1)
    MC=$(sed -n 's/^commit=//p' "$META" | head -1)
    MS=$(sed -n 's/^sha256=//p' "$META" | head -1)
    [ "$MB" = "$BRANCH" ] || return 1
    [ "$MC" = "$(git rev-parse HEAD)" ] || return 1
    [ -z "$(git status --porcelain -- Inc Src Mcu/g071 Makefile 2>/dev/null)" ] || return 1
    CS=$(sha256sum "$ARCH" | awk '{print $1}')
    [ -n "$MS" ] && [ "$MS" = "$CS" ] || return 1
    SZ=$(stat -c%s "$ARCH")
    [ "$SZ" -le "$MAX_SIZE" ] || return 1
    BIN="$ARCH"
    echo "♻ 复用已有产物：firmware/$SAFE.bin（commit ${MC:0:8}，sha ${CS:0:12}，${SZ} 字节）"
    return 0
}

flash_bin() {
    local BIN="$1" SZ TRY OUT
    [ -f "$BIN" ] || { echo "❌❌ 找不到要烧录的固件：$BIN"; return 1; }
    SZ=$(stat -c%s "$BIN")
    echo "→ 烧录文件：$BIN（$SZ 字节）"
    if [ "$SZ" -gt "$MAX_SIZE" ]; then
        echo "❌ bin $SZ > app 上限 $MAX_SIZE —— 会越界写到 EEPROM，拒绝烧录"; return 1
    fi

    echo
    echo "---- 烧录 ----"
    echo "⚠ 烧录【不需要 12V】：电调逻辑由 ST-Link 的 3.3V 供电就够。"
    echo "   反过来说，开着 12V + F1 还给着油，会让 AM32 引导程序认定"
    echo "   '信号持续高 ⇒ 进固件升级模式' 而不跳 app（实测 PC 停在 0x08000104），"
    echo "   此时写 flash 会随机报 'block write failed'。"
    echo "   ⇒ 请先【让 F1 收油】（额外拓展任务 [5]），最好再把 12V 关掉。"
    # ★ xien/n2：支持非交互（VSCode 任务 / ssh）。ESC_YES=1 或 stdin 不是终端 ⇒ 直接继续
    if [ "${ESC_YES:-0}" = "1" ] || [ ! -t 0 ]; then
        echo "   （非交互模式：跳过回车确认）"
    else
        read -rp "   准备好后按回车继续（Ctrl-C 取消）" _ || { echo "❌ 没读到回车（EOF/管道）⇒ 取消烧录，安全第一"; return 1; }
    fi

    # ★ 重试 3 次：'block write failed' 大多数是瞬时（信号/电源抖动、引导程序状态）
    for TRY in 1 2 3; do
        OUT=$(timeout 300 openocd -f interface/stlink.cfg -f target/stm32g0x.cfg \
            -c "adapter speed 500" -c "init" -c "reset halt" \
            -c "flash write_image erase $BIN $APP_ADDR" \
            -c "verify_image $BIN $APP_ADDR" \
            -c "reset run" -c "shutdown" 2>&1)
        echo "$OUT" | grep -aiE "wrote|verified|flash size|RDP level|block write failed|Error" | sed 's/^/  /'
        if echo "$OUT" | grep -qai "verified"; then
            echo "✅ 烧录完成并校验通过（第 $TRY 次尝试；只写 app 段 $APP_ADDR）"
            return 0
        fi
        echo "  ⚠ 第 $TRY 次失败 —— 常见原因：① F1 还在给油（引导程序停在升级模式）② USB/ST-Link 抖动"
        [ "$TRY" -lt 3 ] && sleep 2
    done
    echo "❌ 连续 3 次失败。请依次：① 让 F1 收油 ② 关掉 12V ③ 拔插一次 ST-Link 再试"
    return 1
}

build_current() {
    local BRANCH COMMIT SZ MAKE_LOG SAFE_BRANCH BRANCHES DEL=0 f bn
    BRANCH=$(git rev-parse --abbrev-ref HEAD)
    COMMIT=$(git rev-parse --short HEAD)
    echo "→ 当前分支：$BRANCH    提交：$COMMIT"

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
    SAFE_BRANCH=$(safe_fw_name "$BRANCH")
    if ! cp "$BIN" "$FW_DIR/$SAFE_BRANCH.bin"; then
        echo "❌❌ 归档失败：$FW_DIR/$SAFE_BRANCH.bin"; return 1
    fi
    echo "→ 已固化：firmware/$SAFE_BRANCH.bin（分支 $BRANCH）"
    write_meta "$BRANCH" "$FW_DIR/$SAFE_BRANCH.bin"

    # 清孤儿固件（分支已不存在的；文件名已把 / 映射成 __）
    BRANCHES=$(git branch --format='%(refname:short)' | while IFS= read -r b; do safe_fw_name "$b"; echo; done)
    for f in "$FW_DIR"/*.bin; do
        [ -e "$f" ] || continue
        bn=$(basename "$f" .bin)
        if ! echo "$BRANCHES" | grep -qx "$bn"; then
            echo "  🗑 删除孤儿固件：$bn.bin"; rm -f "$f"; DEL=$((DEL + 1))
        fi
    done
    echo "→ 归档 $(ls -1 "$FW_DIR"/*.bin 2>/dev/null | wc -l) 个；本次删孤儿 $DEL 个"
}

core_build_flash() {
    local TARGET="${1:-}"
    if [ -n "$TARGET" ]; then
        if [ -f "$TARGET" ]; then
            BIN="$TARGET"
        else
            local SAFE; SAFE=$(safe_fw_name "$TARGET")
            if [ -f "$FW_DIR/$SAFE.bin" ]; then
                BIN="$FW_DIR/$SAFE.bin"
            elif [ -f "$FW_DIR/$TARGET.bin" ]; then
                BIN="$FW_DIR/$TARGET.bin"
            else
                echo "❌ 指定的固件不存在：$TARGET"
                echo "   可用：esc-lab.sh flash <分支名>  （读 firmware/<分支名>.bin，/ 会映射成 __）"
                echo "         或 esc-lab.sh flash <bin 路径>"
                echo "   当前已归档："; ls -1 "$FW_DIR"/*.bin 2>/dev/null | sed 's#^#     #' || true
                return 1
            fi
        fi
        echo "→ 指定固件模式（跳过编译）：$TARGET"
    elif [ "${ESC_REUSE:-0}" = "1" ] && try_reuse_current; then
        :
    else
        build_current || return 1
    fi
    flash_bin "$BIN"
}

# ---------------------------------------------------------------- F1 油门
# wait_esc_armed：等 ESC 的 AM32 走完"连续零油门约 1s"的解锁窗口。
#   重烧/复位 ESC 后，必须给它一段真正为 0 的 DShot；只睡 1s 常常不够（实测）。
#   这里用 SWD 读 ESC 的 armed（只读，不扰时序），最多等 ~6s。
wait_esc_armed() {
    local i
    for i in $(seq 1 12); do
        if timeout 10 "$REPO/tools/esc-dbg.sh" peek armed 2>/dev/null | grep -q '= 0x00000001'; then
            echo "   ESC 已 armed（等待 ${i} 拍）"
            return 0
        fi
        sleep 0.4
    done
    echo "   ⚠ 等待 ~6s 后 ESC 仍未 armed —— 继续给油，但可能起不来"
    return 1
}

f1_throttle() {   # $1 = 0 给油 / 1 收油；写后 mdb 回读确认，失败重试 3 次
    local val="${1:-1}" hex try out rb
    printf -v hex '%02x' "$val"
    for try in 1 2 3; do
        out=$(timeout 25 openocd -f interface/jlink.cfg -c "transport select swd" -f target/stm32f1x.cfg \
            -c "adapter speed 500" -c "init" -c "halt" -c "mwb 0x20000274 $val" \
            -c "mdb 0x20000274 1" -c "resume" -c "shutdown" 2>&1)
        rb=$(printf '%s\n' "$out" | sed -n 's/^0x20000274: \([0-9a-fA-F][0-9a-fA-F]\).*/\1/p' | tail -1)
        if [ "$rb" = "$hex" ]; then
            return 0
        fi
        echo "  ⚠ F1 油门写 $val 回读异常（try=$try，回读='${rb:-无}'）"
        sleep 0.2
    done
    echo "❌ F1 油门写 $val 连续失败；最后输出："
    printf '%s\n' "$out" | tail -6 | sed 's/^/    /'
    return 1
}

# ---------------------------------------------------------------- 一条龙
# 默认跑 6 秒（闭环）：发热上限 ≤7s；关键信息在交棒后 0~2s，2~6s 看恶化。
# ★ 注意：跑多久必须【大于触发点】，否则窗口还没冻结电机就停了。
#   扫 4000/6000ms 那两档时要手动输 7 秒。
one_shot() {
    local SECS=${1:-6} TRIG=${2:-36000}
    local TRIG_MS=$(( TRIG / 24 ))          # 帧 -> ms（1 帧 = 41.67µs）
    echo "== 一条龙：arm → 给油 → 采 ${SECS}s → 收油 → dump =="
    echo "   触发点 = ${TRIG} 帧 = ${TRIG_MS} ms"
    # ★ 防呆：跑的时间必须大于触发点，否则窗口还没冻结电机就停了
    if [ "$TRIG_MS" -ge $(( SECS * 1000 )) ]; then
        echo "❌ 触发点 ${TRIG_MS}ms ≥ 本次只跑 ${SECS}s ⇒ 窗口来不及冻结就停机了"
        echo "   建议把秒数改成 $(( TRIG_MS / 1000 + 2 ))（且不超过 7 秒的发热上限）"
        return 1
    fi
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
        echo "  [1] 跑前：清捕获状态 + 设窗口触发点（SWD）"
        echo "  [2] 串口：窗口式全速采样（逐帧波形）"
        echo "  [3] 跑后：dump 窗口波形 + 画图（SWD）"
        echo "  [4] F1 给油（电机会转！）"
        echo "  [5] F1 收油"
        echo "  [6] 一条龙：arm → 给油 → 采 N 秒 → 收油 → dump"
        echo "  [0] 返回"
        read -rp "选择: " c || return      # ★ EOF/管道断就返回，别空转
        case "${c:-x}" in
          1) read -rp "触发点帧数 [36000=交棒后1500ms]: " f || return; "$ESC_DBG" arm "${f:-36000}" ;;
          2) read -rp "秒数 [闭环6 / 开环4]: " s || return; python3 "$WORK/win-dump.py" "$SERIAL" "${s:-6}" ;;
          3) python3 "$WORK/zcw-dump.py" | tail -4 ;;
          4) echo "⚠ 即将给油，电机会转"; f1_throttle 0; echo "已给油" ;;
          5) f1_throttle 1; echo "已收油" ;;
          6) read -rp "采多少秒 [6=闭环 / 4=开环；扫 4000/6000ms 触发点用 7]: " s || return
             read -rp "触发点帧数 [36000=交棒后1500ms]: " f || return
             one_shot "${s:-6}" "${f:-36000}" ;;
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
    read -rp "选择: " c || exit 0        # ★ read 失败(EOF/管道断)就退出，否则会空转死循环
    case "${c:-x}" in
      1) core_build_flash ;;
      2) core_build_flash && extra_menu ;;
      0) exit 0 ;;
      *) echo "无效" ;;
    esac
}

# 无交互：esc-lab.sh 1 / 2
# ---------------------------------------------------------------- 非交互子命令（xien/n2）
# 给 VSCode tasks.json / ssh 用：不弹菜单、不等回车。
#   esc-lab.sh flash            编译+归档+烧录（先 F1 收油）
#   esc-lab.sh cap <秒>          只采集（cap-evt.py：给油→采→收油→存 CSV+NPZ+图）
#   esc-lab.sh flash-cap <秒> [固件]  ★ 采集顺道烧录：先开串口 spool → 烧录 → 给油采 → 解析
#   esc-lab.sh judge <秒> [标签]  计数轮判分（关窗口流）
# 可选：ESC_CCR4=<val> 时，在烧录复位后、给油前写 TIM1->CCR4（CCR4 扫描用）
apply_ccr4() {
    [ -n "${ESC_CCR4:-}" ] || return 0
    echo "→ 设置 ADC 触发点 TIM1->CCR4=$ESC_CCR4"
    "$ESC_DBG" ccr4 "$ESC_CCR4"
}

cmd_flash() { echo "== 当前分支：$(git rev-parse --abbrev-ref HEAD) =="; f1_throttle 1 || { echo "❌ F1 收油失败，拒绝烧录"; return 1; }; core_build_flash "${1:-}"; }

# build-only：只编译+归档+写 meta，不烧录、不碰串口/电机。
cmd_build() { build_current; }

# reuse-check：只检查"当前分支能否安全复用归档产物"，不烧录、不驱动。
cmd_reuse_check() {
    if try_reuse_current; then
        echo "✅ 结论：ESC 2 会跳过编译，直接复用上面的归档 bin"
    else
        echo "❌ 结论：不满足安全复用条件（branch/commit/source/hash 任一不符），ESC 2 会重新编译"
    fi
}
cmd_cap() {
    local secs="${1:-8}"
    echo "== 窗口流采集 ${secs}s（cap-evt.py）=="
    echo "   当前分支：$(git rev-parse --abbrev-ref HEAD)"
    echo "   串口 $SERIAL @ $ESC_BAUD（必须跟固件 ADC_CAP_BAUD 一致）"
    apply_ccr4
    ( cd "$REPO" && python3 tools/cap-evt.py "$secs" )
}
# ★ xien/n2 收尾：采集顺道烧录 —— 先开串口 spool，再烧录复位，再给油采。
#   这样不会漏掉"烧录复位 → 交棒"这段最关键的数据。烧录走 ST-Link，串口走 USB-TTL，不冲突。
cmd_flash_cap() {
    local secs="${1:-8}" target="${2:-}" raw spid
    raw=$(mktemp /tmp/rm2027_spool_XXXXXX.bin)
    export ESC_YES=1
    echo "== 采集顺道烧录 ${secs}s：先开串口 spool → 烧录 → 收油唤醒 → 给油采 → 解析 =="
    echo "   当前分支：$(git rev-parse --abbrev-ref HEAD)"
    echo "   raw: $raw   串口: $SERIAL @ $ESC_BAUD"
    python3 tools/serial-spool.py "$raw" &
    spid=$!
    sleep 0.3
    f1_throttle 1
    if ! core_build_flash "$target"; then
        echo "❌ 烧录失败，停止采集"; kill -INT "$spid" 2>/dev/null; wait "$spid" 2>/dev/null; rm -f "$raw"; return 1
    fi
    if ! apply_ccr4; then
        echo "❌ 写 TIM1->CCR4 失败"; kill -INT "$spid" 2>/dev/null; wait "$spid" 2>/dev/null; rm -f "$raw"; return 1
    fi
    echo "← 唤醒：先收油并等 ESC armed"; f1_throttle 1 || { echo "❌ F1 收油失败"; kill -INT "$spid" 2>/dev/null; wait "$spid" 2>/dev/null; rm -f "$raw"; return 1; }
    wait_esc_armed || true
    echo "→ 给油，采 ${secs}s"; f1_throttle 0 || { echo "❌ F1 给油失败"; kill -INT "$spid" 2>/dev/null; wait "$spid" 2>/dev/null; rm -f "$raw"; return 1; }
    sleep "$secs"
    echo "← 收油"; f1_throttle 1 || echo "⚠ F1 收油写失败，请手动收油/断电"
    kill -INT "$spid" 2>/dev/null; wait "$spid" 2>/dev/null
    echo "→ 解析 raw 并落盘（不重复驱动电机）"
    if ! ( cd "$REPO" && python3 tools/cap-evt.py --raw="$raw" ); then
        echo "❌ raw 解析失败；raw 保留在 $raw（可手动重跑）"; return 1
    fi
    rm -f "$raw"
}
cmd_flash_ext() {
    local target="${1:-}"
    echo "== 编译烧录 + 分支扩展脚本 =="
    cmd_flash "$target" || return 1
    local BRANCH SAFE EXT
    BRANCH=$(git rev-parse --abbrev-ref HEAD)
    SAFE=$(safe_fw_name "$BRANCH")
    EXT="$REPO/tools/branch/$SAFE.py"
    export ESC_BRANCH="$BRANCH"
    export ESC_COMMIT="$(git rev-parse --short HEAD)"
    export ESC_BIN="$FW_DIR/$SAFE.bin"
    export ESC_BIN_SHA="$(sha256sum "$FW_DIR/$SAFE.bin" 2>/dev/null | awk '{print $1}')"
    export ESC_ROOT="$REPO"
    if [ -f "$EXT" ]; then
        echo "→ 运行扩展脚本：tools/branch/$SAFE.py"
        python3 "$EXT" || { echo "❌ 扩展脚本返回非 0"; return 1; }
    else
        echo "（本分支无扩展脚本 tools/branch/$SAFE.py —— 跳过）"
    fi
}

cmd_judge() {
    local secs="${1:-11}"
    echo "== 计数轮判分 ${secs}s（关窗口流）=="
    echo "   当前分支：$(git rev-parse --abbrev-ref HEAD)"
    ( cd "$REPO" && python3 tools/count-run.py "$secs" "${2:-}" )
}
if [ $# -ge 1 ]; then
    case "$1" in
      1) core_build_flash ;;
      2) core_build_flash && extra_menu ;;
      flash)     shift; cmd_flash "${1:-}" ;;
      build-only) cmd_build ;;
      reuse-check) cmd_reuse_check ;;
      cap)       shift; cmd_cap "${1:-8}" ;;
      flash-cap) shift; cmd_flash_cap "${1:-8}" "${2:-}" ;;
      flash-ext) shift; cmd_flash_ext "${1:-}" ;;
      judge)     shift; cmd_judge "${1:-11}" "${2:-}" ;;
      *) echo "用法: esc-lab.sh [1|2|flash [<分支>|<bin>]|flash-ext [<分支>|<bin>]|build-only|reuse-check|cap <秒>|flash-cap <秒> [<分支>|<bin>]|judge <秒> <标签>]"; exit 1 ;;
    esac
else
    while :; do main_menu; done
fi
