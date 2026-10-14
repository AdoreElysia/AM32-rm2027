#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
stab-view.py —— 闭环稳定性视图（从事件 CSV 直接画，不驱动电机）。

用法:
  python3 tools/stab-view.py <csv1> [<csv2> ...]
  # 每个 CSV 一行 4 栏；多个 CSV 叠在一张图上便于对照（如 ON vs OFF）

为什么这样画（口径见 交接/00 §9 与 一切为了校准/03 §十七）：
  · 判分窗口 = 锚定闭环 = `stage==0 且 duty==300`（不能用 stage 定义：
    stage==0 也包含没给油时的空转、以及额度到点后按油门硬拖的 351 工况）
  · 时间轴 = **Δts 累加**（只累加 ≤20×中位间隔的合理间隔），不用绝对 ts
    （ts 会夹进错乱/陈旧事件，把窗口切碎）
  · "漏过零"= `bemf_timeout` 在每秒钟内的**增量**（它是单调计数器；注意收油会被清零，
    所以只能看增量、不能看末值）—— 实测它是**爆发式**的，平均值没有物理意义
"""
import sys, os
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
TICK = 2500.0 / 30.0        # 83.333 拍 / ADC 帧


def load(csvp):
    import csv
    rows = list(csv.DictReader(open(csvp)))
    stg = np.array([int(r['stage']) for r in rows])
    du = np.array([int(r['duty']) for r in rows])
    ts = np.array([int(r['ts_frame']) for r in rows], float)
    cur = np.array([int(r['cur']) for r in rows])
    tmo = np.array([int(r['bemf_timeout']) for r in rows])
    st = np.array([int(r['step']) for r in rows])
    win = (stg == 0) & (np.abs(du - 300) <= 2)
    pair = np.where(win[:-1] & win[1:])[0]
    if len(pair) < 50:
        return None
    dd = np.diff(ts)[pair]
    ok = dd <= max(0.05 * 24000.0, 20.0 * np.median(dd))
    t = np.cumsum(np.where(ok, dd, 0)) / 24000.0
    return dict(csv=csvp, pair=pair, dd=dd, t=t, win=win, cur=cur, tmo=tmo, st=st, n=len(pair))


def main():
    csvs = [a for a in sys.argv[1:] if a.endswith('.csv')]
    if not csvs:
        print('用法: stab-view.py <csv1> [csv2 ...]'); return
    D = [d for d in (load(c) for c in csvs) if d]
    if not D:
        print('❌ 没有一个 CSV 有锚定闭环段'); return
    R = len(D)
    fig, ax = plt.subplots(R, 4, figsize=(20, 3.4 * R), squeeze=False)
    for i, d in enumerate(D):
        t, dd, pair = d['t'], d['dd'], d['pair']
        tag = f"{os.path.basename(d['csv'])[8:21]}  {d['n']} 条 / {t[-1]:.2f}s"
        # ① 换相节拍（按 step 上色）
        a0 = ax[i, 0]
        for s in range(1, 7):
            m = (d['st'][pair + 1] == s)
            a0.plot(t[m], dd[m] * TICK, '.', ms=1.2, alpha=.55, label=f'step{s}')
        a0.axhline(2000, color='#27ae60', ls='--', lw=1, label='基线节拍 2000 拍')
        a0.set_ylabel('换相节拍 (拍)'); a0.set_ylim(0, 4000)
        a0.set_title(f'① 换相节拍（按 step 上色）\n{tag}', fontsize=9.5)
        a0.legend(fontsize=6.5, ncol=4, loc='upper right'); a0.grid(alpha=.25)
        # ② 事件/秒
        a1 = ax[i, 1]
        secs = np.arange(0, np.floor(t[-1]))
        cnt = [int(((t >= a) & (t < a + 1)).sum()) for a in secs]
        a1.bar(secs + .5, cnt, width=.85, color='#2471a3')
        a1.axhline(np.mean(cnt), color='#c0392b', ls=':', lw=1.2,
                   label=f'均值 {np.mean(cnt):.0f}/s（±{100*np.std(cnt)/np.mean(cnt):.1f}%）')
        a1.set_ylabel('事件/秒'); a1.set_title('② 每秒换相数（结构稳定性）', fontsize=9.5)
        a1.legend(fontsize=8); a1.grid(alpha=.25)
        # ③ 漏过零（每秒增量）+ 累计
        a2 = ax[i, 2]
        w = d['win']
        inc = []
        for a in secs:
            m = (t >= a) & (t < a + 1)
            if m.sum() < 5:
                inc.append(0); continue
            mk = np.zeros(len(w), bool); mk[pair[m]] = True; mk[pair[m] + 1] = True
            inc.append(int(d['tmo'][mk].max() - d['tmo'][mk].min()))
        tw = d['tmo'][w]
        n_tmo = int(tw.max() - tw.min())
        a2.bar(secs + .5, inc, width=.85, color='#c0392b')
        a2b = a2.twinx()
        a2b.plot(secs + .5, np.cumsum(inc), color='#7f8c8d', lw=1.2, label='累计')
        a2b.set_ylabel('累计', color='#7f8c8d')
        a2.set_ylabel('漏过零 (次/秒)')
        a2.set_title(f'③ 漏过零【每秒增量】—— 总计 {n_tmo} 次\n（爆发式：集中在少数几秒）', fontsize=9.5)
        a2.grid(alpha=.25)
        # ④ 电流
        a3 = ax[i, 3]
        a3.plot(t, d['cur'][pair + 1], '.', ms=1.2, color='#e67e22', alpha=.6)
        a3.set_ylabel('电流 (电调单位)')
        a3.set_title(f'④ 电流（中位 {np.median(d["cur"][w]):.0f} '
                     f'≈ {np.median(d["cur"][w])*1.8/110:.2f} A）', fontsize=9.5)
        a3.grid(alpha=.25)
        for A in ax[i]:
            A.set_xlabel('秒（Δts 累加轴）')
    fig.suptitle('闭环稳定性视图（判分窗口 = 锚定闭环 duty==300）', fontsize=13)
    plt.tight_layout(rect=[0, 0, 1, 0.965])
    out = os.path.join(os.path.dirname(csvs[0]) or '.',
                       '稳定性_' + '_'.join(os.path.basename(c)[17:23] for c in csvs) + '.png')
    plt.savefig(out, dpi=125)
    print('图 →', out)


main()
