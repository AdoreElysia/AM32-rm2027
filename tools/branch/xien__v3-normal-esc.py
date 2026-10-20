#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""xien/v3-normal-esc：串口常发 + 按油门分段（不占 J-Link）
固件在窗口包 dbg 里打包：bit7..1 = 油门/16，bit0 = 1 表示还在开环（未交棒）。
用法：python3 tools/branch/xien__v3-normal-esc.py [秒数，默认180]
采集期间给油门；检测到"油门出现后又归零"自动结束并打印分段表。
"""
import os, sys, time, json, subprocess, signal
REPO = os.environ.get('ESC_ROOT', os.getcwd())
RAW = '/tmp/esc_branch_raw.bin'
OUT = os.path.join(REPO, 'firmware')
os.makedirs(OUT, exist_ok=True)

def parse_table():
    import numpy as np
    p = RAW.replace('.bin', '.npz')
    if not os.path.exists(p):
        return None
    d = np.load(p)
    raw = d['dbg'].astype(int)
    thr = (raw >> 1) * 16
    hov = raw & 1
    s = d['step'].astype(int) & 7
    cur = d['cur'].astype(float)
    rows = []
    for v in sorted(set(int(x) for x in thr)):
        m = thr == v
        if m.sum() < 100:
            continue
        ss = s[m]; fwd = rev = 0
        for i in range(1, len(ss)):
            if ss[i] != ss[i - 1]:
                if (ss[i - 1] % 6) + 1 == ss[i]:
                    fwd += 1
                elif ss[i - 1] == (ss[i] % 6) + 1:
                    rev += 1
        dur = len(ss) / 24000.0
        rows.append(dict(thr=v, n=int(m.sum()),
                         lo=int(hov[m].sum() * 100 / int(m.sum())),
                         cur=int(np.median(cur[m])), fwd=fwd, rev=rev,
                         erpm=int((fwd / dur) * 60 / 6) if fwd else 0))
    return rows

def main():
    secs = float(sys.argv[1]) if len(sys.argv) > 1 else 180.0
    try:
        os.unlink(RAW)
    except OSError:
        pass
    env = dict(os.environ); env.setdefault('ESC_BAUD', '921600')   # serial-spool 默认 1.5M，固件是 921600
    sp = subprocess.Popen(['python3', 'tools/serial-spool.py', RAW], cwd=REPO, env=env,
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("最长采集 %.0fs；给油后自动结束" % secs, flush=True)
    t0 = time.time(); last = 0.0; seen = False
    try:
        while time.time() - t0 < secs:
            time.sleep(0.5)
            if time.time() - last < 2.0:
                continue
            last = time.time()
            if not os.path.exists(RAW):
                continue
            subprocess.run(['python3', 'tools/win-proto2.py', 'parse', RAW], cwd=REPO,
                           capture_output=True, text=True, timeout=25)
            p = RAW.replace('.bin', '.npz')
            if not os.path.exists(p):
                continue
            try:
                d = np.load(p)
            except Exception:
                continue
            thr = (d['dbg'].astype(int) >> 1) * 16
            if thr.max() > 0:
                seen = True
            if seen and thr[-200:].max() == 0:
                print("检测到油门出现并已收回 => 结束", flush=True)
                break
    except KeyboardInterrupt:
        pass
    sp.send_signal(signal.SIGINT); sp.wait(timeout=3)
    rows = parse_table()
    if not rows:
        print("无数据"); return
    print("\n油门(~) | 帧数 | 开环占比 | 中位电流 | 正向 | 反向 | 估计eRPM", flush=True)
    for r in rows:
        print("%6d | %5d | %3d%% | %6d | %5d | %4d | %6d" % (
            r['thr'], r['n'], r['lo'], r['cur'], r['fwd'], r['rev'], r['erpm']), flush=True)
    json.dump(rows, open(os.path.join(OUT, 'xien__v3-normal-esc.json'), 'w'),
              ensure_ascii=False, indent=1)
    print("已存 firmware/xien__v3-normal-esc.json", flush=True)

if __name__ == '__main__':
    main()
