/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PEDAL_STORAGE_H
#define PEDAL_STORAGE_H
#include "pedal_core.h"
void storage_load(uint8_t PEDAL_XDATA *record);
uint8_t storage_save(const uint8_t PEDAL_XDATA *record);
#endif
