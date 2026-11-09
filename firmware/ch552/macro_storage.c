/* SPDX-License-Identifier: GPL-3.0-or-later
 * Single-copy macros in CH552 DataFlash, independent of the keymap journal. */
#include "macro_storage.h"
#include "storage_backend.h"

#define MACRO_BASE STORAGE_FLASH_BYTES
#define MACRO_COMMIT (MACRO_BASE + PEDAL_MACRO_BUFFER_SIZE)
#define MACRO_CRC_H (MACRO_COMMIT + 1u)
#define MACRO_CRC_L (MACRO_COMMIT + 2u)
#define MACRO_VALID 0xA5u
#define UPLOADING 1u
#define FAILED 2u

static uint8_t valid;
static uint8_t session;
static uint8_t written;

static uint8_t update_byte(uint8_t address, uint8_t value) {
  if (storage_read_byte(address) == value) return 1;
  return storage_write_byte(address, value) && storage_read_byte(address) == value;
}

static uint8_t invalidate(void) {
  valid = 0;
  session = FAILED;
  return update_byte(MACRO_COMMIT, 0);
}

static uint8_t publish(const uint8_t PEDAL_XDATA *buffer) {
  uint8_t i;
  uint16_t crc;
  if (buffer[PEDAL_MACRO_BUFFER_SIZE - 1] ||
      !pedal_macro_valid(buffer, PEDAL_MACRO_BUFFER_SIZE)) return 0;
  for (i = 0; i < PEDAL_MACRO_BUFFER_SIZE; ++i)
    if (storage_read_byte(MACRO_BASE + i) != buffer[i]) return 0;
  crc = pedal_crc16(buffer, PEDAL_MACRO_BUFFER_SIZE);
  if (!update_byte(MACRO_CRC_H, (uint8_t)(crc >> 8)) ||
      !update_byte(MACRO_CRC_L, (uint8_t)crc) ||
      !update_byte(MACRO_COMMIT, MACRO_VALID)) return 0;
  valid = 1;
  session = 0;
  return 1;
}

void storage_macro_load(uint8_t PEDAL_XDATA *buffer) {
  uint8_t i;
  uint16_t crc;
  session = written = valid = 0;
  for (i = 0; i < PEDAL_MACRO_BUFFER_SIZE; ++i)
    buffer[i] = storage_read_byte(MACRO_BASE + i);
  crc = ((uint16_t)storage_read_byte(MACRO_CRC_H) << 8) |
        storage_read_byte(MACRO_CRC_L);
  if (storage_read_byte(MACRO_COMMIT) == MACRO_VALID &&
      pedal_crc16(buffer, PEDAL_MACRO_BUFFER_SIZE) == crc &&
      !buffer[PEDAL_MACRO_BUFFER_SIZE - 1] &&
      pedal_macro_valid(buffer, PEDAL_MACRO_BUFFER_SIZE)) valid = 1;
  if (!valid)
    for (i = 0; i < PEDAL_MACRO_BUFFER_SIZE; ++i) buffer[i] = 0;
}

uint8_t storage_macro_ready(void) {
  return valid;
}

void storage_macro_cancel(void) {
  if (session == UPLOADING) session = FAILED;
}

uint8_t storage_macro_reset(uint8_t PEDAL_XDATA *buffer) {
  uint8_t i;
  if (valid) {
    for (i = 0; i < PEDAL_MACRO_BUFFER_SIZE && !buffer[i]; ++i) {}
    if (i == PEDAL_MACRO_BUFFER_SIZE) return 1;
  }
  if (!invalidate()) return 0;
  for (i = 0; i < PEDAL_MACRO_BUFFER_SIZE; ++i) {
    if (!update_byte(MACRO_BASE + i, 0)) return 0;
    buffer[i] = 0;
  }
  return publish(buffer);
}

uint8_t storage_macro_write(uint8_t PEDAL_XDATA *buffer, uint16_t offset, uint8_t size,
                            const uint8_t PEDAL_XDATA *data) {
  uint8_t i;
  if (offset > PEDAL_MACRO_BUFFER_SIZE || size > 28 ||
      size > PEDAL_MACRO_BUFFER_SIZE - offset) {
    storage_macro_cancel();
    return 0;
  }
  if (!size) return 1;
  if (offset == PEDAL_MACRO_BUFFER_SIZE - 1 && size == 1) {
    if (data[0] == 0xFF) {
      if (!invalidate() || !update_byte(MACRO_BASE + offset, 0xFF)) return 0;
      buffer[offset] = 0xFF;
      written = 0;
      session = UPLOADING;
      return 1;
    }
    if (!data[0]) {
      if (session != UPLOADING) return 0;
      session = FAILED;
      if (!update_byte(MACRO_BASE + offset, 0)) return 0;
      buffer[offset] = 0;
      /* Only received bytes may supply the four sequence terminators. The
       * reset's zero padding cannot disguise a missing final chunk. */
      if (!pedal_macro_valid(buffer, written)) return 0;
      return publish(buffer);
    }
  }
  if (session != UPLOADING) return 0;
  if (offset != written) {
    storage_macro_cancel();
    return 0;
  }
  for (i = 0; i < size; ++i) {
    if (!update_byte(MACRO_BASE + offset + i, data[i])) {
      storage_macro_cancel();
      return 0;
    }
    buffer[offset + i] = data[i];
  }
  written += size;
  return 1;
}
