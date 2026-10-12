#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""win-cur.py —— 三相电压 + 电流一起看（回答"电到底加进去了没有"）。
用法: python3 tools/win-cur.py <evt_run_*_win.npz> [输出目录]"""
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
z=np.load(NPZ); n=len(z['seq']); FS=24.0
va,vb,vc,v0,cur,step=z['va'],z['vb'],z['vc'],z['v0'],z['cur'],z['step']
sw=[]
for i in range(n):
    st=step[i].astype(int); ch=np.where(np.diff(st)!=0)[0]+1
    sw.append(np.median([(ch[k]-ch[k-1])/FS for k in range(1,len(ch))]) if len(ch)>3 else np.nan)
sw=np.array(sw)
fast=np.where(np.isfinite(sw)&(sw<1.7))[0]; slow=np.where(np.isfinite(sw)&(sw>=3.5))[0]
print(f"{n} 片：快档(≈开环) {len(fast)} 片，慢档(≈闭环) {len(slow)} 片")
def stat(tag,idx):
    if not len(idx): return
    c=np.array([np.median(cur[i]) for i in idx]); r=np.array([cur[i].max()-cur[i].min() for i in idx])
    print(f"  {tag}: 电流中位 {np.median(c):.0f} 码   幅摆中位 {np.median(r):.0f} 码")
stat('快档(开环)',fast); stat('慢档(闭环)',slow)
rows=[]
for tag,idx in (('开环·快',fast),('闭环·慢',slow)):
    for i in (idx[:1] if len(idx) else []):
        rows.append((tag,int(z['seq'][i]),i))
for tag,idx in (('开环·快',fast),('闭环·慢',slow)):
    for i in (idx[len(idx)//2:len(idx)//2+1] if len(idx) else []):
        rows.append((tag,int(z['seq'][i]),i))
if not rows: rows=[('任意',int(z['seq'][0]),0)]
fig,axs=plt.subplots(len(rows),2,figsize=(16,2.9*len(rows)),gridspec_kw={'width_ratios':[2,1]})
if len(rows)==1: axs=[axs]
for r,(tag,w,i) in enumerate(rows):
    t=np.arange(cur.shape[1])/FS
    a=axs[r][0]
    a.plot(t,va[i],lw=1.0,color='#c0392b',label='VA'); a.plot(t,vb[i],lw=1.0,color='#2980b9',label='VB')
    a.plot(t,vc[i],lw=1.0,color='#16a085',label='VC'); a.plot(t,v0[i],lw=1.0,color='#8e44ad',ls='--',label='V0')
    a.grid(alpha=.25); a.set_ylabel('ADC 码'); a.set_title(f"{tag}  窗 {w}  步宽 {sw[i]:.2f}ms",fontsize=10)
    if r==0: a.legend(fontsize=8,ncol=5)
    st=step[i].astype(int); ch=np.where(np.diff(st)!=0)[0]+1
    for c in ch: a.axvline(t[c],color='gray',ls=':',lw=.6)
    a2=a.twinx(); a2.plot(t,cur[i],lw=0.9,color='#e67e22',alpha=.9,label='电流'); a2.set_ylabel('电流(码)',color='#e67e22')
    if r==0: a2.legend(fontsize=8,loc='upper right')
    b=axs[r][1]
    b.plot(t,cur[i],lw=1.0,color='#e67e22'); b.grid(alpha=.25); b.set_ylabel('电流(码)')
    b.set_title(f"电流  中位 {np.median(cur[i]):.0f}  最大 {cur[i].max()}  最小 {cur[i].min()}",fontsize=9)
    b.set_xlabel('ms')
plt.suptitle('三相电压 + 电流（虚线=换相点）',fontsize=13)
plt.tight_layout(); p=os.path.join(OUTD,'三相与电流.png'); plt.savefig(p,dpi=115); print(f'\n图 → {p}')
