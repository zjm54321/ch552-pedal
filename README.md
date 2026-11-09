# CH552 USB 脚踏开关

独立的 CH552G 固件项目：VIA 网页改键、修饰键组合、双设备独立配置，
并保留插 USB 时踩住脚踏进入 WCH ISP 的恢复入口。

## 功能

- 每块设备一个脚踏输入：P3.3，按下接地。
- 标准 USB HID 键盘 + VIA Raw HID；不移植完整 QMK。
- 首版支持普通键及 Ctrl / Shift / Alt / GUI + 按键组合，例如 Ctrl+C、
  Ctrl+Shift+K。连续输入多组按键的宏与同时按下的组合键是不同功能。
- 键位保存到本设备的数据 Flash。两块设备可以分别配置为左右脚不同按键。
- 上电踩住直接进入现有 ISP，不依赖用户键位，不清空配置。
- VIA 负责改键；固件烧录使用 WCH ISP 工具，不能把两者混为一谈。

底层复用固定版本的 CH55xduino USB 实现，使用原生 SDCC 编译，无需 Arduino。
工具链由本项目的 `flake.lock` 锁定，不依赖或修改系统 nixcfg。
KiCad 硬件设计的快照在 [`hardware/`](hardware/README.md)。

## 构建与检查

```sh
cd ch552-pedal
nix develop
make check
```

`make check` 运行主机逻辑测试、构建固件并检查 HEX 地址、USB 描述符、
中断向量和 XRAM 分配；不会访问 USB 或烧录设备。
只构建使用 `make firmware`。在开发环境之外也可运行 `just build` / `just check`。

- 固件：`build/ch552/pedal.hex`
- 链接和内存报告：`build/ch552/pedal.map`、`pedal.mem`、`bounds.txt`
- VIA V2 定义（固件声明协议 8，VIA 对协议 11 以下只认 V2）：[`via/pedal.json`](via/pedal.json)
- [改键、组合键、双设备与刷机](docs/usage.md)
- [协议和支持范围](docs/protocol.md) · [验证范围](docs/verification.md)

## 实物使用前

当前固件可编译，逻辑测试与构建审计可在本机运行。USB 枚举、上电进 ISP、
DataFlash 实际保存和两块设备同时配置，需要在实物上验收。
首次烧录使用现有硬件 ISP 入口；安装本固件后才有踩住上电的软件入口。

USB 暂用共享开发测试 ID `0x1209:0x0001`，不是本项目获分配的 ID；
已准备向 pid.codes 申请 `0x1209:0xC552`，见 [申请草稿](docs/pid-codes/README.md)。
获批后同步修改固件与 VIA 定义。

## 许可证

本项目新代码采用 GPL-3.0-or-later，见 `LICENSE`。复用的上游代码保留各自
许可证及署名，具体来源和改动随源码记录。`hardware/` 下的 KiCad 设计文件
采用 CERN-OHL-S-2.0，见 [`hardware/LICENSE`](hardware/LICENSE)。
