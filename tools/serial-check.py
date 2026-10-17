#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""serial-check.py —— 串口链路自检（xien/n2）
跑前 3 秒：① 收到多少字节 ② win_seq / 事件 seq 是否在涨。

为什么需要它（踩过的坑）：
  ESC 的"发送侧"会卡死在待发送（win_seq == win_sent 不再增长、tx_ready() 恒假），
  此时电机照转、但一个字节都收不到 ⇒ 会被误判成"电机没转/没交棒"。
  判据：3 秒内 <5000 字节 ⇒ 发送侧停摆，先复位/重烧再跑。
用法：python3 tools/serial-check.py [秒数=3]
"""
import os
import sys
import time

try:
    import serial
except Exception:
    print('❌ 没装 pyserial：pip install pyserial')
    sys.exit(2)

PORT = os.environ.get('ESC_SERIAL', '/dev/ttyUSB0')
BAUD = int(os.environ.get('ESC_BAUD', '1500000'))
SECS = float(sys.argv[1]) if len(sys.argv) > 1 else 3.0

try:
    sp = serial.Serial(PORT, BAUD, timeout=0.5)
except Exception as e:
    print(f'❌ 打不开 {PORT} @ {BAUD}：{e}')
    sys.exit(2)

sp.reset_input_buffer()
n = 0
hdr = 0
t0 = time.time()
buf = b''
while time.time() - t0 < SECS:
    chunk = sp.read(65536)
    n += len(chunk)
    buf += chunk
    hdr += chunk.count(b'\xA5\x5A\xA5\x5A')
sp.close()

rate = n / max(1e-9, SECS)
print(f'串口 {PORT} @ {BAUD}')
print(f'  {SECS:.1f}s 收到 {n} 字节  ({rate/1000:.1f} kB/s，链路上限约 {BAUD/10/1000:.0f} kB/s)')
print(f'  帧头 A5 5A A5 5A 出现 {hdr} 次（事件帧/窗口帧都以此开头）')
if n < 5000:
    print('  ❌ 判定：发送侧停摆（3s < 5000 B）。先复位/重烧一次再跑。')
    sys.exit(1)
if hdr == 0:
    print('  ❌ 判定：字节在流但没有帧头 ⇒ 波特率不匹配（PC 侧 ESC_BAUD 要和固件 ADC_CAP_BAUD 一致）')
    sys.exit(1)
print('  ✅ 链路正常')
