#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ramp-fit.py v2 —— "每一步到底是【正弦弧】还是【直线】？" 公平比较。

✱ 统一用归一化时间 u = (采样点 − 换相时刻)/步长 ∈ [0,1]
  一个步 = 60° 电角度（六步栅格），与转速无关 ⇒ 转速漂移不进模型。
✱ 所有"自由"拟合都必须带【过零方向符号 S】：同一相的两个悬空段一升一降，
  不加 S 会互相抵消（这是之前反复踩的坑）。

模型（都在 u∈[0.15,0.95] 上拟合，避开 u<0.12 的换相振铃）：
  A 锚定正弦    d = A·S·sin(π/3·u − π/6) + C       2 参数/相（相位钉死＝理论锚定）
  B 自由相位正弦 d = S·(a·sin(π/3·u) + b·cos(π/3·u)) + C  3 参数/相（相位放开）
  C 锚定斜坡    d = A·S·(2u − 1) + C                2 参数/相（理想梯形反电势的线性过渡段）
  D 逐【步】直线 d = k_s·u + b_s                    2 参数/步
  E 逐【段】直线 （段内噪声地板）
  F 逐【段】二次 （段内曲率地板）
✱ 形状比较另做：把每段按"自己的半量程"归一化后再比，剔除段间差异（前版就是被它盖住了）。

用法: python3 tools/ramp-fit.py <win.npz> [输出png] [hw|sw]
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
OUT = sys.argv[2] if len(sys.argv) > 2 else '/tmp/ramp_fit.png'
V0MODE = sys.argv[3] if len(sys.argv) > 3 else 'hw'
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
PHK = {'A': 'va', 'B': 'vb', 'C': 'vc'}
COL = {'A': '#c0392b', 'B': '#2471a3', 'C': '#27ae60'}
FS = 24000.0
WLO, WHI = 1600.0, 2200.0
ULO, UHI = 0.15, 0.95

z = np.load(NPZ)
ST = z['step']
PH = {k: z[k].astype(float) for k in ('va', 'vb', 'vc')}
V0F = (PH['va'] + PH['vb'] + PH['vc']) / 3.0 if V0MODE == 'sw' else z['v0'].astype(float)
print('V0 = %s' % ('软件均值 (VA+VB+VC)/3' if V0MODE == 'sw' else '硬件虚拟中性点'))


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


SEG = []
for wi in range(len(ST)):
    sg = [(i0, i1, s) for i0, i1, s in segs(ST[wi])
          if s in F and i0 != 0 and i1 != 512 and i1 - i0 >= 14]
    if not sg:
        continue
    if not (WLO <= np.median([(i1 - i0) * 83.333 for i0, i1, s in sg]) <= WHI):
        continue
    for i0, i1, s in sg:
        w = float(i1 - i0)
        u = (np.arange(i0, i1) - i0) / w
        d = PH[PHK[F[s]]][wi][i0:i1] - V0F[wi][i0:i1]
        m = (u >= ULO) & (u <= UHI)
        if m.sum() < 8:
            continue
        S = 1.0 if (d[m][-1] - d[m][0]) >= 0 else -1.0
        SEG.append(dict(wi=wi, i0=i0, i1=i1, s=s, ph=F[s], u=u, d=d, S=S, m=m))
print('步段 %d 个' % len(SEG))

SIN = lambda u: np.sin(np.pi / 3 * u - np.pi / 6)
RMP = lambda u: (2 * u - 1)

# ---------- 池化拟合（同相所有段一起）----------
RES = {}
for p in 'ABC':
    E = [e for e in SEG if e['ph'] == p]
    V = np.concatenate([e['d'][e['m']] for e in E])
    uc = np.concatenate([e['u'][e['m']] for e in E])
    Sc = np.concatenate([np.full(e['m'].sum(), e['S']) for e in E])
    out = {}
    # A 锚定正弦
    M = Sc * SIN(uc); a = (M @ V) / (M @ M); C = V.mean()
    out['A'] = (float(a), float(C), float(np.sqrt(np.mean((V - a * M - C) ** 2))))
    # B 自由相位正弦（带 S）
    M2 = np.stack([Sc * np.sin(np.pi / 3 * uc), Sc * np.cos(np.pi / 3 * uc), np.ones_like(V)], 1)
    ab, *_ = np.linalg.lstsq(M2, V, rcond=None)
    out['B'] = (float(np.hypot(ab[0], ab[1])), float(ab[2]), float(np.degrees(np.arctan2(ab[0], ab[1]))),
                float(np.sqrt(np.mean((V - M2 @ ab) ** 2))))
    # C 锚定斜坡
    M3 = Sc * RMP(uc); a3 = (M3 @ V) / (M3 @ M3); C3 = V.mean()
    out['C'] = (float(a3), float(C3), float(np.sqrt(np.mean((V - a3 * M3 - C3) ** 2))))
    RES[p] = out

print('\n===== ① 池化拟合残差（码；含【段间】差异，所以都偏大）=====')
print('   %-6s %26s %36s %26s' % ('相', 'A 锚定正弦(2参)', 'B 自由相位正弦(3参)', 'C 锚定斜坡(2参)'))
for p in 'ABC':
    a, C, r = RES[p]['A']; A2, C2, ph, r2 = RES[p]['B']; a3, C3, r3 = RES[p]['C']
    print('   %-6s A=%+5.0f C=%+5.1f 残%5.1f | A=%5.0f 相位偏差%+6.1f° 残%5.1f | 半幅=%+5.0f C=%+5.1f 残%5.1f'
          % (p, a, C, r, A2, ph, r2, a3, C3, r3))
print('   ⇒ "A 锚定正弦"与"C 锚定斜坡"残差几乎一样 ⇒ 单步内【正弦与直线无法区分】（见③）')
print('   ⇒ 这些残差主要是【段间】差异（每段直流/幅值不同），不是形状 ✗')
print('   ⇒ 注意 A 与 2×半幅 的关系：正弦峰 A ≈ (6/π)×斜坡半幅 ≈ 1.91×半幅 ⇒ 两者是同一斜率的不同写法')

# ---------- 逐段噪声地板 ----------
rl, rq = [], []
for e in SEG:
    uu, dd = e['u'][e['m']], e['d'][e['m']]
    rl.append(dd - np.polyval(np.polyfit(uu, dd, 1), uu))
    rq.append(dd - np.polyval(np.polyfit(uu, dd, 2), uu))
rl = np.concatenate(rl); rq = np.concatenate(rq)
print('\n===== ② 段内噪声/形状地板（把所有段的段内残差合起来）=====')
print('   逐段直线残差 RMS = %.1f 码   逐段二次残差 RMS = %.1f 码   ⇒ 段内"直线性"很好（二次只再降 %.0f%%）'
      % (np.sqrt(np.mean(rl ** 2)), np.sqrt(np.mean(rq ** 2)),
         100 * (1 - np.sqrt(np.mean(rq ** 2)) / np.sqrt(np.mean(rl ** 2)))))

# ---------- ③ 纯理论：本观测窗内 形状 能不能区分 ----------
uu = np.linspace(ULO, UHI, 4001)
s_n = SIN(uu); r_n = RMP(uu)
s_n = (s_n - s_n.mean()) / ((s_n.max() - s_n.min()) / 2)      # 归一化到"半量程=1"
r_n = (r_n - r_n.mean()) / ((r_n.max() - r_n.min()) / 2)
print('\n===== ③ 纯理论：u∈[%.2f,%.2f] 观测窗内，把两者都归一化到"半量程=1" =====' % (ULO, UHI))
print('   均匀采样下 |正弦 − 直线| 最大 = %.4f（=半量程的 %.2f%%），RMS = %.4f（%.2f%%）'
      % (np.max(np.abs(s_n - r_n)), 100 * np.max(np.abs(s_n - r_n)),
         np.sqrt(np.mean((s_n - r_n) ** 2)), 100 * np.sqrt(np.mean((s_n - r_n) ** 2))))
print('   ⇒ 我们只看得到过零前后 ±25° 这一小段，正弦在这里本来就是一条直线 ⇒【不是取舍问题】')

# ---------- ④ 逐【步】自由直线：给"数据自己的过零"和"每步竖直偏移" ----------
print('\n===== ④ 逐【步】自由直线（带 S）⇒ 数据自己的过零位置 & 每步竖直偏移 =====')
print('   %-6s %5s %8s %10s %9s %10s %12s' % ('步', '浮相', '斜率', '截距', 'u0', '偏0.5', '搬到0.5需偏置'))
off_tab = {}
for s in range(1, 7):
    E = [e for e in SEG if e['s'] == s]
    ks, bs = [], []
    for e in E:
        uu_, dd_ = e['u'][e['m']], e['d'][e['m']]
        k, b = np.polyfit(uu_, dd_, 1)
        ks.append(e['S'] * k); bs.append(e['S'] * b)
    k, b = float(np.median(ks)), float(np.median(bs))
    u0 = -b / k
    # 竖直偏置 δ 使 u0→0.5：δ = −k·(u0−0.5)（在 S=+1 参考系里）
    d_off = -k * (u0 - 0.5)
    off_tab[s] = d_off
    print('   %-6d %5s %8.0f %10.1f %9.3f %+9.3f %+12.1f'
          % (s, F[s], k, b, u0, u0 - 0.5, d_off))
print('   （斜率/截距都乘过 S，统一成"上升段"参考系；偏置>0 表示该步信号整体偏高）')

# ---------- ⑤ 决定性判据：段内二次项（归一化后，乘 S）—— 是"弧"还是"直线"？ ----------
uu0 = np.linspace(ULO, UHI, 512)
vth = SIN(uu0); vth = (vth - vth.mean()) / ((vth.max() - vth.min()) / 2)
a2_sine = np.polyfit(uu0, vth, 2)[0]
vth2 = RMP(uu0); vth2 = (vth2 - vth2.mean()) / ((vth2.max() - vth2.min()) / 2)
a2_line = np.polyfit(uu0, vth2, 2)[0]
print('\n===== ⑤ 段内二次项 a2（归一化到半量程=1，并乘 S 统一到"上升"参考系）=====')
print('   理论上：理想正弦 a2 = %+.3f ；  理想直线 a2 = %+.3f （差 %.3f）'
      % (a2_sine, a2_line, abs(a2_sine - a2_line)))
print('   %-6s %12s %14s %14s' % ('相', '数据 a2 中位', '与正弦之比', '段数'))
A2S = {p: [] for p in 'ABC'}
for p in 'ABC':
    for e in SEG:
        if e['ph'] != p:
            continue
        uu_, dd_ = e['u'][e['m']], e['d'][e['m']]
        v = e['S'] * (dd_ - np.median(dd_))
        half = (np.percentile(v, 97.5) - np.percentile(v, 2.5)) / 2
        A2S[p].append(np.polyfit(uu_, v / half, 2)[0])
    md = float(np.median(A2S[p]))
    print('   %-6s %+12.3f %14.2f %14d' % (p, md, md / a2_sine, len(A2S[p])))
allA = np.concatenate([A2S[p] for p in 'ABC'])
print('   全部段 a2 中位 = %+.3f（正弦 %+.3f，直线 %+.3f）；a2 的段间散布 σ = %.3f'
      % (np.median(allA), a2_sine, a2_line, np.std(allA)))
print('   判据：|数据 a2| 若 ≈ |正弦 a2| ⇒ 形状就是正弦弧；若 ≈ 0 ⇒ 形状是真直线；'
      '若散布 σ 远大于 |中位| ⇒ 曲率不稳定（多半是换相振铃尾巴，不是形状）')

# ---------- 图 ----------
NB = 30
uc = (np.arange(NB) + 0.5) / NB
fig, ax = plt.subplots(1, 4, figsize=(21.5, 5.4))

# 左：归一化折叠形状
for p in 'ABC':
    acc = [[] for _ in range(NB)]
    for e in SEG:
        if e['ph'] != p:
            continue
        d = e['d'][e['m']]; u = e['u'][e['m']]
        d = e['S'] * (d - np.median(d))
        half = (np.percentile(d, 97.5) - np.percentile(d, 2.5)) / 2
        d = d / half
        for ui, di in zip(u, d):
            k = min(NB - 1, int(ui * NB))
            if 0 <= k:
                acc[k].append(di)
    md = np.array([np.median(a) if a else np.nan for a in acc])
    ax[0].plot(uc, md, 'o-', ms=4, lw=1.8, color=COL[p], label='相%s 数据(归一化折叠)' % p)
ax[0].plot(uu, s_n, '-', color='k', lw=1.4, label='正弦（归一化）')
ax[0].plot(uu, r_n, '--', color='#8e44ad', lw=1.4, label='直线（归一化）')
ax[0].set_title('① 归一化到"半量程=1"后：\n正弦(实黑) 与 直线(紫虚) 几乎重合', fontsize=10.5)
ax[0].set_xlabel('u = t/w'); ax[0].set_ylabel('归一化（半量程=1）')
ax[0].grid(alpha=.25); ax[0].legend(fontsize=8)

# 中：一条真实窗口 + 锚定模型
wi_sel = SEG[len(SEG) // 2]['wi']
for e in [x for x in SEG if x['wi'] == wi_sel]:
    x = e['i0'] * 1000 / FS + e['u'] * (e['i1'] - e['i0']) * 1000 / FS
    ax[1].plot(x, e['d'], '.', ms=3, color=COL[e['ph']], alpha=.7)
    a3, C3, _ = RES[e['ph']]['C']
    ax[1].plot(x, a3 * e['S'] * RMP(e['u']) + C3, '-', color='k', lw=1.6)
    a, C, _ = RES[e['ph']]['A']
    ax[1].plot(x, a * e['S'] * SIN(e['u']) + C, '--', color='#8e44ad', lw=1.2)
    ax[1].axvline(e['i0'] * 1000 / FS, color='0.55', ls=':', lw=.9)
    ax[1].axvline((e['i0'] + e['i1']) / 2 * 1000 / FS, color='0.35', ls='--', lw=.9)
ax[1].axhline(0, color='0.7', lw=.8)
ax[1].set_title('② 原始数据（点）\n黑实=锚定斜线，紫虚=锚定正弦\n（两者几乎重合 ⇒ 无法区分）', fontsize=10.5)
ax[1].set_ylabel('码'); ax[1].set_xlabel('窗口内时间 (ms)'); ax[1].grid(alpha=.25)

# 右：每步竖直偏移条形图
steps = list(range(1, 7))
ax[2].bar(steps, [off_tab[s] for s in steps],
          color=[COL[F[s]] for s in steps], alpha=.85)
for s in steps:
    ax[2].text(s, off_tab[s], ' %s\n%+.0f' % (F[s], off_tab[s]),
               ha='center', va='bottom' if off_tab[s] > 0 else 'top', fontsize=8)
ax[2].axhline(0, color='0.4', lw=.9)
ax[2].set_xticks(steps); ax[2].set_xticklabels(['步%d' % s for s in steps])
ax[2].set_title('③ 每步竖直偏移（把该步 u0 搬到 0.5 所需，码）\n颜色=该步悬空相', fontsize=10.5)
ax[2].set_ylabel('码'); ax[2].grid(alpha=.25, axis='y')
ax[3].hist(allA, bins=40, color='#7f8c8d', alpha=.85)
ax[3].axvline(0, color='k', lw=1.6, label='理想直线 a2=0')
ax[3].axvline(a2_sine, color='#8e44ad', lw=1.6, ls='--', label='理想正弦 a2=%.2f' % a2_sine)
ax[3].axvline(np.median(allA), color='#c0392b', lw=1.6, label='数据中位 a2=%+.2f' % np.median(allA))
ax[3].set_title('④ 段内二次项 a2 分布（归一化后）\n看它落在"直线"还是"正弦"上', fontsize=10.5)
ax[3].set_xlabel('a2（半量程=1）'); ax[3].set_ylabel('段数'); ax[3].grid(alpha=.25); ax[3].legend(fontsize=8)
plt.tight_layout(); plt.savefig(OUT, dpi=125, bbox_inches='tight')
print('\n图 →', OUT)
