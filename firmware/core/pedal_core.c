/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pedal_core.h"

#define VIA_GET_PROTOCOL_VERSION 0x01u
#define VIA_GET_KEYBOARD_VALUE 0x02u
#define VIA_GET_KEYCODE 0x04u
#define VIA_SET_KEYCODE 0x05u
#define VIA_RESET_KEYMAP 0x06u
#define VIA_BOOTLOADER_JUMP 0x0Bu
#define VIA_MACRO_GET_COUNT 0x0Cu
#define VIA_MACRO_GET_BUFFER_SIZE 0x0Du
#define VIA_MACRO_GET_BUFFER 0x0Eu
#define VIA_MACRO_SET_BUFFER 0x0Fu
#define VIA_MACRO_RESET 0x10u
#define VIA_GET_LAYER_COUNT 0x11u
#define VIA_GET_BUFFER 0x12u
#define VIA_SET_BUFFER 0x13u
#define VIA_UNHANDLED 0xFFu

#define VIA_VALUE_FIRMWARE_VERSION 0x04u
#define VIA_VALUE_KEYCODES_VERSION 0x06u

static void clear_report(uint8_t PEDAL_XDATA *report)
{
    uint8_t i;

    for (i = 0; i < PEDAL_KEYBOARD_REPORT_SIZE; ++i) {
        report[i] = 0;
    }
}

uint8_t pedal_basic_key_valid(uint8_t key)
{
    return key == 0 || (key >= 0x04u && key <= 0xA4u) ||
           (key >= 0xE0u && key <= 0xE7u);
}

uint8_t pedal_keycode_report(uint16_t keycode,
                             uint8_t PEDAL_XDATA *report)
{
    uint8_t key = (uint8_t)keycode;
    uint8_t mods = (uint8_t)(keycode >> 8);

    if (keycode >= PEDAL_MACRO_KEYCODE &&
        keycode < PEDAL_MACRO_KEYCODE + PEDAL_MACRO_COUNT) {
        clear_report(report);
        return 1;
    }
    if (!pedal_basic_key_valid(key)) {
        return 0;
    }
    if (mods != 0) {
        if (mods > 0x1Fu || (mods & 0x0Fu) == 0) {
            return 0;
        }
        if ((mods & 0x10u) != 0) {
            mods = (uint8_t)((mods & 0x0Fu) << 4);
        }
    }
    clear_report(report);
    if (key >= 0xE0u) {
        mods |= (uint8_t)(1u << (key - 0xE0u));
    } else {
        report[2] = key;
    }
    report[0] = mods;
    return 1;
}

uint16_t pedal_crc16(const uint8_t PEDAL_XDATA *bytes, uint8_t length)
{
    uint16_t crc = 0xFFFFu;
    uint8_t i;
    uint8_t bit;

    for (i = 0; i < length; ++i) {
        crc ^= (uint16_t)bytes[i] << 8;
        for (bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000u) != 0) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

uint16_t pedal_record_crc(const uint8_t PEDAL_XDATA *record)
{
    return pedal_crc16(record, 6);
}

uint8_t pedal_core_init(pedal_core_t PEDAL_XDATA *core, const uint8_t PEDAL_XDATA *record,
                        uint8_t record_length)
{
    uint16_t keycode;
    uint16_t crc;

    core->keycode = PEDAL_DEFAULT_KEYCODE;
    core->persistence_dirty = 1;
    core->down = 0;
    clear_report(core->keyboard_report);
    if (record == 0 || record_length != PEDAL_STORAGE_SIZE ||
        record[0] != 0x50u || record[1] != 0x44u || record[2] != 1 ||
        record[3] != 0) {
        return 0;
    }
    crc = (uint16_t)((uint16_t)record[6] << 8) | record[7];
    if (pedal_record_crc(record) != crc) {
        return 0;
    }
    keycode = (uint16_t)((uint16_t)record[4] << 8) | record[5];
    if (!pedal_keycode_report(keycode, core->keyboard_report)) {
        return 0;
    }
    core->keycode = keycode;
    core->persistence_dirty = 0;
    clear_report(core->keyboard_report);
    return 1;
}

void pedal_core_record(const pedal_core_t PEDAL_XDATA *core,
                        uint8_t PEDAL_XDATA *record)
{
    uint16_t crc;

    record[0] = 0x50u;
    record[1] = 0x44u;
    record[2] = 1;
    record[3] = 0;
    record[4] = (uint8_t)(core->keycode >> 8);
    record[5] = (uint8_t)core->keycode;
    crc = pedal_record_crc(record);
    record[6] = (uint8_t)(crc >> 8);
    record[7] = (uint8_t)crc;
}

uint8_t pedal_core_update(pedal_core_t PEDAL_XDATA *core, uint8_t down,
                          uint8_t PEDAL_XDATA *report)
{
    uint8_t i;

    down = down != 0;
    if (down == core->down) {
        return 0;
    }
    if (down) {
        pedal_keycode_report(core->keycode, core->keyboard_report);
    } else {
        clear_report(core->keyboard_report);
    }
    core->down = down;
    for (i = 0; i < PEDAL_KEYBOARD_REPORT_SIZE; ++i) {
        report[i] = core->keyboard_report[i];
    }
    return 1;
}

void pedal_core_inhibit(pedal_core_t PEDAL_XDATA *core, uint8_t down)
{
    core->down = down != 0;
}

static uint8_t set_keycode(pedal_core_t PEDAL_XDATA *core, uint16_t keycode)
{
    PEDAL_XDATA uint8_t report[PEDAL_KEYBOARD_REPORT_SIZE];

    if (!pedal_keycode_report(keycode, report)) {
        return 0;
    }
    if (keycode == core->keycode && !core->persistence_dirty) {
        return PEDAL_VIA_HANDLED;
    }
    core->keycode = keycode;
    core->persistence_dirty = 1;
    return PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE;
}

uint8_t pedal_core_via(pedal_core_t PEDAL_XDATA *core, uint8_t PEDAL_XDATA *report,
                       uint8_t report_length, uint32_t uptime_ms)
{
    uint16_t offset;
    uint16_t keycode;
    uint8_t size;
    uint8_t i;
    uint8_t flags;
    uint8_t bytes[2];

    (void)uptime_ms;
    if (report_length != PEDAL_RAW_REPORT_SIZE) {
        return 0;
    }
    switch (report[0]) {
    case VIA_GET_PROTOCOL_VERSION:
        report[1] = 0;
        report[2] = 13;
        return PEDAL_VIA_HANDLED;
    case VIA_GET_KEYBOARD_VALUE:
        switch (report[1]) {
        case VIA_VALUE_FIRMWARE_VERSION:
        case VIA_VALUE_KEYCODES_VERSION:
            for (i = 2; i < 5; ++i) {
                report[i] = 0;
            }
            report[5] = report[1] == VIA_VALUE_KEYCODES_VERSION
                            ? 9 : PEDAL_FIRMWARE_VERSION;
            return PEDAL_VIA_HANDLED;
        default:
            break;
        }
        break;
    case VIA_GET_KEYCODE:
    case VIA_SET_KEYCODE:
        if (report[1] != 0 || report[2] != 0 || report[3] != 0) {
            break;
        }
        if (report[0] == VIA_GET_KEYCODE) {
            report[4] = (uint8_t)(core->keycode >> 8);
            report[5] = (uint8_t)core->keycode;
            return PEDAL_VIA_HANDLED;
        }
        keycode = (uint16_t)((uint16_t)report[4] << 8) | report[5];
        flags = set_keycode(core, keycode);
        if (flags != 0) {
            return flags;
        }
        break;
    case VIA_RESET_KEYMAP:
        return set_keycode(core, PEDAL_DEFAULT_KEYCODE);
    case VIA_BOOTLOADER_JUMP:
        return PEDAL_VIA_HANDLED | PEDAL_VIA_BOOTLOADER;
    case VIA_MACRO_GET_COUNT:
        report[1] = PEDAL_MACRO_COUNT;
        return PEDAL_VIA_HANDLED;
    case VIA_MACRO_GET_BUFFER_SIZE:
        report[1] = 0;
        report[2] = PEDAL_MACRO_BUFFER_SIZE;
        return PEDAL_VIA_HANDLED;
    case VIA_MACRO_GET_BUFFER:
    case VIA_MACRO_SET_BUFFER:
        offset = (uint16_t)((uint16_t)report[1] << 8) | report[2];
        size = report[3];
        if (size > PEDAL_RAW_REPORT_SIZE - 4u) break;
        if (report[0] == VIA_MACRO_GET_BUFFER)
            return PEDAL_VIA_HANDLED | PEDAL_VIA_MACRO_READ;
        if (offset > PEDAL_MACRO_BUFFER_SIZE ||
            size > PEDAL_MACRO_BUFFER_SIZE - offset) break;
        return PEDAL_VIA_HANDLED | PEDAL_VIA_MACRO_WRITE;
    case VIA_MACRO_RESET:
        return PEDAL_VIA_HANDLED | PEDAL_VIA_MACRO_RESET;
    case VIA_GET_LAYER_COUNT:
        report[1] = 1;
        return PEDAL_VIA_HANDLED;
    case VIA_GET_BUFFER:
    case VIA_SET_BUFFER:
        offset = (uint16_t)((uint16_t)report[1] << 8) | report[2];
        size = report[3];
        if (offset > 2 || size > 2u - offset) {
            break;
        }
        bytes[0] = (uint8_t)(core->keycode >> 8);
        bytes[1] = (uint8_t)core->keycode;
        if (report[0] == VIA_GET_BUFFER) {
            for (i = 0; i < size; ++i) {
                report[4 + i] = bytes[offset + i];
            }
            return PEDAL_VIA_HANDLED;
        }
        if (size == 0) {
            return PEDAL_VIA_HANDLED;
        }
        for (i = 0; i < size; ++i) {
            bytes[offset + i] = report[4 + i];
        }
        keycode = (uint16_t)((uint16_t)bytes[0] << 8) | bytes[1];
        flags = set_keycode(core, keycode);
        if (flags != 0) {
            return flags;
        }
        break;
    default:
        break;
    }
    report[0] = VIA_UNHANDLED;
    return 0;
}
