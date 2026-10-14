#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
phase-view.py —— 六步换向的【逐采样详图 + 每相时间】：加速 / 恒速 / 突变 三个时刻各一行。

用法:
  python3 tools/phase-view.py <evt_run_*.csv>       # 用同名 _win.npz

每行 4 栏：
  ① 四路原始波形（VA/VB/VC/V0）全速 512 点（21.3ms）—— 直接看谁被驱动、谁悬空
  ② "悬空相−V0"（每段用固定相，避开 step 字节的滞后）+ 换相竖线 + 段内变号点（真过零）
  ③ ★【每相时间 Gantt】：A/B/C 三相各自是"高边驱动 / 低边驱动 / 悬空"，按时间铺开
     （状态由数据判：段内三相均值里最大=高边、最小=低边、中间=悬空；并与悬空相映射交叉验证）
  ④ 逐步时长（拍）+ 未钳制步内时间锯齿（其周期 = 该步真实长度）

自动挑三个窗口：① 最快加速（与上一窗电流差最大）② 恒速（六步长度最均匀）③ 突变（出现最长离群步）
⚠ 求过零/求相状态都必须【跳过换相后 ~2 个采样的建立瞬态】（实测 VA 999→77→350）。
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
FLY = {1: 'c', 2: 'a', 3: 'b', 4: 'c', 5: 'a', 6: 'b'}
PH = ('a', 'b', 'c')


def segs_of(z, w):
    st = z['step'][w].astype(int)
    db = z['dbg'][w].astype(int)
    res = [i for i in range(1, 512) if db[i] < db[i - 1]]
    bnd = [0] + res + [512]
    out = []
    for k in range(len(bnd) - 1):
        a, b = bnd[k], bnd[k + 1]
        if b - a < 6:
            continue
        s = int(np.bincount(st[a + 4:b - 2]).argmax()) if b - a > 8 else int(st[a])
        out.append((a, b, s))
    return out, db


def seg_delta(z, w, a, b, s):
    return z['v' + FLY[s]][w][a:b].astype(float) - z['v0'][w][a:b].astype(float)


def zcpos(seg):
    if len(seg) < 8:
        return None
    skip = max(4, int(0.15 * len(seg)))
    ss = seg[skip:]
    if not ((ss.min() < 0 < ss.max()) or (ss.max() < 0 < ss.min())):
        return None
    ix = np.where(np.diff(np.sign(ss)) != 0)[0]
    return None if not len(ix) else (skip + ix[0]) / (len(seg) - 1)


def states(z, w, a, b):
    """返回该段的 (高边相, 低边相, 悬空相)，由三相均值判定"""
    skip = max(4, int(0.15 * (b - a)))
    m = {p: float(np.mean(z['v' + p][w][a + skip:b])) for p in PH}
    order = sorted(PH, key=lambda p: m[p])
    return order[2], order[0], order[1], m


def main():
    csvp = sys.argv[1]
    z = np.load(csvp.replace('.csv', '_win.npz'))
    NW = z['step'].shape[0]
    info = []
    for w in range(NW):
        segs, db = segs_of(z, w)
        if len(segs) < 5:
            continue
        L = np.array([b - a for a, b, _ in segs], float)
        cur = float(np.median(z['cur'][w]))
        info.append(dict(w=w, segs=segs, db=db, L=L, med=np.median(L),
                         spread=float(np.std(L) / np.mean(L)), cur=cur,
                         worst=float(L.max() / np.median(L))))
    if not info:
        print('❌ 没有可用窗口'); return
    # ★ 只看【闭环段】：斜坡窗本身"最均匀"、最容易被误选成"恒速"（实测踩到）。
    #   窗不是连续覆盖的（串口带宽只够 ~1/3），所以不能按序号切；
    #   按"每窗步长中位（拍）"取一个绝对阈：闭环的步长天然比斜坡短一截。
    meds = np.array([it['med'] * TICK for it in info])      # ★ 换算成【拍】
    thr = 1.35 * np.percentile(meds, 15)
    keep = [it for it in info if it['med'] * TICK <= thr]
    if len(keep) < 5:
        keep = [it for it in info if it['med'] * TICK <= np.median(meds)]
    info = keep or info
    print(f'（闭环窗判据：步长中位 ≤ {thr:.0f} 拍 ⇒ 候选 {len(info)}/{NW} 个窗，'
          f'实际 {min(it["med"]*TICK for it in info):.0f}~{max(it["med"]*TICK for it in info):.0f} 拍）')
    # 加速：与上一窗电流差最大；恒速：spread 最小；突变：worst 最大
    acc = max(range(1, len(info)), key=lambda i: abs(info[i]['cur'] - info[i - 1]['cur']))
    stc = min(range(len(info)), key=lambda i: info[i]['spread'])
    jmp = max(range(len(info)), key=lambda i: info[i]['worst'])
    picks = [('加速', info[acc]), ('恒速', info[stc]), ('突变', info[jmp])]

    fig, ax = plt.subplots(3, 4, figsize=(21, 11), squeeze=False,
                           gridspec_kw={'width_ratios': [1.35, 1.35, 1.0, 1.0]})
    for r, (tag, it) in enumerate(picks):
        w, segs, db = it['w'], it['segs'], it['db']
        x = np.arange(512)
        # ① 四路原始
        a0 = ax[r, 0]
        for p, c in zip(PH, ('#2471a3', '#c0392b', '#27ae60')):
            a0.plot(x, z['v' + p][w], lw=.8, color=c, label='V' + p.upper())
        a0.plot(x, z['v0'][w], lw=.9, color='0.35', label='V0')
        for a, b, s in segs:
            a0.axvline(a, color='0.7', lw=.6)
        a0.set_ylabel('ADC 码'); a0.legend(fontsize=7, ncol=4)
        a0.set_title(f'{tag}：窗 {w} / seq {int(z["seq"][w])} —— 四路全速（512点=21.3ms）',
                     fontsize=9.5); a0.grid(alpha=.25)
        # ② 悬空相−V0
        a1 = ax[r, 1]
        d = np.full(512, np.nan)
        for a, b, s in segs:
            d[a:b] = seg_delta(z, w, a, b, s)
        a1.plot(x, d, lw=.9, color='#2471a3')
        a1.axhline(0, color='gray', lw=.8)
        for a, b, s in segs:
            a1.axvline(a, color='#c0392b', lw=.9, alpha=.6)
            p = zcpos(seg_delta(z, w, a, b, s))
            if p is not None:
                k = int(round(p * (b - 1 - a)))
                a1.plot(a + k, d[a + k], 'o', ms=6.5, mfc='none', mec='#1e8449', mew=1.6)
        a1.set_ylabel('悬空相 − V0 (码)')
        a1.set_title('② 悬空相−V0 + 换相竖线 + 真过零(绿○)', fontsize=9.5); a1.grid(alpha=.25)
        # ③ 每相状态 Gantt
        a2 = ax[r, 2]
        for i, p in enumerate(PH):
            a2.axhspan(i - .38, i + .38, color='white')
        bad = 0
        for a, b, s in segs:
            hi, lo, fl, _ = states(z, w, a, b)
            if fl != FLY[s]:
                bad += 1
            for i, p in enumerate(PH):
                if p == hi:
                    a2.add_patch(plt.Rectangle((a, i - .38), b - a, .76, color='#c0392b'))
                elif p == lo:
                    a2.add_patch(plt.Rectangle((a, i - .38), b - a, .76, color='#2471a3'))
                else:
                    a2.add_patch(plt.Rectangle((a, i - .38), b - a, .76, color='#f9e79f',
                                               hatch='///', ec='#b7950b'))
                a2.text((a + b) / 2, i - .05, f's{s}', fontsize=6.5, ha='center', va='center')
        a2.set_yticks(range(3)); a2.set_yticklabels(['A 相', 'B 相', 'C 相'])
        a2.set_ylim(-.6, 2.6); a2.set_xlim(0, 512)
        a2.set_title(f'③ 每相状态（红=高边驱动 蓝=低边驱动 黄=悬空）'
                     f'{"  ⚠ 悬空相映射不符 " + str(bad) + " 段" if bad else "  ✓ 与映射一致"}',
                     fontsize=9)
        # ④ 步长
        a3 = ax[r, 3]
        a3b = a3.twinx()
        a3b.plot(x, db, lw=1.0, color='#b03a2e')
        a3b.set_ylabel('步内时间 (ADC 帧)', color='#b03a2e')
        names = [f'{s}' for _, _, s in segs]
        vals = [(b - a) * TICK for a, b, _ in segs]
        a3.bar(range(len(segs)), vals, color='#8e44ad', alpha=.75)
        a3.set_xticks(range(len(segs))); a3.set_xticklabels(names, fontsize=7)
        a3.set_ylabel('每步时长 (拍)', color='#8e44ad')
        a3.set_xlabel('step（按时间顺序）')
        a3.set_title(f'④ 每步时长（中位 {np.median(vals):.0f} 拍）+ 步内时间锯齿', fontsize=9.5)
        print(f'--- {tag} 窗 {w}（seq {int(z["seq"][w])}）逐步明细 ---')
        print(f'   {"步":>3}{"step":>5}{"时长(拍)":>10}{"高边":>5}{"低边":>5}{"悬空":>5}{"过零位置":>9}')
        for a, b, s in segs:
            hi, lo, fl, _ = states(z, w, a, b)
            p = zcpos(seg_delta(z, w, a, b, s))
            print(f'   {a:3d}{s:5d}{(b-a)*TICK:10.0f}{hi.upper():>5}{lo.upper():>5}{fl.upper():>5}'
                  f'{("  " + format(p, ".3f")) if p is not None else "   无":>9}')
    plt.tight_layout()
    o = os.path.join(os.path.dirname(csvp) or '.', '每相时间_' + os.path.basename(csvp)[8:21] + '.png')
    plt.savefig(o, dpi=120)
    print('图 →', o)


main()
