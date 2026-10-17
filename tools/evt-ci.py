#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""evt-ci.py —— 专查「ci 为什么被推大 6 倍」：把滤波原料和结果画在一起。
用法: python3 tools/evt-ci.py <evt_run_*.csv> [输出目录]
关键判读：
  · thiszctime(每次过零量到的间隔) 也涨到 6ms  ⇒ 转子真慢了（问题在相位/电压）
  · thiszctime 一直在 1ms 附近而 ci 却是 6ms ⇒ 滤波/口径的锅（问题在 PeriodElapsedCallback）"""
import sys, os, csv
import numpy as np
import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
for _f in ('Noto Sans CJK SC','Noto Sans CJK JP'):
    if any(_f in f.name for f in font_manager.fontManager.ttflist):
        plt.rcParams['font.family']=_f; break
plt.rcParams['axes.unicode_minus']=False
CSVP=sys.argv[1]; OUTD=sys.argv[2] if len(sys.argv)>2 else __import__('os').environ.get('RM2027_DATA', __import__('os').path.expanduser('~/桌面/RM2027'))
A=np.array([tuple(int(x) for x in r) for r in list(csv.reader(open(CSVP)))[1:]])
m=np.array([(1<=r[3]<=6 and 0<=r[5]<=2000 and abs(r[2])<3000 and 0<=r[6]<400) for r in A]); A=A[m]
if A.shape[1]<16: print("❌ 这个 CSV 没有滤波原料列（需要 37 字节事件包的固件）"); sys.exit(1)
stage=A[:,7]&1; ci=A[:,11]; wait=A[:,4]; tzc=A[:,12]; lzc=A[:,13]; avgi=A[:,14]; adv=A[:,15]
x=np.arange(len(A))
fig,axs=plt.subplots(4,1,figsize=(17,12),sharex=True)
axs[0].plot(x,ci*0.0005,'-o',ms=2.5,lw=.8,color='#c0392b',label='ci（6步平均，实测算出的）')
axs[0].plot(x,tzc*0.0005,'-o',ms=2,lw=.6,color='#2980b9',alpha=.8,label='thiszctime（本次过零量到的间隔）')
axs[0].plot(x,avgi*0.0005,'-',lw=.8,color='#e67e22',label='average_interval')
axs[0].axhline(6.5,color='#c0392b',ls='--',lw=1,label='天花板 6.5ms')
axs[0].set_ylabel('ms'); axs[0].legend(fontsize=9,ncol=4); axs[0].set_title('ci 与它的"原料"（判读：谁先涨、涨到多少）')
axs[1].plot(x,wait,'-o',ms=2.5,lw=.8,color='#8e44ad',label='waitTime(拍)'); axs[1].axhline(6500,color='#c0392b',ls='--',lw=1)
axs[1].set_ylabel('waitTime (拍)'); axs[1].legend(fontsize=9)
axs[2].plot(x,adv,'-o',ms=2.5,lw=.8,color='#16a085'); axs[2].set_ylabel('advance (拍)'); axs[2].legend(fontsize=9)
axs[3].step(x,stage,where='post',lw=.9,color='#c0392b'); axs[3].set_ylim(-.2,1.2)
axs[3].set_yticks([0,1]); axs[3].set_yticklabels(['闭环','开环']); axs[3].set_ylabel('阶段'); axs[3].set_xlabel('第 n 次换相')
for a in axs: a.grid(alpha=.25)
plt.tight_layout(); p=os.path.join(OUTD,'ci溯源.png'); plt.savefig(p,dpi=120); print(f'图 → {p}')
print(f"\n{'阶段':6}{'事件':>6}{'ci(ms)':>9}{'thiszctime(ms)':>15}{'平均间隔(ms)':>13}{'waitTime拍':>11}{'advance':>9}")
for tag,msk in (('开环',stage==1),('闭环',stage==0)):
    if msk.sum()==0: continue
    print(f"{tag:6}{int(msk.sum()):6d}{np.median(ci[msk])*0.0005:9.2f}{np.median(tzc[msk])*0.0005:15.2f}{np.median(avgi[msk])*0.0005:13.2f}{np.median(wait[msk]):11.0f}{np.median(adv[msk]):9.0f}")
if (stage==0).sum()>10:
    k=np.where(stage==0)[0]
    print(f"\n闭环段起始: ci={ci[k[0]]*0.0005:.2f}ms thiszctime={tzc[k[0]]*0.0005:.2f}ms")
    print(f"闭环段末尾: ci={ci[k[-1]]*0.0005:.2f}ms thiszctime={tzc[k[-1]]*0.0005:.2f}ms")
    print(f"  ⇒ 若这两条同步变大 ⇒ 转子真慢了；若 ci 明显大于 thiszctime ⇒ 滤波/口径问题")
