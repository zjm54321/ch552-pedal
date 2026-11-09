# VIA definition

Both variants use VIA V3: turn **off** **Use V2 definitions (deprecated)**.
For `test` (`0x1209:0x0001`), load minimal `pedal.json` in VIA's Design tab.
For the local convenience `is0` variant (`0xA103:0x0012`), VIA's existing official
is0 definition supplies the one-key layout without a manual import. Authorize the
**Pedal XXXXXXXXXX** product in the WebHID chooser in either case. Firmware declares VIA
protocol 13 and keycodes version `0x00000009`. The ten hex characters are
the physical chip's 40-bit ID; the serial descriptor contains the same ID.
The single matrix key `0,0` is the pedal (one layer).

`0x1209:0x0001` is a shared development test pair, **not an allocation to this
project**. It is for private/local development only; collisions are possible.
The is0 IDs belong to ai03 / illuminati's keyboard, **not to this pedal**.
`0xA103` is absent from public `usb.ids` and appears self-assigned by ai03.
Real is0 or ai03 software may misidentify this pedal as is0. Neither variant is
for sold or certified products; obtain this project's own IDs before release.
The two builds differ only in VID/PID, not product/serial identity, storage,
protocol or macros. Run `make firmware VARIANT=test` / `VARIANT=is0`, or
`make check` to build and audit both with independent objects.

## Official is0 definition provenance

`is0.json` is vendored **verbatim**, including its KLE geometry, from
[the-via/keyboards v3/other/is0/is0.json](https://github.com/the-via/keyboards/blob/9e3e9f4aa9f50f9439fdebc747560f51aaf109a0/v3/other/is0/is0.json),
commit `9e3e9f4aa9f50f9439fdebc747560f51aaf109a0`, under the upstream
[GPL-3.0 license](https://github.com/the-via/keyboards/blob/9e3e9f4aa9f50f9439fdebc747560f51aaf109a0/LICENSE).
SHA-256: `f28225f3114cc2f8ed01872e919401ef5f45d5524544c4a8251afbab8a9fa6f5`.
Its matrix is 1x1 with key `0,0` and no menus. The matching QMK keyboard is
[`keyboards/illuminati/is0`](https://github.com/qmk/qmk_firmware/tree/master/keyboards/illuminati/is0).
Hardware acceptance on 2026-11-09 confirmed automatic matching and UI recognition for `is0` on physical hardware.

## Supported protocol

The firmware implements VIA protocol 13's bounded one-key subset. Ordinary
keyboard keys, legacy modifier+key chords and `MACRO(0)` through `MACRO(3)`
(`0x7700..0x7703`) are supported. Four V11-format macros share 109 bytes, including
terminators and the final write marker. Text uses the US layout; tap/down/up use
one-byte keyboard keycodes, and delays are 0..9999 ms. Media,
layers and advanced QMK keycodes are unavailable/rejected. VIA's generic keycode
picker may show unsupported choices; an `0xFF` reply means rejected, not saved.
The UI key label reflects its mapped key; its physical function is **Pedal**.
VIA remapping and its bootloader-jump command do not flash firmware. Flashing
requires the WCH USB ISP transport and an authorized flashing tool (such as WCHISPTool or `wchisp`).

Macro editing uses VIA's Macros tab; remap the pedal to the matching MACRO key.
Playback finishes even if the pedal is released, and repeated presses during
playback are ignored. Macro writes/reset, USB reset and suspend abort safely.
CRC/structure/framing checks reject partial uploads, but there is no second
macro copy: an interrupted update can lose the previous macros. See
`docs/protocol.md`. Real VIA macro UI behavior (editing, playing, persistence,
and oversize rejection) was verified on physical hardware on 2026-11-09.
