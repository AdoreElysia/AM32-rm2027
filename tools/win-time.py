#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
win-time.py —— 逐窗口【时间序列】分析（一趟跑的"什么时刻发生了什么"）。

为什么需要它：整段中位数会把不同阶段混在一起。实测教训（2026-10-13）：
用"整段幅值中位"汇报时，把**开环段（转子在转，幅值 111~118 码）**和
**交棒后（已停转，幅值 3~43 码）** 混在一起算，得出"转子一直没转"的错误结论。
逐窗口一看就清楚：失败发生在交棒后 ~50 ms 内。

用法:
  python3 tools/win-time.py <run.npz> [起始时间s] [结束时间s]
  （npz 与同名 csv 自动配对；csv 提供 duty / ci / 电流）
输出：每个窗口一行：t、duty、step变化数、步长中位(拍)、幅值中位(码)、认到过零/完整段数
判定口径（已验证）：**幅值 ≥100 码 ⇒ 转子在转；<20 码 ⇒ 停转**。
"""
import sys, csv, os
import numpy as np

F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
NPZ = sys.argv[1]
CSV = NPZ.replace('_win.npz', '.csv')
T0 = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
T1 = float(sys.argv[3]) if len(sys.argv) > 3 else 1e9
FS = 24000.0
TICK_PER_SAMPLE = 1000.0 / FS * 2000.0        # 1 采样 = 41.67µs = 83.33 拍


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k-1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


z = np.load(NPZ)
ST, V0, ts = z['step'], z['v0'], z['ts'] / FS
PH = {'A': z['va'], 'B': z['vb'], 'C': z['vc']}

cts = cds = cci = ccur = None
if os.path.exists(CSV):
    rows = list(csv.DictReader(open(CSV)))
    cts = np.array([int(r['ts_frame']) for r in rows]) / FS
    cds = np.array([int(r['duty_setpoint']) for r in rows])
    cci = np.array([int(r['ci']) for r in rows])
    ccur = np.array([int(r['cur']) for r in rows])

print("窗口    t(s)  duty    步长(拍)  **幅值(码)**  认到/段   ci")
print("-" * 68)
for wi in range(len(ST)):
    if not (T0 <= ts[wi] <= T1):
        continue
    st = ST[wi]
    full = [(i0, i1, s) for i0, i1, s in segs(st)
            if s in F and i0 != 0 and i1 != 512 and i1 - i0 >= 10]
    du = ci_ = cu_ = -1
    if cts is not None:
        sel = np.where((cts >= ts[wi] - 0.05) & (cts <= ts[wi] + 0.05))[0]
        if len(sel):
            du, ci_, cu_ = (int(np.median(cds[sel])), int(np.median(cci[sel])),
                            int(np.median(ccur[sel])))
    if not full:
        print("%4d %7.2f %5d %9s %12s %9s %5d"
              % (int(z['seq'][wi]), ts[wi], du, "—", "—", "—", ci_))
        continue
    w, A, nz = [], [], 0
    for i0, i1, s in full:
        d = (PH[F[s]][wi][i0:i1].astype(float) - V0[wi][i0:i1])
        w.append((i1 - i0) * TICK_PER_SAMPLE)
        dd = d[4:-4]
        if len(dd) >= 4:
            ds = np.sort(dd); A.append(ds[-2] - ds[1])
            if np.any(dd[:-1] * dd[1:] <= 0):
                nz += 1
    am = np.median(A) if A else float('nan')
    tag = ' ✅转' if am >= 100 else (' ✗停' if am < 20 else '')
    print("%4d %7.2f %5d %9.0f %12.0f%s %4d/%-3d %5d"
          % (int(z['seq'][wi]), ts[wi], du, np.median(w), am, tag, nz, len(full), ci_))
