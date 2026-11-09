/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PEDAL_USB_POWER_H
#define PEDAL_USB_POWER_H
#include <stdint.h>

/* Main-loop only, after synchronous storage operations. blocked means a VIA
 * reply or ISP jump is pending. Return 1 after PD wake, 0 if entry was skipped. */
uint8_t USBPowerDown(uint8_t blocked);
#endif
