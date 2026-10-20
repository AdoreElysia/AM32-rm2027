#!/bin/bash
# ============================================================================
# flash-safe.sh —— 只烧 app 区，并且【烧前备份 / 烧后校验】最后一页校准数据
#
#   为什么要有它：校准结果存在 flash 最后一页 0x0801F800（2KB）。
#   app 区只到 0x0800F800 ⇒ 正常烧录不会碰它；但一旦有人手滑敲
#   `flash erase_sector 0 63 63`、或改用全片擦除，校准数据就没了。
#   本脚本把"不许碰"变成可验证的动作：
#     ① 烧前 dump 最后一页 → /tmp/calpage_before.bin
#     ② 只对 app 区 write_image erase（地址 0x08001000）
#     ③ 烧后再 dump → /tmp/calpage_after.bin，逐字节比对
#     ④ 不一致 ⇒ 大红字报警 + 非零退出（以后再也不会"静默丢数据"）
#
#   用法： tools/flash-safe.sh [bin路径]     （默认 obj/AM32_RM2027_ESC_G071_2.21.bin）
# ============================================================================
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${1:-$REPO/obj/AM32_RM2027_ESC_G071_2.21.bin}"
APP=0x08001000
CAL=0x0801F800
CAL_LEN=0x800

[ -f "$BIN" ] || { echo "❌ 找不到 $BIN"; exit 1; }
oc() { openocd -f interface/stlink.cfg -f target/stm32g0x.cfg -c "adapter speed 500" "$@" 2>&1; }

echo "== ① 烧前备份校准页 =="
oc -c init -c halt -c "mww 0x40003000 0xAAAA" -c "dump_image /tmp/calpage_before.bin $CAL $CAL_LEN" -c "mww 0x40003000 0xAAAA" -c resume -c shutdown >/dev/null
[ -f /tmp/calpage_before.bin ] || { echo "❌ 备份失败，拒绝继续"; exit 1; }
echo "   → /tmp/calpage_before.bin  ($(stat -c%s /tmp/calpage_before.bin) 字节)"

echo "== ② 只烧 app 区（不触碰 $CAL）=="
OUT=$(oc -c init -c "reset halt" -c "flash write_image erase $BIN $APP" -c "verify_image $BIN $APP" -c "reset run" -c shutdown)
echo "$OUT" | grep -aiE "wrote|verified" | sed 's/^/   /'
echo "$OUT" | grep -qi "verified" || { echo "❌ 烧录失败"; echo "$OUT" | tail -6; exit 1; }

echo "== ③ 烧后校验校准页 =="
oc -c init -c halt -c "mww 0x40003000 0xAAAA" -c "dump_image /tmp/calpage_after.bin $CAL $CAL_LEN" -c "mww 0x40003000 0xAAAA" -c resume -c shutdown >/dev/null
if cmp -s /tmp/calpage_before.bin /tmp/calpage_after.bin; then
  echo "   ✅ 校准页逐字节未变（$CAL，2KB）—— 校准数据安全"
else
  echo "   ❌❌❌ 校准页被改变了！$CAL 不再等于烧前的备份"
  echo "        烧前: $(sha256sum /tmp/calpage_before.bin | cut -c1-16)"
  echo "        烧后: $(sha256sum /tmp/calpage_after.bin  | cut -c1-16)"
  exit 2
fi
