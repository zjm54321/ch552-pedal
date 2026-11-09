/* SPDX-License-Identifier: GPL-3.0-or-later
 * USB-only wake sequence follows WCH's CH552 HID sample and datasheet §7.2. */
#include "USBhandler.h"
#include "usb_power.h"

uint8_t USBPowerDown(uint8_t blocked) {
  uint8_t slept = 0;
  uint8_t saved_ea = EA;
  EA = 0;
  if (!blocked && UsbSuspended && !UsbRawHeld && !UsbRawBusy) {
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
    WAKE_CTRL = bWAK_BY_USB; /* Never local/pedal wake or remote wakeup. */
    SAFE_MOD = 0;
    /* Arm hardware wake BEFORE the final status check. Keep interrupts masked
     * through PD so a late resume cannot be consumed by an ISR in this window.
     * Already-pending USB events are handled by the ISR instead of sleeping. */
    if ((USB_MIS_ST & bUMS_SUSPEND) &&
        !(USB_INT_FG & 0x07u)) { /* suspend/resume, transfer, bus reset flags */
      PCON |= PD;
      slept = 1;
      /* PD pauses Timer2: discard its partial interval; uptime excludes sleep. */
      TR2 = 0;
      T2COUNT = 0xC180;
      TF2 = 0;
      TR2 = 1;
    }
    SAFE_MOD = 0x55;
    SAFE_MOD = 0xAA;
    WAKE_CTRL = 0;
    SAFE_MOD = 0;
  }
  EA = saved_ea;
  return slept;
}
