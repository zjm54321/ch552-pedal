/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PEDAL_STORAGE_BACKEND_H
#define PEDAL_STORAGE_BACKEND_H
#include <stdint.h>

/* Two eight-byte slots use 16 logical bytes / 32 physical DataFlash bytes.
 * The board backend maps logical addresses to even physical addresses only. */
#define STORAGE_FLASH_BYTES 16u
/* CH552 has 128 logical cells at even physical addresses 0xC000..0xC0FE.
 * Keymap code still uses only STORAGE_FLASH_BYTES; macros own logical 16..127. */
#define STORAGE_DATA_BYTES 128u
uint8_t storage_read_byte(uint8_t address);
uint8_t storage_write_byte(uint8_t address, uint8_t value);
#endif
