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


def link_bytes(secs=3.0):
    """读串口 N 秒，返回字节数。用来在跑前确认 ESC 的发送侧没卡死。"""
    try:
        import serial
    except Exception:
        return -1
    try:
        # ★ xien/n2：波特率跟固件 ADC_CAP_BAUD 走；2 Mbaud 时用 ESC_BAUD=2000000 调本脚本
        import os as _os
        sp = serial.Serial('/dev/ttyUSB0', int(_os.environ.get('ESC_BAUD', '1000000')), timeout=0.5)
        sp.reset_input_buffer()
        n, t0 = 0, time.time()
        while time.time() - t0 < secs:
            n += len(sp.read(65536))
        sp.close()
        return n
    except Exception:
        return -1


def analyse(csvp):
    rows = list(csv.DictReader(open(csvp)))
    if not rows:
        print('❌ CSV 空'); return None
    ts = np.array([int(r['ts_frame']) for r in rows], float)
    stg = np.array([int(r['stage']) for r in rows])
    st = np.array([int(r['step']) for r in rows])
    tmo = np.array([int(r['bemf_timeout']) for r in rows])
    rej = np.array([int(r['zc_reject']) for r in rows])
    du = np.array([int(r['duty']) for r in rows])
    cur = np.array([int(r['cur']) for r in rows])
    # ★★ 判分窗口 = 【锚定闭环】那一段 ★★
    #   踩过两次坑，所以窗口不能用 stage 定义：
    #   ① stage==0 不等于"在闭环里" —— 没给油时的空转事件 stage 也是 0
    #      （实测把 5s 采集算成"闭环跨度 90.88s"就是这个原因）；
    #   ② 额度到点固件自己停机后锚定失效，占空比退回按油门算（input≈400⇒duty≈351），
    #      电机还在转 —— 那是另一种工况，混进来会把六步长度/漏过零都带偏。
    #   锚定闭环的定义很干净：stage==0 且 duty==300。
    ANCOR = 300
    anch = (stg == 0) & (np.abs(du - ANCOR) <= 2)
    drv = (stg == 0) & (du > 100)
    if len(rows) < 300 or int(anch.sum()) < 150:
        print(f'   ⚠⚠ 本轮作废：事件 {len(rows)} 条、其中锚定闭环只有 {int(anch.sum())} 条'
              f'（电机没转 / F1 油门没生效 / 采集没覆盖到闭环）。不要读这一轮的数！')
        return None
    # ★ 只保留 ts【连续】的那一块：CSV 里会夹进 ts 错乱/陈旧的事件
    #   （实测踩到：5s 采集算出"判分窗口 306.57s"，于是速率被垃圾分母算歪）。
    #   切块界 = max(0.2s, 20×中位间隔)，取最长的一块当判分窗口。
    idx = np.where(anch)[0]
    gaps = np.diff(ts[idx]) if len(idx) > 1 else np.array([])
    if len(gaps):
        thr = max(0.2 * 24000.0, 20.0 * np.median(gaps))
        brk = np.where(gaps > thr)[0]
        blocks = np.split(idx, brk + 1)
        blk = max(blocks, key=len)
    else:
        blk = idx
    win = np.zeros(len(rows), bool); win[blk] = True
    i0, i1 = int(blk[0]), int(blk[-1])
    n_win = int(win.sum())
    # ★★ 时长不能用"末 ts − 首 ts" ★★
    #   实测踩到大坑：补偿 ON 那轮算出 0.18s，而固件的 cl_runs_stopped 说每轮都跑满了
    #   1500ms 到点停机 ⇒ 真相是 ts 里有错乱/跳跃的事件，把窗口切碎了。
    #   ⇒ 改成【对窗口内相邻事件的 Δts 求和】（只累加 ≤20×中位间隔的合理间隔），
    #     对孤立的 ts 跳变免疫。
    _pair = win[:-1] & win[1:]
    _d = np.diff(ts)[_pair]
    _ok = _d <= max(0.05 * 24000.0, 20.0 * (np.median(_d) if _d.size else 1.0))
    dur = float(_d[_ok].sum()) / 24000.0
    _lost = int((~_ok).sum())
    if _lost:
        print(f'   （时长按 Δts 累加：跳过 {_lost} 个 ts 跳跃的间隔）')
    if n_win < int(anch.sum()):
        print(f'   ⚠ 丢掉 {int(anch.sum())-n_win} 条 ts 错乱/陈旧的锚定事件（判分窗口取最长连续块）')
    # ★ 窗口太短就不许判：实测踩到 0.18s/293 条的窗口照样被打上"✅ 判据干净"，
    #   那是假阳性——0 次超时在小窗口里没有统计意义。至少 0.8s 且 600 条才判。
    if dur < 0.8 or n_win < 600:
        print(f'   ⚠⚠ 本轮窗口太短（{dur:.2f}s / {n_win} 条，健康时应 ≥1.3s/~1000 条）'
              f' ⇒ 闭环没真正跑起来（停滞或跑飞），本轮不作判分。')
        rate = n_win / max(dur, 1e-9)
        print(f'   （参考：窗口内换相速率仅 {rate:.0f} 次/秒；健康时 ~836/秒）')
        return None
    print(f'   事件 {len(rows)}（开环 {int((stg==1).sum())} / stage0 {int((stg==0).sum())}）')
    print(f'   判分窗口 = 锚定闭环：{n_win} 条事件 / {dur:.2f}s'
          f'（占"被驱动时间" {100.0*anch.sum()/max(1,drv.sum()):.0f}%）')
    print(f'   工况：duty 中位 {np.median(du[anch]):.0f}   电流中位 {np.median(cur[anch]):.0f}'
          f'（P90 {np.percentile(cur[anch],90):.0f}）')
    tw = tmo[win]
    n_tmo = int(tw.max() - tw.min())
    print(f'   ① ★漏过零（bemf_timeout 窗口内增量）= {n_tmo}'
          f'   ⇒ {n_tmo/max(dur,1e-9):.2f} 次/秒' + ('   ✅ 判据干净' if n_tmo == 0 else ''))
    if n_tmo:
        inc = np.where(np.diff(tw) > 0)[0] + 1
        s_inc = st[win][inc]
        print('      按事件 step 分布 ' + ' '.join(f'{s}:{int((s_inc==s).sum())}' for s in range(1, 7)))
    dts = np.diff(ts); inw = win[:-1] & win[1:]
    print('   ② 六步真实换相间隔（Δts 中位，拍）—— 只在锚定窗口内：')
    med = []
    for s in range(1, 7):
        mm = inw & (st[1:] == s)
        v = np.median(dts[mm]) * TICK if mm.sum() > 5 else float('nan')
        med.append(v)
        if mm.sum() > 5:
            print(f'        step{s}: n={int(mm.sum()):5d}  中位 {v:7.0f} 拍   '
                  f'P10 {np.percentile(dts[mm],10)*TICK:6.0f}  P90 {np.percentile(dts[mm],90)*TICK:7.0f}')
    ok = [v for v in med if v == v]
    if ok:
        print(f'      ⇒ 六步中位 {min(ok):.0f}~{max(ok):.0f} 拍，极差 '
              f'{100*(max(ok)-min(ok))/np.mean(ok):.1f}%  ★越小越均匀')
    print(f'   ③ zc_reject 末值 {rej[-1]}（判据拒收累计，与"超时"互补：拒收=来了不认）')
    return n_tmo


def main():
    print(f'===== 计数轮 {TAG}  （{SECS:.0f}s 采集，闭环额度 1500ms）=====')
    cooldown()
    # ★★ 跑前先验串口活着 ★★
    #   踩过（2026-10-14）：ESC 的发送侧会卡死在"待发送"（win_seq==win_sent 不再增长、
    #   tx_ready() 恒假），此时电机照转、但一个字节都收不到 ⇒ 会被误判成"电机没转/
    #   没交棒"（我上一条就这么误判了）。win_enable=1 时窗口流应持续 ~100KB/s。
    nb = link_bytes(3.0)
    if nb >= 0 and nb < 5000:
        print(f'   ⚠⚠ 串口卡死：3 秒只收到 {nb} 字节（正常应 ~300000）'
              f' ⇒ ESC 发送侧停摆，本轮放弃。先复位/重烧一次再跑。')
        return
    print(f'   串口存活检查：3 秒 {nb} 字节 ✓')
    # ★★ 跑前查母线电压 ★★（2026-10-14 踩到）
    #   母线没电时固件照样"跑斜坡"：duty 290、电流却只有 5 单位（≈0.08A）⇒ 无电流、
    #   无转矩、无反电势 ⇒ 永远认不到过零、永远不交棒，最后撞开环 8s 上限。
    #   我当时把它当成"OFF+6000 不交棒"的固件异常查了两轮 —— 先读电压就不会。
    bv = sh('tools/esc-dbg.sh', 'peek', 'battery_voltage')
    try:
        raw = int(bv.stdout.strip().rsplit('=', 1)[-1].split('(')[0].strip(), 16)
        volts = (raw & 0xFFFF) / 100.0
    except Exception:
        volts = float('nan')
    if not (volts == volts) or volts < 10.0:
        print(f'   ⚠⚠ 母线电压只有 {volts:.2f} V（应 ~12 V）⇒ 12V 没给上/掉了，本轮放弃。')
        return
    print(f'   母线电压 {volts:.2f} V ✓')
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
