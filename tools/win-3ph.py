#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""win-3ph.py —— 画窗口里的 VA/VB/VC/V0 三相原始电压 + 自检 V0 vs (VA+VB+VC)/3。
用法: python3 tools/win-3ph.py <evt_run_*_win.npz> [输出目录] [画几片=6]"""
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
NP=int(sys.argv[3]) if len(sys.argv)>3 else 6
z=np.load(NPZ); n=len(z['seq']); FS=24.0
va,vb,vc,v0,step=z['va'],z['vb'],z['vc'],z['v0'],z['step']
print(f"{n} 片；每片 {va.shape[1]} 点 = {va.shape[1]/FS:.1f} ms")
print(f"{'片号':>6} {'VA范围':>13} {'VB范围':>13} {'VC范围':>13} {'V0中位':>7} {'三相均值中位':>12} {'|V0-均值|':>10}")
for i in range(0,n,max(1,n//10)):
    A,B,C,V=va[i],vb[i],vc[i],v0[i]
    mean3=(A.astype(float)+B+C)/3
    print(f"{int(z['seq'][i]):6d} {A.min():6d}~{A.max():<6d} {B.min():6d}~{B.max():<6d} {C.min():6d}~{C.max():<6d} {np.median(V):7.0f} {np.median(mean3):12.0f} {abs(np.median(V)-np.median(mean3)):10.0f}")
idx=np.linspace(0,n-1,NP).astype(int)
fig,axs=plt.subplots(NP,2,figsize=(16,2.6*NP),gridspec_kw={'width_ratios':[2,1]})
for r,i in enumerate(idx):
    t=np.arange(va.shape[1])/FS
    a0=axs[r][0]; a0.plot(t,va[i],lw=1.0,color='#c0392b',label='VA')
    a0.plot(t,vb[i],lw=1.0,color='#2980b9',label='VB')
    a0.plot(t,vc[i],lw=1.0,color='#16a085',label='VC')
    a0.plot(t,v0[i],lw=1.1,color='#8e44ad',ls='--',label='V0')
    a0.axhline(0,color='gray',ls=':',lw=.7); a0.grid(alpha=.25); a0.set_ylabel('ADC 码')
    a0.set_title(f"窗 {int(z['seq'][i])}  VA/VB/VC/V0 原始电压",fontsize=10)
    if r==0: a0.legend(fontsize=8,ncol=4)
    # 右侧：V0 vs 三相均值（自检）
    a1=axs[r][1]
    mean3=(va[i].astype(float)+vb[i]+vc[i])/3
    a1.plot(t,v0[i],lw=1.0,color='#8e44ad',label='V0(实测)')
    a1.plot(t,mean3,lw=1.0,color='#e67e22',ls='--',label='(VA+VB+VC)/3')
    a1.grid(alpha=.25); a1.set_ylabel('自检'); a1.set_title(f"V0 vs 三相均值  偏差中位 {np.median(np.abs(v0[i]-mean3)):.0f} 码",fontsize=9)
    if r==0: a1.legend(fontsize=8)
    a1.set_xlabel('ms')
plt.suptitle('窗口式全速采样的三相原始电压（四路电压版）',fontsize=13)
plt.tight_layout(); p=os.path.join(OUTD,'三相波形.png'); plt.savefig(p,dpi=115); print(f'\n图 → {p}')
