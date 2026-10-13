#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
win-steps.py —— 从"窗口式全速采样"里按【步号】统计换相相位（工具，不改固件）。

用途：为"每步补偿"提供数据。对每一片窗口里的每一段完整换相步，量出
  · w_s  = 该步时长（换相→换相，ms / 拍）
  · p_s  = 过零落在这步内的位置（0=换相点，1=下一步换相点）
  · L_s  = (p_s − 0.5) × w_s —— 就是【过零被认晚了的绝对时间】（拍）
             若"检测延迟"是个固定绝对量，L_s 应当【与转速无关】（这才是数值偏移的依据）
  · d_s  = (1 − p_s) × w_s —— 开环里实测的"过零→该步结束"的延迟（= 理想 waitTime）

用法:
  python3 tools/win-steps.py <xxx_win.npz> [起始下标] [结束下标]
"""
import sys
import numpy as np

FS = 24000.0
DT = 1000.0 / FS                 # ms/点
TICK_PER_MS = 2000.0             # 固件计时器 0.5µs = 2000 拍/ms
PH = {'A': 'va', 'B': 'vb', 'C': 'vc'}
FLOAT = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}

z = np.load(sys.argv[1])
LO = int(sys.argv[2]) if len(sys.argv) > 2 else 0
HI = int(sys.argv[3]) if len(sys.argv) > 3 else len(z['step']) - 1
VA, VB, VC, V0, ST = z['va'], z['vb'], z['vc'], z['v0'], z['step']
PHV = {'A': VA, 'B': VB, 'C': VC}


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


acc = {s: {'w': [], 'p': [], 'L': [], 'd': []} for s in range(1, 7)}
for wi in range(LO, HI + 1):
    st = ST[wi]
    for i0, i1, s in segs(st):
        if s not in FLOAT or i0 == 0 or i1 == len(st):
            continue                       # 掐头去尾：被窗口边界截断的段不要
        w = (i1 - i0) * DT
        d = PHV[FLOAT[s]][wi][i0:i1].astype(float) - V0[wi][i0:i1]
        k = zc(d)
        if k is None:
            continue
        p = k / float(i1 - i0)
        acc[s]['w'].append(w)
        acc[s]['p'].append(p)
        acc[s]['L'].append((p - 0.5) * w)          # ms
        acc[s]['d'].append((1.0 - p) * w)          # ms

print(f"窗口 {LO}..{HI}（{HI-LO+1} 片）  每片 {512*DT:.2f}ms")
print(f"{'step':>4}{'浮':>3}{'段数':>5}{'w中位ms':>9}{'w拍':>7}{'p中位':>8}{'L中位ms':>9}{'L拍':>7}"
      f"{'d中位ms':>9}{'d拍':>7}{'d/w':>7}")
tot = []
for s in range(1, 7):
    a = acc[s]
    if not a['w']:
        print(f"{s:>4}{FLOAT[s]:>3}{0:>5}")
        continue
    w = np.median(a['w']); L = np.median(a['L']); d = np.median(a['d'])
    print(f"{s:>4}{FLOAT[s]:>3}{len(a['w']):>5}{w:>9.3f}{w*TICK_PER_MS:>7.0f}"
          f"{np.median(a['p']):>8.3f}{L:>9.4f}{L*TICK_PER_MS:>7.0f}"
          f"{d:>9.4f}{d*TICK_PER_MS:>7.0f}{d/w:>7.3f}")
    tot.append(w)
if tot:
    print(f"\n六步时长 中位 {np.median(tot):.3f}ms = {np.median(tot)*TICK_PER_MS:.0f} 拍 "
          f"=> {60/(np.median(tot)/1000*6)/6:.0f} 转/分；散布 {np.std(tot)/np.mean(tot)*100:.0f}%")
    print("★ 若 L 各步接近相等 ⇒ 支持'固定绝对延迟'模型（waitTime = ci/2 − L_s）")
