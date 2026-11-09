# CH552 USB 脚踏开关

独立的 CH552G 固件项目：VIA 网页改键、修饰键组合、四个 VIA 宏、双设备独立配置，
并保留插 USB 时踩住脚踏进入 WCH ISP 的恢复入口。

## 功能

- 每块设备一个脚踏输入：P3.3，按下接地。
- 标准 USB HID 键盘 + VIA Raw HID；不移植完整 QMK。
- 支持普通键及 Ctrl / Shift / Alt / GUI + 按键组合，例如 Ctrl+C、Ctrl+Shift+K。
- VIA 协议 13、V3 定义，支持 `MACRO(0)` 至 `MACRO(3)`：US 布局 ASCII、
  tap/down/up 和非阻塞延时。四个宏共享 109 字节，含终止符及最后的写入标记。
- 键位保存到本设备的数据 Flash。两块设备可以分别配置为左右脚不同按键。
- 上电踩住直接进入现有 ISP，不依赖用户键位，不清空配置。
- VIA 负责改键；固件烧录使用 WCH ISP 工具，不能把两者混为一谈。
- USB 挂起时主循环进入仅由 USB 唤醒的 power-down；不支持脚踏/远程唤醒。
  挂起电流与睡眠后的 USB 状态保持仍需实测。

底层复用固定版本的 CH55xduino USB 实现，使用原生 SDCC 编译，无需 Arduino。
工具链由本项目的 `flake.lock` 锁定，不依赖或修改系统 nixcfg。
KiCad 硬件设计的快照在 [`hardware/`](hardware/README.md)。

## 构建与检查

```sh
cd ch552-pedal
nix develop
make check
```

`make check` 运行主机逻辑测试、构建两种固件并按各自定义检查 HEX 地址、USB 描述符、
中断向量和 XRAM 分配；不会访问 USB 或烧录设备。
单独构建使用 `make firmware VARIANT=test`（默认）或 `make firmware VARIANT=is0`。
在开发环境之外也可运行 `just build` / `just check`。

- 固件：`build/ch552/pedal-test.hex`、`build/ch552/pedal-is0.hex`
- 独立对象、链接和内存报告：`build/ch552/test/`、`build/ch552/is0/` 下的
  `pedal.map`、`pedal.mem`、`bounds.txt`
- VIA V3 定义（协议 13，**Use V2 definitions** 关闭）：[`via/pedal.json`](via/pedal.json)、
  [官方 is0 定义副本](via/is0.json)
- [改键、组合键、双设备与刷机](docs/usage.md)
- [协议和支持范围](docs/protocol.md) · [验证范围](docs/verification.md)

## 固件变体与 USB ID

| 变体 | VID:PID | VIA |
|---|---|---|
| `test`（默认） | `0x1209:0x0001` | 手动导入 `via/pedal.json` |
| `is0`（本地便利选项） | `0xA103:0x0012` | 使用现有官方 is0 V3 定义，无需手动导入 |

两者只改变 VID/PID，同一份固件代码、存储和功能；产品名及序列号仍由芯片 UID
生成 **Pedal XXXXXXXXXX**，不会改成所有设备共用的 is0 名称。

`test` 是共享 pid.codes 开发测试 ID，只用于私人开发；`is0` 借用 ai03 / illuminati
键盘的 ID，也没有分配给本项目。公开 `usb.ids` 未列出 `0xA103`，看来是 ai03
自分配。真正的 is0 或 ai03 软件可能把脚踏当作 is0；**两种变体都不可用于销售或
认证产品**，发布前需取得自己的 USB ID。来源与许可见 [via/README.md](via/README.md)。

## 实物使用前

主机测试和链接审计与实物验收分开。本次真实 VIA 宏 UI、is0 自动匹配、PD 电流、
USB 状态保持和恢复不重新枚举仍需在板上验证；两块设备同时配置也需实物检查。
首次烧录使用现有硬件 ISP 入口；安装本固件后才有踩住上电的软件入口。

## 许可证

本项目新代码采用 GPL-3.0-or-later，见 `LICENSE`。复用的上游代码保留各自
许可证及署名，具体来源和改动随源码记录。`hardware/` 下的 KiCad 设计文件
采用 CERN-OHL-S-2.0，见 [`hardware/LICENSE`](hardware/LICENSE)。
