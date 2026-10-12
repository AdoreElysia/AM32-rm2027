#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""win-analyze.py —— 分析【窗口式全速采样】的波形（每片 512 点 @24kHz = 21.3ms 连续）。
用法: python3 tools/win-analyze.py <evt_run_*_win.npz> [输出目录]
对每片测：换相步宽 / 过零次数 / 过零->换相延迟占步宽比(相位) / delta 幅度 / 步号是否连续
出两张图：① 若干片的波形网格（delta + 步号 + 过零/换相标记）② 各片指标随片号变化"""
import sys, os
import numpy as np
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
for _f in ('Noto Sans CJK SC','Noto Sans CJK JP'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family']=_f; break
plt.rcParams['axes.unicode_minus']=False
NPZ=sys.argv[1]; OUTD=sys.argv[2] if len(sys.argv)>2 else '/home/lpl/桌面/RM2027'
z=np.load(NPZ); d=z['d']; step=z['step']; wseq=z['seq']; nwin=len(d)
FS=24.0  # kHz
print(f"窗口 {nwin} 片，每片 {d.shape[1]} 点 = {d.shape[1]/FS:.1f} ms")
# 每片指标
rows=[]
for i in range(nwin):
    di=d[i].astype(float); st=step[i].astype(int)
    ch=np.where(np.diff(st)!=0)[0]+1                 # 换相点
    iv=[(ch[k]-ch[k-1])/FS for k in range(1,len(ch))]  # 步宽 ms
    # 过零：变号 + 斜率上限 + 回看2点单调（跟固件判据同源）
    zc=[]
    for k in range(3,len(di)):
        if abs(di[k]-di[k-1])<=100 and ((di[k-1]<=0<di[k]) or (di[k-1]>=0>di[k])):
            m=di[k-3:k+1]
            if (np.all(np.diff(m)>0) or np.all(np.diff(m)<0)): zc.append(k)
    lat=[]
    for c in ch:
        prev=[p for p in zc if p<c]
        if prev and len(ch)>1:
            pw=[ch[k]-ch[k-1] for k in range(1,len(ch)) if ch[k]<=c]
            if pw: lat.append((c-prev[-1])/pw[-1])
    rows.append(dict(w=int(wseq[i]), nstep=len(ch),
                     step_ms=(np.median(iv) if iv else np.nan),
                     nzc=len(zc), ratio=(np.median(lat) if lat else np.nan),
                     amp=int(np.percentile(np.abs(di),95)), amp_ptp=int(di.max()-di.min()),
                     stepok=bool(np.all(np.diff(st[st>0])!=0)) if len(st) else True))
good=[r for r in rows if np.isfinite(r['step_ms'])]
print(f"\n{'片号':>6} {'步宽ms':>8} {'步数':>5} {'过零':>5} {'过零->换相/步宽':>16} {'|delta|P95':>10} {'幅度峰峰':>8}")
for r in rows[::max(1,nwin//14)]:
    print(f"{r['w']:6d} {r['step_ms']:8.3f} {r['nstep']:5d} {r['nzc']:5d} {r['ratio']:16.3f} {r['amp']:10d} {r['amp_ptp']:8d}")
if good:
    sm=np.array([r['step_ms'] for r in good]); rt=np.array([r['ratio'] for r in good if np.isfinite(r['ratio'])])
    am=np.array([r['amp_ptp'] for r in good]); nz=np.array([r['nzc'] for r in good])
    print(f"\n=== 汇总（{len(good)} 片有效）===")
    print(f"  步宽    中位 {np.median(sm):.3f}ms  P10 {np.percentile(sm,10):.3f}  P90 {np.percentile(sm,90):.3f}  (21.3ms/片 ⇒ 步数 ≈ {21.3/np.median(sm):.1f})")
    print(f"  相位比  中位 {np.median(rt):.3f}  (理想 0.50 = 过零后 30° 换相)")
    print(f"  幅度峰峰 中位 {np.median(am):.0f} 码   P10 {np.percentile(am,10):.0f}  P90 {np.percentile(am,90):.0f}")
    print(f"  过零数  中位 {np.median(nz):.0f} 次/片 (每 60° 一次 ⇒ 理想 ≈ 步数)")
# 图① 波形网格：等间隔取 9 片
idx=np.linspace(0,nwin-1,9).astype(int)
fig,axs=plt.subplots(3,3,figsize=(17,10))
for a,i in zip(axs.ravel(),idx):
    t=np.arange(d.shape[1])/FS
    a.plot(t,d[i],lw=1.0,color='#2c3e50')
    a.axhline(0,color='gray',ls=':',lw=.7)
    st=step[i].astype(int); a.step(t,st*40-200,where='post',lw=.8,color='#8e44ad',alpha=.6)
    ch=np.where(np.diff(st)!=0)[0]+1
    for c in ch: a.axvline(t[c],color='#c0392b',ls='--',lw=.7,alpha=.7)
    a.set_title(f"窗 {int(wseq[i])}  步宽{rows[i]['step_ms']:.2f}ms  {rows[i]['nstep']}步  相位{rows[i]['ratio']:.2f}  幅度{rows[i]['amp_ptp']}",fontsize=9)
    a.grid(alpha=.25); a.set_xlabel('ms'); a.set_ylabel('delta(码)')
plt.suptitle('窗口式全速采样的波形（每片 21.3ms 连续；紫=步号×40-200；红虚线=换相点）',fontsize=12)
plt.tight_layout(); p1=os.path.join(OUTD,'波形_网格.png'); plt.savefig(p1,dpi=115); print(f'\n图① → {p1}')
# 图② 指标随片号
fig,axs=plt.subplots(4,1,figsize=(16,11),sharex=True)
ws=[r['w'] for r in rows]
axs[0].plot(ws,[r['step_ms'] for r in rows],'-o',ms=3,lw=.8,color='#c0392b'); axs[0].axhline(1.0,color='gray',ls='--',lw=1)
axs[0].set_ylabel('换相步宽 (ms)'); axs[0].grid(alpha=.25)
axs[1].plot(ws,[r['ratio'] for r in rows],'-o',ms=3,lw=.8,color='#2980b9'); axs[1].axhline(0.5,color='#27ae60',ls='--',lw=1)
axs[1].set_ylabel('过零→换相 / 步宽'); axs[1].grid(alpha=.25)
axs[2].plot(ws,[r['amp_ptp'] for r in rows],'-o',ms=3,lw=.8,color='#16a085')
axs[2].set_ylabel('delta 幅度峰峰 (码)'); axs[2].grid(alpha=.25)
axs[3].plot(ws,[r['nzc'] for r in rows],'-o',ms=3,lw=.8,color='#8e44ad'); axs[3].set_ylabel('过零次数/片'); axs[3].grid(alpha=.25)
axs[3].set_xlabel('窗口序号（越大越晚）')
plt.tight_layout(); p2=os.path.join(OUTD,'波形_指标.png'); plt.savefig(p2,dpi=120); print(f'图② → {p2}')
