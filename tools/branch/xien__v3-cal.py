#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""xien/v3-cal 扩展脚本：K2a 静态 ADC 校准（会询问母线电压）。
流程：问参数 → 写 cal_bus_mv/cal_duty → cal_mode=1 → 轮询 cal_state → 读 cal_raw → 算 g/o → 打印/归档。
"""
import os, sys, json, re, time, subprocess, argparse

BRANCH=os.environ.get('ESC_BRANCH','?'); COMMIT=os.environ.get('ESC_COMMIT','?')
BIN=os.environ.get('ESC_BIN',''); SHA=os.environ.get('ESC_BIN_SHA','')
ROOT=os.environ.get('ESC_ROOT', os.getcwd())
ELF=os.path.join(ROOT,'obj','AM32_RM2027_ESC_G071_2.21.elf')
DBG=os.path.join(ROOT,'tools','esc-dbg.sh')
FP={1:'A',2:'C',3:'C',4:'B',5:'B',6:'A'}   # 每步 PWM(高侧)相
LOW={1:'B',2:'B',3:'A',4:'A',5:'C',6:'C'}   # 每步 LOW 相
FLOAT={1:'C',2:'A',3:'B',4:'C',5:'A',6:'B'}
# ★ 上面三张表必须和固件 comStep()（Mcu/g071/Src/phaseouts.c）逐条对齐，别凭印象写：
#   comStep 1:C-悬空/B-低/A-PWM   2:A-悬空/B-低/C-PWM   3:B-悬空/A-低/C-PWM
#           4:C-悬空/A-低/B-PWM   5:A-悬空/C-低/B-PWM   6:B-悬空/C-低/A-PWM
#   2026-10-19 修正：第 5/6 步原来把「PWM 相」和「悬空相」写反了（写成 5=A-PWM、6=B-PWM）。
# ★ 相 → TIM1 通道（本板，见 Inc/targets.h 的 HARDWARE_GROUP_G0_RM2027）：
#   A=CH3(PA10/PB1)  B=CH2(PA9/PB0)  C=CH1(PA8/PA7) —— 固件就是按这个给 PWM 相写 CCR 的。
CH={'A':3,'B':2,'C':1}

def sh(cmd):
    r=subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=90)
    return (r.stdout or '') + (r.stderr or '')   # ★ openocd 的 mdw 数据写在 stderr

def sym(name):
    out=sh(f"arm-none-eabi-nm -S '{ELF}' | awk '$4==\"{name}\"{{print $1}}'")
    return out.strip().split('\n')[0] if out.strip() else ''

def peek(name):
    o=sh(f"bash '{DBG}' peek {name}")
    m=re.search(r'=\s*(0x[0-9a-fA-F]+)',o)
    return int(m.group(1),16) if m else None

def poke(name,val):
    sh(f"bash '{DBG}' poke {name} {val}")

def poke16(name,val):
    sh(f"bash '{DBG}' poke16 {name} {val}")

def read_u16_array(name,n):
    a=sym(name)
    if not a: return []
    a=int(a,16)
    # ★ 纪律（同 esc-dbg.sh 第②条）：halt 之后必须喂狗（IWDG_KR @0x40003000 写 0xAAAA），
    #   本固件 IWDG≈512ms 且只在主循环/PWM 中断里喂 ⇒ 一次 halt 超过 ~0.5s 就会被复位。
    o=sh(f"openocd -f interface/stlink.cfg -f target/stm32g0x.cfg -c 'adapter speed 1800' -c init "
         f"-c halt -c 'mww 0x40003000 0xAAAA' -c 'mdw 0x{a:08X} {(n+1)//2}' "
         f"-c 'mww 0x40003000 0xAAAA' -c resume -c shutdown")
    words=[]
    for line in o.splitlines():
        m=re.match(r'^0x[0-9a-fA-F]+: (.*)',line)
        if m: words+=m.group(1).split()
    raw=b''
    for w in words:
        raw+=bytes(int(w[i:i+2],16) for i in (6,4,2,0))
    return [raw[2*i]|(raw[2*i+1]<<8) for i in range(n)]

def run_rank(bus_mv, save=False):
    """★ K3：rank 偏差校准（静态、不转电机）。
    三个驱动状态（让 A/B/C 轮流做 PWM 相）× 4 个 order × 低/高两档 = 24 个驻留期。
    只有"该 sel 里做 PWM 的那个相"有真正的两电平，它的 k 才有效；V0 也跟着变，可看。"""
    RANK_OF = {'A':[1,4,3,2], 'B':[2,1,4,3], 'C':[3,2,1,4], 'V0':[4,3,2,1]}
    CHS = ['A','B','C','V0']               # ★ 别写成 'ABCV0'：那会被拆成 V/0 两个字符
    IDX = {'A':0,'B':1,'C':2,'V0':3}
    SEL = [(0,'step1','A','C'), (1,'step2','C','A'), (2,'step3','C','B')]  # (sel,驱动状态,PWM相,悬空相)
    print('→ 启动 rank 偏差校准（静态，不转电机，24 个驻留期）…')
    poke('cal_state',0); poke('cal_mode',2)
    for _ in range(200):
        time.sleep(0.2)
        if (peek('cal_state') & 0xFF)==1: break
    if (peek('cal_state') & 0xFF)!=1:
        print('❌ rank 校准未完成'); return 1
    raw=read_u16_array('cal_rank',3*4*2*4)   # [sel][order][hi][ch]
    if len(raw)<96:
        print('❌ 读 cal_rank 失败（拿到 %d 个）'%len(raw)); return 1
    res={}
    print('\n== 每个 sel 内，该 PWM 相在各 rank 上的 (低/高) 与 k、b ==')
    for sel,stname,pwmch,_fl in SEL:
        print(f'\n[sel{sel}] 驱动={stname}（{pwmch} 相 PWM、其余低边/悬空）')
        print('  通道 |        rank1        rank2        rank3        rank4  | 增益极差')
        for ch in CHS:
            cells=[];ks=[]
            for r in (1,2,3,4):
                o=RANK_OF[ch].index(r)                    # 该通道在哪个 order 占这个 rank
                lo=raw[sel*32+o*8+0*4+IDX[ch]]
                hi=raw[sel*32+o*8+1*4+IDX[ch]]
                k=(hi-lo)/bus_mv if bus_mv else 0
                ks.append(k); cells.append(f'{lo:4d}/{hi:4d}')
            sp=(max(ks)-min(ks))/(sum(ks)/len(ks))*100 if sum(ks) else 0
            mark='  ← PWM相' if ch==pwmch else ('  ← 参考' if ch=='V0' else '')
            print(f'   {ch:3s}  | '+' '.join(cells)+f' | {sp:+5.1f}%{mark}')
            res.setdefault(ch,{})[sel]={'lo_hi':cells,'k':[round(x,4) for x in ks],'spread_pct':round(sp,1)}
    # ★ 关键分析：差分 (悬空相 − V0) 在同一驱动状态下随 order 的变化 = 纯 rank 误差
    print('\n★★ 差分分析：同一驱动状态下 (悬空相 − V0) 随 order 的变化')
    print('   同一物理状态真值恒定 ⇒ 随 order 变的量 = rank 给【判据输入】带来的误差')
    IDXv = IDX
    tab={}
    for sel,stname,pwmch,fl in SEL:
        row=[]
        for o in range(4):
            rf = RANK_OF[fl][o]; rv = RANK_OF['V0'][o]
            hf = raw[sel*32 + o*8 + 1*4 + IDXv[fl]]
            hv = raw[sel*32 + o*8 + 1*4 + IDXv['V0']]
            row.append(hf - hv)
        tab[fl]=row
        print(f'  悬空相 {fl}（{stname}）: ' + '  '.join(f'order{o}={v:+5d}' for o,v in enumerate(row))
              + f'   | 极差 {max(row)-min(row):4d}')
    print('\n   ⇒ 按 order 分组的"3 周期图案"（步1/4=C, 步2/5=A, 步3/6=B）:')
    best=None
    for o in range(4):
        g=[tab['C'][o], tab['A'][o], tab['B'][o]]
        sp=max(g)-min(g)
        print(f'     order{o}: C={g[0]:+5d}  A={g[1]:+5d}  B={g[2]:+5d}   | 三组极差(3周期误差) = {sp:4d}'
              + ('   ← 现在运行用的就是它' if o==0 else ''))
        if best is None or sp<best[1]: best=(o,sp)
    print(f'   ⇒ 三周期误差最小的 order = order{best[0]}（极差 {best[1]}）；当前 order0 的极差 = '
          f'{max([tab["C"][0],tab["A"][0],tab["B"][0]])-min([tab["C"][0],tab["A"][0],tab["B"][0]])}')
    if save:
        p=os.path.join(ROOT,'firmware',BRANCH.replace('/','__')+'.rank.json')
        open(p,'w',encoding='utf-8').write(json.dumps(
            {'bus_mv':bus_mv,'by_channel':res,'raw':raw},ensure_ascii=False,indent=2))
        print('→ 已写',p)
    return 0

def _rd(name, n_words):
    """按符号读 n_words 个 32 位字（符号地址现取）"""
    a = sym(name)
    if not a: return []
    a = int(a, 16)
    o = sh(f"openocd -f interface/stlink.cfg -f target/stm32g0x.cfg -c 'adapter speed 1800' -c init "
           f"-c halt -c 'mww 0x40003000 0xAAAA' -c 'mdw 0x{a:08X} {n_words}' "
           f"-c 'mww 0x40003000 0xAAAA' -c resume -c shutdown")
    ws = []
    for line in o.splitlines():
        m = re.match(r'^0x[0-9a-fA-F]+: (.*)', line)
        if m: ws += m.group(1).split()
    return [int(w, 16) for w in ws]

def _i16(words, i):
    w = words[i // 2]; return (w >> 16) & 0xFFFF if (i % 2) else w & 0xFFFF
def _s16(v): return v - 0x10000 if v & 0x8000 else v
def _s32(v): return v - 0x100000000 if v & 0x80000000 else v

def run_params():
    """读片上标定结果并打印（rk 因子 / 每步增益 / 偏置 / 已应用的表）"""
    print('→ 读片上参数…')
    rk  = _rd('rk_k', 3)          # int16[5]
    fA  = _rd('fit_A', 7)         # int32[7]
    fM  = _rd('fit_M', 7)
    fg  = _rd('fit_g', 4)         # int16[7]
    fo  = _rd('fit_o', 4)
    sg  = _rd('cal_sg', 4)
    so  = _rd('cal_so', 4)
    fc  = _rd('f_cnt', 4)
    st  = _rd('fit_ready', 1); ap = _rd('cal_apply', 1)
    ready = (st[0] & 0xFF) if st else -1
    applied = (ap[0] & 0xFF) if ap else -1
    print(f"fit_ready={ready}  cal_apply={applied}")
    if ready != 1:
        print('⚠ 本轮没有完成校准（fit_ready!=1）—— 检查母线是否有电、电机是否真转')
    if len(rk) >= 3:
        print('\nrank 因子（rank1..3，rank4 未用）:',
              [_s16(_i16(rk,i)) for i in range(1,4)],
              f"  k3/k1 = {_s16(_i16(rk,3))/max(1,_s16(_i16(rk,1))):.2f}")
    if len(fg) >= 4:
        g = [_s16(_i16(fg,i)) for i in range(1,7)]
        o = [_s16(_i16(fo,i)) for i in range(1,7)]
        A = [_s32(fA[i]) for i in range(1,7)] if len(fA) >= 7 else [0]*6
        M = [_s32(fM[i]) for i in range(1,7)] if len(fM) >= 7 else [0]*6
        n = [_i16(fc,i) for i in range(1,7)] if len(fc) >= 4 else [0]*6
        print('\n步 | 相 | 实例 |  A_s  |  M_s  | fit_g | fit_o')
        ph = {1:'C',2:'A',3:'B',4:'C',5:'A',6:'B'}
        for i in range(6):
            print(f' {i+1} | {ph[i+1]}  | {n[i]:4d} | {A[i]:4d} | {M[i]:+5d} | {g[i]:5d} | {o[i]:+5d}')
        if all(v>0 for v in g):
            base = sum(g[i] for i in (0,3))/2.0
            print(f'  fit_g 归一(以 C 为 1): A={((g[1]+g[4])/2)/base:.2f}  B={((g[2]+g[5])/2)/base:.2f}')
    if len(sg) >= 4:
        s_ = [_s16(_i16(sg,i)) for i in range(1,7)]
        o_ = [_s16(_i16(so,i)) for i in range(1,7)]
        print('\n★ 已应用（判据在用）:')
        print('  cal_sg = { 0,', ', '.join(str(v) for v in s_), '}')
        print('  cal_so = { 0,', ', '.join(str(v) for v in o_), '}')
        base = (s_[0]+s_[3])/2.0 or 1
        print(f'  归一(以 C 为 1): A={((s_[1]+s_[4])/2)/base:.2f}  B={((s_[2]+s_[5])/2)/base:.2f}')
        print('  对照 V2.9: 1152/1629/1325 → 归一 A=1.41 B=1.15')
    return 0

def run_bake():
    """把片上算出的 cal_sg / cal_so 写进 Inc/cal_params_gen.h（永久生效）"""
    print('→ 从片上读取校准结果…')
    sg = _rd('cal_sg', 4); so = _rd('cal_so', 4)
    st = _rd('fit_ready', 1)
    if len(sg) < 4 or len(so) < 4:
        print('❌ 读不到 cal_sg / cal_so'); return 1
    S = [_s16(_i16(sg, i)) for i in range(1, 7)]
    O = [_s16(_i16(so, i)) for i in range(1, 7)]
    ready = (st[0] & 0xFF) if st else 0
    print('  cal_sg =', S)
    print('  cal_so =', O)
    if ready != 1:
        print('⚠ fit_ready=%d（本轮校准没完成）—— 建议先重跑一轮再 bake' % ready)
    if all(v in (0, 1000) for v in S):
        print('⚠ cal_sg 全是 1000，像未校准的默认值 —— 确认真的校准过再 bake')
        if input('   仍要写入？(y/N) ').strip().lower() != 'y':
            return 1
    p = os.path.join(ROOT, 'Inc', 'cal_params_gen.h')
    txt = ('/* ★★★ 自动生成，勿手改 ★★★\n'
           ' *   由 tools/branch/xien__v3-cal.py --stage bake 写入（%s）\n'
           ' *   cal_sg / cal_so = 片上校准结果（rank 因子 × fit_g 合成 + fit_o）\n'
           ' *   用法：本文件存在且 RM2027_USE_BAKED=1 ⇒ 固件默认就用这套参数，不再需要跑校准。\n'
           ' *   回退：删掉本文件，或把 RM2027_USE_BAKED 改成 0。\n'
           ' */\n'
           '#ifndef CAL_PARAMS_GEN_H\n#define CAL_PARAMS_GEN_H\n\n'
           '#define RM2027_USE_BAKED   1\n'
           '#define RM2027_SG_NUM_GEN { 0, %s }\n'
           '#define RM2027_SO_OFF_GEN { 0, %s }\n\n'
           '#endif\n') % (time.strftime('%Y-%m-%d %H:%M:%S'),
                          ', '.join(str(v) for v in S), ', '.join(str(v) for v in O))
    open(p, 'w', encoding='utf-8').write(txt)
    print('→ 已写入', p)
    print('下一步：① 把 Inc/targets.h 的 RM2027_CAL_BUILD 改回 0；② 重新编译 + 烧录')
    return 0

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--bus-mv',type=int,default=None)
    ap.add_argument('--duty',type=int,default=None,
                    help='低档占空比(0..2000)：必须 < CAL_CCR4，让扫描落在关断窗（不给就用固件默认）')
    ap.add_argument('--duty-hi',type=int,default=None,
                    help='高档占空比(0..2000)：必须 > CAL_CCR4+180，让 rank1~4 都落在导通窗（不给就用固件默认）')
    ap.add_argument('--save',action='store_true')
    ap.add_argument('--stage',default='params',choices=['adc','rank','params','bake'],
                    help='params=读片上标定结果并打印（默认）；adc/rank=旧的静态标定')
    a=ap.parse_args()
    print(f'分支 {BRANCH} @ {COMMIT}')
    if a.bus_mv is None:
        s=input('请输入【实测母线电压】(mV)，例如 12030: ').strip()
        a.bus_mv=int(s) if s else 12000
    print(f'母线 = {a.bus_mv} mV'
          + (f', 低档 duty = {a.duty}' if a.duty is not None else '')
          + (f', 高档 duty = {a.duty_hi}' if a.duty_hi is not None else ''))
    poke16('cal_bus_mv',a.bus_mv)
    if a.duty    is not None: poke16('cal_duty',a.duty)
    if a.duty_hi is not None: poke16('cal_duty_hi',a.duty_hi)
    if a.stage=='rank':
        return run_rank(a.bus_mv, save=a.save)
    if a.stage=='params':
        return run_params()
    if a.stage=='bake':
        return run_bake()

    # ★ 两档 duty 只用于"把采样时刻排进正确的窗口"，不参与增益计算：
    #   低档 < CCR4 ⇒ 落在关断窗（0V 参考点）；高档 > CCR4+扫描 ⇒ 落在导通窗（≈Vbus）。
    #   不给 --duty/--duty-hi 就用固件里 CAL_DUTY_LO/HI 的默认值（脚本不再擅自覆盖）。
    poke('cal_state',0); poke('cal_mode',1)
    print('→ 已启动静态校准，等待完成…')
    # ★ cal_state 是 uint8_t，而 esc-dbg.sh 的 peek 读 32 位 ⇒ 必须取低字节，
    #   否则 1 会被读成 0xA00001 之类，st==1 永远不成立 ⇒ 每轮白等满 60 秒。
    for _ in range(120):
        time.sleep(0.2)
        if (peek('cal_state') & 0xFF)==1: break
    st=peek('cal_state') & 0xFF
    print('cal_state =',st)
    raw =read_u16_array('cal_raw',24)
    raw2=read_u16_array('cal_raw2',24)
    if len(raw)<24 or len(raw2)<24:
        print('❌ 读 cal_raw/cal_raw2 失败'); return 1
    lo=[[raw[4*i+j] for j in range(4)] for i in range(6)]
    hi=[[raw2[4*i+j] for j in range(4)] for i in range(6)]
    print('\n步 | 低档VA VB VC V0 | 高档VA VB VC V0 | 高/低/悬')
    for i in range(6):
        print(f' {i+1} | {lo[i][0]:4d} {lo[i][1]:4d} {lo[i][2]:4d} {lo[i][3]:4d} '
              f'| {hi[i][0]:4d} {hi[i][1]:4d} {hi[i][2]:4d} {hi[i][3]:4d} | {FP[i+1]}/{LOW[i+1]}/{FLOAT[i+1]}')
    # ★ 两点差分求增益（K2a-4 修正口径）：
    #   采样落在【导通窗口】时节点就是 Vbus（满导通），**与占空比无关** ⇒
    #       g = (码_高 − 码_低) / Vbus_mV        [码/mV]
    #   原来那个 g = Δ/((d_hi−d_lo)/2000 × Vbus) 是错的：它假设"采样值跟随平均占空比"，
    #   实际高档采到的是满导通 ⇒ 绝对值偏大 1/span（旧参数下 ≈16.7 倍）。
    #   注：低档是"关断参考点"（节点被体二极管钳到 ~0），所以它≈通道零输入偏置，
    #       这个口径把偏置一起减掉了；若要更严，可改用 n3c 静态实测的零输入偏置。
    idx={'A':0,'B':1,'C':2}
    chg={'A':[],'B':[],'C':[]}
    print('\n通道 | 低档码(关断) | 高档码(导通≈Vbus) | 差分 | 增益(码/mV)')
    for i in range(6):
        p_hi=FP[i+1]
        v_lo=lo[i][idx[p_hi]]; v_hi=hi[i][idx[p_hi]]
        d=v_hi-v_lo
        g=d/a.bus_mv if a.bus_mv else 0
        chg[p_hi].append(g)
        print(f'  {p_hi}   | {v_lo:5d} | {v_hi:5d} | {d:+5d} | {g:.4f}')
    gm={p:(sum(chg[p])/len(chg[p])) for p in 'ABC' if chg[p]}
    print('\n通道平均增益：', {p:round(v,4) for p,v in gm.items()})
    if gm:
        ref=sum(gm.values())/len(gm)
        # ★ 防零除：某个通道若全为 0（或读数贴在 0/饱和），ref/v 会炸
        print('相对增益：', {p:(round(ref/v,3) if abs(v)>1e-9 else None) for p,v in gm.items()})
    res={'branch':BRANCH,'commit':COMMIT,'bin_sha':SHA,'bus_mv':a.bus_mv,'duty':a.duty,
         'cal_raw_lo':lo,'cal_raw_hi':hi,'gains':{p:round(gm[p],4) for p in gm}}
    if a.save:
        p=os.path.join(ROOT,'firmware',BRANCH.replace('/','__')+'.cal.json')
        open(p,'w',encoding='utf-8').write(json.dumps(res,ensure_ascii=False,indent=2))
        print('→ 已写',p)
    return 0

if __name__=='__main__': sys.exit(main())
