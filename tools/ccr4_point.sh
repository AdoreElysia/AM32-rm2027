#!/usr/bin/env bash
# ============================================================================
# ccr4_point.sh —— CCR4 单点采集（CCR4 扫描用；烧录复位后写 TIM1->CCR4 再采）
#
# 用法:  ./tools/ccr4_point.sh <ccr4> [秒数=7]
# 例:    ./tools/ccr4_point.sh 200 7
#        ./tools/ccr4_point.sh 180 7
#
# 说明：
#   · 复用当前分支归档 bin（ESC_REUSE=1），不重复 make；
#   · 烧录复位后、给油前用 SWD 写 TIM1->CCR4；
#   · CSV/meta 会记录 ESC_CCR4，方便后续 ccr4_scan_compare.py 汇总。
# ============================================================================
set -u
CCR4="${1:-}"
SECS="${2:-7}"
if [ -z "$CCR4" ]; then
    echo "用法: $0 <ccr4> [秒数=7]"; exit 1
fi
export ESC_CCR4="$CCR4"
export ESC_YES=1
export ESC_REUSE=1
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/esc-lab.sh" flash-cap "$SECS"
