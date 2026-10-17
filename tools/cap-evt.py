#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
cap-evt.py —— 采串口换相事件流 → 存 CSV + 出四栏图。★ 数据一定落盘。

用法:
  python3 tools/cap-evt.py [秒数=7] [输出目录]      # 给油→采→收油→存盘+出图
  python3 tools/cap-evt.py --plot=<已存的CSV>       # 只对已存 CSV 重新出图（不驱动电机）

产出:
  <目录>/evt_run_YYYYmmdd_HHMMSS.csv
  <目录>/evt_run_YYYYmmdd_HHMMSS.png   四栏：换相间隔 / 转速 / 占空比+电流 / 阶段

★ 两个必须记住的坑（都踩过）:
  ① 窗口包(1548B)的头部也是 A5 5A A5 5A，偶尔(CRC8 只有 1/256)会蒙过校验、被当成
     21 字节事件帧 ⇒ 之后整段解析全歪(ts 出现几万帧的跳跃、step/cur 出现不可能的值)。
     ⇒ 必须按 evt-plot.py 的老判据过滤: step 1~6 / duty 0~2000 / |delta|<3000 / cur 0~400。
  ② 电机电源(12V)必须开着，否则电调不换相、一个字节都收不到。
"""
import sys, os, time, struct, subprocess, re, csv, datetime, hashlib
import numpy as np, serial
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
for _f in ('Noto Sans CJK SC', 'Noto Sans CJK JP'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family'] = _f; break
plt.rcParams['axes.unicode_minus'] = False

# ★ xien/n2：路径/串口/波特率全部走环境变量（见 tools/_env.sh），不再硬编码用户名
PORT = __import__('os').environ.get('ESC_SERIAL', '/dev/ttyUSB0')
BAUD = int(__import__('os').environ.get('ESC_BAUD', '1500000'))     # 要跟固件 ADC_CAP_BAUD 一致
OUTD_DEF = __import__('os').environ.get('RM2027_DATA', __import__('os').path.expanduser('~/桌面/RM2027'))
# ADC 触发是 24kHz，但 DMA-TC 重武装漏触发 ⇒ 实际有效帧率约 17~20kHz，且随 ISR 负载变。
# 绝对时间轴只能用“本轮有效帧周期”近似；相位/ci 比值不受影响。
FRAME_US = float(__import__('os').environ.get('ESC_FRAME_US', '50.0'))  # n1 默认 50µs/帧
PLOT_ONLY = None
RAW = None
if len(sys.argv) > 1 and sys.argv[1].startswith('--raw'):
    RAW = sys.argv[1].split('=', 1)[-1]
    SECS, OUTD = 0.0, OUTD_DEF
elif len(sys.argv) > 1 and sys.argv[1].startswith('--plot'):
    PLOT_ONLY = sys.argv[1].split('=', 1)[-1]
    SECS, OUTD = 0.0, OUTD_DEF
else:
    SECS = float(sys.argv[1]) if len(sys.argv) > 1 else 7.0
    OUTD = sys.argv[2] if len(sys.argv) > 2 else OUTD_DEF


def valid(r):
    """剔除"窗口包被误读成事件帧"的假帧。

    ★ 2026-10-13 扩充：原来只查 step/duty/cur/delta，于是**窗口包魔数 A5 5A 被当成 input**
      这类假帧照样进来（实测抓到 input=23205=0x5AA5、input=56666 —— 正好是窗口包头的前几个
      字节），而它会把 `bemf_timeout`/`duty_setpoint` 等字段一起带歪 ⇒ 让人把"解析伪影"
      误读成"电调复位/油门跳变"。踩过三次，所以把后段字段也一并验：
        input ≤ 2047、advance ≤ 600、average_interval 合理、duty_setpoint ≤ 3600。
      （`p[41]` 已改记 desync，不再是 timeout 的高字节。）"""
    _seq, _ts, dl, st, _w, du, cur = r[:7]
    if not ((1 <= st <= 6) and (0 <= du <= 2000) and (abs(dl) < 3000) and (0 <= cur < 400)):
        return False
    iov, smax, uca, cci, tzc, lzc, avgi, adv, dsp, dmax, tmo, rej, inp, spa = r[8:22] if len(r) >= 22 else (0,) * 14
    if not (0 <= inp <= 2047):
        return False
    if not (0 <= adv <= 600):
        return False
    if not (0 <= dsp <= 3600):
        return False
    if not (500 <= avgi <= 60000):
        return False
    return True


def crc8(b):
    c = 0
    for x in b:
        c ^= x
        for _ in range(8):
            c = ((c << 1) ^ 0x07) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c


def oc(args, iface='stlink.cfg', tgt='stm32g0x.cfg', sp=1800):
    return subprocess.run(['openocd', '-f', f'interface/{iface}', '-f', f'target/{tgt}',
        '-c', f'adapter speed {sp}', '-c', 'init', '-c', 'halt', '-c', 'mww 0x40003000 0xAAAA',
        *args, '-c', 'mww 0x40003000 0xAAAA', '-c', 'resume', '-c', 'shutdown'],
        capture_output=True, text=True)


def f1(v):
    subprocess.run(['openocd', '-f', 'interface/jlink.cfg', '-c', 'transport select swd',
        '-f', 'target/stm32f1x.cfg', '-c', 'adapter speed 500', '-c', 'init', '-c', 'halt',
        '-c', f'mwb 0x20000274 {v}', '-c', 'resume', '-c', 'shutdown'],
        capture_output=True, timeout=30)


def rd32(a):
    o = oc(['-c', f'mdw {a:#x} 1'])
    m = re.search(r'^(0x[0-9a-fA-F]+): ([0-9a-fA-F]+)', o.stdout + o.stderr, re.M)
    return int(m.group(2), 16) if m else None


def resets(v):
    if v is None:
        return '?'
    nm = [(26, 'PIN'), (27, 'POR'), (28, 'SFT'), (29, 'IWDG'), (30, 'WWDG'), (31, 'LPWR')]
    on = [n for b, n in nm if v >> b & 1]
    return f'0x{v:08X} ({",".join(on) if on else "无"})'


def parse(buf):
    """返回 (有效事件, 窗口列表, 剔除的假帧数)。窗口列表元素 = (win_seq, win_ts, delta[], step[])"""
    rows, wins, i = [], [], 0
    while i < len(buf) - 49:            # ★ 事件帧 37 -> 49（再加 6 个"影响输出"的量）
        j = buf.find(b'\xA5\x5A\xA5\x5A', i)
        if j < 0:
            break
        if j + 49 > len(buf):
            break
        if crc8(buf[j:j + 48]) == buf[j + 48]:
            s7 = struct.unpack_from('<HIhBHHh', buf, j + 4)
            iov, smax = struct.unpack_from('<HH', buf, j + 19)
            uca, cci = struct.unpack_from('<hH', buf, j + 23)
            tzc, lzc, avgi, adv = struct.unpack_from('<HHHH', buf, j + 28)
            dsp, dmax = struct.unpack_from('<HH', buf, j + 36)
            tmo, rej = struct.unpack_from('<HH', buf, j + 40)
            tmo = tmo & 0xFF              # ★ j+40 只记 8 位 timeout
            desync = buf[j + 41]         # ★ j+41 = desync_happened 低 8 位（原保留字节）
            inp, spa = struct.unpack_from('<HH', buf, j + 44)
            rows.append(s7 + (buf[j + 27] & 1, iov, smax, uca, cci, tzc, lzc, avgi, adv,
                              dsp, dmax, tmo, rej, inp, spa, desync))
            i = j + 49
        elif (j + 12 + 512 * 12 <= len(buf)
              and struct.unpack_from('<H', buf, j + 10)[0] == 512):
            # ★ 2026-10-12 四路电压+电流版：每帧 12 字节 = VA VB VC V0 电流(各 i16) + step + 保留
            wseq, wts = struct.unpack_from('<HI', buf, j + 4)
            pay = np.frombuffer(buf[j + 12:j + 12 + 512 * 12], dtype=np.uint8).reshape(-1, 12)
            def _i16(c):
                return pay[:, c].astype(np.int16) | (pay[:, c + 1].astype(np.int16) << 8)
            wins.append((wseq, wts, _i16(0), _i16(2), _i16(4), _i16(6), _i16(8), pay[:, 10].copy(),
                         pay[:, 11].copy()))   # ★ 第 12 字节 = 未钳制的步内时间(ADC 帧, 饱和 255)
            i = j + 12 + 512 * 12
        else:
            i = j + 1
    n_raw = len(rows)
    rows = [r for r in rows if valid(r)]
    return rows, wins, n_raw - len(rows)


def make_plot(rows, secs, pngp):
    seq = np.array([r[0] for r in rows]); ts = np.array([r[1] for r in rows])
    stg = np.array([r[7] for r in rows])
    dut = np.array([r[5] for r in rows], float); cur = np.array([r[6] for r in rows], float)
    # ★ x 轴用【事件序号】而不是 ts —— 固件的 ts(adc_frame_total) 会出现几万帧的跳跃，
    #   拿它当时间轴会把图完全画歪（实测 x 轴跑到 800000"秒"）。序号轴永远可读。
    t_all = np.arange(len(rows), dtype=float)
    ts0 = ts[0]
    m = (np.diff(seq) == 1) & (np.diff(ts) > 0) & (np.diff(ts) < 24000)
    iv = np.diff(ts)[m] * (FRAME_US / 1000.0)   # 帧→ms；默认 50µs/帧（n1 有效 20kHz）
    tt = np.arange(1, len(rows), dtype=float)[m]
    ol = int(np.sum(stg == 1))
    if iv.size:
        print(f'开环 {ol} / 闭环 {len(rows)-ol}')
        srt = np.sort(iv)
        print(f'换相间隔 中位 {np.median(iv):.3f}ms  P10 {srt[int(len(srt)*.1)]:.3f}  '
              f'P90 {srt[int(len(srt)*.9)]:.3f}  最大 {srt[-1]:.3f}')
        print(f'⇒ 转速(中位) {60/(np.median(iv)*6/1000)/6:.0f} 转/分')
        longn = int(np.sum(iv > 1.2 * np.median(iv)))
        print(f'长步(>1.2x中位) {longn}/{iv.size} = {100*longn/iv.size:.1f}%')
    fig, ax = plt.subplots(4, 1, figsize=(16, 12), sharex=True,
                           gridspec_kw={'height_ratios': [1.3, 1, 1, 0.8]})
    if iv.size:
        ax[0].plot(tt, iv, lw=.7, color='#c0392b')
        ax[0].axhline(1.0, color='gray', ls='--', lw=1, label='1.000ms(开环同速)')
        ax[0].axhline(float(np.median(iv)), color='#8e44ad', ls=':', lw=1,
                      label=f'中位 {np.median(iv):.3f}ms')
    ax[0].set_ylabel('换相间隔 (ms)'); ax[0].legend(fontsize=9)
    ax[0].set_title(f'事件流 {len(rows)} 个 / {secs:.0f}s（开环 {ol}，闭环 {len(rows)-ol}）')
    if iv.size:
        ax[1].plot(tt, [60/(v*6/1000)/6 for v in iv], lw=.7, color='#2980b9')
    ax[1].axhline(1670, color='#27ae60', ls='--', lw=1, label='开环 1670 转/分')
    ax[1].set_ylabel('转速 (转/分)'); ax[1].legend(fontsize=9)
    ax[2].plot(t_all, dut * 100 / 2000.0, lw=.7, color='#16a085', label='占空比(%)')
    ax[2].axhline(20.1, color='#c0392b', ls=':', label='上限 20%')
    ax[2].set_ylabel('占空比 (%)'); ax[2].legend(fontsize=9, loc='upper left')
    a2 = ax[2].twinx(); a2.plot(t_all, cur, lw=.6, color='#e67e22', alpha=.8, label='电流')
    a2.set_ylabel('电流', color='#e67e22'); a2.legend(fontsize=9, loc='upper right')
    ax[3].step(t_all, stg, where='post', lw=.8, color='#8e44ad')
    ax[3].set_ylim(-.2, 1.2); ax[3].set_yticks([0, 1]); ax[3].set_yticklabels(['闭环', '开环'])
    ax[3].set_ylabel('阶段'); ax[3].set_xlabel('第 n 次换相（事件序号；ts 不可信，见脚本头部说明）')
    for A in ax:
        A.grid(alpha=.25)
    plt.tight_layout(); plt.savefig(pngp, dpi=130)
    print(f'图  → {pngp}')


if PLOT_ONLY:
    with open(PLOT_ONLY) as f:
        rr = csv.reader(f); next(rr)
        all_rows = [tuple(int(x) for x in row) for row in rr]
    rows = [r for r in all_rows if valid(r)]
    print(f'（只出图）读入 {len(all_rows)} → 有效 {len(rows)}（剔除假帧 {len(all_rows)-len(rows)}）')
    make_plot(rows, 0, PLOT_ONLY.rsplit('.', 1)[0] + '.png')
    sys.exit(0)

# ---------------- 输出/落盘：原始字节 -> CSV + NPZ + 图 + 换相埋点 ----------------
def _repo_dir():
    r = os.environ.get('RM2027_REPO', '')
    return r if r and os.path.isdir(r) else os.getcwd()


def _git(args):
    try:
        o = subprocess.run(['git', '-C', _repo_dir()] + list(args),
                           capture_output=True, text=True, timeout=5)
        return o.stdout.strip()
    except Exception:
        return ''


def _branch_name():
    b = _git(['rev-parse', '--abbrev-ref', 'HEAD'])
    return '' if b in ('', 'HEAD') else b


def _safe_name(s):
    s = (s or '').replace('/', '__').replace('\\', '__')
    s = re.sub(r'[^\w.\-]+', '_', s, flags=re.UNICODE)
    return s.strip('_')


def _bin_info():
    repo = _repo_dir()
    p = os.path.join(repo, 'obj', 'AM32_RM2027_ESC_G071_2.21.bin')
    if not os.path.isfile(p):
        return '', ''
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return os.path.basename(p), h.hexdigest()


def _write_meta(csvp, buf, rows, wins, nfake, secs):
    branch = _branch_name()
    binname, binsha = _bin_info()
    duty = [r[5] for r in rows] if rows else []
    ci = [r[11] for r in rows if r[11] > 0] if rows else []
    meta = {}
    meta['file'] = os.path.basename(csvp)
    meta['captured_at'] = datetime.datetime.now().isoformat(timespec='seconds')
    meta['branch'] = branch
    meta['commit'] = _git(['rev-parse', 'HEAD'])
    meta['binary'] = binname
    meta['binary_sha256'] = binsha
    meta['serial'] = f'{PORT}@{BAUD}'
    meta['secs'] = f'{secs:.3f}'
    meta['raw_bytes'] = str(len(buf))
    meta['window_packets'] = str(len(wins))
    meta['event_rows'] = str(len(rows))
    meta['fake_frames'] = str(nfake)
    meta['ESC_DUTY'] = os.environ.get('ESC_DUTY', '')
    meta['ESC_STEP_DIV'] = os.environ.get('ESC_STEP_DIV', '')
    meta['ESC_TAG'] = os.environ.get('ESC_TAG', '')
    meta['ESC_FRAME_US'] = f'{FRAME_US:.2f}'
    if duty:
        meta['duty_median'] = f'{float(np.median(duty)):.1f}'
    if ci:
        meta['ci_median_ticks'] = f'{float(np.median(ci)):.1f}'
        meta['f_comm_hz'] = f'{2e6/float(np.median(ci)):.1f}'
        meta['rpm_median'] = f'{3.3333e6/float(np.median(ci)):.0f}'
    metap = csvp.rsplit('.', 1)[0] + '_meta.txt'
    with open(metap, 'w', encoding='utf-8') as f:
        f.write(f"# 采集轮档案  {os.path.basename(csvp)}\n")
        for k, v in meta.items():
            f.write(f"{k}={v}\n")
    return metap

def save_run(buf, secs=0.0, outd=OUTD_DEF):
    """把一段原始串口字节解析并落盘。raw 模式与在线采集共用。"""
    rows, wins, nfake = parse(buf)
    stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    _br = _safe_name(_branch_name())
    _stem = f'evt_run_{_br}_{stamp}' if _br else f'evt_run_{stamp}'
    csvp = os.path.join(outd, _stem + '.csv')
    with open(csvp, 'w', newline='') as f:
        w = csv.writer(f); w.writerow(['seq','ts_frame','delta','step','wait','duty','cur','stage','iov','startup_max','curlim_adj','ci','thiszctime','lastzctime','average_interval','advance','duty_setpoint','duty_max','bemf_timeout','zc_reject','input','stall_adj','desync'])
        w.writerows(rows)
    metap = _write_meta(csvp, buf, rows, wins, nfake, secs)
    rate = f'{len(buf)/secs/1000:.1f} KB/s' if secs else '—'
    print(f'\n原始字节 {len(buf)} ({rate})   窗口包 {len(wins)}   事件帧 有效 {len(rows)}（剔除假帧 {nfake}）')
    print(f'CSV → {csvp}  ({os.path.getsize(csvp)} 字节)')
    print(f'meta → {metap}')
    if wins:
        npzp = csvp.replace('.csv', '_win.npz')
        np.savez(npzp, seq=np.array([w[0] for w in wins]),
                 ts=np.array([w[1] for w in wins]),
                 va=np.array([w[2] for w in wins]), vb=np.array([w[3] for w in wins]),
                 vc=np.array([w[4] for w in wins]), v0=np.array([w[5] for w in wins]),
                 cur=np.array([w[6] for w in wins]), step=np.array([w[7] for w in wins]),
                 dbg=np.array([w[8] for w in wins]))   # ★ dbg = 未钳制的步内时间(ADC 帧, 饱和 255)
        print(f'  （四路电压+电流版：VA/VB/VC/V0/电流 各 512 点）')
        print(f'窗口载荷 → {npzp}  ({os.path.getsize(npzp)} 字节；{len(wins)} 片 x 512 点 @20kHz)')
    if not rows:
        if wins:
            print('⚠ 事件流为空（当前可能 evt_enable=0），但收到了窗口包 → 按“仅窗口流”成功处理')
            return
        print('❌ 既没有事件也没有窗口 —— 检查 12V(母线电压) / F1 油门 / 串口'); sys.exit(1)
    make_plot(rows, secs, csvp.rsplit('.', 1)[0] + '.png')

    # ---- ★ 跑完立刻 dump【换相埋点 OL_CM_*】（RAM 易失，必须趁有电）----
    #   代码自己的注释：它"直接回答换相落在反电势周期的哪个位置"——闭环相位的唯一直接证据。
    try:
        import subprocess as _sp
        _nm = _sp.run(['/usr/bin/arm-none-eabi-nm','-S','obj/AM32_RM2027_ESC_G071_2.21.elf'],
                      capture_output=True,text=True).stdout
        _sym = {}
        for _l in _nm.splitlines():
            _f=_l.split()
            if len(_f)==4: _sym[_f[3]]=int(_f[0],16)
        def _rd(name, words):
            _o=_sp.run(['openocd','-f','interface/stlink.cfg','-f','target/stm32g0x.cfg',
                '-c','adapter speed 1800','-c','init','-c','halt','-c','mww 0x40003000 0xAAAA',
                '-c',f'mdw {_sym[name]:#x} {words}','-c','mww 0x40003000 0xAAAA','-c','resume','-c','shutdown'],
                capture_output=True,text=True)
            # ★ openocd 把 `mdw` 的数据行写在 **stderr** 上（stdout 是空的）⇒ 必须两边都扫。
            #   只扫 stdout 会得到空列表，接着就是 "list index out of range"（2026-10-13 踩到）。
            _v=[]
            for _l in (_o.stdout + _o.stderr).splitlines():
                _m=re.match(r'^0x[0-9a-fA-F]+:\s*(.*)$',_l)
                if _m: _v += [int(x,16) for x in _m.group(1).split()]
            return _v
        # ★ 字宽必须按声明来（2026-10-13 修）：mdw 一次读的是【32 位字】，
        #   OL_CM_D 是 int16[32] ⇒ 只占 64 字节 = 16 个字，每个字里塞着 2 个 int16；
        #   OL_CM_S 是 uint8[32] ⇒ 8 个字，每字 4 个 uint8。
        #   原来 `_rd('OL_CM_D',32)` 读了 32 个字（= 把隔壁 OL_CM_T 也读进来当 D），
        #   又不拆半字 ⇒ 表里印出一堆 16 位巨数（实测 1258289370058217 之类）。
        _D=_rd('OL_CM_D',16); _T=_rd('OL_CM_T',16); _W=_rd('OL_CM_W',16); _S=_rd('OL_CM_S',8)
        _n=_rd('ol_cm_n',1); _i=_rd('ol_cm_idx',1)
        def _u16(ws):
            out=[]
            for x in ws: out += [x & 0xFFFF, (x >> 16) & 0xFFFF]
            return out
        def _i16(x): return x-0x10000 if x & 0x8000 else x
        _ds=[_i16(x) for x in _u16(_D)][:32]
        _T=_u16(_T)[:32]; _W=_u16(_W)[:32]
        _ss=[(_S[k//4]>>((k%4)*8))&0xFF for k in range(32)]
        # ol_cm_idx 指向"下一个要写的槽"⇒ 它是环形缓冲里最老的一条，按它起绕才是时间顺序
        _idx=_i[0] if _i else 0
        _ord=[(_idx+k) & 31 for k in range(32)]
        print(f'\n★ 换相埋点（最近 32 次换相；累计 {_n[0] if _n else "?"} 次）—— 已按时间顺序排列')
        print(f'{"序":>4}{"step":>5}{"delta(悬空相-V0)":>17}{"CNT(拍)":>9}{"waitTime":>10}')
        for k,p in enumerate(_ord):
            print(f'{k:4d}{_ss[p]:5d}{_ds[p]:17d}{_T[p]:9d}{_W[p]:10d}')
        _abs=[abs(x) for x in _ds if x!=0]
        if _abs:
            print(f'  ⇒ |delta| 中位 {np.median(_abs):.0f} 码  最小 {min(_abs)}  最大 {max(_abs)}')
            print(f'  ⇒ 判读：Phase 对 ⇒ 换相时 delta 应在"刚离零点、往平台走"（约幅值 1/2~1）')
            print(f'          Phase 错（在过零点上换相）⇒ delta ≈ 0')
        np.savez(csvp.replace('.csv','_cm.npz'), D=np.array(_ds), T=np.array(_T), W=np.array(_W), S=np.array(_ss))
        print(f'  埋点已存 → {csvp.replace(".csv","_cm.npz")}')
    except Exception as _e:
        print(f'（换相埋点 dump 失败：{_e}）')


# ---------------- --raw 模式：先开串口 spool、再烧录时用 ----------------
#   cap-evt.py --raw=<raw.bin>：只解析已落盘的原始字节，不碰串口、不驱动电机。
if RAW:
    with open(RAW, "rb") as _f:
        _buf = _f.read()
    print(f"（raw 模式）读入 {len(_buf)} 字节：{RAW}")
    save_run(_buf, 0.0, OUTD_DEF)
    sys.exit(0)

# ---------------- 正式采集（直接开串口 + 给油） ----------------
# ---------------- 正式采集 ----------------
oc(['-c', 'mww 0x40021060 0x01000000'])          # 清 RCC 复位标志
print('跑前 RCC_CSR :', resets(rd32(0x40021060)))
s = serial.Serial(PORT, BAUD, timeout=0.2); s.reset_input_buffer()
# ★ 先收油 1 秒再给油：电调的解锁/唤醒需要一段"零油门"，否则可能不启动（实测踩过）
print('← 先收油 3 秒（唤醒/解锁；重烧/复位后 1s 常常不够）'); f1(1); time.sleep(3.0)
print(f'→ 给油，采 {SECS:.0f} 秒 ...'); f1(0)
buf = bytearray(); t0 = time.time()
while time.time() - t0 < SECS:
    c = s.read(65536)
    if c:
        buf += c
f1(1); s.close(); print('← 收油')
print('跑后 RCC_CSR :', resets(rd32(0x40021060)), ' ← 出现 IWDG/POR 说明跑中复位了')


save_run(buf, SECS, OUTD_DEF)
