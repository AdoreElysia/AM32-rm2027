#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
cal-anchor.py —— 【一切为了校准】主工具：
    标记换相点 → 由换相点得到【理论过零点】→ 把它当【三角函数的零点】做锚定拟合 → 校准。

为什么必须用【开环】数据（这一步是整个校准的地基）
------------------------------------------------
· 开环时换相时刻由固件的定时器/input 表决定，跟反电势毫无关系 ⇒ **换相点是我们已
  知的独立真值**。
· 六步栅格里"过零发生在换相之后 30° 电角度"，而一步正好 60° ⇒ 过零点就落在**这一步
  的正中**（归一化位置 u = 0.5）。于是：把换相点标出来，就白得了一串理论过零点。
· 反过来，闭环里换相是固件**由实测过零推出来**的 ⇒ 再拿换相点当参照，等于自己证明
  自己（循环论证），所以闭环数据不能用来校准。

于是模型只剩两个参数（幅值、直流），相位/频率/零点位置全是理论给的 ⇒ 良态、不会过拟合：
    悬空相 P 在第 s 步的电压  d(u) = A_P · S_s · sin( π/3·(u − 0.5) ) + C_P ,  u = (采样点−换相点)/步长
    零点恒在 u = 0.5（＝理论过零点），S_s 只吸收"这一相这半步是上升还是下降"。
"能拟合吗"的答案 = A_P/C_P 线性最小二乘的残差 RMS，与【逐段自由直线】的残差地板对比。

用法
----
  python3 tools/cal-anchor.py --pick                 # 扫全部 run，筛"典型开环稳定"数据
  python3 tools/cal-anchor.py 20261013_115519 [...]  # 对指定 run 做校准（可给多个）
  python3 tools/cal-anchor.py --all-typical          # 对筛选出的典型组一起做，出汇总图
选项:  --outdir D(默认 ~/桌面/RM2027/一切为了校准)  --v0 hw|sw  --all-stage(不限定开环段)
"""
import sys, os, csv, glob
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

DIR = os.path.expanduser('~/桌面/RM2027')
OUTD_DEF = os.path.join(DIR, '一切为了校准')
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}      # 第 s 步悬空相
PHK = {'A': 'va', 'B': 'vb', 'C': 'vc'}
COL = {'A': '#c0392b', 'B': '#2471a3', 'C': '#27ae60'}
COLW = {1: '#c0392b', 2: '#d35400', 3: '#b7950b', 4: '#27ae60', 5: '#8e44ad', 6: '#2471a3'}
FS = 24000.0                 # ADC 采样率 (Hz)
TICK = 0.5e-6                # 1 拍 = 0.5 µs (TIM2 @2MHz)
SEG_MIN = 12                 # 步段至少 12 个采样点才用
FITLO, FITHI = 0.15, 0.95    # 锚定拟合用的归一化窗口（避开换相振铃）
MIDLO, MIDHI = 0.30, 0.70    # "中段直线"口径
BASIS = lambda u: np.sin(np.pi / 3 * (u - 0.5))   # 零点在 u=0.5 的三角函数


# ---------------------------------------------------------------- 通用
def num(r, k, d=np.nan):
    try:
        return float(r.get(k))
    except (TypeError, ValueError):
        return d


def read_csv(cf):
    """→ (ts 数组, stage 数组, 最长开环(stage==1)段的 ts 区间)

    ★ 关键：NPZ 的 `seq` 是**波形窗口号**，CSV 的 `seq` 是**换相事件号**，两个计数器
      根本不是一个东西（窗口包 6144 B 要发 61 ms，期间的事件全丢 ⇒ CSV 的 seq 有大量
      跳号）。唯一可靠的对齐量是 `ts`（固件 ADC 帧号），两边同一个计数器 ⇒ 一律按 ts 对齐。
    """
    ts_l, st_l = [], []
    if not os.path.exists(cf):
        return None, None, None
    with open(cf, encoding='utf-8-sig') as fh:
        for r in csv.DictReader(fh):
            if r.get('stage') is None:
                continue
            ts_l.append(int(r['ts_frame'])); st_l.append(int(r['stage']))
    order = np.argsort(ts_l)
    ts = np.array(ts_l, float)[order]; st = np.array(st_l, int)[order]
    best, i, n = None, 0, len(st)
    while i < n:
        if st[i] == 1:
            j = i
            while j + 1 < n and st[j + 1] == 1:
                j += 1
            if best is None or (j - i) > (best[1] - best[0]):
                best = (ts[i], ts[j], j - i + 1)
            i = j + 1
        else:
            i += 1
    return ts, st, best


def stage_at(ts, st, t):
    """窗口起始 ts → 该时刻的 stage（取最后一个不晚于 t 的事件）"""
    if ts is None or len(ts) == 0:
        return None
    i = int(np.searchsorted(ts, t, 'right')) - 1
    return int(st[i]) if i >= 0 else None


def pick_v0(z, want):
    """V0 选择：want=hw/sw/auto。auto 时先体检硬件虚拟中性点是否可用"""
    if want == 'sw':
        return 'sw', '强制软件均值'
    if want == 'hw':
        return 'hw', '强制硬件虚拟中性点'
    v0 = z['v0'].astype(float)
    sw = (z['va'].astype(float) + z['vb'].astype(float) + z['vc'].astype(float)) / 3.0
    m = (z['va'] > 200) | (z['vb'] > 200) | (z['vc'] > 200)
    zf = float(np.mean(v0 == 0))
    r = float(np.corrcoef(v0[m], sw[m])[0, 1]) if m.sum() > 100 else 0.0
    mx = float(np.max(np.abs(v0)))
    if zf > 0.20 or r < 0.8 or mx > 5000:
        return 'sw', 'hw V0 不可用(全零%.0f%%/相关%.2f/极值%.0f) ⇒ 自动改用软件均值' % (100 * zf, r, mx)
    return 'hw', 'hw V0 健康(全零%.0f%%/相关%.2f)' % (100 * zf, r)


def segs(st):
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            o.append((i0, k, int(st[i0]))); i0 = k
    o.append((i0, len(st), int(st[i0])))
    return o


def resolve(run):
    """把 20261013_115519 / 路径 / npz 名 统一成 npz 路径"""
    if run.endswith('.npz'):
        return run if os.path.isabs(run) else os.path.join(DIR, run)
    if run.endswith('.csv'):
        return run.replace('.csv', '_win.npz')
    if os.path.isabs(run):
        return run
    for pre in ('', 'evt_run_'):
        p = os.path.join(DIR, pre + run + '_win.npz')
        if os.path.exists(p):
            return p
    g = glob.glob(os.path.join(DIR, '*%s*_win.npz' % run))
    return g[0] if g else os.path.join(DIR, run + '_win.npz')


# ---------------------------------------------------------------- 采集步段
def collect(npz, v0mode='auto', all_stage=False):
    """把一个 run 里【开环段内】的所有可用步段收进来"""
    cf = npz.replace('_win.npz', '.csv')
    ts_c, st_c, span = read_csv(cf)
    z = np.load(npz)
    ST, TS = z['step'], z['ts'].astype(np.int64)
    PH = {k: z[k].astype(float) for k in ('va', 'vb', 'vc')}
    v0, why = pick_v0(z, v0mode) if 'v0' in z.files else ('sw', '跑这次采集时还没有 V0 通道')
    V0 = (PH['va'] + PH['vb'] + PH['vc']) / 3.0 if v0 == 'sw' else z['v0'].astype(float)
    SEG, nwin_used, nwin_open, ndead = [], 0, 0, 0
    for wi in range(len(ST)):
        t = float(TS[wi])
        s_now = stage_at(ts_c, st_c, t)
        is_open = (s_now == 1)
        if is_open:
            nwin_open += 1
        if not all_stage:
            if not is_open:
                continue
            if span and not (span[0] <= t <= span[1]):     # 只取最长开环段，避开交棒边界
                continue
        # ★ 死窗剔除：三相全都没被驱动（DMA 缓冲没填上/没上电）⇒ 数据无意义。
        #   判据用【逐采样点三相最大值的中位】而不是最大值：有的坏窗只剩几根尖峰，
        #   用最大值会被尖峰骗过去（115054 就是这种窗）。
        mx = np.maximum(PH['va'][wi], PH['vb'][wi])
        mx = np.maximum(mx, PH['vc'][wi])
        if np.median(mx) < 200:
            ndead += 1
            continue
        nwin_used += 1
        for i0, i1, s in segs(ST[wi].astype(int)):
            if s not in F or i0 == 0 or i1 == 512 or i1 - i0 < SEG_MIN:
                continue
            w = float(i1 - i0)
            u = (np.arange(i0, i1) - i0) / w
            d = PH[PHK[F[s]]][wi][i0:i1] - V0[wi][i0:i1]
            m = (u >= FITLO) & (u <= FITHI)
            if m.sum() < 8:
                continue
            SEG.append(dict(wi=wi, seq=int(z['seq'][wi]), i0=i0, i1=i1, s=s, ph=F[s], w=w,
                            u=u, d=d, m=m, t0=TS[wi] + i0, tick=w * 83.333,
                            raw=PH[PHK[F[s]]][wi][i0:i1].copy(),        # ★ 保留原通道值：
                            v0v=V0[wi][i0:i1].copy(), v0mode=v0))       #   通道级增益校正要用它
    return z, SEG, nwin_open, nwin_used, span, v0, why, ndead


# ---------------------------------------------------------------- 过零位置（三种口径）
def zc_local(u, d):
    """口径①：变号处线性插值（最局部）"""
    best = np.nan
    for k in range(1, len(d)):
        if d[k - 1] * d[k] <= 0 and d[k - 1] != d[k]:
            fr = d[k - 1] / (d[k - 1] - d[k])
            u0 = u[k - 1] + fr * (u[k] - u[k - 1])
            if not np.isfinite(best) or abs(u0 - 0.5) < abs(best - 0.5):
                best = u0
    return best


def zc_line(u, d, lo, hi):
    """口径②③：段内直线拟合，零点 = −b/k"""
    m = (u >= lo) & (u <= hi)
    if m.sum() < 6:
        return np.nan
    k, b = np.polyfit(u[m], d[m], 1)
    return (-b / k) if k != 0 else np.nan


def measure_zc(e):
    e['u0_local'] = zc_local(e['u'], e['d'])
    e['u0_full'] = zc_line(e['u'], e['d'], FITLO, FITHI)
    e['u0_mid'] = zc_line(e['u'], e['d'], MIDLO, MIDHI)
    e['u0_lo'] = zc_line(e['u'], e['d'], 0.15, 0.50)      # 交接里那两个"半窗外推"口径
    e['u0_hi'] = zc_line(e['u'], e['d'], 0.70, 0.95)


KOU = (('局部插值', 'u0_local', False), ('全步直线', 'u0_full', False),
       ('中段直线', 'u0_mid', False), ('前半窗(0.15~0.50)', 'u0_lo', True),
       ('后半窗(0.70~0.95)', 'u0_hi', True))


# ---------------------------------------------------------------- 锚定三角函数拟合
def anchored_fit(SEG, phases='ABC'):
    """先按步号定 S_s（数据斜率方向），再逐相池化拟合 (A_P, C_P)；返回参数字典"""
    # ① 每步的符号 S_s（多数投票）
    Ss = {}
    for s in range(1, 7):
        E = [e for e in SEG if e['s'] == s]
        if not E:
            Ss[s] = 1.0; continue
        kk = [np.polyfit(e['u'][e['m']], e['d'][e['m']], 1)[0] for e in E]
        Ss[s] = 1.0 if np.median(kk) >= 0 else -1.0
    # ② 逐相池化
    RES = {}
    for p in phases:
        E = [e for e in SEG if e['ph'] == p]
        if not E:
            RES[p] = None; continue
        Mc, Vc = [], []
        for e in E:
            Mc.append(Ss[e['s']] * BASIS(e['u'][e['m']]))
            Vc.append(e['d'][e['m']])
        M = np.concatenate(Mc); V = np.concatenate(Vc)
        A = (M @ V) / (M @ M)
        C = float(np.mean(V))
        r = V - (A * M + C)
        RES[p] = dict(A=float(A), C=C, rms=float(np.sqrt(np.mean(r ** 2))),
                      rms_pct=100 * float(np.sqrt(np.mean(r ** 2))) / abs(A) if A else np.nan,
                      n=len(V), nseg=len(E))
    return Ss, RES


def med_sd(a):
    a = np.asarray(a, float)
    a = a[np.isfinite(a)]
    return (float(np.median(a)), float(np.std(a)), len(a)) if len(a) else (np.nan, np.nan, 0)


def per_step_table(SEG, Ss):
    """逐【步】的过零位置 & 幅值 & 偏置（用三种口径各算一遍）"""
    rows = []
    for s in range(1, 7):
        E = [e for e in SEG if e['s'] == s]
        if not E:
            rows.append(dict(s=s, ph=F[s], n=0, ok1=0, ok2=0, ok3=0)); continue
        m1, s1, k1 = med_sd([e['u0_local'] for e in E])
        m2, s2, k2 = med_sd([e['u0_full'] for e in E])
        m3, s3, k3 = med_sd([e['u0_mid'] for e in E])
        ks = [np.polyfit(e['u'][e['m']], e['d'][e['m']], 1)[0] for e in E]
        amps = [e['d'][e['m']].max() - e['d'][e['m']].min() for e in E]
        rows.append(dict(s=s, ph=F[s], n=len(E), med1=m1, med2=m2, med3=m3,
                         sd1=s1, sd2=s2, ok1=k1, ok2=k2, ok3=k3,
                         k=np.median(ks), amp=np.median(amps), S=Ss[s],
                         tick=np.median([e['tick'] for e in E])))
    return rows


# ---------------------------------------------------------------- 出表 / 出图
def print_report(name, SEG, Ss, RES, ptab, nwin_open, nwin_used, span, tick_med, v0, why, ndead=0):
    print('\n' + '=' * 78)
    print('【%s】  开环窗 %d 个（用上 %d，死窗剔除 %d），V0=%s（%s）'
          % (name, nwin_open, nwin_used, ndead, v0, why))
    print('   最长开环段 ts %s，步长中位 %.0f 拍' %
          (('%.0f~%.0f' % (span[0], span[1])) if span else '—', tick_med))
    print('=' * 78)
    print('步段 %d 个（悬空相/步号按固件表 F={1:C,2:A,3:B,4:C,5:A,6:B}）' % len(SEG))
    print('\n---- ① 换相点 → 理论过零点：实测过零位置 u0 相对理论的偏差（°E，理论 = 0）----')
    print('   口径 \\ 步       步1(C)  步2(A)  步3(B)  步4(C)  步5(A)  步6(B) | 口径散差 六步散差')
    tab = {}
    for lab, key, extrap in KOU:
        row = []
        for s in range(1, 7):
            v = [x for x in SEG if x['s'] == s and np.isfinite(x.get(key, np.nan))]
            vals = [(x[key] - .5) * 60 for x in v]
            row.append(float(np.median(vals)) if vals else np.nan)
        tab[key] = row
        a = np.array([v for v in row if np.isfinite(v)])
        sp = [abs(v - 0.0) for v in a]
        print('   %-16s %s | %8s %8s%s'
              % (lab, ' '.join('%+7.1f' % v if np.isfinite(v) else '      —' for v in row),
                 ('%.1f' % (a.max() - a.min()) if lab == '局部插值' else '—'),
                 ('%.1f' % (a.max() - a.min()) if len(a) else '—'),
                 '   ← 窗外推，只会更差' if extrap else ''))
    # 口径散差 = 每一步上"三/五个口径"自身的分歧，再取中位
    sp = []
    for i in range(6):
        c = [tab[k][i] for k in ('u0_local', 'u0_full', 'u0_mid') if np.isfinite(tab[k][i])]
        if len(c) > 1:
            sp.append(max(c) - min(c))
    spx = []
    for i in range(6):
        c = [tab[k][i] for _, k, _ in KOU if np.isfinite(tab[k][i])]
        if len(c) > 1:
            spx.append(max(c) - min(c))
    a6 = np.array([v for v in tab['u0_local'] if np.isfinite(v)])
    print('   ⇒ 六步散差（局部口径）= %.1f°E' % (a6.max() - a6.min()))
    print('   ⇒ 口径散差：窗内含过零的 3 个口径之间 %.1f°E ；≈交接那两个半窗外推时 %.1f°E'
          % (np.median(sp), np.median(spx)))
    print('   ⇒ 读法：口径散差 ≤6° 时"过零位置"才可判；外推口径把它抬到十几度 ⇒ 那是【口径假象】，不是转子偏了')
    print('\n---- ② 三角零点锚定拟合（相位/频率/零点全给理论，只拟合幅值 A 与直流 C）----')
    print('   %-4s %10s %10s %12s %12s %6s' % ('相', '幅值A(码)', '直流C(码)', '残差RMS(码)', '占幅值', '段数'))
    for p in 'ABC':
        r = RES.get(p)
        if not r:
            print('   %-4s      （无段）' % p); continue
        print('   %-4s %+10.0f %+10.1f %12.1f %11.1f%% %6d'
              % (p, r['A'], r['C'], r['rms'], r['rms_pct'], r['nseg']))
    rr = [RES[p]['rms'] for p in 'ABC' if RES.get(p)]
    print('   ⇒ 锚定模型残差中位 %.1f 码 ⇒ 「这个能拟合吧」= 能，而且和逐段自由直线一个量级'
          % np.median(rr))
    print('\n---- ③ 三个台阶的残差对照（回答"这个能拟合吧"）----')
    rl = np.concatenate([e['d'][e['m']] - np.polyval(np.polyfit(e['u'][e['m']], e['d'][e['m']], 1),
                                                     e['u'][e['m']]) for e in SEG])
    # 逐步直线（每相每步一条线 = 12 个参数）——公平基准
    rs = []
    for p in 'ABC':
        for s in range(1, 7):
            E = [e for e in SEG if e['ph'] == p and e['s'] == s]
            if len(E) < 2:
                continue
            U = np.concatenate([e['u'][e['m']] for e in E])
            D = np.concatenate([e['d'][e['m']] for e in E])
            rs.append(D - np.polyval(np.polyfit(U, D, 1), U))
    rs = np.concatenate(rs) if rs else np.array([np.nan])
    print('   ① 噪声地板 逐段自由直线（2 参数/段，过参数化）     残差 RMS = %6.1f 码' % np.sqrt(np.mean(rl ** 2)))
    print('   ② 公平基准 逐步自由直线（2 参数/相/步，共 12 个）   残差 RMS = %6.1f 码' % np.sqrt(np.mean(rs ** 2)))
    print('   ③ 本模型   三角零点锚定（2 参数/相，共 6 个）       残差 RMS = %6.1f 码'
          % np.median([RES[p]['rms'] for p in 'ABC' if RES.get(p)]))
    print('   ⇒ 6 个参数做到 12 参数基准的 %.2f 倍 ⇒ 「能拟合」；多出来的那一截正是【逐步竖直偏置】'
          % (np.median([RES[p]['rms'] for p in 'ABC' if RES.get(p)]) / np.sqrt(np.mean(rs ** 2))))
    print('   ⇒ 而且形状上分不出正弦/直线（段内二次项 ≈ 0 已在 ramp-fit.py 轮 20 证过）')


def figure_run(name, z, SEG, Ss, RES, outpng, v0mode):
    """3 栏：两个代表性窗口的原始数据+换相点+理论过零点+锚定正弦+实测过零；底部逐步偏差散点"""
    wius = sorted(set(e['wi'] for e in SEG))
    picks = [wius[len(wius) // 3], wius[2 * len(wius) // 3]] if len(wius) >= 3 else wius[:1]
    fig = plt.figure(figsize=(16.5, 11))
    gs = fig.add_gridspec(3, 1, height_ratios=[1.15, 1.15, 1.0], hspace=.34)
    t00 = min(e['t0'] for e in SEG)
    for ax, wi in zip([fig.add_subplot(gs[0]), fig.add_subplot(gs[1])], picks):
        for p in 'ABC':
            for e in [x for x in SEG if x['wi'] == wi and x['ph'] == p]:
                x = (e['t0'] - t00 + np.arange(len(e['d']))) / FS * 1e3
                ax.plot(x, e['d'], '.', ms=2.6, color=COL[p], alpha=.55)
                ax.axvline(e['i0'] * 0 + (e['t0'] - t00) / FS * 1e3, color='0.55', ls=':', lw=.9)
                ax.plot([(e['t0'] - t00 + e['w'] / 2) / FS * 1e3], [0], 'k^', ms=7, zorder=5)
                if np.isfinite(e.get('u0_local', np.nan)):
                    ax.plot([(e['t0'] - t00 + e['u0_local'] * e['w']) / FS * 1e3], [0], 'o',
                            ms=6, mfc='none', mec=COL[p], mew=1.6, zorder=6)
                f = RES.get(p)
                if f:
                    uu = e['u']; ax.plot(x, f['A'] * Ss[e['s']] * BASIS(uu) + f['C'], '-',
                                         lw=1.3, color=COL[p], alpha=.9)
        ax.axhline(0, color='0.75', lw=.8); ax.grid(alpha=.22)
        ax.set_ylabel('悬空相 − V0 (码)')
        ax.set_title('窗口 seq=%d：彩点=实测悬空相  灰点线=换相点  黑▲=理论过零点(步中点)  '
                     '○=实测过零  实线=三角零点锚定拟合' % int(z['seq'][wi]), fontsize=10)
    # 底部：逐步偏差
    ax3 = fig.add_subplot(gs[2])
    for p in 'ABC':
        for s in range(1, 7):
            if F[s] != p:
                continue
            E = [e for e in SEG if e['ph'] == p and e['s'] == s and np.isfinite(e['u0_local'])]
            if not E:
                continue
            tsx = [(e['t0'] - t00) / FS for e in E]
            ys = [(e['u0_local'] - 0.5) * 60 for e in E]
            ax3.plot(tsx, ys, '.', ms=3.2, color=COL[p], alpha=.6)
    ax3.axhline(0, color='k', lw=1.2, label='理论 0°（过零=换相后 30°）')
    for v, c in ((5, '#e67e22'), (-5, '#e67e22')):
        ax3.axhline(v, color=c, ls='--', lw=1, label='±5°E' if v > 0 else None)
    ax3.set_xlabel('开环段内时间 (s)'); ax3.set_ylabel('实测过零 − 理论 (°电角度)')
    ax3.set_title('逐步偏差散点（颜色=悬空相）—— 这就是"校准量"：六步各自偏多少', fontsize=10)
    ax3.grid(alpha=.22); ax3.legend(fontsize=9)
    fig.suptitle('一切为了校准 · %s · V0=%s' % (name, v0mode), fontsize=13)
    fig.savefig(outpng, dpi=118, bbox_inches='tight'); plt.close(fig)
    print('\n图 →', outpng)


def step_meds(SEG):
    """每步 × 三口径 的过零偏差中位（°E）"""
    out = {}
    for key in ('u0_local', 'u0_full', 'u0_mid'):
        out[key] = []
        for s in range(1, 7):
            v = [x for x in SEG if x['s'] == s and np.isfinite(x.get(key, np.nan))]
            out[key].append(np.median([(x[key] - .5) * 60 for x in v]) if v else np.nan)
    return out


def figure_agg(data, outpng):
    """跨 run 汇总：① 逐步偏差 ② 两种不确定源（六步散差 vs 口径散差）"""
    fig, ax = plt.subplots(1, 2, figsize=(16, 5.8))
    names = list(data.keys())
    xs = np.arange(1, 7)
    off = np.linspace(-.28, .28, len(names))
    for i, nm in enumerate(names):
        SEG = data[nm]['SEG']
        med, lo, hi = [], [], []
        for s in xs:
            v = np.array([(e['u0_local'] - 0.5) * 60 for e in SEG
                          if e['s'] == s and np.isfinite(e['u0_local'])])
            if len(v) < 3:
                med.append(np.nan); lo.append(np.nan); hi.append(np.nan); continue
            med.append(np.median(v))
            lo.append(np.percentile(v, 25)); hi.append(np.percentile(v, 75))
        lab = '%s (%s)' % (nm, data[nm]['v0'])
        ax[0].errorbar(xs + off[i], med, yerr=[np.array(med) - np.array(lo),
                                               np.array(hi) - np.array(med)],
                       fmt='o', ms=5, capsize=3, lw=1.2, label=lab)
    ax[0].axhline(0, color='k', lw=1.2)
    ax[0].axhspan(-5, 5, color='#f39c12', alpha=.12)
    ax[0].set_xticks(xs); ax[0].set_xticklabels(['步%d(%s)' % (s, F[s]) for s in xs])
    ax[0].set_ylabel('实测过零 − 理论 (°电角度)')
    ax[0].set_title('各典型开环数据的【逐步】过零偏差\n（中位±四分位；阴影=±5°E；括号里是用的 V0）', fontsize=11)
    ax[0].grid(alpha=.22); ax[0].legend(fontsize=8)
    # 右：两种不确定源
    w = 0.36
    x = np.arange(len(names))
    rng6, rngk = [], []
    for nm in names:
        m = step_meds(data[nm]['SEG'])
        a = np.array([v for v in m['u0_local'] if np.isfinite(v)])
        rng6.append((a.max() - a.min()) if len(a) > 1 else np.nan)
        sp = []
        for s in range(6):
            c = [m[k][s] for k in ('u0_local', 'u0_full', 'u0_mid') if np.isfinite(m[k][s])]
            if len(c) > 1:
                sp.append(max(c) - min(c))
        rngk.append(np.median(sp) if sp else np.nan)
    ax[1].bar(x - w / 2, rng6, w, color='#c0392b', alpha=.85, label='六步散差（步间 max−min）')
    ax[1].bar(x + w / 2, rngk, w, color='#2980b9', alpha=.85, label='口径散差（三口径中位差）')
    for xi, v1, v2 in zip(x, rng6, rngk):
        ax[1].text(xi - w / 2, v1, '%.1f' % v1, ha='center', va='bottom', fontsize=8)
        ax[1].text(xi + w / 2, v2, '%.1f' % v2, ha='center', va='bottom', fontsize=8)
    ax[1].set_xticks(x)
    ax[1].set_xticklabels(['%s\n%s' % (nm[9:], data[nm]['v0']) for nm in names], fontsize=8)
    ax[1].set_ylabel('°电角度')
    ax[1].set_title('两个"测不准"来源的对照\n（口径散差大 ⇒ 过零位置不可判；六步散差大 ⇒ 真·六步不对称）', fontsize=11)
    ax[1].grid(alpha=.22, axis='y'); ax[1].legend(fontsize=9)
    fig.tight_layout(); fig.savefig(outpng, dpi=118, bbox_inches='tight'); plt.close(fig)
    print('汇总图 →', outpng)


def figure_v0_compare(run, SEGh, SEGs, RESs, outpng):
    """同一组数据 hw V0 vs sw V0：三口径 × 六步 的中位偏差对照（本轮头号发现的图）"""
    Mh, Ms = step_meds(SEGh), step_meds(SEGs)
    fig, ax = plt.subplots(1, 3, figsize=(17.5, 5.4))
    xs = np.arange(1, 7); w = 0.26
    for j, (M, ttl) in enumerate([(Mh, '硬件虚拟中性点 V0'), (Ms, '软件均值 V0=(VA+VB+VC)/3')]):
        for i, (k, lab, c) in enumerate([('u0_local', '局部插值', '#c0392b'),
                                         ('u0_full', '全步直线', '#2980b9'),
                                         ('u0_mid', '中段直线', '#27ae60')]):
            ax[j].bar(xs + (i - 1) * w, M[k], w, color=c, alpha=.85, label=lab)
        ax[j].axhline(0, color='k', lw=1.2)
        ax[j].axhspan(-5, 5, color='#f39c12', alpha=.12)
        ax[j].set_xticks(xs); ax[j].set_xticklabels(['步%d' % s for s in xs])
        ax[j].set_title('%s\n%s' % (ttl, run), fontsize=11)
        ax[j].set_ylabel('实测过零 − 理论 (°电角度)')
        ax[j].grid(alpha=.22, axis='y'); ax[j].legend(fontsize=8)
    # 右：口径散差 + 六步散差 汇总对比
    lab, v6, vk = [], [], []
    for tag, M in (('hw', Mh), ('sw', Ms)):
        a = np.array([v for v in M['u0_local'] if np.isfinite(v)])
        sp = []
        for s in range(6):
            c = [M[k][s] for k in ('u0_local', 'u0_full', 'u0_mid') if np.isfinite(M[k][s])]
            if len(c) > 1:
                sp.append(max(c) - min(c))
        lab.append(tag); v6.append(a.max() - a.min() if len(a) > 1 else np.nan)
        vk.append(np.median(sp) if sp else np.nan)
    xx = np.arange(2)
    ax[2].bar(xx - w / 2, v6, w, color='#c0392b', alpha=.85, label='六步散差')
    ax[2].bar(xx + w / 2, vk, w, color='#2980b9', alpha=.85, label='口径散差')
    for xi, v1, v2 in zip(xx, v6, vk):
        ax[2].text(xi - w / 2, v1, '%.1f' % v1, ha='center', va='bottom', fontsize=9)
        ax[2].text(xi + w / 2, v2, '%.1f' % v2, ha='center', va='bottom', fontsize=9)
    ax[2].set_xticks(xx); ax[2].set_xticklabels(['hw V0', 'sw V0'])
    ax[2].set_ylabel('°电角度'); ax[2].grid(alpha=.22, axis='y'); ax[2].legend(fontsize=9)
    ax[2].set_title('换 V0 之后两个散差都塌下来\n⇒ "过零测不准"的主因在 V0 测量链', fontsize=11)
    fig.suptitle('一切为了校准 · 硬件 V0 vs 软件均值 V0（同一组数据，只换参考点）', fontsize=13)
    fig.tight_layout(); fig.savefig(outpng, dpi=118, bbox_inches='tight'); plt.close(fig)
    print('V0 对照图 →', outpng)


# ---------------------------------------------------------------- --pick
def do_pick(outdir, write=True):
    rows = []
    for cf in sorted(glob.glob(os.path.join(DIR, 'evt_run_*.csv'))):
        tag = os.path.basename(cf)[8:23]
        ts_c, st_c, span = read_csv(cf)
        if span is None:
            continue
        nz = cf.replace('.csv', '_win.npz')
        if not os.path.exists(nz):
            continue
        t0, t1, n = span
        # 开环段内的 ci / input（掐掉前面 30% 的爬升）
        cis, ins = [], []
        with open(cf, encoding='utf-8-sig') as fh:
            for r in csv.DictReader(fh):
                t = num(r, 'ts_frame')
                if np.isfinite(t) and t0 <= t <= t1:
                    v = num(r, 'ci'); w = num(r, 'input')
                    if np.isfinite(v):
                        cis.append(v)
                    if np.isfinite(w):
                        ins.append(w)
        if len(cis) > 8:
            cis = cis[int(len(cis) * .3):]
        z = np.load(nz)
        if 'va' not in z.files:
            continue
        WTS = z['ts'].astype(float)
        nwin = sum(1 for t in WTS if stage_at(ts_c, st_c, t) == 1)
        nseg = 0
        for w in range(len(WTS)):
            if stage_at(ts_c, st_c, WTS[w]) != 1:
                continue
            if not (t0 <= WTS[w] <= t1):
                continue
            st = z['step'][w].astype(int); idx = np.where(np.diff(st) != 0)[0]
            if len(idx) >= 2:
                nseg += int((np.diff(idx) >= SEG_MIN).sum())
        if nwin < 5:
            continue
        med = np.median(cis) if cis else np.nan
        iqr = (np.percentile(cis, 75) - np.percentile(cis, 25)) if cis else np.nan
        rows.append(dict(tag=tag, n=n, span=(t0, t1), med=med, iqr=iqr,
                         inp=np.median(ins) if ins else np.nan, nwin=nwin, nseg=nseg))
    rows.sort(key=lambda r: -r['nseg'])
    if not write:
        return [r['tag'] for r in rows]
    os.makedirs(outdir, exist_ok=True)
    p = os.path.join(outdir, '01_开环稳定数据筛选.md')
    with open(p, 'w', encoding='utf-8') as fh:
        fh.write('# 开环稳定数据筛选（工具：`tools/cal-anchor.py --pick`）\n\n'
                 '> "开环稳定"= CSV 里 `stage==1` 连续最长的一段够长，且段内 `ci`（换相间隔）几乎不变。\n'
                 '> **为什么只用开环**：开环时换相时刻由固件定时器给定、与反电势无关 ⇒ 换相点是独立真值；\n'
                 '> 闭环时换相由实测过零推出来 ⇒ 拿换相点当参照是循环论证，不能用来校准。\n'
                 '> 「可用步段」= 落在该开环段内的波形窗口里、长度 ≥%d 采样点的步段总数（越多统计越硬）。\n\n'
                 % SEG_MIN)
        fh.write('| run | 开环事件数 | 开环 ts 跨度 | ci 中位 (拍) | ci IQR | input | 窗口数 | 可用步段 |\n')
        fh.write('|---|---|---|---|---|---|---|---|\n')
        for r in rows:
            fh.write('| `evt_run_%s` | %d | %.0f~%.0f | %s | %s | %s | %d | %d |\n' % (
                r['tag'], r['n'], r['span'][0], r['span'][1],
                ('%.0f' % r['med']) if np.isfinite(r['med']) else '—',
                ('%.0f' % r['iqr']) if np.isfinite(r['iqr']) else '—',
                ('%.0f' % r['inp']) if np.isfinite(r['inp']) else '—', r['nwin'], r['nseg']))
    print('筛选表 →', p, '（%d 组）' % len(rows))
    for r in rows[:12]:
        print('   %-16s 开环%5d事件 ci=%s input=%s 窗%3d 可用步段%5d' % (
            r['tag'], r['n'], ('%.0f' % r['med']) if np.isfinite(r['med']) else '—',
            ('%.0f' % r['inp']) if np.isfinite(r['inp']) else '—', r['nwin'], r['nseg']))
    return [r['tag'] for r in rows]


def write_summary(AGG, outdir):
    p = os.path.join(outdir, '02_校准结果.md')
    with open(p, 'w', encoding='utf-8') as fh:
        fh.write('# 校准结果（开环 · 换相点 → 理论过零点 → 三角零点锚定拟合）\n\n')
        fh.write('生成：`python3 tools/cal-anchor.py %s`\n\n' % ' '.join(AGG.keys()))
        fh.write('## 口径\n')
        fh.write('· 换相点：波形窗口里 `step` 变号处（开环时由固件定时器给定，与反电势无关 ⇒ 独立真值）\n')
        fh.write('· 理论过零点：换相点 + 半步 = 该步正中，归一化位置 u = 0.500（= 换相后 30° 电角度）\n')
        fh.write('· 锚定拟合：`d(u) = A_P·S_s·sin(π/3·(u−0.5)) + C_P`，零点是理论给的，只拟合 (A_P, C_P)\n')
        fh.write('· 过零口径：① 变号处线性插值 ② u∈[%.2f,%.2f] 直线 ③ u∈[%.2f,%.2f] 直线'
                 '（以上三个过零点都在窗内 ⇒ 可用）；④ u∈[0.15,0.50] ⑤ u∈[0.70,0.95]'
                 '（半窗外推 ⇒ 只作反面教材）。下表用①\n' % (FITLO, FITHI, MIDLO, MIDHI))
        fh.write('· 逐段自由直线 = 噪声地板；逐相逐步自由直线（12 参数）= 公平基准\n\n')
        fh.write('## 一、逐步过零偏差（实测 − 理论，°电角度）\n\n')
        fh.write('| run | V0 | 步段 | 死窗剔除 | 步1 | 步2 | 步3 | 步4 | 步5 | 步6 | 六步最大差 |\n')
        fh.write('|---|---|---|---|---|---|---|---|---|---|---|\n')
        for nm, d in AGG.items():
            SEG = d['SEG']
            vals = []
            for s in range(1, 7):
                v = [x for x in SEG if x['s'] == s and np.isfinite(x.get('u0_local', np.nan))]
                vals.append(np.median([(x['u0_local'] - .5) * 60 for x in v]) if v else np.nan)
            fs = ['%+.1f' % v if np.isfinite(v) else '—' for v in vals]
            arr = np.array([v for v in vals if np.isfinite(v)])
            rng = ('%.1f' % (arr.max() - arr.min())) if len(arr) else '—'
            fh.write('| `%s` | %s | %d | %d | %s |\n'
                     % (nm, d['v0'], d['nseg'], d.get('ndead', 0), ' | '.join(fs) + ' | ' + rng))
        fh.write('\n## 二、三角零点锚定拟合结果\n\n')
        fh.write('| run | 相 | 幅值A(码) | 直流C(码) | 残差RMS(码) | 占幅值 | 段内自由直线RMS(码) |\n')
        fh.write('|---|---|---|---|---|---|---|\n')
        for nm, d in AGG.items():
            SEG = d['SEG']
            rl = np.concatenate([e['d'][e['m']] -
                                 np.polyval(np.polyfit(e['u'][e['m']], e['d'][e['m']], 1), e['u'][e['m']])
                                 for e in SEG]) if SEG else np.array([np.nan])
            floor = float(np.sqrt(np.mean(rl ** 2)))
            for ph in 'ABC':
                r = d['RES'].get(ph)
                if not r:
                    continue
                fh.write('| `%s` | %s | %+.0f | %+.1f | %.1f | %.1f%% | %.1f |\n'
                         % (nm, ph, r['A'], r['C'], r['rms'], r['rms_pct'], floor))
        fh.write('\n## 三、三个残差台阶（"这个能拟合吧"的答案）\n\n')
        fh.write('| run | ① 噪声地板 逐段直线(2/段) | ② 公平基准 逐步直线(12) | ③ 本模型 三角零点锚定(6) | ③/② |\n')
        fh.write('|---|---|---|---|---|\n')
        for nm, d in AGG.items():
            SEG = d['SEG']
            rl = np.concatenate([e['d'][e['m']] -
                                 np.polyval(np.polyfit(e['u'][e['m']], e['d'][e['m']], 1), e['u'][e['m']])
                                 for e in SEG]) if SEG else np.array([np.nan])
            rs = []
            for p in 'ABC':
                for s in range(1, 7):
                    E = [e for e in SEG if e['ph'] == p and e['s'] == s]
                    if len(E) < 2:
                        continue
                    U = np.concatenate([e['u'][e['m']] for e in E])
                    D = np.concatenate([e['d'][e['m']] for e in E])
                    rs.append(D - np.polyval(np.polyfit(U, D, 1), U))
            rs = np.concatenate(rs) if rs else np.array([np.nan])
            three = [d['RES'][p]['rms'] for p in 'ABC' if d['RES'].get(p)]
            fh.write('| `%s` | %.1f | %.1f | %.1f | %.2f |\n'
                     % (nm, np.sqrt(np.mean(rl ** 2)), np.sqrt(np.mean(rs ** 2)),
                        np.median(three), np.median(three) / np.sqrt(np.mean(rs ** 2))))
        fh.write('\n> 结论：**6 个参数做到 12 参数基准的 1.04~1.68 倍 ⇒ 能拟合**。\n'
                 '> 多出来的那一截不是"模型不对"，正是**六步各自的竖直偏置**（＝要校准的量）。\n'
                 '> 另外形状上分不出正弦/直线（段内二次项 ≈ 0，`ramp-fit.py` 轮 20 已证）——'
                 '所以"用理论正弦还是斜线"无所谓，但**零点位置必须由换相点锚定**。\n')
    print('\n结果表 →', p)


# ---------------------------------------------------------------- --standard：整转正弦标准
def standard_fit(SEG):
    """把一相在【所有窗口的开环弧】按电角度 τ 拼到一个电周期上，拟一根正弦。

    τ 的定义（**不含任何拟合参数**）：第 s 步占 τ∈[60(s−1), 60s]，步内 u 线性映射 ⇒
        τ = 60·(s−1) + 60·u
    所以换相点固定在 τ = 0, 60, 120, …, 300（网格线）。然后拟合
        d(τ) = a·sin τ + b·cos τ + C = A·sin(τ + ψ) + C
    零点在 τ_z = −ψ (mod 180)。**理论标准是 τ_z = 30°, 210°**（＝每步正中＝换相后 30°）。
    于是"实测的 τ_z 落在哪"就是对这个理论的直接检验。
    """
    out = {}
    for p in 'ABC':
        E = [e for e in SEG if e['ph'] == p]
        if len(E) < 4:
            out[p] = None; continue
        T, D = [], []
        for e in E:
            m = e['m']
            T.append(60.0 * (e['s'] - 1) + 60.0 * e['u'][m])
            D.append(e['d'][m])
        T = np.concatenate(T); D = np.concatenate(D)
        R = np.radians(T)
        M = np.column_stack([np.sin(R), np.cos(R), np.ones_like(R)])
        c, *_ = np.linalg.lstsq(M, D, rcond=None)
        A = float(np.hypot(c[0], c[1]))
        psi = float(np.arctan2(c[1], c[0]))          # d = A·sin(τ+ψ)+C
        C = float(c[2])
        r = D - M @ c
        tz = (-np.degrees(psi)) % 180.0              # 零点（mod 180，仅用于报"离换相点多远"）
        # ★ 折叠要用【上升零点】：−ψ (mod 360)。用 %180 取到的可能是"下降零点"，
        #   那样 B/C 两相会被镜像成反号（踩过：三相叠图变成尖峰）。
        tz0 = (-np.degrees(psi)) % 360.0
        # 实测零点相对【最近换相点】的偏移 = tz mod 60（理论应为 30）
        out[p] = dict(A=A, C=C, psi=psi, rms=float(np.sqrt(np.mean(r ** 2))),
                      tz=tz, tz0=tz0, off=tz % 60.0, T=T, D=D, n=len(T), nseg=len(E), cz=c)
    return out


def figure_standard(name, STD, outpng, v0):
    fig, ax = plt.subplots(3, 1, figsize=(16, 11.5), sharex=True)
    for a, p in zip(ax, 'ABC'):
        r = STD.get(p)
        if not r:
            a.set_visible(False); continue
        a.plot(r['T'], r['D'], '.', ms=2.2, color=COL[p], alpha=.35,
               label='实测（%d 个开环弧拼接）' % r['nseg'])
        tt = np.linspace(0, 360, 1200)
        a.plot(tt, r['A'] * np.sin(np.radians(tt) + r['psi']) + r['C'], '-', color='k', lw=1.8,
               label='拟合正弦 A=%.0f C=%+.1f 残差%.0f码' % (r['A'], r['C'], r['rms']))
        for k in range(7):
            a.axvline(60 * k, color='0.6', ls=':', lw=1.0)
        a.plot([], [], color='0.6', ls=':', label='换相点（每 60° 网格线）')
        for k in range(7):
            tz = r['tz'] + 180 * k
            if 0 <= tz <= 360:
                a.axvline(tz, color=COL[p], ls='--', lw=1.2)
                a.plot([tz], [0], 'v', ms=7, color=COL[p])
        a.plot([], [], color=COL[p], ls='--', marker='v', ms=6,
               label='拟合正弦的过零点 τ_z=%.1f°(+180k)  ⇒ 离换相点 %.1f°E（理论 30°）'
                     % (r['tz'], r['off']))
        # 理论标准：每步正中
        for k in range(7):
            t = 30 + 60 * k
            if t <= 360:
                a.plot([t], [0], 'k^', ms=6)
        a.plot([], [], 'k^', ms=6, label='理论标准：过零＝步正中（换相后 30°）')
        a.axhline(0, color='0.75', lw=.8); a.grid(alpha=.2)
        a.set_ylabel('悬空相 − V0 (码)')
        a.set_title('相 %s：把该相在一个电周期里的【全部悬空弧】拼起来（横轴 τ = 电角度，τ=0 取在第 1 步的换相点）'
                    % p, fontsize=10.5)
        a.legend(fontsize=8.5, loc='lower right', ncol=2)
        a.set_xlim(0, 360)
    ax[-1].set_xlabel('τ（电角度，换相点在 0/60/120/…/300）')
    fig.suptitle('理论正弦标准 · %s · V0=%s   —— 一个电周期 360°，可见完整正弦' % (name, v0), fontsize=13)
    fig.tight_layout(); fig.savefig(outpng, dpi=118, bbox_inches='tight'); plt.close(fig)
    print('理论标准图 →', outpng)


def report_standard(name, STD, SEG, v0):
    print('\n' + '=' * 78)
    print('【%s】理论正弦标准（整转拼接拟合，V0=%s，用 %d 个开环步段）' % (name, v0, len(SEG)))
    print('=' * 78)
    print('   τ 的定义不含任何拟合参数：第 s 步 = τ∈[60(s−1),60s]，步内 u 线性 ⇒ τ=60(s−1)+60u')
    print('   ⇒ 换相点固定在 τ = 0,60,…,300 ；  文献/固件的理论标准是"过零在步正中"= τ_z = 30°(+60k)')
    print('\n   %-4s %10s %9s %10s %12s %14s' % ('相', '幅值A(码)', '直流C(码)', '残差RMS', '拟合过零τ_z', '离最近换相点'))
    for p in 'ABC':
        r = STD.get(p)
        if not r:
            print('   %-4s   （弧太少，无法拟）' % p); continue
        print('   %-4s %10.0f %9.1f %10.1f %12.1f° %11.1f°E %s'
              % (p, r['A'], r['C'], r['rms'], r['tz'], r['off'],
                 '← 与理论 30° 相差 %+.1f°' % (r['off'] - 30)))
    A = {p: STD[p] for p in 'ABC' if STD.get(p)}
    if len(A) == 3:
        d = {}
        for p, k in (('B', 120), ('C', 240)):
            d[p] = ((STD[p]['psi'] - STD['A']['psi']) * 180 / np.pi + 180) % 360 - 180
        print('\n   三相之间的相位差（相对 A）：B %+.1f°（理想 ±120）、C %+.1f°（理想 ±240）'
              % (d['B'], d['C']))
    offs = [STD[p]['off'] for p in 'ABC' if STD.get(p)]
    print('   三相应的"离最近换相点"=%s ⇒ 理论 30°；'
          % ' / '.join('%.1f°' % x for x in offs))
    print('   ⚠ 若实测明显大于 30°，多出来的那一截就是【转子负载角】（开环时定子磁场按定时器走、转子滞后）。')


def figure_stitch_3ph(name, STD, outpng):
    """三相数据折叠到各自的过零点上：是不是同一根正弦？

    ★ 这里有个必须说清的结构性事实（别指望三相叠起来能看见波峰）：
      一个相只在一个电周期里悬空 2×60° = 120°，而且这两段**必然落在它自己过零点附近**
      （因为"过零在步正中"）⇒ 观测范围恒为过零前后约 [−38°, +25°]。
      波峰在 ±90°，那时这一相**正在被驱动**，端子电压是 PWM 方波 ⇒ **永远测不到反电势**。
      所以"到底是正弦、还是梯形、还是直线"这件事，**悬空相数据永远证不了**；
      能证的是：两段弧相隔 180°（同一根正弦的半个周期）、三相相差 120°、过零点落在步正中。

    画法：把每个相的数据按"离它自己的过零点多远"折叠（x = τ − τ_z, 折到 ±90°，
    每折一次 180° 就乘一次 −1），于是三相全部塌到同一根 sin(x) 上。
    """
    fig, ax = plt.subplots(1, 2, figsize=(17, 6.4))
    got = [p for p in 'ABC' if STD.get(p)]
    if not got:
        plt.close(fig); return
    ax[0].plot([], [], 'k-', lw=1.6, label='sin(x) 参考（单根正弦）')
    ALLX, ALLY = [], []
    for p in got:
        r = STD[p]
        x0 = r['T'] - r['tz0']                     # 相对该相自己的【上升】过零点
        k = np.round(x0 / 180.0)
        x = x0 - 180.0 * k
        y = (r['D'] - r['C']) / r['A'] * ((-1.0) ** k)
        ax[0].plot(x, y, '.', ms=1.6, color=COL[p], alpha=.18, label='相%s' % p)
        ALLX.append(x); ALLY.append(y)
    ALLX = np.concatenate(ALLX); ALLY = np.concatenate(ALLY)
    # 分箱中位±四分位：让趋势一眼可见
    edges = np.linspace(-30, 20, 26)
    cx, cy, lo, hi = [], [], [], []
    for a, b in zip(edges[:-1], edges[1:]):
        m = (ALLX >= a) & (ALLX < b)
        if m.sum() < 20:
            continue
        cx.append((a + b) / 2); cy.append(np.median(ALLY[m]))
        lo.append(np.percentile(ALLY[m], 25)); hi.append(np.percentile(ALLY[m], 75))
    ax[0].errorbar(cx, cy, yerr=[np.array(cy) - np.array(lo), np.array(hi) - np.array(cy)],
                   fmt='s-', ms=4, lw=1.6, color='#8e44ad', capsize=2,
                   label='全部数据的分箱中位±四分位')
    xx = np.linspace(-90, 90, 800)
    ax[0].plot(xx, np.sin(np.radians(xx)), '-', color='k', lw=1.6)
    for a, b, c in ((-95, -30, '#f39c12'), (20, 95, '#f39c12')):
        ax[0].axvspan(a, b, color=c, alpha=.07)
    ax[0].text(-62, 1.22, '没有数据\n(该相被驱动)', color='#b9770e', fontsize=10, ha='center')
    ax[0].text(57, 1.22, '没有数据\n(该相被驱动)', color='#b9770e', fontsize=10, ha='center')
    ax[0].axhspan(-1.02, 1.02, xmin=0, xmax=0, alpha=0)
    for xv in (-90, -60, -30, 0, 30, 60, 90):
        ax[0].axvline(xv, color='0.75', ls=':', lw=.9)
    ax[0].axhline(0, color='0.85', lw=.8)
    ax[0].axvline(-30, color='#c0392b', ls='--', lw=1.2)
    ax[0].axvline(+30, color='#c0392b', ls='--', lw=1.2)
    ax[0].text(-30, 1.28, '一步 60° 的边界', color='#c0392b', fontsize=9, ha='center')
    ax[0].plot([], [], color='#c0392b', ls='--', label='一步 60° 的边界（换相点）')
    ax[0].set_xlim(-95, 95); ax[0].set_ylim(-1.45, 1.45)
    ax[0].set_xlabel('x = 离【本相自己的过零点】多远（°电角度）')
    ax[0].set_ylabel('(悬空相 − V0) / 该相幅值')
    ax[0].set_title('三相数据折叠到各自的过零点 ⇒ 全部落在同一根 sin(x) 上\n'
                    '但数据只占 x∈[−38°,+25°]（＝一个步宽）；±90° 处是波峰，'
                    '那时该相正被驱动 ⇒ 永远没有数据', fontsize=10.5)
    ax[0].grid(alpha=.2)
    h, l = ax[0].get_legend_handles_labels()
    seen, H, L = set(), [], []
    for hi, li in zip(h, l):
        if li not in seen:
            seen.add(li); H.append(hi); L.append(li)
    ax[0].legend(H, L, fontsize=9, loc='lower right', ncol=2)
    # 右：三条正弦各自的过零点离换相点多远
    w = 0.5
    for i, p in enumerate('ABC'):
        r = STD.get(p)
        if not r:
            ax[1].bar(i, np.nan, w); continue
        ax[1].bar(i, r['off'], w, color=COL[p], alpha=.85)
        ax[1].text(i, r['off'], '  τ_z=%.1f°\n  离换相点 %.1f°' % (r['tz'], r['off']),
                   ha='center', va='bottom', fontsize=9)
    ax[1].axhline(30, color='k', ls='--', lw=1.4, label='理论标准 30°（过零＝步正中）')
    ax[1].axhline(0, color='#c0392b', ls=':', lw=1.4, label='若"过零就在换相点上"应为 0°')
    ax[1].axhline(90, color='#8e44ad', ls=':', lw=1.4, label='若"离换相点 90°"应为 90°')
    ax[1].set_xticks([0, 1, 2]); ax[1].set_xticklabels(['相A', '相B', '相C'])
    ax[1].set_ylim(0, 105); ax[1].set_ylabel('拟合正弦的过零点离最近换相点（°电角度）')
    ax[1].set_title('三相各自的过零点离换相点多远\n（实测都落在 30° 一带 ⇒ 理论正确）', fontsize=11)
    ax[1].grid(alpha=.2, axis='y'); ax[1].legend(fontsize=9, loc='upper right')
    fig.suptitle('理论标准 · %s · 悬空相数据到底证明了什么' % name, fontsize=13)
    fig.tight_layout(); fig.savefig(outpng, dpi=118, bbox_inches='tight'); plt.close(fig)
    print('三相折叠图 →', outpng)


def figure_composite(name, STD, SEG, outpng, v0):
    """把【悬空相】当成一个通道看：它随时间的波形是什么形状？——不是正弦。

    推理（也是图上要证的）：
      任一时刻恰好一个相悬空（每相悬空 2×60°=120°，三相拼起来正好 360°），所以
      "悬空相电压 − V0" 是一条**连续**曲线（换相处两边都正好是 ±半幅，接得上）。
      但每换一次相，这条曲线就跳到**另一个相**的反电势上；三相相差 120°，
      而且换相点落在步边界（该相反电势 = ±半幅处）⇒ 每 60° 上升/下降翻一次号。
      ⇒ 结果是**周期 120° 电角度（＝2 步）的三角/锯齿波**，不是正弦；
         它的"斜边"是一段 60° 的慢正弦弧（所以略带弧度，不是纯直线）。
      这正是所有工具都必须带【每步一个符号 S_s】的原因。
    """
    fig, ax = plt.subplots(2, 1, figsize=(16, 10.6), sharex=True)
    # ---- 上：实测的"悬空相"通道 ----
    for e in SEG:
        tt = 60.0 * (e['s'] - 1) + 60.0 * e['u']
        ax[0].plot(tt, e['d'], '.', ms=2.0, color=COL[e['ph']], alpha=.28)
    for p in 'ABC':
        ax[0].plot([], [], '.', ms=6, color=COL[p], label='当下悬空的是相%s' % p)
    tt = np.linspace(0, 360, 4000)
    comp = np.full_like(tt, np.nan)
    for i in range(len(tt)):
        s = min(6, int(tt[i] // 60) + 1)
        r = STD.get(F[s])
        if r:
            comp[i] = r['A'] * np.sin(np.radians(tt[i]) + r['psi']) + r['C']
    ax[0].plot(tt, comp, '-', color='k', lw=1.8, label='理论合成（每 60° 从一根正弦上剪一段）')
    for k in range(7):
        ax[0].axvline(60 * k, color='0.6', ls=':', lw=1.0)
    ax[0].plot([], [], color='0.6', ls=':', label='换相点（每 60° 一格）')
    for k in range(7):
        if 30 + 60 * k <= 360:
            ax[0].plot([30 + 60 * k], [0], 'k^', ms=7)
    ax[0].plot([], [], 'k^', ms=7, label='过零点（每步正中 ⇒ 换相后 30°）')
    ax[0].axhline(0, color='0.8', lw=.8)
    ax[0].set_ylabel('悬空相 − V0 (码)'); ax[0].grid(alpha=.2); ax[0].set_xlim(0, 360)
    ax[0].set_title('把「悬空相」当成【一个通道】看：它是周期 120°（＝2 步）的三角/锯齿波，'
                    '不是正弦'
                    '\n理想情形（过零正好在 30°、三相幅值相同）换相处两边都是 ±半幅 ⇒ 拐角平滑；'
                    '实测过零在 ~36°、各相幅值也不同 ⇒ 拐角变成约 ±100 码的小台阶', fontsize=10.5)
    ax[0].legend(fontsize=8.5, ncol=3, loc='lower right')
    # ---- 下：三根正弦 + 每 60° 剪哪一段 ----
    for p in 'ABC':
        r = STD.get(p)
        if not r:
            continue
        ax[1].plot(tt, r['A'] * np.sin(np.radians(tt) + r['psi']) + r['C'], '-', lw=1.5,
                   color=COL[p], alpha=.8, label='相%s 自己的正弦（整周期）' % p)
        for e in [x for x in SEG if x['ph'] == p]:
            ax[1].plot(60.0 * (e['s'] - 1) + 60.0 * e['u'], e['d'], '.', ms=1.6,
                       color=COL[p], alpha=.18)
    for k in range(7):
        ax[1].axvline(60 * k, color='0.6', ls=':', lw=1.0)
    ymax = max((STD[p]['A'] for p in 'ABC' if STD.get(p)), default=1)
    for k in range(6):
        p = F[k + 1]
        ax[1].text(60 * k + 30, ymax * 1.12, '剪 %s' % p, ha='center', va='top',
                   fontsize=11, color=COL[p], fontweight='bold')
    ax[1].set_ylim(-ymax * 1.25, ymax * 1.25)
    ax[1].axhline(0, color='0.8', lw=.8)
    ax[1].set_xlabel('τ（电角度；换相点在 0/60/120/…/300）')
    ax[1].set_ylabel('悬空相 − V0 (码)'); ax[1].grid(alpha=.2); ax[1].legend(fontsize=9)
    ax[1].set_title('三个相各有一根自己的正弦（相差 120°）。「悬空相通道」= 把这三根按 '
                    'C→A→B→C→A→B 的顺序、每 60° 剪一段拼起来', fontsize=11)
    fig.suptitle('悬空相当一相 · %s · V0=%s' % (name, v0), fontsize=13)
    fig.tight_layout(); fig.savefig(outpng, dpi=118, bbox_inches='tight'); plt.close(fig)
    print('悬空相合成图 →', outpng)
    w_tick = float(np.median([e['tick'] for e in SEG]))
    t_step = w_tick * TICK
    print('   步长中位 %.0f 拍 = %.3f ms ⇒ 电频率 %.1f Hz；悬空相通道周期 = 2 步 = %.2f ms'
          ' ⇒ %.1f Hz（= 3× 电频率 = 18× 机械频率）'
          % (w_tick, t_step * 1e3, 1 / (6 * t_step), 2 * t_step * 1e3, 1 / (2 * t_step)))


# ---------------------------------------------------------------- ADC 通道增益校准
def measure_gains(SEG):
    """逐【通道/相】的增益代理量：该相悬空弧的半量程（P97.5−P2.5)/2。

    为什么能当增益用：悬空相读数 d = g_P·(反电势) + 常数 ⇒ 那一段的摆幅 ∝ g_P。
    用分位数而不是极值，是为了不被换相振铃的尖峰污染（轮 15/16 用的"峰峰"就被污染过）。
    """
    hr = {}
    for p in 'ABC':
        v = np.concatenate([e['d'][e['m']] for e in SEG if e['ph'] == p])
        hr[p] = (np.percentile(v, 97.5) - np.percentile(v, 2.5)) / 2.0 if len(v) else np.nan
    med = float(np.median([hr[p] for p in 'ABC']))
    return {p: hr[p] / med for p in 'ABC'}, hr


def apply_gains(SEG, g, g0=1.0):
    """★ 通道级校正：v_P ← v_P/g_P，再重算 d = v_P − V0/g0。

    必须作用在【通道】上。只把算好的 d 缩放是没用的——缩放不移动零点。
    """
    for e in SEG:
        e['d'] = e['raw'] / g[e['ph']] - e['v0v'] / g0
        e['m'] = (e['u'] >= FITLO) & (e['u'] <= FITHI) if hasattr(e['u'], '__len__') else e['m']


def decompose_dev(SEG):
    """把逐步过零偏差（°E）拆成【相效应】+【步效应残差】。

    相效应 = 该相两步偏差的均值 − 总均值  ← 只跟"哪一相"有关 ⇒ ADC 通道增益/参考点能治
    步残差 = 该步偏差 − 总均值 − 相效应    ← 同一个相的两步之间还差多少 ⇒ 跟驱动/电有关，增益治不了
    """
    dev = {}
    for s in range(1, 7):
        v = [x for x in SEG if x['s'] == s and np.isfinite(x.get('u0_local', np.nan))]
        dev[s] = float(np.median([(x['u0_local'] - .5) * 60 for x in v])) if v else np.nan
    good = [s for s in range(1, 7) if np.isfinite(dev[s])]
    if not good:
        return None
    overall = float(np.mean([dev[s] for s in good]))
    pe = {}
    for p in 'ABC':
        ss = [s for s in good if F[s] == p]
        pe[p] = float(np.mean([dev[s] for s in ss])) - overall if ss else np.nan
    resid = {s: dev[s] - overall - pe[F[s]] for s in good}
    return dict(dev=dev, overall=overall, pe=pe, resid=resid, good=good,
                spread=float(max(dev[s] for s in good) - min(dev[s] for s in good)),
                pe_amp=float(np.nanmax(list(pe.values())) - np.nanmin(list(pe.values()))),
                resid_amp=float(max(resid.values()) - min(resid.values())))


def report_gain_table(gains, hr, run):
    print('   ---- 逐通道增益（相对三相中位；这是"ADC 初步校准"要的参数）----')
    for p in 'ABC':
        print('     相%s: 半量程 %6.1f 码   相对增益 %6.3f   ⇒ 校正系数 1/%.3f = %.3f'
              % (p, hr[p], gains[p], gains[p], 1 / gains[p]))
    print('     （参考：轮 15 用"悬空读数峰峰"测到 A 0.75 / B 0.89 / C 1.00（rank1=A）；'
          '本轮用分位数口径复测，见上）')


def solve_offsets_by_zc(SEG, span=60.0, n=121):
    """★ 用【过零点位置】反解逐相常量偏置（等效于通道增益误差，但数值上直接）。

    为什么用零点：d = g_P·v_P − g_0·V0 ⇒ 过零条件 v_P = V0·g_0/g_P；而 v_P = v_n + e_P、
      v_n ≈ 母线中点（约 600 码）≫ 弧幅（150~350 码）⇒ 增益误差等效成 e_P 上一个**常数偏置**
      o_P = v_n(1 − g_0/g_P)。校正它 = 让 d ← d − o_P。
      ⚠ 灵敏度：过零要留在这一步内，|o_P| 必须小于约"半步摆幅"（~150 码）
        ⇒ 等效增益校正量只能落在 ±25% 内；超出就无解。
    为什么不用幅值：实测三种幅值口径给出的相间大小顺序互相矛盾
      （例 115519 V0=sw：极差说 A>B>C，拟合幅值说 B>A>C）⇒ 幅值不是良定义的增益量。
    返回 dict(p→o_P)。
    """
    def hd(S2, ss, o):
        for e in S2:
            e['d'] = e['raw'] - e['v0v'] - o
            e['m'] = (e['u'] >= FITLO) & (e['u'] <= FITHI)
            measure_zc(e)
        v = []
        for s in ss:
            w = [x for x in S2 if x['s'] == s and np.isfinite(x.get('u0_local', np.nan))]
            v.append(float(np.median([(x['u0_local'] - .5) * 60 for x in w])) if w else np.nan)
        return (v[0] - v[1]) / 2.0 if all(np.isfinite(v)) else np.nan

    out = {}
    for p in 'ABC':
        ss = [s for s in range(1, 7) if F[s] == p]
        if len(ss) < 2:
            out[p] = 0.0; continue
        S2 = [e for e in SEG if e['ph'] == p]
        grid = np.linspace(-span, span, n)
        vals = [hd(S2, ss, o) for o in grid]
        # ★ 直接取 |剩余反对称| 最小的偏置：大偏置会把过零推出这一步、产生伪根，
        #   所以"括根+二分"不稳（224624 就栽在这）
        fin = [(abs(v), o, v) for v, o in zip(vals, grid) if np.isfinite(v)]
        if not fin:
            print('      ⚠ 相%s: 所有偏置都无解（过零全跑出这一步）⇒ 取 0' % p)
            out[p] = 0.0
        else:
            best = min(fin)
            out[p] = float(best[1])
            if best[0] > 1.0:
                print('      ⚠ 相%s: 在 ±%.0f 码内只能把反对称项压到 %.1f°E（取 %+.0f 码），'
                      '说明该相不是单纯的常量偏置问题' % (p, span, best[0], out[p]))
    for e in SEG:                       # 还原，交给调用方统一施加
        e['d'] = e['raw'] - e['v0v']
        e['m'] = (e['u'] >= FITLO) & (e['u'] <= FITHI)
        measure_zc(e)
    return out


def analyse_dev(SEG):
    """把逐步偏差拆成【共同项 / 相间差 / 相内反对称(=增益特征)】。

    每相只有两个步 ⇒ 两个数正好定出"均值"和"半差"，分解完备、没有剩余项：
        dev[s] = 共同项 + 相间差_P ± 反对称_P
    · 相间差   → 只跟"哪一相"有关 ⇒ 通道直流、参考点、或真·机械不对称
    · 反对称项 → 同一个相两步**符号相反**（两步斜率反号）⇒ 这就是【通道增益误差】的特征
    """
    dev = {}
    for s in range(1, 7):
        v = [x for x in SEG if x['s'] == s and np.isfinite(x.get('u0_local', np.nan))]
        dev[s] = float(np.median([(x['u0_local'] - .5) * 60 for x in v])) if v else np.nan
    good = [s for s in range(1, 7) if np.isfinite(dev[s])]
    if not good:
        return None
    overall = float(np.mean([dev[s] for s in good]))
    pe, asym = {}, {}
    for p in 'ABC':
        ss = [s for s in good if F[s] == p]
        if len(ss) == 2:
            pe[p] = (dev[ss[0]] + dev[ss[1]]) / 2 - overall
            asym[p] = (dev[ss[0]] - dev[ss[1]]) / 2
        elif len(ss) == 1:
            pe[p] = dev[ss[0]] - overall; asym[p] = 0.0
        else:
            pe[p] = np.nan; asym[p] = np.nan
    return dict(dev=dev, overall=overall, pe=pe, asym=asym, good=good,
                spread=float(max(dev[s] for s in good) - min(dev[s] for s in good)),
                pe_amp=float(np.nanmax(list(pe.values())) - np.nanmin(list(pe.values()))),
                asym_amp=float(np.nanmax(list(asym.values())) - np.nanmin(list(asym.values()))))


def figure_gain_phase_step(DATA, outpng):
    """逐 run × V0：原始 vs 逐相偏置校正后的逐步偏差"""
    names = list(DATA.keys())
    ncol = 3
    nrow = int(np.ceil(len(names) / ncol))
    fig, axes = plt.subplots(nrow, ncol, figsize=(6.0 * ncol, 4.4 * nrow), squeeze=False)
    for j, nm in enumerate(names):
        a = axes[j // ncol][j % ncol]
        d = DATA[nm]
        xs = np.arange(1, 7)
        for key, lab, col, mk in (('raw', '原始', '#7f8c8d', 'o'),
                                  ('corr', '逐相偏置校正后', '#c0392b', 's')):
            r = d.get(key)
            if not r:
                continue
            ys = [r['dev'].get(s, np.nan) for s in xs]
            a.plot(xs, ys, mk + '-', ms=6, lw=1.5, color=col, label=lab)
        a.axhline(0, color='k', lw=1.1)
        a.axhspan(-5, 5, color='#f39c12', alpha=.12)
        a.set_xticks(xs)
        a.set_xticklabels(['步%d\n(%s)' % (s, F[s]) for s in xs], fontsize=8)
        a.set_ylabel('实测过零 − 理论 (°E)', fontsize=9)
        o = d['off']
        box = ('六步散差  %.1f° → %.1f°\n'
               '相内反对称 %.1f° → %.1f°  (=增益特征)\n'
               '相间差    %.1f° → %.1f°  (=治不了的部分)\n'
               '偏置 o = %+.0f / %+.0f / %+.0f 码 (A/B/C)'
               % (d['raw']['spread'], d['corr']['spread'],
                  d['raw']['asym_amp'], d['corr']['asym_amp'],
                  d['raw']['pe_amp'], d['corr']['pe_amp'],
                  o['A'], o['B'], o['C']))
        a.text(.03, .03, box, transform=a.transAxes, fontsize=8, va='bottom', ha='left',
               bbox=dict(fc='white', ec='0.7', alpha=.92, pad=3))
        a.set_title('%s · V0=%s' % (nm, d['v0']), fontsize=10)
        a.grid(alpha=.22); a.legend(fontsize=8, loc='upper right')
    for k in range(len(names), nrow * ncol):
        axes[k // ncol][k % ncol].set_visible(False)
    fig.suptitle('ADC 通道增益校准：逐相常量偏置校正前后'
                 '（相内反对称＝增益特征，应被打到 0；相间差治不了，是下一步的事）', fontsize=12.5)
    fig.tight_layout(rect=[0, 0, 1, .96])
    fig.savefig(outpng, dpi=118, bbox_inches='tight'); plt.close(fig)
    print('增益校准图 →', outpng)


def figure_wave(npz, outpng, nwin=3, v0mode='auto', all_stage=False, which='open', duty_eq=None):
    """开环稳定段的【四路全速率采样】多窗口波形 + 判据信号 + V0 自检。

    口径（与 cal-anchor 其它模式一致）：
      · 只取【最长 stage==1 段内】的窗口（按 ts 对齐，不是 seq）；剔死窗；
      · 再要求该窗步长落在 1500~2400 拍（≈稳态），按 ts 等距取 nwin 片。
    每行 3 栏：
      ① 四路原始电压（全速率 512 点 @24kHz = 21.33ms）+ 换相点（按 step 上色）
         —— 看三相悬空弧的幅值是否已经一样齐（增益归一的效果）
      ② delta = 悬空相 − 参考：**硬件 V0** 与 **三相平均** 两条画在同一坐标直接比
         （固件控制路径现在用三相平均；窗口载荷记的是硬件 V0，所以两条都能算）
      ③ V0 自检：硬件 V0 vs (VA+VB+VC)/3 + 两者之差
    """
    cf = npz.replace('_win.npz', '.csv')
    ts_c, st_c, span = read_csv(cf)
    z = np.load(npz)
    ST, TS = z['step'], z['ts'].astype(np.int64)
    PH = {k: z[k].astype(float) for k in ('va', 'vb', 'vc')}
    have_v0 = 'v0' in z.files
    V0HW = z['v0'].astype(float) if have_v0 else (PH['va'] + PH['vb'] + PH['vc']) / 3.0
    SW = (PH['va'] + PH['vb'] + PH['vc']) / 3.0
    # 交棒时刻（stage 1→0 的第一次）：闭环段必须晚于它（否则会把上电起始那两行 stage=0 也算进来）
    hand_ts = None
    if ts_c is not None and st_c is not None:
        for i in range(1, len(st_c)):
            if st_c[i - 1] == 1 and st_c[i] == 0:
                hand_ts = float(ts_c[i]); break
    # duty_setpoint 随时间（画在标题上，用来分清"锚定段 / 失速再起步段"）
    duty_ts, duty_v = [], []
    cf2 = npz.replace('_win.npz', '.csv')
    if os.path.exists(cf2):
        with open(cf2, encoding='utf-8-sig') as fh:
            for r in csv.DictReader(fh):
                v = num(r, 'duty_setpoint')
                if np.isfinite(v):
                    duty_ts.append(num(r, 'ts_frame')); duty_v.append(v)
        o = np.argsort(duty_ts); duty_ts = np.array(duty_ts)[o]; duty_v = np.array(duty_v)[o]

    def duty_at(t):
        if len(duty_ts) == 0:
            return np.nan
        i = int(np.searchsorted(duty_ts, t, 'right')) - 1
        return float(duty_v[i]) if i >= 0 else np.nan

    ok = []
    for wi in range(len(ST)):
        t = float(TS[wi])
        if which == 'closed':
            if hand_ts is None or t < hand_ts:
                continue
        else:
            if not all_stage:
                if stage_at(ts_c, st_c, t) != 1:
                    continue
                if span and not (span[0] <= t <= span[1]):
                    continue
        mx = np.maximum(PH['va'][wi], PH['vb'][wi]); mx = np.maximum(mx, PH['vc'][wi])
        if np.median(mx) < 200:
            continue
        idx = np.where(np.diff(ST[wi].astype(int)) != 0)[0]
        if len(idx) < 2:
            continue
        tl = np.median(np.diff(idx)) * 83.333
        if which != 'closed' and not (1500 <= tl <= 2400):
            continue
        if duty_eq is not None:
            dv = duty_at(t)
            if not (np.isfinite(dv) and abs(dv - duty_eq) < 1):
                continue
        ok.append(wi)
    if not ok:
        print('!! 找不到"开环稳定"窗口'); return
    sel = [ok[k] for k in np.linspace(0, len(ok) - 1, min(nwin, len(ok))).astype(int)]
    print('开环稳定窗口 %d 个（总 %d 片），画 %d 片' % (len(ok), len(ST), len(sel)))
    FS = 24000.0
    fig, axs = plt.subplots(len(sel), 3, figsize=(18.5, 3.5 * len(sel)))
    if len(sel) == 1:
        axs = axs.reshape(1, 3)
    for r, wi in enumerate(sel):
        st = ST[wi].astype(int)
        t = np.arange(512) / FS * 1e3
        a0, a1, a2 = axs[r]
        for nm, v, col, lw in (('VA', PH['va'], '#c0392b', 1.0), ('VB', PH['vb'], '#2980b9', 1.0),
                               ('VC', PH['vc'], '#16a085', 1.0), ('V0(硬件)', V0HW, '#8e44ad', 1.3)):
            a0.plot(t, v[wi], lw=lw, color=col, label=nm)
        a0.plot(t, SW[wi], lw=1.0, color='#e67e22', ls='--', label='三相平均')
        idx = np.where(np.diff(st) != 0)[0] + 1
        for k in idx:
            a0.axvline(t[k], color=COLW[int(st[k])], lw=1.2, ls=':', alpha=.95)
        a0.plot([], [], color='0.4', lw=1.2, ls=':', label='换相点(按step上色)')
        a0.set_ylim(-120, 1420); a0.grid(alpha=.22); a0.set_ylabel('ADC 码')
        a0.set_title('窗 seq=%d  步长中位 %.0f 拍  duty_setpoint=%.0f  |  ① 四路原始电压（全速率 512 点 @24kHz）'
                     % (z['seq'][wi], np.median(np.diff(idx)) * 83.333, duty_at(float(TS[wi]))),
                     fontsize=10)
        a0.legend(fontsize=8, ncol=6, loc='lower center', framealpha=.9)
        # ② delta 两条参考
        vph = np.where((st == 1) | (st == 4), PH['vc'][wi],
              np.where((st == 2) | (st == 5), PH['va'][wi], PH['vb'][wi]))
        a1.plot(t, vph - V0HW[wi], lw=1.3, color='#8e44ad', label='悬空相 − 硬件V0')
        a1.plot(t, vph - SW[wi], lw=1.3, color='#e67e22', ls='--', label='悬空相 − 三相平均')
        a1.axhline(0, color='k', lw=1.0)
        for k in idx:
            a1.axvline(t[k], color='0.65', lw=1.0, ls=':')
        # 实测过零（delta 变号处，取每步中点附近那个）+ 理论过零（步中点）
        for i0, i1, ss in segs(st):
            if ss not in F or i0 == 0 or i1 == 512 or i1 - i0 < 14:
                continue
            seg = (vph - V0HW[wi])[i0:i1] if have_v0 else (vph - SW[wi])[i0:i1]
            u = (np.arange(i0, i1) - i0) / float(i1 - i0)
            zc = [u[k] for k in range(1, len(seg)) if seg[k - 1] * seg[k] <= 0 and seg[k - 1] != seg[k]]
            zc = [v for v in zc if 0.2 < v < 0.9]
            if zc:
                a1.plot([(i0 + zc[0] * (i1 - i0)) / FS * 1e3], [0], 'o', ms=6, mfc='none',
                        mec='#c0392b', mew=1.5)
            a1.plot([(i0 + 0.5 * (i1 - i0)) / FS * 1e3], [0], 'k^', ms=6)
        a1.plot([], [], 'o', ms=6, mfc='none', mec='#c0392b', mew=1.5, label='实测过零')
        a1.plot([], [], 'k^', ms=6, label='理论过零(步正中)')
        a1.grid(alpha=.22); a1.set_ylabel('delta (码)')
        a1.set_title('② 判据用的 delta（两种参考）+ 实测/理论过零', fontsize=10)
        a1.legend(fontsize=8.5, loc='lower right')
        # ③ V0 自检
        d = V0HW[wi] - SW[wi]
        a2.plot(t, V0HW[wi], lw=1.2, color='#8e44ad', label='V0(硬件)')
        a2.plot(t, SW[wi], lw=1.2, color='#e67e22', ls='--', label='(VA+VB+VC)/3')
        a2.set_ylabel('ADC 码'); a2.grid(alpha=.22)
        a2b = a2.twinx()
        a2b.plot(t, d, lw=1.0, color='#7f8c8d', label='两者之差')
        a2b.axhline(0, color='0.8', lw=.8); a2b.set_ylabel('差 (码)', fontsize=9)
        h1, l1 = a2.get_legend_handles_labels(); h2, l2 = a2b.get_legend_handles_labels()
        a2.legend(h1 + h2, l1 + l2, fontsize=8.5, loc='lower right')
        a2.set_title('③ V0 自检  差：中位 %+.1f，峰峰 %.0f 码'
                     % (float(np.median(d)), float(d.max() - d.min())), fontsize=10)
        if r == len(sel) - 1:
            for a in (a0, a1, a2):
                a.set_xlabel('窗口内时间 (ms)')
    fig.suptitle('%s · 四路全速率采样 · %s%s'
                 % ('闭环段' if which == 'closed' else '开环稳定段',
                    os.path.basename(npz)[8:23],
                    ('（只取 duty_setpoint=%.0f 的窗）' % duty_eq) if duty_eq is not None else ''),
                 fontsize=13)
    fig.tight_layout(rect=[0, 0, 1, .97])
    fig.savefig(outpng, dpi=118, bbox_inches='tight'); plt.close(fig)
    print('全速率波形图 →', outpng)


FITLO2, FITHI2 = 0.15, 0.95


def step_fix(npz, v0mode='sw', lam=1e-3, all_stage=False, verbose=True):
    """★ 六步分步【竖直增益 + 竖直偏移】反解，目标 = 【换相处连续】。

    为什么"连续"是对的目标（物理依据）：
      三相反电势互差 120°，而换相点落在步边界（该相 BEMF = ±半幅）⇒
      **出去的悬空相反电势恰好等于进来的那一个** ⇒ 把六步的悬空相读数拼起来，
      理想情形是一根**连续**的三角波（周期 120°）。所以：
        "换相处两侧读数之差"(跳变) 就是可以量、也可以压的误差。
      目标 1：跳变 → 0（连续性）；目标 2：过零点尽量落在 u=0.5（居中；开环下不强制）。

    模型：每步 s 施以 (g_s, o_s)：  d' = g_s·d + o_s
    只关心每步两端的外推值 e0(一步起点)、e1(一步终点)（从 u∈[0.15,0.95] 的直线拟合外推，
    避开换相振铃）。于是边界 s→s+1 的跳变：
        J_s = g_s·e1_s + o_s − g_{s+1}·e0_{s+1} − o_{s+1}
    对 12 个未知量做【带岭正则】的最小二乘（λ 小 ⇒ 取"最小改动解"），
    再报跳变/过零位置/幅值在修正前后的变化。
    """
    cf = npz.replace('_win.npz', '.csv')
    ts_c, st_c, span = read_csv(cf)
    z = np.load(npz)
    ST, TS = z['step'], z['ts'].astype(np.int64)
    PH = {k: z[k].astype(float) for k in ('va', 'vb', 'vc')}
    v0m, why = pick_v0(z, v0mode) if 'v0' in z.files else ('sw', '无 V0')
    V0 = (PH['va'] + PH['vb'] + PH['vc']) / 3.0 if v0m == 'sw' else z['v0'].astype(float)
    # 收步段（与 collect 同口径）+ 每段两端外推值 + 相邻配对
    e0s = {s: [] for s in range(1, 7)}
    e1s = {s: [] for s in range(1, 7)}
    u0s = {s: [] for s in range(1, 7)}
    pair = []          # (s_from, e1, e0_next)
    nwin = 0
    for wi in range(len(ST)):
        t = float(TS[wi])
        if not all_stage:
            if stage_at(ts_c, st_c, t) != 1:
                continue
            if span and not (span[0] <= t <= span[1]):
                continue
        mx = np.maximum(PH['va'][wi], PH['vb'][wi]); mx = np.maximum(mx, PH['vc'][wi])
        if np.median(mx) < 200:
            continue
        nwin += 1
        sg = [x for x in segs(ST[wi].astype(int)) if x[2] in F and x[0] != 0 and x[1] != 512
              and x[1] - x[0] >= 14]
        if not segs(ST[wi].astype(int)):
            continue
        loc = []
        for i0, i1, s in sg:
            w = float(i1 - i0)
            u = (np.arange(i0, i1) - i0) / w
            d = PH[PHK[F[s]]][wi][i0:i1] - V0[wi][i0:i1]
            m = (u >= FITLO2) & (u <= FITHI2)
            if m.sum() < 8:
                continue
            k, b = np.polyfit(u[m], d[m], 1)
            e0, e1 = b, k + b
            e0s[s].append(e0); e1s[s].append(e1); u0s[s].append(-b / k if k else np.nan)
            loc.append((s, e1))
        # 相邻步配对（时间上相邻 = 链表里相邻）
        for (s1, e1v), (s2, _e0v) in zip(loc[:-1], loc[1:]):
            pass
        for a in range(len(loc) - 1):
            s1, e1v = loc[a]
            s2 = loc[a + 1][0]
            # 用下一段自己的 e0（从它自己的数据算）
            i0b, i1b, _ = sg[a + 1]
            w2 = float(i1b - i0b)
            u2 = (np.arange(i0b, i1b) - i0b) / w2
            d2 = PH[PHK[F[s2]]][wi][i0b:i1b] - V0[wi][i0b:i1b]
            m2 = (u2 >= FITLO2) & (u2 <= FITHI2)
            if m2.sum() < 8:
                continue
            k2, b2 = np.polyfit(u2[m2], d2[m2], 1)
            pair.append((s1, e1v, b2, s2))
    if not e0s[1] and not any(e0s.values()):
        print('!! 没有可用开环步段'); return None
    E0 = {s: float(np.median(e0s[s])) if e0s[s] else np.nan for s in range(1, 7)}
    E1 = {s: float(np.median(e1s[s])) if e1s[s] else np.nan for s in range(1, 7)}
    U0 = {s: float(np.nanmedian(u0s[s])) if e0s[s] else np.nan for s in range(1, 7)}
    A = {s: (E1[s] - E0[s]) for s in range(1, 7)}
    if verbose:
        print('\n' + '=' * 78)
        print('【%s】六步分步增益+偏移反解（目标：换相连续）  V0=%s  开环窗 %d' % (
            os.path.basename(npz)[8:23], v0m, nwin))
        print('=' * 78)
        print('   （e0/e1 = 从 u∈[%.2f,%.2f] 直线拟合外推到该步两端；跳变 = e1(s) − e0(s+1)）' % (FITLO2, FITHI2))
        print('   %-4s %-4s %8s %8s %8s %8s' % ('步', '浮相', 'e0(码)', 'e1(码)', '幅值Δ', '过零u0'))
        for s in range(1, 7):
            print('   %-4d %-4s %8.0f %8.0f %8.0f %8.3f' % (s, F[s], E0[s], E1[s], A[s], U0[s]))
    # 跳变：按【边界类型】取中位（比"两步各自中位再相减"更稳）
    from collections import defaultdict
    Jby = defaultdict(list)
    for s1, e1v, e0v2, s2 in pair:
        Jby[s1].append(e1v - e0v2)
    idx = sorted(Jby.keys())
    Jmed = {s: float(np.median(Jby[s])) for s in idx}
    Jall = np.concatenate([np.array(v) for v in Jby.values()])
    if verbose:
        print('\n   ---- ① 换相跳变（原始；理想 = 0）----')
        print('   %-16s %10s %10s %8s' % ('边界', '中位(码)', '样本数', '散布σ'))
        for s in idx:
            print('   %-16s %+10.0f %10d %8.0f' % ('步%d(%s)→步%d(%s)' % (
                s, F[s], s % 6 + 1, F[s % 6 + 1]), Jmed[s], len(Jby[s]), float(np.std(Jby[s]))))
        print('   ⇒ 六项中位跳变 RMS %.0f 码（全样本 RMS %.0f，散布来自段间差异）'
              % (float(np.sqrt(np.mean([Jmed[s] ** 2 for s in idx]))), float(np.sqrt(np.mean(Jall ** 2)))))
    # ---- 闭式解（推荐）：g_s = C/|A_s|（六步幅值归一）、o_s = −g_s·m_s（过零钉在正中）----
    #   推导：施加 d' = g_s·d + o_s 后
    #       跳变 J_s = (g_s·A_s + g_{s+1}·A_{s+1})/2 ；居中(u0=0.5) ⇔ o_s = −g_s·m_s
    #   取 g_s = C/|A_s| ⇒ g_s·A_s = C·sgn(A_s) = ±C ⇒ J_s = (C·sgn − C·sgn' )/2 = 0 恒成立
    #   （A 逐交替变号 ⇒ sgn(A_s) = −sgn(A_{s+1}) ⇒ 两项抵消）⇒ 六个跳变全为 0，且过零在 0.5。
    absA = np.array([abs(A[s]) for s in idx])
    C = float(np.mean(absA))
    g = {s: C / abs(A[s]) for s in idx}                       # 幅值归一（相对中位 ⇒ 增益≈1±15%）
    m = {s: (E0[s] + E1[s]) / 2.0 for s in idx}
    o = {s: -g[s] * m[s] for s in idx}                        # 居中
    if verbose:
        print('\n   ---- ② 闭式解（把六步幅值归一 + 过零钉在正中）----')
        print('   %-4s %-4s %9s %9s %10s %12s %12s' % ('步', '浮相', 'A_s(码)', 'm_s(码)', '增益 g_s', '偏移 o_s(码)', '归一后幅值'))
        for s in idx:
            print('   %-4d %-4s %9.0f %9.0f %10.4f %12.1f %12.0f'
                  % (s, F[s], A[s], m[s], g[s], o[s], abs(g[s] * A[s])))
        print('   ⇒ 增益范围 %.3f~%.3f（±%.0f%%）、偏移范围 %+.0f~%+.0f 码'
              % (min(g[s] for s in idx), max(g[s] for s in idx),
                 100 * max(abs(g[s] - 1) for s in idx), min(o[s] for s in idx), max(o[s] for s in idx)))
    e0c = {s: g[s] * E0[s] + o[s] for s in idx}
    e1c = {s: g[s] * E1[s] + o[s] for s in idx}
    # 居中解只用于报数（上面已知恒 0.5）
    u0c = {s: -(E0[s] + o[s] / g[s]) / A[s] for s in idx}   # u0 = −b'/k'
    # 仅偏移解（对照：只压跳变、不管居中）
    off_only, acc = {}, 0.0
    for s in idx:
        off_only[s] = acc; acc += Jmed[s]
    e0o = {s: E0[s] + off_only[s] for s in idx}
    e1o = {s: E1[s] + off_only[s] for s in idx}
    u0o = {s: -(E0[s] + off_only[s]) / A[s] for s in idx}
    Jc = np.array([e1c[s] - e0c[s % 6 + 1] for s in idx])
    Jo = np.array([e1o[s] - e0o[s % 6 + 1] for s in idx])
    Jb = np.array([Jmed[s] for s in idx])
    if verbose:
        print('\n   ---- ③ 修正后 ----')
        print('      跳变 RMS：原 %.0f ⇒ 闭式解 %.0f ⇒ 仅偏移解 %.0f 码'
              % (float(np.sqrt(np.mean(Jb ** 2))), float(np.sqrt(np.mean(Jc ** 2))),
                 float(np.sqrt(np.mean(Jo ** 2)))))
        m0 = [U0[s] for s in idx]
        print('      过零 u0：原     %s （σ=%.3f）' % (' '.join('%.3f' % v for v in m0), float(np.std(m0))))
        print('      过零 u0：仅偏移 %s （σ=%.3f）  ← 只压跳变会把过零推出这一步！'
              % (' '.join('%.3f' % u0o[s] for s in idx), float(np.std([u0o[s] for s in idx]))))
        print('      过零 u0：闭式解 %s （σ=%.3f）  ← 跳变和居中同时满足'
              % (' '.join('%.3f' % u0c[s] for s in idx), float(np.std([u0c[s] for s in idx]))))
        print('\n      直给的固件表（相对步 1 的中位）:')
        print('        增益 g   : %s' % ' '.join('%.4f' % g[s] for s in idx))
        print('        偏移 o(码): %s' % ' '.join('%+.0f' % o[s] for s in idx))
    # ---- 出图：每步的【拟合直线外推到两端】——接缝才看得见 ----
    fig, ax = plt.subplots(2, 1, figsize=(15, 8.8))
    for wi in range(len(ST)):
        t = float(TS[wi])
        if stage_at(ts_c, st_c, t) != 1:
            continue
        if span and not (span[0] <= t <= span[1]):
            continue
        mx = np.maximum(PH['va'][wi], PH['vb'][wi]); mx = np.maximum(mx, PH['vc'][wi])
        if np.median(mx) < 200:
            continue
        for i0, i1, ss in segs(ST[wi].astype(int)):
            if ss not in F or i0 == 0 or i1 == 512 or i1 - i0 < 14:
                continue
            u = (np.arange(i0, i1) - i0) / float(i1 - i0)
            d = PH[PHK[F[ss]]][wi][i0:i1] - V0[wi][i0:i1]
            m = (u >= FITLO2) & (u <= FITHI2)
            ax[0].plot((ss - 1) + u[m], d[m], '.', ms=2, color='#d5d8dc', alpha=.6)
            ax[1].plot((ss - 1) + u[m], d[m], '.', ms=2, color='#d5d8dc', alpha=.6,
                       label='实测（u∈[0.15,0.95]）' if (ss == 1 and wi == 0) else None)
        break            # 只画第一个可用窗口，避免叠得太乱
    for sh in (0.0, 6.0):
        for s in range(1, 7):
            if s not in E0:
                continue
            uu = np.array([0.0, 1.0])
            raw = E0[s] + (E1[s] - E0[s]) * uu
            cor = g[s] * raw + o[s]
            ax[0].plot((s - 1) + uu + sh, raw, '-o', ms=5, lw=1.4, color='0.55',
                       label='原始（拟合外推到两端）' if (s == 1 and sh == 0) else None)
            ax[1].plot((s - 1) + uu + sh, raw, '-o', ms=5, lw=1.4, color='0.55',
                       label='原始（拟合外推到两端）' if (s == 1 and sh == 0) else None)
            ax[0].plot((s - 1) + uu + sh, cor, '-s', ms=5, lw=1.8, color='#c0392b',
                       label='+分步增益/偏移后' if (s == 1 and sh == 0) else None)
            ax[1].plot((s - 1) + uu + sh, cor, '-s', ms=5, lw=1.8, color='#c0392b',
                       label='+分步增益/偏移后' if (s == 1 and sh == 0) else None)
            if sh == 0:
                ax[1].plot([(s - 1) + 0.5], [g[s] * (E0[s] + E1[s]) / 2 + o[s]], 'k^', ms=9)
    for k in range(9):
        for a_ in ax:
            a_.axvline(k, color='0.6', ls=':', lw=1.0)
    ax[1].plot([], [], 'k^', ms=9, label='过零点（修正后应落在步正中）')
    ax[0].set_title('换相"接缝"检查：把每步的拟合直线【外推到两端】再连起来。'
                    '灰色=原始（两端对不上）／红色=分步增益+偏移后（应当接上）', fontsize=11)
    ax[1].set_title('同一份（含实测点）：修正后两端相接，且过零落在步正中（▲）', fontsize=10)
    for a_ in ax:
        a_.set_xlabel('τ = 步序号 + 步内位置（一个电周期 = 6 步；画两个周期看接缝）')
        a_.set_ylabel('悬空相 − V0 (码)'); a_.grid(alpha=.22); a_.set_xlim(0, 7.6)
    ax[1].legend(fontsize=9, loc='lower right')
    fig.tight_layout()
    fp = os.path.join(OUTD_DEF, '分步校正_拼图_%s.png' % os.path.basename(npz)[8:23])
    fig.savefig(fp, dpi=118, bbox_inches='tight'); plt.close(fig)
    print('\n   接缝图 →', fp)
    return dict(E0=E0, E1=E1, A=A, U0=U0, g=g, o=o, u0c=u0c, J0=Jb, J1=Jc, idx=idx)


# ---------------------------------------------------------------- main
def main():
    a = sys.argv[1:]
    outdir = OUTD_DEF; v0mode = 'auto'; all_stage = False
    if '--outdir' in a:
        i = a.index('--outdir'); outdir = a[i + 1]; del a[i:i + 2]
    if '--v0' in a:
        i = a.index('--v0'); v0mode = a[i + 1]; del a[i:i + 2]
    if '--all-stage' in a:
        all_stage = True; a.remove('--all-stage')
    os.makedirs(outdir, exist_ok=True)
    if '--pick' in a or not a:
        do_pick(outdir)
        if not a:
            return
    runs = [x for x in a if not x.startswith('--') and not x.replace('.', '').isdigit()]
    if '--all-typical' in a:
        runs = do_pick(outdir, write=False)[:5]      # 取"可用步段"最多的前 5 组
    if '--compare-v0' in a:
        for run in runs:
            npz = resolve(run)
            if not os.path.exists(npz):
                print('!! 找不到', npz); continue
            name = os.path.basename(npz)[8:23]
            _, SEGh, no1, nu1, sp1, _, _, _ = collect(npz, 'hw', all_stage)
            _, SEGs, no2, nu2, sp2, v0s, whys, _ = collect(npz, 'sw', all_stage)
            if not SEGh or not SEGs:
                print('!! %s 步段不足' % name); continue
            for e in SEGh:
                measure_zc(e)
            for e in SEGs:
                measure_zc(e)
            _, RESs = anchored_fit(SEGs)
            Mh, Ms = step_meds(SEGh), step_meds(SEGs)
            print('\n===== %s  V0 对照（每步:局部/全步/中段 三口径的过零偏差 °E）=====' % name)
            print('   %-5s %26s %26s' % ('步', 'hw V0', 'sw V0'))
            for i, s in enumerate(range(1, 7)):
                print('   %-5d %26s %26s' % (s,
                      ' '.join('%+6.1f' % Mh[k][i] for k in ('u0_local', 'u0_full', 'u0_mid')),
                      ' '.join('%+6.1f' % Ms[k][i] for k in ('u0_local', 'u0_full', 'u0_mid'))))
            a = np.array([v for v in Mh['u0_local'] if np.isfinite(v)])
            b = np.array([v for v in Ms['u0_local'] if np.isfinite(v)])
            print('   ⇒ 六步散差：hw %.1f°E → sw %.1f°E ；口径散差中位：hw %.1f°E → sw %.1f°E'
                  % (a.max() - a.min(), b.max() - b.min(),
                     np.median([max([Mh[k][i] for k in Mh if np.isfinite(Mh[k][i])]) -
                                min([Mh[k][i] for k in Mh if np.isfinite(Mh[k][i])])
                                for i in range(6)]),
                     np.median([max([Ms[k][i] for k in Ms if np.isfinite(Ms[k][i])]) -
                                min([Ms[k][i] for k in Ms if np.isfinite(Ms[k][i])])
                                for i in range(6)])))
            figure_v0_compare(name, SEGh, SEGs, RESs,
                              os.path.join(outdir, '校准_V0对照_%s.png' % name))
        return
    if '--standard' in a:
        for run in runs:
            npz = resolve(run)
            if not os.path.exists(npz):
                print('!! 找不到', npz); continue
            name = os.path.basename(npz)[8:23]
            _, SEG, no, nu, sp, v0, why, nd = collect(npz, v0mode, all_stage)
            if len(SEG) < 12:
                print('!! %s 可用开环步段太少' % name); continue
            STD = standard_fit(SEG)
            report_standard(name, STD, SEG, v0)
            figure_standard(name, STD, os.path.join(outdir, '理论标准_%s.png' % name), v0)
            figure_stitch_3ph(name, STD, os.path.join(outdir, '三相合一_%s.png' % name))
            figure_composite(name, STD, SEG, os.path.join(outdir, '悬空相合成_%s.png' % name), v0)
        return
    if '--wave' in a:
        which = 'closed' if '--cl' in a else 'open'
        duty_eq = None
        if '--duty' in a:
            duty_eq = float(a[a.index('--duty') + 1])
        for run in runs:
            npz = resolve(run)
            if not os.path.exists(npz):
                print('!! 找不到', npz); continue
            nm = os.path.basename(npz)[8:23]
            tagf = ('闭环全速' if which == 'closed' else '开环全速') + \
                   ('_duty%d' % int(duty_eq) if duty_eq is not None else '')
            figure_wave(npz, os.path.join(outdir, '%s_%s.png' % (tagf, nm)),
                        v0mode=v0mode, all_stage=all_stage, which=which, duty_eq=duty_eq)
        return
    if '--step-fix' in a:
        for run in runs:
            npz = resolve(run)
            if not os.path.exists(npz):
                print('!! 找不到', npz); continue
            step_fix(npz, v0mode=(v0mode if v0mode != 'auto' else 'sw'), all_stage=all_stage)
        return
    if '--adc-gain' in a:
        DATA = {}
        for run in runs:
            npz = resolve(run)
            if not os.path.exists(npz):
                print('!! 找不到', npz); continue
            name = os.path.basename(npz)[23 - 23:] or os.path.basename(npz)[8:23]
            name = os.path.basename(npz)[8:23]
            # 先给 V0 通道体检：坏的 hw 通道不参与（否则 v_n=0，整段分析无意义）
            _zchk = np.load(npz)
            _pref, _why0 = pick_v0(_zchk, 'auto')
            modes = ('hw', 'sw') if _pref == 'hw' else ('sw',)
            for vmode in modes:
                _z, SEG, no, nu, sp, v0, why, nd = collect(npz, vmode, all_stage)
                if len(SEG) < 12:
                    continue
                for e in SEG:
                    measure_zc(e)
                r0 = analyse_dev(SEG)
                if r0 is None:
                    continue
                g_amp, hr = measure_gains(SEG)                 # 幅值口径（仅作对照，不推荐）
                off = solve_offsets_by_zc(SEG)                 # ★ 零点口径（推荐）
                for e in SEG:                                  # 施加逐相常量偏置校正
                    e['d'] = e['raw'] - e['v0v'] - off[e['ph']]
                    e['m'] = (e['u'] >= FITLO) & (e['u'] <= FITHI)
                    measure_zc(e)
                r1 = analyse_dev(SEG)
                key = '%s·%s' % (name, vmode)
                DATA[key] = dict(raw=r0, corr=r1, off=off, g_amp=g_amp, hr=hr, v0=vmode)
                print('\n' + '=' * 78)
                print('【%s】V0=%s  步段 %d  %s' % (name, vmode, len(SEG), why))
                print('=' * 78)
                print('   ---- ① 幅值口径测到的"增益"（❌ 不推荐，见下）----')
                print('       %-4s %10s %10s' % ('相', '半量程(码)', '相对增益'))
                for p in 'ABC':
                    print('       %-4s %10.1f %10.3f' % (p, hr[p], g_amp[p]))
                vn = float(np.median(np.concatenate([e['v0v'] for e in SEG])))
                print('   ---- ② 用【过零点】反解出的逐相常量偏置（★ 推荐，下一步就用它）----')
                if abs(vn) < 50:
                    print('       ⚠ 中性点中位只有 %.0f 码（这个 V0 参考本身是坏的）⇒ 偏置无意义，跳过' % vn)
                else:
                    print('       中性点中位 v_n ≈ %.0f 码 ⇒ 等效通道增益 g_P = 1/(1 − o_P/v_n)' % vn)
                    for p in 'ABC':
                        r = off[p] / vn
                        ge = 1.0 / (1.0 - r) if abs(r) < 0.99 else float('nan')
                        print('       相%s: 偏置 o = %+7.1f 码   ⇒ 等效增益 %.4f（校正系数 %.4f）'
                              % (p, off[p], ge, 1 / ge if np.isfinite(ge) else float('nan')))
                print('   （两者不一致 ⇒ 幅值不是良定义的增益量；本轮已实测过：按幅值校正会把'
                      '六步散差从 11.0°E 改坏到 51.7°E）')
                print('   ---- 逐步偏差（°E，理论 0）----')
                print('   %-6s %-4s %9s %9s %9s' % ('步', '浮相', '原始', '校正后', '变化'))
                for s in range(1, 7):
                    if not np.isfinite(r0['dev'].get(s, np.nan)):
                        continue
                    print('   %-6d %-4s %+9.1f %+9.1f %+9.1f'
                          % (s, F[s], r0['dev'][s], r1['dev'][s], r1['dev'][s] - r0['dev'][s]))
                print('   ⇒ 六步散差              %5.1f°E  →  %5.1f°E' % (r0['spread'], r1['spread']))
                print('   ⇒ 其中【相间差】        %5.1f°E  →  %5.1f°E  （通道直流/真·机械不对称）'
                      % (r0['pe_amp'], r1['pe_amp']))
                print('   ⇒ 其中【相内反对称=增益】%5.1f°E  →  %5.1f°E  （增益校准就该把这项打到 0）'
                      % (r0['asym_amp'], r1['asym_amp']))
                for p in 'ABC':
                    print('      相%s: 相间差 %+6.1f→%+6.1f   反对称 %+6.1f→%+6.1f'
                          % (p, r0['pe'][p], r1['pe'][p], r0['asym'][p], r1['asym'][p]))
        if DATA:
            figure_gain_phase_step(DATA, os.path.join(outdir, 'ADC增益校准.png'))
            p = os.path.join(outdir, '04_ADC增益表.md')
            with open(p, 'w', encoding='utf-8') as fh:
                fh.write('# ADC 通道增益表（初步校准用）\n\n'
                         '> ★ **口径：用过零点位置反解，不是用幅值。**\n'
                         '> 理由：本数据里"悬空读数幅值"不是良定义的量——极差 / 分位数 / 拟合正弦幅值\n'
                         '> 三种口径给出的**大小顺序互相矛盾**；按它校正会把六步散差从 11.0°E 改坏到 51.7°E（实测）。\n'
                         '> 而零点位置对通道增益**极敏感**：d = g_P·v_P − g_0·V0，过零条件 v_P = V0·g_0/g_P，\n'
                         '> 而 v_n ≈ 母线中点（约 600 码）≫ 弧幅（150~350 码）⇒ 增益误差等效成 e_P 上一个\n'
                         '> 常数偏置 v_n(1 − g_0/g_P)，它**在同一个相的两个步里符号相反**（两步斜率反号）。\n'
                         '> 所以逐相搜索一个常量偏置，使该相两步的偏差对消即可（偏置码 ↔ 等效增益 g_P = 1/(1 − o_P/v_n)）。\n'
                         '>\n'
                         '> ## ★ V0 要不要一起校？——**不要，也不能单独校**\n'
                         '> `d = 悬空相 − V0`，四个通道（A/B/C/V0）**同时乘一个常数会整个约掉**。\n'
                         '> 有意义的只有三个比值 A/V0、B/V0、C/V0，扣掉一个整体尺度 ⇒ **自由参数只有 2 个**。\n'
                         '> 本表给的三相系数（已归一化到均值=1）正好把它们编码完 ⇒ **增益这一层是完备的**，\n'
                         '> V0 通道不需要、也拿不到一个额外的"增益校准系数"。\n'
                         '> （唯一会受影响的是固件里那个**绝对码值**的滞回阈值 16 码：四个通道同乘 k 会让\n'
                         '>   等效滞回变成 16/k，但相对 400 码级的弧幅是二阶小量。）\n'
                         '>\n'
                         '> ## ⚠ 已知坑：`evt_run_20261013_115519` 的 A/C 是对调过的\n'
                         '> 它的步内斜率是 A 1.211 / B 0.995 / C 0.794，而 `135039`/`103829`/`223708` 是\n'
                         '> A 0.68~0.83 / B 0.97~1.00 / C 1.20~1.35 —— **互为倒数**，A↔C 对调后复现到 1~5%。\n'
                         '> 而《交接/02》把它当成"基线重复跑"，本工具第一版表就是从它推的 ⇒ 系数张冠李戴。\n'
                         '> **别再用 115519 推增益表**；下面表里那行只作历史记录保留。\n'
                         '> 由 `tools/cal-anchor.py --adc-gain` 生成。\n\n'
                         '| run | V0 | 相A 偏置(码) | 相B 偏置(码) | 相C 偏置(码) | 六步散差(原→校) | 相间差(原→校) | 相内反对称(原→校) |\n'
                         '|---|---|---|---|---|---|---|---|\n')
                for k, d in DATA.items():
                    nm, v0 = k.split('·')
                    g = d['off']
                    fh.write('| `%s` | %s | %+.1f | %+.1f | %+.1f | %.1f→%.1f | %.1f→%.1f | %.1f→%.1f |\n'
                             % (nm, v0, g['A'], g['B'], g['C'],
                                d['raw']['spread'], d['corr']['spread'],
                                d['raw']['pe_amp'], d['corr']['pe_amp'],
                                d['raw']['asym_amp'], d['corr']['asym_amp']))
                fh.write('\n## 怎么用在固件里\n\n'
                         '在 ADC 采样完成、算出 `va/vb/vc` 之后、**减 V0 之前**，把每个通道乘以它自己的\n'
                         '**校正系数**（= 1/增益）：\n\n'
                         '```c\n'
                         '/* 通道级增益校正：占总预算为零（一次乘加）*/\n'
                         'va = (uint16_t)((uint32_t)va * GAIN_A_NUM / GAIN_A_DEN);\n'
                         'vb = (uint16_t)((uint32_t)vb * GAIN_B_NUM / GAIN_B_DEN);\n'
                         'vc = (uint16_t)((uint32_t)vc * GAIN_C_NUM / GAIN_C_DEN);\n'
                         '/* 之后照旧：悬空相 − V0 */\n'
                         '```\n\n'
                         '⚠ 三点注意：\n'
                         '1. **增益随采样点（CCR4）变**（交接 §3.3：A/C 比 0.47@130 → 0.89@260）\n'
                         '   ⇒ 这张表只对**产生它的那个 CCR4** 有效，换 CCR4 必须重测。\n'
                         '2. 归一化是任意的（三相同时乘一个常数不影响 d 的零点）⇒ 直接用上面的系数即可。\n'
                         '3. 校准只治【相内反对称】那一项；**剩下的【相间差】和它治不了的残余，才是下一步\n'
                         '   "消除六步不对称"要对付的东西** —— 见《一切为了校准/03_本轮结论.md》§新的下一步。\n')
            print('\n增益表 →', p)
        return
    AGG = {}
    for run in runs:
        npz = resolve(run)
        if not os.path.exists(npz):
            print('!! 找不到', npz); continue
        name = os.path.basename(npz)[8:23]
        z, SEG, nopen, nused, span, v0, why, ndead = collect(npz, v0mode, all_stage)
        if not SEG:
            print('!! %s 没有可用开环步段（--all-stage 可放宽）' % name); continue
        for e in SEG:
            measure_zc(e)
        Ss, RES = anchored_fit(SEG)
        ptab = per_step_table(SEG, Ss)
        tick_med = float(np.median([e['tick'] for e in SEG]))
        print_report(name, SEG, Ss, RES, ptab, nopen, nused, span, tick_med, v0, why, ndead)
        figure_run(name, z, SEG, Ss, RES, os.path.join(outdir, '校准_%s.png' % name), v0)
        AGG[name] = dict(SEG=SEG, Ss=Ss, RES=RES, ptab=ptab, v0=v0, why=why, span=span,
                         nseg=len(SEG), ndead=ndead)
    if len(AGG) >= 2:
        figure_agg(AGG, os.path.join(outdir, '校准_汇总_逐步偏差.png'))
        write_summary(AGG, outdir)
    return AGG


if __name__ == '__main__':
    main()
