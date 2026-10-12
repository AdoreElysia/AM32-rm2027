#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""evt-whole.py —— 整轮事件流的"全过程"图（用重建的可信时间轴）。
用法: python3 tools/evt-whole.py <evt_run_*.csv> [输出目录]
时间轴不用 ts（固件的 ts 会跳），改用：time += (seq差) x (邻域步长中位)"""
import sys, os, csv
import numpy as np
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
for _f in ('Noto Sans CJK SC','Noto Sans CJK JP'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family']=_f; break
plt.rcParams['axes.unicode_minus']=False
CSVP=sys.argv[1]; OUTD=sys.argv[2] if len(sys.argv)>2 else '/home/lpl/桌面/RM2027'
A=np.array([tuple(int(x) for x in r) for r in list(csv.reader(open(CSVP)))[1:]])
m=np.array([(1<=r[3]<=6 and 0<=r[5]<=2000 and abs(r[2])<3000 and 0<=r[6]<400) for r in A]); A=A[m]
seq,ts,step,wait,duty,cur=A[:,0],A[:,1],A[:,3],A[:,4],A[:,5],A[:,6]
stage=A[:,7]&1
n=len(A)
ivp=np.full(n,np.nan)
for k in range(1,n):
    d=ts[k]-ts[k-1]
    if seq[k]-seq[k-1]==1 and 0<d<24000: ivp[k]=d/24.0
W=80; t=np.zeros(n)
for k in range(1,n):
    lo,hi=max(1,k-W),min(n,k+W); loc=ivp[lo:hi]; loc=loc[np.isfinite(loc)]
    sw=np.median(loc) if len(loc) else 1.0
    gap=seq[k]-seq[k-1]
    if gap<=0 or gap>500: gap=1
    t[k]=t[k-1]+gap*sw/1000.0
rpm=np.where(ivp>0, 60/(ivp*6/1000)/6, np.nan)
print(f"重建时间轴总长 {t[-1]:.2f} 秒（{n} 个事件）")
chg=[k for k in range(1,n) if stage[k]!=stage[k-1]]
print(f"阶段切换点(事件号) {chg[:6]}  对应时刻 " + ", ".join(f"{t[k]:.2f}s" for k in chg[:6]))
fig,axs=plt.subplots(5,1,figsize=(17,14),sharex=True)
axs[0].plot(t,rpm,'-',lw=.8,color='#c0392b'); axs[0].axhline(1670,color='#27ae60',ls='--',lw=1,label='开环 1670 转/分')
axs[0].set_ylabel('转速 (转/分)'); axs[0].legend(fontsize=9); axs[0].set_title(f'整轮全过程（{os.path.basename(CSVP)}，重建时间轴，共 {t[-1]:.1f} 秒）')
axs[1].plot(t,ivp,'-',lw=.8,color='#2980b9'); axs[1].axhline(1.0,color='gray',ls='--',lw=1,label='1.000ms')
axs[1].set_ylabel('换相间隔 (ms)'); axs[1].legend(fontsize=9)
axs[2].plot(t,duty*100/2000,'-',lw=.8,color='#16a085',label='占空比(%)')
axs[2].axhline(20.1,color='#c0392b',ls=':',lw=1,label='20% 上限'); axs[2].set_ylabel('占空比 (%)'); axs[2].legend(fontsize=9,loc='upper left')
a2=axs[2].twinx(); a2.plot(t,cur,'-',lw=.6,color='#e67e22',alpha=.8,label='电流'); a2.set_ylabel('电流',color='#e67e22')
axs[3].plot(t,wait,'-',lw=.8,color='#8e44ad'); axs[3].axhline(6500,color='#c0392b',ls='--',lw=1,label='天花板 6500 拍')
axs[3].set_ylabel('waitTime (拍)'); axs[3].legend(fontsize=9)
axs[4].step(t,stage,where='post',lw=.9,color='#c0392b'); axs[4].set_ylim(-.2,1.2)
axs[4].set_yticks([0,1]); axs[4].set_yticklabels(['闭环','开环']); axs[4].set_ylabel('阶段'); axs[4].set_xlabel('时间 (秒)（重建）')
for k in chg[:3]:
    for a in axs: a.axvline(t[k],color='k',ls='-.',lw=1,alpha=.5)
for a in axs: a.grid(alpha=.25)
plt.tight_layout(); p=os.path.join(OUTD,'整轮全过程.png'); plt.savefig(p,dpi=120); print(f'图 → {p}')
# 逐秒表
print(f"\n{'时刻':>7}{'转速':>7}{'间隔ms':>8}{'duty%':>7}{'waitTime':>9}{'电流':>6}  阶段")
for s in range(int(t[-1])+1):
    k=(t>=s)&(t<s+1)
    if k.sum()<3: continue
    print(f"{s:5d}-{s+1:<2d}{np.nanmedian(rpm[k]):7.0f}{np.nanmedian(ivp[k]):8.2f}{np.median(duty[k])*100/2000:7.1f}{np.median(wait[k]):9.0f}{np.median(cur[k]):6.0f}  {'开环' if np.median(stage[k])==1 else '闭环'}")
