#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""sine-ideal.py（v2，极简）—— 逐窗口扫频定 ω → 逐段拟合 (A,C,φ) → 段内求零点得 p。
对比三种 V0：硬件星形 / 软件均值 / 软件均值+通道级修正。只用"保正弦"的操作。"""
import sys
import numpy as np

NPZ = sys.argv[1]
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
PHK = {'A': 'va', 'B': 'vb', 'C': 'vc'}
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


# 收集窗口
WIN = []
for wi in range(len(ST)):
    sg = [(i0, i1, s) for i0, i1, s in segs(ST[wi]) if s in F and i0 != 0 and i1 != 512 and i1 - i0 >= 12]
    if not sg: continue
    if not (WLO <= np.median([(i1 - i0) * 83.333 for i0, i1, s in sg]) <= WHI): continue
    WIN.append((wi, sg))
print('可用窗口 %d' % len(WIN))


def sig(wi, i0, i1, chin, v0):
    a = i0 + int((i1 - i0) * 0.30); b = i0 + int((i1 - i0) * 0.70)
    return a, b


def lsfit(w, t, v):
    M = np.column_stack([np.sin(w * t), np.cos(w * t), np.ones_like(t)])
    c, *_ = np.linalg.lstsq(M, v, rcond=None)
    return c, v - M @ c


# ① 逐窗口定 ω（扫频，用三相总残差）
om = {}
for wi, sg in WIN:
    per = np.mean([(i1 - i0) for i0, i1, s in sg]) / FS
    f0 = 1.0 / (6 * per)
    best = None
    for f in np.arange(f0 * 0.9, f0 * 1.1, 0.02):
        w = 2 * np.pi * f; tot = 0.0; n = 0
        for i0, i1, s in sg:
            a, b = sig(wi, i0, i1, s, None)
            if b - a < 6: continue
            t = (TS[wi] + np.arange(a, b)) / FS
            v = PH[PHK[F[s]]][wi][a:b].astype(float) - V0HW[wi][a:b]
            _, r = lsfit(w, t, v); tot += float(np.mean(r ** 2)); n += 1
        if n and (best is None or tot / n < best[0]): best = (tot / n, f)
    om[wi] = 2 * np.pi * best[1]
print('ω 已逐窗口拟合（%.0f~%.0f Hz）' % (min(om.values()) / 2 / np.pi, max(om.values()) / 2 / np.pi))

# ② 每相通道级参数（用硬件 V0 时）
acc = {p: [] for p in 'ABC'}
for wi, sg in WIN:
    w = om[wi]
    for i0, i1, s in sg:
        a, b = sig(wi, i0, i1, s, None)
        if b - a < 6: continue
        t = (TS[wi] + np.arange(a, b)) / FS
        v = PH[PHK[F[s]]][wi][a:b].astype(float) - V0HW[wi][a:b]
        c, _ = lsfit(w, t, v)
        acc[F[s]].append((float(np.hypot(c[0], c[1])), float(c[2])))
A = {p: float(np.median([x[0] for x in acc[p]])) for p in 'ABC'}
C = {p: float(np.median([x[1] for x in acc[p]])) for p in 'ABC'}
Aref = float(np.mean(list(A.values())))
print('\n== ① 每相通道级参数 ==')
for p in 'ABC':
    print('   %s: 幅值 %6.0f 码  直流 %+8.1f 码   增益系数 %.3f' % (p, A[p], C[p], Aref / A[p]))
print('   ⇒ 幅值比 A/C = %.2f  B/C = %.2f （B 最弱）' % (A['A'] / A['C'], A['B'] / A['C']))


def run(v0mode, corr):
    """返回 per-step p 列表 + 残差"""
    out = {s: [] for s in range(1, 7)}; rr = []
    for wi, sg in WIN:
        w = om[wi]
        for i0, i1, s in sg:
            a, b = sig(wi, i0, i1, s, None)
            if b - a < 6: continue
            idx = np.arange(a, b); t = (TS[wi] + idx) / FS
            raw = {'A': PH['va'][wi][a:b].astype(float), 'B': PH['vb'][wi][a:b].astype(float),
                   'C': PH['vc'][wi][a:b].astype(float)}
            if corr:
                for p in 'ABC': raw[p] = (raw[p] - C[p]) * (Aref / A[p])
            v0 = V0HW[wi][a:b].astype(float) if v0mode == 'hw' else (raw['A'] + raw['B'] + raw['C']) / 3.0
            fl = F[s]
            d = raw[fl] - v0
            c, r = lsfit(w, t, d)
            a_, b_, c_ = c; Amp = np.hypot(a_, b_)
            if Amp < 1e-6: continue
            phi = np.arctan2(b_, a_)
            val = -c_ / Amp
            if abs(val) > 1: continue
            tg = np.linspace(t.min(), t.max(), 4000)
            fz = Amp * np.sin(w * tg + phi) + c_
            k = np.where(np.diff(np.sign(fz)) != 0)[0]
            if not len(k): continue
            tz = tg[k[0]]
            p = (tz - (TS[wi] + i0) / FS) / ((i1 - i0) / FS)
            out[s].append(float(p)); rr.append(float(np.sqrt(np.mean(r ** 2))))
    m = [np.median(out[s]) if out[s] else np.nan for s in range(1, 7)]
    return m, rr


print('\n== ② 三种口径下的"过零位置 p"（0.5 = 步正中）==')
print('   %-34s %s' % ('口径', '  '.join('%5d' % s for s in range(1, 7))))
for mode, corr, tag in (('hw', False, '硬件星形 V0（现状）'),
                        ('sw', False, '软件均值 V0（通道未修正）'),
                        ('swc', True, '软件均值 V0 + 通道级修正')):
    m, rr = run(mode, corr)
    print('   %-34s %s   σ=%.3f 均值=%.3f 残差中位%.1f码'
          % (tag, '  '.join('%5.3f' % x for x in m), np.nanstd(m), np.nanmean(m),
             np.median(rr) if rr else float('nan')))
print('\n   浮相：                     ' + '  '.join('%5s' % F[s] for s in range(1, 7)))
