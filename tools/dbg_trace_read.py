#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
dbg_trace_read.py —— 读回 AM32 里的「RAM 黑匣子」（Inc/dbg_trace.h）并打印分析

为什么需要它：电机抖动时，靠单点变量看不出因果；这里把 RAM 里录下的
换相/过零事件时间线 + 中断计数 + BEMF 波形(burst) 一口气读出来。

用法（本机 ST-Link 设备节点属 root，所以要 sudo）：
    sudo python3 tools/dbg_trace_read.py
    sudo python3 tools/dbg_trace_read.py --elf obj/AM32_RM2027_ESC_G071_2.21.elf --keep /tmp/trace.bin

读之前建议让电调"跑一段"（解锁 + 给点油门），或者干脆带着电机抖动几秒，再执行本脚本。
脚本**只读 RAM**，不会 halt/复位目标（dump_image 在运行中也能读）。

⚠️ 结构体布局必须和 Inc/dbg_trace.h 保持一致；下面的常量是 2026-10-06 用
   host 端 offsetof 复核过的（sizeof(dbg_trace_t)=6452）。
"""
import argparse
import os
import struct
import subprocess
import sys

# ---- 与 Inc/dbg_trace.h 对齐的常量（已用 offsetof 复核） ----
OFF = {
    'commutations': 0,  'zc_events': 4,   'zc_too_early': 8,  'awd_irq': 12,
    'adc_cb': 16,       'tenkhz_ticks': 20, 'step_err': 24,   'spike_rejected': 28,
    'last_center': 32,  'last_hyst': 34,  'last_delta': 36,   'last_step': 38,
    'enable': 39,       'head': 40,       'ring': 44,
    'burst_arm': 5420,  'burst_slot': 5421, 'burst_count': 5424, 'burst': 5428,
    # ---- 长窗口（多步长示波器）----
    'long_delta': 6452, 'long_step': 6964, 'long_head': 7220,
    'long_div': 7222,    'long_wraps': 7224,
}
RING_N = 192
EV_SIZE = 28          # sizeof(dbg_ev_t)
BURST_N = 32
BURST_CNT = 16
LONG_N = 256          # 长窗口点数
LONG_DECIM = 16       # 降采样比（12.7kHz/16 ≈ 794 Hz）
TRACE_SIZE = 7228     # sizeof(dbg_trace_t)

EV_FMT = '<IHHHHhHHHHBBBB2x'   # t100, interval, avg_int, ccr, center, delta, va,vb,vc,v0, type, step, flags, seq, pad
EV_FLAGS = [(1 << 0, 'RUN'), (1 << 1, 'ARM'), (1 << 2, 'BEMF'), (1 << 3, 'RISE'), (1 << 4, 'AWDF')]


def sh(cmd, **kw):
    return subprocess.run(cmd, check=False, capture_output=True, text=True, **kw)


def get_symbol(elf, name):
    out = sh(['arm-none-eabi-nm', '-S', elf]).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 4 and parts[3] == name:
            return int(parts[0], 16), int(parts[1], 16)
    return None, None


def dump_ram(addr, size, dest):
    cfg = [
        'openocd', '-f', 'interface/stlink.cfg', '-f', 'target/stm32g0x.cfg',
        '-c', 'adapter speed 500', '-c', 'init',
        '-c', 'dump_image %s 0x%08X 0x%X' % (dest, addr, size),
        '-c', 'shutdown',
    ]
    r = sh(cfg)
    if not os.path.exists(dest) or os.path.getsize(dest) != size:
        print(r.stdout[-1500:]); print(r.stderr[-1500:])
        sys.exit('❌ dump 失败（是不是没加 sudo？ST-Link 设备节点属 root）')
    return dest


def u32(d, o):  return struct.unpack_from('<I', d, o)[0]
def u16(d, o):  return struct.unpack_from('<H', d, o)[0]
def i16(d, o):  return struct.unpack_from('<h', d, o)[0]


def fmt_flags(f):
    return '|'.join(n for bit, n in EV_FLAGS if f & bit) or '-'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--elf', default='obj/AM32_RM2027_ESC_G071_2.21.elf')
    ap.add_argument('--keep', default=None, help='保存原始 dump 到该路径')
    ap.add_argument('--freeze', action='store_true',
                    help='读之前先把 enable 写 0（冻结记录，读到的是稳定快照）')
    args = ap.parse_args()

    if not os.path.exists(args.elf):
        sys.exit('❌ 找不到 ELF：%s（先 make RM2027_ESC_G071）' % args.elf)

    addr, size = get_symbol(args.elf, 'g_trace')
    if addr is None:
        sys.exit('❌ ELF 里没有 g_trace 符号')
    if size != TRACE_SIZE:
        print('⚠️ 注意：g_trace 大小 %d ≠ 脚本预期的 %d ⇒ 结构体改过，'
              '请同步更新本脚本的 OFF/RING_N/TRACE_SIZE' % (size, TRACE_SIZE))

    if args.freeze:
        sh(['openocd', '-f', 'interface/stlink.cfg', '-f', 'target/stm32g0x.cfg',
            '-c', 'adapter speed 500', '-c', 'init',
            '-c', 'mww 0x%08X 0' % (addr + OFF['enable']), '-c', 'shutdown'])
        print('（已把 enable 写 0 冻结核记录；要恢复记录：'
              'openocd -c "mww 0x%08X 1"' % (addr + OFF['enable']) + '）')

    tmp = args.keep or '/tmp/am32_trace.bin'
    dump_ram(addr, size, tmp)
    d = open(tmp, 'rb').read()

    # ---------------- 计数与速率 ----------------
    c = {k: u32(d, OFF[k]) for k in
         ('commutations', 'zc_events', 'zc_too_early', 'awd_irq', 'adc_cb', 'tenkhz_ticks',
          'step_err', 'spike_rejected')}
    head = u16(d, OFF['head'])
    enable = d[OFF['enable']]
    burst_count = u32(d, OFF['burst_count'])

    print('=' * 78)
    print('AM32 RAM 黑匣子 @ 0x%08X   (%d 字节, enable=%d, 环形缓冲 head=%d, burst=%d 段)'
          % (addr, size, enable, head, burst_count))
    print('=' * 78)
    print('  换相 (interruptRoutine)      : %d' % c['commutations'])
    print('  过零 (zcfoundroutine)        : %d' % c['zc_events'])
    print('  过零被"太早"丢弃             : %d' % c['zc_too_early'])
    print('  AWD 中断                     : %d' % c['awd_irq'])
    print('  ADC 扫描完成回调             : %d' % c['adc_cb'])
    print('  10 kHz 时间拍                : %d' % c['tenkhz_ticks'])
    print('  step 序列异常                : %d' % c['step_err'])
    print('  BEMF 尖刺被丢弃              : %d' % c['spike_rejected'])
    print('  单点快照: step=%d delta=%d center=%d hyst=%d'
          % (d[OFF['last_step']], i16(d, OFF['last_delta']),
             u16(d, OFF['last_center']), u16(d, OFF['last_hyst'])))

    # ---------------- 现场判读（先给结论） ----------------
    print('-' * 78)
    print('【自动判读】')
    if c['commutations'] == 0:
        print('  ✗ 一次换相都没有：没进 running（没解锁？油门没给？）')
    if c['adc_cb'] == 0:
        print('  ✗ ADC 回调 0 次：采样序列根本没启动')
    elif c['tenkhz_ticks']:
        rate = c['adc_cb'] / (c['tenkhz_ticks'] * 100e-6)
        print('  · ADC 采样率 ≈ %.0f Hz（%d 次 / %.0f ms）'
              % (rate, c['adc_cb'], c['tenkhz_ticks'] * 0.1))
    if c['commutations'] > 0 and c['awd_irq'] == 0:
        print('  ✗ 有换相但 AWD 中断 0 次 ⇒ 过零检测从未触发（窗口/通道/使能问题）')
    if c['adc_cb'] > 0 and c['awd_irq'] > 10 * c['adc_cb']:
        print('  ✗ AWD 中断数远大于采样数 ⇒ 中断风暴（标志没清/一直落在窗外）')
    if c['step_err'] > 0:
        print('  ✗ step 序列出现重复/跳步 %d 次 ⇒ 换相时序乱了（这就是抖动的直接体现）'
              % c['step_err'])
    if c['zc_too_early'] > 0:
        print('  · 有 %d 次过零因"太早"被丢弃（噪声或相位超前）' % c['zc_too_early'])

    # ---------------- 事件时间线 ----------------
    ring = []
    for i in range(RING_N):
        rec = struct.unpack_from(EV_FMT, d, OFF['ring'] + i * EV_SIZE)
        ring.append(rec)
    # 按 seq（低 8 位）排序恢复时间顺序
    ring.sort(key=lambda r: r[11])
    ring = [r for r in ring if r[0] or r[4] or r[10] or r[13]]   # 丢掉全零空槽

    print('-' * 78)
    print('【事件时间线】（最近 %d 条，t100 单位 100 µs；interval 单位 0.5 µs）' % len(ring))
    print('   %-9s %-6s %-6s %-7s %-6s %-6s %-6s %-12s' %
          ('t(s)', 'type', 'step', 'dt_step', 'delta', 'center', 'ccr', 'flags'))
    prev_t = None
    for r in ring[-40:]:
        t100, interval, avg_i, ccr, center, delta, va, vb, vc, v0, typ, stp, fl, seq = r
        t_s = t100 * 100e-6
        dt = ((t100 - prev_t) % 65536) * 100e-6 if prev_t is not None else 0.0
        prev_t = t100
        print('   %-9.4f %-6s %-6d %-7.3f %-6d %-6d %-6d %-12s' %
              (t_s, 'COMM' if typ == 1 else ('ZC' if typ == 2 else '?'),
               stp, dt * 1000, delta, center, ccr, fmt_flags(fl)))

    # ---------------- 换相间隔抖动统计 ----------------
    ivs = [r[1] for r in ring if r[10] == 1 and r[1] > 0]
    if len(ivs) >= 2:
        ivs_us = sorted(v * 0.5 for v in ivs)
        n = len(ivs_us)
        print('-' * 78)
        print('【换相间隔抖动】（单位 µs，共 %d 个样本）' % n)
        print('   min=%.1f  p25=%.1f  中位=%.1f  p75=%.1f  max=%.1f   最大/最小=%.2f'
              % (ivs_us[0], ivs_us[n // 4], ivs_us[n // 2], ivs_us[3 * n // 4],
                 ivs_us[-1], ivs_us[-1] / max(ivs_us[0], 1e-9)))
        print('   （同一转速下这个比值应接近 1；>1.3 就说明换相忽快忽慢 = 抖）')

    # ---------------- BEMF 波形 burst（简易示波器） ----------------
    slots = burst_count if burst_count < BURST_CNT else BURST_CNT
    if slots:
        print('-' * 78)
        print('【换相后的 BEMF 波形】(悬空相 − V0，ADC 码；每行 = 一次换相后的 %d 个采样点)' % BURST_N)
        idx = [(u32(d, OFF['burst_slot']) - 1 - k) % BURST_CNT for k in range(min(slots, 6))][::-1]
        for s in idx:
            row = [i16(d, OFF['burst'] + (s * BURST_N + k) * 2) for k in range(BURST_N)]
            lo, hi = min(row), max(row)
            scale = max(abs(lo), abs(hi), 1)
            print('   burst%-2d  范围 [%6d, %6d]  ' % (s, lo, hi) +
                  ''.join('█' if v > 0 else ('▁' if v == 0 else '·') for v in row))
        print('   （看这里能判断：过零点是否出现在正确位置、波形是否被 PWM 污染）')

    # ---------------- 长窗口：多步长示波器 ----------------
    if 'long_delta' in OFF and size >= TRACE_SIZE:
        head = u16(d, OFF['long_head'])
        wraps = u32(d, OFF['long_wraps'])
        raw_d = [i16(d, OFF['long_delta'] + 2 * i) for i in range(LONG_N)]
        raw_s = [d[OFF['long_step'] + i] for i in range(LONG_N)]
        order = list(range(head, LONG_N)) + list(range(0, head))   # 按时间先后重排
        dl = [raw_d[i] for i in order]
        st = [raw_s[i] for i in order]

        print('-' * 78)
        print('【长窗口】最近 %d 点 × %d 采样/点 ≈ %.0f ms（已滚动 %d 圈）'
              % (LONG_N, LONG_DECIM, LONG_N * LONG_DECIM / 12687.0 * 1000.0, wraps))
        print('   上一行 = 悬空相−V0（每列取 4 点中的最大绝对值方向）；下一行 = 当时的 step')

        stride = LONG_N // 64
        cols = [max(dl[i:i + stride], key=abs) for i in range(0, LONG_N, stride)]
        cols_s = [st[i] for i in range(0, LONG_N, stride)]
        lo, hi = min(cols), max(cols)
        span = max(hi - lo, 1)
        lv = ' .:-=+*#%@'

        def bucket(v):
            return lv[min(len(lv) - 1, int((v - lo) / span * (len(lv) - 1)))]

        print('   Δ |' + ''.join(bucket(v) for v in cols))
        print('   s |' + ''.join(str(x) if x else '.' for x in cols_s))
        print('   （step 出现重复数字 = 那段时间没换相；数字按 1→6 顺序出现 = 换相在正常推进）')

    if not args.keep:
        print('\n（原始 dump 在 %s；要保留请加 --keep <路径>）' % tmp)


if __name__ == '__main__':
    main()
