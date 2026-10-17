#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""serial-spool.py —— 先把串口【开着】持续落盘，供"采集顺道烧录"用。

为什么要它（用户原话：电脑准备串口采集，顺道烧录）：
  烧录会让 ESC 复位，若烧完才打开串口，就会漏掉复位后第一段（那是所有结论
  最集中的 0~2s）。所以正确顺序是【先开串口收集 → 再烧录 → 再给油】。
  本脚本只负责第一段：把原始字节持续写到文件；烧录/给油由 esc-lab.sh 编排。
  停止：SIGINT/SIGTERM（脚本收到后把缓冲区冲刷并退出）。

用法：
  python3 tools/serial-spool.py <输出.bin> [秒数=无限]
环境：
  ESC_SERIAL（默认 /dev/ttyUSB0）、ESC_BAUD（默认 1500000，须与固件一致）
"""
import os
import signal
import sys
import time

try:
    import serial
except Exception:
    print('❌ 没装 pyserial：pip install pyserial')
    sys.exit(2)

PORT = os.environ.get('ESC_SERIAL', '/dev/ttyUSB0')
BAUD = int(os.environ.get('ESC_BAUD', '1500000'))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    out = sys.argv[1]
    limit = float(sys.argv[2]) if len(sys.argv) > 2 else None
    stop = {'v': False}

    def on_sig(_sig, _frm):
        stop['v'] = True

    signal.signal(signal.SIGINT, on_sig)
    signal.signal(signal.SIGTERM, on_sig)

    try:
        sp = serial.Serial(PORT, BAUD, timeout=0.1)
    except Exception as e:
        print(f'❌ 打不开 {PORT} @ {BAUD}：{e}')
        return 2

    sp.reset_input_buffer()
    n = 0
    t0 = time.time()
    with open(out, 'wb') as f:
        while not stop['v']:
            if limit is not None and time.time() - t0 >= limit:
                break
            chunk = sp.read(65536)
            if chunk:
                f.write(chunk)
                n += len(chunk)
        f.flush()
    sp.close()
    print(f'spool: {out}  {n} 字节  {n / max(1e-9, time.time() - t0) / 1000:.1f} kB/s')
    return 0


if __name__ == '__main__':
    sys.exit(main())
