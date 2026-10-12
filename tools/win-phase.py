#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""win-phase.py —— 从四路波形里量【相位】：过零点、换相点、过零->换相延迟/步宽、BEMF 幅度。
用法: python3 tools/win-phase.py <evt_run_*_win.npz> [输出目录]
悬空相按 step 选：step1/4->C, 2/5->A, 3/6->B（与固件一致）；delta = 悬空相 − V0。"""
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
va,vb,vc,v0,step=z['va'],z['vb'],z['vc'],z['v0'],z['step']
rows=[]
for i in range(n):
    st=step[i].astype(int); A=va[i].astype(float); B=vb[i].astype(float); C=vc[i].astype(float); V=v0[i].astype(float)
    fl=np.where((st==1)|(st==4), C, np.where((st==2)|(st==5), A, B))   # 悬空相
    d=fl-V                                                            # delta
    ch=np.where(np.diff(st)!=0)[0]+1                                  # 换相点
    if len(ch)<3: 
        rows.append(dict(w=int(z['seq'][i]), n=len(ch), sw=np.nan, ps=np.nan, nzc=0, amp=np.nan)); continue
    iv=[(ch[k]-ch[k-1])/FS for k in range(1,len(ch))]
    # 过零：变号 + 斜率≤100 + 回看2点单调（与固件判据同源）
    zc=[]
    for k in range(3,len(d)):
        if abs(d[k]-d[k-1])<=100 and ((d[k-1]<=0<d[k]) or (d[k-1]>=0>d[k])):
            m=d[k-3:k+1]
            if np.all(np.diff(m)>0) or np.all(np.diff(m)<0): zc.append(k)
    lat=[]
    for c in ch:
        p=[q for q in zc if q<c]
        pw=[ch[k]-ch[k-1] for k in range(1,len(ch)) if ch[k]<=c]
        if p and pw: lat.append((c-p[-1])/pw[-1])
    rows.append(dict(w=int(z['seq'][i]), n=len(ch), sw=float(np.median(iv)),
                     ps=float(np.median(lat)) if lat else np.nan, nzc=len(zc),
                     amp=float(d.max()-d.min()) if len(d) else np.nan))
good=[r for r in rows if np.isfinite(r['sw'])]
fast=[r for r in good if r['sw']<2.0]; slow=[r for r in good if r['sw']>=2.0]
print(f"{n} 片；有效 {len(good)} 片（其余在大段静止/无换相）")
print(f"  步宽 <2ms（≈开环区间，快）: {len(fast)} 片")
print(f"  步宽 ≥2ms（≈闭环区间，慢）: {len(slow)} 片")
for tag,g in (('全部',good),('快档(≈开环)',fast),('慢档(≈闭环)',slow)):
    if not g: continue
    sw=np.array([r['sw'] for r in g]); ps=np.array([r['ps'] for r in g if np.isfinite(r['ps'])])
    amp=np.array([r['amp'] for r in g]); nz=np.array([r['nzc'] for r in g])
    print(f"\n--- {tag}（{len(g)} 片）---")
    print(f"  步宽  中位 {np.median(sw):.3f}ms ⇒ {60/(np.median(sw)*6/1000)/6:.0f} 转/分")
    print(f"  过零数 中位 {np.median(nz):.0f} 次/片（21.3ms 里）")
    print(f"  相位比 中位 {np.median(ps) if len(ps) else float('nan'):.3f}   (理想 0.50 = 过零后 30° 换相；<0.5 提前, >0.5 滞后)")
    print(f"  BEMF 幅度峰峰 中位 {np.median(amp):.0f} 码  P10 {np.percentile(amp,10):.0f}  P90 {np.percentile(amp,90):.0f}")
# 逐片打印
print(f"\n{'片号':>6} {'步宽ms':>7} {'转速':>6} {'过零':>5} {'相位比':>7} {'幅度':>6}  阶段")
for r in rows[::max(1,n//22)]:
    if not np.isfinite(r['sw']): print(f"{r['w']:6d} {'—':>7} {'—':>6} {r['nzc']:5d} {'—':>7} {'—':>6}  静止"); continue
    tag='开环区' if r['sw']<2.0 else '闭环区'
    print(f"{r['w']:6d} {r['sw']:7.3f} {60/(r['sw']*6/1000)/6:6.0f} {r['nzc']:5d} {r['ps']:7.3f} {r['amp']:6.0f}  {tag}")
# 图
fig,axs=plt.subplots(4,1,figsize=(16,11),sharex=True)
ws=[r['w'] for r in rows]
axs[0].plot(ws,[r['sw'] for r in rows],'-o',ms=3,lw=.8,color='#c0392b'); axs[0].axhline(2.0,color='gray',ls='--',lw=1,label='2ms 分界(开环/闭环)')
axs[0].set_ylabel('步宽 (ms)'); axs[0].legend(fontsize=9); axs[0].grid(alpha=.25)
axs[1].plot(ws,[r['ps'] for r in rows],'-o',ms=3,lw=.8,color='#2980b9'); axs[1].axhline(0.5,color='#27ae60',ls='--',lw=1,label='理想 0.5')
axs[1].set_ylabel('过零→换相 / 步宽'); axs[1].legend(fontsize=9); axs[1].grid(alpha=.25)
axs[2].plot(ws,[r['amp'] for r in rows],'-o',ms=3,lw=.8,color='#16a085')
axs[2].set_ylabel('BEMF 幅度峰峰 (码)'); axs[2].grid(alpha=.25)
axs[3].plot(ws,[r['nzc'] for r in rows],'-o',ms=3,lw=.8,color='#8e44ad'); axs[3].set_ylabel('过零数/片'); axs[3].grid(alpha=.25)
axs[3].set_xlabel('窗口序号（越大越晚）')
plt.suptitle('四路波形量出的相位（悬空相按步号选）',fontsize=13)
plt.tight_layout(); p=os.path.join(OUTD,'相位分析.png'); plt.savefig(p,dpi=120); print(f'\n图 → {p}')
