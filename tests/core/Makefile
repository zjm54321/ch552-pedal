CC ?= cc
CFLAGS ?= -O2
CFLAGS += -std=c99 -Wall -Wextra -Werror -pedantic
CPPFLAGS += -I../../firmware/core

.PHONY: test clean
test: build/test_core
	./build/test_core

build/test_core: test_core.c ../../firmware/core/pedal_core.c ../../firmware/core/pedal_core.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) ../../firmware/core/pedal_core.c test_core.c $(LDFLAGS) -o $@

clean:
	rm -f build/test_core
