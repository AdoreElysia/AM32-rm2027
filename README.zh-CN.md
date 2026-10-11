# AM32-MultiRotor-ESC-firmware

**基于 ARM 的电调固件**（MultiRotor = 多旋翼；ESC = Electronic Speed Controller，电调，即驱动无刷电机的那块小板子）

许可证：GPL-3.0

AM32 固件是为 STM32 系列 ARM 处理器设计的，用来控制无刷电机（BLDC，Brushless DC，无刷直流电机）。这个固件的目标是**安全、快速**：启动又快又平顺，油门响应线性。它被设计成可以配合多种载具类型和飞控一起使用。固件也可以在编译时加入对履带车（crawler，爬行类车辆/攀爬车）的支持。用于履带车场景请阅读这个 wiki 页面：[Crawler Hardware](https://github.com/AlkaMotors/AM32-MultiRotor-ESC-firmware/wiki/Crawler-Hardware-and-AM32)。

## 特性

AM32 具备以下功能：

- 支持通过 Betaflight 直通（passthrough，指借飞控的 USB 口把电调的串口"透传"到电脑上，不用另外接线）、单线串口（single wire serial）或 Arduino 来升级固件
- 支持舵机 PWM 信号，以及 DShot（300、600）电机协议
- 支持双向 DShot（bi-directional Dshot，转速数据能回传给飞控）
- 支持 KISS 标准电调遥测（telemetry，电调把温度/电压/转速等数据报给飞控）
- PWM 频率可变
- 正弦波启动模式（sinusoidal startup），专门用来把大尺寸电机顺利带到转速上来

## 编译说明

下载并安装 Keil 社区版（community edition，即免费版）。打开 "Keil projects" 文件夹里你要用的那颗 MCU（单片机）对应的 Keil 工程。如果提示安装 MCU 支持包（pack），就装上。在下拉框里选择编译目标（build target），然后编译工程。

## 固件发布与配置工具

最新固件版本可以在[这里](https://am32.ca/downloads)找到。

最主要的配置工具是 [AM32 Configurator](https://am32.ca)，它支持在网页浏览器里做参数配置和固件升级。

你也可以使用桌面版配置工具，从这里下载：

[WINDOWS 版](https://drive.google.com/file/d/16kaPek9umz7fQFunzBeW4pp2LgT6_E5o/view?usp=drive_link)
[LINUX 版](https://drive.google.com/file/d/1QtSKwp3RT6sncPADsPkmdasGqNIk68HH/view?usp=sharing)

另外，你还可以用 [Online-ESC Configurator](https://esc-configurator.com/)，只要浏览器支持 Web Serial（网页直接访问串口的能力），就能用它刷固件或改参数。

## 硬件

AM32 目前支持 STSPIN32F0、STM32F051、STM32G071、GD32E230、AT32F415 和 AT32F421。

不推荐使用 CKS32F051，因为它有太多随机的毛病。

目标（target，指固件里针对某块具体板子编译出的那一套配置）兼容性列表在[这里](https://github.com/am32-firmware/AM32/blob/main/Inc/targets.h)。

## 安装与 Bootloader

（Bootloader = 引导程序，一小段先运行的程序，负责把主固件写进单片机；没有它就没法用串口刷固件）

要在**一块空白的电调**上使用 AM32 固件，必须先用 ST-LINK、GD-LINK、CMSIS-DAP 或 AT-LINK 这类下载器装好 bootloader。bootloader 取决于电调上用的是哪颗 MCU。要选择与电调的 MCU 型号**以及信号输入引脚**都匹配的那个 bootloader。兼容性表里列出了各个 bootloader 的引脚定义。当前的 bootloader 可以在[这里](https://github.com/am32-firmware/AM32-bootloader)找到。

bootloader 装好之后，主固件就可以用配置工具配合一块 Betaflight 飞控来烧写，或者用一个改成单线模式的 USB 串口适配器直连来烧写。

要更新已有的 AM32 bootloader，更新工具在[这里](https://github.com/am32-firmware/AM32-unlocker)。

## 支持与开发者频道

有两种方式可以获得支持，或参与改进 AM32。

我们有一个 Discord 服务器：

https://discord.gg/h7ddYMmEVV

礼仪提醒：请多等一会儿再走开——有时候大家在飞、在睡觉或在上班，没法立刻回复。

如果你想支持这个项目，欢迎加入 Patreon：

https://www.patreon.com/user?u=44228479

## 赞助者

AM32 项目能走到今天，离不开以下赞助者的帮助：

- Holmes Hobbies - https://holmeshobbies.com/ - 没有 HH 的支持，项目不会是今天的样子。想要最好的 AM32 体验，看看 Crawlmaster V2！
- Repeat Robotics - https://repeat-robotics.com/ - 把 AM32 电调带进了格斗机器人圈子！
- Quaternium - https://www.quaternium.com/ - 固件开发支持与硬件捐赠
- Airbot - 大量硬件捐赠
- NeutronRC - 提供硬件、AM32 推广和原理图
- Aikon - 硬件捐赠和原理图
- Skystars - 提供硬件，并愿意在第一批商用 AM32 电调上冒险尝试
- Diatone - 硬件捐赠
- T-motor - 电机和硬件捐赠
- HLGRC - 硬件捐赠

## 贡献者

衷心感谢所有为 AM32 项目贡献时间、建议和代码的人：

Un!t
Hugo Chiang (Dusking)
Micheal Keller (Mikeller)
ColinNiu
Jacob Walser

以及来自飞手和驱动开发者的反馈：

Jye Smith
Markus Gritsch
Voodoobrew

（还有更多的人）

---

## 译者补充：原文的几处小问题

1. **原文有几处拼写错误**，译文按正确意思处理了：`THe bootloader`、`ont he esc`、`Hardaware donations`（应为 Hardware）、`CMIS-DAP`（应为 CMSIS-DAP）、`from can be installed`（多了一个 from）。
2. **原文 "The AM32 firmware is designed for STM32 ARM processors" 在事实上有遗漏**：从"硬件"一节可见，它还支持 GD32E230、AT32F415、AT32F421，这些是国产的 ARM Cortex-M 芯片（不是 ST 的）。准确说法应是"面向 ARM Cortex-M 系列的单片机"。
3. **原文里的仓库链接指向两个不同的组织名**（`AlkaMotors/AM32-MultiRotor-ESC-firmware` 与 `am32-firmware/AM32`），说明项目迁移过。要查最新 README 或 bootloader，建议以 `am32-firmware` 组织下的仓库为准。
4. 本仓库里除 `README.md` 外还有 `env_setup_scripts/README-SITL.md`（SITL = 软件在环仿真，即在电脑上模拟电调运行），那是本地仿真用的说明，与这份上游 README 不是同一份文档。

（本文档译自本仓库 README.md，仅作阅读参考；与原文冲突时以英文原文为准。）
