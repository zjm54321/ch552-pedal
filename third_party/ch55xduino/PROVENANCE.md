# Pinned CH55xduino subset

Source: <https://github.com/DeqingSun/ch55xduino>
Commit: `c9f9a2a6516255284064a9dd248670545f25a322`
Vendored on 2026-11-03. Builds use these local files only; no downloads.
Upstream root `LICENSE` (LGPL-2.1) is copied verbatim here. Original notices,
including WCH header notices and the LUFA permissive notices, are preserved.

## Upstream paths

Prefix `ch55xduino/ch55x/`:

- `USBhandler.c/h`, `USBconstant.c/h` came from
  `libraries/Generic_Examples/examples/05.USB/qmkCompatibleKeyboard/src/userQmkCompatibleKeyboard/`.
- `include/ch5xx.h`, `include/ch5xx_usb.h` came from `variants/ch552/include/`.
- `usbCommonDescriptors/StdDescriptors.h` and `HIDClassCommon.h` came from
  `cores/ch55xduino/usbCommonDescriptors/` (LUFA/Dean Camera notices in-file).

## Local adaptations

- `USBhandler.c/h`: retain upstream USB SETUP/IN/OUT state machine, endpoint DMA
  layout, interrupt dispatcher and CH552 USB initialization. Add boot HID class
  requests, HID descriptors, generic descriptor pointer for dynamic XRAM strings,
  supported configuration/interface bounds, reset hooks, and mailbox-aware
  endpoint CLEAR_FEATURE behavior. Remove OS-detection callbacks. Local fixes on
  2026-11-09: SET_REPORT arms its zero-length DATA1 IN status stage; the resume
  branch clears only `UIF_SUSPEND` instead of writing `USB_INT_FG = 0xFF`;
  endpoint SET/CLEAR_FEATURE and GET_STATUS are validated, limited to EP1/EP2
  and report real per-direction halt state.
- `USBconstant.c/h`: retain upstream descriptor structures and Raw HID usage
  `0xFF60/0x61`; use an eight-byte boot keyboard with no report ID or mouse,
  correct Raw HID Data/Variable/Absolute input/output flags, 32-byte Raw reports,
  local development VID/PID, and chip-ID-derived product/serial strings.
- `firmware/ch552/usb_transport.c` replaces upstream Arduino keyboard/mouse/VIA
  callbacks with nonblocking DMA ownership, a bounded main-loop Raw mailbox, and
  reset epochs. Its USB initialization and endpoint completion sequence derive
  from `USBHIDKeyboardMouse.c` in the same example.
- `firmware/ch552/storage_flash.c` adapts `cores/ch55xduino/eeprom.c`'s CH552
  DataFlash access sequence, limits writes to logical bytes 0..15, checks command
  status and readback, and reports failure. `firmware/ch552/storage.c` is the new
  host-testable two-slot journal above it and contains no upstream code.
- `firmware/ch552/main.c`'s ISP entry sequence derives from
  `cores/ch55xduino/USBCDC.c`: detach USB, disable interrupts, zero TMOD, wait
  100 ms, call the installed bootloader at `0x3800`.
- The four descriptor/register support headers are otherwise unchanged.

The board glue is GPL-3.0-or-later as marked in-file; adapted library code retains
its upstream LGPL provenance. No upstream `via.c`, Arduino runtime, board manager,
or alternative USB stack is included.
