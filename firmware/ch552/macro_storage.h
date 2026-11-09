/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PEDAL_MACRO_STORAGE_H
#define PEDAL_MACRO_STORAGE_H

#include "pedal_macro.h"

/* Call only after the power-up held-pedal ISP decision. No startup writes.
 * Invalid/torn records read as empty macros and are never playable. */
void storage_macro_load(uint8_t PEDAL_XDATA *buffer);
uint8_t storage_macro_ready(void);
uint8_t storage_macro_reset(uint8_t PEDAL_XDATA *buffer);
/* Updates require the VIA start marker, contiguous chunks from zero, then the
 * final zero marker. A failed/missing chunk cannot be certified by that zero. */
uint8_t storage_macro_write(uint8_t PEDAL_XDATA *buffer, uint16_t offset, uint8_t size,
                            const uint8_t PEDAL_XDATA *data);
/* Latch upload failure on invalid writes or transport epoch changes. */
void storage_macro_cancel(void);

#endif
