#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""xien/v3-normal-esc：串口常发 + 按油门自动分段（不需要 J-Link，Ozone 可继续占用）
   固件已在窗口包 dbg 字节里放了「油门/8」⇒ 串口流自带油门信息。
   用法：python3 tools/branch/xien__v3-normal-esc.py [秒数]
   采集期间你手动给油门（200/400/600…），结束后自动按油门分段打印。
"""
import os, sys, time, json, subprocess, signal
REPO = os.environ.get('ESC_ROOT', os.getcwd())
RAW = '/tmp/esc_branch_raw.bin'
OUT = os.path.join(REPO, 'firmware'); os.makedirs(OUT, exist_ok=True)

def main():
    secs = float(sys.argv[1]) if len(sys.argv) > 1 else 20.0
    try: os.unlink(RAW)
    except OSError: pass
    env = dict(os.environ); env.setdefault('ESC_BAUD','921600')   # ★ serial-spool 默认 1.5M，固件是 921600
    sp = subprocess.Popen(['python3','tools/serial-spool.py',RAW], cwd=REPO, env=env,
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f"采集 {secs:.0f}s（串口 {os.environ.get('ESC_SERIAL','/dev/ttyUSB0')}）… 请手动给油门", flush=True)
    t0 = time.time()
    try:
        while time.time() - t0 < secs:
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    sp.send_signal(signal.SIGINT); sp.wait(timeout=3)
    sz = os.path.getsize(RAW) if os.path.exists(RAW) else 0
    print(f"采集结束：{sz} 字节", flush=True)
    if sz == 0: return
    r = subprocess.run(['python3','tools/win-proto2.py','parse',RAW], cwd=REPO,
                       capture_output=True, text=True)
    for l in (r.stdout or '').splitlines():
        if '有效' in l or 'npz' in l: print('  ' + l, flush=True)
    import numpy as np
    p = RAW.replace('.bin','.npz')
    if not os.path.exists(p):
        cand = [f for f in os.listdir('/tmp') if f.endswith('.npz')]
        print("  无 npz（解析失败）", flush=True); return
    d = np.load(p)
    thr = d['dbg'].astype(int) * 8 if 'dbg' in d.files else None
    if thr is None: print("  npz 里没有 dbg 字段", flush=True); return
    s = d['step'].astype(int) & 7
    cur = d['cur'].astype(float)
    print("\n油门(dbg×8) | 帧数 | 中位电流码 | 换相次数 | 方向(正/反)", flush=True)
    res = {}
    for v in sorted(set(int(x) for x in thr)):
        m = thr == v
        if m.sum() < 20: continue
        ss = s[m]
        tr = 0; rev = 0
        for i in range(1, len(ss)):
            if ss[i] != ss[i-1]:
                if (ss[i-1] % 6) + 1 == ss[i]: tr += 1
                elif ss[i-1] == (ss[i] % 6) + 1: rev += 1
        med = int(np.median(cur[m]))
        print(f"  {v:5d}     | {int(m.sum()):5d} | {med:8d} | {tr:6d} | {tr}/{rev}", flush=True)
        res[str(v)] = {'n': int(m.sum()), 'med_cur': med, 'fwd': tr, 'rev': rev}
    json.dump(res, open(os.path.join(OUT,'xien__v3-normal-esc.json'),'w'), ensure_ascii=False, indent=1)
    print("已存 firmware/xien__v3-normal-esc.json", flush=True)

if __name__ == '__main__':
    main()
