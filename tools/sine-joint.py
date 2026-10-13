#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sine-joint.py —— 整转联合拟合：每相用它在一个窗口里的【全部悬空弧】一起拟合一根正弦。

为什么（排雷）：
  · 对只有 24° 的弧做拟合，幅值与直流强相关 ⇒ 两者解耦不开 ⇒ 前面那些"每相幅值 657/491/791、
    A 相直流 −259/−314"大概率是拟合假象。
  · 每相在一个窗口(21.3ms≈3.4 转)里有 2 段悬空弧、共约 120°/转 ⇒ 联合拟合条件数好得多。
  · ω 逐窗口扫频拟合（不用平均步长）；时间原点统一用 ADC 全局帧号。

图上标出：换相点(灰点线) / 实测过零(各相彩色○) / **理想过零=步中点(黑▲)** / 各相拟合正弦(线)
用法: python3 tools/sine-joint.py <win.npz> [输出png]
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
OUT = sys.argv[2] if len(sys.argv) > 2 else NPZ.replace('_win.npz', '_整转拟合.png')
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
PHK = {'A': 'va', 'B': 'vb', 'C': 'vc'}
COL = {'A': '#c0392b', 'B': '#2471a3', 'C': '#27ae60'}
FS = 24000.0
WLO, WHI = 1600.0, 2200.0

z = np.load(NPZ)
ST, V0HW, TS = z['step'], z['v0'], z['ts'].astype(np.int64)
PH = {k: z[k] for k in ('va', 'vb', 'vc')}


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


WIN = []
for wi in range(len(ST)):
    sg = [(i0, i1, s) for i0, i1, s in segs(ST[wi])
          if s in F and i0 != 0 and i1 != 512 and i1 - i0 >= 12]
    if not sg:
        continue
    if not (WLO <= np.median([(i1 - i0) * 83.333 for i0, i1, s in sg]) <= WHI):
        continue
    WIN.append((wi, sg))
print('可用窗口 %d' % len(WIN))


def ls(w, t, v):
    M = np.column_stack([np.sin(w * t), np.cos(w * t), np.ones_like(t)])
    c, *_ = np.linalg.lstsq(M, v, rcond=None)
    return c, float(np.sqrt(np.mean((v - M @ c) ** 2)))


# ---- 逐窗口：① 扫频定 ω  ② 每相联合拟合（整窗口全部弧）----
RES = []
for wi, sg in WIN:
    per = np.mean([(i1 - i0) for i0, i1, s in sg]) / FS
    f0 = 1 / (6 * per)
    best = None
    for sgn in (+1, -1):
      for f in np.arange(f0 * 0.9, f0 * 1.1, 0.02):
        w = sgn * 2 * np.pi * f; tot = 0.0; n = 0
        for i0, i1, s in sg:
            a = i0 + int((i1 - i0) * 0.3); b = i0 + int((i1 - i0) * 0.7)
            if b - a < 6: continue
            t = (TS[wi] + np.arange(a, b)) / FS
            v = PH[PHK[F[s]]][wi][a:b].astype(float) - V0HW[wi][a:b]
            _, r = ls(w, t, v); tot += r * r; n += 1
        if n and (best is None or tot / n < best[0]): best = (tot / n, f, sgn)
    w = best[2] * 2 * np.pi * best[1]
    print('   窗口#%d: ω 取 %s%.1f Hz（残差 %.1f）' % (int(z['seq'][wi]), '+' if best[2] > 0 else '-', best[1], np.sqrt(best[0])))
    e = dict(wi=wi, w=w, sg=sg, ph={})
    for p in 'ABC':
        tt = []; vv = []
        for i0, i1, s in sg:
            if F[s] != p: continue
            a = i0 + int((i1 - i0) * 0.3); b = i0 + int((i1 - i0) * 0.7)
            if b - a < 6: continue
            tt.append((TS[wi] + np.arange(a, b)) / FS)
            vv.append(PH[PHK[p]][wi][a:b].astype(float) - V0HW[wi][a:b])
        if len(tt) < 2:                     # 少于两段弧 ⇒ 不拟合（避开条件数差的病态）
            e['ph'][p] = None; continue
        t = np.concatenate(tt); v = np.concatenate(vv)
        c, r = ls(w, t, v)
        e['ph'][p] = dict(A=float(np.hypot(c[0], c[1])), C=float(c[2]),
                          phi=float(np.arctan2(c[1], c[0])), rms=r, n=len(v))
    RES.append(e)

print('\n== 整转联合拟合：每相参数（中位）==')
Ag = {p: [] for p in 'ABC'}; Cg = {p: [] for p in 'ABC'}; Pg = {p: [] for p in 'ABC'}
for e in RES:
    for p in 'ABC':
        if e['ph'][p]:
            Ag[p].append(e['ph'][p]['A']); Cg[p].append(e['ph'][p]['C']); Pg[p].append(e['ph'][p]['phi'])
A = {p: float(np.median(Ag[p])) for p in 'ABC'}
C = {p: float(np.median(Cg[p])) for p in 'ABC'}
Ph = {p: float(np.median(Pg[p])) for p in 'ABC'}
Aref = float(np.mean(list(A.values())))
for p in 'ABC':
    print('   %s: 幅值 %6.0f 码   直流 %+8.1f 码   增益系数 %.3f   （拟合段数 %d）'
          % (p, A[p], C[p], Aref / A[p], len(Ag[p])))
print('   ⇒ 幅值比  A/C=%.2f  B/C=%.2f' % (A['A'] / A['C'], A['B'] / A['C']))
for p, k in (('B', -120), ('C', -240)):
    d = np.degrees(np.median([((e['ph'][p]['phi'] - e['ph']['A']['phi']) % (2 * np.pi))
                              for e in RES if e['ph'][p] and e['ph']['A']])) 
    print('   ⇒ %s − A 的相位差 = %.1f°（理想 %.0f°）' % (p, d, k % 360))

# ---- 图：挑两个窗口，画数据(只在悬空弧) + 拟合正弦 + 换相点 + 实测过零 + 理想过零(步中点) ----
sel = RES[len(RES) // 2:len(RES) // 2 + 2]
fig, axes = plt.subplots(len(sel), 1, figsize=(16, 5.0 * len(sel)))
if len(sel) == 1: axes = [axes]
for ax, e in zip(axes, sel):
    wi, w, sg = e['wi'], e['w'], e['sg']
    t0 = TS[wi]
    for p in 'ABC':
        for i0, i1, s in sg:
            if F[s] != p: continue
            x = (np.arange(i0, i1)) / FS * 1e3
            ax.plot(x, PH[PHK[p]][wi][i0:i1].astype(float) - V0HW[wi][i0:i1],
                    '.', ms=2.2, color=COL[p], alpha=.45)
        f = e['ph'][p]
        if f:
            tt = (t0 + np.arange(0, 512)) / FS
            ax.plot(np.arange(512) / FS * 1e3,
                    f['A'] * np.sin(w * tt + f['phi']) + f['C'], '-', lw=1.2, color=COL[p])
    # 换相点 + 步中点 + 实测过零
    for i0, i1, s in sg:
        ax.axvline(i0 / FS * 1e3, color='0.55', ls=':', lw=.9)
        ax.plot([i0 / FS * 1e3], [520], 'k^', ms=5)          # 理想过零（步中点）
    ax.set_ylim(-560, 620); ax.grid(alpha=.22)
    ax.set_ylabel('悬空相 − V0 (码)')
    ax.set_title('窗口 #%d：彩点=实测(仅该相悬空时)  彩线=整转联合拟合正弦  '
                 '灰点线=换相点  黑▲=理想过零(步中点)' % int(z['seq'][wi]), fontsize=10)
axes[-1].set_xlabel('窗口内时间 (ms)')
plt.tight_layout(); plt.savefig(OUT, dpi=125)
print('\n图 →', OUT)
