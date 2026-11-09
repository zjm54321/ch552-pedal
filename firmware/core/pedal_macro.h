/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PEDAL_MACRO_H
#define PEDAL_MACRO_H

#include "pedal_core.h"

typedef struct {
    uint8_t type; /* 0 = terminator, 1 = tap/text, 2 = down, 3 = up, 4 = delay */
    uint8_t key;
    uint8_t mods; /* Temporary US-layout shift for text only. */
    uint16_t delay;
} pedal_macro_action_t;

typedef struct {
    uint32_t wait_until;
    uint8_t active;
    uint8_t position;
    uint8_t epoch;
    uint8_t phase;
    uint8_t held[PEDAL_KEYBOARD_REPORT_SIZE];
} pedal_macro_player_t;

/* Four complete V11 sequences and zero-only trailing padding must fit length.
 * A token's action/key/delay bytes are never mistaken for macro delimiters. */
uint8_t pedal_macro_valid(const uint8_t PEDAL_XDATA *buffer, uint8_t length);
void pedal_macro_read(const uint8_t PEDAL_XDATA *buffer, uint16_t offset, uint8_t size,
                      uint8_t PEDAL_XDATA *data);
uint8_t pedal_macro_start(pedal_macro_player_t PEDAL_XDATA *player, const uint8_t PEDAL_XDATA *buffer,
                          uint8_t id, uint8_t epoch);
/* Call only when the preceding queued/DMA report has completed. Return 1 for
 * one report, 0 while waiting/idle. Never wait here: USB interrupts stay live.
 * An epoch change, malformed token, overflow, or completion releases all keys. */
uint8_t pedal_macro_step(pedal_macro_player_t PEDAL_XDATA *player, const uint8_t PEDAL_XDATA *buffer,
                         uint32_t now, uint8_t epoch, uint8_t PEDAL_XDATA *report);
void pedal_macro_abort(pedal_macro_player_t PEDAL_XDATA *player, uint8_t PEDAL_XDATA *report);

#endif
