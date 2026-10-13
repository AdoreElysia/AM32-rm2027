# -*- coding: utf-8 -*-
"""更深两层分析（离线，不需要电机）：
A) 每相【原始悬空相读数】的峰峰值 —— 直接检验各通道增益（不用 delta，避免 V0 混进来）
B) 偏置 vs 负载（占空比/电流）—— 用一趟里【斜坡段】占空比自然变化来扫，检验"动态偏置"假设
"""
import sys, csv, os
import numpy as np

FS = 24000.0
TPS = 1000.0 / FS * 2000.0        # 拍/采样
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
PHKEY = {'A': 'va', 'B': 'vb', 'C': 'vc'}


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


def rob_pp(d):
    """次大−次小（对单帧尖峰鲁棒）"""
    d = np.sort(d)
    return (d[-2] - d[1]) if len(d) >= 4 else np.nan


def analyze(npz):
    csvp = npz.replace('_win.npz', '.csv')
    z = np.load(npz)
    ST, V0, ts = z['step'], z['v0'], z['ts'] / FS
    PH = {k: z[k] for k in ('va', 'vb', 'vc')}
    duty = cur = None
    if os.path.exists(csvp):
        rows = list(csv.DictReader(open(csvp)))
        cts = np.array([int(r['ts_frame']) for r in rows]) / FS
        duty = np.array([int(r['duty_setpoint']) for r in rows])
        cur = np.array([int(r['cur']) for r in rows])

    # ---------- A) 每相原始悬空读数的峰峰值 ----------
    raw = {'A': [], 'B': [], 'C': []}
    for wi in range(len(ST)):
        for i0, i1, s in segs(ST[wi]):
            if s not in F or i0 == 0 or i1 == 512 or i1 - i0 < 12:
                continue
            ph = F[s]
            v = PH[PHKEY[ph]][wi][i0:i1].astype(float)
            p = rob_pp((v - v.mean())[3:-3])
            if p == p and p < 4000:
                raw[ph].append(p)
    print("A) 每相【原始悬空读数】峰峰值（码）：")
    base = None
    for ph in ('A', 'B', 'C'):
        if not raw[ph]:
            print("   %s: 无样本" % ph); continue
        m = float(np.median(raw[ph]))
        if ph == 'C':
            base = m
    for ph in ('A', 'B', 'C'):
        if not raw[ph]:
            continue
        m = float(np.median(raw[ph]))
        r = (m / base) if base else float('nan')
        print("   %s: n=%4d 峰峰中位 %6.1f 码  相对 C 相 %.2f" % (ph, len(raw[ph]), m, r))

    # ---------- B) 偏置 vs 占空比（用斜坡段）----------
    if duty is None:
        print("\nB) 无 CSV，跳过"); return
    print("\nB) 竖直中心 vs 占空比（斜坡段，逐窗口；用于检验'偏置随负载变'）：")
    print("   %8s %6s %6s | %s" % ('t(s)', 'duty', 'cur', '逐步竖直中心(码) 1..6'))
    for wi in range(len(ST)):
        sel = np.where((cts >= ts[wi] - 0.05) & (cts <= ts[wi] + 0.05))[0]
        if not len(sel):
            continue
        du = float(np.median(duty[sel])); cu = float(np.median(cur[sel]))
        if du <= 0 or du > 360:          # 只看斜坡段（duty 290~355），跳过锚定 300 之外/停机
            continue
        cs = []
        ok = True
        for s in range(1, 7):
            vals = []
            for i0, i1, ss in segs(ST[wi]):
                if ss != s or i0 == 0 or i1 == 512 or i1 - i0 < 12:
                    continue
                d = np.sort((PH[PHKEY[F[s]]][wi][i0:i1].astype(float) - V0[wi][i0:i1])[3:-3])
                if len(d) >= 10:
                    vals.append((d[-2] + d[1]) / 2)
            cs.append(float(np.median(vals)) if vals else np.nan)
        if all(c != c for c in cs):
            continue
        print("   %8.2f %6.0f %6.0f | %s" % (ts[wi], du, cu,
              ' '.join('%+6.0f' % c if c == c else '     -' for c in cs)))


for f in sys.argv[1:]:
    print("=" * 78)
    print("文件:", os.path.basename(f))
    analyze(f)
