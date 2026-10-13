#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
count-run.py —— 【计数轮】判分器：关掉窗口流 → 跑一轮 → 出精确报表。

用法:
  python3 tools/count-run.py [秒数=10] [标签]

为什么必须关窗口流（这是本项目"漏过零"统计长期不可信的根因）：
  · 窗口流一个包 12+512*12 = 6156 B，每 21.3ms 发一个 ⇒ 需要 ~288 KB/s
  · 串口 1Mbaud 只有 ~125 KB/s ⇒ 数学上不可能共存，开着窗口流时事件流最多采到 ~3 成
  · 而"漏过零次数"只能从事件载荷 p[40] 数（bemf_timeout_happened 在收油时被
    main.c:4191 清零 ⇒ 跑后 SWD dump 读它恒为 0，那个口径作废）
  ⇒ 关掉窗口流后事件流只要 ~43 KB/s，实测 12796/13978 = 91.5% 全收、线上字节数
    与"换相数×49B"精确对上 ⇒ 零丢失 ⇒ 超时计数成为精确值。
  ★ 而且它本身就是个观测者效应源：实测同一固件、同一轮长，
    窗口流开 → bemf_timeout 最大 57（≈9.5 次/秒）；窗口流关 → 全程 0。见交接/04。

判分口径（每轮都打这 5 项）：
  ① bemf_timeout 的【最大值】（不是末值！收油会把它清零）
  ② 闭环段内的超时次数 / 秒
  ③ 各 step 的真实换相间隔（Δts 中位，拍）—— 六步散布
  ④ zc_reject 增量（判据拒收次数，与"超时"互补：拒收是"来了不认"，超时是"压根没来"）
  ⑤ 交棒是否发生 / 开环闭环事件数 / 坏帧
"""
import sys, os, re, csv, time, glob, subprocess
import numpy as np

REPO = '/home/lpl/桌面/RM2027/AM32-rm2027'
OUTD = '/home/lpl/桌面/RM2027'
SECS = float(sys.argv[1]) if len(sys.argv) > 1 else 10.0
TAG = sys.argv[2] if len(sys.argv) > 2 else ''
TICK = 2500.0 / 30.0        # 83.333 拍/ADC 帧


def sh(*a, **kw):
    return subprocess.run(a, cwd=REPO, capture_output=True, text=True, **kw)


def cooldown():
    f = '/tmp/esc_last_run'
    if os.path.exists(f):
        last = int(float(open(f).read().split('.')[0]))
        w = 60 - (int(time.time()) - last)
        if w > 0:
            print(f'   冷却中 {w}s ...')
            time.sleep(w)


def poke(sym, val):
    return sh('tools/esc-dbg.sh', 'poke', sym, str(val)).stdout.strip()


def analyse(csvp):
    rows = list(csv.DictReader(open(csvp)))
    if not rows:
        print('❌ CSV 空'); return
    ts = np.array([int(r['ts_frame']) for r in rows], float)
    stg = np.array([int(r['stage']) for r in rows])
    st = np.array([int(r['step']) for r in rows])
    tmo = np.array([int(r['bemf_timeout']) for r in rows])
    rej = np.array([int(r['zc_reject']) for r in rows])
    m = stg == 0
    print(f'   行数 {len(rows)}   开环 {int((stg==1).sum())}   闭环 {int(m.sum())}', end='')
    print('   ✅ 交棒发生' if m.sum() else '   ❌ 未交棒')
    if m.sum():
        dur = (ts[m].max() - ts[m].min()) / 24000.0
        print(f'   闭环 ts 跨度 {dur:5.2f}s（含停机后滑行）')
        print(f'   ① bemf_timeout 最大 = {tmo.max():4d}   ★ 这是漏过零次数'
              f'{"（=0，判据干净）" if tmo.max()==0 else ""}')
        if tmo.max():
            # 计数增长的时刻（用来定位发生在哪一步）
            inc = np.where(np.diff(tmo) > 0)[0] + 1
            s_inc = st[inc] if len(inc) else np.array([], int)
            dist = {s: int((s_inc == s).sum()) for s in range(1, 7)}
            print(f'   ② 超时增长 {len(inc)} 次，按事件 step 分布 ' +
                  ' '.join(f'{s}:{dist[s]}' for s in range(1, 7)))
    print(f'   ③ 各 step 真实换相间隔（Δts 中位，拍）：')
    dts = np.diff(ts); good = (dts > 0) & (dts < 300)
    med = []
    for s in range(1, 7):
        mm = good & (st[1:] == s) & (stg[1:] == 0) & (stg[:-1] == 0)
        v = np.median(dts[mm]) * TICK if mm.sum() else float('nan')
        med.append(v)
        print(f'        step{s}: n={int(mm.sum()):5d}  中位 {v:7.0f} 拍', end='')
        print(f'   P90 {np.percentile(dts[mm],90)*TICK:7.0f}' if mm.sum() else '')
    ok = [v for v in med if v == v]
    if ok:
        print(f'      ⇒ 六步中位 {min(ok):.0f}~{max(ok):.0f} 拍，'
              f'极差 {100*(max(ok)-min(ok))/np.mean(ok):.1f}%（越小越均匀）')
    print(f'   ④ zc_reject 末值 {rej[-1]}（判据拒收累计；≤ 每次换相约 0.4 倍为常态）')
    return tmo.max()


def main():
    print(f'===== 计数轮 {TAG}  （{SECS:.0f}s 采集，闭环额度 1500ms）=====')
    cooldown()
    print('   → 关窗口流（把带宽全给事件流）')
    print('   ', poke('win_enable', 0))
    before = set(glob.glob(os.path.join(OUTD, 'evt_run_*.csv')))
    r = sh('python3', 'tools/cap-evt.py', str(SECS))
    for ln in r.stdout.splitlines():
        if re.search(r'原始字节|开环 |长步|换相间隔', ln):
            print('   ' + ln.strip())
    after = set(glob.glob(os.path.join(OUTD, 'evt_run_*.csv'))) - before
    if not after:
        print('❌ 没产生新 CSV'); return
    csvp = sorted(after, key=os.path.getmtime)[-1]
    print(f'   CSV {os.path.basename(csvp)}')
    analyse(csvp)
    print('   → 恢复窗口流')
    print('   ', poke('win_enable', 1))


main()
