# Offline build: enter the project-local locked Nix shell first.
SDCC ?= sdcc
PACKIHX ?= packihx
PYTHON ?= python3
BUILD := build/ch552
.DELETE_ON_ERROR:
INCLUDES := -Ifirmware/core -Ifirmware/ch552 -Ithird_party/ch55xduino
TARGET_FLAGS := -mmcs51 --model-large --int-long-reent --nooverlay --opt-code-size \
	-DCH552 -DUSER_USB_RAM=266 --xram-loc 266 --xram-size 758 --code-size 0x3800
SOURCES := firmware/ch552/main.c firmware/ch552/usb_transport.c \
	firmware/ch552/storage.c firmware/ch552/storage_flash.c firmware/core/pedal_core.c \
	third_party/ch55xduino/USBhandler.c third_party/ch55xduino/USBconstant.c
OBJECTS := $(addprefix $(BUILD)/,$(SOURCES:.c=.rel))
HEADERS := $(wildcard firmware/core/*.h firmware/ch552/*.h \
	third_party/ch55xduino/*.h third_party/ch55xduino/include/*.h \
	third_party/ch55xduino/usbCommonDescriptors/*.h)

.PHONY: all firmware firmware-check test check clean
all: firmware

firmware: $(BUILD)/pedal.hex firmware-check

$(BUILD)/%.rel: %.c $(HEADERS) Makefile
	mkdir -p $(dir $@)
	$(SDCC) $(TARGET_FLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/pedal.ihx: $(OBJECTS)
	$(SDCC) $(TARGET_FLAGS) $(OBJECTS) -o $@

$(BUILD)/pedal.hex: $(BUILD)/pedal.ihx
	$(PACKIHX) $< > $@

firmware-check: $(BUILD)/pedal.hex
	$(PYTHON) scripts/check_firmware.py --build-dir $(BUILD)
	$(PYTHON) -m unittest discover -s tests -p test_check_firmware.py

test:
	$(MAKE) -C tests/core test
	$(MAKE) -C tests/board test

check: test firmware

clean:
	$(PYTHON) -c 'import shutil; shutil.rmtree("build/ch552", ignore_errors=True)'
	$(MAKE) -C tests/core clean
	$(MAKE) -C tests/board clean
