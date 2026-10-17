# xien/n3b-ccr4-扫描 —— CCR4 单变量扫描

> 从 `xien/n3-adc-归一` 分出。只改 ADC 触发点 `TIM1->CCR4`，固定其他工况。
> 目标：找出“避开换相振铃、且给低 duty 留最多导通余量”的最小 CCR4。

## 1. 固定工况

- `ol_ci_tgt = 1700`（1961rpm）
- `ol_input_tgt = 350`（duty≈300）
- 事件流关
- 窗口 `N=8`
- 启动链按 test-12：5000 起、input 400→350、554Hz/s、8s

## 2. 为什么可以只烧一次

`CCR4` 是 TIM1 的比较寄存器，烧录复位后由 `peripherals.c` 写成 `ADC_TRIGGER_CCR4`；
本分支用 SWD 在**复位后、给油前**改写 `TIM1->CCR4`，所以同一份固件可以扫多档：

```bash
./tools/ccr4_point.sh 200 7
./tools/ccr4_point.sh 190 7
./tools/ccr4_point.sh 180 7
./tools/ccr4_point.sh 170 7
./tools/ccr4_point.sh 160 7
./tools/ccr4_point.sh 140 7
```

每点建议跑 **2 轮**，中间冷却。  
脚本会：开串口 spool → 烧录/复用归档 → 复位 → 写 `TIM1->CCR4` → 等 armed → 给油 7s → 收油 → 解析落盘。  
meta 会记录 `ESC_CCR4`。

## 3. 分析

```bash
python3 tools/ccr4_scan_compare.py --ci 1700 --outdir /home/xien/RM2027/analysis \
  200=<run200_win.npz> \
  190=<run190_win.npz> \
  ...
```

输出：

- `ccr4_scan_summary.csv`：每点 `L / 有效采样率 / A / phase / P10~P90 / 无过零 / 电流 / edge_pp`
- `ccr4_scan_compare.png`：A、相位、无过零、段首峰峰值 vs CCR4

**注意**：
- `edge_pp` 只是“段首 5 点峰峰值”的粗指标，真正判振铃要结合原始波形；
- CCR4 每改一档，采样点相对开关沿变了，**g/o 必须重新标**；本扫描只做粗筛，选中后再用 `adc_cal_baseline.py` 标该档。

## 4. 判据

- CCR4 太小 → 采样落进开关振铃：A 异常/相位噪/无过零升/edge_pp 大；
- CCR4 太大 → 低 duty 时可能超出导通期，采不到；
- 优先选：`A` 合理且平稳、相位分布窄、无过零低、电流不异常、`edge_pp` 不随 CCR4 明显恶化 的**最小 CCR4**；
- 200 是历史硬顶，先以 200 为基线；不建议超过 200。

## 5. 安全

- 无桨 + 固定 + 可调电源限流；
- 每点 7s，冷却后再跑；
- 异响/电流爬升/堵转立即 F1 收油。
