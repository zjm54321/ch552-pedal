/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PEDAL_USB_TRANSPORT_H
#define PEDAL_USB_TRANSPORT_H
#include <stdint.h>

extern volatile __xdata uint8_t UsbEpoch;
extern volatile __xdata uint8_t UsbKeyboardBusy;
extern volatile __xdata uint8_t UsbRawBusy;
extern volatile __xdata uint8_t UsbRawHeld;
extern volatile __xdata uint8_t UsbIdle;
extern volatile __xdata uint8_t UsbProtocol;
extern volatile __xdata uint8_t UsbLed;
extern volatile __xdata uint8_t UsbSuspended;
extern volatile __xdata uint8_t UsbInputEpoch;
extern volatile __xdata uint8_t UsbKeyboardReport[8];

void USBTransportReset(void);
void USBTransportSuspend(uint8_t suspended);
uint8_t USBEndpointHalt(uint8_t address, uint8_t halt);
uint8_t USBEndpointStatus(uint8_t address);
void USBInit(void);
uint8_t USBKeyboardSend(const __xdata uint8_t *report, uint8_t epoch, uint8_t input_epoch);
uint8_t USBRawTake(__xdata uint8_t *report, uint8_t epoch);
uint8_t USBRawReply(const __xdata uint8_t *report, uint8_t epoch);

#endif
