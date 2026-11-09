/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PEDAL_BOARD_CONFIG_H
#define PEDAL_BOARD_CONFIG_H

/* Shared pid.codes test pair: private development ONLY, not our allocation.
 * Obtain an allocated VID/PID before distribution; update via/pedal.json too. */
#define USB_VID 0x1209
#define USB_PID 0x0001
#define F_CPU 16000000UL
#define USER_USB_RAM 266
#define PEDAL_DEBOUNCE_MS 8u
#define PEDAL_QUEUE_SIZE 8u

#endif
