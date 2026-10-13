#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
offline-fix.py —— 离线算一板参数，并对比【修正前 / 修正后】的过零点检测。

思路（用户方案）：开环稳定段 → ① 每步幅值归一化（增益表）→ ② 再算每步竖直偏置 → ③ 看
过零位置 p 是否被"卡"到同一条线上（六步一致）。p = 过零落在步内的位置（0=换相点，1=下一次）。

产出：
  · 控制台：每相/每步的增益与偏置表；p 的逐步对比 + 散布
  · 图：每个步一段（3 个窗口），画【原始 delta(灰)】与【修正后 delta(黑)】，
        标出原始过零(红虚线)与修正后过零(绿实线)，并标每步的 p
用法: python3 tools/offline-fix.py <baseline_win.npz> [输出png]
"""
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
for _f in ('Noto Sans CJK SC', 'Noto Sans CJK JP'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family'] = _f; break
plt.rcParams['axes.unicode_minus'] = False

NPZ = sys.argv[1]
OUT = sys.argv[2] if len(sys.argv) > 2 else NPZ.replace('_win.npz', '_离线修正对比.png')
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
TPS = 1000.0 / 24000.0 * 2000.0            # 拍/采样
WLO, WHI = 1600.0, 2200.0                  # 开环稳定段（分割外生 ⇒ 测量干净）

z = np.load(NPZ)
ST, V0 = z['step'], z['v0']
PH = {'A': z['va'], 'B': z['vb'], 'C': z['vc']}


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


def rob(d):
    s = np.sort(d)
    return (s[-2] - s[1]), (s[-2] + s[1]) / 2.0


# ---------- 收集（开环稳定段）----------
rec = []          # (wi, i0, i1, step, d_raw, raw_pp_per_phase)
for wi in range(len(ST)):
    for i0, i1, s in segs(ST[wi]):
        if s not in F or i0 == 0 or i1 == 512 or i1 - i0 < 14:
            continue
        w = (i1 - i0) * TPS
        if not (WLO <= w <= WHI):
            continue
        d = PH[F[s]][wi][i0:i1].astype(float) - V0[wi][i0:i1]
        rec.append((wi, i0, i1, s, d))

# 每相"纯通道"倍率（用原始悬空读数峰峰）
rawph = {'A': [], 'B': [], 'C': []}
for wi, i0, i1, s, d in rec:
    v = PH[F[s]][wi][i0:i1].astype(float)
    pp, _ = rob(v[3:-3]); rawph[F[s]].append(pp)
gC = float(np.median(rawph['C']))
gain_ch = {p: float(np.median(rawph[p])) / gC for p in 'ABC'}
print('== 每相纯通道倍率（原始悬空读数峰峰，相对 C）==')
for p in 'ABC':
    print('   %s: %.3f' % (p, gain_ch[p]))

# 每步幅值（原始 delta）→ 增益表
amp = {s: [] for s in range(1, 7)}
off = {s: [] for s in range(1, 7)}
for wi, i0, i1, s, d in rec:
    a, o = rob(d[3:-3]); amp[s].append(a); off[s].append(o)
Amed = {s: float(np.median(amp[s])) for s in range(1, 7)}
Aref = float(np.median(list(Amed.values())))
gain = {s: Aref / Amed[s] for s in range(1, 7)}
print('\n== 增益表（每步幅值归一，参考=六步中位 %.0f 码）==' % Aref)
print('   step  幅值(码)  增益')
for s in range(1, 7):
    print('    %d    %6.0f   %.3f' % (s, Amed[s], gain[s]))

# ② 先增益归一，再重测偏置
off2 = {s: [] for s in range(1, 7)}
for wi, i0, i1, s, d in rec:
    d1 = d * gain[s]
    _, o = rob(d1[3:-3]); off2[s].append(o)
OFF = {s: float(np.median(off2[s])) for s in range(1, 7)}
print('\n== 偏置表（在【归一化后】的 delta 上测）==')
print('   step  偏置(码)')
for s in range(1, 7):
    print('    %d   %+7.1f' % (s, OFF[s]))


def p_of(d, skip=3):
    dd = d[skip:-skip] if len(d) > 2 * skip else d
    for k in range(1, len(dd)):
        if dd[k - 1] * dd[k] <= 0 and dd[k - 1] != dd[k]:
            return (k + skip) / float(len(d))
    return np.nan


# ③ 对比 p：原始 / 只归一化 / 归一化+偏置
P0 = {s: [] for s in range(1, 7)}
P1 = {s: [] for s in range(1, 7)}
P2 = {s: [] for s in range(1, 7)}
for wi, i0, i1, s, d in rec:
    P0[s].append(p_of(d))
    d1 = d * gain[s]
    P1[s].append(p_of(d1))
    P2[s].append(p_of(d1 - OFF[s]))
print('\n== 过零位置 p（0.5=步正中）==')
print('   step 浮相   原始p   只归一化   归一化+偏置')
sp0, sp1, sp2 = [], [], []
for s in range(1, 7):
    a, b, c = (np.nanmedian(P0[s]), np.nanmedian(P1[s]), np.nanmedian(P2[s]))
    sp0.append(a); sp1.append(b); sp2.append(c)
    print('    %d   %s  %7.3f  %8.3f  %10.3f' % (s, F[s], a, b, c))
print('   → 六步 p 散布(σ): 原始 %.3f   只归一化 %.3f   **归一化+偏置 %.3f**'
      % (np.nanstd(sp0), np.nanstd(sp1), np.nanstd(sp2)))
print('   → 六步 p 均值    : 原始 %.3f   只归一化 %.3f   归一化+偏置 %.3f'
      % (np.nanmean(sp0), np.nanmean(sp1), np.nanmean(sp2)))

# ---------- 画图：3 个窗口，原始 vs 修正后 ----------
wins = sorted(set(r[0] for r in rec))
pick = [wins[int(len(wins) * f)] for f in (0.3, 0.55, 0.8)]
fig, axes = plt.subplots(len(pick), 1, figsize=(16, 4.2 * len(pick)), sharex=False)
if len(pick) == 1:
    axes = [axes]
for ax, wi in zip(axes, pick):
    t = np.arange(512) * (1000.0 / 24000.0)
    for i0, i1, s in segs(ST[wi]):
        if s not in F or (i1 - i0) < 20:
            continue
        d = PH[F[s]][wi][i0:i1].astype(float) - V0[wi][i0:i1]
        x = t[i0:i1]
        ax.plot(x, d, lw=.9, color='0.65', label='原始 delta' if s == 1 else None)
        d1 = d * gain[s]
        d2 = d1 - OFF[s]
        ax.plot(x, d2, lw=1.3, color='k', label='修正后 delta' if s == 1 else None)
        k0 = p_of(d)
        if k0 == k0 and int(k0 * len(d)) < len(x):
            ax.axvline(x[int(k0 * len(d))], color='#c0392b', ls='--', lw=1.0)
        k2 = p_of(d2)
        if k2 == k2 and int(k2 * len(d)) < len(x):
            ax.axvline(x[int(k2 * len(d))], color='#27ae60', ls='-', lw=1.4)
        ax.axvline(x[0], color='#2471a3', ls=':', lw=.9)
        if k0 == k0:
            ax.text(x[2], 430, 'p %.2f→%.2f' % (k0, k2), fontsize=7.5)
    ax.axhline(0, color='gray', lw=.7)
    ax.set_ylim(-520, 520); ax.grid(alpha=.22)
    ax.set_ylabel('delta (码)')
    ax.set_title('窗口 #%d：灰=原始 delta，黑=修正后(增益归一+偏置)；红虚线=原始过零，绿实线=修正后过零，蓝点线=换相点'
                 % int(z['seq'][wi]), fontsize=10)
    ax.legend(fontsize=8, loc='upper right')
axes[-1].set_xlabel('窗口内时间 (ms) —— 一片 = 512 点 @24kHz = 21.33ms')
plt.tight_layout(); plt.savefig(OUT, dpi=125)
print('\n图 →', OUT)
