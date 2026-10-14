#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ao-dump.py —— 读【自适应电压偏移】的运行时状态（走 SWD，不占串口）。

用法: python3 tools/ao-dump.py [标签]
读 4 个数组（都定义在 Src/main.c，仅 RM2027_AO_ADAPT=1 时存在）：
  ao_off[7]   每步当前偏移（码，d' = d + ao_off[step]）
  ao_chg[7]   每步偏移被改过几次  ← ★收敛判据：收敛后就不再涨
  ao_nozc[7]  每步"整段无过零"次数 ← 可见性判据
  ao_err[7]   最近一次误差（拍，正 = 过零偏晚；死区 RM2027_AO_DEAD）
注意：openocd 的 mdw 数据行写在 **stderr** 上 ⇒ stdout+stderr 都要扫。
"""
import os, re, subprocess, sys

REPO = '/home/lpl/桌面/RM2027/AM32-rm2027'
ELF = os.path.join(REPO, 'obj/AM32_RM2027_ESC_G071_2.21.elf')
os.chdir(REPO)
TAG = sys.argv[1] if len(sys.argv) > 1 else ''

nm = subprocess.run(['arm-none-eabi-nm', '-S', ELF], capture_output=True, text=True).stdout
sym = {}
for l in nm.splitlines():
    f = l.split()
    if len(f) == 4:
        sym[f[3]] = int(f[0], 16)

if 'ao_off' not in sym:
    print('❌ ELF 里没有 ao_off ⇒ 这一版没开 RM2027_AO_ADAPT（或不是 test-08）'); sys.exit(1)


def rd(name, words):
    o = subprocess.run(['openocd', '-f', 'interface/stlink.cfg', '-f', 'target/stm32g0x.cfg',
                        '-c', 'adapter speed 1800', '-c', 'init', '-c', 'halt',
                        '-c', 'mww 0x40003000 0xAAAA',
                        '-c', 'mdw %#x %d' % (sym[name], words),
                        '-c', 'mww 0x40003000 0xAAAA', '-c', 'resume', '-c', 'shutdown'],
                       capture_output=True, text=True)
    v = []
    for l in (o.stdout + o.stderr).splitlines():
        m = re.match(r'^0x[0-9a-fA-F]+:\s*(.*)$', l)
        if m:
            v += [int(x, 16) for x in m.group(1).split()]
    return v


def u16(ws):
    out = []
    for w in ws:
        out += [w & 0xFFFF, (w >> 16) & 0xFFFF]
    return out


def s16(ws):
    return [x - 0x10000 if x & 0x8000 else x for x in u16(ws)]


off = s16(rd('ao_off', 4))[:7]
chg = u16(rd('ao_chg', 4))[:7]
noz = u16(rd('ao_nozc', 4))[:7]
err = s16(rd('ao_err', 4))[:7]
print(f'=== 自适应电压偏移状态 {TAG} ===')
print(f'{"step":>5}{"ao_off(码)":>12}{"改过几次":>10}{"无过零次数":>11}{"最近误差(拍)":>13}')
for s in range(1, 7):
    print(f'{s:5d}{off[s]:12d}{chg[s]:10d}{noz[s]:11d}{err[s]:13d}')
print(f'  合计：改动 {sum(chg[1:7])} 次   无过零 {sum(noz[1:7])} 次')
