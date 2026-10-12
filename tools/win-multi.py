#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
win-multi.py —— 把"窗口式全速采样"画成【多窗口波形图】（用户要的那种）。

用法:
  python3 tools/win-multi.py <xxx_win.npz> [窗口数=6] [输出png] [起始下标] [结束下标]
  （下标是"收到数组里的序号 0..N-1"，不是窗口号；给了范围就只在这个区间里等距挑）

每片窗口 = 512 点 @24kHz = 21.33ms 连续全速数据（VA VB VC V0 电流 + step），
所以一张图里能并排看到十几步换相，是"看六步换向"最直接的证据。

图上有什么：
  · 红/蓝/绿细线 = VA / VB / VC 原始电压（ADC 码）
  · 紫虚线      = V0（虚拟中性点 = (VA+VB+VC)/3）
  · 黑粗线      = delta = 悬空相 − V0（就是过零判据用的反电势）
  · 绿竖线      = 【从波形上认到的过零】（每段内 delta 首次变号）
  · 彩色虚线    = 换相点，颜色按 step 号（红1 橙2 黄3 绿4 紫5 蓝6）
  · 段内小字「N 浮X」= 第几步 / 哪一相悬空
标题给出：窗口号、每步时长(ms)、认到过零数/换相数 ⇒ 六步是否均匀一眼看出。

⚠ 不接任何 SWD / 串口，纯离线读 npz。
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

FS = 24000.0                     # 窗口采样率（ADCDMA 每帧一次）
DT_MS = 1000.0 / FS              # 每点毫秒 (41.667µs)
FLOAT_PHASE = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}   # 源: Mcu/g071/Src/ADC.c:90-92
STEP_COLOR = {1: '#c0392b', 2: '#d35400', 3: '#b7950b', 4: '#27ae60', 5: '#8e44ad', 6: '#2471a3'}

NPZ = sys.argv[1] if len(sys.argv) > 1 else '/home/lpl/桌面/RM2027/evt_run_win.npz'
NWIN = int(sys.argv[2]) if len(sys.argv) > 2 else 6
OUT = sys.argv[3] if len(sys.argv) > 3 else NPZ.replace('.npz', '_多窗口.png')
LO = int(sys.argv[4]) if len(sys.argv) > 4 else 0
HI = int(sys.argv[5]) if len(sys.argv) > 5 else None

z = np.load(NPZ)
seq, ts = z['seq'], z['ts']
VA, VB, VC, V0 = z['va'], z['vb'], z['vc'], z['v0']
STEP = z['step']
PH = {'A': VA, 'B': VB, 'C': VC}
N = len(seq)
HI = N - 1 if HI is None else min(HI, N - 1)
LO = max(0, min(LO, HI))


def segments(st):
    """返回 [(i0, i1, step)] —— step 变化的边界处切开（i1 为开区间）"""
    segs, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            segs.append((i0, k, int(st[i0])))
            i0 = k
    segs.append((i0, len(st), int(st[i0])))
    return segs


def zero_cross(d, skip=3):
    """段内 delta 首次变号的点（跳过换相后 skip 点的振铃）"""
    for k in range(max(skip, 1), len(d)):
        if d[k - 1] * d[k] <= 0 and d[k - 1] != d[k]:
            return k
    return None


def step_widths(win_i):
    """该窗口每步时长(ms)；两端被窗口边界截断的段标记出来"""
    st = STEP[win_i]
    out = []
    for i0, i1, s in segments(st):
        partial = (i0 == 0) or (i1 == len(st))
        out.append((( i1 - i0) * DT_MS, s, partial))
    return out


def annotate_win(ax, win_i):
    st = STEP[win_i]
    va, vb, vc, v0 = VA[win_i], VB[win_i], VC[win_i], V0[win_i]
    x = np.arange(512) * DT_MS
    for y, c, lb in ((va, '#c0392b', 'VA'), (vb, '#2471a3', 'VB'),
                     (vc, '#27ae60', 'VC'), (v0, '#8e44ad', 'V0')):
        ax.plot(x, y, lw=.7, color=c, alpha=.85, label=lb,
                ls='--' if lb == 'V0' else '-')

    segs = segments(st)
    n_zc = 0
    for i0, i1, s in segs:
        if s not in FLOAT_PHASE:
            continue
        d = PH[FLOAT_PHASE[s]][win_i][i0:i1].astype(float) - v0[i0:i1]
        ax.plot(x[i0:i1], d, lw=1.5, color='k', alpha=.75)
        # 换相线（段起点）
        ax.axvline(x[i0], color=STEP_COLOR.get(s, 'gray'), ls='--', lw=1.0, alpha=.85)
        # 悬空相标记
        ax.text(x[i0] + 0.15, 1240, f'{s} 浮{FLOAT_PHASE[s]}',
                fontsize=7.5, color=STEP_COLOR.get(s, 'gray'))
        k = zero_cross(d)
        if k is not None:
            n_zc += 1
            ax.axvline(x[i0 + k], color='#0b6623', ls='-', lw=1.2, alpha=.9)
    return len(segs), n_zc


idxs = np.linspace(LO, HI, NWIN).astype(int)
fig, axes = plt.subplots(NWIN, 1, figsize=(17, 3.0 * NWIN), sharex=True)
if NWIN == 1:
    axes = [axes]

for ax, wi in zip(axes, idxs):
    nseg, nzc = annotate_win(ax, wi)
    ws = step_widths(wi)
    body = ' '.join(f'{w:.2f}{"*" if p else ""}' for w, _s, p in ws)
    full = [w for w, _s, p in ws if not p]
    cv = (np.std(full) / np.mean(full) * 100) if len(full) > 2 else float('nan')
    tag = '开环(固定时基)' if cv < 8 else ('闭环' if cv == cv else '未换相')
    ax.set_ylim(-60, 1400)
    ax.set_ylabel('ADC 码')
    ax.grid(alpha=.22)
    ax.legend(fontsize=7.5, ncol=5, loc='lower right')
    ax.set_title(f'窗口 #{int(seq[wi])}（收到第 {wi+1}/{N} 片，t≈{ts[wi]/24000.0:.2f}s）   '
                 f'步长(ms): {body}   [{tag} 散布 {cv:.0f}%]   认到过零 {nzc}/{nseg}', fontsize=10)
axes[-1].set_xlabel('窗口内时间 (ms) —— 一片 = 512 点 @24kHz = 21.33ms 连续全速')
plt.tight_layout()
plt.savefig(OUT, dpi=125)
print(f'图 → {OUT}')
print(f'（挑选的窗口序号 {[int(seq[i]) for i in idxs]}，共收到 {N} 片：{int(seq[0])}~{int(seq[-1])}）')
