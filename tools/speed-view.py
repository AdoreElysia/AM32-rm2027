#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
speed-view.py —— 闭环整体转速图 + 自动定位"明显突变"时刻（从事件 CSV 画，不驱动电机）。

用法:
  python3 tools/speed-view.py <csv> [<csv2> ...]

转速换算（本板 9N12P = 6 极对）：
  1 拍 = 0.5µs（TIM2）；1 个机械转 = 6 电周期 = 36 次换相
  ⇒ rpm = 60 / (步长拍 × 0.5e-6 × 36) = 3.3333e6 / 步长拍
  校验：步长 2000 拍 → 1667 rpm（与 cap-evt.py 打印的"转速(中位)"一致）

面板：
  ① 转速 vs 时间（Δts 累加轴），开环段灰底、锚定闭环段绿底；
     自动标出【最快加速点】【最猛突变点（|Δrpm| 前 3）】并打印其采样定位
  ② 步长(ms) 原始曲线，红色 × 标 >1.5× 中位的离群步（= 漏过零靠兜底硬踹的那一步）

定位出来的时刻，就用来去 `_win.npz` 里取那一窗的全速波形（见 tools/step-window.py）。
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
TICK = 2500.0 / 30.0
RPM_K = 60.0 / (0.5e-6 * 36)        # rpm = RPM_K / 步长拍


def main():
    csv = sys.argv[1]
    import csv as _csv
    rows = list(_csv.DictReader(open(csv)))
    stg = np.array([int(r['stage']) for r in rows])
    du = np.array([int(r['duty']) for r in rows])
    ts = np.array([int(r['ts_frame']) for r in rows], float)
    tmo = np.array([int(r['bemf_timeout']) for r in rows])
    st = np.array([int(r['step']) for r in rows])
    d = np.diff(ts)
    ok = (d > 0) & (d < 300)                     # 合理的相邻间隔
    t = np.concatenate([[0], np.cumsum(np.where(ok, d, 0))]) / 24000.0
    step_ticks = d * TICK
    rpm = np.where(ok, RPM_K / np.maximum(step_ticks, 1), np.nan)
    med = np.nanmedian(step_ticks)
    out = step_ticks > 1.5 * med                 # 离群步（漏过零/兜底）
    anch = (stg == 0) & (np.abs(du - 300) <= 2)

    fig, ax = plt.subplots(2, 1, figsize=(19, 8), sharex=True,
                           gridspec_kw={'height_ratios': [2, 1]})
    a0 = ax[0]
    tt = t[:-1]                       # 与"相邻间隔"同长度（t 前面补了一个 0）
    a0.fill_between(tt, 0, 9000, where=(stg[:-1] == 1),
                    color='0.85', step='post', label='开环段(stage=1)')
    a0.fill_between(tt, 0, 9000, where=anch[:-1], color='#d5f5e3', step='post',
                    label='锚定闭环(duty==300)')
    a0.plot(t[1:], rpm, lw=.8, color='#2471a3', label='转速（由步长换算）')
    # 最快加速点：滑动 0.2s 窗口内 Δrpm 最大
    W = int(0.2 * 24000)
    best, bi = 0, 0
    n = len(rpm)
    for i in range(0, max(1, n - W), 50):
        seg = rpm[i:i + W]
        if np.all(np.isnan(seg)):
            continue
        dr = np.nanmax(seg) - np.nanmin(seg)
        if dr > best:
            best, bi = dr, i + int(np.nanargmax(seg))
    # 最猛突变：相邻 rpm 差的前 3
    dr = np.abs(np.diff(rpm))
    dr[np.isnan(dr)] = 0
    jumps = sorted(np.argsort(dr)[-3:].tolist())
    for k, j in enumerate(jumps):
        a0.annotate(f'突变{k+1}', xy=(t[j + 1], rpm[j + 1]), xytext=(0, 26),
                    textcoords='offset points', fontsize=8, ha='center', color='#922b21',
                    arrowprops=dict(arrowstyle='->', color='#922b21', lw=.9))
    a0.annotate('最快加速', xy=(t[bi], rpm[bi]), xytext=(0, -34), textcoords='offset points',
                fontsize=8.5, ha='center', color='#1e8449',
                arrowprops=dict(arrowstyle='->', color='#1e8449', lw=1.1))
    a0.axhline(1667, color='#27ae60', ls='--', lw=1, label='步长 2000 拍 ⇒ 1667 rpm')
    a0.set_ylabel('机械转速 (rpm)'); a0.set_ylim(0, 9000)
    a0.set_title(f'闭环整体转速  {os.path.basename(csv)}   '
                 f'（转速 = 3.3333e6 / 步长拍；6 极对、36 次换相/机械转）', fontsize=11)
    a0.legend(fontsize=8.5, loc='upper left'); a0.grid(alpha=.25)
    a1 = ax[1]
    a1.plot(t[1:], step_ticks / 2000.0, lw=.7, color='#7f8c8d', label='步长 (ms)')
    a1.plot(t[1:][out], step_ticks[out] / 2000.0, 'x', ms=4, color='#c0392b',
            label='离群步 >1.5× 中位（漏过零兜底）')
    a1.axhline(med / 2000.0, color='#27ae60', ls=':', lw=1, label=f'中位 {med/2000:.2f} ms')
    a1.set_ylabel('步长 (ms)'); a1.set_xlabel('秒（Δts 累加轴）')
    a1.legend(fontsize=8.5); a1.grid(alpha=.25)
    plt.tight_layout()
    o = os.path.join(os.path.dirname(csv) or '.', '转速_' + os.path.basename(csv)[8:21] + '.png')
    plt.savefig(o, dpi=125)
    print('图 →', o)
    f = lambda i: f'{t[i+1]:6.2f}s/事件{i+1}'
    print(f'  最快加速点 : {f(bi)}   rpm {rpm[bi]:.0f}')
    for k, j in enumerate(sorted(jumps)):
        print(f'  突变{k+1}      : {f(j)}   rpm {rpm[j]:.0f} → {rpm[j+1]:.0f}'
              f'   步长 {step_ticks[j]/2000:.2f} → {step_ticks[j+1]/2000:.2f} ms'
              f'   step {st[j+1]}')

    print(f'  时长 {t[-1]:.2f}s  离群步 {int(out.sum())} 个  转速中位 {np.nanmedian(rpm):.0f} rpm')


main()
