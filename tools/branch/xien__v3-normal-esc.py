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
    try: os.unlink(RAW)
    except OSError: pass
    env = dict(os.environ); env.setdefault('ESC_BAUD', '921600')
    sp = subprocess.Popen(['python3','tools/serial-spool.py',RAW], cwd=REPO, env=env,
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("最长采集 %.0fs；\u6cb9\u95e8\u4e0a\u5347\u6cbf\u5f00\u59cb\u4fdd\u5b58\u3001\u4e0b\u964d\u6cbf\u540e 0.5s \u505c\u6b62" % secs, flush=True)
    MAGIC = b'\xa5\x5a\xa5\x5a\xc3\x3c\x96\x69'
    files = []
    buf = b''; pos = 0; active = False; last_nz = 0.0
    def flush_keep(bs, from_off):
        out = b''.join(bs)
        fn = '/tmp/edge_%d.bin' % (len(files) if False else 0)
        return out
    kept = []
    KEEP = '/tmp/edge_keep.bin'
    kf = open(KEEP, 'wb')
    def flush_keep():
        kf.seek(0); kf.write(b''.join(kept)); kf.truncate(); kf.flush()
    t0 = time.time()
    try:
        while time.time() - t0 < secs:
            time.sleep(0.05)
            if not os.path.exists(RAW): continue
            with open(RAW, 'rb') as f:
                f.seek(0, 2); size = f.tell()
                if size <= pos: continue
                f.seek(pos); buf += f.read(size - pos); pos = size
            while True:
                k = buf.find(MAGIC)
                if k < 0: break
                if k + 22 > len(buf): break
                ln = int.from_bytes(buf[k+10:k+12], 'little')
                total = 20 + ln + 2
                if k + total > len(buf): break
                pkt = buf[k:k+total]
                dbg = pkt[20 + 11] if ln >= 12 else 0
                thr = (dbg >> 1) * 16
                now = time.time()
                if thr > 0:
                    active = True; last_nz = now
                if active:
                    kept.append(pkt)
                    if len(kept) % 50 == 0:
                        flush_keep()
                if active and thr == 0 and (now - last_nz) > 0.5:
                    active = False
                buf = buf[k+total:]
            if active and len(kept) > 0 and (time.time() - last_nz) > 0.5:
                active = False
    except KeyboardInterrupt:
        pass
    flush_keep(); kf.close()
    sp.send_signal(signal.SIGINT); sp.wait(timeout=3)
    if not kept:
        print("没有捕获到油门段"); return
    eb = '/tmp/edge_keep.bin'
    open(eb, 'wb').write(b''.join(kept))
    print("边沿截取：%d 个包，%d 字节 => %s" % (len(kept), os.path.getsize(eb), eb), flush=True)
    subprocess.run(['python3','tools/win-proto2.py','parse',eb], cwd=REPO, capture_output=True, text=True)
    npz = eb.replace('.bin','.npz')
    if not os.path.exists(npz): print("解析失败"); return
    import numpy as np
    d = np.load(npz)
    raw = d['dbg'].astype(int); thr = (raw >> 1) * 16; ol = raw & 1
    st = d['step'].astype(int); s = st & 7
    print("\n油门 | 帧数 | duty | zero_crosses | e_rpm | 电流 | 闭环% | 换相")
    for v in sorted(set(int(x) for x in thr)):
        m = thr == v
        if m.sum() < 80: continue
        dd = d['duty'][m].astype(float); dd = dd[dd > 0]
        zz = d['zc'][m].astype(float); zz = zz[zz > 0]
        ee = d['erpm'][m].astype(float); ee = ee[ee > 0]
        cc = d['cur'][m].astype(float); cc = cc[cc > 0]
        ss = s[m]; tr = 0
        for i2 in range(1, len(ss)):
            if ss[i2] != ss[i2-1]: tr += 1
        print("%5d | %5d | %5.0f | %6.0f | %5.0f | %4.0f | %3d%% | %4d" % (
            v, int(m.sum()), np.median(dd) if len(dd) else 0, np.median(zz) if len(zz) else 0,
            np.median(ee) if len(ee) else 0, np.median(cc) if len(cc) else 0,
            100-int(ol[m].sum()*100/int(m.sum())), tr))


if __name__ == "__main__":
    main()
