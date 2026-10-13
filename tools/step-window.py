#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
step-window.py —— 【错误的换步窗口】全速过零采样图 + 六步时长基准表。

用法:
  python3 tools/step-window.py <evt_run_*.csv> [看几个窗口=3] [--list]

输入: <csv> 旁边的 _win.npz（512 点/窗 @24kHz 全速采样：VA/VB/VC/V0/电流/step/dbg）
输出: <csv 同目录>/换步窗口_<stamp>.png

为什么看这个：
  ① 窗口里的 `dbg` 是【未钳制的步内时间】（距上次换相过了几个 ADC 帧，1 帧=83.33 拍），
     每采样点一个锯齿 ⇒ 锯齿的【周期】就是那一步的真实长度（没被 commutate() 的
     [ci/4,2ci] 离群值剔除、也没被 ci 的一阶滤波抹过）。
  ② 用 `step` 字节找换相边界、用三路电压算"悬空相−V0"，就能在窗口里直接看到
     【这一步的过零点落在哪、离步中点有多远、有没有被漏掉】。
  ③ 两遍法排序：先用全部窗口建【逐步基准中位长度】，再按"本窗每一步相对自己基准的偏离"
     排序 —— 这样才不会被"六步本身就不一样长"和"窗口边缘半截步"带偏。
     整段没有变号的段（=这一步没有过零，只能靠兜底硬踹）单独标出来。
"""
import sys, os
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
_CJK = None
for _f in ('Noto Sans CJK SC', 'Noto Sans CJK JP'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family'] = _f; _CJK = _f
        break
plt.rcParams['axes.unicode_minus'] = False

TICK_PER_FRAME = 2500.0 / 30.0      # 83.333 拍/帧（1 帧=41.667µs；1 拍=0.5µs）
FLY = {1: 'c', 2: 'a', 3: 'b', 4: 'c', 5: 'a', 6: 'b'}   # 悬空相映射


def delta_of_win(z, w):
    """逐采样点的"悬空相−V0"（相按当点的 step 字节选）。

    ⚠ 只能用来画连续曲线：窗口里的 step 字节比 dbg 回绕（=换相）晚约 4 个采样
      （实测），那几个采样会被按【旧相】算 ⇒ 段首出现假变号。要测"过零在段内哪"
      必须改用 seg_delta()（整段固定一个相）。"""
    st = z['step'][w].astype(int)
    v0 = z['v0'][w].astype(float)
    d = np.full(512, np.nan)
    for k, p in FLY.items():
        m = st == k
        d[m] = z['v' + p][w][m].astype(float) - v0[m]
    return d, st


def seg_delta(z, w, a, b, s):
    """一段内【固定用该段的相】算的"悬空相−V0"（避开 step 字节的滞后）"""
    return z['v' + FLY[s]][w][a:b].astype(float) - z['v0'][w][a:b].astype(float)


def segments(st, db):
    """按 dbg 回绕（换相）切段 → [(起,止,step)]；step 取【段中段众数】，
    因为回绕点处的 step 字节还是旧值（约 4 个采样的滞后）。"""
    edges = [0] + [i for i in range(1, 512) if db[i] < db[i - 1]] + [512]
    out = []
    for i in range(len(edges) - 1):
        a, b = edges[i], edges[i + 1]
        if b - a > 6:
            s = int(np.bincount(st[a + 4:b - 2]).argmax())
        else:
            s = int(st[a])
        out.append((a, b, s))
    return out


def zcpos(seg):
    """一段里悬空相−V0 的【真】过零位置（段内归一化 0~1）；没有变号返回 None。

    ★ 必须跳过换相后的建立瞬态：实测（窗 36 的原始四路值）每次换相后约 2 个采样里
      悬空相是从"被驱动"释放出来的（VA 999→77→350，V0 同时在动），(悬空相−V0)
      会从 +527 猛落到 −398、再回到 ≈−110 —— 这次"变号"是假的，落在段首 0~0.1。
      真过零在段中间（−110 一路走到 +89 时穿过 0，位置 ≈0.52）。
      这也正是固件用 zc_lo = 150 拍（=75µs ≈ 1.8 采样）挡掉"换相振铃"的理由。
    """
    sg = np.where(~np.isnan(seg))[0]
    if len(sg) < 8:
        return None
    skip = max(4, int(0.15 * len(seg)))          # 跳过建立瞬态
    ss = seg[skip:]
    if not ((ss.min() < 0 < ss.max()) or (ss.max() < 0 < ss.min())):
        return None
    ix = np.where(np.diff(np.sign(ss)) != 0)[0]
    return None if not len(ix) else (skip + ix[0]) / (len(seg) - 1)


def main():
    csvp = sys.argv[1]
    nwin = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    z = np.load(csvp.replace('.csv', '_win.npz'))
    NW = z['step'].shape[0]

    # ---------- 第一遍：建逐步基准 ----------
    per = {s: [] for s in range(1, 7)}
    cache = []
    for w in range(NW):
        d, st = delta_of_win(z, w)
        db = z['dbg'][w].astype(int)
        segs = segments(st, db)
        cache.append((d, st, db, segs))
        for k, (a, b, s) in enumerate(segs):
            if k == 0 or k == len(segs) - 1:      # 掐掉窗口边缘的半截步
                continue
            if b - a >= 6:
                per[s].append(b - a)
    base = {s: (np.median(v) if v else np.nan) for s, v in per.items()}
    print('=' * 78)
    print('【六步时长基准】（未钳制，单位：ADC 帧 / 拍）—— 全 %d 个窗统计' % NW)
    print('  step   n     中位(帧)   中位(拍)   相对全体中位')
    allmed = np.nanmedian([base[s] for s in base])
    for s in range(1, 7):
        print(f'   {s}   {len(per[s]):5d}   {base[s]:8.1f}   {base[s]*TICK_PER_FRAME:8.0f}   '
              f'{base[s]/allmed:8.3f}')
    print('=' * 78)

    # ---------- 第二遍：挑"相对自己基准最离谱"的窗 ----------
    rank = []
    for w, (d, st, db, segs) in enumerate(cache):
        # ★ 只看"锚定闭环"的窗：一段一个换相步 ⇒ 21.3ms 的窗里应有 ~20 段。
        #   起步/滑行段一段能长到几百个采样点（一个窗只有 5~7 段），那不是"出错"。
        if len(segs) < 14:
            continue
        worst, miss, det = 0.0, 0, []
        for k, (a, b, s) in enumerate(segs):
            p = zcpos(seg_delta(z, w, a, b, s))
            edge = (k == 0 or k == len(segs) - 1)
            L = b - a
            r = (L / base[s]) if (base[s] and not edge and L >= 6) else 1.0
            det.append((a, b, s, L, r, p, edge))
            if not edge:
                worst = max(worst, abs(r - 1.0))
            if p is None and L >= 8 and not edge:
                miss += 1
        rank.append((miss, worst, w, det))
    rank.sort(key=lambda x: (-x[0], -x[1]))

    sel = rank[:nwin]
    print('前 %d 个"换步出错"窗口：' % nwin)
    for miss, worst, w, det in sel:
        print(f'  窗 {w:4d} seq={int(z["seq"][w]):5d}  段数 {len(det):2d}  '
              f'最大偏离自己的基准 {worst*100:5.1f}%  整段无过零 {miss} 段')
    if '--list' in sys.argv:
        return

    # ---------- 画图 ----------
    fig, ax = plt.subplots(len(sel), 3, figsize=(19.5, 3.7 * len(sel)),
                           gridspec_kw={'width_ratios': [1.5, 1.05, 1.25]})
    if len(sel) == 1:
        ax = ax.reshape(1, -1)
    for r, (miss, worst, w, det) in enumerate(sel):
        d, st, db, segs = cache[w]
        x = np.arange(512)

        a0 = ax[r, 0]
        a0.plot(x, d, lw=.9, color='#2471a3', label='悬空相 − V0')
        a0.axhline(0, color='gray', lw=.8)
        for a, b, s, L, rr, p, edge in det:
            a0.axvline(a, color='#c0392b', lw=.9, alpha=.65)
            if p is not None:
                seg = seg_delta(z, w, a, b, s)
                k = int(round(p * (b - 1 - a)))
                a0.plot(a + k, seg[k], 'o', ms=8, mfc='none', mec='#27ae60', mew=1.9)
                a0.annotate(f'{s}|变号 {p:.3f}', xy=(a + k, seg[k]),
                            xytext=(0, 20 if seg[k] > 0 else -26), textcoords='offset points',
                            fontsize=7, color='#1e8449', ha='center')
            else:
                a0.axvspan(a, b, color='#f5b7b1', alpha=.45)
                a0.annotate(f'{s}|无过零', xy=((a + b) / 2, 0), fontsize=7.5,
                            color='#922b21', ha='center', va='center')
        a0.set_ylabel('悬空相 − V0 (码)')
        a0.set_title(f'窗 {w} / seq {int(z["seq"][w])}：全速"悬空相−V0"   '
                     f'红竖线=换相(按 dbg 回绕)  绿○=该相在段内的变号点  粉带=整段无变号', fontsize=9.5)
        a0.grid(alpha=.25)

        a1 = ax[r, 1]
        a1.plot(x, db, lw=1.1, color='#b03a2e')
        for a, b, s, L, rr, p, edge in det:
            a1.axvline(a, color='#7f8c8d', lw=.7, alpha=.6)
            col = '#922b21' if (p is None and not edge) else '#6c3483'
            a1.text((a + b) / 2, max(db[a:b].max(), 1) * 1.03,
                    f'{s}\n{L*TICK_PER_FRAME:.0f}拍\n{rr:.2f}×', fontsize=6.8,
                    ha='center', va='bottom', color=col)
        a1.set_ylabel('步内时间 (ADC 帧)')
        a1.set_title('未钳制步内时间（ADC 帧）：锯齿周期=该步真实长度，×=相对自己基准', fontsize=9.5)
        a1.grid(alpha=.25)

        a2 = ax[r, 2]
        a2.axis('off')
        head = f'  段  step  长度(拍)  ÷自己基准  段内过零位置'
        lines = [head]
        for a, b, s, L, rr, p, edge in det:
            pos = '  无变号 ⚠' if p is None else f'  {p:.3f}'
            tag = ' (边缘)' if edge else ''
            lines.append(f'  {a:4d}  {s:4d}  {L*TICK_PER_FRAME:8.0f}   {rr:8.2f}   {pos}{tag}')
        a2.text(0, 1.0, f'本窗逐步明细（变号位置：0.5 = 步中点）' + (f'   ⚠ 有 {miss} 段无过零' if miss else ''),
                fontsize=10.5, va='top', color='#922b21' if miss else '#1a5276')
        a2.text(0, 0.92, '\n'.join(lines), fontsize=8.4, va='top')

    ax[-1, 0].set_xlabel('窗内采样点 (512 点 @24kHz = 21.3ms)')
    ax[-1, 1].set_xlabel('窗内采样点')
    fig.suptitle(f'换步出错窗口 —— 全速过零采样    {os.path.basename(csvp)}', fontsize=13)
    plt.tight_layout(rect=[0, 0, 1, 0.965])
    out = os.path.join(os.path.dirname(csvp) or '.',
                       '换步窗口_' + os.path.basename(csvp)[8:23] + '.png')
    plt.savefig(out, dpi=125)
    print('图 →', out)


main()
