# CH552 USB pedal

## Scope

- This is an independent project. Keep all code, toolchains, build outputs and
  project configuration here; never change the system Nix configuration for this project.
- The original KiCad working directory, outside this repository, is read-only.
  Hardware is established; do not redesign or rewrite the KiCad files.
- Target: CH552G, USB full speed, one active-low pedal on P3.3 per USB device.

## Required behavior

- A held pedal during USB power-up enters the existing WCH ISP bootloader before
  normal keyboard operation, independently of saved keymap validity. Do not erase
  user mappings when entering ISP. Preserve bootloader Flash from `0x3800` upward.
- Standard USB HID keyboard output, including modifier + key chords.
- Browser remapping through VIA Raw HID, with persistent per-device keymaps.
  Prefer a small accurately implemented protocol subset over full QMK/Vial.
- Two identical units can remain connected and be configured independently.
  Give physical units identifiable USB names/serials; do not use VID/PID as a
  unique physical-device identifier.
- VIA remapping and entering ISP are separate from actually flashing firmware.
  Describe the real flashing tool and transport; do not label VIA a firmware flasher.

## Engineering

- Build with a project-local locked Nix flake. Do not depend on the system flake.
- Reuse a pinned upstream CH55x USB implementation where practical, preserve
  attribution/licenses, and state which parts were changed. No floating downloads.
- Complex build/generation helpers use Python 3. Keep incidental shell steps small.
- Bound every host-supplied VIA report, index, length and supported keycode before
  changing device state. Unsupported behavior must not become an unrelated key.
- A key held while remapping must release the exact report originally pressed.
- Never flash USB hardware, erase memory, or change device option bytes implicitly.

## Evidence

- Writers run only focused checks for their owned files and provide exact commands,
  exit statuses and artifacts. Parent owns final integrated validation.
- Host tests establish pure keycode/protocol/persistence/state logic. Real SDCC
  builds establish the target code fits application Flash and XRAM. Neither is a
  claim of physical USB enumeration, ISP operation or browser/device acceptance.
- Hardware acceptance requires a connected pedal and explicit flashing authority.
