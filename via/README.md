# VIA definition

In VIA's Design tab, turn on **Use V2 definitions (deprecated)**, load `pedal.json`,
then authorize the
**Pedal XXXXXXXXXX** product in its WebHID chooser. The firmware declares VIA
protocol 8, and VIA requires a V2 definition for any protocol below 11 (a V3
import reports that the V2 definition is missing). The ten hex characters are
the physical chip's 40-bit ID; the serial descriptor contains the same ID.
The single matrix key `0,0` is the pedal (one layer).

`0x1209:0x0001` is a shared development test pair, **not an allocation to this
project**. Use it only for local development; collisions with other test devices
are possible. Obtain an allocated VID/PID before release/distribution and change
both `firmware/ch552/board_config.h` and `pedal.json`.

The firmware implements VIA protocol 8's bounded one-key subset. Ordinary
keyboard keys and legacy modifier+key chords are supported; macros, media,
layers and advanced QMK keycodes are unavailable/rejected. VIA's generic keycode
picker may show unsupported choices; an `0xFF` reply means rejected, not saved.
The UI key label reflects its mapped key; its physical function is **Pedal**.
VIA remapping and its bootloader-jump command do not flash firmware. Flashing
requires the WCH USB ISP transport and a separately authorized `wchisp` action.
