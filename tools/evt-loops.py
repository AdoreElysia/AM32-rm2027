#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""evt-loops.py —— 看"三只捏占空比的手"和"相位"随时间的走向。
用法: python3 tools/evt-loops.py <evt_run_*.csv> [输出目录]
需要 29 字节事件包（含 iov/startup_max/curlim_adj/ci 四列）。"""
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
if A.shape[1]<12: print("❌ 这个 CSV 没有环变量列（需要 29 字节事件包的固件）"); sys.exit(1)
seq,ts,step,wait,duty=A[:,0],A[:,1],A[:,3],A[:,4],A[:,5]
stage=A[:,7]&1; iov, smax, uca, ci = A[:,8], A[:,9], A[:,10], A[:,11]
x=np.arange(len(A))
iv=np.full(len(A),np.nan)
for k in range(1,len(A)):
    d=ts[k]-ts[k-1]
    if seq[k]-seq[k-1]==1 and 0<d<24000: iv[k]=d/24.0
phase=np.where(ci>0, wait/np.maximum(ci,1), np.nan)   # 相位比 = waitTime/ci
fig,axs=plt.subplots(6,1,figsize=(17,16),sharex=True)
axs[0].plot(x,60/(iv*6/1000)/6,'-o',ms=2.5,lw=.7,color='#c0392b'); axs[0].axhline(1670,color='#27ae60',ls='--',lw=1,label='开环 1670')
axs[0].set_ylabel('转速 (转/分)'); axs[0].legend(fontsize=9); axs[0].set_title('闭环时序：谁在拉慢、谁在顶着天花板')
axs[1].plot(x,duty,'-o',ms=2.5,lw=.7,color='#16a085',label='duty(实际)')
axs[1].plot(x,iov,'-',lw=.9,color='#2980b9',label='速度环输出 input_ov')
axs[1].plot(x,smax,'-',lw=.9,color='#c0392b',label='起步天花板 startup_max')
axs[1].plot(x,uca,'-',lw=.9,color='#e67e22',label='电流环夹子')
axs[1].axhline(450,color='gray',ls=':',lw=1,label='CL_INPUT_CAP 450')
axs[1].set_ylabel('占空比刻度 (0~2000)'); axs[1].legend(fontsize=8,ncol=5)
axs[2].plot(x,phase,'-o',ms=2.5,lw=.7,color='#8e44ad'); axs[2].axhline(0.5,color='#27ae60',ls='--',lw=1,label='理想 0.50')
axs[2].set_ylabel('相位比 waitTime/ci'); axs[2].legend(fontsize=9)
axs[3].plot(x,ci*0.0005,'-o',ms=2.5,lw=.7,color='#2c3e50'); axs[3].axhline(6.5,color='#c0392b',ls='--',lw=1,label='天花板 6.5ms')
axs[3].set_ylabel('ci (ms)'); axs[3].legend(fontsize=9)
axs[4].plot(x,iv,'-o',ms=2.5,lw=.7,color='#c0392b'); axs[4].axhline(1.0,color='gray',ls='--',lw=1)
axs[4].set_ylabel('换相间隔 (ms)')
axs[5].step(x,stage,where='post',lw=.8,color='#8e44ad'); axs[5].set_ylim(-.2,1.2)
axs[5].set_yticks([0,1]); axs[5].set_yticklabels(['闭环','开环']); axs[5].set_ylabel('阶段'); axs[5].set_xlabel('第 n 次换相')
for a in axs: a.grid(alpha=.25)
plt.tight_layout(); p=os.path.join(OUTD,'闭环时序.png'); plt.savefig(p,dpi=120); print(f'图 → {p}')
print(f"\n{'阶段':6}{'事件':>6}{'转速':>7}{'相位比':>7}{'ci(ms)':>8}{'duty':>6}{'速度环':>7}{'天花板':>7}{'电流夹子':>8}")
for tag,msk in (('开环',stage==1),('闭环',stage==0)):
    if msk.sum()==0: continue
    print(f"{tag:6}{int(msk.sum()):6d}{60/(np.nanmedian(iv[msk])*6/1000)/6:7.0f}{np.nanmedian(phase[msk]):7.3f}{np.median(ci[msk])*0.0005:8.2f}{np.median(duty[msk]):6.0f}{np.median(iov[msk]):7.0f}{np.median(smax[msk]):7.0f}{np.median(uca[msk]):8.0f}")
