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
- Suspend: edges are not queued while suspended. On resume, or on a complete
  suspend/resume observed late through `UsbInputEpoch`, main drops queued edges
  and reports only the current debounced state. Low-power sleep and remote wakeup
  are not implemented: suspend current is unmeasured and may exceed the USB limit.
- Endpoint halt is tracked per direction for EP1/EP2 only, validated before any
  state change, reported by GET_STATUS and preserved by send/completion paths.
- Runtime VIA `0x0B` queues its reply and a keyboard release, allows up to 100 ms
  for ACKs, then detaches USB, disables interrupts/timers, zeros TMOD, waits
  another 100 ms and calls ISP. Neither ISP-entry path writes configuration.

USB DMA is fixed at `[0,8)`, `[10,138)`, `[138,266)`; SDCC allocations start at
266. All application HEX addresses must be below `0x3800`; DataFlash and the
installed bootloader are not part of the image. `--nooverlay` prevents ISR and
main/core local-variable overlay. Build products and the address/descriptor audit
are in `build/ch552/`; `make firmware` never accesses USB hardware.

See `third_party/ch55xduino/PROVENANCE.md` for the pinned reused stack and local
changes, and `via/README.md` for the development-only IDs and configuration flow.
