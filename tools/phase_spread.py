#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""phase_spread.py —— 从窗口 npz 评估"六步过零相位"一致性（判据好不好的硬指标）

指标：每一步内，对 (悬空相 − V0) 做中段线性拟合，求过零点在步内的归一化位置 p∈(0,1)。
     理想 p≈0.5；六步的 p 越是集中在 0.5、散布越小，判据越好。

用法：python3 tools/phase_spread.py <xxx_win.npz> [--drop 3] [--minlen 40]
输出：每步 p 的中位/样本数，以及总体中位与"六步中位之间的极差"。
"""
import sys
import argparse
import numpy as np

FLOAT = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}


def segments(idx):
    if len(idx) == 0:
        return []
    brk = np.where(np.diff(idx) != 1)[0] + 1
    return [s for s in np.split(idx, brk) if len(s)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('npz')
    ap.add_argument('--drop', type=int, default=3)
    ap.add_argument('--minlen', type=int, default=40)
    a = ap.parse_args()

    d = np.load(a.npz)
    st = d['step'].astype(int)
    s = st & 7
    V = {'A': d['va'].astype(float), 'B': d['vb'].astype(float), 'C': d['vc'].astype(float)}
    v0 = d['v0'].astype(float)
    cur = d['cur'].astype(float) if 'cur' in d else None

    per = {}
    for stp in range(1, 7):
        f = FLOAT[stp]
        ps = []
        for seg in segments(np.where(s == stp)[0]):
            seg = seg[a.drop:]
            n = len(seg)
            if n < a.minlen:
                continue
            t = np.arange(n, dtype=float)
            y = V[f][seg] - v0[seg]
            m = (t >= 0.2 * (n - 1)) & (t <= 0.8 * (n - 1))
            if m.sum() < 8:
                continue
            k, b = np.polyfit(t[m], y[m], 1)
            if abs(k) < 1e-9:
                continue
            p = -b / k / (n - 1)
            if 0.0 < p < 1.0:
                ps.append(p)
        per[stp] = ps

    print(f'{a.npz}  采样 {len(st)}')
    meds = []
    print(' 步 | 悬空相 | 相位中位 | n')
    for stp in range(1, 7):
        ps = per[stp]
        if ps:
            m = float(np.median(ps))
            meds.append(m)
            print(f'  {stp} |   {FLOAT[stp]}    |  {m:.3f}   | {len(ps)}')
        else:
            print(f'  {stp} |   {FLOAT[stp]}    |   --     | 0')
    allp = [p for ps in per.values() for p in ps]
    if allp:
        print(f'\n总体: n={len(allp)}  中位={np.median(allp):.3f}  极差={np.ptp(allp):.3f}')
    if len(meds) >= 2:
        print(f'六步中位之间: 极差={max(meds) - min(meds):.3f}   (越小越好)')
    if cur is not None:
        print(f'电流: 中位={np.median(cur):.1f}  峰={cur.max():.1f}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
