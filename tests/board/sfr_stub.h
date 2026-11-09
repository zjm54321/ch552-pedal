/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PEDAL_SFR_STUB_H
#define PEDAL_SFR_STUB_H
#include <cstdint>

#define __data
#define __xdata
#define __code
#define __at(address)
#define __sfr extern volatile uint8_t
#define __sbit extern volatile uint8_t
#define __sfr16 extern volatile uint16_t
#define __sfr32 extern volatile uint32_t
#include "include/ch5xx.h"

/* Model both documented flag access routes: byte writes are W1C; direct
 * bit assignments of zero clear only that flag. Events are injected by the
 * simulated peripheral, never by writing a one through the firmware API. */
struct HostFlags {
  uint8_t value = 0;
  operator uint8_t() const { return value; }
  HostFlags &operator=(uint8_t bits) {
    value &= ~(bits & 0x1F);
    return *this;
  }
  HostFlags &operator|=(uint8_t bits) { return *this = uint8_t(value | bits); }
};
extern HostFlags HostUsbFlags;
struct HostFlagBit {
  uint8_t mask;
  operator bool() const { return (HostUsbFlags.value & mask) != 0; }
  HostFlagBit &operator=(uint8_t value) {
    if (!value) HostUsbFlags.value &= ~mask;
    return *this;
  }
};
extern HostFlagBit HostTransfer, HostReset, HostSuspend;
extern uint8_t HostMisStatus;
extern uint8_t HostInjectFlags;
uint8_t host_usb_mis_st();
#define USB_INT_FG HostUsbFlags
#define UIF_TRANSFER HostTransfer
#define UIF_BUS_RST HostReset
#define UIF_SUSPEND HostSuspend
#define U_TOG_OK (HostUsbFlags.value & 0x40)
#define USB_MIS_ST host_usb_mis_st()
#endif
