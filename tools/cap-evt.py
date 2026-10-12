#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
cap-evt.py —— 一次运行：采串口事件流 → 存 CSV + 出四栏图。★ 数据一定落盘，不再只算在内存里。
用法: python3 tools/cap-evt.py [秒数=7] [输出目录=/home/lpl/桌面/RM2027]
产出:
  <目录>/evt_run_YYYYmmdd_HHMMSS.csv   逐次换相的事件（CRC 校验通过）
  <目录>/evt_run_YYYYmmdd_HHMMSS.png   四栏图：换相间隔 / 转速 / 占空比+电流 / step
                                                背景按 开环/闭环 分色
注意: 电机电源(12V)必须开着，否则电调不换相、一个字节都收不到。
"""
import sys, os, time, struct, subprocess, re, csv, datetime, statistics
import serial
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
for _f in ('Noto Sans CJK SC','Noto Sans CJK JP'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family']=_f; break
plt.rcParams['axes.unicode_minus']=False

SECS=float(sys.argv[1]) if len(sys.argv)>1 else 7.0
OUTD=sys.argv[2] if len(sys.argv)>2 else '/home/lpl/桌面/RM2027'
PORT='/dev/ttyUSB0'; BAUD=1000000
def crc8(b):
    c=0
    for x in b:
        c^=x
        for _ in range(8): c=((c<<1)^0x07)&0xFF if c&0x80 else (c<<1)&0xFF
    return c
def oc(args,iface='stlink.cfg',tgt='stm32g0x.cfg',sp=1800):
    return subprocess.run(['openocd','-f',f'interface/{iface}','-f',f'target/{tgt}',
        '-c',f'adapter speed {sp}','-c','init','-c','halt','-c','mww 0x40003000 0xAAAA',
        *args,'-c','mww 0x40003000 0xAAAA','-c','resume','-c','shutdown'],capture_output=True,text=True)
def f1(v):
    subprocess.run(['openocd','-f','interface/jlink.cfg','-c','transport select swd',
        '-f','target/stm32f1x.cfg','-c','adapter speed 500','-c','init','-c','halt',
        '-c',f'mwb 0x20000274 {v}','-c','resume','-c','shutdown'],capture_output=True,timeout=30)
def rd32(a):
    o=oc(['-c',f'mdw {a:#x} 1'])
    m=re.search(r'^(0x[0-9a-fA-F]+): ([0-9a-fA-F]+)',o.stdout+o.stderr,re.M)
    return int(m.group(2),16) if m else None
def resets(v):
    if v is None: return '?'
    nm=[(26,'PIN'),(27,'POR'),(28,'SFT'),(29,'IWDG'),(30,'WWDG'),(31,'LPWR')]
    on=[n for b,n in nm if v>>b&1]
    return f'0x{v:08X} ({",".join(on) if on else "无"})'

# 清复位标志 → 跑 → 再看（跑中若有复位，标志会重新置起来）
oc(['-c','mww 0x40021060 0x01000000'])
print('跑前 RCC_CSR :', resets(rd32(0x40021060)))
s=serial.Serial(PORT,BAUD,timeout=0.2); s.reset_input_buffer()
print(f'→ 给油，采 {SECS:.0f} 秒 ...'); f1(0)
buf=bytearray(); t0=time.time()
while time.time()-t0<SECS:
    c=s.read(65536)
    if c: buf+=c
f1(1); s.close(); print('← 收油')
print('跑后 RCC_CSR :', resets(rd32(0x40021060)), '  ← 若出现 IWDG/POR 说明跑中复位了')

rows=[]; wins=0; i=0
while i < len(buf)-21:
    j=buf.find(b'\xA5\x5A\xA5\x5A',i)
    if j<0: break
    if crc8(buf[j:j+20])==buf[j+20]:
        rows.append(struct.unpack_from('<HIhBHHh',buf,j+4)+(buf[j+19],)); i=j+21
    else:
        if j+12<=len(buf) and struct.unpack_from('<H',buf,j+10)[0]==512: wins+=1; i=j+12+512*3
        else: i=j+1
stamp=datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
csvp=os.path.join(OUTD,f'evt_run_{stamp}.csv')
with open(csvp,'w',newline='') as f:
    w=csv.writer(f); w.writerow(['seq','ts_frame','delta','step','wait','duty','cur','stage'])
    w.writerows(rows)
print(f'\n原始字节 {len(buf)} 字节 ({len(buf)/SECS/1000:.1f} KB/s)   事件帧 {len(rows)}   窗口包 {wins}')
print(f'CSV → {csvp}  ({os.path.getsize(csvp)} 字节)')
if not rows:
    print('❌ 一个事件都没收到 —— 检查 12V 是否开着（母线电压）'); sys.exit(1)
seq=[r[0] for r in rows]; ts=[r[1] for r in rows]; stg=[r[7] for r in rows]
dut=[r[5] for r in rows]; cur=[r[6] for r in rows]
iv=[(ts[k]-ts[k-1])/24.0 for k in range(1,len(rows)) if seq[k]-seq[k-1]==1 and 0<ts[k]-ts[k-1]<24000]
tt=[(ts[k]-ts[0])/24000.0 for k in range(1,len(rows)) if seq[k]-seq[k-1]==1 and 0<ts[k]-ts[k-1]<24000]
ol=sum(1 for x in stg if x==1)
if iv:
    m=statistics.median(iv); srt=sorted(iv)
    print(f'开环事件 {ol} / 闭环 {len(rows)-ol}')
    print(f'换相间隔 中位 {m:.3f}ms  P10 {srt[int(len(srt)*.1)]:.3f}  P90 {srt[int(len(srt)*.9)]:.3f}  最大 {max(iv):.3f}')
    print(f'⇒ 转速(中位) {60/(m*6/1000)/6:.0f} 转/分')
fig,ax=plt.subplots(4,1,figsize=(16,12),sharex=True,gridspec_kw={'height_ratios':[1.3,1,1,0.8]})
if iv:
    ax[0].plot(tt,iv,lw=.7,color='#c0392b'); ax[0].axhline(1.0,color='gray',ls='--',lw=1,label='1.000ms(开环同速)')
ax[0].set_ylabel('换相间隔 (ms)'); ax[0].legend(fontsize=9); ax[0].set_title(f'事件流 {len(rows)} 个 / {SECS:.0f}s（开环 {ol}，闭环 {len(rows)-ol}）  红=间隔 灰虚线=开环基准')
if iv: ax[1].plot(tt,[60/(v*6/1000)/6 for v in iv],lw=.7,color='#2980b9')
ax[1].axhline(1670,color='#27ae60',ls='--',lw=1,label='开环 1670 转/分'); ax[1].set_ylabel('转速 (转/分)'); ax[1].legend(fontsize=9)
ax[2].plot([(t-ts[0])/24000 for t in ts],dut*100/2000,lw=.7,color='#16a085',label='占空比(%)')
ax[2].axhline(20.1,color='#c0392b',ls=':',label='上限 20%'); ax[2].set_ylabel('占空比 (%)'); ax[2].legend(fontsize=9)
a2=ax[2].twinx(); a2.plot([(t-ts[0])/24000 for t in ts],cur,lw=.6,color='#e67e22',alpha=.8,label='电流'); a2.set_ylabel('电流',color='#e67e22')
ax[3].step([(t-ts[0])/24000 for t in ts],stg,where='post',lw=.7,color='#8e44ad'); ax[3].set_ylim(-.2,1.2)
ax[3].set_yticks([0,1]); ax[3].set_yticklabels(['闭环','开环']); ax[3].set_ylabel('阶段'); ax[3].set_xlabel('时间 (s)')
for A in ax: A.grid(alpha=.25)
plt.tight_layout(); pngp=csvp.replace('.csv','.png'); plt.savefig(pngp,dpi=130)
print(f'图  → {pngp}')
