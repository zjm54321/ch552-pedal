/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host-testable two-slot DataFlash journal; no SFRs or code-Flash writes. */
#include "pedal_core.h"
#include "storage.h"
#include "storage_backend.h"

#define SLOT_COMMIT 0xA5u
#define SLOT_NONE 0xFFu
#define SLOT_LEGACY 0xFEu

static uint8_t active_slot;
static uint16_t generation;

static void read_slot(uint8_t base, uint8_t PEDAL_XDATA *bytes) {
  uint8_t i;
  for (i = 0; i < PEDAL_STORAGE_SIZE; ++i)
    bytes[i] = storage_read_byte(base + i);
}

static uint8_t valid_slot(const uint8_t PEDAL_XDATA *bytes, uint8_t PEDAL_XDATA *report) {
  uint16_t crc = ((uint16_t)bytes[6] << 8) | bytes[7];
  return bytes[0] == SLOT_COMMIT && bytes[1] == 2 &&
         pedal_record_crc(bytes) == crc &&
         pedal_keycode_report(((uint16_t)bytes[4] << 8) | bytes[5], report);
}

void storage_load(uint8_t PEDAL_XDATA *record) {
  uint8_t base;
  uint16_t next_generation;
  PEDAL_XDATA pedal_core_t loaded;
  /* A valid v1 record at zero remains readable, and is never overwritten by
   * the first migration save: that save targets the second slot. */
  read_slot(0, record);
  active_slot = pedal_core_init(&loaded, record, PEDAL_STORAGE_SIZE)
                    ? SLOT_LEGACY : SLOT_NONE;
  generation = 0;
  for (base = 0; base < STORAGE_FLASH_BYTES; base += PEDAL_STORAGE_SIZE) {
    read_slot(base, record);
    if (!valid_slot(record, loaded.keyboard_report)) continue;
    next_generation = ((uint16_t)record[2] << 8) | record[3];
    /* Only adjacent generations coexist; unsigned subtraction handles wrap. */
    if (active_slot >= SLOT_LEGACY ||
        (uint16_t)(next_generation - generation) < 0x8000u) {
      active_slot = base;
      generation = next_generation;
      loaded.keycode = ((uint16_t)record[4] << 8) | record[5];
    }
  }
  if (active_slot == SLOT_NONE) {
    record[0] = 0; /* Let core initialization mark the default as not durable. */
  } else {
    pedal_core_record(&loaded, record);
  }
}

uint8_t storage_save(const uint8_t PEDAL_XDATA *record) {
  PEDAL_XDATA uint8_t bytes[PEDAL_STORAGE_SIZE];
  uint8_t i;
  uint8_t target = active_slot == 0 || active_slot == SLOT_LEGACY ? 8 : 0;
  uint16_t next_generation = generation + 1u;
  uint16_t crc;
  PEDAL_XDATA pedal_core_t proposed;
  if (!pedal_core_init(&proposed, record, PEDAL_STORAGE_SIZE)) return 0;
  bytes[0] = SLOT_COMMIT;
  bytes[1] = 2;
  bytes[2] = (uint8_t)(next_generation >> 8);
  bytes[3] = (uint8_t)next_generation;
  bytes[4] = record[4];
  bytes[5] = record[5];
  crc = pedal_record_crc(bytes);
  bytes[6] = (uint8_t)(crc >> 8);
  bytes[7] = (uint8_t)crc;

  /* Never touch the active slot. Verify the entire unpublished candidate
   * before committing, then verify the commit too. Even a failed final
   * readback leaves the old slot intact; the caller marks RAM dirty for retry. */
  if (!storage_write_byte(target, 0)) return 0;
  for (i = 1; i < PEDAL_STORAGE_SIZE; ++i)
    if (!storage_write_byte(target + i, bytes[i])) return 0;
  if (storage_read_byte(target) != 0) return 0;
  for (i = 1; i < PEDAL_STORAGE_SIZE; ++i)
    if (storage_read_byte(target + i) != bytes[i]) return 0;
  if (!storage_write_byte(target, SLOT_COMMIT)) return 0;
  for (i = 0; i < PEDAL_STORAGE_SIZE; ++i)
    if (storage_read_byte(target + i) != bytes[i]) return 0;
  active_slot = target;
  generation = next_generation;
  return 1;
}
