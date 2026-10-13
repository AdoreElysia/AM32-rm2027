#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sine-fit.py（v2，逐窗口拟合）—— 把悬空相数据拟合成正弦，算残差，检验"修正后应是同一根正弦"。

★ v1 的问题：用"整段平均步长"定 ω，但斜坡段频率在变（加速）⇒ 整段不是纯正弦 ⇒ 拟合退化
  （拟合幅值只有 19/6/28 码、残差比"幅值"还大）。
★ v2：**逐窗口拟合** —— 一个窗口 21.33ms ≈ 3.4 个电周期，窗口内 ω 视为常数（由该窗口自己的
  平均步长给出）⇒ 每相单独做线性最小二乘（sin/cos 基）拿 A / C / φ。

理想情形（同一台电机、三相理想对称）：A_A = A_B = A_C、C=0、φ 相差正好 120°。
用法: python3 tools/sine-fit.py <win.npz> [输出png]
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
OUT = sys.argv[2] if len(sys.argv) > 2 else NPZ.replace('_win.npz', '_正弦拟合.png')
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
FS = 24000.0
WLO, WHI = 1600.0, 2200.0

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


rows = []          # (wi, ph, A, C, phi_deg, rms, t, v, fit)
for wi in range(len(ST)):
    sg = segs(ST[wi])
    ok = [(i0, i1, s) for i0, i1, s in sg if s in F and i0 != 0 and i1 != 512 and i1 - i0 >= 12]
    if not ok:
        continue
    ws = [(i1 - i0) * 83.333 for i0, i1, s in ok]
    if not (WLO <= np.median(ws) <= WHI):
        continue
    wt = (i1 - i0 for i0, i1, s in ok)
    # 该窗口的电频率：6 个换相步为一个电周期
    per = np.mean([(i1 - i0) for i0, i1, s in ok]) / FS          # 秒/步
    w = 2 * np.pi / (6 * per)
    for i0, i1, s in ok:
        a = i0 + int((i1 - i0) * 0.30); b = i0 + int((i1 - i0) * 0.70)
        if b - a < 6:
            continue
        # ★ 用【全局帧时间】当原点（而不是每段自己的 t=0）—— 否则相位的绝对值随机、中位数无意义
        t = (int(z['ts'][wi]) + np.arange(a, b)) / FS
        v = PH[F[s]][wi][a:b].astype(float) - V0[wi][a:b]
        M = np.column_stack([np.sin(w * t), np.cos(w * t), np.ones_like(t)])
        coef, *_ = np.linalg.lstsq(M, v, rcond=None)
        A = float(np.hypot(coef[0], coef[1])); phi = float(np.degrees(np.arctan2(coef[1], coef[0])))
        r = v - M @ coef
        rows.append((wi, F[s], A, float(coef[2]), phi, float(np.sqrt(np.mean(r ** 2))), t, v, coef, w))

print('== 逐窗口、逐相 正弦拟合统计（开环稳定段，%d 个段样本）==' % len(rows))
print('  相   n     A中位(码)   C中位(码)   φ中位(deg)   残差RMS中位(码)  残差/A')
agg = {}
for ph in ('A', 'B', 'C'):
    R = [r for r in rows if r[1] == ph]
    if not R:
        print('   %s: 无' % ph); continue
    A = np.median([r[2] for r in R]); C = np.median([r[3] for r in R])
    P = np.median([r[4] for r in R]); E = np.median([r[5] for r in R])
    agg[ph] = (A, C, P, E)
    print('   %s %4d  %10.0f  %10.1f  %11.1f  %14.1f  %8.3f' % (ph, len(R), A, C, P, E, E / A))

print('\n== 三相相位差（★按窗口为单位，窗口内 ω 恒定 ⇒ 可比）==')
dw = {}
for wi in sorted(set(r[0] for r in rows)):
    d = {}
    for r in rows:
        if r[0] == wi:
            d[r[1]] = r[4]
    if len(d) == 3:
        dw.setdefault('BA', []).append((d['B'] - d['A']) % 360)
        dw.setdefault('CA', []).append((d['C'] - d['A']) % 360)
for k, v in dw.items():
    print('   %s: 中位 %.1f°   （理想 120.0 或 240.0）' % (k, np.median(v)))
print('\n== 三相一致性（理想：A 全等、C≈0、φ 相差 120°）==')
if len(agg) == 3:
    AA = np.mean([agg[p][0] for p in 'ABC'])
    for ph in ('A', 'B', 'C'):
        A, C, P, E = agg[ph]
        print('   %s: A/平均 %.3f   C %+7.1f 码   Δφ(相对A) %+8.1f°  （理想 0/∓120/±120，即 -120 或 +120）'
              % (ph, A / AA, C, P - agg['A'][2]))
    d1 = (agg['B'][2] - agg['A'][2]) % 360
    d2 = (agg['C'][2] - agg['A'][2]) % 360
    print('   ⇒ B−A = %.1f°（理想 120 或 240）   C−A = %.1f°（理想 240 或 120）' % (d1, d2))
    print('   ⇒ 若明显偏离 120/240 ⇒ 三相不是"同一根正弦"，这正是要修的东西')

print('\n== 残差对比：三相"各自拟合" vs "共同正弦模型（同一 A、相差 120°、C=0）" ==')
best = None
for sgn in (+1, -1):
    for ph0 in np.arange(0, 360, 1.0):
        Ms = []; vs = []
        for ph, k in (('A', 0), ('B', sgn * -1), ('C', sgn * 1)):
            for r in rows:
                if r[1] != ph:
                    continue
                t, v, w = r[6], r[7], r[9]
                Ms.append(np.sin(w * t + np.radians(ph0 + k * 120))); vs.append(v)
        if not Ms:
            continue
        M = np.concatenate(Ms); V = np.concatenate(vs)
        k_ = (M @ V) / (M @ M)
        rms = float(np.sqrt(np.mean((V - k_ * M) ** 2)))
        if best is None or rms < best[0]:
            best = (rms, k_, ph0, sgn)
print('   最优共同模型：A=%.0f 码  初相 %.0f°  方向 %+d  ⇒ 残差RMS %.1f 码（占 A 的 %.1f%%）'
      % (best[1], best[2], best[3], best[0], 100 * best[0] / best[1]))
print('   （若这个残差 ≈ 各相"各自拟合"的残差，说明三相本来就在同一根正弦上；'
      '若大很多，说明三相不一致）')

# ---- 图：两个窗口，三相样本点 + 各自拟合线 + 共同模型 ----
wins = sorted(set(r[0] for r in rows))
pick = wins[len(wins)//2:len(wins)//2 + 2]
fig, axes = plt.subplots(len(pick), 1, figsize=(15, 4.6 * len(pick)))
if len(pick) == 1:
    axes = [axes]
col = {'A': '#c0392b', 'B': '#2471a3', 'C': '#27ae60'}
for ax, wi in zip(axes, pick):
    for ph, color in col.items():
        for r in rows:
            if r[0] != wi or r[1] != ph:
                continue
            t, v, coef, w = r[6], r[7], r[8], r[9]
            ax.plot(t * 1e3, v, 'o', ms=2.6, color=color, alpha=.6)
            tt = np.linspace(t.min(), t.max(), 400)
            ax.plot(tt * 1e3, np.hypot(coef[0], coef[1]) * np.sin(w * tt + np.arctan2(coef[1], coef[0])) + coef[2],
                    '-', lw=1.2, color=color)
    ax.grid(alpha=.22); ax.set_ylabel('悬空相 − V0 (码)')
    ax.set_title('窗口 #%d：点=实测（只用段中间40%%），线=逐相正弦拟合' % int(z['seq'][wi]), fontsize=10)
axes[-1].set_xlabel('窗口内时间 (ms)')
plt.tight_layout(); plt.savefig(OUT, dpi=125)
print('\n图 →', OUT)
