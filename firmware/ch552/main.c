/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "board_config.h"
#include "USBhandler.h"
#include "pedal_core.h"
#include "macro_storage.h"
#include "storage.h"
#include "usb_power.h"
#include "usb_transport.h"

static __xdata pedal_core_t core;
static __xdata pedal_core_t proposed;
static __xdata uint8_t record[PEDAL_STORAGE_SIZE];
static __xdata uint8_t via_report[PEDAL_RAW_REPORT_SIZE];
static __xdata uint8_t edge_report[PEDAL_KEYBOARD_REPORT_SIZE];
static __xdata uint8_t macro_buffer[PEDAL_MACRO_BUFFER_SIZE];
static __xdata pedal_macro_player_t player;
static __xdata uint8_t queue[PEDAL_QUEUE_SIZE][PEDAL_KEYBOARD_REPORT_SIZE];
static __xdata uint8_t queue_head;
static __xdata uint8_t queue_count;
static volatile __xdata uint32_t tick_ms;

/* Explicit declarations in this translation unit make SDCC emit both vectors.
 * All files use --nooverlay: ISR callback locals never overlay main/core locals.
 * Equal-priority interrupts do not nest; neither ISR calls the core or storage. */
void device_usb_interrupt(void) __interrupt(INT_NO_USB) {
  USBInterrupt();
}

void timer2_interrupt(void) __interrupt(INT_NO_TMR2) {
  TF2 = 0;
  ++tick_ms;
}

static void clock_and_input_init(void) {
  EA = 0;
  USB_CTRL = 0; /* Never attach normal HID before the power-up decision. */
  SAFE_MOD = 0x55;
  SAFE_MOD = 0xAA;
  CLOCK_CFG = (CLOCK_CFG & ~MASK_SYS_CK_SEL) | bOSC_EN_INT | 0x05;
  SAFE_MOD = 0;
  P3_3 = 1;
  P3_MOD_OC |= 0x08;
  P3_DIR_PU |= 0x08; /* Quasi-bidirectional input with internal pull-up. */
  /* 16 MHz / 16000 = 1 kHz, Timer2 auto-reload at full Fsys. */
  T2CON = 0;
  T2MOD = bTMR_CLK | bT2_CLK;
  RCAP2 = 0xC180;
  T2COUNT = 0xC180;
  TR2 = 1;
}

static void delay_polled_ms(uint8_t milliseconds) {
  while (milliseconds--) {
    while (!TF2) {}
    TF2 = 0;
  }
}

static void enter_isp(void) {
  /* Same USB-off / interrupts-off / TMOD=0 / 100 ms / 0x3800 sequence as
   * upstream USBCDC.c, with our additional Timer2 interrupt shut down. */
  EA = 0;
  IE_USB = 0;
  USB_INT_EN = 0;
  USB_CTRL = 0;
  UDEV_CTRL = 0;
  ET2 = 0;
  TR0 = 0;
  TR1 = 0;
  TMOD = 0;
  TF2 = 0;
  delay_polled_ms(100);
  T2CON = 0;
  T2MOD = 0;
  __asm
    lcall #0x3800
  __endasm;
  while (1) {}
}

static void boot_on_held(void) {
  uint8_t samples;
  uint8_t held = 0;
  /* Allow the established 0.1 uF input RC to charge before sampling. Then
   * require 20 consecutive active-low 1 ms samples in a finite 40 ms window.
   * This path reads no DataFlash, keymap, CRC, or USB descriptor state. */
  delay_polled_ms(100);
  for (samples = 0; samples < 40; ++samples) {
    delay_polled_ms(1);
    if (!P3_3) {
      if (++held == 20) enter_isp();
    } else {
      held = 0;
    }
  }
}

static uint32_t uptime_ms(void) {
  uint32_t result;
  EA = 0;
  result = tick_ms;
  EA = 1;
  return result;
}

static void enqueue(const __xdata uint8_t *report) {
  uint8_t i;
  uint8_t slot = (queue_head + queue_count) & (PEDAL_QUEUE_SIZE - 1);
  for (i = 0; i < PEDAL_KEYBOARD_REPORT_SIZE; ++i) queue[slot][i] = report[i];
  ++queue_count;
}

static void enqueue_release(void) {
  uint8_t i;
  pedal_macro_abort(&player, edge_report);
  pedal_core_update(&core, 0, edge_report);
  for (i = 0; i < PEDAL_KEYBOARD_REPORT_SIZE; ++i)
    edge_report[i] = core.keyboard_report[i] = 0;
  enqueue(edge_report);
}

static uint8_t mapped_macro(void) {
  return core.keycode >= PEDAL_MACRO_KEYCODE &&
         core.keycode < PEDAL_MACRO_KEYCODE + PEDAL_MACRO_COUNT;
}

void main(void) {
  uint8_t epoch;
  uint8_t input_epoch;
  uint8_t candidate;
  uint8_t stable;
  uint8_t debounce = 0;
  uint8_t sampled;
  uint8_t flags;
  uint8_t reply_pending = 0;
  uint8_t boot_pending = 0;
  uint8_t macro_running = 0;
  uint8_t resync = 0;
  uint8_t command;
  uint8_t i;
  uint32_t now;
  uint32_t last_scan = 0;
  uint32_t last_sent = 0;
  uint32_t boot_started = 0;

  clock_and_input_init();
  boot_on_held(); /* MUST stay before storage_load and pedal_core_init. */
  storage_load(record);
  pedal_core_init(&core, record, sizeof(record));
  storage_macro_load(macro_buffer);
  USBIdentityInit();
  candidate = !P3_3;
  stable = 0; /* Runtime press still requires the full debounce interval. */
  tick_ms = 0;
  TF2 = 0;
  ET2 = 1;
  USBInit();
  epoch = UsbEpoch;
  input_epoch = UsbInputEpoch;
  enqueue_release();

  while (1) {
    now = uptime_ms();
    if (epoch != UsbEpoch) {
      /* Bus reset, unconfigure and reconfigure invalidate transport work, not
       * the saved mapping. A fresh host first sees zero then current state. */
      epoch = UsbEpoch;
      input_epoch = UsbInputEpoch;
      queue_head = queue_count = 0;
      reply_pending = 0;
      macro_running = 0;
      storage_macro_cancel();
      enqueue_release();
      candidate = !P3_3;
      debounce = 0;
      resync = 1;
    }

    if (input_epoch != UsbInputEpoch) {
      input_epoch = UsbInputEpoch;
      queue_head = queue_count = 0;
      if (macro_running) {
        /* A suspended bus cannot accept the release. Latch zero locally and
         * send it on resume; never restart a macro from a suspended press. */
        macro_running = 0;
        enqueue_release();
        pedal_core_inhibit(&core, stable);
      }
      candidate = !P3_3;
      debounce = 0;
      resync = 1;
    }

    if (resync && !UsbSuspended && !boot_pending &&
        debounce == PEDAL_DEBOUNCE_MS && queue_count < PEDAL_QUEUE_SIZE) {
      /* Timer2 did not count sleep. Use eight fresh awake samples, not the
       * stale pre-PD state. A normal held chord remains latched until this
       * reconciliation; macros require a new release/press after resume. */
      if (mapped_macro()) pedal_core_inhibit(&core, stable);
      else pedal_core_update(&core, stable, edge_report);
      enqueue(core.keyboard_report);
      resync = 0;
    }

    if (now != last_scan) {
      last_scan = now;
      sampled = !P3_3;
      if (sampled != candidate) {
        candidate = sampled;
        debounce = 0;
      } else if (debounce < PEDAL_DEBOUNCE_MS) {
        if (++debounce == PEDAL_DEBOUNCE_MS) stable = candidate;
      }
    }

    /* A stalled host cannot overwrite any queued edge or the DMA report.
     * At saturation, defer accepting further edges until space exists; the
     * latest stable physical state is then reconciled, including its release.
     * Unbounded taps during an unbounded host stall cannot all be retained. */
    if (macro_running) {
      pedal_core_inhibit(&core, stable); /* Release/repress never aborts/restarts. */
    } else if (!UsbSuspended && !boot_pending && !resync &&
               queue_count < PEDAL_QUEUE_SIZE &&
               pedal_core_update(&core, stable, edge_report)) {
      enqueue(edge_report);
      if (core.down && mapped_macro() && storage_macro_ready())
        macro_running = pedal_macro_start(&player, macro_buffer,
                           (uint8_t)(core.keycode - PEDAL_MACRO_KEYCODE), input_epoch);
    }

    /* One macro report at a time: pairing and delays are tied to completed
     * transport reports, never merely to enqueuing ahead of a stalled host. */
    if (macro_running && !UsbSuspended && !boot_pending &&
        !queue_count && !UsbKeyboardBusy) {
      if (!player.active) macro_running = 0;
      else if (pedal_macro_step(&player, macro_buffer, now, input_epoch, edge_report)) {
        for (i = 0; i < PEDAL_KEYBOARD_REPORT_SIZE; ++i)
          core.keyboard_report[i] = edge_report[i];
        enqueue(edge_report);
      }
    }

    if (queue_count && USBKeyboardSend(queue[queue_head], epoch, input_epoch)) {
      queue_head = (queue_head + 1) & (PEDAL_QUEUE_SIZE - 1);
      --queue_count;
      last_sent = now;
    } else if (!queue_count && UsbIdle &&
               (uint32_t)(now - last_sent) >= (uint16_t)UsbIdle * 4u &&
               USBKeyboardSend(core.keyboard_report, epoch, input_epoch)) {
      last_sent = now;
    }

    if (!reply_pending && !boot_pending && USBRawTake(via_report, epoch)) {
      command = via_report[0];
      proposed = core;
      flags = pedal_core_via(&proposed, via_report, sizeof(via_report), now);
      if (command == 0x0F && !(flags & PEDAL_VIA_MACRO_WRITE))
        storage_macro_cancel();
      if (flags & PEDAL_VIA_MACRO_READ)
        pedal_macro_read(macro_buffer,
          ((uint16_t)via_report[1] << 8) | via_report[2], via_report[3], via_report + 4);
      if (flags & (PEDAL_VIA_MACRO_WRITE | PEDAL_VIA_MACRO_RESET)) {
        if (macro_running && ((flags & PEDAL_VIA_MACRO_RESET) || via_report[3])) {
          macro_running = 0;
          queue_head = queue_count = 0;
          enqueue_release();
          pedal_core_inhibit(&core, stable);
          proposed = core;
        }
        if (flags & PEDAL_VIA_MACRO_RESET)
          i = storage_macro_reset(macro_buffer);
        else
          i = storage_macro_write(macro_buffer,
            ((uint16_t)via_report[1] << 8) | via_report[2], via_report[3], via_report + 4);
        if (!i) {
          via_report[0] = 0xFF;
          flags = 0;
        }
      }
      if (flags & PEDAL_VIA_SAVE) {
        pedal_core_record(&proposed, record);
        if (!storage_save(record)) {
          /* Do not echo a successful remap if durable readback failed. The
           * current RAM mapping and exact held report remain unchanged. */
          via_report[0] = 0xFF;
          core.persistence_dirty = 1;
          flags = 0;
        } else {
          proposed.persistence_dirty = 0;
        }
      }
      if (flags & PEDAL_VIA_HANDLED) core = proposed;
      reply_pending = 1;
      if (flags & PEDAL_VIA_BOOTLOADER) {
        boot_pending = 1;
        macro_running = 0;
        boot_started = now;
        queue_head = queue_count = 0;
        enqueue_release();
      }
    }
    if (reply_pending && USBRawReply(via_report, epoch)) reply_pending = 0;

    if (boot_pending &&
        ((!reply_pending && !UsbRawBusy && !UsbKeyboardBusy && !queue_count) ||
         (uint32_t)(now - boot_started) >= 100u)) enter_isp();

    /* No boot check or storage call can reach this main-only sleep point. Raw
     * mailbox/IN ownership and reply/ISP work prohibit power-down. */
    if (USBPowerDown(reply_pending || boot_pending || macro_running)) {
      candidate = !P3_3;
      debounce = 0;
      last_scan = uptime_ms();
      resync = 1;
    }
  }
}
