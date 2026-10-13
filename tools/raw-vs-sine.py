#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
raw-vs-sine.py —— 只回答两件事（用户 2026-10-13 提问）：
   ① "原始数据 + 拟合正弦波" （不掺任何补偿）
   ② "每一步到底是不是一条直线？" —— 同一批数据上，正弦模型 vs 逐步直线模型，比残差。

模型（理论锚定，只拟合幅值/直流，不拟合相位与频率）：
   d(t) = A_p · S · sin( ω·(t − w/2) ) + C_p
   · t 以【该步自己的换相时刻】为原点 ⇒ 步长 w = 该段长度，ω = 2π/(6w)（理论值）
   · 过零钉在步正中（t = w/2）—— 理论值，不拟合
   · S = ±1 由数据斜率定（上升段/下降段交替）
   逐步直线模型：对每段单独最小二乘拟合 d ≈ k·t + b（这是"每一步是一条直线"的最优版本）

用法: python3 tools/raw-vs-sine.py <win.npz> [输出png] [窗口序号]
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
OUT = sys.argv[2] if len(sys.argv) > 2 else NPZ.replace('_win.npz', '_原始vs正弦_%s.png' % (sys.argv[4] if len(sys.argv) > 4 else 'hw'))
WANT = int(sys.argv[3]) if (len(sys.argv) > 3 and sys.argv[3].lstrip('-').isdigit()) else None
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}      # 每步哪一相悬空
PHK = {'A': 'va', 'B': 'vb', 'C': 'vc'}
COL = {'A': '#c0392b', 'B': '#2471a3', 'C': '#27ae60'}
FS = 24000.0
WLO, WHI = 1600.0, 2200.0        # 只取匀速段（0.5µs ticks）
TRIM = 0.20                      # 两端各掐 20%（避开换相振铃）——只用于拟合，绘图仍画全段

z = np.load(NPZ)
ST, V0HW = z['step'], z['v0']
PH = {k: z[k] for k in ('va', 'vb', 'vc')}

# V0 来源：hw = 板上电阻虚拟中性点通道；sw = (VA+VB+VC)/3 由三个相通道软件算
V0MODE = sys.argv[4] if len(sys.argv) > 4 else 'hw'
if V0MODE == 'sw':
    V0F = (PH['va'].astype(float) + PH['vb'].astype(float) + PH['vc'].astype(float)) / 3.0
else:
    V0F = V0HW.astype(float)
print('V0 来源 = %s' % ('软件均值 (VA+VB+VC)/3' if V0MODE == 'sw' else '板上电阻虚拟中性点通道'))


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


# ---------------- 收集步段 ----------------
SEG = []
for wi in range(len(ST)):
    sg = [(i0, i1, s) for i0, i1, s in segs(ST[wi])
          if s in F and i0 != 0 and i1 != 512 and i1 - i0 >= 14]
    if not sg:
        continue
    if not (WLO <= np.median([(i1 - i0) * 83.333 for i0, i1, s in sg]) <= WHI):
        continue
    for i0, i1, s in sg:
        a = i0 + int((i1 - i0) * TRIM); b = i0 + int((i1 - i0) * (1 - TRIM))
        if b - a < 8:
            continue
        w = (i1 - i0) / FS
        tfull = (np.arange(i0, i1) - i0) / FS                  # ★以该段换相点为原点
        dfull = PH[PHK[F[s]]][wi][i0:i1].astype(float) - V0F[wi][i0:i1]
        t = (np.arange(a, b) - i0) / FS
        d = PH[PHK[F[s]]][wi][a:b].astype(float) - V0F[wi][a:b]
        S = 1.0 if (d[-1] - d[0]) >= 0 else -1.0               # ★过零方向
        SEG.append(dict(wi=wi, i0=i0, i1=i1, s=s, ph=F[s], w=w, t=t, d=d,
                        tfull=tfull, dfull=dfull, S=S))

print('收集到 %d 个步段（%d 个窗口）' % (len(SEG), len(set(e['wi'] for e in SEG))))

# ---------------- ① 理论锚定正弦：只拟合 (A_p, C_p) ----------------
for e in SEG:
    om = 2 * np.pi / (6 * e['w'])
    e['bas'] = e['S'] * np.sin(om * (e['t'] - e['w'] / 2))
A, C = {}, {}
for p in 'ABC':
    E = [e for e in SEG if e['ph'] == p]
    M = np.concatenate([e['bas'] for e in E]); V = np.concatenate([e['d'] for e in E])
    A[p] = float((M @ V) / (M @ M))
    C[p] = float(np.mean([np.mean(e['d']) for e in E]))
for e in SEG:
    om = 2 * np.pi / (6 * e['w'])
    e['sine'] = A[e['ph']] * e['S'] * np.sin(om * (e['t'] - e['w'] / 2)) + C[e['ph']]
    e['sine_full'] = A[e['ph']] * e['S'] * np.sin(om * (e['tfull'] - e['w'] / 2)) + C[e['ph']]

print('\n=== ① 理论锚定正弦（只拟合 每相幅值A/直流C，共 6 个参数）===')
for p in 'ABC':
    E = [e for e in SEG if e['ph'] == p]
    V = np.concatenate([e['d'] for e in E]); M = np.concatenate([e['sine'] for e in E])
    r = V - M
    print('   %s: A=%+7.0f 码  C=%+6.1f 码   残差RMS %5.1f 码 (=幅值 %.1f%%)  样本%d'
          % (p, A[p], C[p], np.sqrt(np.mean(r ** 2)),
             100 * np.sqrt(np.mean(r ** 2)) / abs(A[p]), len(V)))

# ---------------- ② 逐步直线：每段单独拟合 (k,b) ----------------
print('\n=== ② 逐步直线（每段单独拟合 k,b ⇒ 每步 2 个参数）===')
RL = {p: [] for p in 'ABC'}
for e in SEG:
    k, b = np.polyfit(e['t'], e['d'], 1)
    e['line'] = k * e['t'] + b
    e['line_full'] = k * e['tfull'] + b
    r = e['d'] - e['line']
    RL[e['ph']].append(np.sqrt(np.mean(r ** 2)))
for p in 'ABC':
    print('   %s: 残差RMS 中位 %5.1f 码 (=幅值 %.1f%%)   段数 %d'
          % (p, np.median(RL[p]), 100 * np.median(RL[p]) / abs(A[p]), len(RL[p])))

# 残差对比（同一批数据、同一拟合窗）
print('\n=== ③ 谁更像？同一批数据上的残差（越小越像）===')
print('   %-6s %14s %14s %10s' % ('相', '正弦模型RMS', '逐步直线RMS', '直线/正弦'))
for p in 'ABC':
    E = [e for e in SEG if e['ph'] == p]
    rs_all = np.concatenate([e['d'] - e['sine'] for e in E])
    rl_all = np.concatenate([e['d'] - e['line'] for e in E])
    rs_rms = float(np.sqrt(np.mean(rs_all ** 2)))
    rl_rms = float(np.sqrt(np.mean(rl_all ** 2)))
    print('   %-6s %11.1f 码 %11.1f 码 %10.2f' % (p, rs_rms, rl_rms, rl_rms / rs_rms))

# ---------------- ④ 纯理论：一个 60° 步窗内，正弦离直线最远差多少 ----------------
th = np.linspace(-np.pi / 6, np.pi / 6, 2001)         # 以过零为 0，±30° 电角度
sine = np.sin(th)
line = th * (np.sin(np.pi / 6) / (np.pi / 6))          # 过两端点(±30°,±0.5)的直线
print('\n=== ④ 纯理论（不含数据）：60° 窗内 正弦 vs 直线 ===')
print('   最大偏差 %.3f（占峰值1的 %.2f%%；占该窗峰峰值1.0的 %.2f%%）'
      % (np.max(np.abs(sine - line)), 100 * np.max(np.abs(sine - line)),
         100 * np.max(np.abs(sine - line))))

# ---------------- 相位折叠：把所有段按"段内相对位置"叠起来 ----------------
print('\n=== ⑤ 相位折叠平均形状（把所有段按 t/w 叠平均，看真实形状）===')
NB = 24
fold = {}
for p in 'ABC':
    acc = [[] for _ in range(NB)]
    for e in SEG:
        if e['ph'] != p:
            continue
        u = e['tfull'] / e['w']                        # 0..1
        v = e['dfull'] * np.sign(A[p]) * e['S'] / abs(A[p])   # 归一化到"该相幅值=1"的上升段
        for ui, vi in zip(u, v):
            k = min(NB - 1, int(ui * NB))
            if 0 <= k:
                acc[k].append(vi)
    fold[p] = (np.array([np.median(a) if a else np.nan for a in acc]),
               np.array([np.std(a) if len(a) > 1 else np.nan for a in acc]))
    m, sd = fold[p]
    print('   %s: 归一化(1=幅值) 折叠后 中位曲线 min=%+.2f max=%+.2f；段间散布中位 %.3f（=幅值的 %.1f%%）'
          % (p, np.nanmin(m), np.nanmax(m), np.nanmedian(sd), 100 * np.nanmedian(sd)))

# ---------------- 图 ----------------
wi_sel = WANT if WANT is not None else SEG[len(SEG) // 2]['wi']
sel = [e for e in SEG if e['wi'] == wi_sel]
fig = plt.figure(figsize=(15.5, 11))
gs = fig.add_gridspec(3, 1, height_ratios=[1.15, 1.15, 1.0], hspace=0.34)

# --- ① 原始数据 ---
ax = fig.add_subplot(gs[0])
for e in sel:
    x = e['i0'] * 1000 / FS + e['tfull'] * 1e3
    ax.plot(x, e['dfull'], '.', ms=3.0, color=COL[e['ph']], alpha=.85,
            label=('悬空相 ' + e['ph'] if e is sel[0] else None))
    ax.axvline(e['i0'] * 1000 / FS, color='0.55', ls=':', lw=.9)
    ax.axvline((e['i0'] + e['i1']) / 2 * 1000 / FS, color='k', ls='--', lw=.8)
ax.axhline(0, color='0.7', lw=.8)
ax.set_title('① 原始数据（悬空相 − V0，全部采样点，无删减）   灰点线＝换相点   黑虚线＝步中点', fontsize=11)
ax.set_ylabel('码'); ax.grid(alpha=.2); ax.legend(fontsize=8, loc='upper right')

# --- ② 原始数据 + 拟合正弦 + 逐步直线 ---
ax = fig.add_subplot(gs[1])
for e in sel:
    x = e['i0'] * 1000 / FS + e['tfull'] * 1e3
    ax.plot(x, e['dfull'], '.', ms=3.0, color=COL[e['ph']], alpha=.8)
    ax.plot(x, e['sine_full'], '-', lw=1.6, color=COL[e['ph']])            # 正弦
    ax.plot(x, e['line_full'], '--', lw=1.3, color='k', alpha=.75)         # 直线
    ax.axvline(e['i0'] * 1000 / FS, color='0.55', ls=':', lw=.9)
    ax.axvline((e['i0'] + e['i1']) / 2 * 1000 / FS, color='k', ls='--', lw=.8)
ax.axhline(0, color='0.7', lw=.8)
ax.set_title('② 原始数据（点） ＋ 理论锚定正弦（彩实线） ＋ 每步各自最优直线（黑虚线）', fontsize=11)
ax.set_ylabel('码'); ax.grid(alpha=.2)

# --- ③ 相位折叠（全部段叠起来看真实形状） ---
ax = fig.add_subplot(gs[2])
u = np.linspace(0, 1, 400)
th2 = (u - 0.5) * (np.pi / 3)
ax.plot(u, np.sin(th2) / np.sin(np.pi / 6), '-', color='k', lw=1.6, label='理论正弦（归一化）')
ax.plot(u, (u - 0.5) * 2, '--', color='0.45', lw=1.4, label='过端点直线（归一化）')
for p in 'ABC':
    m, sd = fold[p]
    xs = (np.arange(NB) + 0.5) / NB
    ax.plot(xs, m, 'o-', ms=3, lw=1.0, color=COL[p], alpha=.85, label='相 %s 折叠中位' % p)
ax.set_title('③ 相位折叠：把 %d 个步段按"段内相对位置 t/w"叠起来（中位曲线）——形状是弧还是直线，一眼可见'
             % len(SEG), fontsize=11)
ax.set_xlabel('段内相对位置 t/w （0=换相点，0.5=步中点，1=下一换相点）')
ax.set_ylabel('归一化（1=该相幅值）'); ax.grid(alpha=.2); ax.legend(fontsize=8, ncol=2)

plt.savefig(OUT, dpi=125, bbox_inches='tight')
print('\n图 →', OUT)
