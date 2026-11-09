/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "macro_storage.h"
#include "storage_backend.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static uint8_t flash[256], buffer[PEDAL_MACRO_BUFFER_SIZE];
static unsigned int writes, reads, fail_write, fail_read, mode, cut_after;
static jmp_buf power_loss;
static const uint8_t old_data[] = {'o', 'l', 'd', 0, 0, 0, 0};
/* More than one 28-byte chunk, with all four terminators in received data. */
static const uint8_t new_data[] = {
  'a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a',
  'a','a','a','a','a','a','a','a','a','a','a','a','a','a','a','a',0,
  1,2,0xE0,1,1,6,1,3,0xE0,0,'A','!',0,1,4,'1','0','|','x',0
};

uint8_t storage_read_byte(uint8_t address) {
  assert(address < STORAGE_DATA_BYTES);
  ++reads;
  return flash[2u * address] ^ (reads == fail_read ? 1 : 0);
}

uint8_t storage_write_byte(uint8_t address, uint8_t value) {
  assert(address >= STORAGE_FLASH_BYTES && address < STORAGE_DATA_BYTES);
  if (!cut_after) longjmp(power_loss, 1);
  ++writes;
  if (writes == fail_write) {
    if (mode == 1) flash[2u * address] = value; /* Complete byte, failed status. */
    if (mode == 2) flash[2u * address] = 0x5A; /* Torn byte. */
    return 0;
  }
  flash[2u * address] = value;
  if (writes == cut_after) longjmp(power_loss, 1);
  return 1;
}

static void no_faults(void) {
  writes = reads = fail_write = fail_read = mode = 0;
  cut_after = 10000;
}

static uint8_t marker(uint8_t value) {
  return storage_macro_write(buffer, PEDAL_MACRO_BUFFER_SIZE - 1, 1, &value);
}

static uint8_t upload(const uint8_t *data, uint8_t size) {
  uint8_t offset, count, success = 1, committed;
  if (!storage_macro_reset(buffer)) return 0; /* App reset is outside try/finally. */
  if (!marker(0xFF)) success = 0;
  for (offset = 0; success && offset < size; offset += count) {
    count = size - offset > 28 ? 28 : size - offset;
    if (!storage_macro_write(buffer, offset, count, data + offset)) success = 0;
  }
  committed = marker(0); /* The app sends this even after a failed chunk. */
  return success && committed;
}

static void prepare(void) {
  unsigned int i;
  memset(flash, 0xFF, sizeof(flash));
  for (i = 0; i < 32; ++i) flash[i] = (uint8_t)(i + 1); /* Existing keymap area. */
  no_faults();
  storage_macro_load(buffer);
  assert(!storage_macro_ready());
  assert(upload(old_data, sizeof(old_data)));
  no_faults();
}

static int matches(const uint8_t *data, size_t size) {
  unsigned int i;
  if (memcmp(buffer, data, size)) return 0;
  for (i = (unsigned int)size; i < sizeof(buffer); ++i) if (buffer[i]) return 0;
  return 1;
}

static void accepted_or_disabled(void) {
  storage_macro_load(buffer);
  if (storage_macro_ready()) {
    assert(pedal_macro_valid(buffer, sizeof(buffer)));
    assert(matches(old_data, sizeof(old_data)) || matches(new_data, sizeof(new_data)) ||
           matches((uint8_t[]){0}, 1));
  } else {
    assert(matches((uint8_t[]){0}, 1));
  }
}

static void reserved_unchanged(const uint8_t *saved) {
  unsigned int i;
  for (i = 0; i < sizeof(flash); ++i)
    if (i < 32 || (i & 1)) assert(flash[i] == saved[i]);
}

static void test_upload_reset_and_wear(void) {
  uint8_t saved[sizeof(flash)];
  prepare();
  memcpy(saved, flash, sizeof(saved));
  assert(upload(new_data, sizeof(new_data)));
  assert(storage_macro_ready() && matches(new_data, sizeof(new_data)));
  reserved_unchanged(saved);
  no_faults();
  storage_macro_load(buffer);
  assert(storage_macro_ready() && matches(new_data, sizeof(new_data)));
  no_faults();
  assert(storage_macro_write(buffer, sizeof(buffer), 0, 0) && writes == 0);
  assert(storage_macro_reset(buffer));
  no_faults();
  assert(storage_macro_reset(buffer) && writes == 0); /* Unchanged reset is free. */
  assert(marker(0xFF));
  no_faults();
  assert(storage_macro_write(buffer, 0, 4, (uint8_t[4]){0}) && writes == 0);
  assert(marker(0) && storage_macro_ready());
}

static void test_partial_unknown_and_failed_chunks(void) {
  prepare();
  assert(storage_macro_reset(buffer) && marker(0xFF));
  assert(storage_macro_write(buffer, 0, 28, new_data));
  assert(!marker(0)); /* Reset padding must not supply unreceived terminators. */
  accepted_or_disabled();
  assert(!storage_macro_ready());
  assert(storage_macro_reset(buffer) && marker(0xFF));
  assert(!storage_macro_write(buffer, 28, 1, new_data)); /* Hole/out of order. */
  assert(!marker(0));
  assert(storage_macro_reset(buffer) && marker(0xFF));
  assert(!storage_macro_write(buffer, 0, 29, new_data));
  assert(!marker(0));
  assert(storage_macro_reset(buffer) && marker(0xFF));
  assert(!storage_macro_write(buffer, UINT16_MAX, 1, new_data));
  assert(!marker(0));
  assert(storage_macro_reset(buffer) && marker(0xFF));
  assert(storage_macro_write(buffer, 0, 6, (uint8_t[]){1, 0xFF, 0, 0, 0, 0}));
  assert(!marker(0)); /* Structurally invalid action. */
  assert(storage_macro_reset(buffer) && marker(0xFF));
  assert(storage_macro_write(buffer, 0, sizeof(old_data), old_data));
  storage_macro_cancel(); /* USB reset/unknown-write rejection latches failure. */
  assert(!marker(0));
  no_faults();
  storage_macro_load(buffer); /* No live upload session survives a reboot. */
  assert(!storage_macro_ready() && !marker(0));
}

static void test_every_failed_write_and_power_cut(void) {
  static uint8_t saved[sizeof(flash)];
  static unsigned int total, step, failure, boundary;
  prepare();
  memcpy(saved, flash, sizeof(saved));
  assert(upload(new_data, sizeof(new_data)));
  total = writes;
  for (step = 1; step <= total; ++step) {
    for (failure = 0; failure < 3; ++failure) {
      memcpy(flash, saved, sizeof(flash));
      no_faults();
      storage_macro_load(buffer);
      no_faults();
      fail_write = step; mode = failure;
      assert(!upload(new_data, sizeof(new_data)));
      reserved_unchanged(saved);
      no_faults();
      accepted_or_disabled();
    }
  }
  for (boundary = 0; boundary <= total; ++boundary) {
    memcpy(flash, saved, sizeof(flash));
    no_faults();
    storage_macro_load(buffer);
    no_faults();
    cut_after = boundary;
    if (setjmp(power_loss) == 0) {
      upload(new_data, sizeof(new_data));
      assert(0 && "power cut was not reached");
    }
    no_faults();
    reserved_unchanged(saved);
    accepted_or_disabled();
    if (boundary == total) assert(storage_macro_ready() && matches(new_data, sizeof(new_data)));
  }
  printf("macro storage fault coverage: %u physical writes, %u power-cut boundaries\n", total, total + 1);
}

static void test_crc_and_readback(void) {
  uint8_t saved[sizeof(flash)];
  unsigned int byte, bit;
  prepare();
  memcpy(saved, flash, sizeof(saved));
  for (byte = STORAGE_FLASH_BYTES; byte < STORAGE_DATA_BYTES; ++byte) {
    for (bit = 0; bit < 8; ++bit) {
      memcpy(flash, saved, sizeof(flash));
      flash[2u * byte] ^= (uint8_t)(1u << bit);
      no_faults();
      storage_macro_load(buffer);
      assert(!storage_macro_ready() && matches((uint8_t[]){0}, 1));
    }
  }
  memcpy(flash, saved, sizeof(flash));
  no_faults();
  storage_macro_load(buffer);
  no_faults();
  fail_read = 2; /* Invalidation readback fault prohibits changing the payload. */
  assert(!storage_macro_reset(buffer));
  assert(!storage_macro_ready());
  no_faults();
  accepted_or_disabled();
}

int main(void) {
  test_upload_reset_and_wear();
  test_partial_unknown_and_failed_chunks();
  test_every_failed_write_and_power_cut();
  test_crc_and_readback();
  puts("macro storage tests: PASS (framing, readback/CRC, every failed/torn write and power cut, unchanged-byte wear, journal isolation)");
  return 0;
}
