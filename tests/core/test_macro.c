/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pedal_macro.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t buffer[PEDAL_MACRO_BUFFER_SIZE];
static pedal_macro_player_t player;
static uint8_t report[PEDAL_KEYBOARD_REPORT_SIZE];

static void load(const uint8_t *bytes, size_t size)
{
    memset(buffer, 0, sizeof(buffer));
    memset(&player, 0, sizeof(player));
    memset(report, 0xCC, sizeof(report));
    assert(size <= sizeof(buffer));
    memcpy(buffer, bytes, size);
}

static int has_key(uint8_t key)
{
    unsigned int i;
    for (i = 2; i < sizeof(report); ++i) if (report[i] == key) return 1;
    return 0;
}

static void zero_report(void)
{
    assert(memcmp(report, (uint8_t[8]){0}, sizeof(report)) == 0);
}

static void test_actions_delay_and_trigger(void)
{
    const uint8_t actions[] = {1, 2, 0xE0, 1, 1, 0x06, 1, 4, '2', '0', '|',
                               1, 3, 0xE0, 0, 0, 0, 0};
    pedal_core_t core;
    load(actions, sizeof(actions));
    assert(pedal_macro_valid(buffer, sizeof(buffer)));
    assert(pedal_macro_start(&player, buffer, 0, 7));
    assert(pedal_macro_step(&player, buffer, 0, 7, report));
    assert(report[0] == 1);
    assert(pedal_macro_step(&player, buffer, 1, 7, report));
    assert(report[0] == 1 && has_key(0x06));
    assert(pedal_macro_step(&player, buffer, 2, 7, report));
    assert(report[0] == 1 && !has_key(0x06));
    assert(!pedal_macro_step(&player, buffer, 3, 7, report));
    assert(!pedal_macro_step(&player, buffer, 22, 7, report));
    assert(!pedal_macro_start(&player, buffer, 1, 7)); /* Repress ignored. */
    pedal_core_init(&core, 0, 0);
    pedal_core_inhibit(&core, 0); /* Pedal release does not abort playback. */
    pedal_core_inhibit(&core, 1);
    assert(core.down && player.active);
    assert(pedal_macro_step(&player, buffer, 23, 7, report));
    zero_report();
    assert(pedal_macro_step(&player, buffer, 24, 7, report));
    assert(!player.active);
    zero_report();
    assert(!pedal_macro_step(&player, buffer, 25, 7, report));
    assert(!pedal_macro_start(&player, buffer, PEDAL_MACRO_COUNT, 7));
}

static void test_text_and_shift(void)
{
    static const uint8_t text[] = {'A', 'a', '!', '1', '{', '|', '~', '\n', 0, 0, 0, 0};
    static const uint8_t keys[] = {4, 4, 0x1E, 0x1E, 0x2F, 0x31, 0x35, 0x28};
    static const uint8_t shifts[] = {2, 0, 2, 0, 2, 2, 2, 0};
    unsigned int i;
    load(text, sizeof(text));
    assert(pedal_macro_start(&player, buffer, 0, 0));
    for (i = 0; i < sizeof(keys); ++i) {
        assert(pedal_macro_step(&player, buffer, i * 2, 0, report));
        assert(report[0] == shifts[i] && has_key(keys[i]));
        assert(pedal_macro_step(&player, buffer, i * 2 + 1, 0, report));
        zero_report();
    }
    assert(pedal_macro_step(&player, buffer, 30, 0, report));
    zero_report();
    /* Temporary left shift never releases an explicitly held shift. */
    load((uint8_t[]){1, 2, 0xE5, 'A', 0, 0, 0, 0}, 8);
    assert(pedal_macro_start(&player, buffer, 0, 0));
    assert(pedal_macro_step(&player, buffer, 0, 0, report) && report[0] == 0x20);
    assert(pedal_macro_step(&player, buffer, 1, 0, report) && report[0] == 0x22);
    assert(pedal_macro_step(&player, buffer, 2, 0, report) && report[0] == 0x20);
    assert(pedal_macro_step(&player, buffer, 3, 0, report));
    zero_report(); /* Unmatched explicit down is released at completion. */
    for (i = 32; i < 127; ++i) {
        uint8_t shifted = (i >= 'A' && i <= 'Z') || strchr("!\"#$%&()*+:<>?@^_{|}~", (int)i) != 0;
        load((uint8_t[]){(uint8_t)i, 0, 0, 0, 0}, 5);
        assert(pedal_macro_start(&player, buffer, 0, 1));
        assert(pedal_macro_step(&player, buffer, 0, 1, report));
        assert(report[0] == (shifted ? 2 : 0));
        assert(pedal_macro_step(&player, buffer, 1, 1, report));
        zero_report();
    }
}

static void test_pairing_and_epoch_abort(void)
{
    const uint8_t actions[] = {1, 2, 0xE0, 1, 2, 4, 1, 2, 5, 1, 3, 4,
                               1, 3, 0xE0, 0, 'x', 0, 0, 0};
    uint8_t pressed[256] = {0};
    unsigned int downs = 0, ups = 0, i;
    load(actions, sizeof(actions));
    assert(pedal_macro_start(&player, buffer, 0, 5));
    while (player.active) {
        assert(pedal_macro_step(&player, buffer, 0, 5, report));
        for (i = 4; i <= 0xE7; ++i) {
            uint8_t held = i >= 0xE0 ? (report[0] >> (i - 0xE0)) & 1u : (unsigned int)has_key((uint8_t)i);
            if (held && !pressed[i]) ++downs;
            if (!held && pressed[i]) ++ups;
            pressed[i] = held;
        }
    }
    assert(downs == 3 && ups == downs);
    zero_report();
    assert(pedal_macro_start(&player, buffer, 1, 5)); /* Locate parsed sequence. */
    assert(pedal_macro_step(&player, buffer, 0, 5, report) && has_key(0x1B));
    assert(pedal_macro_step(&player, buffer, 0, 6, report));
    assert(!player.active);
    zero_report();
    assert(pedal_macro_start(&player, buffer, 0, 6));
    assert(pedal_macro_step(&player, buffer, 0, 6, report) && report[0] == 1);
    buffer[3] = 1; buffer[4] = 0xFF; /* Corrupt the next action after start. */
    assert(pedal_macro_step(&player, buffer, 1, 6, report));
    assert(!player.active);
    zero_report();
}

static void test_bounds_and_malformed(void)
{
    static const uint8_t invalid[][10] = {
        {1, 0xFF}, {1, 0}, {1, 1, 3}, {1, 2, 0xFF},
        {1, 4, '|'}, {1, 4, '-', '1', '|'},
        {1, 4, '1', '0', '0', '0', '0', '|'}, {0x80}
    };
    unsigned int i;
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        load(invalid[i], sizeof(invalid[i]));
        assert(!pedal_macro_valid(buffer, sizeof(buffer)));
        assert(!pedal_macro_start(&player, buffer, 0, 0));
    }
    memset(buffer, 'a', sizeof(buffer));
    assert(!pedal_macro_valid(buffer, sizeof(buffer)));
    buffer[sizeof(buffer) - 1] = 1; /* Prefix at the last index cannot over-read. */
    assert(!pedal_macro_valid(buffer, sizeof(buffer)));
    memset(buffer, 0, sizeof(buffer));
    assert(!pedal_macro_valid(buffer, 3));
    assert(!pedal_macro_valid(buffer, PEDAL_MACRO_BUFFER_SIZE + 1));
    assert(pedal_macro_valid(buffer, 4));
    /* One-byte KC_NO belongs to its action, not the next macro separator. */
    load((uint8_t[]){1, 1, 0, 0, 'b', 0, 0, 0}, 8);
    assert(pedal_macro_start(&player, buffer, 1, 1));
    assert(pedal_macro_step(&player, buffer, 0, 1, report) && has_key(5));
    /* Six held keyboard usages fit; the seventh aborts with a release. */
    for (i = 0; i < 7; ++i) {
        buffer[i * 3] = 1; buffer[i * 3 + 1] = 2; buffer[i * 3 + 2] = (uint8_t)(i + 4);
    }
    memset(buffer + 21, 0, sizeof(buffer) - 21);
    memset(&player, 0, sizeof(player));
    assert(pedal_macro_start(&player, buffer, 0, 1));
    for (i = 0; i < 7; ++i) assert(pedal_macro_step(&player, buffer, i, 1, report));
    assert(!player.active);
    zero_report();
    load((uint8_t[]){1, 4, '2', '0', '|', 'a', 0, 0, 0, 0}, 10);
    assert(pedal_macro_start(&player, buffer, 0, 0));
    assert(!pedal_macro_step(&player, buffer, UINT32_MAX - 10, 0, report));
    assert(!pedal_macro_step(&player, buffer, 8, 0, report));
    assert(pedal_macro_step(&player, buffer, 9, 0, report) && has_key(4));
    pedal_macro_abort(&player, report);
    zero_report();
}

int main(void)
{
    test_actions_delay_and_trigger();
    test_text_and_shift();
    test_pairing_and_epoch_abort();
    test_bounds_and_malformed();
    puts("macro playback tests: PASS (V11 actions/text/shift/delay, pairing, completion, epoch abort, bounds)");
    return 0;
}
