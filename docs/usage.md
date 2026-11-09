# 改键、双脚踏与刷机

## 用 VIA 网页改键

使用支持 WebHID 的桌面 Chrome / Edge，打开 <https://usevia.app/>。
普通键盘功能无需打开网页；网页仅用于配置。

1. 在 **Settings** 打开 **Show Design tab**。
2. 在 **Design → Load Draft Definition → Load** 导入 `via/pedal.json`，
   导入前先打开 **Use V2 definitions (deprecated)**。固件声明 VIA 协议 8，
   VIA 对协议 11 以下的设备只查 V2 定义，用 V3 导入会提示找不到 V2 定义。
3. 返回 **Configure**，授权并选择目标脚踏。
4. 点击唯一的按键，选择普通键；组合键使用 **Special → Any**。
5. 确认后读回键位，并在文本编辑器或目标应用里检查按下和松开。

常用组合键输入：

| 用途 | Any 中输入 | 等价十六进制输入 |
|---|---|---|
| Ctrl+C | `LCTL(KC_C)` | `0x106` |
| Ctrl+Shift+K | `LSFT(LCTL(KC_K))` | `0x30e` |
| 右 Ctrl+C | `RCTL(KC_C)` | `0x1106` |
| 只按住左 Ctrl | `KC_LCTL` | `0xe0` |
| 不输出按键 | `KC_NO` | `0x0` |

当前 VIA 的 Any 输入框要求十六进制的规范写法，勿补齐前导零：例如输入
`0x106`，而不是 `0x0106`。协议与存储内部仍使用完整的 16-bit keycode。

固件只接受普通键、修饰键及两者组合；VIA 中的媒体键、宏和高级 QMK
行为不属于首版功能，选择后不会覆盖原键位。范围见 [protocol.md](protocol.md)。
改键后关闭网页、拔插 USB，检查配置仍然有效。

## 同时使用两个设备

两块设备可以同时连接，不需要各自占用不同 VID/PID。每块设备由芯片 UID
生成 USB 序列号，并在产品名中加入后缀，便于浏览器中识别。

- 分别授权两个设备，在 VIA 的设备选择中切换；每次改键只作用于当前设备。
- 第一次可只插一块，确认其名称后在外壳标注“左/右”，再插第二块。
- 先给左右设备配置不同的测试键，例如 F13 与 F14，分别踩下确认没有串改，
  然后再设置实际快捷键。
- 测试同时踩下，以及一只松开、另一只仍按住的情况。两个设备若输出相同键，
  或跨设备组合修饰键，目标操作系统/应用的合并规则会影响效果。

VID/PID 用于查找型号定义，不能用于唯一识别左右设备。浏览器的 WebHID
接口不向网页暴露 USB serial，因此不能只依靠序列号解决选择问题。

## 踩住上电进入 ISP

先拔掉 USB，踩住脚踏不放，再插入 USB，等待 ISP 设备枚举后松开。
此入口先于键位读取和正常 USB 键盘启动；映射为无动作或保存记录损坏，
不会影响启动判断，也不会因进入 ISP 而清空配置。

这是固件启动阶段跳转到芯片原有 bootloader。若应用程序本身已损坏、
无法执行启动代码，需要使用板上的硬件 ISP 入口；若 bootloader 也被破坏，
则需要编程器恢复。首次烧录尚未安装本固件时，同样使用现有硬件 ISP 方法。

VIA 协议的 `0x0B` 命令也可以请求进入 ISP；是否提供对应按钮取决于前端。
**VIA 改键不是刷固件**：进入 ISP 后，设备使用 WCH 烧录协议而非 VIA。

## 实际刷入固件

本项目默认构建和检查都不访问 USB。只有明确选择烧录工具并执行刷入，
才会擦写应用 Flash。升级前保存 VIA 布局或记下键位，升级后读回确认；
不同烧录器的擦除策略可能影响 DataFlash。

锁定环境包含 `wchisp 0.3.0`。在设备已进入 ISP 后，先确认芯片型号与目标：

```sh
nix develop
wchisp probe
wchisp info
# 确认输出为目标 CH552 后，在项目目录执行：
wchisp flash build/ch552/pedal.hex
```

`wchisp flash` 默认执行擦除、写入、校验并重启；不要添加跳过校验选项。
每次只让目标设备进入 ISP。若多个设备已在 ISP，先用 `wchisp probe` 确认，
再通过全局 `--device INDEX` 指定目标。CH552 若与该版本工具不兼容，
使用 WCHISPTool 或 CH55xduino 的 `vnproch55x`，不要尝试更改芯片配置位。

### 网页刷机选项

CH55xduino 提供现成的 [WebUSB 烧录页](https://deqingsun.github.io/ch55xduino/bootloaderWebtool/)，
接受 Intel HEX，支持 CH552，源码允许的 bootloader 版本为 2.3.1–2.5.0。
其已核对版本的校验循环存在发送写入命令而非校验命令的问题，因此首版
不把该网页的“Flash Finished”当作刷入校验证据；优先使用上面的原生工具。

Windows 上 WebUSB 访问 ISP 接口可能需要 WinUSB 驱动；这只涉及 bootloader，
不要替换正常键盘 HID 接口的驱动。更换 ISP 驱动也可能影响原生 WCH 工具。

### Windows / WSL USB 归属

Windows 浏览器改键或刷机时，设备必须由 Windows 持有。若之前附加给 WSL，
在 Windows PowerShell 查看并分离：

```powershell
usbipd list
usbipd detach --busid <BUSID>
```

在 WSL 内使用 `wchisp` 时，按 USB/IP 的流程将目标设备附加到 WSL；进入 ISP
后会重新枚举，必要时重新检查 BUSID。Windows 浏览器和 WSL 不能同时占用
同一 USB 设备。Linux 还需要匹配 `4348:55e0` 的 USB 访问权限；权限不足时
先解决设备权限，不要用反复刷入作为探测手段。

## 来源

- [VIA 定义与配置](https://www.caniusevia.com/docs/specification)
- [VIA Any 输入解析](https://github.com/the-via/app/blob/main/src/components/inputs/custom-keycode-modal.tsx)
- [WebUSB 烧录源码（已核对版本）](https://github.com/DeqingSun/ch55xduino/blob/c9f9a2a6516255284064a9dd248670545f25a322/bootloaderWebtool/ch55xbl.js#L175-L206)
- [WSL USB/IP](https://learn.microsoft.com/en-us/windows/wsl/connect-usb)
