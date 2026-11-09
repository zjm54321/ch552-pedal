/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pedal_core.h"
#include "storage.h"
#include "storage_backend.h"

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static uint8_t flash[128];
static unsigned int writes;
static unsigned int reads;
static unsigned int fail_write;
static unsigned int fail_read;
static unsigned int failure_mode;
static unsigned int cut_after;
static jmp_buf power_loss;

uint8_t storage_read_byte(uint8_t address) {
  unsigned int physical = (unsigned int)address * 2;
  assert(address < STORAGE_FLASH_BYTES && physical < sizeof(flash));
  assert((physical & 1) == 0);
  ++reads;
  return flash[physical] ^ (reads == fail_read ? 1 : 0);
}

uint8_t storage_write_byte(uint8_t address, uint8_t value) {
  unsigned int physical = (unsigned int)address * 2;
  assert(address < STORAGE_FLASH_BYTES && physical < sizeof(flash));
  assert((physical & 1) == 0);
  if (cut_after == 0) longjmp(power_loss, 1);
  ++writes;
  if (writes == fail_write) {
    if (failure_mode == 1) flash[physical] = value; /* Written but status failed. */
    if (failure_mode == 2) flash[physical] = 0x5A; /* Torn byte. */
    return 0;
  }
  flash[physical] = value;
  if (writes == cut_after) longjmp(power_loss, 1);
  return 1;
}

static void no_faults(void) {
  writes = reads = 0;
  fail_write = fail_read = 0;
  failure_mode = 0;
  cut_after = 100;
}

static void load(pedal_core_t *core) {
  uint8_t record[PEDAL_STORAGE_SIZE];
  storage_load(record);
  pedal_core_init(core, record, sizeof(record));
}

/* Same transactional ownership/dirty policy as the board main loop. */
static uint8_t remap(pedal_core_t *core, uint16_t keycode) {
  uint8_t report[PEDAL_RAW_REPORT_SIZE] = {0x05};
  uint8_t record[PEDAL_STORAGE_SIZE];
  pedal_core_t proposed = *core;
  uint8_t flags;
  report[4] = (uint8_t)(keycode >> 8);
  report[5] = (uint8_t)keycode;
  flags = pedal_core_via(&proposed, report, sizeof(report), 0);
  if (flags & PEDAL_VIA_SAVE) {
    pedal_core_record(&proposed, record);
    if (!storage_save(record)) {
      core->persistence_dirty = 1;
      return 0;
    }
    proposed.persistence_dirty = 0;
  }
  if (flags & PEDAL_VIA_HANDLED) *core = proposed;
  return flags;
}

static void prepare(unsigned int state, pedal_core_t *core) {
  uint8_t record[PEDAL_STORAGE_SIZE];
  unsigned int i;
  memset(flash, 0xFF, sizeof(flash));
  no_faults();
  load(core);
  assert(core->persistence_dirty);
  if (state == 0) { /* Legacy mapping at zero. */
    core->keycode = 0x0106;
    pedal_core_record(core, record);
    for (i = 0; i < sizeof(record); ++i) flash[2 * i] = record[i];
    load(core);
  } else {
    assert(remap(core, 0x0106) & PEDAL_VIA_SAVE); /* First slot, gen 1. */
    if (state == 2) {
      assert(remap(core, 0x030E) & PEDAL_VIA_SAVE); /* Both slots, gen 2. */
    }
  }
  assert(!core->persistence_dirty);
  no_faults();
}

static void assert_reserved_unchanged(const uint8_t *before) {
  unsigned int i;
  for (i = 0; i < sizeof(flash); ++i)
    if ((i & 1) || i >= STORAGE_FLASH_BYTES * 2) assert(flash[i] == before[i]);
}

static void test_failed_writes_and_retry(void) {
  unsigned int state, step, mode;
  pedal_core_t core, rebooted;
  uint8_t before[sizeof(flash)];
  uint8_t held[PEDAL_KEYBOARD_REPORT_SIZE];
  uint16_t retained;
  for (state = 0; state < 3; ++state) {
    for (step = 1; step <= 9; ++step) {
      for (mode = 0; mode < 3; ++mode) {
        prepare(state, &core);
        retained = core.keycode;
        assert(pedal_core_update(&core, 1, held));
        memcpy(before, flash, sizeof(flash));
        fail_write = step;
        failure_mode = mode;
        assert(remap(&core, 0x0052) == 0);
        assert(core.keycode == retained && core.persistence_dirty);
        assert(memcmp(core.keyboard_report, held, sizeof(held)) == 0);
        assert_reserved_unchanged(before);
        /* The previously active record survives even a failed commit. */
        assert(memcmp(flash + (state == 2 ? 16 : 0),
                      before + (state == 2 ? 16 : 0), 16) == 0);
        /* Re-saving the retained mapping MUST request and perform a save. */
        no_faults();
        assert(remap(&core, retained) == (PEDAL_VIA_HANDLED | PEDAL_VIA_SAVE));
        assert(writes == 9 && !core.persistence_dirty);
        no_faults();
        load(&rebooted);
        assert(rebooted.keycode == retained && !rebooted.persistence_dirty);
        no_faults();
        assert(remap(&core, retained) == PEDAL_VIA_HANDLED && writes == 0);
      }
    }
  }
}

static void test_readback_faults(void) {
  unsigned int step;
  pedal_core_t core, rebooted;
  uint16_t retained;
  for (step = 1; step <= 16; ++step) {
    prepare(2, &core);
    retained = core.keycode;
    fail_read = step;
    assert(remap(&core, 0x0052) == 0);
    assert(core.keycode == retained && core.persistence_dirty);
    no_faults();
    /* Before publication the old key wins; after publication the fully
     * verified new key may win even if its final readback reported failure. */
    load(&rebooted);
    assert(rebooted.keycode == (step <= 8 ? retained : 0x0052));
    /* Re-establish runtime ownership, which a reboot above would otherwise
     * change, and retry the retained key as the running board does. */
    no_faults();
    assert(remap(&core, retained) & PEDAL_VIA_SAVE);
    load(&rebooted);
    assert(rebooted.keycode == retained);
  }
}

static void test_power_loss_boundaries(void) {
  /* Static loop state is defined across longjmp; automatic changed locals
   * are not inspected after a simulated reset. */
  static unsigned int state, boundary;
  static uint16_t retained;
  pedal_core_t core, rebooted;
  for (state = 0; state < 3; ++state) {
    for (boundary = 0; boundary <= 9; ++boundary) {
      prepare(state, &core);
      retained = core.keycode;
      cut_after = boundary;
      if (setjmp(power_loss) == 0) {
        remap(&core, 0x0052);
        assert(0 && "power cut was not reached");
      }
      no_faults();
      load(&rebooted);
      assert(rebooted.keycode == (boundary == 9 ? 0x0052 : retained));
      assert(!rebooted.persistence_dirty);
    }
  }
}

static void test_blank_legacy_corruption_and_wrap(void) {
  pedal_core_t core, rebooted;
  uint8_t saved[sizeof(flash)];
  uint8_t slot[PEDAL_STORAGE_SIZE];
  unsigned int byte, bit, i;
  uint16_t crc;
  memset(flash, 0xFF, sizeof(flash));
  no_faults();
  load(&core);
  assert(core.keycode == PEDAL_DEFAULT_KEYCODE && core.persistence_dirty);
  fail_write = 5;
  assert(!remap(&core, 0x0052));
  no_faults();
  assert(remap(&core, PEDAL_DEFAULT_KEYCODE) & PEDAL_VIA_SAVE);
  load(&rebooted);
  assert(rebooted.keycode == PEDAL_DEFAULT_KEYCODE && !rebooted.persistence_dirty);

  prepare(0, &core);
  load(&rebooted);
  assert(rebooted.keycode == 0x0106 && !rebooted.persistence_dirty);
  assert(remap(&core, 0x030E) & PEDAL_VIA_SAVE);
  assert(flash[0] == 0x50); /* Migration preserved the legacy slot. */

  prepare(2, &core);
  memcpy(saved, flash, sizeof(flash));
  for (byte = 0; byte < PEDAL_STORAGE_SIZE; ++byte) {
    for (bit = 0; bit < 8; ++bit) {
      memcpy(flash, saved, sizeof(flash));
      flash[16 + 2 * byte] ^= (uint8_t)(1u << bit);
      load(&rebooted);
      assert(rebooted.keycode == 0x0106); /* Fall back to older independent slot. */
    }
  }
  /* A CRC-valid but unsupported mapping is still rejected. */
  memcpy(flash, saved, sizeof(flash));
  for (i = 0; i < sizeof(slot); ++i) slot[i] = flash[16 + 2 * i];
  slot[4] = 0x7E;
  slot[5] = 0;
  crc = pedal_record_crc(slot);
  slot[6] = (uint8_t)(crc >> 8);
  slot[7] = (uint8_t)crc;
  for (i = 0; i < sizeof(slot); ++i) flash[16 + 2 * i] = slot[i];
  load(&rebooted);
  assert(rebooted.keycode == 0x0106);

  /* Seed a valid generation 65535, then save generation zero and reload. */
  memcpy(flash, saved, sizeof(flash));
  for (i = 0; i < sizeof(slot); ++i) slot[i] = flash[16 + 2 * i];
  slot[2] = slot[3] = 0xFF;
  crc = pedal_record_crc(slot);
  slot[6] = (uint8_t)(crc >> 8);
  slot[7] = (uint8_t)crc;
  flash[0] = 0; /* No other valid generation until wrap is saved. */
  for (i = 0; i < sizeof(slot); ++i) flash[16 + 2 * i] = slot[i];
  load(&core);
  assert(core.keycode == 0x030E);
  assert(remap(&core, 0x0052) & PEDAL_VIA_SAVE);
  load(&rebooted);
  assert(rebooted.keycode == 0x0052 && flash[4] == 0 && flash[6] == 0);
}

int main(void) {
  test_failed_writes_and_retry();
  test_readback_faults();
  test_power_loss_boundaries();
  test_blank_legacy_corruption_and_wrap();
  puts("storage tests: PASS (journal, every write/readback fault and power-cut boundary, dirty retry, v1, CRC, wrap)");
  return 0;
}
