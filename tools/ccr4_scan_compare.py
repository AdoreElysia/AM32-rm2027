#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ccr4_scan_compare.py —— CCR4 扫描对比（只读 _win.npz）

用法:
  python3 tools/ccr4_scan_compare.py [--ci 1700] [--outdir DIR] \
      [200=path_win.npz] [180=path_win.npz] ...

每个点输出:
  原始幅值 A、过零相位、无过零率、L、有效采样率、电流、换相后前几个采样点的
  edge_pp（悬空相−V0 在段首的峰峰值，作为振铃/建立粗指标）。

注意：CCR4 每改一档，ADC 采样点/幅值都会变；本脚本只做“选 CCR4”的粗筛，
      选中后再单独重标该档的六步 g/o。
"""
import os, sys, csv, argparse, re
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import adc_cal_baseline as A

for _f in ('Noto Sans CJK SC', 'Noto Sans CJK JP', 'WenQuanYi Zen Hei', 'DejaVu Sans'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family'] = _f
        break
plt.rcParams['axes.unicode_minus'] = False


def meta_ccr4(npz):
    p = re.sub(r'_win\.npz$', '_meta.txt', npz)
    if not os.path.exists(p):
        return ''
    for line in open(p, encoding='utf-8', errors='ignore'):
        if line.startswith('ESC_CCR4='):
            return line.strip().split('=', 1)[1]
    return ''


def one_point(path):
    d = np.load(path, allow_pickle=True)
    segs = []
    for w in range(len(d['ts'])):
        win = {k: d[k][w] for k in ('va', 'vb', 'vc', 'v0', 'cur', 'step', 'dbg')}
        ss = A.segments_from_window(win)
        core0 = [s for s in ss if not s['edge']]
        if len(core0) < 8:
            continue
        L0 = np.median([s['dbg_max'] for s in core0 if 5 <= s['dbg_max'] <= 60])
        amp0 = np.median([s['amp'] for s in core0 if s['amp'] > 0])
        # 只取 plateau 窗：L≈15~19 帧；避免把斜坡/起动窗算进无过零率
        if not (15 <= L0 <= 19) or amp0 < 250:
            continue
        segs.extend(core0)
    core = [s for s in segs if not s['edge']]
    if not core:
        return None
    amps = np.array([s['amp'] for s in core if s['amp'] > 0])
    ph = np.array([s['phase'] for s in core if s['phase'] is not None and -0.2 < s['phase'] < 1.4])
    Ls = np.array([s['dbg_max'] for s in core if 5 <= s['dbg_max'] <= 60])
    cur = np.array([s['cur'] for s in core])
    edge = np.array([np.ptp(s['y'][:min(5, len(s['y']))]) for s in core if len(s['y']) >= 5])
    noz = sum(1 for s in core if s['phase'] is None or not (-0.2 < s['phase'] < 1.4))
    return dict(n=len(core), amp=float(np.median(amps)) if len(amps) else float('nan'),
                phase=float(np.median(ph)) if len(ph) else float('nan'),
                p10=float(np.percentile(ph, 10)) if len(ph) else float('nan'),
                p90=float(np.percentile(ph, 90)) if len(ph) else float('nan'),
                nozc=100.0 * noz / len(core) if core else float('nan'),
                L=float(np.median(Ls)) if len(Ls) else float('nan'),
                cur=float(np.median(cur)) if len(cur) else float('nan'),
                edge_pp=float(np.median(edge)) if len(edge) else float('nan'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ci', type=float, default=1700.0)
    ap.add_argument('--outdir', default='/home/xien/RM2027/analysis')
    ap.add_argument('runs', nargs='+', help='label=path_win.npz；label 建议写 CCR4 数值')
    a = ap.parse_args()
    rows = []
    for r in a.runs:
        if '=' in r:
            label, path = r.split('=', 1)
        else:
            label, path = os.path.basename(r), r
        if not os.path.exists(path):
            print('⚠ 缺文件', path); continue
        res = one_point(path)
        if res is None:
            print('⚠ 无有效段', path); continue
        ccr4 = label if re.fullmatch(r'[0-9.]+', label) else meta_ccr4(path)
        if ccr4:
            try: ccr4n = float(ccr4)
            except ValueError: ccr4n = None
        else:
            ccr4n = None
        res.update(label=label, ccr4=ccr4, ccr4n=ccr4n, path=path)
        if res['L'] == res['L'] and res['L'] > 0:
            res['frame_us'] = a.ci * 0.5 / res['L']
            res['sample_khz'] = 1000.0 / res['frame_us']
        else:
            res['frame_us'] = float('nan'); res['sample_khz'] = float('nan')
        rows.append(res)
        print(f"{label:>6} ccr4={ccr4 or '?':>4} n={res['n']:4d} L={res['L']:.1f} "
              f"eff={res['sample_khz']:.1f}kHz A={res['amp']:.0f} phase={res['phase']:.3f} "
              f"[{res['p10']:.3f},{res['p90']:.3f}] noZC={res['nozc']:.1f}% cur={res['cur']:.0f} edge_pp={res['edge_pp']:.0f}")
    if not rows:
        return
    os.makedirs(a.outdir, exist_ok=True)
    csvp = os.path.join(a.outdir, 'ccr4_scan_summary.csv')
    keys = ['ccr4', 'label', 'n', 'L', 'frame_us', 'sample_khz', 'amp', 'phase', 'p10', 'p90', 'nozc', 'cur', 'edge_pp', 'path']
    with open(csvp, 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=keys, extrasaction='ignore')
        w.writeheader(); w.writerows(rows)
    print(f'CSV → {csvp}')
    # 图
    x = [r['ccr4n'] if r['ccr4n'] is not None else i for i, r in enumerate(rows)]
    if all(r['ccr4n'] is not None for r in rows):
        xl = 'CCR4 (TIM1 ticks)'
    else:
        xl = 'run index'
    fig, ax = plt.subplots(2, 2, figsize=(13, 9))
    ax[0][0].plot(x, [r['amp'] for r in rows], 'o-', color='#2980b9')
    ax[0][0].set_title('悬空相−V0 幅值 A'); ax[0][0].set_ylabel('码'); ax[0][0].grid(alpha=.25)
    ax[0][1].plot(x, [r['phase'] for r in rows], 'o-', color='#c0392b')
    ax[0][1].fill_between(x, [r['p10'] for r in rows], [r['p90'] for r in rows], color='#c0392b', alpha=.15)
    ax[0][1].axhline(0.5, color='k', ls='--', lw=1); ax[0][1].set_title('过零相位（P10~P90）'); ax[0][1].grid(alpha=.25)
    ax[1][0].plot(x, [r['nozc'] for r in rows], 'o-', color='#8e44ad')
    ax[1][0].set_title('整段无过零率'); ax[1][0].set_ylabel('%'); ax[1][0].grid(alpha=.25)
    ax[1][1].plot(x, [r['edge_pp'] for r in rows], 'o-', color='#e67e22')
    ax[1][1].set_title('段首 5 点峰峰值（振铃/建立粗指标）'); ax[1][1].set_ylabel('码'); ax[1][1].grid(alpha=.25)
    for a0 in ax.flat:
        a0.set_xlabel(xl)
    plt.tight_layout()
    png = os.path.join(a.outdir, 'ccr4_scan_compare.png')
    plt.savefig(png, dpi=140); print(f'PNG → {png}')


if __name__ == '__main__':
    main()
