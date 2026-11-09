/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pedal_core.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    pedal_core_t core;
    uint8_t storage[PEDAL_STORAGE_SIZE];
    unsigned int writes;
    unsigned int boot_requests;
} fixture_t;

static void request(uint8_t report[PEDAL_RAW_REPORT_SIZE], uint8_t command)
{
    memset(report, 0, PEDAL_RAW_REPORT_SIZE);
    report[0] = command;
}

static void start(fixture_t *device)
{
    memset(device, 0, sizeof(*device));
    memset(device->storage, 0xFF, sizeof(device->storage));
    assert(pedal_core_init(&device->core, device->storage,
                           sizeof(device->storage)) == 0);
    assert(device->core.keycode == PEDAL_DEFAULT_KEYCODE);
    /* Protocol-only fixture starts with a durable default; the real journal
     * and blank/failed-save dirty states are exercised by tests/board. */
    pedal_core_record(&device->core, device->storage);
    device->core.persistence_dirty = 0;
}

/* A board stand-in: the core cannot write storage or enter ISP by itself. */
static uint8_t transact(fixture_t *device, uint8_t *report, uint8_t length)
{
    uint8_t flags = pedal_core_via(&device->core, report, length, 0x12345678UL);

    if ((flags & PEDAL_VIA_SAVE) != 0) {
        assert((flags & PEDAL_VIA_HANDLED) != 0);
        assert((flags & PEDAL_VIA_BOOTLOADER) == 0);
        pedal_core_record(&device->core, device->storage);
        device->core.persistence_dirty = 0;
        ++device->writes;
    }
    if ((flags & PEDAL_VIA_BOOTLOADER) != 0) {
        ++device->boot_requests;
    }
    return flags;
}

static uint8_t remap(fixture_t *device, uint16_t keycode)
{
    uint8_t report[PEDAL_RAW_REPORT_SIZE];

    request(report, 0x05);
    report[4] = (uint8_t)(keycode >> 8);
    report[5] = (uint8_t)keycode;
    return transact(device, report, sizeof(report));
}

static void expect_report(const uint8_t *report, uint8_t mods, uint8_t key)
{
    const uint8_t expected[PEDAL_KEYBOARD_REPORT_SIZE] = {
        mods, 0, key, 0, 0, 0, 0, 0
    };

    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_keycodes(void)
{
    static const struct {
        uint16_t keycode;
        uint8_t mods;
        uint8_t key;
    } cases[] = {
        {0x0000, 0x00, 0x00}, /* KC_NO */
        {0x0004, 0x00, 0x04}, /* A */
        {0x002C, 0x00, 0x2C}, /* Space */
        {0x0052, 0x00, 0x52}, /* Up */
        {0x0073, 0x00, 0x73}, /* F24 */
        {0x00A4, 0x00, 0xA4}, /* ExSel, last basic keyboard usage */
        {0x0106, 0x01, 0x06}, /* LCtrl+C */
        {0x030E, 0x03, 0x0E}, /* LCtrl+LShift+K */
        {0x1106, 0x10, 0x06}, /* RCtrl+C */
        {0x130E, 0x30, 0x0E}, /* RCtrl+RShift+K */
        {0x0F04, 0x0F, 0x04}, /* all left mods + A */
        {0x1F04, 0xF0, 0x04}, /* all right mods + A */
        {0x0100, 0x01, 0x00}, /* QK_MODS + KC_NO */
        {0x1400, 0x40, 0x00}, /* right-alt-only chord */
        {0x01E6, 0x41, 0x00}, /* left ctrl + right alt */
        {0x11E1, 0x12, 0x00}, /* right ctrl + left shift */
        {0x00E0, 0x01, 0x00},
        {0x00E1, 0x02, 0x00},
        {0x00E2, 0x04, 0x00},
        {0x00E3, 0x08, 0x00},
        {0x00E4, 0x10, 0x00},
        {0x00E5, 0x20, 0x00},
        {0x00E6, 0x40, 0x00},
        {0x00E7, 0x80, 0x00}
    };
    static const uint16_t invalid[] = {
        0x0001, 0x0002, 0x0003, 0x00A5, 0x00A8, 0x00DF, 0x00E8, 0x00FF,
        0x0101, 0x03A8, 0x1000, 0x1004, 0x1FE8, 0x2004, 0x4004, 0x5200,
        0x7700, 0x7E00, 0xFFFF
    };
    uint8_t report[PEDAL_KEYBOARD_REPORT_SIZE];
    uint8_t untouched[PEDAL_KEYBOARD_REPORT_SIZE];
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        memset(report, 0xCC, sizeof(report));
        assert(pedal_keycode_report(cases[i].keycode, report) == 1);
        expect_report(report, cases[i].mods, cases[i].key);
    }
    memset(untouched, 0xCC, sizeof(untouched));
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        memcpy(report, untouched, sizeof(report));
        assert(pedal_keycode_report(invalid[i], report) == 0);
        assert(memcmp(report, untouched, sizeof(report)) == 0);
    }
}

static void test_queries(void)
{
    fixture_t device;
    uint8_t report[PEDAL_RAW_REPORT_SIZE];
    uint8_t keyboard[PEDAL_KEYBOARD_REPORT_SIZE];

    start(&device);
    request(report, 0x01);
    assert(transact(&device, report, sizeof(report)) == PEDAL_VIA_HANDLED);
    assert(report[0] == 0x01 && report[1] == 0 && report[2] == 8);

    request(report, 0x11);
    assert(transact(&device, report, sizeof(report)) == PEDAL_VIA_HANDLED);
    assert(report[1] == 1);

    request(report, 0x04);
    assert(transact(&device, report, sizeof(report)) == PEDAL_VIA_HANDLED);
    assert(report[4] == 0 && report[5] == 0x2C);

    request(report, 0x02);
    report[1] = 0x01;
    assert(transact(&device, report, sizeof(report)) == PEDAL_VIA_HANDLED);
    assert(report[2] == 0x12 && report[3] == 0x34);
    assert(report[4] == 0x56 && report[5] == 0x78);

    memset(report, 0xFF, sizeof(report));
    report[0] = 0x02;
    report[1] = 0x02;
    assert(transact(&device, report, sizeof(report)) == PEDAL_VIA_HANDLED);
    assert(report[2] == 0 && report[3] == 0 && report[4] == 0 && report[5] == 0);

    request(report, 0x02);
    report[1] = 0x03;
    assert(transact(&device, report, sizeof(report)) == PEDAL_VIA_HANDLED);
    assert(report[2] == 0);
    assert(pedal_core_update(&device.core, 1, keyboard) == 1);
    assert(transact(&device, report, sizeof(report)) == PEDAL_VIA_HANDLED);
    assert(report[2] == 1);
    assert(pedal_core_update(&device.core, 0, keyboard) == 1);
    assert(transact(&device, report, sizeof(report)) == PEDAL_VIA_HANDLED);
    assert(report[2] == 0);

    memset(report, 0xFF, sizeof(report));
    report[0] = 0x0C;
    assert(transact(&device, report, sizeof(report)) == PEDAL_VIA_HANDLED);
    assert(report[1] == 0);
    memset(report, 0xFF, sizeof(report));
    report[0] = 0x0D;
    assert(transact(&device, report, sizeof(report)) == PEDAL_VIA_HANDLED);
    assert(report[1] == 0 && report[2] == 0);
    assert(device.writes == 0 && device.boot_requests == 0);
}

static void test_invalid_commands(void)
{
    static const uint8_t unknown[] = {
        0x00, 0x03, 0x07, 0x08, 0x09, 0x0A, 0x0E, 0x0F, 0x10, 0x14, 0xFE, 0xFF
    };
    fixture_t device;
    uint8_t report[256];
    uint8_t before[256];
    unsigned int length;
    unsigned int coordinate;
    unsigned int value;
    uint8_t command;
    size_t i;

    start(&device);
    memset(report, 0xCC, sizeof(report));
    report[0] = 0x05;
    report[1] = 0;
    report[2] = 0;
    report[3] = 0;
    report[4] = 0x03;
    report[5] = 0x0E;
    memcpy(before, report, sizeof(report));
    for (length = 0; length <= 255; ++length) {
        if (length == PEDAL_RAW_REPORT_SIZE) {
            continue;
        }
        assert(transact(&device, report, (uint8_t)length) == 0);
        assert(memcmp(report, before, sizeof(report)) == 0);
        assert(device.core.keycode == PEDAL_DEFAULT_KEYCODE);
    }
    /* A zero-length report must not dereference its pointer. */
    assert(transact(&device, 0, 0) == 0);

    for (command = 0x04; command <= 0x05; ++command) {
        for (coordinate = 1; coordinate <= 3; ++coordinate) {
            for (value = 1; value <= 255; ++value) {
                request(report, command);
                report[coordinate] = (uint8_t)value;
                report[4] = 0x03;
                report[5] = 0x0E;
                memcpy(before, report, PEDAL_RAW_REPORT_SIZE);
                assert(transact(&device, report, PEDAL_RAW_REPORT_SIZE) == 0);
                assert(report[0] == 0xFF);
                assert(memcmp(report + 1, before + 1, PEDAL_RAW_REPORT_SIZE - 1) == 0);
                assert(device.core.keycode == PEDAL_DEFAULT_KEYCODE);
            }
        }
    }
    for (i = 0; i < sizeof(unknown); ++i) {
        request(report, unknown[i]);
        assert(transact(&device, report, PEDAL_RAW_REPORT_SIZE) == 0);
        assert(report[0] == 0xFF);
    }
    request(report, 0x02);
    report[1] = 0xFF;
    assert(transact(&device, report, PEDAL_RAW_REPORT_SIZE) == 0);
    assert(report[0] == 0xFF);
    assert(remap(&device, 0x7E00) == 0);
    assert(remap(&device, 0x00A8) == 0);
    assert(remap(&device, 0x0101) == 0);
    assert(device.core.keycode == PEDAL_DEFAULT_KEYCODE);
    assert(device.writes == 0 && device.boot_requests == 0);
}

static void test_buffers(void)
{
    static const uint16_t offsets[] = {0, 1, 2, 3, 255, 256, 65535};
    fixture_t device;
    uint8_t report[PEDAL_RAW_REPORT_SIZE + 2];
    uint8_t before[PEDAL_RAW_REPORT_SIZE + 2];
    uint8_t *packet = report + 1;
    unsigned int size;
    unsigned int command;
    unsigned int i;
    uint16_t offset;
    uint8_t expected;

    start(&device);
    for (command = 0x12; command <= 0x13; ++command) {
        for (i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
            offset = offsets[i];
            for (size = 0; size <= 255; ++size) {
                memset(report, 0xCC, sizeof(report));
                request(packet, (uint8_t)command);
                packet[1] = (uint8_t)(offset >> 8);
                packet[2] = (uint8_t)offset;
                packet[3] = (uint8_t)size;
                /* For every valid write, supply exactly the current bytes. */
                packet[4] = offset == 0 ? 0 : 0x2C;
                packet[5] = 0x2C;
                memcpy(before, report, sizeof(report));
                expected = offset <= 2 && size <= 2u - offset
                               ? PEDAL_VIA_HANDLED : 0;
                assert(transact(&device, packet, PEDAL_RAW_REPORT_SIZE) == expected);
                assert(report[0] == 0xCC && report[PEDAL_RAW_REPORT_SIZE + 1] == 0xCC);
                if (expected == 0) {
                    assert(packet[0] == 0xFF);
                    assert(memcmp(report + 2, before + 2, PEDAL_RAW_REPORT_SIZE) == 0);
                } else if (command == 0x12 && size != 0) {
                    assert(packet[4] == (offset == 0 ? 0 : 0x2C));
                    if (size == 2) {
                        assert(packet[5] == 0x2C);
                    }
                }
                assert(device.core.keycode == PEDAL_DEFAULT_KEYCODE);
            }
        }
    }
    assert(device.writes == 0);

    request(packet, 0x13);
    packet[3] = 1;
    packet[4] = 0x01;
    assert(transact(&device, packet, PEDAL_RAW_REPORT_SIZE) ==
           (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
    assert(device.core.keycode == 0x012C);
    request(packet, 0x13);
    packet[2] = 1;
    packet[3] = 1;
    packet[4] = 0x06;
    assert(transact(&device, packet, PEDAL_RAW_REPORT_SIZE) ==
           (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
    assert(device.core.keycode == 0x0106);

    request(packet, 0x12);
    packet[3] = 2;
    assert(transact(&device, packet, PEDAL_RAW_REPORT_SIZE) == PEDAL_VIA_HANDLED);
    assert(packet[4] == 0x01 && packet[5] == 0x06);

    request(packet, 0x13);
    packet[3] = 2;
    packet[4] = 0x20;
    packet[5] = 0x04;
    assert(transact(&device, packet, PEDAL_RAW_REPORT_SIZE) == 0);
    assert(packet[0] == 0xFF && device.core.keycode == 0x0106);

    request(packet, 0x13);
    packet[2] = 1;
    packet[3] = 1;
    packet[4] = 0xA8;
    assert(transact(&device, packet, PEDAL_RAW_REPORT_SIZE) == 0);
    assert(packet[0] == 0xFF && device.core.keycode == 0x0106);

    request(packet, 0x13);
    packet[3] = 2;
    packet[4] = 0x03;
    packet[5] = 0x0E;
    assert(transact(&device, packet, PEDAL_RAW_REPORT_SIZE) ==
           (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
    assert(device.core.keycode == 0x030E && device.writes == 3);
    assert(transact(&device, packet, PEDAL_RAW_REPORT_SIZE) == PEDAL_VIA_HANDLED);
    assert(device.writes == 3);
}

static uint16_t test_crc(const uint8_t *bytes, size_t length)
{
    uint16_t crc = 0xFFFF;
    unsigned int bit;
    size_t i;

    for (i = 0; i < length; ++i) {
        for (bit = 0; bit < 8; ++bit) {
            unsigned int high = ((crc >> 15) ^ (bytes[i] >> (7 - bit))) & 1u;
            crc = (uint16_t)(crc << 1);
            if (high != 0) {
                crc ^= 0x1021;
            }
        }
    }
    return crc;
}

static void fix_crc(uint8_t *record)
{
    uint16_t crc = test_crc(record, 6);

    record[6] = (uint8_t)(crc >> 8);
    record[7] = (uint8_t)crc;
}

static void test_persistence_and_boot(void)
{
    fixture_t device;
    pedal_core_t restarted;
    uint8_t record[PEDAL_STORAGE_SIZE];
    uint8_t report[PEDAL_RAW_REPORT_SIZE];
    unsigned int byte;
    unsigned int bit;
    unsigned int length;
    uint16_t crc;

    start(&device);
    assert(test_crc((const uint8_t *)"123456789", 9) == 0x29B1);
    assert(remap(&device, PEDAL_DEFAULT_KEYCODE) == PEDAL_VIA_HANDLED);
    assert(device.writes == 0);
    assert(remap(&device, 0x030E) == (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
    assert(device.writes == 1);
    assert(remap(&device, 0x030E) == PEDAL_VIA_HANDLED);
    assert(device.writes == 1);
    assert(device.storage[0] == 0x50 && device.storage[1] == 0x44);
    assert(device.storage[2] == 1 && device.storage[3] == 0);
    assert(device.storage[4] == 0x03 && device.storage[5] == 0x0E);
    crc = test_crc(device.storage, 6);
    assert(device.storage[6] == (uint8_t)(crc >> 8));
    assert(device.storage[7] == (uint8_t)crc);
    assert(pedal_core_init(&restarted, device.storage, sizeof(device.storage)) == 1);
    assert(restarted.keycode == 0x030E && restarted.down == 0);
    expect_report(restarted.keyboard_report, 0, 0);

    for (byte = 0; byte < sizeof(record); ++byte) {
        for (bit = 0; bit < 8; ++bit) {
            memcpy(record, device.storage, sizeof(record));
            record[byte] ^= (uint8_t)(1u << bit);
            assert(pedal_core_init(&restarted, record, sizeof(record)) == 0);
            assert(restarted.keycode == PEDAL_DEFAULT_KEYCODE);
            assert(restarted.down == 0);
            expect_report(restarted.keyboard_report, 0, 0);
        }
    }
    memcpy(record, device.storage, sizeof(record));
    record[2] = 2;
    fix_crc(record);
    assert(pedal_core_init(&restarted, record, sizeof(record)) == 0);
    record[2] = 1;
    record[3] = 1;
    fix_crc(record);
    assert(pedal_core_init(&restarted, record, sizeof(record)) == 0);
    record[3] = 0;
    record[4] = 0x7E;
    record[5] = 0x00;
    fix_crc(record);
    assert(pedal_core_init(&restarted, record, sizeof(record)) == 0);
    assert(restarted.keycode == PEDAL_DEFAULT_KEYCODE);
    expect_report(restarted.keyboard_report, 0, 0);

    for (length = 0; length <= 255; ++length) {
        if (length == PEDAL_STORAGE_SIZE) {
            continue;
        }
        assert(pedal_core_init(&restarted, device.storage, (uint8_t)length) == 0);
        assert(restarted.keycode == PEDAL_DEFAULT_KEYCODE);
    }
    assert(pedal_core_init(&restarted, 0, PEDAL_STORAGE_SIZE) == 0);
    memset(record, 0, sizeof(record));
    assert(pedal_core_init(&restarted, record, sizeof(record)) == 0);
    memset(record, 0xFF, sizeof(record));
    assert(pedal_core_init(&restarted, record, sizeof(record)) == 0);

    memcpy(record, device.storage, sizeof(record));
    request(report, 0x0B);
    assert(transact(&device, report, sizeof(report)) ==
           (PEDAL_VIA_HANDLED | PEDAL_VIA_BOOTLOADER));
    assert(device.writes == 1 && device.boot_requests == 1);
    assert(device.core.keycode == 0x030E);
    assert(memcmp(record, device.storage, sizeof(record)) == 0);

    request(report, 0x06);
    assert(transact(&device, report, sizeof(report)) ==
           (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
    assert(device.core.keycode == PEDAL_DEFAULT_KEYCODE && device.writes == 2);
    assert(transact(&device, report, sizeof(report)) == PEDAL_VIA_HANDLED);
    assert(device.writes == 2);

    assert(remap(&device, 0x0000) == (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
    assert(pedal_core_init(&restarted, device.storage, sizeof(device.storage)) == 1);
    assert(restarted.keycode == 0x0000);
}

static void test_runtime_and_independent_devices(void)
{
    fixture_t first;
    fixture_t second;
    pedal_core_t restarted;
    uint8_t report[PEDAL_KEYBOARD_REPORT_SIZE];
    uint8_t original[PEDAL_KEYBOARD_REPORT_SIZE];

    start(&first);
    start(&second);
    memset(report, 0xCC, sizeof(report));
    memcpy(original, report, sizeof(original));
    assert(pedal_core_update(&first.core, 0, report) == 0);
    assert(memcmp(report, original, sizeof(report)) == 0);
    assert(remap(&first, 0x0106) == (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
    assert(pedal_core_update(&first.core, 1, report) == 1);
    expect_report(report, 0x01, 0x06);
    memcpy(original, report, sizeof(original));
    assert(remap(&first, 0x130E) == (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
    assert(memcmp(first.core.keyboard_report, original, sizeof(original)) == 0);
    assert(pedal_core_update(&first.core, 255, report) == 0);
    assert(memcmp(report, original, sizeof(report)) == 0);
    assert(pedal_core_update(&first.core, 0, report) == 1);
    expect_report(report, 0, 0);
    assert(pedal_core_update(&first.core, 1, report) == 1);
    expect_report(report, 0x30, 0x0E);
    assert(pedal_core_update(&first.core, 0, report) == 1);
    expect_report(report, 0, 0);

    assert(remap(&first, 0x00E7) == (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
    assert(pedal_core_update(&first.core, 1, report) == 1);
    expect_report(report, 0x80, 0);
    assert(remap(&first, 0) == (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
    expect_report(first.core.keyboard_report, 0x80, 0);
    assert(pedal_core_update(&first.core, 0, report) == 1);
    expect_report(report, 0, 0);
    assert(pedal_core_update(&first.core, 1, report) == 1);
    expect_report(report, 0, 0);
    assert(first.core.down == 1);
    assert(pedal_core_update(&first.core, 0, report) == 1);
    expect_report(report, 0, 0);

    assert(second.core.keycode == PEDAL_DEFAULT_KEYCODE && second.writes == 0);
    assert(pedal_core_update(&second.core, 1, report) == 1);
    expect_report(report, 0, 0x2C);
    assert(pedal_core_update(&second.core, 0, report) == 1);
    expect_report(report, 0, 0);
    assert(remap(&second, 0x030E) == (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
    assert(first.core.keycode == 0 && second.core.keycode == 0x030E);
    assert(pedal_core_init(&restarted, first.storage, sizeof(first.storage)) == 1);
    assert(restarted.keycode == 0);
    assert(pedal_core_init(&restarted, second.storage, sizeof(second.storage)) == 1);
    assert(restarted.keycode == 0x030E);
}

int main(void)
{
    test_keycodes();
    test_queries();
    test_invalid_commands();
    test_buffers();
    test_persistence_and_boot();
    test_runtime_and_independent_devices();
    puts("core tests: PASS (keycodes, VIA, bounds, persistence, runtime, isolation)");
    return 0;
}
