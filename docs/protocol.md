# VIA 与按键范围

## 设备模型

每块脚踏是独立的 USB 键盘，并带一个 VIA Raw HID 接口：usage page
`0xFF60`、usage `0x61`、双向 32 字节报告。矩阵只有 `[0,0]`，只有一层。
不同设备共享同一型号定义，但不共享键位或存储；配置请求必须发送给选中的
HID device 对象，不能按 VID/PID 广播。

协议实现位于 `firmware/core/pedal_core.c`，与 USB 和 DataFlash 驱动分开。
声明的 VIA 协议版本是 **13**，响应版本字节为 `00 0D`，使用最小 V3 定义。
默认 `test` 使用 `via/pedal.json`；本地便利 `is0` 使用未改动的官方
`via/is0.json`，仅 VID/PID 不同。二者矩阵、协议、存储、宏及 per-chip 身份一致，
V2 toggle 都关闭；ID 限制见 `via/README.md`。

## 支持的按键

| 按键 | 16-bit keycode | 输出 |
|---|---|---|
| Space（默认） | `0x002C` | 按住发送 Space，松开释放 |
| C | `0x0006` | 普通按键 |
| Ctrl+C | `0x0106` | 左 Ctrl 与 C 同时按下/松开 |
| Ctrl+Shift+K | `0x030E` | 左 Ctrl、左 Shift、K 同时按下/松开 |
| 右 Ctrl+C | `0x1106` | 右 Ctrl 与 C |
| 左 Ctrl | `0x00E0` | 单独修饰键，可配合另一块脚踏或普通键盘 |
| 无动作 | `0x0000` | 不发送按键，上电进 ISP 仍有效 |
| `MACRO(n)`，n = 0..3 | `0x7700 + n` | 踩下播放对应宏，松开不终止 |

支持键盘 usage `0x04..0xA4`、修饰键 `0xE0..0xE7`，及传统 QMK
`QK_MODS` 修饰键编码。高字节低四位分别为 Ctrl / Shift / Alt / GUI；
高字节的 `0x10` 位表示使用右侧修饰键。例如 `0x03` 是左 Ctrl+左 Shift，
`0x13` 是右 Ctrl+右 Shift。

宏使用协议 12/13 字典的 `0x7700..0x7703`，不使用旧版宏 keycode。
媒体/鼠标键、层切换、Mod-Tap 和 Tap-Dance 未实现。
VIA 通用界面可能仍显示这些选项；固件拒绝不支持的 keycode，不改变原键位。
一只脚踏可以输出“修饰键 + 一个普通键”；两只脚踏也能各自输出不同按键。
操作系统对不同键盘的修饰键/相同键合并行为，应以实际应用测试结果为准。

改键时若脚踏正按住，当前已按下的报告保持不变；松开先释放原报告，
下次踩下才使用新键位，避免修饰键卡住。

## 实现的命令

| 命令 | 内容 |
|---|---|
| `0x01` | 协议版本 13：`01 00 0D` |
| `0x02`，值 `0x04` | 固件版本：`02 04 00 00 00 01` |
| `0x02`，值 `0x06` | keycodes version：`02 06 00 00 00 09`（四字节大端 BCD） |
| `0x04 / 0x05` | 读取/设置 `[layer=0,row=0,col=0]` 的 keycode |
| `0x06` | 恢复默认 Space，且仅在变化时请求保存 |
| `0x0B` | 请求进入 WCH ISP，不清空键位 |
| `0x0C / 0x0D` | 宏数量 4 / buffer 容量 109（大端 `00 6D`） |
| `0x0E / 0x0F` | 宏 buffer 读取/写入；大端 16-bit offset、1-byte size、最多 28 data bytes |
| `0x10` | 清空四个宏，不改变键位 |
| `0x11` | 层数为 1 |
| `0x12 / 0x13` | 读取/写入两字节大端 keymap buffer |

非法坐标、越界 buffer、未知命令与不支持的键位返回 `0xFF`，不修改映射。
非 32 字节请求不交给协议处理。buffer 部分写入先构造完整候选 keycode，
验证后才更新；合法空操作不请求写入。
所有其他 GET/SET keyboard value id 都返回未知命令，包括旧 uptime、布局和矩阵查询。
宏读取原样回显 offset/size，截取容量内数据并在超出的部分填零（包括完全越界读取），
所以 VIA 最后一块固定 28 字节的读取不会越界。宏写入越界或 size > 28 时返回 `FF`。

## 存储记录

核心保存记录共八字节：`50 44 01 00 key_hi key_lo crc_hi crc_lo`。
CRC-16/CCITT-FALSE 覆盖前六字节。空白、损坏、版本不符或不支持的 keycode
回退为 Space；启动回退不自动写 Flash。纯协议核心只返回保存请求。

芯片上的 DataFlash 由 `firmware/ch552/storage.c` 写成双槽日志，每槽八字节：
`A5 02 gen_hi gen_lo key_hi key_lo crc_hi crc_lo`。保存时只写非活动槽，校验后才
发布，所以任一步断电都保留上一份有效键位。上述旧格式记录仍可读取，首次保存时迁移。
保存失败时 VIA 返回 `0xFF`，再次发送相同键位会真正重写，而不是当作无变化。

上电是否进 ISP 必须在读取该记录之前判断。因此即使映射为无动作或存储记录
损坏，踩住插 USB 仍走相同启动入口。烧录固件与保存改键是两条独立路径。

## 宏格式、存储与断电边界

宏使用 VIA Macro API V11（协议 >= 11）：四个 `00` 终止的序列，之后只能是零填充。
文本用 US 布局 ASCII；动作是 `01 01 key`（tap）、`01 02 key`（down）、
`01 03 key`（up）和 `01 04 <十进制 ASCII 毫秒> 7C`（delay，0..9999）。
动作 key 是一个字节，限普通键盘 usages / 修饰键 / KC_NO。支持可打印 ASCII、
Backspace、Tab、LF、Escape、DEL；不支持 Unicode、媒体动作或未知控制字节。
动作参数中的 `00` 不会被误当宏分隔符。所有 token 和延时索引都受长度检查。

CH552 的 128 个逻辑 DataFlash 字节分配如下（物理地址 = `C000 + 2 * logical`）：

| 逻辑地址 | 内容 |
|---|---|
| 0..15 | 原有键位双槽日志，格式与兼容性不变 |
| 16..124 | 109-byte 宏 buffer；最后字节是 VIA 上传标记 |
| 125 | 固件有效标记 `A5`（修改前先写并读回 `00`） |
| 126..127 | CRC-16/CCITT-FALSE，大端，覆盖全部 109 bytes |

上传流程为 reset（清零）、最后字节写 `FF`、从 offset 0 连续写块、最后字节写 `00`。
固件只对实际接收字节内完整的四个宏终止符作确认；reset 留下的零不能代替未接收块。
写入失败、跳过/乱序块、非法写入或 USB epoch 改变会锁定本次上传为失败，最后 `00`
不能解除失败。最终结构验证及完整读回成功后计算/写入 CRC，最后发布有效标记。
启动时同时检查标记、CRC 和结构；无效时 RAM buffer 为零且禁止播放，不自动写入。
写宏前停止当前播放，已排队的未来动作丢弃，保留活动 DMA 的顺序并随后发送全释放。

这是**单份**宏存储，不是键位的双槽日志：修改开始后旧宏不保证保留。逐字节断电
可能保留旧宏、清空的宏、完整新宏，或使宏无效；不会播放未验证的部分宏。CRC 可检测
单比特损坏及常见撕裂，但 16-bit CRC 有碰撞可能，也无法判断主机是否有意发送完整但
不同的有效序列。它不提供对任意损坏的绝对保证。VIA 最后 `00` 不是完整性证明；
主机没有发送长度/校验和，只有本次连续接收范围、失败锁、结构、读回和 CRC 可验证。
不变化的 payload / CRC / 标记字节跳过写入，重复清空已空的宏不写 Flash；仍然不能
避免 VIA 每次 reset/rewrite 带来的磨损，按 DataFlash 每 cell 约 10,000 次写入规划。

播放由主循环状态机逐动作推进；前一报告完成后才推进下一报告或开始延时，不阻塞
USB ISR。最多六个普通键及八个修饰键；溢出/未知动作立即停止并全释放。文本的临时
Shift 不会释放宏显式按住的 Shift。每次 tap 发送按下和恢复原 held 状态，显式 held
状态在完成/中止时全部清零。释放脚踏不终止，播放/最终释放尚未完成时再次踩下被忽略。
改键保留当前播放，宏写入/reset、USB reset/epoch 改变和挂起会中止。

## 挂起 power-down

ISR 只更新 USB/input epoch 和取消键盘 DMA；主循环在存储操作返回后、无 VIA 回复 /
Raw mailbox / ISP / 活跃宏工作时进入 PD。先禁用中断并在 SAFE_MOD 下仅设置
`WAKE_CTRL = bWAK_BY_USB`，重新检查 `USB_MIS_ST & bUMS_SUSPEND` 和待处理 USB
事件后才 `PCON |= PD`；醒后 SAFE_MOD 下清除 WAKE_CTRL，再恢复原中断状态。
不设置 P3.3 wake，不支持 remote wakeup；普通 held 报告在主机处保持，恢复后用
八个新的 1 ms 样本协调当前状态。宏挂起时本地全释放，恢复后发送零，不重播挂起点击。
Timer2 在 PD 不前进，醒后重新装载其计数；uptime/延时不包含睡眠时长，去抖重新开始。

**未实测估算**：数据手册 5V `ISLP5` 为典型 0.1 / 最大 0.2 mA；USB 保持连接的
1.5 kΩ pull-up 与主机 15 kΩ pull-down 约消耗 0.2 mA，所以松开约 0.3 mA 典型。
踩住 P3.3 再加 `IUP5` 典型 70 / 最大 140 µA，约 0.37 mA 典型、0.54 mA 的粗略
上界，另加板上其他负载。未关闭 USB LDO；不能使用 LDO-off 的 0.008 mA 数值。
2026-11-09 实物单板验收中，VIA 13 连接、is0 自动识别、宏界面写入与播放、挂起恢复不重新枚举及无卡键均已验证正常；挂起电流未提供仪表实测读数，上述约 0.3 mA 仍为数据手册估算值。双设备同时使用未单独报告。

### 核对来源

- VIA app：`src/utils/keycodes-version.ts:34-84`、`src/store/firmwareSlice.ts:74-135`；
  `src/utils/keyboard-api.ts:584-647`、`src/utils/macro-api/macro-api.v11.ts:110-234`；
  `src/utils/key-to-byte/v12.ts:28-29`、`v13.ts:1-28`。
- QMK：`quantum/via.c:425-450`（原样回显）、`quantum/dynamic_keymap.c:140-171`，
  `quantum/send_string/send_string.c:37-58,108-142,153-194`。
- WCH CH552 HID sample `WCH-CH552-HID-USB.c:577-589`；CH552 datasheet V1.8
  §7.2 / `CH552DS1-V1.8.txt:1108-1156`、§18.2 / `CH552DS1-V1.8.txt:3600-3630`。
  核对参考上述上游代码库与数据手册对应章节。
