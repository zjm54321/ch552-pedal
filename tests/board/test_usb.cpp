/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "USBhandler.h"
#include "pedal_core.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

HostFlags HostUsbFlags;
HostFlagBit HostTransfer{2}, HostReset{1}, HostSuspend{4};
uint8_t HostMisStatus, HostInjectFlags;
uint8_t host_usb_mis_st() {
  HostUsbFlags.value |= HostInjectFlags;
  HostInjectFlags = 0;
  return HostMisStatus;
}

volatile uint8_t EA, IE_USB, USB_RX_LEN, USB_INT_ST, USB_INT_EN;
volatile uint8_t USB_CTRL, USB_DEV_AD, UDEV_CTRL;
volatile uint8_t UEP0_CTRL, UEP1_CTRL, UEP2_CTRL;
volatile uint8_t UEP0_T_LEN, UEP1_T_LEN, UEP2_T_LEN, UEP4_1_MOD, UEP2_3_MOD;
volatile uint16_t UEP0_DMA, UEP1_DMA, UEP2_DMA;

/* Descriptor requests are separately tested against the actual linked image.
 * These are link stubs only; no test below requests a descriptor. */
const USB_Descriptor_Device_t DeviceDescriptor = {};
const USB_Descriptor_Configuration_t ConfigurationDescriptor = {};
const uint8_t ReportDescriptor[] = {0};
const uint8_t RawHIDReportDescriptor[] = {0};
const uint8_t LanguageDescriptor[] = {0};
const uint16_t ManufacturerDescriptor[] = {0};
uint16_t SerialDescriptor[11], ProductDescriptor[17];

void USB_EP0_SETUP();
void USB_EP0_IN();
void USB_EP0_OUT();

static void setup(uint8_t type, uint8_t request, uint16_t value,
                  uint16_t index, uint16_t length) {
  UsbSetupBuf->bRequestType = type;
  UsbSetupBuf->bRequest = request;
  UsbSetupBuf->wValueL = value;
  UsbSetupBuf->wValueH = value >> 8;
  UsbSetupBuf->wIndexL = index;
  UsbSetupBuf->wIndexH = index >> 8;
  UsbSetupBuf->wLengthL = length;
  UsbSetupBuf->wLengthH = length >> 8;
  USB_RX_LEN = 8;
  USB_EP0_SETUP();
}

static bool ep0_stalled() {
  return (UEP0_CTRL & MASK_UEP_T_RES) == UEP_T_RES_STALL;
}

static void configure() {
  HostUsbFlags.value = 0x40;
  HostMisStatus = HostInjectFlags = 0;
  EA = 1;
  setup(0, USB_SET_CONFIGURATION, 1, 0, 0);
  assert(UsbConfig == 1 && !ep0_stalled());
  USB_EP0_IN();
}

static void raw_out(uint8_t length = 32) {
  HostUsbFlags.value |= 0x40;
  USB_RX_LEN = length;
  for (uint8_t i = 0; i < 32; ++i) Ep2Buffer[i] = i;
  USB_EP2_OUT();
}

static void test_set_report_status() {
  configure();
  setup(0x21, 0x09, 0x0200, 0, 1);
  assert(!ep0_stalled());
  assert((UEP0_CTRL & MASK_UEP_T_RES) == UEP_T_RES_NAK);
  assert((UEP0_CTRL & MASK_UEP_R_RES) == UEP_R_RES_ACK);
  USB_RX_LEN = 1;
  Ep0Buffer[0] = 0x05;
  HostUsbFlags.value &= ~0x40; /* Bad data toggle cannot complete the data stage. */
  USB_EP0_OUT();
  assert(UsbLed == 0);
  HostUsbFlags.value |= 0x40;
  USB_EP0_OUT();
  assert(UsbLed == 0x05 && UEP0_T_LEN == 0);
  assert((UEP0_CTRL & MASK_UEP_T_RES) == UEP_T_RES_ACK);
  assert(UEP0_CTRL & bUEP_T_TOG); /* Zero-length DATA1 IN status. */
  USB_EP0_IN();
  assert((UEP0_CTRL & MASK_UEP_T_RES) == UEP_T_RES_NAK);

  setup(0x21, 0x09, 0x0200, 0, 1);
  USB_RX_LEN = 2;
  USB_EP0_OUT();
  assert(ep0_stalled());
  setup(0xA1, 0x02, 0, 0, 1); /* A control-read status OUT must not arm IN. */
  USB_EP0_IN();
  USB_RX_LEN = 0;
  USB_EP0_OUT();
  assert((UEP0_CTRL & MASK_UEP_T_RES) == UEP_T_RES_NAK);
}

static void test_epoch_take_race() {
  uint8_t report[32] = {};
  configure();
  const uint8_t old_epoch = UsbEpoch; /* Main has just checked its epoch. */
  setup(0, USB_SET_CONFIGURATION, 1, 0, 0); /* ISR races before the take. */
  raw_out();
  assert(UsbRawHeld);
  assert(!USBRawTake(report, old_epoch));
  assert(UsbRawHeld && (UEP2_CTRL & MASK_UEP_R_RES) == UEP_R_RES_NAK);
  assert(USBRawTake(report, UsbEpoch)); /* New owner still receives the request. */
  for (uint8_t i = 0; i < 32; ++i) assert(report[i] == i);
  assert(!USBRawTake(report, UsbEpoch));
  assert(!USBRawReply(report, old_epoch));
  assert(USBRawReply(report, UsbEpoch));
  USB_EP2_IN();
  assert(!UsbRawHeld && !UsbRawBusy);
  raw_out(31);
  assert(!USBRawTake(report, UsbEpoch) && !UsbRawHeld);
  assert((UEP2_CTRL & MASK_UEP_R_RES) == UEP_R_RES_ACK);
  setup(0, USB_SET_CONFIGURATION, 0, 0, 0);
  raw_out();
  assert(!UsbRawHeld && !USBRawTake(report, UsbEpoch));
}

static void test_resume_preserves_late_events() {
  uint8_t report[32] = {};
  configure();
  assert(USBKeyboardSend(report, UsbEpoch, UsbInputEpoch));
  HostUsbFlags.value = 4; /* Resume is the only flag at ISR entry. */
  HostInjectFlags = 2; /* IN completes after transfer/reset branches. */
  USB_INT_ST = UIS_TOKEN_IN | 1;
  USBInterrupt();
  assert(HostUsbFlags.value & 2);
  assert(UsbKeyboardBusy);
  USBInterrupt();
  assert(!UsbKeyboardBusy && !(HostUsbFlags.value & 2));

  raw_out();
  assert(USBRawTake(report, UsbEpoch) && USBRawReply(report, UsbEpoch));
  HostUsbFlags.value = 4;
  HostInjectFlags = 2;
  USB_INT_ST = UIS_TOKEN_IN | 2;
  USBInterrupt();
  assert(UsbRawHeld && UsbRawBusy && (HostUsbFlags.value & 2));
  USBInterrupt();
  assert(!UsbRawHeld && !UsbRawBusy);

  const uint8_t epoch = UsbEpoch;
  HostUsbFlags.value = 4;
  HostInjectFlags = 1; /* Bus reset arrives during resume handling. */
  USBInterrupt();
  assert(UsbConfig && (HostUsbFlags.value & 1));
  USBInterrupt();
  assert(!UsbConfig && UsbEpoch != epoch && !(HostUsbFlags.value & 1));
}

static void feature(uint8_t endpoint, bool halt) {
  setup(0x02, halt ? USB_SET_FEATURE : USB_CLEAR_FEATURE, 0, endpoint, 0);
  assert(!ep0_stalled());
}

static void status(uint8_t endpoint, uint8_t expected) {
  setup(0x82, USB_GET_STATUS, 0, endpoint, 2);
  assert(!ep0_stalled() && Ep0Buffer[0] == expected && Ep0Buffer[1] == 0);
}

static void test_endpoint_halt() {
  uint8_t report[32] = {};
  configure();
  for (const uint8_t endpoint : {0x01, 0x81, 0x02, 0x82}) {
    feature(endpoint, true);
    status(endpoint, 1);
    feature(endpoint, false);
    status(endpoint, 0);
  }
  for (const uint8_t endpoint : {0x00, 0x80, 0x03, 0x83, 0x04, 0x84, 0x11, 0xFF}) {
    const uint8_t before1 = UEP1_CTRL, before2 = UEP2_CTRL;
    setup(0x02, USB_SET_FEATURE, 0, endpoint, 0);
    assert(ep0_stalled() && before1 == UEP1_CTRL && before2 == UEP2_CTRL);
    setup(0x02, USB_CLEAR_FEATURE, 0, endpoint, 0);
    assert(ep0_stalled() && before1 == UEP1_CTRL && before2 == UEP2_CTRL);
  }
  /* Reject malformed selector, high index, length and direction before writes. */
  for (const uint8_t command : {USB_SET_FEATURE, USB_CLEAR_FEATURE}) {
    const uint8_t before = UEP1_CTRL;
    setup(0x02, command, 1, 0x81, 0);
    assert(ep0_stalled() && UEP1_CTRL == before);
    setup(0x02, command, 0x100, 0x81, 0);
    assert(ep0_stalled() && UEP1_CTRL == before);
    setup(0x02, command, 0, 0x181, 0);
    assert(ep0_stalled() && UEP1_CTRL == before);
    setup(0x02, command, 0, 0x81, 1);
    assert(ep0_stalled() && UEP1_CTRL == before);
    setup(0x82, command, 0, 0x81, 0);
    assert(ep0_stalled() && UEP1_CTRL == before);
  }

  assert(USBKeyboardSend(report, UsbEpoch, UsbInputEpoch));
  feature(0x81, true);
  USB_EP1_IN();
  assert((UEP1_CTRL & MASK_UEP_T_RES) == UEP_T_RES_STALL);
  assert(!USBKeyboardSend(report, UsbEpoch, UsbInputEpoch));
  feature(0x81, false);
  assert(!(UEP1_CTRL & bUEP_T_TOG));
  assert(USBKeyboardSend(report, UsbEpoch, UsbInputEpoch));
  USB_EP1_IN();

  raw_out();
  feature(0x02, true);
  assert(USBRawTake(report, UsbEpoch));
  feature(0x82, true);
  assert(!USBRawReply(report, UsbEpoch));
  feature(0x82, false);
  assert(USBRawReply(report, UsbEpoch));
  feature(0x82, true);
  USB_EP2_IN(); /* A late completion cannot clear either halt direction. */
  assert((UEP2_CTRL & MASK_UEP_T_RES) == UEP_T_RES_STALL);
  assert((UEP2_CTRL & MASK_UEP_R_RES) == UEP_R_RES_STALL);
  status(0x02, 1);
  status(0x82, 1);
  feature(0x02, false);
  assert(!(UEP2_CTRL & bUEP_R_TOG));
  feature(0x82, false);

  raw_out(31);
  feature(0x02, true);
  assert(!USBRawTake(report, UsbEpoch));
  assert((UEP2_CTRL & MASK_UEP_R_RES) == UEP_R_RES_STALL);
  feature(0x02, false);
  raw_out();
  feature(0x02, true);
  feature(0x02, false);
  assert((UEP2_CTRL & MASK_UEP_R_RES) == UEP_R_RES_NAK && UsbRawHeld);
  assert(USBRawTake(report, UsbEpoch) && USBRawReply(report, UsbEpoch));
  USB_EP2_IN();
  configure(); /* Configuration clears halt state in both directions. */
  status(0x82, 0);
}

static void test_suspend_transport() {
  uint8_t report[8] = {0, 0, 0x2C};
  configure();
  const uint8_t input_epoch = UsbInputEpoch;
  assert(USBKeyboardSend(report, UsbEpoch, input_epoch));
  HostMisStatus = bUMS_SUSPEND;
  HostUsbFlags.value = 4;
  USBInterrupt();
  assert(UsbSuspended && !UsbKeyboardBusy && UEP1_T_LEN == 0);
  assert(!USBKeyboardSend(report, UsbEpoch, UsbInputEpoch));
  feature(0x81, true);
  USBTransportSuspend(1);
  assert((UEP1_CTRL & MASK_UEP_T_RES) == UEP_T_RES_STALL);
  feature(0x81, false);
  HostMisStatus = 0;
  HostUsbFlags.value = 4;
  USBInterrupt();
  assert(!UsbSuspended);
  assert(!USBKeyboardSend(report, UsbEpoch, input_epoch)); /* No pre-suspend queue replay. */
  assert(USBKeyboardSend(report, UsbEpoch, UsbInputEpoch));
}

int main() {
  test_set_report_status();
  test_epoch_take_race();
  test_resume_preserves_late_events();
  test_endpoint_halt();
  test_suspend_transport();
  std::puts("USB SFR tests: PASS (SET_REPORT status, epoch race, late resume events/W1C, halt, suspend transport)");
}
