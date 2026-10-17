#!/bin/bash
# esc-dbg.sh —— 电调（STM32G071）的 SWD "夹具"：把"跑前 arm / 跑后 dump"标准化成一条命令。
#
# 为什么要有它：
#   实验流程里 PC 侧只做两件事 —— 跑【前】武装捕获、跑【后】取数据。跑的过程中一次都不碰 SWD
#   （一次 halt 就停电机、把波形和时序全打乱；而且本固件开着 IWDG）。
#   原来这两件事靠手敲 openocd、地址还是硬编码的（比如 0x200003b4）——一旦重新编译、
#   变量布局一变，硬编码地址就静默失效。本脚本一律用 arm-none-eabi-nm 从【当前 ELF】现取地址。
#
# ★★ 四条纪律（都是从踩过的坑里抄来的，别改）★★
#   ① 地址必须现取，不能硬编码（变量地址每次编译都可能漂）。
#   ② halt 之后立刻写 IWDG_KR(0x40003000)=0xAAAA 喂狗。本固件 IWDG≈512ms，喂狗只在
#      主循环/PWM 中断里 ⇒ halt 久了必被复位、pc 停在 bootloader。
#   ③ mdw 写在 -f 脚本文件里不输出 ⇒ 读数必须用 "-c mdw ..."。
#   ④ 不同分支变量名不一样 ⇒ 按符号是否存在动态生成，缺符号跳过，别让整条命令失败。
#
# 用法：
#   tools/esc-dbg.sh ramcheck        验证 16K 之外真的有 RAM（改链接脚本后跑一次）
#   tools/esc-dbg.sh addr <符号名>    打印某符号的当前地址
#   tools/esc-dbg.sh peek <符号名>    读一个变量（u32）
#   tools/esc-dbg.sh poke <名> <val>  写 8 位
#   tools/esc-dbg.sh poke16 <名> <val> 写 16 位
#   tools/esc-dbg.sh poke32 <名> <val> 写 32 位（细粒度目标 ol_ci_tgt 用这个）
#   tools/esc-dbg.sh clear           清捕获状态（跑前先清，避免读到上一轮残留）
#   tools/esc-dbg.sh settrig <帧数>   设"交棒后多少帧"触发点（1 帧 = 41.67µs；1500ms => 36000）
#   tools/esc-dbg.sh arm [<帧数>]     clear + settrig 一步到位（跑前）
#   tools/esc-dbg.sh snap            转调 esc-snap.sh 快照
set -u

ELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$ELF_DIR/_env.sh"
ELF="$RM2027_REPO/obj/AM32_RM2027_ESC_G071_2.21.elf"
NM=/usr/bin/arm-none-eabi-nm
OCD_IF=interface/stlink.cfg
OCD_TG=target/stm32g0x.cfg
SPEED=1800                      # kHz
IWDG_KR=0x40003000              # 写 0xAAAA = 喂狗

[ -f "$ELF" ] || { echo "❌ 找不到 $ELF，先编译"; exit 1; }

# ---- 从 ELF 取符号地址（找不到时输出空）----
symaddr() {
    "$NM" -S "$ELF" 2>/dev/null | awk -v s="$1" '$4==s {print "0x"$1; exit}'
}
need() {
    local a; a=$(symaddr "$1")
    [ -n "$a" ] || { echo "❌ ELF 里没有符号 $1（分支不对？忘了编译？）" >&2; exit 1; }
    printf '%s' "$a"
}

# ---- 一次 openocd 会话：始终 halt→喂狗→(动作)→喂狗→resume ----
run_oc() {
    openocd -f "$OCD_IF" -f "$OCD_TG" -c "adapter speed $SPEED" -c init \
        -c "halt" -c "mww $IWDG_KR 0xAAAA" "$@" \
        -c "mww $IWDG_KR 0xAAAA" -c "resume" -c "shutdown" 2>&1
}
rd32() {
    run_oc -c "mdw $1 1" | grep -oE '^0x[0-9a-fA-F]+: [0-9a-fA-F]+' | head -1 | awk '{print $2}'
}

cmd=${1:-help}
case "$cmd" in
  addr)
    a=$(symaddr "${2:-}"); [ -n "$a" ] && echo "$2 = $a" || echo "❌ 没有符号 ${2:-<空>}"
    ;;

  peek)
    a=$(need "${2:-}"); v=$(rd32 "$a"); echo "$2 @ $a = 0x$v (十进制 $((16#$v)))"
    ;;

  poke)
    # poke <符号名> <值> —— 写【一个字节】(mwb)。给"运行时开关"用，例如
    #   tools/esc-dbg.sh poke win_enable 0   → 停发窗口流，把 1Mbaud 全给事件流
    #     （窗口流需要 288KB/s、串口只有 ~125KB/s ⇒ 数学上不可能共存，开着窗口流时
    #       事件流最多只采到 ~30%，"漏过零次数"就永远只是个采样值。关掉它，
    #       事件流只要 34KB/s ⇒ 逐事件零丢失，超时计数变成精确值。）
    #   tools/esc-dbg.sh poke win_enable 1   → 恢复窗口流（要看全速波形时用）
    [ $# -ge 3 ] || { echo "用法: esc-dbg.sh poke <符号名> <0..255>"; exit 1; }
    a=$(need "$2")
    run_oc -c "mwb $a $3" >/dev/null
    v=$(rd32 "$a")
    echo "$2 @ $a ← $3   （回读 0x$v，低字节 = $(( 16#${v: -2} )))"
    ;;

  poke16)
    # poke16 <符号名> <值> —— 写【16 位】(mwh)。给"窗口流降采样倍数 win_every_n"用：
    #   tools/esc-dbg.sh poke16 win_every_n 8    → 每 8 个窗只发 1 个（窗内仍 512 点全速）
    #   tools/esc-dbg.sh poke16 win_every_n 1    → 恢复上游行为（每窗都发）
    # ★ 为什么用 16 位而不是 8 位：N 要能取到 1~255 之间的任意值做扫描，
    #   而且 win_every_n 在固件里就是 uint16_t（见 Src/main.c）。
    [ $# -ge 3 ] || { echo "用法: esc-dbg.sh poke16 <符号名> <0..65535>"; exit 1; }
    a=$(need "$2")
    run_oc -c "mwh $a $3" >/dev/null
    v=$(rd32 "$a")
    echo "$2 @ $a ← $3   （回读 0x$v，低 16 位 = $(( 16#${v: -4} )))"
    ;;

  poke32)
    # poke32 <符号名> <值> —— 写【32 位】(mww)。给细粒度目标 ci 用：
    #   tools/esc-dbg.sh poke32 ol_ci_tgt 1700   → 目标换相间隔 1700 拍（1 拍分辨率）
    [ $# -ge 3 ] || { echo "用法: esc-dbg.sh poke32 <符号名> <0..4294967295>"; exit 1; }
    a=$(need "$2")
    run_oc -c "mww $a $3" >/dev/null
    v=$(rd32 "$a")
    echo "$2 @ $a ← $3   （回读 0x$v）"
    ;;

  ramcheck)
    # ★ 改链接脚本（RAM 16K→36K）之后必跑一次：往 16K 之外写魔数再读回。
    #   0x20003FFC = 旧上限（16K 边界），0x20008800 = 36K 区靠顶（避开栈顶 0x20009000）。
    echo "== RAM 写读验证 =="
    ok=1
    for A in 0x20003FFC 0x20004000 0x20008000 0x20008800 0x20008FFC; do
        # ★ 魔数用小写：openocd 的 mdw 输出是小写十六进制，大小写不一致会误判成"读不回"
        MAGIC=$(printf '0x%08x' $(( (RANDOM<<16 ^ RANDOM) & 0xFFFFFFFF )))
        run_oc -c "mww $A $MAGIC" >/dev/null
        got=$(rd32 "$A" | tr 'A-Z' 'a-z')
        if [ "$got" = "${MAGIC#0x}" ]; then
            echo "  ✅ $A 写读一致（$MAGIC）"
        else
            echo "  ❌ $A 写 $MAGIC 读回 0x$got —— 这段 RAM 不存在！"
            ok=0
        fi
    done
    [ $ok -eq 1 ] \
        && echo "== 结论：36K RAM 真实存在，链接脚本保持 LENGTH = 36K ==" \
        || echo "== 结论：有地址读不回 ⇒ 芯片不是 36K，把链接脚本改回去 =="
    ;;

  clear)
    # 清"冻结/触发/流"状态。缺符号自动跳过（不同分支命名不同）。
    ARGS=()
    for s in zcw_arm_st zcw_state zcw_win zcw_arm_frame zcw_idx zcw_trig_idx \
             zcw_n_trig zcw_ci_trig win_seq win_sent; do
        a=$(symaddr "$s") || true
        [ -n "$a" ] && ARGS+=(-c "mww $a 0")
    done
    [ ${#ARGS[@]} -gt 0 ] || { echo "❌ 一个状态符号都没找到"; exit 1; }
    run_oc "${ARGS[@]}" >/dev/null
    echo "✅ 捕获状态已清零（${#ARGS[@]} 个变量）"
    ;;

  settrig)
    [ $# -ge 2 ] || { echo "用法: esc-dbg.sh settrig <帧数>  (1 帧 = 41.67µs; 1500ms => 36000)"; exit 1; }
    a=$(need zcw_trig_delay_frames)
    run_oc -c "mww $a $2" >/dev/null
    awk -v f="$2" 'BEGIN{printf "✅ 触发点 = %s 帧 = %.1f ms（交棒后）\n", f, f/24.0}'
    ;;

  arm)
    FRAMES=${2:-36000}
    "$0" clear
    "$0" settrig "$FRAMES"
    echo "== 跑前准备完成：可以给油了 =="
    ;;

  snap)
    bash "$RM2027_WORK/esc-snap.sh"
    ;;

  help|*)
    sed -n '17,24p' "$0" | sed 's/^# \{0,1\}//'
    ;;
esac
