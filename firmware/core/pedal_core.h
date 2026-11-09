/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PEDAL_CORE_H
#define PEDAL_CORE_H

#include <stdint.h>

#define PEDAL_RAW_REPORT_SIZE 32u
#define PEDAL_KEYBOARD_REPORT_SIZE 8u
#define PEDAL_STORAGE_SIZE 8u
#define PEDAL_DEFAULT_KEYCODE 0x002Cu /* KC_SPACE */

#define PEDAL_VIA_HANDLED 0x01u
#define PEDAL_VIA_SAVE 0x02u
#define PEDAL_VIA_BOOTLOADER 0x04u

typedef struct {
    uint16_t keycode;
    uint8_t persistence_dirty;
    uint8_t down;
    uint8_t keyboard_report[PEDAL_KEYBOARD_REPORT_SIZE];
} pedal_core_t;

/*
 * Call the board's boot-on-held check BEFORE this initializer or storage reads.
 * record may be null; only an exactly PEDAL_STORAGE_SIZE-byte valid record is
 * loaded. Otherwise use KC_SPACE. Return 1 if loaded, 0 if using the default.
 * Initialization performs no persistence operation and requests no write.
 */
uint8_t pedal_core_init(pedal_core_t *core, const uint8_t *record,
                        uint8_t record_length);

/*
 * Supported: KC_NO, keyboard usages 0x04..0xA4, modifiers 0xE0..0xE7, and
 * QMK legacy QK_MODS 0x0100..0x1FFF with a nonempty left/right modifier mask
 * and a supported low-byte key. KC_TRNS and advanced/consumer/macro codes are
 * unsupported. Modifier-only chords, including QK_MODS + KC_NO, are allowed.
 * Return 1 and write the eight-byte boot-keyboard report on success; on failure
 * return 0 without changing report. report must have eight writable bytes.
 */
uint8_t pedal_keycode_report(uint16_t keycode,
                             uint8_t report[PEDAL_KEYBOARD_REPORT_SIZE]);

/*
 * Feed the debounced physical state (zero = released, nonzero = pressed).
 * Return 1 only on an edge, copying the new eight-byte report to report.
 * No edge leaves report untouched. While held, remapping never modifies the
 * original pressed keyboard_report; the release report clears those keys.
 * The caller owns transport delivery/retry of each returned report.
 */
uint8_t pedal_core_update(pedal_core_t *core, uint8_t down,
                          uint8_t report[PEDAL_KEYBOARD_REPORT_SIZE]);

/*
 * Process one in/out VIA report. report must reference report_length bytes.
 * Exactly 32 bytes are required; other lengths return 0 without touching it.
 * Unknown/invalid 32-byte commands set report[0] to 0xFF and return 0.
 * Success echoes the request with the command's response fields populated.
 * Subset: protocol version 8; uptime/layout-options(0)/physical-matrix values;
 * key get/set/reset; layer count 1; macro count/size 0; buffer get/set; ISP.
 * Other commands, including EEPROM erase and macro operations, are unhandled.
 * Key coordinates must all be zero. The two-byte keymap buffer is big-endian;
 * partial writes validate the complete proposed key before any change. Empty
 * operations at offsets 0..2 succeed without a save; reads/writes past 2 fail.
 *
 * Result flags: HANDLED; SAVE when a validated mapping changed or is not durable;
 * BOOTLOADER only for the explicit 0x0B command (never SAVE at the same time).
 * On SAVE the board serializes with pedal_core_record and writes exactly eight
 * payload bytes to its bounded journal. Clear persistence_dirty only after a
 * successful save; set it on a failed save, including on the retained RAM core.
 * On BOOTLOADER the board releases keyboard
 * output and enters ISP; neither this core nor the request erases/writes Flash.
 * Storage, USB transport, uptime sampling, and bootloader entry are board-owned.
 * uptime_ms is used for the standard big-endian keyboard uptime value.
 */
uint8_t pedal_core_via(pedal_core_t *core, uint8_t *report,
                       uint8_t report_length, uint32_t uptime_ms);

/*
 * Explicit serialization, not a Flash write. Eight-byte version-1 record:
 * 50 44 01 00 key_hi key_lo crc_hi crc_lo. CRC-16/CCITT-FALSE covers bytes 0..5.
 * Only manipulate core through this API so its keycode remains validated.
 */
void pedal_core_record(const pedal_core_t *core,
                        uint8_t record[PEDAL_STORAGE_SIZE]);

/* CRC-16/CCITT-FALSE over the first six bytes of either storage format. */
uint16_t pedal_record_crc(const uint8_t *record);

#endif
