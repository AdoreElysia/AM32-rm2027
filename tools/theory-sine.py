#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
theory-sine.py —— 按理论锚定相位/频率，只拟合幅值；再试分步补偿并打分。

理论锚定（用户方案，避开我前面所有病态拟合）：
  · 频率：由【每一步自己的步长】给出  ω = 2π/(6·w)     ⇒ 每段用【自己换相时刻】当时间原点，
    所以转速漂移完全不进模型（这是关键：不再需要全局 ω）。
  · 相位：过零点钉在"换相点→下一换相点"的正中 ⇒ 即相对换相点 30° 电角度。于是模型为
        d(t) = A_p·sin( ω·(t−t_c) − π/6 ) + C_p          （t_c = 该段起点的换相时刻）
    A_p 允许有符号（吸收"上升沿/下降沿"的交替）；ω 与 π/6 都是【理论值】不拟合。
  · 只拟合 (A_p, C_p) —— 每相两个参数、线性最小二乘 ⇒ 良态。

然后用理论正弦当基准，试几种【分步补偿】并打分：
   评分① 过零位置与理论(步正中)的偏差；评分② "还像不像正弦"（对理论正弦的残差 + 换相点跳变）
用法: python3 tools/theory-sine.py <win.npz> [输出png]
"""
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
for _f in ('Noto Sans CJK SC', 'Noto Sans CJK JP'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family'] = _f; break
plt.rcParams['axes.unicode_minus'] = False

NPZ = sys.argv[1]
OUT = sys.argv[2] if len(sys.argv) > 2 else NPZ.replace('_win.npz', '_理论正弦.png')
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
PHK = {'A': 'va', 'B': 'vb', 'C': 'vc'}
COL = {'A': '#c0392b', 'B': '#2471a3', 'C': '#27ae60'}
FS = 24000.0
WLO, WHI = 1600.0, 2200.0

z = np.load(NPZ)
ST, V0HW, TS = z['step'], z['v0'], z['ts'].astype(np.int64)
PH = {k: z[k] for k in ('va', 'vb', 'vc')}


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


# ---------- 收集所有"步段"（每段用自己的换相时刻为原点）----------
SEG = []      # dict: wi, i0, i1, step, ph, tc, w, t, d
for wi in range(len(ST)):
    sg = [(i0, i1, s) for i0, i1, s in segs(ST[wi])
          if s in F and i0 != 0 and i1 != 512 and i1 - i0 >= 14]
    if not sg:
        continue
    if not (WLO <= np.median([(i1 - i0) * 83.333 for i0, i1, s in sg]) <= WHI):
        continue
    for i0, i1, s in sg:
        # 掐掉两端各 20%（避开换相瞬态），中间用于拟合
        a = i0 + int((i1 - i0) * 0.20); b = i0 + int((i1 - i0) * 0.80)
        if b - a < 8:
            continue
        t = (np.arange(a, b) - i0) / FS               # ★ 以【该段换相时刻 i0】为原点
        d = PH[PHK[F[s]]][wi][a:b].astype(float) - V0HW[wi][a:b]
        S = 1.0 if (d[-1] - d[0]) >= 0 else -1.0      # ★ 过零方向（由数据斜率判定）
        SEG.append(dict(wi=wi, i0=i0, i1=i1, s=s, ph=F[s], w=(i1 - i0) / FS, t=t, d=d, S=S))

print('收集到 %d 个步段' % len(SEG))

# ---------- ① 只拟合 (A_p, C_p) ----------
# 基函数： sin(ω(t−w/2))  （t 以换相点为原点；理论过零在 t=w/2）
for e in SEG:
    w = 2 * np.pi / (6 * e['w'])
    e['basis'] = e['S'] * np.sin(w * (e['t'] - e['w'] / 2))

A = {}; C = {}
for p in 'ABC':
    E = [e for e in SEG if e['ph'] == p]
    M = np.concatenate([e['basis'] for e in E])
    V = np.concatenate([e['d'] for e in E])
    k = (M @ V) / (M @ M)                    # 只用一列 → 幅值（含符号）
    c0 = float(np.mean([np.mean(e['d']) for e in E]))   # 直流：各段均值的中位性替代
    res = V - k * M
    A[p] = float(k); C[p] = c0
print('\n== ① 理论锚定下的幅值/直流拟合（只 2 个参数/相）==')
for p in 'ABC':
    E = [e for e in SEG if e['ph'] == p]
    V = np.concatenate([e['d'] for e in E]); M = np.concatenate([e['basis'] for e in E])
    r = V - (A[p] * M + C[p])
    print('   %s: 幅值 %+7.0f 码   直流 %+6.1f 码   残差RMS %5.1f 码（占幅值 %.1f%%）   样本 %d'
          % (p, A[p], C[p], np.sqrt(np.mean(r ** 2)), 100 * np.sqrt(np.mean(r ** 2)) / abs(A[p]), len(V)))


def zc_pos(d, t, w_theory=0.5):
    """段内实测过零位置 p（用线性插值，t 以换相点为原点，w=段长）"""
    for k in range(1, len(d)):
        if d[k - 1] * d[k] <= 0 and d[k - 1] != d[k]:
            frac = d[k - 1] / (d[k - 1] - d[k]) if d[k - 1] != d[k] else 0.0
            return (t[k - 1] + frac * (t[k] - t[k - 1])) / (t[-1] + (t[1] - t[0])) if False else None
    return None


# 统一用"段内相对位置"：t 从 a 到 b，但段的完整范围是 i0..i1 ⇒ 换算
def p_measured(e, d):
    tt = e['t'] - 0.0
    for k in range(1, len(d)):
        if d[k - 1] * d[k] <= 0 and d[k - 1] != d[k]:
            frac = d[k - 1] / (d[k - 1] - d[k])
            tz = tt[k - 1] + frac * (tt[k] - tt[k - 1])          # 距 b 起点？—— 这里 tt 是以拟合窗 a 为 0
            # 换算到"以换相点为原点"：拟合窗从 i0+20% 开始
            return tz + 0.0
    return None


# 简化：直接把拟合窗起点相对换相点的偏移加回去
for e in SEG:
    e['off0'] = (np.arange(len(e['t']))[0] + 0.0)  # 占位（下面用 i0 重算）


def p_of_seg(e, d):
    """返回 p = 过零时刻相对换相点 / 段长（理论 = 0.5）"""
    for k in range(1, len(d)):
        if d[k - 1] * d[k] <= 0 and d[k - 1] != d[k]:
            frac = d[k - 1] / (d[k - 1] - d[k])
            tz_fitwin = e['t'][k - 1] + frac * (e['t'][k] - e['t'][k - 1])   # 以拟合窗起点为 0
            tz_seg = tz_fitwin + (e['t'][0])                                  # 以换相点为 0
            return tz_seg / e['w']
    return np.nan


def score(mode):
    """mode: 'none' | 'off' | 'off_slew' | 'gain' ；返回 (每步 p 列表, 残差列表, 换相跳变)"""
    P = {s: [] for s in range(1, 7)}; R = []; JUMP = []
    for e in SEG:
        d = e['d'].copy()
        p = e['ph']
        if mode in ('off', 'off_slew'):
            d = d - corr.get(e['s'], 0.0)
        if mode == 'gain':
            d = d * (np.mean([abs(A[q]) for q in 'ABC']) / abs(A[p]))
        pp = p_of_seg(e, d)
        if pp == pp:
            P[e['s']].append(pp)
        w = 2 * np.pi / (6 * e['w'])
        m = A[p] * e['S'] * np.sin(w * (e['t'] - e['w'] / 2)) + C[p]
        if mode in ('off', 'off_slew'):
            m = m - corr.get(e['s'], 0.0)
        if mode == 'gain':
            m = m * (np.mean([abs(A[q]) for q in 'ABC']) / abs(A[p])) * np.sign(A[p])
        R.append(float(np.sqrt(np.mean((d - m) ** 2))))
    return P, R


# ② 先看"不补偿"的现状
P0, R0 = score('none')
print('\n== ② 现状（不补偿）：过零位置 p（理论=0.500）==')
print('   step:  ' + '  '.join('%5d' % s for s in range(1, 7)))
print('   浮相:  ' + '  '.join('%5s' % F[s] for s in range(1, 7)))
m0 = [np.median(P0[s]) if P0[s] else np.nan for s in range(1, 7)]
print('   p:     ' + '  '.join('%5.3f' % x for x in m0))
print('   ⇒ 相对理论的偏差(中位): ' + '  '.join('%+5.3f' % (x - 0.5) for x in m0))
print('   ⇒ p 散布 σ=%.3f   与理论(0.5)的平均绝对偏差=%.3f   残差中位 %.1f 码'
      % (np.nanstd(m0), np.nanmean(np.abs(np.array(m0) - 0.5)), np.median(R0)))

# ③ 分步补偿：按"把该步的 p 搬到 0.5"所需竖直平移 = -k_local·(p-0.5)·w
#    k_local：用该段数据在过零点附近的斜率（码/秒）
K = {s: [] for s in range(1, 7)}
for e in SEG:
    p = p_of_seg(e, e['d'])
    if p != p:
        continue
    k = np.polyfit(e['t'], e['d'], 1)[0]      # 整段斜率（码/秒，符号含方向）
    K[e['s']].append((k, e['w']))
corr = {s: float(np.median([-k * (np.median(P0[s]) - 0.5) * ww for k, ww in K[s]])) for s in range(1, 7)}
print('\n== ③ 分步补偿量（把该步 p 搬到 0.5 所需竖直平移，码）==')
print('   ' + '  '.join('step%d %+7.1f' % (s, corr[s]) for s in range(1, 7)))
P1, R1 = score('off')
m1 = [np.median(P1[s]) if P1[s] else np.nan for s in range(1, 7)]
print('   ⇒ 补偿后 p: ' + '  '.join('%5.3f' % x for x in m1))
print('   ⇒ p 散布 σ=%.3f   与理论平均绝对偏差=%.3f   残差中位 %.1f 码'
      % (np.nanstd(m1), np.nanmean(np.abs(np.array(m1) - 0.5)), np.median(R1)))

# ④ 增益补偿（把每相幅值拉到共同值）
P2, R2 = score('gain')
m2 = [np.median(P2[s]) if P2[s] else np.nan for s in range(1, 7)]
print('\n== ④ 只做通道级增益归一（保正弦、静态）==')
print('   ⇒ 补偿后 p: ' + '  '.join('%5.3f' % x for x in m2))
print('   ⇒ p 散布 σ=%.3f   与理论平均绝对偏差=%.3f   残差中位 %.1f 码'
      % (np.nanstd(m2), np.nanmean(np.abs(np.array(m2) - 0.5)), np.median(R2)))

print('\n== ⑤ 打分汇总（越小越好）==')
print('   %-22s %10s %14s %12s' % ('方案', 'p散布σ', '|p-0.5|均值', '残差中位(码)'))
for tag, P_, R_ in (('不补偿', P0, R0), ('分步偏置', P1, R1), ('通道级增益', P2, R2)):
    mm = [np.median(P_[s]) if P_[s] else np.nan for s in range(1, 7)]
    print('   %-22s %10.3f %14.3f %12.1f'
          % (tag, np.nanstd(mm), np.nanmean(np.abs(np.array(mm) - 0.5)), np.median(R_)))

# ---------- 图：一个窗口：理论正弦 vs 实测 vs 补偿后 ----------
sel = [e for e in SEG if e['wi'] == SEG[len(SEG) // 2]['wi']]
fig, ax = plt.subplots(2, 1, figsize=(15, 9))
t0 = 0.0
for e in sel:
    x = (e['t'] * FS) / FS * 1e3                       # ms（以换相点为原点）
    ax[0].plot(e['i0'] * 1000 / FS + x, e['d'], '.', ms=2.5, color=COL[e['ph']], alpha=.5)
    w = 2 * np.pi / (6 * e['w'])
    mm = A[e['ph']] * e['S'] * np.sin(w * (e['t'] - e['w'] / 2)) + C[e['ph']]
    ax[0].plot(e['i0'] * 1000 / FS + x, mm, '-', lw=1.4, color=COL[e['ph']])
    ax[1].plot(e['i0'] * 1000 / FS + x, e['d'] - corr[e['s']], '.', ms=2.5, color=COL[e['ph']], alpha=.5)
    ax[1].plot(e['i0'] * 1000 / FS + x, mm - corr[e['s']], '-', lw=1.4, color=COL[e['ph']])
    for A_ in (ax[0], ax[1]):
        A_.axvline(e['i0'] * 1000 / FS, color='0.6', ls=':', lw=.8)
        A_.axvline((e['i0'] + e['i1']) / 2 * 1000 / FS, color='k', ls='--', lw=.8)
ax[0].set_title('上：实测(点) vs 理论锚定正弦(线)   下：加"分步偏置"补偿后   灰点线=换相点  黑虚线=理论过零(步中点)',
                fontsize=10)
for A_ in ax:
    A_.grid(alpha=.22); A_.set_ylabel('悬空相 − V0 (码)')
ax[1].set_xlabel('窗口内时间 (ms)')
plt.tight_layout(); plt.savefig(OUT, dpi=125)
print('\n图 →', OUT)
