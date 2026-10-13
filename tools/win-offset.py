#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
win-offset.py —— 量【开环稳定段】的每步竖直偏移与幅值（含奇偶分组）。
用途：判定"采样时刻假设" —— 把 `ADC_TRIGGER_CCR4` 后移后，奇偶偏置是否缩小。

口径（与固件判据同一套）：
  幅值 = 次大 − 次小 ；  竖直中心 = (次大 + 次小) / 2      （单位 ADC 码）
  只用【开环斜坡稳定段】：本步时长 1700~2100 拍（即交棒前那一段，换相由固定时基给、
  分割是外生的 ⇒ 测量干净；闭环段的分割由我们自己的换相决定，不能用于此测量）。

用法:
  python3 tools/win-offset.py <run_win.npz> [步长下限=1700] [步长上限=2100]
"""
import sys
import numpy as np

F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
NPZ = sys.argv[1]
WLO = float(sys.argv[2]) if len(sys.argv) > 2 else 1700.0
WHI = float(sys.argv[3]) if len(sys.argv) > 3 else 2100.0
TICK_PER_SAMPLE = 1000.0 / 24000.0 * 2000.0        # 1 采样 = 83.33 拍


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


z = np.load(NPZ)
ST, V0 = z['step'], z['v0']
PH = {'A': z['va'], 'B': z['vb'], 'C': z['vc']}

off = {s: [] for s in range(1, 7)}
amp = {s: [] for s in range(1, 7)}
for wi in range(len(ST)):
    for i0, i1, s in segs(ST[wi]):
        if s not in F or i0 == 0 or i1 == 512 or i1 - i0 < 12:
            continue
        w = (i1 - i0) * TICK_PER_SAMPLE
        if not (WLO <= w <= WHI):
            continue
        d = np.sort((PH[F[s]][wi][i0:i1].astype(float) - V0[wi][i0:i1])[3:-3])
        if len(d) < 10:
            continue
        off[s].append((d[-2] + d[1]) / 2.0)
        amp[s].append(d[-2] - d[1])

print("样本：每步 %s" % {s: len(off[s]) for s in range(1, 7)})
print("%4s%5s%10s%12s%12s" % ('step', '浮相', 'n', '竖直中心(码)', '幅值(码)'))
mo = {}
for s in range(1, 7):
    if not off[s]:
        print('%4d%5s%10d%12s%12s' % (s, F[s], 0, '—', '—')); continue
    m = float(np.median(off[s])); mo[s] = m
    print('%4d%5s%10d%12.1f%12.0f' % (s, F[s], len(off[s]), m, np.median(amp[s])))
odd = [mo[s] for s in (1, 3, 5) if s in mo]
even = [mo[s] for s in (2, 4, 6) if s in mo]
print()
if odd and even:
    print('★ 奇步(1,3,5) 中心均值 = %+.1f 码' % np.mean(odd))
    print('★ 偶步(2,4,6) 中心均值 = %+.1f 码' % np.mean(even))
    print('★ **奇偶差 = %+.1f 码**（判定采样时刻假设：CCR4 后移后此值应缩小）'
          % (np.mean(odd) - np.mean(even)))
    print('   逐步中心 = {%s}' % ', '.join('%+.0f' % mo[s] for s in range(1, 7) if s in mo))
