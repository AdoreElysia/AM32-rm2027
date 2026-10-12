#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""evt-ci2.py —— ci 与 waitTime 同一坐标（都换算成 ms）+ 放大看抖动。
用法: python3 tools/evt-ci2.py <evt_run_*.csv> [输出目录]"""
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
TICK=0.0005
stage=A[:,7]&1; wait=A[:,4]*TICK; ci=A[:,11]*TICK; tzc=A[:,12]*TICK; duty=A[:,5]; cur=A[:,6]
# 换相间隔（实测）
seq,ts=A[:,0],A[:,1]; ivp=np.full(len(A),np.nan)
for k in range(1,len(A)):
    d=ts[k]-ts[k-1]
    if seq[k]-seq[k-1]==1 and 0<d<24000: ivp[k]=d/24.0
H=[k for k in range(1,len(A)) if stage[k]!=stage[k-1] and stage[k]==0]
H=H[-1] if H else 0
x=np.arange(len(A))
fig,axs=plt.subplots(4,1,figsize=(17,13),sharex=True)
axs[0].plot(x,ci,'-o',ms=2.5,lw=.9,color='#c0392b',label='ci（电调认为的步长）')
axs[0].plot(x,wait,'-o',ms=2.5,lw=.9,color='#8e44ad',label='waitTime（换相点=ci/2）')
axs[0].plot(x,tzc,'-',lw=.7,color='#2980b9',alpha=.75,label='thiszctime（实际量到的过零间隔）')
axs[0].axhline(6.5,color='#c0392b',ls='--',lw=1.2,label='ci 天花板 6.5ms')
axs[0].axvline(H,color='k',ls='-.',lw=1.2,alpha=.6)
axs[0].set_ylabel('ms'); axs[0].legend(fontsize=9,ncol=4)
axs[0].set_title(f'同一坐标：ci / waitTime / 实测过零间隔（{os.path.basename(CSVP)}）   竖线=交棒')
axs[1].plot(x,ivp,'-o',ms=2.5,lw=.8,color='#c0392b'); axs[1].axhline(1.0,color='gray',ls='--',lw=1,label='1.000ms(开环)')
axs[1].set_ylabel('换相间隔 (ms)'); axs[1].legend(fontsize=9)
# 放大：闭环段
if (stage==0).sum()>20:
    k=np.where(stage==0)[0]
    axs[2].plot(k,ivp[k],'-o',ms=4,lw=.8,color='#c0392b')
    axs[2].set_ylabel('间隔(ms) 闭环放大'); axs[2].set_title('闭环段放大（看抖动：步与步之间的散）',fontsize=10)
    axs[3].plot(k,ci[k],'-o',ms=4,lw=.8,color='#c0392b',label='ci'); axs[3].plot(k,wait[k],'-o',ms=4,lw=.8,color='#8e44ad',label='waitTime')
    axs[3].set_ylabel('ms 闭环放大'); axs[3].legend(fontsize=9)
else:
    axs[2].plot(x,ci,'-',color='#c0392b'); axs[3].plot(x,wait,'-',color='#8e44ad')
for a in axs: a.grid(alpha=.25)
axs[3].set_xlabel('第 n 次换相')
plt.tight_layout(); p=os.path.join(OUTD,'ci与wait同一坐标.png'); plt.savefig(p,dpi=120); print(f'图 → {p}')
# 文字：闭环段的稳定性
if (stage==0).sum()>20:
    k=np.where(stage==0)[0]; v=ivp[k]; v=v[np.isfinite(v)]
    print(f"\n闭环段 换相间隔: 中位 {np.median(v):.2f}ms  P10 {np.percentile(v,10):.2f}  P90 {np.percentile(v,90):.2f}  最大 {v.max():.2f}")
    print(f"  步与步的变化(相邻差): 中位 {np.median(np.abs(np.diff(v))):.3f}ms  最大 {np.abs(np.diff(v)).max():.2f}ms")
    print(f"  ⇒ 抖动 = 相邻差/中位 = {100*np.median(np.abs(np.diff(v)))/np.median(v):.1f}%（步与步）")
    c=ci[k]; print(f"闭环段 ci: 首 {c[0]:.2f} 末 {c[-1]:.2f}ms   中位 {np.median(c):.2f}")
    w=wait[k]; print(f"闭环段 waitTime: 首 {w[0]:.3f} 末 {w[-1]:.3f}ms  中位 {np.median(w):.3f}   (ci/2 应等于 {np.median(c)/2:.3f})")
