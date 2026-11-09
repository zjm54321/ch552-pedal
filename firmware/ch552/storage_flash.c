/* SPDX-License-Identifier: GPL-3.0-or-later
 * CH552 DataFlash sequence adapted from CH55xduino cores/ch55xduino/eeprom.c,
 * created by Deqing Sun. See third_party/ch55xduino/PROVENANCE.md. */
#include "include/ch5xx.h"
#include "storage_backend.h"

uint8_t storage_read_byte(uint8_t address) {
  if (address >= STORAGE_DATA_BYTES) return 0xFF;
  ROM_ADDR_H = DATA_FLASH_ADDR >> 8;
  ROM_ADDR_L = address << 1;
  ROM_CTRL = ROM_CMD_READ;
  return ROM_DATA_L;
}

uint8_t storage_write_byte(uint8_t address, uint8_t value) {
  uint8_t ok;
  uint8_t saved_ea;
  if (address >= STORAGE_DATA_BYTES) return 0;
  saved_ea = EA;
  EA = 0;
  SAFE_MOD = 0x55;
  SAFE_MOD = 0xAA;
  GLOBAL_CFG |= bDATA_WE; /* Never enable bCODE_WE. */
  SAFE_MOD = 0;
  ROM_ADDR_H = DATA_FLASH_ADDR >> 8;
  ROM_ADDR_L = address << 1;
  ROM_DATA_L = value;
  ok = 0;
  if (ROM_STATUS & bROM_ADDR_OK) {
    ROM_CTRL = ROM_CMD_WRITE;
    ok = (ROM_STATUS & (bROM_ADDR_OK | bROM_CMD_ERR)) == bROM_ADDR_OK;
  }
  SAFE_MOD = 0x55;
  SAFE_MOD = 0xAA;
  GLOBAL_CFG &= ~bDATA_WE;
  SAFE_MOD = 0;
  if (ok) ok = storage_read_byte(address) == value;
  EA = saved_ea;
  return ok;
}
