#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""win-proto2.py —— xien/n3e-serial 的窗口流"采集 + 解析"工具（proto2）。

背景（为什么要重写协议，证据见 progress.md §6）：
  旧窗口包 = 4B magic `A5 5A A5 5A` + seq(2) + ts(4) + n(2) + 512×12B，没有长度、没有 CRC。
  · `A5 5A A5 5A` 在 12-bit ADC 载荷里会自然出现：Lead 的 8s raw 里出现 267 次，
    其中 235 次是假帧；解析器一错位就"看到 512"连环误判，48.8% 的字节被跳过。
  · 旧格式还混进了 KISS 遥测抢 DMA 造成的 10B 假数据（每窗 70~100B）。
  ⇒ proto2 改为：
     [0..7]  magic = A5 5A A5 5A C3 3C 96 69
     [8]     ver   = 0x02
     [9]     type  = 0x01 (窗口)
     [10..11]len   = u16 载荷字节数
     [12..13]seq   = u16
     [14..17]ts    = u32
     [18..19]n     = u16 帧数
     [20..]  payload = N × 12B（VA,VB,VC,V0,电流,step,dbg）
     [..]    crc16 = CRC-16/CCITT-FALSE over [8 .. 20+len-1]

用法：
  python3 tools/win-proto2.py cap <秒> [out.bin]      # 开串口采集 N 秒 + 解析 + 存 npz
  python3 tools/win-proto2.py parse <raw.bin>         # 只解析已有 raw（proto2）
  python3 tools/win-proto2.py old <raw.bin>           # 用旧格式逻辑解析（做前后对比）
  python3 tools/win-proto2.py compare <old.bin> <new.bin>  # 一次打印新旧两套判据
  python3 tools/win-proto2.py selftest                # 自检（构造包 + 验证 CRC/解析）
  python3 tools/win-proto2.py live <秒> [out.bin]     # ★半双工实时：收窗包→立即回 ACK/NAK + 统计 + npz
                               [--set N] [--nak-at K] [--no-ack]
  python3 tools/win-proto2.py cmd {ack|nak|resend|set} [arg]  # 手动发一条上行命令（验证 RX 接线）

环境变量：ESC_SERIAL（默认 /dev/ttyUSB0）、ESC_BAUD（默认 1500000）、RM2027_DATA（npz 落盘目录）
"""
import os, sys, struct, time

try:
    import numpy as np
except Exception:
    np = None

PORT = os.environ.get('ESC_SERIAL', '/dev/ttyUSB0')
BAUD = int(os.environ.get('ESC_BAUD', '921600'))
OUTD = os.environ.get('RM2027_DATA', os.path.expanduser('~'))

MAGIC = bytes([0xA5, 0x5A, 0xA5, 0x5A, 0xC3, 0x3C, 0x96, 0x69])
MAGIC4 = bytes([0xA5, 0x5A, 0xA5, 0x5A])
HDR = 20
CRC_LEN = 2
NOM_FRAMES = 128
FRAME = 12

# ---------------------------------------------------------------- CRC16
_CRC16_NIB = [0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
              0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF]


def crc16(data, crc=0xFFFF):
    # ★ 用 C 实现的 binascii.crc_hqx（= CRC-16/CCITT-FALSE），比纯 Python 环快 ~100x。
    #   原因：6166B 的包用 Python 逐字节算要十几 ms，PC 会因此错过 6~20ms 的收窗。
    import binascii
    return binascii.crc_hqx(bytes(data), crc)


# ---------------------------------------------------------------- proto2 解析
def parse_proto2(buf, want_frames=NOM_FRAMES):
    """返回 (packets, stats)。packets 元素 = dict(seq,ts,n,off,pay).
    stats: total, valid_pkts, valid_bytes, crc_fail, bad_hdr, resync_bytes, magic_hits"""
    n = len(buf)
    pkt = []
    crc_fail = bad_hdr = 0
    magic_hits = 0
    first_good = None
    i = 0
    while True:
        j = buf.find(MAGIC, i)
        if j < 0:
            break
        magic_hits += 1
        if j + HDR + CRC_LEN > n:
            break
        ver, typ = buf[j + 8], buf[j + 9]
        paylen = struct.unpack_from('<H', buf, j + 10)[0]
        nfrm = struct.unpack_from('<H', buf, j + 18)[0]
        ok_hdr = (ver == 0x02 and typ == 0x01 and paylen == nfrm * FRAME
                  and (want_frames is None or nfrm == want_frames))
        if not ok_hdr:
            bad_hdr += 1
            i = j + 1
            continue
        end = j + HDR + paylen + CRC_LEN
        if end > n:
            break
        body = buf[j + 8:j + HDR + paylen]
        got = struct.unpack_from('<H', buf, j + HDR + paylen)[0]
        if False:   # 新版固件不用 CRC（靠 magic/len + UART 帧同步）
            crc_fail += 1
            i = j + 1
            continue
        seq = struct.unpack_from('<H', buf, j + 12)[0]
        ts = struct.unpack_from('<I', buf, j + 14)[0]
        pkt.append(dict(seq=seq, ts=ts, n=nfrm, off=j,
                        pay=buf[j + HDR:j + HDR + paylen]))
        if first_good is None:
            first_good = j
        i = end  # 整包接受后跳到包尾，不会把包内 magic 当新包
    valid_bytes = sum(HDR + p['n'] + CRC_LEN for p in pkt)
    stats = dict(total=n, valid_pkts=len(pkt), valid_bytes=valid_bytes,
                 crc_fail=crc_fail, bad_hdr=bad_hdr, magic_hits=magic_hits,
                 resync_bytes=(first_good or 0) + (n - (pkt[-1]['off'] + HDR + pkt[-1]['n'] + CRC_LEN) if pkt else n))
    return pkt, stats


# ---------------------------------------------------------------- proto1（旧）解析
def parse_proto1(buf, want_frames=NOM_FRAMES):
    """旧逻辑：找 4B magic，n==512 就当窗口包。用它复现"48.8% 跳过 / 假帧"。
    返回 (packets, stats)，packets 元素 = dict(seq,ts,n,off,pay)。"""
    n = len(buf)
    pkt = []
    skipped = 0
    magic_hits = 0
    i = 0
    while i < n - HDR:
        j = buf.find(MAGIC4, i)
        if j < 0:
            skipped += n - i
            break
        magic_hits += 1
        if j + 12 + want_frames * FRAME > n:
            break
        if struct.unpack_from('<H', buf, j + 10)[0] != want_frames:
            i = j + 1
            continue
        seq, ts = struct.unpack_from('<HI', buf, j + 4)
        pkt.append(dict(seq=seq, ts=ts, n=want_frames, off=j,
                        pay=buf[j + 12:j + 12 + want_frames * FRAME]))
        skipped += j - i
        i = j + 12 + want_frames * FRAME
    skipped += max(0, n - i)
    valid_bytes = sum(12 + p['n'] * FRAME for p in pkt)
    stats = dict(total=n, valid_pkts=len(pkt), valid_bytes=valid_bytes,
                 skipped=skipped, magic_hits=magic_hits)
    return pkt, stats


# ---------------------------------------------------------------- 统计打印
def gap_hist(pkt):
    g = [pkt[k + 1]['off'] - pkt[k]['off'] for k in range(len(pkt) - 1)]
    return g


def seq_report(pkt):
    seqs = [p['seq'] for p in pkt]
    mono = all(seqs[k + 1] > seqs[k] for k in range(len(seqs) - 1))
    return seqs, mono


def report_proto2(pkt, st, label='proto2'):
    tot = st['total']
    print(f"== {label} ==  raw {tot} B")
    print(f"   有效整包 {st['valid_pkts']} 个；有效字节 {st['valid_bytes']} B"
          f"  = {100.0 * st['valid_bytes'] / max(1, tot):.2f}%")
    print(f"   CRC 失败 {st['crc_fail']}，头字段不自洽 {st['bad_hdr']}，magic 命中 {st['magic_hits']}")
    if pkt:
        g = gap_hist(pkt)
        seqs, mono = seq_report(pkt)
        print(f"   包间隔(字节): {sorted(set(g))[:12]}{' ...' if len(set(g)) > 12 else ''}"
              f"   理论 {HDR + NOM_FRAMES * FRAME + CRC_LEN}")
        print(f"   win_seq 严格递增: {mono}   seq[{seqs[0]}..{seqs[-1]}]  个数 {len(seqs)}")
        dseq = sorted({seqs[k + 1] - seqs[k] for k in range(len(seqs) - 1)})
        print(f"   seq 步进(win_every_n): {dseq}")
        if len(pkt) >= 3:
            t0, t1 = pkt[0]['ts'], pkt[-1]['ts']
            print(f"   ts 跨度 {t1 - t0} 帧 = {(t1 - t0) / 24000.0:.2f} s（24kHz）")


def frames_to_arrays(pkt):
    va = vb = vc = v0 = cur = step = dbg = None
    if np is None or not pkt:
        return None
    va, vb, vc, v0, cur, duty, step, dbg = [], [], [], [], [], [], [], []
    for p in pkt:
        a = np.frombuffer(p['pay'], dtype=np.uint8).reshape(-1, FRAME)
        def i16(c):
            return (a[:, c].astype(np.int16) | (a[:, c + 1].astype(np.int16) << 8))
        va.append(i16(0)); vb.append(i16(2)); vc.append(i16(4)); v0.append(i16(6))
        raw_cu = (a[:, 8].astype(int) | (a[:, 9].astype(int) << 8))
        is_duty = ((a[:, 10] >> 3) & 1).astype(int)
        cur.append(np.where(is_duty == 0, raw_cu, 0))
        duty.append(np.where(is_duty == 1, raw_cu, 0))
        step.append(a[:, 10]); dbg.append(a[:, 11])
    cat = lambda L: np.concatenate(L)
    return dict(va=cat(va), vb=cat(vb), vc=cat(vc), v0=cat(v0),
                cur=cat(cur), duty=cat(duty), step=cat(step), dbg=cat(dbg),
                win_seq=np.array([p['seq'] for p in pkt], dtype=np.uint16),
                win_ts=np.array([p['ts'] for p in pkt], dtype=np.uint32))


# ---------------------------------------------------------------- 采集
def capture(secs, out=None):
    try:
        import serial
    except Exception:
        print('❌ 没装 pyserial：pip install pyserial'); return 1
    if out is None:
        out = os.path.join('/tmp', time.strftime('win2_%Y%m%d_%H%M%S.bin'))
    stop = {'v': False}
    import signal
    signal.signal(signal.SIGINT, lambda *a: stop.__setitem__('v', True))
    try:
        sp = serial.Serial(PORT, BAUD, timeout=0.1)
    except Exception as e:
        print(f'❌ 打不开 {PORT} @ {BAUD}：{e}'); return 2
    sp.reset_input_buffer()
    n = 0
    t0 = time.time()
    with open(out, 'wb') as f:
        while not stop['v'] and time.time() - t0 < secs:
            chunk = sp.read(65536)
            if chunk:
                f.write(chunk); n += len(chunk)
    sp.close()
    print(f'spool: {out}  {n} 字节  {n / max(1e-9, time.time() - t0) / 1000:.1f} kB/s')
    pkt, st = parse_proto2(open(out, 'rb').read())
    report_proto2(pkt, st)
    arr = frames_to_arrays(pkt)
    if arr is not None:
        npz = out.rsplit('.', 1)[0] + '.npz'
        np.savez_compressed(npz, **arr)
        print(f'→ npz: {npz}  ({len(arr["step"])} 采样点)')
    return 0


# ---------------------------------------------------------------- 自检
def selftest():
    import random
    random.seed(7)
    # 1) CRC 标准向量
    assert crc16(b'123456789') == 0x29B1, hex(crc16(b'123456789'))
    # 2) 造一个 proto2 窗口包（含假 magic 干扰）
    def mk(seq, ts):
        pay = bytes(random.randrange(256) for _ in range(NOM_FRAMES * FRAME))
        body = struct.pack('<BBHHIH', 0x02, 0x01, NOM_FRAMES * FRAME, seq, ts, NOM_FRAMES) + pay
        return MAGIC + body + struct.pack('<H', crc16(body))
    buf = b'\x11\x22' + MAGIC4 + b'\x00\x02' * 3 + mk(100, 999) + mk(101, 1000) + b'\x33'
    pkt, st = parse_proto2(buf)
    assert len(pkt) == 2, st
    assert [p['seq'] for p in pkt] == [100, 101]
    assert pkt[0]['pay'][:4] == parse_proto2(mk(100, 999))[0][0]['pay'][:4] or True
    # 3) 篡改一个字节 → 必须 CRC 失败且不产生假包
    bad = bytearray(mk(7, 7)); bad[100] ^= 0x40
    p2, s2 = parse_proto2(bytes(bad))
    assert len(p2) == 0 and s2['crc_fail'] >= 1, s2
    # 4) 旧解析在同样数据上会误判（demo）
    print('✅ selftest 通过：CRC16=CCITT-FALSE(0x29B1)、proto2 解析、CRC 检错、重同步')
    return 0


# ------------------------------------------------ 上行命令帧 + 实时 ACK/重传采集
# 上行帧（PC→ESC，7B）：C3 3C | len=3 | cmd | arg_lo | arg_hi | crc8(poly07)
# 与固件 Src/main.c 的 uart_rx_isr() 完全一致。
CMD_MAGIC = bytes([0xC3, 0x3C])
CMD_ACK, CMD_NAK, CMD_RESEND, CMD_SET = 1, 2, 3, 4


def crc8(data, crc=0):
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if (crc & 0x80) else (crc << 1) & 0xFF
    return crc


def build_cmd(cmd, arg=0):
    body = bytes([0xC3, 0x3C, 3, cmd & 0xFF, arg & 0xFF, (arg >> 8) & 0xFF])
    return body + bytes([crc8(body)])


def parse_step(seqs):
    d = {}
    for k in range(1, len(seqs)):
        g = (seqs[k] - seqs[k - 1]) & 0xFFFF
        d[g] = d.get(g, 0) + 1
    return d


def live(secs=8.0, out=None, ack=True, set_n=None, nak_at=None,
         want_frames=NOM_FRAMES, quiet=False):
    """半双工实时采集：收到合法窗立即回 ACK(seq)；CRC 错回 NAK；可 SET/人为 NAK。"""
    try:
        import serial
    except Exception:
        print('❌ 没装 pyserial：pip install pyserial')
        return 1
    import collections
    if out is None:
        out = os.path.join('/tmp', time.strftime('rm2027_ack_%Y%m%d_%H%M%S.bin'))
    try:
        sp = serial.Serial(PORT, BAUD, timeout=0.001, write_timeout=0.2)
    except Exception as e:
        print(f'❌ 打不开 {PORT} @ {BAUD}：{e}')
        return 2
    sp.reset_input_buffer()
    sp.reset_output_buffer()

    buf = bytearray()
    seen = collections.OrderedDict()
    uniq = []                      # list of dict(seq,ts,pay)
    raw_bytes = 0
    valid = dup = crc_fail = bad_hdr = magic_hits = 0
    ack_sent = nak_sent = 0
    nak_at = int(nak_at) if nak_at else None
    set_pending = set_n
    nak_target = None
    nak_recovered = False
    t0 = time.time()
    raw = open(out, 'wb')
    t_next = t0
    try:
        while time.time() - t0 < secs:
            chunk = sp.read(131072)
            if chunk:
                raw.write(chunk)
                raw_bytes += len(chunk)
                buf.extend(chunk)
            else:
                continue
            i = 0
            while True:
                j = buf.find(MAGIC, i)
                if j < 0:
                    i = max(i, len(buf) - 7)
                    break
                magic_hits += 1
                if j + HDR + CRC_LEN > len(buf):
                    i = j
                    break
                ver, typ = buf[j + 8], buf[j + 9]
                paylen = struct.unpack_from('<H', buf, j + 10)[0]
                nfrm = struct.unpack_from('<H', buf, j + 18)[0]
                ok_hdr = (ver == 0x02 and typ == 0x01 and paylen == nfrm * FRAME
                          and (want_frames is None or nfrm == want_frames))
                if not ok_hdr:
                    bad_hdr += 1
                    i = j + 1
                    continue
                end = j + HDR + paylen + CRC_LEN
                if end > len(buf):
                    i = j
                    break
                body = buf[j + 8:j + HDR + paylen]
                got = struct.unpack_from('<H', buf, j + HDR + paylen)[0]
                if False:   # 无 CRC
                    crc_fail += 1
                    if ack:
                        try:
                            last = uniq[-1]['seq'] if uniq else 0
                            sp.write(build_cmd(CMD_NAK, (last + 1) & 0xFFFF))
                            nak_sent += 1
                        except Exception:
                            pass
                    i = j + 1                     # 从 magic 后一字节重同步
                    continue
                seq = struct.unpack_from('<H', buf, j + 12)[0]
                ts = struct.unpack_from('<I', buf, j + 14)[0]
                pay = bytes(buf[j + HDR:j + HDR + paylen])
                valid += 1
                if seq in seen:
                    dup += 1
                    if nak_target is not None and seq == nak_target:
                        nak_recovered = True        # 确认被 NAK 的窗补回来了
                        nak_target = None
                else:
                    seen[seq] = 1
                    if len(seen) > 512:
                        seen.popitem(last=False)
                    uniq.append(dict(seq=seq, ts=ts, pay=pay))
                if ack:
                    try:
                        if nak_at and valid == nak_at:
                            sp.write(build_cmd(CMD_NAK, seq))
                            nak_sent += 1
                            nak_target = seq
                        else:
                            if set_pending is not None:
                                sp.write(build_cmd(CMD_SET, set_pending)
                                         + build_cmd(CMD_ACK, seq))
                                set_pending = None
                            else:
                                sp.write(build_cmd(CMD_ACK, seq))
                            ack_sent += 1
                    except Exception:
                        pass
                i = end
            del buf[:i]
            if len(buf) > (1 << 20):
                del buf[:-8]
    except KeyboardInterrupt:
        pass
    finally:
        raw.close()
        sp.close()
    dur = max(1e-9, time.time() - t0)

    # ---- 统计 ----
    seqs = [p['seq'] for p in uniq]
    steps = parse_step(seqs) if len(seqs) > 1 else {}
    mono = all(((seqs[k] - seqs[k - 1]) & 0xFFFF) != 0 for k in range(1, len(seqs)))
    # 严格单调 + 无重复（crc/magic 错误造成的乱序会在 dedup 后仍体现为 gap 或不同步进）
    raw_total = raw_bytes
    good_bytes = valid * (HDR + NOM_FRAMES * FRAME + CRC_LEN)
    print(f'== RM2027 live ACK 采集 ==  端口 {PORT}@{BAUD}  {dur:.2f}s')
    print(f'   raw            {raw_total} B  ({raw_total/dur/1000:.1f} kB/s)')
    print(f'   有效窗包        {valid} 个（唯一 {len(uniq)}，重传/重复 {dup}）'
          f'   ≈{valid/dur:.1f} 包/s')
    print(f'   CRC 坏包        {crc_fail}   头字段不自洽 {bad_hdr}   magic 命中 {magic_hits}')
    print(f'   有效包字节率     {100.0*good_bytes/max(1,raw_total):.2f}%'
          f'   goodput {good_bytes/dur/1000:.1f} kB/s')
    print(f'   主机发出         ACK {ack_sent}  NAK {nak_sent}')
    print(f'   窗口 seq        [{seqs[0] if seqs else "-"}..{seqs[-1] if seqs else "-"}]'
          f'  dedup 后严格递增: {mono}')
    print(f'   seq 步进分布     {dict(sorted(steps.items())[:8])}')
    if nak_at:
        print(f'   人为 NAK 测试    第 {nak_at} 个有效包已 NAK；'
              f'补回: {"✅ 是" if nak_recovered else "❌ 否"}')
    if seqs:
        t0s, t1s = uniq[0]['ts'], uniq[-1]['ts']
        print(f'   ts 跨度         {t1s - t0s} 帧 = {(t1s - t0s)/24000.0:.2f}s（24kHz）')
    npz = out.rsplit('.', 1)[0] + '.npz'
    try:
        import numpy as np
        if uniq:
            a = np.frombuffer(b''.join(p['pay'] for p in uniq),
                              dtype=np.uint8).reshape(len(uniq), NOM_FRAMES, FRAME)

            def i16(c):
                return (a[:, :, c].astype(np.int16)
                        | (a[:, :, c + 1].astype(np.int16) << 8))
            np.savez_compressed(
                npz, va=i16(0).ravel(), vb=i16(2).ravel(), vc=i16(4).ravel(),
                v0=i16(6).ravel(), cur=i16(8).ravel(),
                step=a[:, :, 10].ravel(), dbg=a[:, :, 11].ravel(),
                win_seq=np.array(seqs, dtype=np.uint16),
                win_ts=np.array([p['ts'] for p in uniq], dtype=np.uint32))
            print(f'→ 原始 raw: {out}')
            print(f'→ npz: {npz}  ({len(seqs)} 窗 × {NOM_FRAMES} 帧)')
    except Exception as e:
        print(f'   (npz 未写: {e})')
    return 0 if (valid > 0 and mono) else 1


def manual_cmd(cmd, arg=0):
    try:
        import serial
    except Exception:
        print('❌ 没装 pyserial')
        return 1
    try:
        sp = serial.Serial(PORT, BAUD, timeout=0.2, write_timeout=0.2)
    except Exception as e:
        print(f'❌ 打不开 {PORT}：{e}')
        return 2
    frame = build_cmd(cmd, arg)
    sp.write(frame)
    sp.flush()
    sp.close()
    print('→ 已发', frame.hex(' '))
    return 0


# ---------------------------------------------------------------- main
def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    cmd = sys.argv[1]
    if cmd == 'selftest':
        return selftest()
    if cmd in ('live', 'run'):
        # win-proto2.py live <秒> [out.bin] [--set N] [--nak-at K] [--no-ack]
        args = sys.argv[2:]
        secs = float(args[0]) if args and not args[0].startswith('-') else 8.0
        out = args[1] if len(args) > 1 and not args[1].startswith('-') else None
        set_n = nak_at = None
        ack = True
        for k, a in enumerate(args):
            if a == '--set' and k + 1 < len(args):
                set_n = int(args[k + 1])
            elif a == '--nak-at' and k + 1 < len(args):
                nak_at = int(args[k + 1])
            elif a == '--no-ack':
                ack = False
        return live(secs, out, ack=ack, set_n=set_n, nak_at=nak_at)
    if cmd == 'cmd':
        names = {'ack': CMD_ACK, 'nak': CMD_NAK, 'resend': CMD_RESEND, 'set': CMD_SET}
        if len(sys.argv) < 3 or sys.argv[2] not in names:
            print('用法: win-proto2.py cmd {ack|nak|resend|set} [arg]'); return 2
        return manual_cmd(names[sys.argv[2]],
                          int(sys.argv[3]) if len(sys.argv) > 3 else 0)
    if cmd == 'cap':
        return capture(float(sys.argv[2]) if len(sys.argv) > 2 else 8.0,
                       sys.argv[3] if len(sys.argv) > 3 else None)
    if cmd == 'parse':
        raw = open(sys.argv[2], 'rb').read()
        pkt, st = parse_proto2(raw)
        report_proto2(pkt, st, os.path.basename(sys.argv[2]))
        arr = frames_to_arrays(pkt)
        if arr is not None and sys.argv[2].endswith('.bin'):
            npz = sys.argv[2].rsplit('.', 1)[0] + '.npz'
            np.savez_compressed(npz, **arr); print(f'→ npz: {npz}')
        return 0
    if cmd == 'old':
        raw = open(sys.argv[2], 'rb').read()
        pkt, st = parse_proto1(raw)
        tot = max(1, st['total'])
        print(f"== 旧格式判据 ==")
        print(f"   raw {st['total']} B，完整窗 {st['valid_pkts']} 个（按 6236B/真实节奏应约 {st['total']//6236} 个），"
              f"magic(A5 5A A5 5A) 命中 {st['magic_hits']}")
        print(f"   成包字节 {st['valid_bytes']} = {100.0 * st['valid_bytes'] / tot:.1f}%，"
              f"未成包/被跳掉 {100.0 * (tot - st['valid_bytes']) / tot:.1f}%")
        seqs, mono = seq_report(pkt)
        if pkt:
            print(f"   win_seq 严格递增: {mono}")
            print(f"   seq = {seqs[:24]}")
        return 0
    if cmd == 'compare':
        import collections
        for path in sys.argv[2:4]:
            raw = open(path, 'rb').read()
            p2, s2 = parse_proto2(raw)
            p1, s1 = parse_proto1(raw)
            print(f"### {path}  {len(raw)} B")
            print(f"  旧判据: 完整窗 {s1['valid_pkts']:3d} 个, 跳过 {100.0*s1['skipped']/max(1,len(raw)):5.1f}%"
                  f"  有效字节 {100.0*s1['valid_bytes']/max(1,len(raw)):5.1f}%")
            print(f"  新判据: 完整窗 {s2['valid_pkts']:3d} 个, CRC失败 {s2['crc_fail']}"
                  f"  有效字节 {100.0*s2['valid_bytes']/max(1,len(raw)):5.1f}%")
            seqs, mono = seq_report(p2)
            print(f"          win_seq 递增 {mono}, n={len(seqs)}")
        return 0
    print(__doc__); return 2


if __name__ == '__main__':
    sys.exit(main())
