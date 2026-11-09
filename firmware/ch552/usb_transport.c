/* SPDX-License-Identifier: GPL-3.0-or-later
 * Nonblocking endpoint glue adapted from CH55xduino USBHIDKeyboardMouse.c,
 * created by Deqing Sun. See third_party/ch55xduino/PROVENANCE.md. */
#include "USBhandler.h"
#include "usb_transport.h"

volatile __xdata uint8_t UsbEpoch;
volatile __xdata uint8_t UsbKeyboardBusy;
volatile __xdata uint8_t UsbRawBusy;
volatile __xdata uint8_t UsbRawHeld;
volatile __xdata uint8_t UsbIdle;
volatile __xdata uint8_t UsbProtocol;
volatile __xdata uint8_t UsbLed;
volatile __xdata uint8_t UsbSuspended;
volatile __xdata uint8_t UsbInputEpoch;
volatile __xdata uint8_t UsbKeyboardReport[8];
static __xdata uint8_t HaltIn;
static __xdata uint8_t HaltOut;
static volatile __xdata uint8_t RawPending;
static volatile __xdata uint8_t RawLength;

/* Called only with interrupts disabled (startup or USB ISR). No core calls. */
void USBTransportReset(void) {
  uint8_t i;
  UsbKeyboardBusy = 0;
  UsbRawBusy = 0;
  UsbRawHeld = 0;
  RawPending = 0;
  RawLength = 0;
  UsbIdle = 0;
  UsbProtocol = 1;
  UsbLed = 0;
  UsbSuspended = 0;
  HaltIn = HaltOut = 0;
  UEP1_T_LEN = 0;
  UEP2_T_LEN = 0;
  for (i = 0; i < 8; ++i) UsbKeyboardReport[i] = 0;
  ++UsbEpoch;
  ++UsbInputEpoch;
}

/* Called in the USB ISR. Cancel stale keyboard DMA, not Raw HID ownership.
 * Input epochs also catch a complete suspend/resume during a main-loop save. */
void USBTransportSuspend(uint8_t suspended) {
  UsbSuspended = suspended;
  ++UsbInputEpoch;
  if (suspended) {
    UEP1_T_LEN = 0;
    if (!(HaltIn & 2))
      UEP1_CTRL = (UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
    UsbKeyboardBusy = 0;
  }
}

/* Only the four declared interrupt directions may be halted. EP0 accepts
 * GET_STATUS but cannot acquire ENDPOINT_HALT through feature requests. */
uint8_t USBEndpointStatus(uint8_t address) {
  uint8_t mask;
  if (address == 0 || address == 0x80) return 0;
  if (!UsbConfig || (address != 1 && address != 0x81 &&
                     address != 2 && address != 0x82)) return 0xFF;
  mask = address & 0x0F;
  mask = 1u << mask;
  return ((address & 0x80 ? HaltIn : HaltOut) & mask) != 0;
}

uint8_t USBEndpointHalt(uint8_t address, uint8_t halt) {
  uint8_t mask;
  if (address == 0 || address == 0x80 || USBEndpointStatus(address) == 0xFF)
    return 0;
  mask = 1u << (address & 0x0F);
  if (address & 0x80) {
    if (halt) HaltIn |= mask;
    else HaltIn &= ~mask;
  } else {
    if (halt) HaltOut |= mask;
    else HaltOut &= ~mask;
  }
  switch (address) {
  case 0x81:
    UEP1_CTRL = (UEP1_CTRL & ~(MASK_UEP_T_RES | (halt ? 0 : bUEP_T_TOG))) |
                (halt ? UEP_T_RES_STALL :
                 UsbKeyboardBusy ? UEP_T_RES_ACK : UEP_T_RES_NAK);
    break;
  case 0x01:
    UEP1_CTRL = (UEP1_CTRL & ~(MASK_UEP_R_RES | (halt ? 0 : bUEP_R_TOG))) |
                (halt ? UEP_R_RES_STALL : UEP_R_RES_ACK);
    break;
  case 0x82:
    UEP2_CTRL = (UEP2_CTRL & ~(MASK_UEP_T_RES | (halt ? 0 : bUEP_T_TOG))) |
                (halt ? UEP_T_RES_STALL :
                 UsbRawBusy ? UEP_T_RES_ACK : UEP_T_RES_NAK);
    break;
  case 0x02:
    UEP2_CTRL = (UEP2_CTRL & ~(MASK_UEP_R_RES | (halt ? 0 : bUEP_R_TOG))) |
                (halt ? UEP_R_RES_STALL :
                 UsbRawHeld ? UEP_R_RES_NAK : UEP_R_RES_ACK);
    break;
  }
  return 1;
}

void USBInit(void) {
  USBTransportReset();
  USBDeviceEndPointCfg();
  UEP0_T_LEN = 0;
  USBDeviceCfg();
  USBDeviceIntCfg();
}

void USB_EP1_IN(void) {
  UEP1_T_LEN = 0;
  if (!(HaltIn & 2))
    UEP1_CTRL = (UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
  UsbKeyboardBusy = 0;
}

void USB_EP2_IN(void) {
  UEP2_T_LEN = 0;
  if (!(HaltIn & 4))
    UEP2_CTRL = (UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
  if (!(HaltOut & 4))
    UEP2_CTRL = (UEP2_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
  UsbRawBusy = 0;
  UsbRawHeld = 0;
}

void USB_EP1_OUT(void) {
  if (!(HaltOut & 2) && U_TOG_OK && USB_RX_LEN == 1) UsbLed = Ep1Buffer[0];
}

void USB_EP2_OUT(void) {
  if (!U_TOG_OK || !UsbConfig || UsbRawHeld || (HaltOut & 4)) return;
  /* Hardware NAKs all further OUT traffic until the reply is ACKed. The DMA
   * receive half is the mailbox: never overwrite or process it in this ISR. */
  UEP2_CTRL = (UEP2_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_NAK;
  UsbRawHeld = 1;
  if (!RawPending) {
    RawLength = USB_RX_LEN;
    RawPending = 1;
  }
}

uint8_t USBKeyboardSend(const __xdata uint8_t *report, uint8_t epoch, uint8_t input_epoch) {
  uint8_t i;
  uint8_t saved_ea = EA;
  EA = 0;
  if (!UsbConfig || UsbKeyboardBusy || epoch != UsbEpoch || UsbSuspended ||
      input_epoch != UsbInputEpoch || (HaltIn & 2)) {
    EA = saved_ea;
    return 0;
  }
  for (i = 0; i < 8; ++i) {
    Ep1Buffer[64 + i] = report[i];
    UsbKeyboardReport[i] = report[i];
  }
  UEP1_T_LEN = 8;
  UsbKeyboardBusy = 1;
  UEP1_CTRL = (UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
  EA = saved_ea;
  return 1;
}

uint8_t USBRawTake(__xdata uint8_t *report, uint8_t epoch) {
  uint8_t i;
  uint8_t saved_ea = EA;
  EA = 0;
  if (epoch != UsbEpoch || !UsbConfig || !RawPending || !UsbRawHeld ||
      UsbRawBusy) {
    EA = saved_ea;
    return 0;
  }
  RawPending = 0;
  if (RawLength != 32) {
    /* Malformed packets cannot reach the core or acquire an unrelated reply. */
    UsbRawHeld = 0;
    if (!(HaltOut & 4))
      UEP2_CTRL = (UEP2_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
    EA = saved_ea;
    return 0;
  }
  for (i = 0; i < 32; ++i) report[i] = Ep2Buffer[i];
  EA = saved_ea;
  return 1;
}

uint8_t USBRawReply(const __xdata uint8_t *report, uint8_t epoch) {
  uint8_t i;
  uint8_t saved_ea = EA;
  EA = 0;
  if (!UsbConfig || !UsbRawHeld || UsbRawBusy || epoch != UsbEpoch ||
      (HaltIn & 4)) {
    EA = saved_ea;
    return 0;
  }
  for (i = 0; i < 32; ++i) Ep2Buffer[64 + i] = report[i];
  UEP2_T_LEN = 32;
  UsbRawBusy = 1;
  UEP2_CTRL = (UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
  EA = saved_ea;
  return 1;
}
