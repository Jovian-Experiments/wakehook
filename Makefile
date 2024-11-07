all: wakehook

SDBUS_CFLAGS := $(shell pkg-config --cflags libsystemd)
SDBUS_LDFLAGS := $(shell pkg-config --libs libsystemd)

CFLAGS += -Wall -Wextra -Werror -Wno-format-truncation -Wno-stringop-overflow $(SDBUS_CFLAGS)

ifneq ($(ASAN),)
  CFLAGS += -g -fsanitize=address
  LDFLAGS += -g -fsanitize=address
else ifneq ($(DEBUG),)
  CFLAGS += -g
  LDFLAGS += -g
else
  CFLAGS += -O2
  LDFLAGS += -O2
endif

.PHONY: clean install

clean:
	rm -f wakehook.o wakehook

install: all LICENSE
	install -D -m 755 wakehook $(DESTDIR)/usr/lib/hwsupport/wakehook
	install -D -m 644 LICENSE $(DESTDIR)/usr/share/licenses/wakehook
	install -D -m 644 systemd.service $(DESTDIR)/usr/lib/systemd/user/wakehook.service
	mkdir -p $(DESTDIR)/usr/lib/systemd/user/gamescope-session.service.wants
	ln -s ../wakehook.service $(DESTDIR)/usr/lib/systemd/user/gamescope-session.service.wants/

wakehook: wakehook.o
	$(CC) $(SDBUS_LDFLAGS) $(LDFLAGS) -o $@ $^
