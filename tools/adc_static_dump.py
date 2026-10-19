#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""adc_static_dump.py —— 静态 ADC 测试：SWD 连续读 ADCDataDMA / 母线 / 电流（不驱动电机）

用法:
  python3 tools/adc_static_dump.py [--samples 20] [--interval-ms 10] [--out DIR]

前提:
  · 当前板子烧的是 xien/n3c-adc-static 分支（ADC_STATIC_TEST=1/2）；
  · F1 保持收油，电机不转；母线 12V 稳定；
  · 脚本只 halt/resume 读 RAM，不写 PWM/不给油。

输出:
  adc_static_<时间>.csv
  列: sample, adc0..adc6, raw_volts, raw_current, frame
"""
import os, sys, re, csv, time, argparse, subprocess

ELF = '/home/xien/RM2027/AM32/obj/AM32_RM2027_ESC_G071_2.21.elf'
NM = '/usr/bin/arm-none-eabi-nm'


def sym_addr(name):
    out = subprocess.run([NM, '-S', ELF], capture_output=True, text=True).stdout
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 4 and p[3] == name:
            return int(p[0], 16)
    raise SystemExit(f'找不到符号 {name}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--samples', type=int, default=20)
    ap.add_argument('--interval-ms', type=int, default=10)
    ap.add_argument('--out', default='/home/xien/RM2027/analysis')
    a = ap.parse_args()
    dma = sym_addr('ADCDataDMA')
    rv = sym_addr('ADC_raw_volts')
    rc = sym_addr('ADC_raw_current')
    fr = sym_addr('adc_frame_total')
    os.makedirs(a.out, exist_ok=True)
    cmds = ['openocd', '-f', 'interface/stlink.cfg', '-f', 'target/stm32g0x.cfg',
            '-c', 'adapter speed 500', '-c', 'init', '-c', 'halt',
            '-c', 'mww 0x40003000 0xAAAA',
            '-c', f'mdw {dma:#x} 4', '-c', f'mdh {rv:#x} 1',
            '-c', f'mdh {rc:#x} 1', '-c', f'mdw {fr:#x} 1']
    for _ in range(a.samples - 1):
        cmds += ['-c', 'resume', '-c', f'sleep {a.interval_ms}', '-c', 'halt',
                 '-c', 'mww 0x40003000 0xAAAA',
                 '-c', f'mdw {dma:#x} 4', '-c', f'mdh {rv:#x} 1',
                 '-c', f'mdh {rc:#x} 1', '-c', f'mdw {fr:#x} 1']
    cmds += ['-c', 'resume', '-c', 'shutdown']
    out = subprocess.run(cmds, capture_output=True, text=True, timeout=60)
    text = out.stdout + out.stderr
    lines = []
    for line in text.splitlines():
        m = re.match(r'^0x[0-9a-fA-F]+:\s*([0-9a-fA-F ]+)$', line)
        if m:
            lines.append(m.group(1).split())
    # 每组 4 行：mdw(4 words) + mdh(rv) + mdh(rc) + mdw(frame)
    if len(lines) < 4:
        raise SystemExit('openocd 读数不足：\n' + text[-1500:])
    rows = []
    for k in range(len(lines) // 4):
        w = lines[k * 4]
        if len(w) < 4:
            continue
        u16 = []
        for x in w[:4]:
            v = int(x, 16); u16 += [v & 0xFFFF, (v >> 16) & 0xFFFF]
        rvv = int(lines[k * 4 + 1][0], 16)
        rcv = int(lines[k * 4 + 2][0], 16)
        frv = int(lines[k * 4 + 3][0], 16)
        rows.append([k] + u16[:7] + [rvv, rcv, frv])
    stamp = time.strftime('%Y%m%d_%H%M%S')
    csvp = os.path.join(a.out, f'adc_static_{stamp}.csv')
    with open(csvp, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f)
        w.writerow(['sample', 'adc0', 'adc1', 'adc2', 'adc3', 'adc4', 'adc5', 'adc6',
                    'raw_volts', 'raw_current', 'frame'])
        w.writerows(rows)
    print(f'ADCDataDMA @ {dma:#x}  raw_volts @ {rv:#x}  frame @ {fr:#x}')
    print(f'samples={len(rows)}  CSV → {csvp}')
    if rows:
        import statistics
        names = ['adc0', 'adc1', 'adc2', 'adc3', 'adc4', 'adc5', 'adc6', 'raw_volts', 'raw_current']
        for i, name in enumerate(names, start=1):
            v = [r[i] for r in rows]
            print(f'  {name:12s} mean={statistics.mean(v):8.2f}  std={statistics.pstdev(v):6.2f}  min={min(v):5d} max={max(v):5d}')


if __name__ == '__main__':
    main()
