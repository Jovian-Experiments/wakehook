all: wakehook

SDBUS_CFLAGS := $(shell pkg-config --cflags libsystemd)
SDBUS_LDFLAGS := $(shell pkg-config --libs libsystemd)

CFLAGS += -Wall -Wextra -Werror -Wno-format-truncation -Wno-stringop-overflow $(SDBUS_CFLAGS)
LDFLAGS += $(SDBUS_LDFLAGS)

ifneq ($(ASAN),)
  CFLAGS += -g -fsanitize=address
  LDFLAGS += -g -fsanitize=address
else ifneq ($(DEBUG),)
  CFLAGS += -g
  LDFLAGS += -g
else
  CFLAGS += -O3 -D_FORTIFY_SOURCE=2
endif

.PHONY: clean install

clean:
	rm -f wakehook.o wakehook

install: all

wakehook: wakehook.c
	$(CC) $(LDFLAGS) -o $@ $^
