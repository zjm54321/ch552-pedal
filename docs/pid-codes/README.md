# pid.codes 申请草稿

目标：为本项目申请 `0x1209:0xC552`（2026-11-09 核对：pid.codes 已登记列表和
open PR 中均未占用）。获批前固件继续使用共享测试号 `0x1209:0x0001`。

## 前置条件（pid.codes 要求，未满足会被拒）

- [ ] 公开的源码仓库，计划 `https://github.com/zjm54321/ch552-pedal`。
- [ ] 仓库内包含可修改的 PCB 设计文件（原理图 + 布局）和固件源码。
      硬件原件在仓库之外，只复制进仓库
      `hardware/`，不改原件。
- [ ] 硬件和软件都使用公认的开源许可证，仓库中有对应 LICENSE 文件：
      固件 GPL-3.0-or-later（已有 `LICENSE`）；硬件计划 CERN-OHL-S-2.0。

## 提交步骤

1. Fork `https://github.com/pidcodes/pidcodes.github.com`。
2. 复制本目录下 `org/zjm54321/index.md` 与 `1209/C552/index.md` 到 fork 的同名路径。
3. 再次确认 `1209/C552` 未被他人占用（已登记目录和 open PR）。
4. 提交 PR，标题：`Add PID 1209:C552 for CH552 USB Foot Pedal`。

## 获批后要改的地方

- `firmware/ch552/board_config.h`：`USB_PID 0xC552`
- `via/pedal.json`：`"productId": "0xC552"`
- `via/README.md`、`README.md`、`docs/usage.md`：删除测试号说明
- `make check` 会交叉核对固件与 VIA 定义中的 VID/PID。
