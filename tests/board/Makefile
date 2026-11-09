CC ?= cc
CXX ?= c++
CFLAGS ?= -O2
CPPFLAGS += -I../../firmware/core -I../../firmware/ch552 -I../../third_party/ch55xduino

.PHONY: test clean
test: build/test_storage build/test_usb
	./build/test_storage
	./build/test_usb

build/test_storage: test_storage.c ../../firmware/ch552/storage.c ../../firmware/core/pedal_core.c \
	../../firmware/ch552/storage.h ../../firmware/ch552/storage_backend.h ../../firmware/core/pedal_core.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c99 -Wall -Wextra -Werror -pedantic \
		../../firmware/core/pedal_core.c ../../firmware/ch552/storage.c test_storage.c -o $@

# C++ proxies model write-one-to-clear SFRs while compiling the actual C USB
# handler/transport, not a second implementation of their state machines.
build/test_usb: test_usb.cpp sfr_stub.h ../../third_party/ch55xduino/USBhandler.c \
	../../firmware/ch552/usb_transport.c ../../firmware/ch552/usb_transport.h
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CFLAGS) -std=c++17 -Wall -Wextra -Wno-unknown-pragmas \
		-Wno-parentheses -fpermissive -DCH552 -DUSER_USB_RAM=266 \
		-include sfr_stub.h -x c++ ../../third_party/ch55xduino/USBhandler.c \
		../../firmware/ch552/usb_transport.c test_usb.cpp -o $@

clean:
	rm -f build/test_storage build/test_usb
