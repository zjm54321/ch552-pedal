# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-11-09

Initial public release of the CH552G USB foot-pedal firmware.

### Added
- **USB HID & Raw HID**: CH552G full-speed USB implementation providing a standard 8-byte boot keyboard report and a 32-byte bidirectional Raw HID interface (`0xFF60:0x61`).
- **VIA Protocol 13 Support**: Reports VIA protocol version 13, keycodes version `0.0.9`, and firmware version `1` (`PEDAL_FIRMWARE_VERSION 1u`). Uses VIA V3 JSON definitions.
- **Keycode & Chord Support**: Ordinary keyboard usages (`0x04..0xA4`), modifiers (`0xE0..0xE7`), and QMK-style `QK_MODS` modifier combinations (Ctrl, Shift, Alt, GUI with a base key, e.g., Ctrl+C, Ctrl+Shift+K).
- **VIA V11 Macros**: 4 macro slots (`MACRO(0)`..`MACRO(3)`, `0x7700..0x7703`) sharing a 109-byte buffer in DataFlash. Supports US ASCII layout text, tap/down/up actions, and nonblocking delays (0..9999 ms).
- **Power-Fail Safe Keymap Storage**: Dual-slot journal in DataFlash (logical bytes 0..15) with generation counter and CRC-16. Saves write to the inactive slot, verify, commit, and re-verify, guaranteeing mapping integrity across power cuts.
- **CRC-Protected Macro Storage**: Single-copy macro buffer in DataFlash (logical bytes 16..127) with framing validation, write failure latch, readback verification, and CRC-16 check.
- **Held-Pedal ISP Entry**: Holding the pedal during USB connection waits 100 ms for input RC settling, samples P3.3, and jumps to the installed WCH ISP bootloader (`0x3800`) before any DataFlash read or normal USB attachment. Mappings and macros are preserved.
- **USB Suspend Power-Down**: Enters CH552 power-down mode (`PCON |= PD`) during USB suspend with USB-only wakeup (`WAKE_CTRL = bWAK_BY_USB`). No remote wakeup. Resumes without stuck keys or replayed taps.
- **Per-Chip Identity**: Unique USB product and serial strings ("Pedal XXXXXXXXXX") generated from the MCU's 40-bit hardware UID (`3FFA..3FFF`).
- **Two Firmware Variants**:
  - `pedal-test`: VID `0x1209`, PID `0x0001` (pid.codes shared test pair), configured via manual import of `via/pedal.json` (V3, "Use V2 definitions" OFF).
  - `pedal-is0`: VID `0xA103`, PID `0x0012` (ai03 is0 keyboard ID), automatically recognised by the VIA web app without manual JSON import.
- **Flash & Memory Optimization**: Explicit XRAM pointers for core/storage/playback/transport state fit SDCC output into 13,651 emitted Flash bytes (limit `0x3800`, 629 bytes free), with XRAM ending at `0x0358` and 187 bytes linker-available stack.
- **Hardware Acceptance**: Verified on physical CH552G hardware (bootloader 2.50) for WCHISPTool flashing (JP1 hardware jumper), per-chip UID string enumeration ("Pedal 95BA7575BF"), VIA 13 connection, is0 auto-recognition, key remapping and persistence, modifier chords, macro editing/playback/persistence/oversize rejection, suspend/resume, and held-pedal ISP entry.
