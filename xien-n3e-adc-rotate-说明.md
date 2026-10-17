# xien/n3e-adc-rotate —— 开环 20s + A/B/C/V0 运行时顺序轮转（实验分支）

> 状态：**代码已编译通过，但硬件验证未通过，暂不要用于最终数据。**
> 目的：在一个 20s 开环稳态里反复交换 A/B/C/V0 的 ADC rank，分离“采样顺序”误差。

## 1. 当前实现

- `ol_ci_tgt=1700`、`ol_input_tgt=350`、CCR4=160、事件流关、N=8；
- `XIEN_OL_RUN_MS=22000`（20s 采集留余量）；
- 在 `ADC.c` 里新增：
  - `adc_order`：0..3 四序；
  - `adc_order_pending`：待切换顺序；
  - `adc_frame_order`：本帧实际映射顺序；
  - `adc_apply_order()`：设置 rank1..4 的通道；
- 四序：
  - 0: A,B,C,V0
  - 1: B,C,V0,A
  - 2: C,V0,A,B
  - 3: V0,A,B,C
- 切换时机：
  - 在 `win_push()` 检测到进入 step1（每个电周期）时设置 `adc_order_pending`；
  - 在 ADC-TC 中断里、当前帧映射完成后应用 pending；
  - 绝不在主循环直接写 ADC 序列寄存器。
- `win_step` 高 2 位记录本帧 order，低 3 位为 step；分析时必须 `order=(step>>6)&3; step&=7`。

## 2. 已知问题（重要）

2026-10-17 20:5x 的 20s 实跑没有进入正常驱动：

- `cur` 中位只有 4~5 码（正常约 20）；
- 相电压约 20 码，不是正常 BEMF；
- 虽然 step 序列在走，但**那轮数据不能用于 rank/归一化分析**。

怀疑方向：

1. 运行时写 ADC rank 与启动/测量链的时序仍有冲突；
2. 四序切换影响 `bemf_adc_vph/v0` 的连续性，触发了某些保护/锁判据；
3. 需要在“电机已经稳定转起来之后”才允许轮转，启动阶段固定 order0；
4. 或者放弃运行时轮转，改用固定顺序的多次编译/烧录做四序平均。

## 3. 下次有电时的最小验证

```bash
git checkout xien/n3e-adc-rotate
ESC_YES=1 ./tools/esc-lab.sh flash
python3 tools/cap-evt.py 8
```

先看：

- `cur` 中位是否 ~20；
- step 是否 1..6 正常循环；
- `win_step` 的 order 是否每个电周期变化；
- 如果仍 cur≈4，先回退到“启动阶段固定 order0，平稳后再轮转”。

## 4. 安全

- 只允许开环、无桨、固定、电源限流；
- 不能用于闭环在线判据（顺序交替会给阈值注入交替偏置）；
- 数据无效时不要在台架上继续硬跑，先回退正常分支。
