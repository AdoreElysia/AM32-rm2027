#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
offline-fix.py（v2，稳健口径）—— 离线参数表 + 修正前后过零位置对比（画在数据上）。

★ v1 的问题（已作废）：用"峰峰值"量幅值 —— 但一个步内 delta 是【整段斜坡】，
  峰峰值取决于纳入多长的斜坡，而每步换相时刻本来就偏得不一样 ⇒ 幅值被相位污染
  （换统计量/换掐头去尾，表就变 10~20%）。
★ v2 改成【对斜坡做线性拟合】取两个稳健量：
    斜率 |k|      → 增益表（k_ref/|k|）
    拟合零点 p    → 偏置修正量 = −k·(0.5−p)·w（把过零搬到步正中所需的竖直平移）
  拟合只用段中间 40%（两端各去 30%）⇒ 天然避开换相瞬态 ✓

⚠ 按用户要求 waitTime 不动 ⇒ 偏置修正必须【减掉六步共模】（只保留步间差异），
  这样六步相对相位拉齐、整体相位不变（waitTime 表仍成立）。

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
WLO, WHI = 1600.0, 2200.0
FRAC = 0.30

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


def fit(v, frac=FRAC):
    n = len(v); a = int(n * frac); b = n - a
    if b - a < 8:
        return None
    x = np.arange(a, b, dtype=float)
    k, b0 = np.polyfit(x, v[a:b], 1)
    if abs(k) < 1e-6:
        return None
    return k, -b0 / k / n          # k(码/采样, 带符号), p(段内相对位置)


# ---------- ① 参数表 ----------
S = {s: [] for s in range(1, 7)}
for wi in range(len(ST)):
    for i0, i1, s in segs(ST[wi]):
        if s not in F or i0 == 0 or i1 == 512 or i1 - i0 < 24:
            continue
        if not (WLO <= (i1 - i0) * 83.333 <= WHI):
            continue
        v = PH[F[s]][wi][i0:i1].astype(float) - V0[wi][i0:i1]
        r = fit(v)
        if r:
            S[s].append((r[0], r[1], i1 - i0))

kabs = {s: float(np.median([abs(k) for k, _, _ in S[s]])) for s in range(1, 7)}
kref = float(np.median(list(kabs.values())))
gain = {s: kref / kabs[s] for s in range(1, 7)}
# ★ 关键：减共模必须在【p 域】做 —— 目标取"六步 p 的共同值 pbar"（而不是 0.5），
#   否则每步 k·w 不同 ⇒ 减掉同样的码数并不等于相同的 p 位移 ⇒ 六步落不到一条线上。
pbar = float(np.mean([np.median([pp for _, pp, _ in S[s]]) for s in range(1, 7)]))
craw = {s: float(np.median([-k * (0.5 - pp) * n for k, pp, n in S[s]])) for s in range(1, 7)}
corr = {s: float(np.median([-k * (pbar - pp) * n for k, pp, n in S[s]])) for s in range(1, 7)}
cmean = float(np.mean(list(craw.values())))

print('== ① 增益表（斜率口径，k_ref=%.2f 码/采样）==' % kref)
print('   step   |k|     增益')
for s in range(1, 7):
    print('    %d   %6.2f  %.3f' % (s, kabs[s], gain[s]))
print('\n== ② 偏置修正（目标：把六步 p 都搬到共同值 pbar=%.3f，整体相位不动）==' % pbar)
print('   （对照）搬到 p=0.5 的平移: ' + ' '.join('%+7.1f' % craw[s] for s in range(1, 7)))
print('   实际采用(减共模/保相位):   ' + ' '.join('%+7.1f' % corr[s] for s in range(1, 7)))
print('   （step1..6 对应浮相 C A B C A B）')


def p_after(s, p, k, n_):
    kk = k * gain[s]
    if abs(kk) < 1e-9:
        return np.nan
    return p - corr[s] / (kk * n_)


print('\n== ③ 过零位置 p（斜率拟合口径）==')
print('   step 浮相   修正前p   修正后p   样本')
sp0, sp1 = [], []
for s in range(1, 7):
    a = float(np.median([p for _, p, _ in S[s]]))
    b = float(np.median([p_after(s, p, k, n_) for k, p, n_ in S[s]]))
    sp0.append(a); sp1.append(b)
    print('    %d   %s   %7.3f  %8.3f  %5d' % (s, F[s], a, b, len(S[s])))
print('   → 六步 p 散布(σ): 修正前 %.3f   **修正后 %.3f**' % (np.std(sp0), np.std(sp1)))
print('   → 六步 p 均值    : 修正前 %.3f   修正后 %.3f   （减共模 ⇒ 均值应基本不变）'
      % (np.mean(sp0), np.mean(sp1)))

# ---------- ④ 画图 ----------
wins = sorted(set(seg[0] for wi in range(len(ST)) for seg in [None] if False)) or []
wins = []
for wi in range(len(ST)):
    if any((i1 - i0) >= 20 for i0, i1, s in segs(ST[wi]) if s in F):
        wins.append(wi)
pick = wins[int(len(wins) * 0.35):int(len(wins) * 0.35) + 2]
fig, axes = plt.subplots(len(pick), 1, figsize=(16, 4.4 * len(pick)))
if len(pick) == 1:
    axes = [axes]
for ax, wi in zip(axes, pick):
    t = np.arange(512) * (1000.0 / 24000.0)
    for i0, i1, s in segs(ST[wi]):
        if s not in F or (i1 - i0) < 12:
            continue
        d = PH[F[s]][wi][i0:i1].astype(float) - V0[wi][i0:i1]
        x = t[i0:i1]
        d2 = d * gain[s] - corr[s]
        ax.plot(x, d, lw=1.0, color='0.62', label='原始 delta' if s == 1 else None)
        ax.plot(x, d2, lw=1.4, color='k', label='修正后（增益+去共模偏置）' if s == 1 else None)
        r0 = fit(d); r2 = fit(d2)
        if r0 and 0 <= int(r0[1] * len(d)) < len(x):
            ax.axvline(x[int(r0[1] * len(d))], color='#c0392b', ls='--', lw=1.0)
        if r2 and 0 <= int(r2[1] * len(d2)) < len(x):
            ax.axvline(x[int(r2[1] * len(d2))], color='#27ae60', ls='-', lw=1.5)
        ax.axvline(x[0], color='#2471a3', ls=':', lw=.9)
        if r0 and r2:
            ax.text(x[1], 460, 'p %.2f→%.2f' % (r0[1], r2[1]), fontsize=7)
    ax.axhline(0, color='gray', lw=.7)
    ax.set_ylim(-560, 560); ax.grid(alpha=.22); ax.set_ylabel('delta (码)')
    ax.set_title('窗口 #%d  灰=原始 黑=修正后  红虚线=原始过零 绿实线=修正后过零  蓝点线=换相点'
                 % int(z['seq'][wi]), fontsize=10)
    ax.legend(fontsize=8, loc='upper right')
axes[-1].set_xlabel('窗口内时间 (ms) —— 512 点 @24kHz = 21.33ms 连续全速')
plt.tight_layout(); plt.savefig(OUT, dpi=125)
print('\n图 →', OUT)
