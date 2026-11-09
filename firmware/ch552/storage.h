/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PEDAL_STORAGE_H
#define PEDAL_STORAGE_H
#include <stdint.h>
void storage_load(uint8_t *record);
uint8_t storage_save(const uint8_t *record);
#endif
