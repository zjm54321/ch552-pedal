CC ?= cc
CFLAGS ?= -O2
CFLAGS += -std=c99 -Wall -Wextra -Werror -pedantic
CPPFLAGS += -I../../firmware/core

.PHONY: test clean
test: build/test_core build/test_macro
	./build/test_core
	./build/test_macro

build/test_core: test_core.c ../../firmware/core/pedal_core.c ../../firmware/core/pedal_macro.c \
	../../firmware/core/pedal_core.h ../../firmware/core/pedal_macro.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) ../../firmware/core/pedal_core.c ../../firmware/core/pedal_macro.c test_core.c $(LDFLAGS) -o $@

build/test_macro: test_macro.c ../../firmware/core/pedal_core.c ../../firmware/core/pedal_macro.c \
	../../firmware/core/pedal_core.h ../../firmware/core/pedal_macro.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) ../../firmware/core/pedal_core.c ../../firmware/core/pedal_macro.c test_macro.c $(LDFLAGS) -o $@

clean:
	rm -f build/test_core build/test_macro
