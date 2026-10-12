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
import sys, os, time, struct, subprocess, re, csv, datetime
import numpy as np, serial
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
for _f in ('Noto Sans CJK SC', 'Noto Sans CJK JP'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family'] = _f; break
plt.rcParams['axes.unicode_minus'] = False

PORT, BAUD, OUTD_DEF = '/dev/ttyUSB0', 1000000, '/home/lpl/桌面/RM2027'
PLOT_ONLY = None
if len(sys.argv) > 1 and sys.argv[1].startswith('--plot'):
    PLOT_ONLY = sys.argv[1].split('=', 1)[-1]
    SECS, OUTD = 0.0, OUTD_DEF
else:
    SECS = float(sys.argv[1]) if len(sys.argv) > 1 else 7.0
    OUTD = sys.argv[2] if len(sys.argv) > 2 else OUTD_DEF


def valid(r):
    """剔除"窗口包被误读成事件帧"的假帧（判据抄 evt-plot.py）"""
    _seq, _ts, dl, st, _w, du, cur, _g = r
    return (1 <= st <= 6) and (0 <= du <= 2000) and (abs(dl) < 3000) and (0 <= cur < 400)


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
    import struct as _st
    while i < len(buf) - 21:
        j = buf.find(b'\xA5\x5A\xA5\x5A', i)
        if j < 0:
            break
        if crc8(buf[j:j + 20]) == buf[j + 20]:
            rows.append(struct.unpack_from('<HIhBHHh', buf, j + 4) + (buf[j + 19],))
            i = j + 21
        elif j + 12 <= len(buf) and struct.unpack_from('<H', buf, j + 10)[0] == 512:
            wseq, wts = struct.unpack_from('<HI', buf, j + 4)
            pay = np.frombuffer(buf[j + 12:j + 12 + 512 * 3], dtype=np.uint8).reshape(-1, 3)
            d = (pay[:, 0].astype(np.int16) | (pay[:, 1].astype(np.int16) << 8))
            wins.append((wseq, wts, d, pay[:, 2].copy()))
            i = j + 12 + 512 * 3
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
    iv = np.diff(ts)[m] / 24.0
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

# ---------------- 正式采集 ----------------
oc(['-c', 'mww 0x40021060 0x01000000'])          # 清 RCC 复位标志
print('跑前 RCC_CSR :', resets(rd32(0x40021060)))
s = serial.Serial(PORT, BAUD, timeout=0.2); s.reset_input_buffer()
print(f'→ 给油，采 {SECS:.0f} 秒 ...'); f1(0)
buf = bytearray(); t0 = time.time()
while time.time() - t0 < SECS:
    c = s.read(65536)
    if c:
        buf += c
f1(1); s.close(); print('← 收油')
print('跑后 RCC_CSR :', resets(rd32(0x40021060)), ' ← 出现 IWDG/POR 说明跑中复位了')

rows, wins, nfake = parse(buf)
stamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
csvp = os.path.join(OUTD, f'evt_run_{stamp}.csv')
with open(csvp, 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['seq', 'ts_frame', 'delta', 'step', 'wait', 'duty', 'cur', 'stage'])
    w.writerows(rows)
rate = f'{len(buf)/SECS/1000:.1f} KB/s' if SECS else '—'
print(f'\n原始字节 {len(buf)} ({rate})   窗口包 {wins}   事件帧 有效 {len(rows)}（剔除假帧 {nfake}）')
print(f'CSV → {csvp}  ({os.path.getsize(csvp)} 字节)')
if wins:
    npzp = csvp.replace('.csv', '_win.npz')
    np.savez(npzp, seq=np.array([w[0] for w in wins]),
             ts=np.array([w[1] for w in wins]),
             d=np.array([w[2] for w in wins]), step=np.array([w[3] for w in wins]))
    print(f'窗口载荷 → {npzp}  ({os.path.getsize(npzp)} 字节；{len(wins)} 片 x 512 点 @24kHz)')
if not rows:
    print('❌ 一个事件都没收到 —— 检查 12V(母线电压) / F1 油门'); sys.exit(1)
make_plot(rows, SECS, csvp.rsplit('.', 1)[0] + '.png')
