#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""evt-phases.py —— 从事件流 CSV 出两张图 + 分析：① 整体（有效阶段）② 各阶段典型放大。
用法: python3 tools/evt-phases.py <evt_run_*.csv> [输出目录]
纵坐标给足: 换相间隔/转速/占空比/电流/waitTime/delta/步号/阶段。
★ x 轴一律用【事件序号】—— 固件的 ts(adc_frame_total) 会跳几万帧，当时间轴会把图画歪。"""
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
rows=[tuple(int(x) for x in r) for r in list(csv.reader(open(CSVP)))[1:]]
# ★ 过滤"窗口包被误读成事件帧"的假帧（判据抄 evt-plot.py）
def _ok(r):
    _s,_t,d,st,_w,du,c,_g=r
    return 1<=st<=6 and 0<=du<=2000 and abs(d)<3000 and 0<=c<400
_n0=len(rows); rows=[r for r in rows if _ok(r)]; print(f'读入 {_n0} → 有效 {len(rows)}（剔除假帧 {_n0-len(rows)}）')
seq=np.array([r[0] for r in rows]); ts=np.array([r[1] for r in rows])
dl=np.array([r[2] for r in rows]); stp=np.array([r[3] for r in rows])
wt=np.array([r[4] for r in rows]); dut=np.array([r[5] for r in rows],float)
cur=np.array([r[6] for r in rows],float); stg=np.array([r[7] for r in rows])
n=len(rows); x=np.arange(n)
rpm=np.full(n,np.nan); iv=np.full(n,np.nan)
for k in range(1,n):
    d=ts[k]-ts[k-1]
    if seq[k]-seq[k-1]==1 and 0<d<24000:
        iv[k]=d/24.0; rpm[k]=60/((d/24.0)*6/1000)/6
# 阶段切换点
chg=[int(k) for k in range(1,n) if stg[k]!=stg[k-1]]
# ★ 交棒点要取【最后一次 开环->闭环 切换】—— 那才是真正进闭环、并且会塌陷的那一次。
#   （第一次切换可能只是 2 个事件的瞬时闪现，拿它分栏会把四张图全错位。）
_cand=[k for k in chg if stg[k]==0]
hand=_cand[-1] if _cand else None
print(f"事件 {n} 个；阶段切换点(事件序号) {chg}；交棒点={hand}")
if hand:
    print(f"  交棒(开环->闭环)点 = 第 {hand} 个事件；开环 {int((stg==1).sum())} 个 / 闭环 {int((stg==0).sum())} 个")
ci=(stg==0)
print(f"\n=== 开环段 vs 闭环段 ===")
for nm,mask in (('开环',stg==1),('闭环',stg==0)):
    m=mask&np.isfinite(iv)
    if m.sum():
        v=iv[m]
        print(f"  {nm}: n={m.sum():4d}  间隔中位 {np.median(v):.3f}ms  转速中位 {60/(np.median(v)*6/1000)/6:4.0f}   "
              f"P90 {np.sort(v)[int(len(v)*.9)]:6.3f}ms  长步占比 {100*np.mean(v>1.2*np.median(v)):4.1f}%   "
              f"duty中位 {np.median(dut[mask]):.0f}({np.median(dut[mask])*100/2000:.1f}%)  cur中位 {np.median(cur[mask]):.0f}  "
              f"wait中位 {np.median(wt[mask]):.0f}")
# 步号缺步（丢换相）
bad=[k for k in range(1,n) if stp[k]!=(stp[k-1]%6)+1]
print(f"\n步号不按 1->6 循环的地方: {len(bad)} 处（每处=丢了一次换相或错相）")
# 长步分位
vc=iv[ci&np.isfinite(iv)]
if vc.size:
    for q in (0.5,0.75,0.9,0.95,0.99,1.0):
        print(f"   闭环间隔 P{int(q*100):2d} = {np.sort(vc)[min(int(vc.size*q),vc.size-1)]:.3f} ms", end='')
    print()
# 长步的间隔分布（周期性？）
longi=[k for k in range(1,n) if np.isfinite(iv[k]) and ci[k] and iv[k]>2*np.median(vc)]
if len(longi)>3:
    d=np.diff(longi)
    print(f"   闭环长步(>2x中位) {len(longi)} 个，其序号间隔: 中位 {int(np.median(d))}  众数 {int(np.bincount(d).argmax())}  范围 {d.min()}~{d.max()}")
    from collections import Counter
    print(f"   长步所在步号分布: {dict(Counter(stp[longi].tolist()))}   非长步步号分布: {dict(Counter(stp[ci&np.isfinite(iv)].tolist()))}")

def panels(axs, sl, title):
    i0,i1=sl; xs=x[i0:i1]
    axs[0].plot(xs,iv[i0:i1],'-o',ms=2.5,lw=.8,color='#c0392b'); axs[0].axhline(np.nanmedian(iv[i0:i1]),color='gray',ls='--',lw=.8)
    axs[0].set_ylabel('换相间隔 (ms)'); axs[0].set_title(title)
    axs[1].plot(xs,rpm[i0:i1],'-o',ms=2.5,lw=.8,color='#2980b9'); axs[1].axhline(1670,color='#27ae60',ls='--',lw=.8)
    axs[1].set_ylabel('转速 (转/分)')
    axs[2].plot(xs,dut[i0:i1]*100/2000,'-o',ms=2.5,lw=.8,color='#16a085'); axs[2].set_ylabel('占空比 (%)')
    a2=axs[2].twinx(); a2.plot(xs,cur[i0:i1],'-o',ms=2,lw=.7,color='#e67e22'); a2.set_ylabel('电流',color='#e67e22')
    axs[3].plot(xs,wt[i0:i1],'-o',ms=2.5,lw=.8,color='#8e44ad'); axs[3].set_ylabel('waitTime (拍)')
    axs[4].plot(xs,dl[i0:i1],'-o',ms=2.5,lw=.8,color='#2c3e50'); axs[4].axhline(0,color='gray',lw=.7,ls=':')
    axs[4].set_ylabel('delta (ADC码)')
    axs[5].step(xs,stp[i0:i1],where='post',lw=.9,color='#8e44ad'); axs[5].set_ylim(.5,6.5); axs[5].set_ylabel('步号')
    axs[6].step(xs,stg[i0:i1],where='post',lw=.9,color='#c0392b'); axs[6].set_ylim(-.2,1.2)
    axs[6].set_yticks([0,1]); axs[6].set_yticklabels(['闭环','开环']); axs[6].set_ylabel('阶段')
    for A in axs: A.grid(alpha=.25)
    axs[-1].set_xlabel('第 n 次换相（事件序号）')

# ① 整体
fig,axs=plt.subplots(7,1,figsize=(18,15),sharex=True)
panels(axs,(0,n),f'整体（有效事件 {n} 个）  交棒点≈第 {hand} 个事件')
p1=os.path.join(OUTD,'分析_整体.png'); plt.tight_layout(); plt.savefig(p1,dpi=120); print(f'\n图① → {p1}'); plt.close()
# ② 各阶段典型
if hand:
    W=45
    segs=[('开环·斜坡',(max(0,hand-3*W),hand-W)),
          ('交棒瞬间',(hand-W,hand+W)),
          ('闭环·稳态',(hand+W,hand+3*W)),
          ('闭环·后段',(n-2*W,n))]
else:
    segs=[('前段',(0,90)),('中段',(n//2-45,n//2+45)),('后段',(n-90,n))]
fig,axs=plt.subplots(7,len(segs),figsize=(7*len(segs),15),sharex='col')
for c,(名,(a,b)) in enumerate(segs):
    panels([axs[r][c] for r in range(7)],(max(0,a),min(n,b)),名)
p2=os.path.join(OUTD,'分析_阶段典型.png'); plt.tight_layout(); plt.savefig(p2,dpi=110); print(f'图② → {p2}')
