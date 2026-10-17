# xien/n1 —— 开环锚定扫描（固定开环点版）

> 从 `xien/n2b-串口-收尾@test12` 分出。目标：用干净、可复算的固定开环点，回答
> “正确稳态的转速/频率/duty 是什么”，并给后续 ADC 归一、CCR4 扫描、过零方法比较提供数据。
>
> 用户工作方式：**你改变量 → 编译烧录 → 采集；我负责数据按分支+时间落盘、算指标、记点、出图。**

---

## 1. 每次只改这一块（`Inc/targets.h` 顶部）

```c
#define XIEN_OL_SCAN_MODE   1      // 1 = 纯开环、永不交棒；保留已知好斜坡
#define XIEN_OL_INPUT      340     // 斜坡【终点】油门刻度（不是最终 duty；duty 看 CSV 的 duty 列）
#define XIEN_OL_STEP_DIV   17      // 斜坡【终点】开环步长；命令 ci = 本值 × 100 拍
```

> ★ 启动链保持 test-12 验证过的写法：
> 对齐 200ms → 斜坡 ~3.2s（`OL_STEP_DIV_START=5000`、`OL_INPUT_START=400`）→ 恒速 → **不交棒**。
> 你改的只是斜坡终点，不再跳过斜坡。

| `XIEN_OL_STEP_DIV` | 命令 ci（拍） | 目标 rpm | 用途 |
|---|---|---|---|
| 17 | 1700 | ≈1961 | 慢档，对照旧交棒锚点 1667~1833 |
| 13 | 1300 | ≈2564 | 中间档 |
| 11 | 1100 | ≈3030 | 贴近 test-12 好闭环 1000~1083 |
| 10 | 1000 | ≈3333 | 好闭环最快档 |

`XIEN_OL_INPUT` 是油门刻度，实际 duty 由固件映射后得到：
- 历史参考：`XIEN_OL_INPUT=340` ⇒ 实测 `duty≈290`
- 想要 duty≈300：先从 347~350 试，再按 CSV 里的实际 `duty` 微调
- **每跑完一轮，把实际 duty 告诉我，我记进开环锚定台账**

---

## 2. 编译烧录 + 采集

推荐直接用 VSCode 任务：

- **ESC 2 · 编译烧录 + 接收准备（固定 7s）**
  - 会先开串口 spool，再编译（源码改了会重新编译）/烧录/复位，等 armed 后给油采 7s，最后解析落盘
  - **已经固定采 7s**（VSCode ESC2 不再问秒数）；固件开环 8s 上限仍然在
  - 会先等 ESC `armed` 再放 F1 给油；重烧/复位后 1s 常常不够，现在由 `wait_esc_armed` 自动等
  - 电机会真的转 7s；当前前提：**无桨 + 已固定 + 可调电源限流**
  - 因为改了 `targets.h`，ESC2 的“安全复用”会判定源码脏 → 自动重新编译，不会误烧旧产物

如果只想手动编译烧录：
```bash
./tools/esc-lab.sh flash          # 编译当前分支 + 归档 + 烧录（不转电机）
python3 tools/cap-evt.py 6        # 给油采 6 秒 + 落盘/出图
```

---

## 3. 数据命名与落盘

`cap-evt.py` 现在会自动按 **分支 + 时间** 命名：

```
evt_run_xien__n1-开环锚定_YYYYmmdd_HHMMSS.csv
evt_run_xien__n1-开环锚定_YYYYmmdd_HHMMSS_meta.txt
evt_run_xien__n1-开环锚定_YYYYmmdd_HHMMSS_win.npz
evt_run_xien__n1-开环锚定_YYYYmmdd_HHMMSS.png
```

`_meta.txt` 自动记录：
`branch / commit / binary / binary_sha256 / serial@baud / secs / raw_bytes /
window_packets / event_rows / fake_frames / duty_median / ci_median_ticks /
f_comm_hz / rpm_median / ESC_DUTY / ESC_STEP_DIV / ESC_TAG`

> 你告诉我实际 duty 后，我把它写进开环锚定台账（`analysis/ol_anchor_log.csv`），
> 并和 CSV 里的 `duty_median` 对照，确认映射关系。

---

## 4. 第一轮建议测试点

先跑粗矩阵，每点至少 2 轮，中间冷却：

| 序号 | 步长档 | `XIEN_OL_STEP_DIV` | `XIEN_OL_INPUT`（先试） |
|---|---|---|---|
| 1 | 慢 | 17 | 340 / 347 / 354 / 361 / 368 |
| 2 | 中 | 13 | 340 / 347 / 354 / 361 / 368 |
| 3 | 好闭环档 | 11 | 340 / 347 / 354 / 361 / 368 |
| 4 | 快 | 10 | 347 / 354 / 361 / 368（更高速，先确认前 3 档能跟随） |

原则：
- 先慢档、低 input；确认转子跟得上，再加速/加 duty；
- 每点采 ≥3s，取最后 2s 当稳态；
- 出现堵转、电流爬升、异响 → 立即 F1 收油、停止该点；
- 每点冷却后再跑第二轮，检查可重复性。

---

## 5. 验收指标（我拿到 CSV 后自动算）

1. **跟随误差**：实际 `ci` 中位 vs 命令 `XIEN_OL_STEP_DIV×100`，目标 ≤5%；
2. 转速 `rpm = 3.3333e6 / ci`；
3. 换相频率 `f_comm = 2e6 / ci`；
4. 电流中位、P90，看是否稳定/爬升；
5. 长步比例（>1.2×中位）、step 序列是否单调无漏；
6. `bemf_timeout` 增量、`zc_reject` 增量、`evt_drop`；
7. 窗口流里悬空相−V0 的幅值与过零可见性；
8. 两轮重复性。

最终输出：
- duty × 步长档 → 实际 rpm 曲线族；
- 跟随误差热图/稳定边界；
- 电流 vs duty；
- 推荐开环锚点（最贴近好闭环 duty≈300 / ci≈1000~1083 / rpm≈3077~3333 且稳定的点）；
- 代表性窗口波形。

---

## 6. 安全红线

- 仅 **无桨 + 固定** 状态跑；
- 可调电源限流先设好，不要顶限流；
- 单点开环 8s 上限（固件既有 `OL_STARTUP_TICKS_MAX`），采集建议 6s；
- 一点一冷却，不要连续猛跑；
- 出现异常立即 F1 收油/断电；过流跳闸后要人工干预才恢复。


---

## 7. 事件流已关闭（当前试验）

为了判断“串口发送本身是否造成卡顿”，本分支默认把事件流整体关掉：

- `Inc/targets.h` 里 `RM2027_EVT_STREAM=0`；
- 固件运行时变量 `evt_enable`（默认 0）：0 = 不组事件包、不发事件帧，只留窗口流；
- 想临时恢复事件流（不用重编译）：
  ```bash
  ./tools/esc-dbg.sh poke evt_enable 1     # 开
  ./tools/esc-dbg.sh poke evt_enable 0     # 关
  ```
- 关掉后 `cap-evt.py` 收不到事件是正常的；只要窗口包 >0，就按“仅窗口流”成功处理，不再报错。
- 想进一步判断“窗口流本身是否卡顿”：
  ```bash
  ./tools/esc-dbg.sh poke win_every_n 1    # 窗口流拉到最满
  ./tools/esc-dbg.sh poke win_enable 0     # 窗口流也关掉
  ```
  对比同一 `input/step_div` 下三种情况：事件+窗口 / 仅窗口 / 全关。


---

## 8. 连续升频参数（本次修正）

现在的开环斜坡是**恒定斜率连续升频**：`f(t)` 以 `ol_freq_slope` Hz/s 从起点频率升到目标；
每个换相步只增加很小的频率增量，不是阶跃跳变。

本分支默认：

```c
#define XIEN_OL_FREQ_SLOPE 400   // Hz/s（比原来的 554 更缓、更连续）
#define XIEN_OL_RUN_MS     10000 // 开环总时长上限 10s（原 8s）
```

- 想更连续/更缓：把 `XIEN_OL_FREQ_SLOPE` 改成 300 或更低；
- 想更快：改大（例如 554/700），但丢步风险变大；
- **运行时可经 SWD 改，不用重编译**：
  ```bash
  ./tools/esc-dbg.sh poke16 ol_freq_slope 300
  ./tools/esc-dbg.sh poke16 ol_freq_slope 554
  ```
- 目标频率仍然由 `XIEN_OL_STEP_DIV` 决定；`ol_freq_slope` 只决定“多快爬到目标”。
