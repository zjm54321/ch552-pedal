# 硬件设计

KiCad 原理图、PCB、CH552G 符号/封装库和测试外壳 STL，复制自
本地 KiCad 工作目录（2026-11-09 快照）。
原始目录仍是硬件设计的工作副本；在那里修改后再同步到这里。

- 原理图：`脚踏开关.kicad_sch`
- PCB：`脚踏开关.kicad_pcb`
- 主控：CH552G；脚踏信号 P3.3 低电平有效。
- 引导入口：JP1 为硬件 ISP 引导跳线（短接 P3.6），供首次烧录时引导；安装本固件后，插 USB 时踩住脚踏（P3.3）即可软件直接进入 ISP，无需再短接 JP1。

## 许可证

本目录下的硬件设计文件采用 CERN-OHL-S-2.0（Strongly Reciprocal），全文见
[`LICENSE`](LICENSE)。固件和其他软件采用仓库根目录 `LICENSE` 中的
GPL-3.0-or-later。

SPDX-License-Identifier: CERN-OHL-S-2.0
