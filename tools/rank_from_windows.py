#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""rank_from_windows.py —— 从【开环/闭环实跑】的窗口数据里提 rank 表 + 六步偏置表

为什么要有它（方法论，别再用静态数据凑）：
  · rank（采样槽位）偏差 **只在开环稳定态才有定义**。静态（不转/逐状态蠕动）测出来的
    顺序结论【不可外推】—— 实测对比：静态说 order3 最好(8码)、实跑数据说 order0 最好(18码)，
    其它序实跑都是 91~100 码。
  · newinput 之类的状态会异步被清（见交接），所以取数必须"整轮跑完再离线算"。

输入：win-proto2.py cap 出来的 _win.npz（字段 va/vb/vc/v0/cur/step；step 的 bit6-7 = adc order）
输出：
  · 每 (步, order) 的 悬空相−V0 中位（= 该步该序的偏置）
  · 按 order 分组的"三周期极差"（六步里 1≡4/2≡5/3≡6 ⇒ 这就是 rank 引起的 3 周期误差）
  · 用 order0 出的六步偏置建议值（写进 RM2027_SO_OFF 的第一版候选）

用法：
  python3 tools/rank_from_windows.py <xxx_win.npz> [--v0-mode hw] [--drop 3]
"""
import sys
import argparse
import numpy as np

FLOAT = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}   # 步 → 悬空相
# 六步增益表（当前 targets.h 的 RM2027_SG_NUM），用于把"码"折算成归一化单位
SG = {1: 1152, 2: 1629, 3: 1325, 4: 1152, 5: 1629, 6: 1325}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('npz')
    ap.add_argument('--drop', type=int, default=3,
                    help='每步边界丢掉前 N 个点（避开换相尖峰），默认 3')
    ap.add_argument('--min', type=int, default=8, help='每格最少样本数')
    ap.add_argument('--sg-from-order0', action='store_true',
                    help='额外用 order0 估算六步偏置并打印可粘贴的 RM2027_SO_OFF')
    a = ap.parse_args()

    d = np.load(a.npz)
    st = d['step'].astype(int)
    order = (st >> 6) & 3
    s = st & 7
    V = {'A': d['va'].astype(float), 'B': d['vb'].astype(float), 'C': d['vc'].astype(float)}
    v0 = d['v0'].astype(float)

    # 丢掉每步边界前 drop 个点（换相尖峰会污染中位）
    keep = np.ones(len(s), bool)
    last = None
    for i in range(len(s)):
        if s[i] != last:
            keep[i:i + a.drop] = False
            last = s[i]

    print(f'样本 {len(s)}，step 分布 ' + str({k: int((s == k).sum()) for k in range(1, 7)}))
    print(f'order 分布 ' + str({k: int((order == k).sum()) for k in range(4)}))

    tab = {}
    print('\n步 | 悬空相 |' + ''.join(f'  order{o}  ' for o in range(4)) + '| 极差')
    for stp in range(1, 7):
        f = FLOAT[stp]
        row = []
        for o in range(4):
            m = keep & (s == stp) & (order == o)
            row.append(float(np.median(V[f][m] - v0[m])) if m.sum() >= a.min else np.nan)
        tab[stp] = row
        print(f' {stp} |   {f}    | ' + ' '.join(f'{v:+8.0f}' for v in row)
              + f' | {np.nanmax(row) - np.nanmin(row):6.0f}')

    print('\n★ 按 order 分组的"3 周期"（步1/4=C, 2/5=A, 3/6=B）—— 这就是 rank 引起的误差')
    best = None
    for o in range(4):
        g = [tab[1][o], tab[2][o], tab[3][o]]
        sp = np.nanmax(g) - np.nanmin(g)
        tag = '   ← 当前运行用的（order0）' if o == 0 else ''
        print(f'  order{o}: C={g[0]:+7.0f}  A={g[1]:+7.0f}  B={g[2]:+7.0f}  | 三周期极差={sp:6.0f}{tag}')
        if best is None or sp < best[1]:
            best = (o, sp)
    print(f'  ⇒ 三周期误差最小的 order = order{best[0]}（{best[1]:.0f} 码）')

    if a.sg_from_order0:
        print('\n★ 用 order0 估的六步偏置（so[s] = −median(diff) × sg[s]/1000）')
        so = []
        for stp in range(1, 7):
            v = tab[stp][0]
            so.append(0 if np.isnan(v) else int(round(-v * SG[stp] / 1000.0)))
        print('  #define RM2027_SO_OFF { 0, %s }' % ', '.join(str(x) for x in so))
        print('  ⚠ 这是【候选】：只有开环稳态一段有效；建议先只取开环段（step 单调那段）再算。')
    return 0


if __name__ == '__main__':
    sys.exit(main())
