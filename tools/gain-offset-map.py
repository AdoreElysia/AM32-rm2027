#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gain-offset-map.py —— A/B/C/V0 四路的【增益】与【偏置】对照（开环稳定段）。
回答：① 每相的倍率（增益）是多少、是否常数；② 每步的竖直偏移是多少；
      ③ 过零落在步内什么位置 p（理想 0.5）；④ 增益/偏置对 CCR4 的敏感性。

口径：
  增益（每相）   = 该相悬空时【原始读数】的峰峰（次大−次小）        —— 纯通道量，不含 V0
  偏置（每步）   = delta=(悬空相−V0) 的竖直中心 = 段两端平台中位的中点
  p（每步）      = 过零落在段内的位置（0=换相点, 1=下一次换相）；0.5 = 正中
用法: python3 tools/gain-offset-map.py <win.npz> [步长下限] [步长上限]
"""
import sys
import numpy as np

F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
PHK = {'A': 'va', 'B': 'vb', 'C': 'vc'}
TPS = 1000.0 / 24000.0 * 2000.0

NPZ = sys.argv[1]
WLO = float(sys.argv[2]) if len(sys.argv) > 2 else 1600.0
WHI = float(sys.argv[3]) if len(sys.argv) > 3 else 2200.0

z = np.load(NPZ)
ST, V0 = z['step'], z['v0']
PH = {'A': z['va'], 'B': z['vb'], 'C': z['vc']}


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k-1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


def rob(d):
    s = np.sort(d); return (s[-2] - s[1]), (s[-2] + s[1]) / 2.0


raw = {'A': [], 'B': [], 'C': [], 'V0': []}
per = {s: {'off': [], 'amp': [], 'p': []} for s in range(1, 7)}
m3err = []
for wi in range(len(ST)):
    for i0, i1, s in segs(ST[wi]):
        if s not in F or i0 == 0 or i1 == 512 or i1 - i0 < 14:
            continue
        w = (i1 - i0) * TPS
        if not (WLO <= w <= WHI):
            continue
        v = PH[F[s]][wi][i0:i1].astype(float)
        pp, _ = rob(v[3:-3]); raw[F[s]].append(pp)
        pv = V0[wi][i0:i1].astype(float)
        pp0, _ = rob(pv[3:-3]); raw['V0'].append(pp0)
        m3 = (PH['A'][wi][i0:i1].astype(float) + PH['B'][wi][i0:i1]
              + PH['C'][wi][i0:i1]) / 3.0
        m3err.append(float(np.median(pv - m3)))
        d = v - pv
        a, o = rob(d[3:-3])
        per[s]['amp'].append(a); per[s]['off'].append(o)
        dd = d[3:-3]
        kk = None
        for k in range(1, len(dd)):
            if dd[k-1] * dd[k] <= 0 and dd[k-1] != dd[k]:
                kk = k; break
        per[s]['p'].append(kk / float(len(dd)) if kk else np.nan)

print('== 每相【原始悬空读数】峰峰（纯通道增益）==')
base = float(np.median(raw['C'])) if raw['C'] else float('nan')
for ph in ('A', 'B', 'C', 'V0'):
    if not raw[ph]:
        print('  %s: 无' % ph); continue
    m = float(np.median(raw[ph]))
    print('  %-3s n=%4d 峰峰中位 %6.1f 码  相对 C %.2f' % (ph, len(raw[ph]), m, m / base))
print('  V0 与 (VA+VB+VC)/3 的中位差 = %+.1f 码' % float(np.median(m3err)))
print()
print('== 每步：竖直偏移 / 幅值 / 过零位置 p ==')
print('  step 浮相   n   偏移(码)  幅值(码)   p(0.5=正中)')
mo = {}
for s in range(1, 7):
    r = per[s]
    if not r['off']:
        continue
    mo[s] = float(np.median(r['off']))
    print('   %d   %s  %4d  %+8.1f  %7.0f   %8.3f'
          % (s, F[s], len(r['off']), mo[s], np.median(r['amp']), np.nanmedian(r['p'])))
odd = [mo[s] for s in (1, 3, 5) if s in mo]; even = [mo[s] for s in (2, 4, 6) if s in mo]
if odd and even:
    print('  ★ 奇步均值 %+.1f / 偶步均值 %+.1f ⇒ 奇偶差 %+.1f 码'
          % (np.mean(odd), np.mean(even), np.mean(odd) - np.mean(even)))
print()
print('== 偏置 与 p 的关系（理想：偏置=0 ⇔ p=0.5）==')
for s in range(1, 7):
    if s in mo:
        print('   step%d 偏置 %+6.1f 码  ⇒ p = %.3f' % (s, mo[s], np.nanmedian(per[s]['p'])))
