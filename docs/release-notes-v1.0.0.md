# CH552 USB 脚踏开关固件 v1.0.0 发布说明

这是 CH552G USB 单键脚踏开关固件的第一个正式公开发布版本（**v1.0.0**）。固件内部向 VIA 汇报的固件版本号为 `1`（`PEDAL_FIRMWARE_VERSION 1u`）。

---

## 发布文件（Assets）与选型指南

本发布包含以下固件产物与配置文件：

| 文件名 | 类型 | 说明 |
|---|---|---|
| `pedal-is0-v1.0.0.hex` | 固件镜像（Intel HEX） | **推荐日常使用**。VID `0xA103` / PID `0x0012` |
| `pedal-is0-v1.0.0.bin` | 固件镜像（Raw Binary） | 同上，自地址 0 展平、空洞填零二进制格式 |
| `pedal-test-v1.0.0.hex` | 固件镜像（Intel HEX） | **开发/测试使用**。VID `0x1209` / PID `0x0001` |
| `pedal-test-v1.0.0.bin` | 固件镜像（Raw Binary） | 同上，自地址 0 展平、空洞填零二进制格式 |
| `pedal-v3.json` | VIA V3 键盘定义 | 供 `pedal-test` 变体在 VIA 网页端手动导入 |
| `is0.json` | VIA V3 键盘定义 | 来自上游 the-via/keyboards 的官方 is0 定义副本（GPL-3.0） |
| `SHA256SUMS` | 校验和清单 | 所有发布文件的 SHA-256 哈希值 |

### 如何选择固件变体？

- **普通用户 / 日常使用**：推荐下载 **`pedal-is0-v1.0.0.hex`**（或 `.bin`）。
  该变体借用了 ai03 is0 单键键盘的 USB ID。打开 VIA 网页端（<https://usevia.app/>）并连接设备后，VIA 会通过内置的官方键盘库**自动识别并加载单键界面**，无需任何手动导入 JSON 的操作。
- **开发与独立测试**：选择 **`pedal-test-v1.0.0.hex`**（或 `.bin`）。
  该变体使用 pid.codes 的开源测试 ID。在 VIA 网页端使用时，需先在 **Settings** 打开 **Show Design tab**，关闭 **Use V2 definitions (deprecated)**，然后在 **Design** 标签页中手动加载 `pedal-v3.json`。
- **.hex 与 .bin 的区别**：
  官方 WCHISPTool 均支持这两种格式；`.bin` 格式是将 HEX 镜像自物理地址 `0x0000` 展平、未分配间隙填零的纯二进制文件。两者烧录入芯片后的运行效果完全相同。
- **合规声明**：
  上述两种 USB ID 均**未分配给本项目**（`0x1209:0x0001` 为公开测试对；`0xA103:0x0012` 借用自 ai03，且未收录于公开 usb.ids 数据库）。**两种变体均不得用于商业销售或认证产品**。两种变体的固件逻辑、存储结构及功能完全一致，且均由芯片内部 40 位硬件 UID 生成每片唯一的设备名称与序列号（`Pedal XXXXXXXXXX`）。

---

## 核心特性

- **标准 USB HID 键盘与 VIA Raw HID**：基于 CH552G 全速 USB 运行，包含 8 字节标准 Boot 键盘报告与 32 字节双向 Raw HID 接口（Usage Page `0xFF60`，Usage `0x61`）。
- **VIA 协议 13**：汇报协议版本 13、按键码版本 `0.0.9`、固件版本 `1`。完全兼容现代 VIA V3 键盘规范。
- **丰富的按键与修饰键组合**：支持标准普通按键（`0x04..0xA4`）、修饰键（`0xE0..0xE7`）以及 QMK 样式的 `QK_MODS` 组合键（支持 Ctrl、Shift、Alt、GUI 任意组合，如 Ctrl+C、Ctrl+Shift+K 等）。
- **4 个 VIA V11 宏**：支持 `MACRO(0)` 至 `MACRO(3)`（按键码 `0x7700..0x7703`），共用 109 字节 DataFlash 缓冲区。支持美式（US）ASCII 布局文本录入、tap/down/up 动作及 0..9999 ms 的非阻塞延时。
- **掉电安全的键位存储**：使用 DataFlash 逻辑地址 0..15 的双槽日志设计，结合世代计数器与 CRC-16 校验。保存时仅写入非活动槽并经双重读回校验后再行提交，保证在任意写入阶段遭遇异常断电都不会损坏上一份有效键位。
- **CRC 校验的宏存储**：DataFlash 逻辑地址 16..127 存放 109 字节宏数据、有效标记与 CRC-16。具备结构化边界检查、写入失败锁与读回比对，防止播放不完整或破损的宏。
- **踩住上电直进 WCH ISP**：拔出 USB 后按住脚踏插入，固件在上电 100 ms 硬件 RC 滤波后连续采样 P3.3 引脚，直接跳转到芯片内置的 WCH ISP 引导程序（`0x3800`）。该过程发生在读取 DataFlash 及初始化 USB 之前，既不依赖既有键位有效性，也不会清空用户的改键与宏配置。
- **USB 挂起低功耗**：主机休眠/USB 挂起时，主循环进入 CH552 硬件掉电模式（`PCON |= PD`），仅配置 USB 总线唤醒（`WAKE_CTRL = bWAK_BY_USB`）。不支持脚踏唤醒或远程唤醒电脑。唤醒恢复后无按键残留卡死，亦不重播挂起期间的踩踏事件。

---

## 固件烧录指引

推荐使用沁恒官方 **WCHISPTool**（Windows 平台）：

1. 打开 WCHISPTool，芯片系列选择 **CH55x**，芯片型号选择 **CH552**，下载接口选择 **USB**。
2. 设备进入 ISP 模式：
   - **首次烧录**（芯片为空白或未刷入本固件）：短接板上的 **JP1** 硬件跳线（将 P3.6 引导脚置为有效电平），再插入 USB。
   - **后续升级**（已安装本固件）：拔出 USB，**踩住脚踏不放并重新插入 USB**，等待系统提示音或识别到 ISP 设备后松开即可，无需开盖短接跳线。
3. 勾选“**目标程序文件1**”复选框，浏览并载入对应的 `.hex` 或 `.bin` 文件。
4. 保持所有配置选项为默认，下载配置脚选择 **P3.6**。
5. **重要警告**：**不用点“校验”**：下载完成后芯片会直接运行固件、退出 ISP，此时再校验多半报错，不代表刷坏。
6. 点击“**下载**”，完成后重新插拔 USB 即可正常使用。

*注：项目开发环境中内置了命令行工具 `wchisp 0.3.0`，在 Linux/WSL 下可在进入 ISP 后通过 `wchisp flash <firmware.hex>` 烧录；因部分 CH552 bootloader 存在已知兼容性限制，若遇通讯问题请优先使用 WCHISPTool。*

---

## 硬件验收测试记录（2026-11-09）

2026-11-09 在真实 CH552G 硬件单板（Bootloader 2.50）上完成全流程验收，用户确认所有操作均表现“正常”：
- WCHISPTool 成功通过 JP1 跳线烧录固件；
- 系统正常枚举为复合设备，设备产品名与序列号为芯片 UID 派生的 `Pedal 95BA7575BF`；
- VIA 13 正常连接，`is0` 变体自动识别单键界面；
- 普通键与组合键（如 Ctrl+Shift+K）映射、读回、拔插断电保持均正常；
- 宏编辑、保存、播放及断电持久化正常，超长宏写入被安全拦截拒绝；
- 电脑休眠挂起进入掉电模式，唤醒后无需重新枚举立即恢复输入，无卡键或事件重播；
- 踩住脚踏上电成功直接进入 ISP 烧录模式，退出后已保存的键位与宏均未受损。

**测试范围边界说明**：
- 验收结果来自用户的实操报告，未逐项提供日志。
- 挂起电流未提供仪表实测读数，数据手册给出的约 0.3 mA 释放 / 0.37 mA 踩下数值仍为理论估算。
- 双设备同时插入独立改键与使用的场景未在本次实物中单独测试与报告。

---

## 固件审计与校验和

- **编译环境**：SDCC 4.6.0，使用 `--model-large --int-long-reent --nooverlay --opt-code-size`，全 XRAM 核心状态指针。
- **固件空间占用**（两变体一致）：
  - 13,651 字节有效 Flash（最高物理地址 `0x358A`，低于 ISP 起始地址 `0x3800`，剩余 629 字节）。
  - XRAM 末端地址 `0x0358`（DMA 占用 266 字节，应用 XRAM 590 字节，剩余 168 字节可用）。
  - 栈起始地址 `0x45`，链接器可用栈空间 187 字节（静态分配，非运行时峰值）。

### SHA-256 校验和

```
520f1651914c560084d1e4b1c4193ef9fbf533c7ff0a3ec8444a43ffc4d741c0  pedal-test.hex
f7660f2ced2ff6ba8421b5c800c23f629b85cac79de521e5a8add22798707c2a  pedal-is0.hex
```

---

## English Summary

Release v1.0.0 is the first public firmware release for the CH552G USB foot pedal.

- **Assets**:
  - `pedal-is0-v1.0.0.hex` / `.bin`: Recommended. Borrowed ai03 is0 keyboard ID (`0xA103:0x0012`) for zero-configuration automatic recognition in the VIA web app (<https://usevia.app/>).
  - `pedal-test-v1.0.0.hex` / `.bin`: For testing. Uses pid.codes ID (`0x1209:0x0001`); requires loading `pedal-v3.json` in VIA with "Use V2 definitions" OFF.
  - `pedal-v3.json` / `is0.json`: VIA V3 keyboard definitions.
  - `SHA256SUMS`: Hash list for released files.
  - Neither USB ID is assigned to this project; not for commercial or certified products. Both builds retain per-chip serial/product strings ("Pedal XXXXXXXXXX") from MCU UID.
- **Features**: VIA protocol 13, keycodes 0.0.9, firmware version 1; ordinary keys and modifier chords (e.g. Ctrl+Shift+K); 4 V11 macros sharing 109 bytes; power-fail safe keymap journal in DataFlash; CRC-16 protected macros; USB suspend power-down with USB-only wake; held-pedal power-up enters WCH ISP bootloader without erasing configuration.
- **Flashing**: Use WCHISPTool (CH552, USB, tick 目标程序文件1, default settings, P3.6, no need to click 校验: the chip leaves ISP and runs the firmware after download, so a later verify usually fails). First flash uses JP1 jumper; subsequent flashes hold the pedal during USB connection.
- **Hardware Acceptance**: Verified on physical hardware (single unit, bootloader 2.50) on 2026-11-09. All verification guide steps confirmed normal. (~0.3 mA suspend current remains a datasheet estimate; dual-device test was not separately reported).
