#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ao-view.py —— 自适应电压偏移的结果图（跑完一轮后画）。

用法: python3 tools/ao-view.py [标签]
数据来源：SWD 读 ao_off/ao_chg/ao_nozc/ao_err（复用 ao-dump 的读法，不占串口）。
对照项（本板实测常数，见 一切为了校准/03 §十九）：
  · 每步【半幅 A_s】= 可见性上限：偏移超过它，交点被推出弧线 ⇒ 该段整段无过零
      A_s = {—,145, 86,111,142,139,115} 码（106 个闭环窗量出）
  · 老表的偏移 so = {—,−36,+61,−45,+41,−60,+42} 码（±60 限幅，未超 A_s 但远不够修相位）
  · 按误差折算的【应补量】= {—,−75,+26,−68,+44,−230,+15} 码（理想过零 0.500 反推）

四栏：① ao_off vs A_s 上限 vs 老表 vs 应补量  ② 改动次数（收敛判据）
      ③ 最近误差 + 死区  ④ 无过零次数（可见性受损）
"""
import os, re, subprocess, sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
for _f in ('Noto Sans CJK SC', 'Noto Sans CJK JP'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family'] = _f
        break
plt.rcParams['axes.unicode_minus'] = False

REPO = '/home/lpl/桌面/RM2027/AM32-rm2027'
ELF = os.path.join(REPO, 'obj/AM32_RM2027_ESC_G071_2.21.elf')
TAG = sys.argv[1] if len(sys.argv) > 1 else ''
AS = {1: 145, 2: 86, 3: 111, 4: 142, 5: 139, 6: 115}      # 每步半幅（可见性上限）
OLD = {1: -36, 2: 61, 3: -45, 4: 41, 5: -60, 6: 42}      # 老表
NEED = {1: -75, 2: 26, 3: -68, 4: 44, 5: -230, 6: 15}    # 按误差折算的应补量
DEAD = 120

os.chdir(REPO)
nm = subprocess.run(['arm-none-eabi-nm', '-S', ELF], capture_output=True, text=True).stdout
sym = {}
for l in nm.splitlines():
    f = l.split()
    if len(f) == 4:
        sym[f[3]] = int(f[0], 16)
if 'ao_off' not in sym:
    print('❌ ELF 里没有 ao_off'); sys.exit(1)


def rd(name, words):
    o = subprocess.run(['openocd', '-f', 'interface/stlink.cfg', '-f', 'target/stm32g0x.cfg',
                        '-c', 'adapter speed 1800', '-c', 'init', '-c', 'halt',
                        '-c', 'mww 0x40003000 0xAAAA', '-c', 'mdw %#x %d' % (sym[name], words),
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


off = s16(rd('ao_off', 4))[1:7]
chg = u16(rd('ao_chg', 4))[1:7]
noz = u16(rd('ao_nozc', 4))[1:7]
err = s16(rd('ao_err', 4))[1:7]
x = np.arange(6)
lbl = [f'step{s}' for s in range(1, 7)]

fig, ax = plt.subplots(1, 4, figsize=(21, 5))
# ① 偏移 vs 上限
a = ax[0]
a.bar(x - 0.3, off, 0.2, color='#c0392b', label='回路学到 ao_off')
a.bar(x - 0.1, [OLD[s] for s in range(1, 7)], 0.2, color='#7f8c8d', label='老表 so')
a.bar(x + 0.1, [NEED[s] for s in range(1, 7)], 0.2, color='#2471a3', label='按误差折算应补量')
for i in range(6):
    s = i + 1
    a.fill_between([i - 0.45, i + 0.35], -AS[s], AS[s], color='#f9e79f', alpha=.55, zorder=0)
    a.axhline(0, color='gray', lw=.8)
a.set_xticks(x); a.set_xticklabels(lbl)
a.set_ylabel('偏移（码）')
a.set_title('① 偏移量 vs 【可见性上限 ±A_s】(黄带)\n超出去 = 交点被推出弧线 = 整段无过零', fontsize=10)
a.legend(fontsize=8); a.grid(alpha=.25, axis='y')
# ② 改动次数
a = ax[1]
a.bar(x, chg, 0.55, color='#8e44ad')
for i, v in enumerate(chg):
    a.text(i, v + 0.5, str(v), ha='center', fontsize=9)
a.set_xticks(x); a.set_xticklabels(lbl); a.set_ylabel('改动次数')
a.set_title(f'② 改动次数（收敛判据）\n合计 {sum(chg)} 次 —— 收敛后应停住', fontsize=10)
a.grid(alpha=.25, axis='y')
# ③ 误差
a = ax[2]
a.bar(x, err, 0.55, color='#e67e22')
a.axhspan(-DEAD, DEAD, color='#d5f5e3', label=f'死区 ±{DEAD} 拍')
a.axhline(0, color='gray', lw=.8)
a.set_xticks(x); a.set_xticklabels(lbl); a.set_ylabel('最近误差（拍，正=过零偏晚）')
a.set_title('③ 误差仍在死区之外 ⇒ 没收敛', fontsize=10)
a.legend(fontsize=8); a.grid(alpha=.25, axis='y')
# ④ 无过零
a = ax[3]
a.bar(x, noz, 0.55, color='#922b21')
a.set_xticks(x); a.set_xticklabels(lbl); a.set_ylabel('整段无过零次数')
a.set_title(f'④ 可见性受损：无过零合计 {sum(noz)} 次\n（基线该类事件是每步 2~18/330）', fontsize=10)
a.grid(alpha=.25, axis='y')
fig.suptitle(f'自适应电压偏移 · 第 1 轮结果 {TAG}   （test-08 @ 4acaf20，闭环 6s）', fontsize=13)
plt.tight_layout(rect=[0, 0, 1, 0.92])
o = os.path.join('/home/lpl/桌面/RM2027', '自适应偏移_' + (TAG or 'run1').replace(' ', '_') + '.png')
plt.savefig(o, dpi=125)
print('图 →', o)
