/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PEDAL_BOARD_CONFIG_H
#define PEDAL_BOARD_CONFIG_H

/* Default shared pid.codes test pair: private development ONLY. Make selects
 * test or borrowed is0 IDs via -DUSB_VID/-DUSB_PID; neither is our allocation. */
#ifndef USB_VID
#define USB_VID 0x1209
#endif
#ifndef USB_PID
#define USB_PID 0x0001
#endif
#define F_CPU 16000000UL
#define USER_USB_RAM 266
#define PEDAL_DEBOUNCE_MS 8u
#define PEDAL_QUEUE_SIZE 8u

#endif
