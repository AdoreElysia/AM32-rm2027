# xien/n3c-adc-static —— 不转电机的 ADC 静态校准

> 从 `xien/n3b-ccr4-扫描` 分出。只改 ADC 扫描序列，不驱动电机；F1 必须保持收油。
> 目标：先把 ADC 本身的顺序/settling/offset 误差量出来，再回去做六步 g/o 残差。

## 1. 两种静态模式

在 `Inc/targets.h` 顶部：

```c
#define ADC_STATIC_TEST  1
```

- **模式 1（默认）**：同一个**母线电压通道**复制到 rank1/2/3/4/6；rank5=电流、rank7=温度。
  - `ADCDataDMA[0..3]` 和 `[5]` 都是同一路母线电压；
  - 真值相同，差异 = ADC 顺序/settling 误差。
- **模式 2**：同一相连续采两次：
  ```
  rank1=VA, rank2=VA
  rank3=VB, rank4=VB
  rank5=VC, rank6=VC
  rank7=V0
  ```
  - 比较同相第一/第二次采样，直接看建立不足。
- **模式 3**：母线(高) 与 V0(低) 交替：
  ```
  rank1/3/5/7 = 母线电压
  rank2/4/6   = V0
  ```
  - 真值分别恒定；看大电压跳变后母线/V0 的读数是否被“前一个通道”污染。

`ADC_TRIGGER_CCR4 = 160`（本分支默认，按已选 CCR4）。

## 2. 烧录 + 静态采集（不转电机）

```bash
cd /home/xien/RM2027/AM32
git checkout xien/n3c-adc-static
ESC_YES=1 ./tools/esc-lab.sh flash
python3 tools/adc_static_dump.py --samples 50 --interval-ms 10 --out /home/xien/RM2027/analysis
```

前提：
- F1 收油（`hold_zero=1`）；
- 母线 12V 稳定；
- 电机不转、无桨。

脚本只 halt/resume 读 RAM，不写 PWM、不给油。

输出：`analysis/adc_static_<时间>.csv`
列：`sample, adc0..adc6, raw_volts, raw_current, frame`。

## 3. 判读

### 模式 1（母线通道多 rank）

- `adc0..3`、`adc5` 应该都等于同一母线电压；
- 如果 rank 之间均值差 > 噪声（std 的几倍）⇒ **顺序/settling 误差**；
- 记录每个 rank 的均值/标准差，后面做 rank 级 gain/offset 修正。

### 模式 2（同相连续两次）

- `adc0 vs adc1`、`adc2 vs adc3`、`adc4 vs adc5` 对应同名相两次采样；
- 如果第二次明显更稳/更高 ⇒ 第一次建立不足；
- 可据此决定：加 dummy sample、加长采样时间、还是改采样顺序。

## 4. 后续

静态结果出来后：
1. 若顺序误差明显 → 先做 rank 级 ADC 校正（或改采样时序）；
2. 若 offset 明显 → 先减 offset；
3. 再回到 `xien/n3b-ccr4-扫描` 的 CCR4=160 数据，重标六步 g/o；
4. 最后做过零方法对比。

⚠ 本分支是“静态测试固件”，不要拿它跑闭环/给油；测完切回 `xien/n3-adc-归一` 或 `xien/n3b-ccr4-扫描`。


## 6. 2026-10-17 实测结果

- **模式 1**（母线重复在 rank1/2/3/4/6）：均值 1354.68~1355.07，std≈2.0~2.2；
  rank 间差 <0.4 码 ⇒ 无显著顺序/settling 误差。
- **模式 2**（VA/VA、VB/VB、VC/VC）：同相两次差 0.05~0.77 码，均小于噪声 std≈3.5；
  V0≈46.3，三相≈50.8~52.1（静态偏置差约 5 码）。
- **模式 3**（母线/V0 交替）：
  - 母线 rank1/3/5/7 = 1353.21/1353.18/1353.11/1353.09（std≈2）；
  - V0 rank2/4/6 = 64.17/63.97/64.00（std≈3.7）；
  ⇒ **大电压跳变也没有明显顺序/电荷共享误差**。
- 母线 12V/11≈1.09V，理论码≈1353；实测 1353.2 ⇒ 母线通道/参考很准。
- 结论：当前 ADC 顺序/settling 不是六步幅值差异的主因；A/B/C 的差异更可能来自真实相电压/驱动/板级不对称，需要回到开环稳态数据做六步 g/o 残差。
