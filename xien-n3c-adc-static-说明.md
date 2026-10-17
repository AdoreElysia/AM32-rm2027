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
