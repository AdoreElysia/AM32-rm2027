#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""win-plot-bemf.py：从窗口流 npz 画六步「悬空相 − V0」波形
用法：python3 tools/win-plot-bemf.py [npz] [输出.png]
"""
import sys, os
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

npz = sys.argv[1] if len(sys.argv) > 1 else '/tmp/esc_branch_raw.npz'
out = sys.argv[2] if len(sys.argv) > 2 else 'firmware/bemf_waveform.png'
d = np.load(npz)
st = d['step'].astype(int); s = st & 7
V = {'A': d['va'].astype(float), 'B': d['vb'].astype(float), 'C': d['vc'].astype(float)}
v0 = d['v0'].astype(float)
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
thr = (d['dbg'].astype(int) >> 1) * 16 if 'dbg' in d.files else np.zeros(len(s))
duty = d['duty'].astype(float) if 'duty' in d.files else np.zeros(len(s))
# 选帧数最多的非零油门段
vals = [v for v in sorted(set(int(x) for x in thr)) if v > 0]
pick = max(vals, key=lambda v: (thr == v).sum()) if vals else 0
m = thr == pick if vals else np.ones(len(s), bool)
idx = np.where(m)[0]
i0, i1 = idx[0], idx[-1]
sl = slice(max(0, i0 - 200), min(len(s), i1 + 200))
t = (np.arange(len(s))[sl] - i0) / 24000.0 * 1000.0   # ms
ss = s[sl]
fl = np.array([V[F[x]][j] if x in F else 0.0 for j, x in zip(range(sl.start, sl.stop), ss)]) - v0[sl]
dd = duty[sl]
dd = np.where(dd > 0, dd, np.nan)
fig, ax = plt.subplots(2, 1, figsize=(12, 7), sharex=True)
ax[0].plot(t, fl, lw=0.8, color='C0', label='floating - V0')
ax[0].plot(t, dd, lw=0.9, color='C1', alpha=0.8, label='duty')
ax[0].set_ylabel('ADC code'); ax[0].legend(loc='upper right'); ax[0].grid(alpha=.3)
ax[0].set_title('throttle=%d  floating phase - V0 & duty (window %.1f ms)' % (pick, t[-1]))
cmap = plt.get_cmap('tab10')
for stp in range(1, 7):
    mm = ss == stp
    if mm.sum() < 3: continue
    ax[1].plot(t[mm], fl[mm], '.', ms=2, color=cmap(stp), label='step %d (%s)' % (stp, F[stp]))
ax[1].set_xlabel('time (ms)'); ax[1].set_ylabel('floating - V0')
ax[1].legend(ncol=3, fontsize=8); ax[1].grid(alpha=.3)
plt.tight_layout(); os.makedirs(os.path.dirname(out) or '.', exist_ok=True)
plt.savefig(out, dpi=130)
print('saved:', out, ' throttle=', pick, ' frames=', int(m.sum()))
