#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
run-check.py —— 一轮实验的【唯一入口】+【强制冷却】+【一屏判定】。

用法:
  python3 tools/run-check.py            # 跑一轮（8s），跑完 dump + 分析 + 判定
  python3 tools/run-check.py 6          # 指定采集秒数
  python3 tools/run-check.py --no-run   # 不跑电机，只对最近一次数据出判定（冷却期用）
  python3 tools/run-check.py --since    # 只报"距上次跑结束过了多少秒"

为什么要它：
  · 用户要求"每轮测试中间至少间隔一分钟、并且这一分钟要用来分析数据"。
    ⇒ 本脚本把"上一轮结束时刻"写进 /tmp/esc_last_run，**不满 60s 直接拒绝跑**，
      逼着这一分钟用在分析上（而不是顺手再来一轮）。
  · 判定标准写死在这里，避免"数字好看就算成功"。

判定 4+2 项（口径见《闭环补偿方案演变记录.md》2c 节：本文件所有"转速"= 换相节拍）：
  1) 交棒发生了（stage 1→0）                     —— 没有 ⇒ 先查 12V / 验证窗口
  2) lock_trips == 0                              —— 锁定判据没触发
  3) A×ci / 基准 ≥ 0.5（基准=最近16步中位）        —— 没滑差
  4) ci 与"波形量出的六步"一致（偏差 <25%）        —— 固件的时间与硬件一致
  +) 六步散布（与基线 2~4% 比）
  +) 幅值中位（当转子转速代理，与基线 ~158 码 比）
"""
import sys, os, csv, subprocess, time, struct, re, glob
import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = __import__('os').environ.get('RM2027_DATA', __import__('os').path.expanduser('~/桌面/RM2027'))
STAMP = '/tmp/esc_last_run'
GAP_S = 60.0

def last_run():
    try:
        return float(open(STAMP).read().strip())
    except Exception:
        return 0.0

def since():
    return time.time() - last_run()

def die(msg):
    print('❌ ' + msg); sys.exit(1)

# ---------------------------------------------------------------- 冷却闸
if '--since' in sys.argv:
    print('距上次跑结束：%.0f s（阈值 %.0f s）' % (since(), GAP_S)); sys.exit(0)

RUN = '--no-run' not in sys.argv
if RUN and since() < GAP_S:
    die('冷却不足：距上次跑结束只过了 %.0f s（要求 ≥%.0f s）。'
        '请先用 --no-run 分析上一轮数据，等满 %.0f s 再跑。'
        % (since(), GAP_S, GAP_S - since()))

SECS = 8
for a in sys.argv[1:]:
    if a.isdigit():
        SECS = int(a)

# ---------------------------------------------------------------- 分析（无电机也能跑）
def elf_sym():
    sym = {}
    out = subprocess.run(['/usr/bin/arm-none-eabi-nm', '-S', 'obj/AM32_RM2027_ESC_G071_2.21.elf'],
                         capture_output=True, text=True, cwd=REPO).stdout
    for l in out.splitlines():
        f = l.split()
        if len(f) == 4:
            sym[f[3]] = int(f[0], 16)
    return sym

def rd32(addr, n):
    o = subprocess.run(['openocd', '-f', 'interface/stlink.cfg', '-f', 'target/stm32g0x.cfg',
        '-c', 'adapter speed 1800', '-c', 'init', '-c', 'halt', '-c', 'mww 0x40003000 0xAAAA',
        '-c', 'mdw %#x %d' % (addr, n), '-c', 'mww 0x40003000 0xAAAA', '-c', 'resume',
        '-c', 'resume', '-c', 'shutdown'], capture_output=True, text=True)
    v = []
    for l in (o.stdout + o.stderr).splitlines():
        m = re.match(r'^0x[0-9a-fA-F]+:\s*(.*)$', l)
        if m:
            v += [int(x, 16) for x in m.group(1).split()]
    return b''.join(x.to_bytes(4, 'little') for x in v)

def dump_state():
    sym = elf_sym(); d = {}
    for nm, fmt in (('lock_flux', '<I'), ('lock_flux_ref', '<I'), ('lock_trips', '<H'),
                    ('lock_last_amp', '<h'), ('ci_clamp_rejects', '<H'), ('ol_stop_reason', '<B')):
        if nm in sym:
            try:
                d[nm] = struct.unpack_from(fmt, rd32(sym[nm], 1), 0)[0]
            except Exception:
                pass
    return d

FLOAT = {1: 'C', 2: 'A', 3: 'B', 4: 'C', 5: 'A', 6: 'B'}
def segs(st):
    out, i0 = [], 0
    for k in range(1, len(st)):
        if st[k] != st[k-1]:
            out.append((i0, k, int(st[i0]))); i0 = k
    out.append((i0, len(st), int(st[i0]))); return out

def analyze(csvp, npzp):
    rows = list(csv.DictReader(open(csvp)))
    ts = np.array([int(r['ts_frame']) for r in rows]) / 24000.0
    st = np.array([int(r['stage']) for r in rows]); ci = np.array([int(r['ci']) for r in rows])
    cur = np.array([int(r['cur']) for r in rows]); tmo = np.array([int(r['bemf_timeout']) for r in rows])
    ds = np.array([int(r['duty_setpoint']) for r in rows])
    chg = np.where(np.diff(st) != 0)[0]
    A = {}
    A['transitions'] = [(round(ts[k], 2), int(st[k]), int(st[k+1])) for k in chg]
    handoff = None
    for k in chg:
        if st[k] == 1 and st[k+1] == 0:
            handoff = ts[k+1]
    A['handoff'] = handoff
    cl = np.where(st == 0)[0]
    if handoff is not None:
        cl = np.array([k for k in cl if ts[k] >= handoff])
    A['n_cl'] = len(cl)
    A['cl_dur'] = (ts[cl[-1]] - ts[cl[0]]) if len(cl) > 1 else 0.0
    A['ci'] = float(np.median(ci[cl])) if len(cl) else float('nan')
    A['rpm_comm'] = 60 / (A['ci'] / 2000 * 0.006) / 6 if A['ci'] == A['ci'] else float('nan')
    A['timeouts'] = int(tmo[-1]); A['cur'] = float(np.median(cur[cl])) if len(cl) else float('nan')
    A['duty'] = sorted(set(ds[cl].tolist()))[:5] if len(cl) else []
    # 波形
    z = np.load(npzp); WST = z['step']; WTS = z['ts'] / 24000.0
    V0 = z['v0']; PH = {'A': z['va'], 'B': z['vb'], 'C': z['vc']}
    hw = handoff if handoff is not None else 1e9
    wid = {s: [] for s in range(1, 7)}; amp = {s: [] for s in range(1, 7)}
    for wi in range(len(WST)):
        if WTS[wi] < hw:
            continue
        for i0, i1, s in segs(WST[wi]):
            if s not in FLOAT or i0 == 0 or i1 == 512 or i1 - i0 < 8:
                continue
            wid[s].append((i1 - i0) * 83.333)          # 拍（1 采样 = 83.3 拍）
            d = np.sort((PH[FLOAT[s]][wi][i0:i1].astype(float) - V0[wi][i0:i1])[3:-3])
            amp[s].append(d[-2] - d[1])
    wmed = [np.median(wid[s]) if wid[s] else np.nan for s in range(1, 7)]
    amed = [np.median(amp[s]) if amp[s] else np.nan for s in range(1, 7)]
    A['w'] = wmed
    A['w_med'] = float(np.nanmedian(wmed)) if np.any(np.isfinite(wmed)) else float('nan')
    A['spread'] = float(np.nanstd(wmed) / np.nanmean(wmed) * 100) if np.any(np.isfinite(wmed)) else float('nan')
    A['A'] = float(np.nanmedian(amed)) if np.any(np.isfinite(amed)) else float('nan')
    A['miss5'] = len(wid[5])
    return A

def verdict(A, S):
    print('\n================ 一屏判定 ================')
    ok = True
    # 1 交棒
    if A['handoff'] is None:
        print('1) 交棒：❌ 没发生 ⇒ 先查 12V 母线 / 验证窗口（三相全平、电流≈4 就是没电）'); ok = False
    else:
        print('1) 交棒：✅ %.2f s，闭环 %.2f s' % (A['handoff'], A['cl_dur']))
    # 2 锁
    lt = S.get('lock_trips', None)
    if lt is None:
        print('2) lock_trips：? （没读到）')
    elif lt == 0:
        print('2) lock_trips：✅ 0')
    else:
        print('2) lock_trips：❌ %d（失锁被停机，看 A×ci 与幅值）' % lt); ok = False
    # 3 A×ci
    fl, ref = S.get('lock_flux'), S.get('lock_flux_ref')
    if fl and ref:
        r = fl / ref
        print('3) A×ci / 基准：%s %.2f  (flux %d / ref %d)' % ('✅' if r >= 0.5 else '❌', r, fl, ref))
        if r < 0.5: ok = False
    # 4 ci vs 波形
    if A['ci'] == A['ci'] and A['w_med'] == A['w_med']:
        dev = abs(A['w_med'] - A['ci']) / A['ci'] * 100
        print('4) ci %d 拍 vs 波形六步中位 %d 拍：偏差 %.0f%% %s'
              % (A['ci'], A['w_med'], dev, '✅' if dev < 25 else '❌'))
        if dev >= 25: ok = False
    if A['spread'] == A['spread']:
        print('5) 六步散布 %.0f%%（基线 2~4%%）：%s' % (A['spread'], '✅' if A['spread'] <= 8 else '❌'))
        if A['spread'] > 8: ok = False
    print('+) 幅值中位 %.0f 码（基线 ~158，转子转速代理）  漏认(step5 段数) %d'
          % (A['A'], A['miss5']))
    print('+) 换相节拍 %.0f 转/分   超时 %d   电流 %.0f   duty %s'
          % (A['rpm_comm'], A['timeouts'], A['cur'], A['duty']))
    print('   六步(拍)：%s' % '/'.join('%.0f' % x for x in A['w']))
    print('   转子转速代理（A∝ω，基线 158 码 ⇒ ~1111 转/分）：%.0f 转/分'
          % (1111 * A['A'] / 158 if A['A'] == A['A'] else float('nan')))
    print('==========================================')
    print('结论：' + ('✅ 通过（可用作对照/继续）' if ok else '❌ 未通过（详见上面标 ❌ 的项）'))
    return ok

# ---------------------------------------------------------------- 跑一轮
if RUN:
    print('▶ 跑一轮：%d s（含收油/给油）…' % SECS)
    r = subprocess.run(['python3', 'tools/cap-evt.py', str(SECS)], cwd=REPO,
                       capture_output=True, text=True)
    tail = [l for l in r.stdout.splitlines() if ('CSV →' in l or '窗口载荷' in l)]
    for l in tail: print('  ' + l)
    open(STAMP, 'w').write(str(time.time()))
else:
    print('（--no-run：只分析，不跑电机）')

csvs = sorted(glob.glob(DATA + '/evt_run_*.csv'), key=os.path.getmtime)
npzs = sorted(glob.glob(DATA + '/evt_run_*_win.npz'), key=os.path.getmtime)
if not csvs:
    die('没找到数据文件')
csvp = csvs[-1]
npzp = npzs[-1]
print('\n▶ 分析：%s' % os.path.basename(csvp))
S = dump_state()
verdict(analyze(csvp, npzp), S)
print('\n（距下次可跑还剩 %.0f s；这段时间用来写结论/编译下一版）'
      % max(0, GAP_S - since()))
