#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
index-runs.py —— 把所有采集数据（evt_run_*.csv / _win.npz）做成一张索引表（Markdown）。

为什么要它：`~/桌面/RM2027/evt_run_*.csv` 有 40+ 组，文件名只有时间戳，
**数据文件本身不带固件版本信息**（这是我留下的坑，见《交接/00_交接说明.md》）。
本脚本按"能从数据里量出来的东西"给每组数据打指纹，方便交接和回查。

口径（与《闭环补偿方案演变记录.md》一致）：1 拍 = 0.5 µs；步长(拍) = 采样点数 × 83.333。
用法:  python3 tools/index-runs.py [目录=~/桌面/RM2027] [--csv 只输出表格不加说明]
"""
import sys, os, glob, csv
import numpy as np

DIR = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('--') else os.path.expanduser('~/桌面/RM2027')
F = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
PER_SAMPLE = 1e6 / 24000 / 0.5      # 1 采样点 = 83.333 拍

# 文档里已明确标注过的 run（《闭环补偿方案演变记录.md》）
KNOWN = {
    '20261013_103829': '复测#1 基线（无补偿）',
    '20261013_103913': '复测#2 奇偶偏置',
    '20261013_105232': '复测#3 电压域增益+偏置（干净重跑）',
    '20261013_104034': '复测#4 每步偏置表',
    '20261013_105721': '复测#4 逐窗口（闭环 1500ms）',
}


def steps_of(st):
    """返回该窗口所有完整步段的长度（采样点数）"""
    o, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k - 1]:
            if st[i0] in F:
                o.append(k - i0)
            i0 = k
    return o


rows = []
for npzf in sorted(glob.glob(os.path.join(DIR, 'evt_run_*_win.npz'))):
    tag = os.path.basename(npzf)[8:23]      # ★ 原先写 [8:21]，把秒的最后两位截掉了
                                            #   ⇒ 表名对不上、KNOWN 的 5 条标注永远匹配不上
    z = np.load(npzf)
    ST = z['step']
    per_win = [np.median(steps_of(ST[wi])) for wi in range(len(ST))]
    per_win = [x for x in per_win if x == x]
    if not per_win:
        continue
    cruise = sum(1 for x in per_win if 1600 / PER_SAMPLE <= x <= 2200 / PER_SAMPLE)
    # CSV：交棒是否发生 + duty
    # ★ 判据修正：不能再写"出现过 stage==0 就算交棒" —— 每次上电起始那 2 行就是 0
    #   （启动阶段），于是"全程纯开环"的 run 也会被标成"交棒=是"。
    #   真正的交棒是 stage 由 1（开环）变 0（闭环）这一次跳变。
    handed, duty, prev = False, [], None
    cf = npzf.replace('_win.npz', '.csv')
    if os.path.exists(cf):
        with open(cf, newline='', encoding='utf-8', errors='replace') as fh:
            for r in csv.DictReader(fh):
                s = r.get('stage')
                if s in ('0', '1'):
                    if s == '0' and prev == '1':
                        handed = True
                    prev = s
                if 'duty_setpoint' in r:
                    try:
                        v = float(r['duty_setpoint'])
                        if 0 < v < 2000:
                            duty.append(v)
                    except ValueError:
                        pass
    rows.append(dict(
        tag=tag,
        n=len(ST),
        s_min=min(per_win) * PER_SAMPLE,
        s_med=np.median(per_win) * PER_SAMPLE,
        s_max=max(per_win) * PER_SAMPLE,
        cruise=cruise,
        handed=handed,
        duty_med=(np.median(duty) if duty else float('nan')),
        br='是' if handed else '**否**',
        label=KNOWN.get(tag, ''),
        ready='★' if cruise >= 30 else ('' if cruise < 5 else '·'),
    ))

print('| run | 窗数 | 步长 最小/中位/最大 (拍) | 巡航窗 | 交棒 | duty中位 | 文档标注 | 可分析 |')
print('|---|---|---|---|---|---|---|---|')
for r in rows:
    print('| `evt_run_%s` | %d | %.0f / %.0f / %.0f | %d | %s | %s | %s | %s |' % (
        r['tag'], r['n'], r['s_min'], r['s_med'], r['s_max'], r['cruise'], r['br'],
        ('%.0f' % r['duty_med']) if r['duty_med'] == r['duty_med'] else '—',
        r['label'] or '（未标注）', r['ready']))
if '--csv' not in sys.argv:
    print()
    print('列说明：')
    print('  · **巡航窗** = 步长落在 1600~2200 拍的窗口数（＝可用于过零/形状分析的稳态段）')
    print('  · **交棒** = CSV 里 stage 由 1（开环）跳成 0（闭环）发生过；否 ⇒ 这一轮**全程开环**'
          '（注意：不能写"出现过 stage=0 就算交棒"——每次上电起始两行就是 0）')
    print('  · **可分析** ★ = 巡航窗 ≥30（够做统计）；· = 5~29；空 = <5（基本只能看过程）')
    print('  · 步长越小＝越快：1600 拍/步 ≈ 2080 转/分（电机机械转速，6 极对）')
    print('  · ⚠ 数据文件不带固件版本 ⇒ 「未标注」的行无法确定当时烧的是哪个分支（见交接说明 §4）')
