#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
longrun-view.py —— 【长闭环】数据的总览图 + 每个"问题时刻"的全部素材。

为什么要它：短跑（1.5 s）看不出规律；长跑（20 s）里"丢过零"是**每秒 ~26 次**的稳定过程，
必须把整条时间线和每个问题时刻摊开看。

口径 / 坑（都在 一切为了校准/03_本轮结论.md 里）：
  · 时间轴 = 固件 ts 帧号 / 24000 = **真实秒**（已用 CL_RUN_MS=20000 ↔ 实测闭环 19.10 s 标定）。
  · `bemf_timeout` 是**8 位、会绕回** ⇒ 累计值必须加 256×绕回次数，否则低估 2 倍。
  · 事件帧 CRC8 有 1/256 误通过率，`valid()` 不查 duty_setpoint/input ⇒ CSV 里有 ~0.02% 坏帧
    （判据：input 不在 0~2047 视作坏帧，图上灰掉）。
  · CSV 会丢约 2/3 的事件 ⇒ 任何"跨行"比较都必须以 seq 差为步距。
  · 波形只有约 1/3 时间覆盖，而且**丢过零恰好很少落在覆盖内** ⇒ 多数问题时刻没有波形。

用法: python3 tools/longrun-view.py <evt_run_*.csv> [<同名_win.npz>] [输出目录]
"""
import sys, os, csv
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

F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
CSV = sys.argv[1]
NPZ = sys.argv[2] if len(sys.argv) > 2 else CSV.replace('.csv', '_win.npz')
OUTD = sys.argv[3] if len(sys.argv) > 3 else os.path.dirname(CSV)
TAG = os.path.basename(CSV)[8:23]
FS = 24000.0


def num(r, k, d=np.nan):
    try:
        return float(r[k])
    except (TypeError, ValueError, KeyError):
        return d


rows = list(csv.DictReader(open(CSV, encoding='utf-8-sig')))
T = np.array([num(r, 'ts_frame') for r in rows]) / FS        # 秒
stage = np.array([num(r, 'stage') for r in rows])
ci = np.array([num(r, 'ci') for r in rows])
avg = np.array([num(r, 'average_interval') for r in rows])
w = np.array([num(r, 'wait') for r in rows])
tzc = np.array([num(r, 'thiszctime') for r in rows])
lzc = np.array([num(r, 'lastzctime') for r in rows])
dsp = np.array([num(r, 'duty_setpoint') for r in rows])
dut = np.array([num(r, 'duty') for r in rows])
inp = np.array([num(r, 'input') for r in rows])
dsc = np.array([num(r, 'desync') for r in rows]) if 'desync' in rows[0] else np.zeros(len(rows))
stall = np.array([num(r, 'stall_adj') for r in rows])
tmo = np.array([num(r, 'bemf_timeout') for r in rows])
cur = np.array([num(r, 'cur') for r in rows])
step = np.array([num(r, 'step') for r in rows])
# 坏帧：input 不可能 >2047
bad = ~((inp >= 0) & (inp <= 2047))
# timeout 解绕回
tmo_u = np.zeros(len(tmo))
k = 0
for i in range(1, len(tmo)):
    if tmo[i] < tmo[i - 1]:
        k += 1
    tmo_u[i] = tmo[i] + 256 * k
hand = None
for i in range(1, len(stage)):
    if stage[i - 1] == 1 and stage[i] == 0:
        hand = T[i]
        break
inc = [i for i in range(1, len(tmo)) if tmo[i] != tmo[i - 1]]
print('%s：事件 %d，时长 %.1fs，交棒 %s' % (TAG, len(rows), T[-1] - T[0],
                                       ('%.1fs' % hand) if hand else '—'))
print('   超时累计 %d 次（末值 %d + %d 次绕回×256）' % (tmo_u[-1], tmo[-1], k))
if hand:
    m = T >= hand
    print('   闭环 %.1fs ⇒ 超时 %d 次 ⇒ **%.1f 次/秒**' %
          (T[-1] - hand, tmo_u[-1] - tmo_u[np.argmax(m)], (tmo_u[-1] - tmo_u[np.argmax(m)]) / (T[-1] - hand)))
print('   坏帧 %d 个（%.3f%%）' % (bad.sum(), 100 * bad.mean()))

# ---------------- 图一：时间线总览 ----------------
fig, ax = plt.subplots(6, 1, figsize=(17, 15), sharex=True)
if hand:
    for a in ax:
        a.axvspan(T[hand is not None and 0 or 0], T[-1], color='#f39c12', alpha=.06)
ax[0].plot(T, ci, '-', lw=.9, color='#c0392b', label='ci（滤波后，供 waitTime 用）')
ax[0].plot(T, avg, '-', lw=.8, color='#2471a3', alpha=.75, label='average_interval')
ax[0].set_ylabel('换相间隔 (拍)'); ax[0].set_ylim(0, 4500); ax[0].legend(fontsize=8); ax[0].set_title(
    '① 换相间隔：`ci` 被滤波+钳制 ⇒ **看着稳不代表没抖**（这正是"抖动被抹掉"的那个量）', fontsize=10)
ax[1].plot(T, tzc, '.', ms=2, color='#8e44ad', alpha=.6, label='thiszctime（本次过零间隔，原始）')
ax[1].plot(T, lzc, '.', ms=2, color='#16a085', alpha=.5, label='lastzctime')
ax[1].set_ylabel('过零间隔 (拍)'); ax[1].legend(fontsize=8); ax[1].set_title(
    '② 过零间隔（原始、未钳制）：**这条才反映真实的抖**', fontsize=10)
ax[2].plot(T, tmo_u, '-', lw=1.4, color='#e67e22')
ax[2].plot(T[inc], tmo_u[inc], '.', ms=3, color='#c0392b')
ax[2].set_ylabel('丢过零累计'); ax[2].set_title(
    '③ **丢过零（超时兜底）累计**：红点=每一次丢失；闭环内 %.1f 次/秒' %
    (((tmo_u[-1] - tmo_u[np.argmax(T >= (hand or 0))]) / (T[-1] - (hand or 0))) if hand else float('nan')), fontsize=10)
ax[3].plot(T, dsp, '-', lw=1.0, color='#27ae60', label='duty_setpoint')
ax[3].plot(T, dut, '-', lw=1.0, color='#7f8c8d', alpha=.8, label='duty（实际）')
ax[3].plot(T[bad], dsp[bad], 'x', ms=6, color='k', label='坏帧(input>2047)')
ax[3].set_ylabel('占空比'); ax[3].set_ylim(0, 600); ax[3].legend(fontsize=8); ax[3].set_title(
    '④ 占空比：锚定段恒 300；黑×=坏帧（CRC 误通过，别当固件 bug）', fontsize=10)
ax[4].plot(T, inp, '-', lw=.9, color='#2980b9', label='input')
ax[4].plot(T, stall, '-', lw=.9, color='#d35400', alpha=.8, label='stall_adj')
ax[4].plot(T, dsc, '-', lw=1.0, color='#8e44ad', label='desync 计数')
ax[4].plot(T, cur, '-', lw=.9, color='#16a085', alpha=.8, label='cur')
ax[4].set_ylabel('输入/计数'); ax[4].set_ylim(0, 3000); ax[4].legend(fontsize=8, ncol=4); ax[4].set_title(
    '⑤ input / stall_adj / desync / 电流', fontsize=10)
if os.path.exists(NPZ):
    z = np.load(NPZ)
    WT = z['ts'].astype(float) / FS
    ST, DBG = z['step'], z['dbg'].astype(int)
    wl, dp = [], []
    for i in range(len(WT)):
        s = ST[i].astype(int)
        idx = np.where(np.diff(s) != 0)[0]
        wl.append(np.median(np.diff(idx)) * 83.333 if len(idx) >= 2 else np.nan)
        d = DBG[i]
        dips = np.where(np.diff(d) < 0)[0]   # 步内单调增，换相时归零 ⇒ 任何下降=一次换相
        dp.append(np.median(np.diff(dips)) * 83.333 if len(dips) >= 2 else np.nan)
    ax[5].plot(WT, wl, 'o-', ms=3, lw=.9, color='#c0392b', label='波形 step 字节量出的步长')
    ax[5].plot(WT, dp, 's-', ms=3, lw=.9, color='#2471a3', label='dbg 锯齿量出的步长（未钳制）')
    ax[5].fill_between(WT, 0, 1, where=np.isfinite(dp), transform=ax[5].get_xaxis_transform(),
                       color='#2471a3', alpha=.05)
    ax[5].legend(fontsize=8); ax[5].set_title(
        '⑤ 两个独立通道量出的步长（波形 step 字节 vs 新增 dbg）：一致 ⇒ 数据可信；'
        '不一致 ⇒ 那一窗的 step 字节陈旧', fontsize=10)
ax[5].set_ylabel('步长 (拍)')
ax[5].set_xlabel('时间 (s)')
fig.suptitle('长跑总览 · %s' % TAG, fontsize=13)
fig.tight_layout()
p1 = os.path.join(OUTD, '长跑_总览_%s.png' % TAG)
fig.savefig(p1, dpi=118, bbox_inches='tight')
print('总览图 →', p1)

# ---------------- 图二：每个问题时刻的全部素材 ----------------
# 挑 n 个"超时发生"的时刻（优先挑波形覆盖到的）
have = []
if os.path.exists(NPZ):
    z = np.load(NPZ)
    WTS = z['ts'].astype(float)
    cov = [(WTS[i], WTS[i] + 512) for i in range(len(WTS))]
    for i in inc:
        t = num(rows[i], 'ts_frame')
        if any(a <= t < b for a, b in cov):
            have.append(i)
sel = (have[:4] if have else []) + [i for i in inc[:6] if i not in have[:4]]
sel = sel[:6]
if sel:
    n = len(sel)
    fig, ax = plt.subplots(n, 3, figsize=(19, 2.9 * n),
                           gridspec_kw={'width_ratios': [1.35, 1.0, 1.15]})
    if n == 1:
        ax = ax.reshape(1, 3)
    for r_, i in enumerate(sel):
        t0 = T[i]
        a0, a1, a2 = ax[r_][0], ax[r_][1], ax[r_][2]
        # 左：该窗的波形（若有覆盖）—— 画 delta = 悬空相 − V0 与 step
        got = False
        if os.path.exists(NPZ):
            for wi in range(len(WTS)):
                if WTS[wi] <= num(rows[i], 'ts_frame') < WTS[wi] + 512:
                    st = z['step'][wi].astype(int)
                    vph = np.where((st == 1) | (st == 4), z['vc'][wi],
                          np.where((st == 2) | (st == 5), z['va'][wi], z['vb'][wi])).astype(float)
                    d = vph - z['v0'][wi].astype(float)
                    x = (num(rows[i], 'ts_frame') - WTS[wi]) / FS * 1e3 + np.arange(512) / FS * 1e3
                    a0.plot(x, d, '.', ms=2, color='#2980b9', alpha=.6)
                    a1.plot(x, z['dbg'][wi].astype(int), '-', lw=1.0, color='#c0392b')
                    a1.set_xlim(0, 21.5)
                    a0.set_title('窗 %d 的波形：悬空相−V0（问题时刻用红竖线）' % int(z['seq'][wi]), fontsize=9)
                    a1.set_title('同一窗的 dbg（未钳制步内时间，饱和 255）', fontsize=9)
                    got = True
                    break
        if not got:
            a0.text(.5, .5, '该时刻**没有波形覆盖**\n（波形只覆盖约 1/3 时间，\n丢过零又恰好很少落在覆盖内）',
                    ha='center', va='center', fontsize=10, transform=a0.transAxes)
            a0.set_title('窗内无波形', fontsize=9)
            a2.axis('off')
        if got:
            a0.axvline((num(rows[i], 'ts_frame') - WTS[wi]) / 24.0, color='r', lw=1.2)
        # 第三栏：事件流上下文文本
        a1 = ax[r_][2]
        m = slice(max(0, i - 3), min(len(rows), i + 4))
        txt = 'event-stream context  (* = the missed-ZC moment)\n' + \
              'seq     ts(s)   step wait  ci  thiszc lastzc  avg  tmo dsp\n'
        for j in range(*m.indices(len(rows))):
            txt += ('%-7s %7.3f  %d  %5.0f %5.0f %6.0f %6.0f %5.0f %3.0f %5.0f %s\n' % (
                rows[j]['seq'], T[j], step[j], w[j], ci[j], tzc[j], lzc[j], avg[j],
                tmo[j], dsp[j], '  <-- *' if j == i else ''))
        a2.axis('off')
        a2.text(0.0, 1.0, txt, ha='left', va='top', fontsize=7.4, family='monospace',
                transform=a2.transAxes)
        for a_ in (a0, a1):
            a_.grid(alpha=.2)
    fig.suptitle('问题时刻（丢过零）的全部素材 · %s' % TAG, fontsize=13)
    fig.tight_layout(rect=[0, 0, 1, .97])
    p2 = os.path.join(OUTD, '长跑_问题时刻_%s.png' % TAG)
    fig.savefig(p2, dpi=115, bbox_inches='tight')
    print('问题时刻图 →', p2)
