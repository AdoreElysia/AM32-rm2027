#!/usr/bin/env bash
# ============================================================================
# _env.sh —— 路径真源（xien/n2 新增）：让整套工具不依赖具体用户名
#
# 为什么需要它：
#   原来 18 个脚本里硬编码 `/home/lpl/桌面/RM2027/...`（队友那台机器的用户名）。
#   换机器 / SSH / VSCode 任务都会踩空。本文件统一解析，其它脚本 source 它即可。
#
# 解析顺序（全部可用环境变量覆盖）：
#   RM2027_ROOT  项目根（默认 = $HOME/桌面/RM2027，找不到则试 ~/RM2027、~/Desktop/RM2027）
#   RM2027_REPO  固件仓（默认 = 本脚本所在仓库根，即 tools/ 的上一级）
#   RM2027_DATA  数据落盘目录（evt_run_*.csv / *.npz 的家，默认 = RM2027_ROOT）
#   RM2027_WORK  PC 侧临时脚本目录（默认 = RM2027_ROOT/rm2027-drone/work）
#   ESC_SERIAL   串口（默认 /dev/ttyUSB0）
#   ESC_BAUD     波特率（默认 1000000，要跟固件 Inc/targets.h 的 ADC_CAP_BAUD 一致）
# ============================================================================
set -u

_MYDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "${RM2027_ROOT:-}" ]; then
    for _d in "$HOME/桌面/RM2027" "$HOME/RM2027" "$HOME/Desktop/RM2027" "$_MYDIR/.."; do
        [ -d "$_d" ] && { RM2027_ROOT="$(cd "$_d" && pwd)"; break; }
    done
fi
: "${RM2027_ROOT:=$HOME/桌面/RM2027}"
: "${RM2027_REPO:=$(cd "$_MYDIR/.." && pwd)}"
: "${RM2027_DATA:=$RM2027_ROOT}"
# PC 侧临时脚本目录：队友机是 <root>/rm2027-drone/work；本机可能放在数据仓或 shared 工作区
if [ -z "${RM2027_WORK:-}" ]; then
    for _w in "$RM2027_ROOT/rm2027-drone/work" \
               "$RM2027_ROOT/RM2027-data/rm2027-drone/work" \
               "$RM2027_ROOT/RM2027-data/drone/work" \
               "/mnt/h/AI/shared/projects/competition/rm2027-drone/work" \
               "$RM2027_ROOT/work"; do
        # 优先选真正带 PC 侧采集脚本的目录（避免落到空的同名目录）
        [ -d "$_w" ] || continue
        if [ -f "$_w/evt-dump.py" ] || [ -f "$_w/win-dump.py" ] || [ -f "$_w/zcw-dump.py" ]; then
            RM2027_WORK="$_w"; break
        fi
        [ -z "${RM2027_WORK:-}" ] && RM2027_WORK="$_w"
    done
fi
: "${RM2027_WORK:=$RM2027_ROOT/rm2027-drone/work}"
: "${ESC_SERIAL:=/dev/ttyUSB0}"
: "${ESC_BAUD:=2000000}"

export RM2027_ROOT RM2027_REPO RM2027_DATA RM2027_WORK ESC_SERIAL ESC_BAUD
