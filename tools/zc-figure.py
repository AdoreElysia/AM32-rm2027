#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
zc-figure.py —— 三栏对照图：① 原始数据/换向点/原始过零 ② 理论锚定拟合/拟合过零 ③ 分步补偿后

理论锚定（不拟合相位/频率）：
  每段用【自己的换相时刻】为原点，ω = 2π/(6·段长)（由栅格给出），
  过零钉在步正中 ⇒ 基函数 = S·sin(ω·(t − w/2))，S = 该段过零方向（由数据斜率判定）。
  只拟合 (A_p, C_p)。
用法: python3 tools/zc-figure.py <win.npz> [输出png]
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
OUT = sys.argv[2] if len(sys.argv) > 2 else NPZ.replace('_win.npz', '_过零对照图.png')
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
PHK = {'A': 'va', 'B': 'vb', 'C': 'vc'}
COL = {'A': '#c0392b', 'B': '#2471a3', 'C': '#27ae60'}
FS = 24000.0
WLO, WHI = 1600.0, 2200.0

z = np.load(NPZ)
ST, V0HW = z['step'], z['v0']
PH = {k: z[k] for k in ('va', 'vb', 'vc')}


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


# ---- 收集步段（全数据集，用于拟合 A/C）----
SEG = []
for wi in range(len(ST)):
    sg = [(i0, i1, s) for i0, i1, s in segs(ST[wi])
          if s in F and i0 != 0 and i1 != 512 and i1 - i0 >= 14]
    if not sg:
        continue
    if not (WLO <= np.median([(i1 - i0) * 83.333 for i0, i1, s in sg]) <= WHI):
        continue
    for i0, i1, s in sg:
        # ★ 拟合只用段中间 60%（两端各去 20%）—— 否则换相振铃尖峰主导最小二乘
        a = i0 + int((i1 - i0) * 0.20); b = i0 + int((i1 - i0) * 0.80)
        if b - a < 8: continue
        w = (i1 - i0) / FS
        t = (np.arange(a, b) - i0) / FS          # 仍以【换相点】为原点
        d = PH[PHK[F[s]]][wi][a:b].astype(float) - V0HW[wi][a:b]
        S = 1.0 if (d[-1] - d[0]) >= 0 else -1.0
        om = 2 * np.pi / (6 * w)
        tf = (np.arange(i0, i1) - i0) / FS
        df = PH[PHK[F[s]]][wi][i0:i1].astype(float) - V0HW[wi][i0:i1]
        SEG.append(dict(wi=wi, i0=i0, i1=i1, a=a, b=b, s=s, ph=F[s], w=w, S=S, t=t, d=d,
                        tf=tf, df=df, bas=S * np.sin(om * (t - w / 2)), om=om))

# ---- 只拟合 (A_p, C_p) ----
A = {}; C = {}
for p in 'ABC':
    E = [e for e in SEG if e['ph'] == p]
    M = np.concatenate([np.column_stack([e['bas'], np.ones_like(e['bas'])]) for e in E])
    V = np.concatenate([e['d'] for e in E])
    coef, *_ = np.linalg.lstsq(M, V, rcond=None)
    A[p] = float(coef[0]); C[p] = float(coef[1])

print('== 理论锚定拟合（只 2 参数/相）==')
for p in 'ABC':
    E = [e for e in SEG if e['ph'] == p]
    V = np.concatenate([e['d'] for e in E]); B = np.concatenate([e['bas'] for e in E])
    r = V - (A[p] * B + C[p])
    print('   %s: 幅值 %+7.0f  直流 %+6.1f  残差RMS %5.1f 码（%.1f%%）'
          % (p, A[p], C[p], np.sqrt(np.mean(r ** 2)), 100 * np.sqrt(np.mean(r ** 2)) / abs(A[p])))

# ---- 分步补偿量：把该步实测过零搬到 0.5 ----
def p_from(e, d):
    """在【裁剪段】找变号，返回相对整段的 p（0=换相点，1=下一次换相）"""
    for k in range(1, len(d)):
        if d[k - 1] * d[k] <= 0 and d[k - 1] != d[k]:
            fr = d[k - 1] / (d[k - 1] - d[k])
            tz = e['t'][k - 1] + fr * (e['t'][k] - e['t'][k - 1])   # 以换相点为 0
            return tz / e['w']
    return np.nan


def p_meas(e):
    return p_from(e, e['d'])


P0 = {s: [] for s in range(1, 7)}
K = {s: [] for s in range(1, 7)}
for e in SEG:
    pp = p_meas(e)
    if pp == pp:
        P0[e['s']].append(pp)
        K[e['s']].append((np.polyfit(e['t'], e['d'], 1)[0], e['w']))
corr = {s: float(np.median([-k * (np.median(P0[s]) - 0.5) * w for k, w in K[s]])) for s in range(1, 7)}
print('\n== 分步补偿量（码）==')
print('   ' + '  '.join('step%d %+7.1f' % (s, corr[s]) for s in range(1, 7)))

# ---- 图：挑一个窗口，三栏 ----
wins = sorted(set(e['wi'] for e in SEG))
wi = wins[len(wins) // 2]
sel = [e for e in SEG if e['wi'] == wi]
fig, ax = plt.subplots(3, 1, figsize=(16, 12), sharex=True)
x = lambda i: (i - sel[0]['i0']) * 1000 / FS          # 以该窗口第一段换相点为 0
for e in sel:
    tms = x(e['i0']) + e['tf'] * 1e3
    mod = A[e['ph']] * e['S'] * np.sin(e['om'] * (e['tf'] - e['w'] / 2)) + C[e['ph']]
    # ① 原始数据（彩点） + 模型（同色细线，作参照）
    ax[0].plot(tms, e['df'], '.', ms=2.4, color=COL[e['ph']], alpha=.55)
    ax[0].plot(tms, mod, '-', lw=.8, color=COL[e['ph']], alpha=.40)
    # ② 模型（彩实线） + 数据（淡点）
    ax[1].plot(tms, e['df'], '.', ms=2.0, color=COL[e['ph']], alpha=.28)
    ax[1].plot(tms, mod, '-', lw=1.6, color=COL[e['ph']])
    # ③ ★ 原始数据(浅灰) vs 补偿后数据(彩点)；模型只留一条黑虚线当参照
    ax[2].plot(tms, e['df'], '.', ms=1.8, color='0.72')
    ax[2].plot(tms, e['df'] - corr[e['s']], '.', ms=2.4, color=COL[e['ph']], alpha=.6)
    ax[2].plot(tms, mod, '--', lw=.9, color='k', alpha=.45)
    for k, A_ in enumerate(ax):
        A_.axvline(x(e['i0']), color='0.6', ls=':', lw=.9)                       # 换向点
        A_.axvline(x(e['i0']) + 0.5 * e['w'] * 1e3, color='k', ls='-', lw=1.0, alpha=.7)  # 步中点
    pp = p_from(e, e['d'])
    if pp == pp:
        ax[0].axvline(x(e['i0']) + pp * e['w'] * 1e3, color='#c0392b', ls='--', lw=1.0)
        ax[0].text(x(e['i0']) + pp * e['w'] * 1e3 + .05, 520, '%.2f' % pp, fontsize=7)
        ax[1].axvline(x(e['i0']) + pp * e['w'] * 1e3, color='#c0392b', ls='--', lw=1.0)
        ax[1].text(x(e['i0']) + pp * e['w'] * 1e3 + .05, 520, '%.2f' % pp, fontsize=7)
        ax[2].axvline(x(e['i0']) + pp * e['w'] * 1e3, color='#c0392b', ls=':', lw=.9)
    pp2 = p_from(e, e['d'] - corr[e['s']])
    if pp2 == pp2:
        ax[2].axvline(x(e['i0']) + pp2 * e['w'] * 1e3, color='#27ae60', ls='-', lw=1.3)
        ax[2].text(x(e['i0']) + pp2 * e['w'] * 1e3 + .05, 520, '%.2f' % pp2, fontsize=7)
for k, tt in enumerate(('① 原始：彩点=实测数据  同色细线=拟合正弦(参照)  红虚线=原始过零  黑实线=步中点  灰点线=换向点',
                        '② 拟合：彩实线=拟合正弦模型  淡点=实测数据  红虚线=原始过零  黑实线=步中点',
                        '③ 补偿后：浅灰点=原始数据  彩点=补偿后数据  **绿实线=补偿后数据的过零**  红点线=原始过零  黑实线=步中点')):
    ax[k].set_title(tt, fontsize=10); ax[k].grid(alpha=.22); ax[k].set_ylabel('悬空相 − V0 (码)')
    ax[k].set_ylim(-560, 620)
ax[2].set_xlabel('窗口 #%d 内时间 (ms)   彩点=实测（按悬空相着色）  灰点线=换向点' % int(z['seq'][wi]))
plt.tight_layout(); plt.savefig(OUT, dpi=125)
print('\n图 →', OUT)
