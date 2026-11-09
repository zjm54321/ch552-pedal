# 改键、双脚踏与刷机

## 用 VIA 网页改键

使用支持 WebHID 的桌面 Chrome / Edge，打开 <https://usevia.app/>。
普通键盘功能无需打开网页；网页仅用于配置。

先选固件变体：默认 `test`（`1209:0001`）按下面步骤手动导入定义；
`is0`（`A103:0012`）使用 VIA 现有官方 is0 V3 定义，无需导入，直接进入
**Configure** 授权 **Pedal XXXXXXXXXX**。两者都用协议 13，V2 toggle 关闭，
产品名/序列号保持每芯片唯一。is0 的界面名称/外形来自官方键盘定义，物理输入仍是脚踏。

1. 在 **Settings** 打开 **Show Design tab**。
2. 在 **Design → Load Draft Definition → Load** 导入 `via/pedal.json`，
   导入前关闭 **Use V2 definitions (deprecated)**。固件声明 VIA 协议 13，
   使用 V3 定义，不再使用旧版 V2 定义。
3. 返回 **Configure**，授权并选择目标脚踏。
4. 点击唯一的按键，选择普通键；组合键使用 **Special → Any**。
5. 确认后读回键位，并在文本编辑器或目标应用里检查按下和松开。

两个 ID 都未分配给本项目：`1209:0001` 只供私人开发测试；is0 ID 属于
ai03 / illuminati，公开 `usb.ids` 未列出 `A103`，看来是其自分配。真正的 is0 /
ai03 软件可能误认脚踏。便利变体只用于本地开发，**不得用于销售或认证产品**。

常用组合键输入：

| 用途 | Any 中输入 | 等价十六进制输入 |
|---|---|---|
| Ctrl+C | `LCTL(KC_C)` | `0x106` |
| Ctrl+Shift+K | `LSFT(LCTL(KC_K))` | `0x30e` |
| 右 Ctrl+C | `RCTL(KC_C)` | `0x1106` |
| 只按住左 Ctrl | `KC_LCTL` | `0xe0` |
| 不输出按键 | `KC_NO` | `0x0` |
| 播放宏 0（其余依次为 1、2、3） | `MACRO(0)` | `0x7700` |

当前 VIA 的 Any 输入框要求十六进制的规范写法，勿补齐前导零：例如输入
`0x106`，而不是 `0x0106`。协议与存储内部仍使用完整的 16-bit keycode。

固件接受普通键、修饰键及两者组合和 `MACRO(0..3)`；媒体键和高级 QMK
行为仍会被拒绝，不覆盖原键位。范围见 [protocol.md](protocol.md)。
改键后关闭网页、拔插 USB，检查配置仍然有效。

### VIA 宏

在 VIA 的 **Macros** 页编辑四个宏，再把唯一按键映射到相应 MACRO 键。
可输入 ASCII 文本、`{KC_ENTER}`、`{KC_LCTL,KC_C}`，或 `{250}` 表示 250 ms
延时。明确的按下/松开是 `{+KC_LCTL}` / `{-KC_LCTL}`；动作只能使用单字节
键盘 keycode。延时范围 0..9999 ms。文本按 **US 键盘布局**发送，不是 Unicode；
目标系统的布局、Caps Lock 和已经按住的修饰键会影响结果。

四个宏共享 109 字节（四个 `00` 终止符和最后一字节标记也占空间）。踩下启动后
播放到结束，松开不会中止，播放期间再次踩下被忽略。修改宏、USB reset 或挂起会
安全停止并释放；普通改键不会改变正在播放的序列。VIA 保存采用先 reset、写末字节
`FF`、顺序写块、最后写 `00` 的流程；失败后不要把最后的 `00` 当作成功，请重新保存。
单份存储不能保留更新前的宏，断电后可能为空/不可播放；键位双槽日志不受影响。

2026-11-09 实物单板验收中，VIA 13 连接、is0 自动识别、宏写入与播放、长宏超限拒绝、挂起恢复无卡键及断电保持均已验证正常；挂起电流未提供仪表实测数值，约 0.3 mA 仍为数据手册估算值。挂起时脚踏不能唤醒主机，挂起期间的点击不会补发；恢复后重新去抖并报告当前状态，宏需松开后重新踩下。

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
（注：2026-11-09 实物验收未单独报告两块设备同时接入的情况。）

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

### 推荐工具：WCHISPTool（Windows）

WCH 官方提供的 WCHISPTool 是经过实物验证（Bootloader 2.50）的标准烧录方式：

1. 打开 WCHISPTool，芯片系列选择 **CH55x**，芯片型号选择 **CH552**，下载接口选择 **USB**。
2. 设备进入 ISP：
   - **首次烧录**：板上尚未安装本固件时，短接 PCB 上的 **JP1** 硬件跳线（将 P3.6 引导脚置为有效电平），再插入 USB。
   - **后续升级**：已刷入本固件后，拔出 USB，**踩住脚踏不放并插入 USB**，等待系统识别到 ISP 设备后松开即可，无需再开盖短接跳线。
3. 固件选择：勾选“**目标程序文件1**”复选框，浏览并载入固件文件。
   - 支持 `.hex` 文件（如编译产物 `build/ch552/pedal-is0.hex` 或发布包文件）。
   - 亦支持 `.bin` 文件（发布包中提供的从地址 0 展平、空洞填零镜像）。
4. 参数配置：保持所有芯片配置选项为默认值，下载配置脚选择 **P3.6**。
5. **重要警告**：**不用点“校验”**：下载完成后芯片会直接运行固件、退出 ISP，此时再校验多半报错，不代表刷坏。
6. 点击“**下载**”，等待进度完成并提示成功后重新插拔 USB 进入正常运行模式。

### 命令行工具：wchisp

锁定环境中包含开源工具 `wchisp 0.3.0`，供 Linux/WSL/开发环境使用。在设备已进入 ISP 后，先确认芯片型号与目标：

```sh
nix develop
wchisp probe
wchisp info
# 确认输出为目标 CH552 后，在项目目录执行：
wchisp flash build/ch552/pedal-test.hex
# 本地 is0 便利变体则选择 build/ch552/pedal-is0.hex
```

`wchisp flash` 默认执行擦除、写入、校验并重启；不要添加跳过校验选项。
每次只让目标设备进入 ISP。若多个设备已在 ISP，先用 `wchisp probe` 确认，
再通过全局 `--device INDEX` 指定目标。注意 wchisp 在部分 CH552 bootloader 下存在已知兼容性限制，若遇通讯问题请改用 WCHISPTool 或 CH55xduino 的 `vnproch55x`，不要尝试更改芯片配置位。

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
