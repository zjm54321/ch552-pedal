# Offline build: enter the project-local locked Nix shell first.
SDCC ?= sdcc
PACKIHX ?= packihx
PYTHON ?= python3
VARIANT ?= test
VARIANTS := test is0
ifeq ($(filter $(VARIANT),$(VARIANTS)),)
$(error VARIANT must be test or is0)
endif
BUILD_ROOT := build/ch552
BUILD := $(BUILD_ROOT)/$(VARIANT)
HEX := $(BUILD_ROOT)/pedal-$(VARIANT).hex
USB_VID_test := 0x1209
USB_PID_test := 0x0001
USB_VID_is0 := 0xA103
USB_PID_is0 := 0x0012
DEFINITION_test := via/pedal.json
DEFINITION_is0 := via/is0.json
DEFINITION := $(DEFINITION_$(VARIANT))
.DELETE_ON_ERROR:
INCLUDES := -Ifirmware/core -Ifirmware/ch552 -Ithird_party/ch55xduino
TARGET_FLAGS := -mmcs51 --model-large --int-long-reent --nooverlay --opt-code-size \
	-DCH552 -DUSER_USB_RAM=266 -DUSB_VID=$(USB_VID_$(VARIANT)) -DUSB_PID=$(USB_PID_$(VARIANT)) \
	--xram-loc 266 --xram-size 758 --code-size 0x3800
SOURCES := firmware/ch552/main.c firmware/ch552/usb_transport.c \
	firmware/ch552/storage.c firmware/ch552/storage_flash.c firmware/ch552/macro_storage.c \
	firmware/ch552/usb_power.c firmware/core/pedal_core.c firmware/core/pedal_macro.c \
	third_party/ch55xduino/USBhandler.c third_party/ch55xduino/USBconstant.c
OBJECTS := $(addprefix $(BUILD)/,$(SOURCES:.c=.rel))
HEADERS := $(wildcard firmware/core/*.h firmware/ch552/*.h \
	third_party/ch55xduino/*.h third_party/ch55xduino/include/*.h \
	third_party/ch55xduino/usbCommonDescriptors/*.h)

.PHONY: all firmware firmware-both firmware-check test check clean
all: firmware

firmware: $(HEX) firmware-check

firmware-both:
	$(MAKE) firmware VARIANT=test
	$(MAKE) firmware VARIANT=is0

$(BUILD)/%.rel: %.c $(HEADERS) Makefile
	mkdir -p $(dir $@)
	$(SDCC) $(TARGET_FLAGS) $(INCLUDES) -c $< -o $@

$(BUILD)/pedal.ihx: $(OBJECTS)
	$(SDCC) $(TARGET_FLAGS) $(OBJECTS) -o $@

$(HEX): $(BUILD)/pedal.ihx
	$(PACKIHX) $< > $@

firmware-check: $(HEX)
	$(PYTHON) scripts/check_firmware.py --build-dir $(BUILD) --hex $(HEX) --definition $(DEFINITION)

test:
	$(MAKE) -C tests/core test
	$(MAKE) -C tests/board test

check: test firmware-both
	$(PYTHON) -m unittest discover -s tests -p test_check_firmware.py

clean:
	$(PYTHON) -c 'import shutil; shutil.rmtree("build/ch552", ignore_errors=True)'
	$(MAKE) -C tests/core clean
	$(MAKE) -C tests/board clean
