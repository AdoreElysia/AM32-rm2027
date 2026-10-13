#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
comp-compare.py —— 把所有【闭环】跑放在一起比：谁活着、谁什么时候死、六步多齐。

产出两张图：
  <out>_存活.png ：每跑的【逐窗口幅值 vs 交棒后时间】叠在一起（转子是否在转的时间线）
  <out>_总览.png ：每跑的汇总柱状（闭环幅值 / 六步散布 / 超时 / 锁判据）

口径：幅值 = 悬空相−V0 在一步内的稳健峰峰（段两端平台中位之差）；
     ≥100 码 = 转子在转；<20 = 停转。见《闭环补偿方案演变记录.md》2c 节。
"""
import sys, os, csv
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager

for _f in ('Noto Sans CJK SC', 'Noto Sans CJK JP'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family'] = _f; break
plt.rcParams['axes.unicode_minus'] = False

F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
PHK = {'A': 'va', 'B': 'vb', 'C': 'vc'}
FS = 24000.0
TPS = 1000.0 / FS * 2000.0


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k-1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


def one(npz):
    csvp = npz.replace('_win.npz', '.csv')
    rows = list(csv.DictReader(open(csvp)))
    cts = np.array([int(r['ts_frame']) for r in rows]) / FS
    cst = np.array([int(r['stage']) for r in rows])
    cdu = np.array([int(r['duty_setpoint']) for r in rows])
    ccur = np.array([int(r['cur']) for r in rows])
    ctmo = np.array([int(r['bemf_timeout']) for r in rows])
    chg = np.where(np.diff(cst) != 0)[0]
    hand = None
    for k in chg:
        if cst[k] == 1 and cst[k+1] == 0:
            hand = cts[k+1]
    if hand is None:
        return None
    z = np.load(npz)
    ST, V0, ts = z['step'], z['v0'], z['ts'] / FS
    PH = {'A': z['va'], 'B': z['vb'], 'C': z['vc']}
    T, A, W = [], [], []
    for wi in range(len(ST)):
        if ts[wi] < hand:
            continue
        vals, ws = [], []
        for i0, i1, s in segs(ST[wi]):
            if s not in F or i0 == 0 or i1 == 512 or i1 - i0 < 16:
                continue
            d = PH[F[s]][wi][i0:i1].astype(float) - V0[wi][i0:i1]
            n = len(d); k = max(2, int(n * 0.2))
            vals.append(abs(np.median(d[:k]) - np.median(d[-k:])))
            ws.append((i1 - i0) * TPS)
        if vals:
            T.append(ts[wi] - hand); A.append(float(np.median(vals)))
            W.append(float(np.median(ws)))
    if not T:
        return None
    Tn = np.array(T); An = np.array(A)
    a0 = An[(Tn >= 0.2) & (Tn <= 0.5)]
    a1 = An[(Tn >= 1.0) & (Tn <= 1.4)]
    sel = np.where((cts >= hand) & (cts <= hand + 4.0))[0]
    return dict(t=np.array(T), a=np.array(A), w=np.array(W), hand=hand,
                dur=(T[-1] if T else 0),
                spread=(np.std(W)/np.mean(W)*100 if len(W) > 2 else np.nan),
                amp0=float(np.median(a0)) if len(a0) else float('nan'),
                amp_end=float(np.median(a1)) if len(a1) else float('nan'),
                cur=float(np.median(ccur[sel])) if len(sel) else float('nan'),
                tmo=int(ctmo[-1]), duty=(sorted(set(cdu[sel].tolist()))[:3] if len(sel) else []))


RUNS = [
    ("开环段（参照）", None),
]
DATA = '/home/lpl/桌面/RM2027'
files = sorted([f for f in os.listdir(DATA) if f.startswith('evt_run_20261013') and f.endswith('_win.npz')])
LABEL = {
    '102836': '轮6 奇偶偏置(原版)', '103545': '12V未上电·诊断', '103733': '基础设施首跑(锁误触发)',
    '103829': '基线 CCR4=130', '103913': '复测#2 奇偶偏置', '104007': '(基线·接线遗漏)',
    '104034': '复测#3 增益+偏置', '105232': '复测#3 干净重跑', '105721': '复测#4 偏置表',
    '105921': '基线 CCR4=200 ★', '110151': 'CCR4=260(越界)', '110547': 'V0 软均值',
    '111152': '常量偏移 +25（隔离实验）',
}
res = []
for f in files:
    stamp = f.split('_')[3][:6]
    try:
        r = one(os.path.join(DATA, f))
    except Exception as e:
        print('  跳过 %s（%s）' % (f, e)); continue
    if r is None:
        continue
    r['label'] = LABEL.get(stamp, f'run {stamp}')
    res.append(r)

print('%-22s %8s %8s %8s %8s %8s %8s' % ('跑', '闭环幅值', '末段幅值', '六步散布', '电流', '超时', '闭环窗口数'))
for r in res:
    print('%-22s %8.0f %8.0f %7.0f%% %8.0f %8d %8d'
          % (r['label'], r['amp0'], r['amp_end'], r['spread'], r['cur'], r['tmo'], len(r['t'])))

fig, ax = plt.subplots(2, 1, figsize=(14, 10), gridspec_kw={'height_ratios': [2, 1]})
for r in res:
    c = '#c0392b' if '★' in r['label'] else ('#7f8c8d' if '诊断' in r['label'] else None)
    ax[0].plot(r['t'], r['a'], lw=1.6, marker='.', ms=3, label=r['label'], color=c)
ax[0].axhline(100, color='g', ls='--', lw=1, alpha=.6)
ax[0].axhline(20, color='r', ls='--', lw=1, alpha=.6)
ax[0].text(0.02, 105, '≥100 码 = 转子在转', color='g', fontsize=9)
ax[0].text(0.02, 8, '<20 码 = 停转', color='r', fontsize=9)
ax[0].set_xlabel('交棒后时间 (s)'); ax[0].set_ylabel('闭环幅值（稳健峰峰，码）')
ax[0].set_title('各方式【闭环存活时间线】：幅值 = 悬空相反电势峰峰（转子在不在转的直接读数）')
ax[0].legend(fontsize=8, ncol=3); ax[0].grid(alpha=.25); ax[0].set_ylim(0, 400)

n = len(res)
xs = np.arange(n)
ax[1].bar(xs - 0.2, [r['amp0'] for r in res], 0.4, label='闭环初段幅值', color='#2980b9')
ax[1].bar(xs + 0.2, [r['amp_end'] for r in res], 0.4, label='闭环末段幅值', color='#e67e22')
ax[1].set_xticks(xs); ax[1].set_xticklabels([r['label'] for r in res], rotation=30, ha='right', fontsize=8)
ax[1].set_ylabel('幅值（码）'); ax[1].legend(fontsize=9); ax[1].grid(alpha=.25, axis='y')
ax[1].set_title('交棒后 0.2~0.5s vs 1.0~1.4s 幅值（后者掉下去 = 交棒后自己崩；两者都在 1.5s 通电窗内，不含滑行）')
plt.tight_layout()
out = sys.argv[1] if len(sys.argv) > 1 else '/home/lpl/桌面/RM2027/各补偿方式对比.png'
plt.savefig(out, dpi=125)
print('\n图 →', out)
