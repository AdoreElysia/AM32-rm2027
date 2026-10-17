#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""adc_cal_baseline.py —— A1：开环稳态窗口的 ADC 基线/归一化分析（只读）

用法:
  python3 tools/adc_cal_baseline.py <win.npz> [--ci 1700] [--outdir DIR] [--label 350/1700]
输出:
  adc_norm_segments.csv  每段原始/归一后的幅值、相位、offset
  adc_norm_steps.csv     六步 g/o 表与验证
  adc_norm_summary.csv   每窗概览
  adc_norm_baseline.png  六栏图

口径:
  · 悬空相映射 step→phase：1:C 2:A 3:B 4:C 5:A 6:B
  · y = V(悬空相) − V0
  · A = P97−P3；offset = 中段(20~80%) y 的中位
  · phase = 中段线性拟合零点 / (n−1)，理想≈0.5，开环带载通常 >0.5
  · 归一化只做“六步相对幅值/偏置”，不改变绝对 Ke；跨 CCR4/采样顺序不可直接比
"""
import os, sys, csv, argparse
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager

for _f in ('Noto Sans CJK SC', 'Noto Sans CJK JP', 'WenQuanYi Zen Hei', 'DejaVu Sans'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family'] = _f
        break
plt.rcParams['axes.unicode_minus'] = False

FP = {1: 'c', 2: 'a', 3: 'b', 4: 'c', 5: 'a', 6: 'b'}
VKEY = {'a': 'va', 'b': 'vb', 'c': 'vc'}


def segments_from_window(win, min_len=8):
    va, vb, vc, v0 = (win['va'].astype(float), win['vb'].astype(float),
                      win['vc'].astype(float), win['v0'].astype(float))
    st = np.asarray(win['step']).astype(int)
    cur = win['cur'].astype(float)
    V = {'a': va, 'b': vb, 'c': vc}
    out = []
    if not len(st):
        return out
    starts = [0]
    for i in range(1, len(st)):
        if st[i] != st[i - 1]:
            starts.append(i)
    starts.append(len(st))
    for a, b in zip(starts[:-1], starts[1:]):
        n = b - a
        s = int(st[a])
        if n < min_len or not (1 <= s <= 6):
            continue
        y = V[FP[s]][a:b] - v0[a:b]
        t = np.arange(n, dtype=float)
        m = (t >= 0.20 * (n - 1)) & (t <= 0.80 * (n - 1))
        if m.sum() < 4:
            continue
        # ★ 只用中段线性拟合的“整步变化量”当幅值，避免换相尖峰污染 P97-P3。
        k, b0 = np.polyfit(t[m], y[m], 1)
        amp = float(abs(k) * (n - 1))
        off = float(k * (n - 1) / 2.0 + b0)          # 拟合线在中点的值
        resid = float(np.sqrt(np.mean((y[m] - (k * t[m] + b0)) ** 2)))
        phase = float((-b0 / k) / (n - 1)) if abs(k) > 1e-12 else None
        out.append(dict(step=s, i0=int(a), i1=int(b - 1), n=int(n),
                        edge=bool(a == 0 or b == len(st)),
                        amp=amp, off=off, mean=float(np.mean(y[m])),
                        phase=phase, slope=float(k), resid=resid,
                        cur=float(np.median(cur[a:b])),
                        dbg_max=float(np.max(win['dbg'][a:b])),
                        y=y.copy()))
    return out


def analyze(path, ci_tgt):
    d = np.load(path, allow_pickle=True)
    nwin = len(d['ts'])
    segs, wins = [], []
    for w in range(nwin):
        win = {k: d[k][w] for k in ('va', 'vb', 'vc', 'v0', 'cur', 'step', 'dbg')}
        ss = segments_from_window(win)
        for s in ss:
            s['win'] = w; s['win_ts'] = int(d['ts'][w]); s['run'] = os.path.basename(path)
            segs.append(s)
        good = [s for s in ss if s['phase'] is not None and -0.2 < s['phase'] < 1.4]
        wins.append(dict(win=w, win_ts=int(d['ts'][w]), nseg=len(ss),
                         nozc=len(ss) - len(good),
                         phase_med=float(np.median([s['phase'] for s in good])) if good else float('nan'),
                         amp_med=float(np.median([s['amp'] for s in ss])) if ss else float('nan'),
                         L_med=float(np.median([s['dbg_max'] for s in ss])) if ss else float('nan'),
                         n_edge=sum(1 for s in ss if s['edge'])))
    # 只用非窗口边界的段求六步表
    core = [s for s in segs if not s['edge'] and s['amp'] > 1e-9]
    step_stats = {}
    for s in range(1, 7):
        v = [r for r in core if r['step'] == s]
        if not v:
            step_stats[s] = dict(n=0)
            continue
        amps = np.array([r['amp'] for r in v])
        offs = np.array([r['off'] for r in v])
        ph = np.array([r['phase'] for r in v if r['phase'] is not None and -0.2 < r['phase'] < 1.4])
        step_stats[s] = dict(
            n=len(v), amp=float(np.median(amps)), off=float(np.median(offs)),
            phase=float(np.median(ph)) if len(ph) else float('nan'),
            p10=float(np.percentile(ph, 10)) if len(ph) else float('nan'),
            p90=float(np.percentile(ph, 90)) if len(ph) else float('nan'))
    # g/o：把六步幅值归一到六步幅值中位；offset 直接减该步中位
    amps = [step_stats[s].get('amp', np.nan) for s in range(1, 7)]
    amps = np.array([a for a in amps if a == a and a > 0])
    amp_ref = float(np.median(amps)) if len(amps) else 1.0
    for s in range(1, 7):
        st = step_stats[s]
        if st.get('n', 0) and st.get('amp', 0) > 0:
            st['gain'] = amp_ref / st['amp']
            st['off_use'] = st['off']
        else:
            st['gain'] = 1.0
            st['off_use'] = 0.0
    # 重算归一化后的相位：y_norm = (y - offset_s) * gain_s
    for r in segs:
        g = step_stats[r['step']]['gain']; o = step_stats[r['step']]['off_use']
        y = r.get('y')
        if y is not None:
            n = r['n']; t = np.arange(n, dtype=float)
            m = (t >= 0.20 * (n - 1)) & (t <= 0.80 * (n - 1))
            yy = (y - o) * g
            if m.sum() >= 4:
                k2, b2 = np.polyfit(t[m], yy[m], 1)
                r['phase_norm'] = float((-b2 / k2) / (n - 1)) if abs(k2) > 1e-12 else None
            else:
                r['phase_norm'] = None
        else:
            r['phase_norm'] = None
        r['gain'] = g; r['off_use'] = o
        r['o_fw'] = -o * g              # 固件形式：d_norm = d*g/1000 + o_fw
        r['g_x1000'] = g * 1000.0
        r['amp_norm'] = r['amp'] * g
    # 每步归一化后的相位中位
    for s in range(1, 7):
        pn = [r['phase_norm'] for r in segs if r['step'] == s and r.get('phase_norm') is not None
              and -0.2 < r['phase_norm'] < 1.4 and not r['edge']]
        step_stats[s]['phase_norm'] = float(np.median(pn)) if pn else float('nan')
    return segs, wins, step_stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('npz')
    ap.add_argument('--ci', type=float, default=1700.0, help='目标 ci（0.5µs 拍）')
    ap.add_argument('--outdir', default='/home/xien/RM2027/analysis')
    ap.add_argument('--label', default='')
    a = ap.parse_args()
    os.makedirs(a.outdir, exist_ok=True)
    segs, wins, steps = analyze(a.npz, a.ci)
    tag = a.label or os.path.basename(a.npz).replace('.npz', '')
    # CSV
    segp = os.path.join(a.outdir, f'adc_norm_segments_{tag}.csv')
    fields = ['run','win','win_ts','step','i0','i1','n','edge','amp','off','mean',
              'phase','slope','resid','cur','dbg_max','gain','off_use','amp_norm','phase_norm']
    with open(segp, 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction='ignore')
        w.writeheader(); w.writerows(segs)
    stepp = os.path.join(a.outdir, f'adc_norm_steps_{tag}.csv')
    with open(stepp, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f); w.writerow(['step', 'n', 'amp_raw', 'amp_norm', 'gain', 'off_raw', 'o_fw', 'g_x1000', 'phase_raw', 'phase_norm', 'p10', 'p90'])
        for s in range(1, 7):
            st = steps[s]
            gain = st.get('gain', 1)
            off = st.get('off_use', 0)
            o_fw = -off * gain
            w.writerow([s, st.get('n', 0), f"{st.get('amp', float('nan')):.3f}",
                        f"{st.get('amp', float('nan')) * gain:.3f}",
                        f"{gain:.4f}", f"{off:.3f}", f"{o_fw:.2f}", f"{gain*1000:.0f}",
                        f"{st.get('phase', float('nan')):.4f}",
                        f"{st.get('phase_norm', float('nan')):.4f}",
                        f"{st.get('p10', float('nan')):.4f}", f"{st.get('p90', float('nan')):.4f}"])
    winp = os.path.join(a.outdir, f'adc_norm_summary_{tag}.csv')
    with open(winp, 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=list(wins[0].keys()) if wins else ['win'])
        w.writeheader(); w.writerows(wins)
    # 打印
    print(f'run={a.npz}\nci={a.ci:.1f} ticks  f={2e6/a.ci:.1f}Hz  rpm={3.3333e6/a.ci:.0f}')
    print(f'windows={len(wins)}  segments={len(segs)}')
    for s in range(1, 7):
        st = steps[s]
        if st.get('n'):
            print(f"  step{s}: n={st['n']:4d} amp={st['amp']:7.1f}  g={st['gain']:.4f}  off_raw={st['off_use']:+7.1f}  "
                  f"g_x1000={st['gain']*1000:.0f}  o_fw={-st['off_use']*st['gain']:+.1f}  phase={st['phase']:.3f}")
    # 图
    fig, ax = plt.subplots(2, 3, figsize=(17, 9))
    sarr = np.arange(1, 7)
    raw = [steps[s].get('amp', np.nan) for s in sarr]
    nrm = [steps[s].get('amp', np.nan) * steps[s].get('gain', 1) for s in sarr]
    ax[0][0].bar(sarr - .18, raw, .36, label='raw', color='#7f8c8d')
    ax[0][0].bar(sarr + .18, nrm, .36, label='normalized', color='#2980b9')
    ax[0][0].set_title('六步幅值：原始 vs 归一'); ax[0][0].set_xlabel('step'); ax[0][0].legend(); ax[0][0].grid(alpha=.25)
    ph = [steps[s].get('phase', np.nan) for s in sarr]
    lo = [steps[s].get('phase', np.nan) - steps[s].get('p10', np.nan) for s in sarr]
    hi = [steps[s].get('p90', np.nan) - steps[s].get('phase', np.nan) for s in sarr]
    ax[0][1].errorbar(sarr, ph, yerr=[lo, hi], fmt='o-', color='#c0392b')
    ax[0][1].axhline(0.5, color='k', ls='--', lw=1); ax[0][1].set_title('六步过零相位（P10~P90）'); ax[0][1].set_xlabel('step'); ax[0][1].grid(alpha=.25)
    ax[0][2].bar(sarr, [steps[s].get('off_use', 0) for s in sarr], color='#8e44ad')
    ax[0][2].set_title('六步直流偏置（相对 V0）'); ax[0][2].set_xlabel('step'); ax[0][2].grid(alpha=.25)
    # 相位直方图
    pr = [r['phase'] for r in segs if r['phase'] is not None and -0.2 < r['phase'] < 1.4]
    pn = [r['phase_norm'] for r in segs if r.get('phase_norm') is not None and -0.2 < r['phase_norm'] < 1.4]
    ax[1][0].hist(pr, bins=40, alpha=.6, label='raw', color='#7f8c8d')
    ax[1][0].hist(pn, bins=40, alpha=.6, label='normalized', color='#2980b9')
    ax[1][0].set_title('相位分布：原始 vs 归一'); ax[1][0].legend(); ax[1][0].grid(alpha=.25)
    # A vs f（单点也画，方便后续叠加）
    ax[1][1].scatter([2e6 / a.ci], [np.nanmedian([r['amp'] for r in segs])], color='#e67e22')
    ax[1][1].set_title('A–f（单点；后续多 ci 叠加成 Ke 线）'); ax[1][1].set_xlabel('f_comm (Hz)'); ax[1][1].set_ylabel('A (码)'); ax[1][1].grid(alpha=.25)
    # 例子波形
    ex = max(segs, key=lambda r: r['n']) if segs else None
    if ex:
        # 重新取该段原始数据
        d = np.load(a.npz); w = ex['win']; win = {k: d[k][w] for k in ('va','vb','vc','v0','step')}
        y = win[VKEY[FP[ex['step']]]].astype(float) - win['v0'].astype(float)
        y = y[ex['i0']:ex['i1'] + 1]
        yn = (y - ex['off_use']) * ex['gain']
        ax[1][2].plot(y, label='raw', color='#7f8c8d')
        ax[1][2].plot(yn, label='normalized', color='#2980b9')
        ax[1][2].set_title(f"示例 step{ex['step']} 悬空相−V0"); ax[1][2].legend(); ax[1][2].grid(alpha=.25)
    plt.tight_layout()
    png = os.path.join(a.outdir, f'adc_norm_baseline_{tag}.png')
    plt.savefig(png, dpi=140)
    print(f'CSV → {segp}\nCSV → {stepp}\nCSV → {winp}\nPNG → {png}')


if __name__ == '__main__':
    main()
