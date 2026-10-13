#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
win-compare.py —— 【两种补偿方式】的全速多窗口时序对比图。

用法:
  python3 tools/win-compare.py <npzA> <i0,i1,i2> <标签A> <npzB> <i0,i1,i2> <标签B> <输出png>

上三片 = A 方式，下三片 = B 方式；每片 512 点 @24kHz = 21.33ms 连续全速。
图上：红/蓝/绿=VA/VB/VC，紫虚线=V0，黑粗线=delta(悬空相−V0)，
      绿竖线=从波形认到的过零，彩色虚线=换相点（颜色按 step 号），段内小字=N 浮X。
"""
import sys
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

DT = 1000.0 / 24000.0
FLOAT = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
SC = {1: '#c0392b', 2: '#d35400', 3: '#b7950b', 4: '#27ae60', 5: '#8e44ad', 6: '#2471a3'}

A_NPZ, A_IDX, A_LAB = sys.argv[1], sys.argv[2], sys.argv[3]
B_NPZ, B_IDX, B_LAB = sys.argv[4], sys.argv[5], sys.argv[6]
OUT = sys.argv[7]


def segs(st):
    out, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            out.append((i0, k, int(st[i0])))
            i0 = k
    out.append((i0, len(st), int(st[i0])))
    return out


def zc(d, skip=3):
    for k in range(max(skip, 1), len(d)):
        if d[k - 1] * d[k] <= 0 and d[k - 1] != d[k]:
            return k
    return None


def panel(ax, npz, wi, tag):
    z = np.load(npz)
    va, vb, vc, v0, st = z['va'][wi], z['vb'][wi], z['vc'][wi], z['v0'][wi], z['step'][wi]
    ph = {'A': va, 'B': vb, 'C': vc}
    x = np.arange(512) * DT
    for y, c, lb in ((va, '#c0392b', 'VA'), (vb, '#2471a3', 'VB'),
                     (vc, '#27ae60', 'VC'), (v0, '#8e44ad', 'V0')):
        ax.plot(x, y, lw=.7, color=c, alpha=.85, label=lb, ls='--' if lb == 'V0' else '-')
    sg = segs(st)
    nzc = 0
    for i0, i1, s in sg:
        if s not in FLOAT:
            continue
        d = ph[FLOAT[s]][i0:i1].astype(float) - v0[i0:i1]
        ax.plot(x[i0:i1], d, lw=1.5, color='k', alpha=.75)
        ax.axvline(x[i0], color=SC.get(s, 'gray'), ls='--', lw=1.0, alpha=.85)
        ax.text(x[i0] + 0.15, 1240, f'{s} 浮{FLOAT[s]}', fontsize=7.5, color=SC.get(s, 'gray'))
        k = zc(d)
        if k is not None:
            nzc += 1
            ax.axvline(x[i0 + k], color='#0b6623', ls='-', lw=1.2, alpha=.9)
    full = [((i1 - i0) * DT) for i0, i1, _s in sg if i0 != 0 and i1 != 512]
    cv = np.std(full) / np.mean(full) * 100 if len(full) > 2 else float('nan')
    body = ' '.join(f'{(i1-i0)*DT:.2f}' for i0, i1, _s in sg)
    ax.set_ylim(-60, 1360)
    ax.set_ylabel('ADC 码')
    ax.grid(alpha=.22)
    ax.legend(fontsize=7, ncol=5, loc='lower right')
    ax.set_title(f'【{tag}】窗口 #{int(z["seq"][wi])}（收到第 {wi+1}/{len(z["seq"])} 片）   '
                 f'步长(ms): {body}   散布 {cv:.0f}%   认到过零 {nzc}/{len(sg)}', fontsize=10)


idxa = [int(v) for v in A_IDX.split(',')]
idxb = [int(v) for v in B_IDX.split(',')]
fig, axes = plt.subplots(len(idxa) + len(idxb), 1,
                         figsize=(17, 3.0 * (len(idxa) + len(idxb))), sharex=True)
n = 0
for wi in idxa:
    panel(axes[n], A_NPZ, wi, A_LAB); n += 1
for wi in idxb:
    panel(axes[n], B_NPZ, wi, B_LAB); n += 1
axes[-1].set_xlabel('窗口内时间 (ms) —— 一片 = 512 点 @24kHz = 21.33ms 连续全速')
plt.tight_layout()
plt.savefig(OUT, dpi=125)
print(f'图 → {OUT}')
