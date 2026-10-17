# xien/n3-adc-归一 —— A1 ADC 基线/归一化

> 从 `xien/n1-开环锚定` 分出。只做 ADC 基线采集与六步相对归一，不改开环/闭环控制链。
> 目标工况：`ol_ci_tgt=1700`（1961rpm）、`ol_input_tgt=350`（duty≈300）、事件流关、窗口 N=8。

---

## 1. 固件默认（A1 基线）

- `XIEN_OL_INPUT = 350`
- `XIEN_OL_STEP_DIV = 17` ⇒ `XIEN_OL_CI_TGT = 1700`
- `XIEN_OL_SCAN_MODE = 1`：纯开环、不交棒
- 启动链按 test-12：5000 起、`OL_INPUT_START=400`、`ol_freq_slope=554Hz/s`、8s
- `RM2027_EVT_STREAM = 0`：事件流关闭
- `RM2027_WIN_EVERY_N = 8`

运行中也可以改：
```bash
./tools/esc-dbg.sh poke32 ol_ci_tgt 1700
./tools/esc-dbg.sh poke16 ol_input_tgt 350
```

## 2. 采集

VSCode：**ESC 2 · 编译烧录 + 接收准备（固定采 7s）**  
同一工况跑 **2 轮**，中间冷却。

产物（按分支+时间）：
```
evt_run_xien__n3-adc-归一_<time>.csv
evt_run_xien__n3-adc-归一_<time>_win.npz
evt_run_xien__n3-adc-归一_<time>_meta.txt
```

## 3. 分析

```bash
python3 tools/adc_cal_baseline.py <win.npz> --ci 1700 --label input350
```

输出到 `analysis/`：

- `adc_norm_segments_<label>.csv`：每段原始/归一后的幅值、相位、offset、slope、resid；
- `adc_norm_steps_<label>.csv`：六步 `g`/`o` 表与验证；
- `adc_norm_summary_<label>.csv`：每窗概览；
- `adc_norm_baseline_<label>.png`：六栏图。

**注意**：A1 的 g/o 是“六步相对归一”（把六步幅值/偏置拉到一个共同尺度），不是绝对 Ke 标定；
绝对 Ke/增益需要多个 ci/多个 input 点拟合 `A=Ke·f` 后再定。

## 4. 判据

- 六步幅值归一后应接近相等（g 表有效）；
- 相位分布不应因归一化变差；
- 无过零率不应升高；
- A 与 f_comm 在多点扫描时应线性过原点；
- 同一 CCR4/同采样顺序内比较，不跨配置直接比 A。

## 5. 下一步

A1 数据齐后，另开 `xien/n3b-ccr4-扫描`，只改 `ADC_TRIGGER_CCR4`，固定本工况扫
`200/190/180/170/160/140`，比较 A、振铃、相位、无过零、电流。
