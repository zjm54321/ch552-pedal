# CH552G board implementation

- P3.3 is active-low with the internal pull-up; the established external 0.1 uF
  capacitor is unchanged. The internal clock is explicitly set to 16 MHz.
- Before **any DataFlash read or core initialization**, startup waits 100 ms for
  the input RC, then samples for at most 40 ms. Twenty consecutive low samples
  enter the installed ISP at `0x3800`, without normal USB attachment or writes.
- Timer2 runs at Fsys with a 16000-count auto-reload (1 ms). Runtime input needs
  eight observed stable 1 ms intervals. USB and Timer2 vectors live in `main.c`.
- Normal USB is a boot keyboard (eight-byte input, no report ID) plus a separate
  32-byte Raw HID interface, usage page `0xFF60`, usage `0x61`. Per-chip product
  and serial strings use all five UID bytes at `3FFA,3FFC,3FFD,3FFE,3FFF`.
- USB ISR callbacks own transport only. Raw OUT remains NAK until the one reply
  is ACKed; malformed lengths are discarded before core processing. Endpoint
  hardware retries unacknowledged IN reports. Main has eight queued keyboard
  reports plus the active DMA report; no queued report is overwritten. If a host
  stops polling long enough to fill the queue, new edges are deferred and the
  latest debounced state is reconciled when space returns. Unlimited taps during
  an unlimited host stall cannot all be retained; the eventual release is kept.
- USB reset/configuration changes clear DMA ownership and notify main by epoch.
  Main discards old-session work, releases, and resynchronizes physical state.
- Saved mapping uses a two-slot journal in DataFlash logical bytes 0..15, at even
  addresses `0xC000..0xC01E`. Each eight-byte slot holds commit byte, version 2,
  a 16-bit generation, the keycode and CRC. A save writes only the inactive slot
  with commit byte zero, reads it back, publishes the commit byte, then reads it
  back again; the active slot is untouched until the new one is verified. Power
  loss at any write boundary therefore leaves a valid mapping. A failed save
  returns VIA `0xFF`, keeps the RAM map and held report, and marks persistence
  dirty so re-sending the same mapping really writes. A valid version-1 record
  from the previous build is read at slot 0 and migrates on the first save.
  Commit bytes take two writes per save, so at the datasheet's roughly 10,000
  writes per cell, expect about 10,000 mapping changes.
- VIA protocol 13 reports keycodes version `00 00 00 09` and firmware version
  `00 00 00 01`. Only these two GET keyboard values are supported; all other
  GET/SET values return unknown-command. Definitions are V3, V2 off: minimal
  `via/pedal.json` for test IDs, official unmodified `via/is0.json` for is0 IDs.
- Four `MACRO(0..3)` mappings use `0x7700..0x7703`. Logical DataFlash 16..124
  holds the 109-byte V11 macro buffer; 125 is the validity byte and 126..127
  the big-endian CRC-16 over all 109 bytes. The keymap journal is unchanged.
  Reset/start-FF/contiguous chunks/final-zero framing, a live failure latch,
  complete structure validation and readback precede CRC/validity publication.
  Unchanged bytes are not rewritten. A torn update disables macros rather than
  playing partial data; the previous macros are not retained in a second copy.
  See `docs/protocol.md` for the exact power-fail guarantees and CRC limitations.
- The nonblocking macro player uses 144 code-space bytes for US ASCII/shift.
  Tap/down/up/delay actions use single-byte HID keyboard keycodes; delays are
  0..9999 ms. Main advances only after preceding reports complete. Six ordinary
  keys plus eight modifiers fit; malformed/overflow/epoch events release all.
  Releasing the pedal does not abort, and represses during playback are ignored.
  Remapping leaves playback intact; macro writes/reset abort before mutation.
- Suspend: no suspended taps are queued/replayed. Main aborts macros, latches
  their zero release for resume, and enters power-down only after synchronous
  storage work, with no reply/mailbox/ISP work pending. With interrupts masked,
  SAFE_MOD arms `bWAK_BY_USB` only; the final suspend/status/USB-event check gates
  `PCON |= PD`. Wake clears WAKE_CTRL under SAFE_MOD and restores interrupts.
  No P3.3 wake or remote wakeup. A normal held report stays latched at the host
  until eight fresh awake debounce samples reconcile it; macros need a new edge.
  Timer2 pauses during PD; its phase and debounce restart, uptime excludes PD.
  Unmeasured 5V estimate: roughly 0.3 mA released / 0.37 mA held (0.1 mA MCU,
  ~0.2 mA USB pull-up, +70 µA held-input pull-up); other board loads add current.
  On 2026-11-09 hardware acceptance, USB suspend and resume without stuck keys
  or event replay was confirmed on a physical board; no numeric suspend current
  was measured, so the ~0.3 mA figure remains a datasheet estimate.
- Endpoint halt is tracked per direction for EP1/EP2 only, validated before any
  state change, reported by GET_STATUS and preserved by send/completion paths.
- Runtime VIA `0x0B` queues its reply and a keyboard release, allows up to 100 ms
  for ACKs, then detaches USB, disables interrupts/timers, zeros TMOD, waits
  another 100 ms and calls ISP. Neither ISP-entry path writes configuration.

USB DMA is fixed at `[0,8)`, `[10,138)`, `[138,266)`; SDCC allocations start at
266. All application HEX addresses must be below `0x3800`; DataFlash and the
installed bootloader are not part of the image. `--nooverlay` prevents ISR and
main/core local-variable overlay. All target state/report pointers are qualified
XRAM pointers, while the ASCII tables remain in code space; host tests erase
the target qualifier. No generic descriptor pointer or ISR safety was removed.

## Builds and measured fit

`make firmware VARIANT=test` (default) and `make firmware VARIANT=is0` compile
the same sources with only `-DUSB_VID/-DUSB_PID` different. Defaults remain
`1209:0001`; the is0 convenience build borrows `A103:0012`. No product/serial
string change: both are **Pedal XXXXXXXXXX** from the chip UID. Neither ID pair
is allocated to this project; restrictions/provenance are in `via/README.md`.
Objects/maps/memory/audits are isolated in `build/ch552/test/` and
`build/ch552/is0/`; images are `build/ch552/pedal-test.hex` and
`build/ch552/pedal-is0.hex`. `make check` builds/audits both, never USB hardware.

2026-11-09 fit measurements (same `--model-large --nooverlay` throughout):

| Candidate | Flash span / highest | XRAM end | Linker-available stack |
|---|---|---|---|
| Baseline protocol 8 | 10,084 / `0x2763` (10,028 emitted) | `0x0290` | 182 bytes |
| Protocol 13 + macros + PD, generic pointers | 15,832 / `0x3DD7`, rejected | `0x0384` | 130 bytes |
| Explicit XRAM core/storage/playback/transport pointers | 13,707 / `0x358A` (13,651 emitted) | `0x0358` | 187 bytes |

The sole size optimization saves 2,125 Flash bytes and leaves 629 bytes below
`0x3800`; all bounds/CRC/framing checks and the 109-byte macro capacity remain.
This stack figure is linker placement, not measured runtime high-water.

See `third_party/ch55xduino/PROVENANCE.md` for the pinned reused stack and local
changes, and `via/README.md` for the development-only IDs and configuration flow.

## Hardware acceptance (2026-11-09)

Tested on physical CH552G hardware (WCH bootloader 2.50) using WCHISPTool:
- Flashing via JP1 hardware jumper succeeded;
- Enumeration as per-chip UID product/serial ("Pedal 95BA7575BF");
- VIA 13 connection and automatic is0 layout recognition;
- Key remap and persistence across power-cycling (DataFlash dual-slot journal);
- Modifier chords (Ctrl+Shift+K etc.);
- V11 macro write, playback, persistence, and oversize rejection;
- Suspend power-down and wake without stuck keys or replayed taps;
- Held pedal at power-up successfully enters WCH bootloader without erasing keymaps or macros.

Note: Suspend current was not measured with a meter (datasheet estimate only). Simultaneous dual-unit operation was not separately reported.
