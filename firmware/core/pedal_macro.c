/* SPDX-License-Identifier: GPL-3.0-or-later
 * US ASCII/shift tables adapted from QMK quantum/send_string/send_string.c
 * (Copyright 2021, GPL-2.0-or-later); V11 tokens follow its send-string format.
 * Playback is a new bounded, nonblocking implementation, not the QMK runtime. */
#include "pedal_macro.h"

#ifdef __SDCC
#define CODE __code
#else
#define CODE
#endif

/* 128 key bytes plus 16 bit-packed shift bytes, all in application code space. */
static const CODE uint8_t ascii_key[128] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0x2A, 0x2B, 0x28, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0x29, 0, 0, 0, 0,
    0x2C, 0x1E, 0x34, 0x20, 0x21, 0x22, 0x24, 0x34,
    0x26, 0x27, 0x25, 0x2E, 0x36, 0x2D, 0x37, 0x38,
    0x27, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x33, 0x33, 0x36, 0x2E, 0x37, 0x38,
    0x1F, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
    0x1B, 0x1C, 0x1D, 0x2F, 0x31, 0x30, 0x23, 0x2D,
    0x35, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
    0x1B, 0x1C, 0x1D, 0x2F, 0x31, 0x30, 0x35, 0x4C
};
static const CODE uint8_t ascii_shift[16] = {
    0, 0, 0, 0, 0x7E, 0x0F, 0, 0xD4,
    0xFF, 0xFF, 0xFF, 0xC7, 0, 0, 0, 0x78
};

#define PHASE_TAP_UP 1u
#define PHASE_DELAY 2u

static void copy_report(uint8_t PEDAL_XDATA *to, const uint8_t PEDAL_XDATA *from)
{
    uint8_t i;
    for (i = 0; i < PEDAL_KEYBOARD_REPORT_SIZE; ++i) to[i] = from[i];
}

static uint8_t next_action(const uint8_t PEDAL_XDATA *buffer, uint8_t length,
                           uint8_t PEDAL_XDATA *position, pedal_macro_action_t PEDAL_XDATA *action)
{
    uint8_t byte;
    uint8_t digits;
    if (length > PEDAL_MACRO_BUFFER_SIZE || *position >= length) return 0;
    byte = buffer[(*position)++];
    action->mods = 0;
    action->type = 0;
    if (!byte) return 1;
    if (byte != 1) {
        if (byte >= 128 || !ascii_key[byte]) return 0;
        action->type = 1;
        action->key = ascii_key[byte];
        if ((ascii_shift[byte >> 3] >> (byte & 7)) & 1) action->mods = 2;
        return 1;
    }
    if (*position >= length) return 0;
    action->type = buffer[(*position)++];
    if (action->type >= 1 && action->type <= 3) {
        if (*position >= length) return 0;
        action->key = buffer[(*position)++];
        return pedal_basic_key_valid(action->key);
    }
    if (action->type != 4) return 0;
    action->delay = 0;
    digits = 0;
    while (*position < length) {
        byte = buffer[(*position)++];
        if (byte == 0x7C) return digits != 0;
        if (byte < '0' || byte > '9' || ++digits > 4) return 0;
        action->delay = action->delay * 10u + byte - '0';
    }
    return 0;
}

uint8_t pedal_macro_valid(const uint8_t PEDAL_XDATA *buffer, uint8_t length)
{
    PEDAL_XDATA uint8_t position = 0;
    uint8_t count = 0;
    PEDAL_XDATA pedal_macro_action_t action;
    while (count < PEDAL_MACRO_COUNT) {
        if (!next_action(buffer, length, &position, &action)) return 0;
        if (!action.type) ++count;
    }
    while (position < length) if (buffer[position++]) return 0;
    return 1;
}

void pedal_macro_read(const uint8_t PEDAL_XDATA *buffer, uint16_t offset, uint8_t size,
                      uint8_t PEDAL_XDATA *data)
{
    uint8_t i;
    for (i = 0; i < size; ++i)
        data[i] = offset < PEDAL_MACRO_BUFFER_SIZE &&
                  i < PEDAL_MACRO_BUFFER_SIZE - offset ? buffer[offset + i] : 0;
}

uint8_t pedal_macro_start(pedal_macro_player_t PEDAL_XDATA *player, const uint8_t PEDAL_XDATA *buffer,
                          uint8_t id, uint8_t epoch)
{
    PEDAL_XDATA uint8_t position = 0;
    uint8_t i;
    PEDAL_XDATA pedal_macro_action_t action;
    if (player->active || id >= PEDAL_MACRO_COUNT ||
        !pedal_macro_valid(buffer, PEDAL_MACRO_BUFFER_SIZE)) return 0;
    while (id) {
        if (!next_action(buffer, PEDAL_MACRO_BUFFER_SIZE, &position, &action))
            return 0;
        if (!action.type) --id;
    }
    for (i = 0; i < PEDAL_KEYBOARD_REPORT_SIZE; ++i) player->held[i] = 0;
    player->position = position;
    player->epoch = epoch;
    player->phase = 0;
    player->active = 1;
    return 1;
}

void pedal_macro_abort(pedal_macro_player_t PEDAL_XDATA *player, uint8_t PEDAL_XDATA *report)
{
    uint8_t i;
    player->active = player->phase = 0;
    for (i = 0; i < PEDAL_KEYBOARD_REPORT_SIZE; ++i)
        player->held[i] = report[i] = 0;
}

static uint8_t change_key(uint8_t PEDAL_XDATA *report, uint8_t key, uint8_t down)
{
    uint8_t i;
    uint8_t empty = 0;
    if (!key) return 1;
    if (key >= 0xE0) {
        i = 1u << (key - 0xE0);
        if (down) report[0] |= i;
        else report[0] &= ~i;
        return 1;
    }
    for (i = 2; i < PEDAL_KEYBOARD_REPORT_SIZE; ++i) {
        if (report[i] == key) {
            if (!down) report[i] = 0;
            return 1;
        }
        if (!report[i]) empty = i;
    }
    if (!down) return 1;
    if (!empty) return 0; /* No rollover/overflow report: release safely. */
    report[empty] = key;
    return 1;
}

uint8_t pedal_macro_step(pedal_macro_player_t PEDAL_XDATA *player, const uint8_t PEDAL_XDATA *buffer,
                         uint32_t now, uint8_t epoch, uint8_t PEDAL_XDATA *report)
{
    PEDAL_XDATA pedal_macro_action_t action;
    if (!player->active) return 0;
    if (epoch != player->epoch) goto release;
    if (player->phase == PHASE_TAP_UP) {
        player->phase = 0;
        copy_report(report, player->held);
        return 1;
    }
    if (player->phase == PHASE_DELAY) {
        if ((int32_t)(now - player->wait_until) < 0) return 0;
        player->phase = 0;
    }
    if (!next_action(buffer, PEDAL_MACRO_BUFFER_SIZE, &player->position, &action))
        goto release;
    switch (action.type) {
    case 1:
        copy_report(report, player->held);
        if (!change_key(report, action.key, 1)) goto release;
        report[0] |= action.mods;
        player->phase = PHASE_TAP_UP;
        return 1;
    case 2:
    case 3:
        if (!change_key(player->held, action.key, action.type == 2)) goto release;
        copy_report(report, player->held);
        return 1;
    case 4:
        player->wait_until = now + action.delay;
        player->phase = PHASE_DELAY;
        return 0;
    default:
        break;
    }
release:
    pedal_macro_abort(player, report);
    return 1;
}
