# SPDX-License-Identifier: GPL-2.0
# Makefile for zpoline hooks
include ../scripts/Makefile.include
include ../scripts/Makefile.arch

libdir ?= /usr/lib/zpoline

ifeq ($(srctree),)
srctree := $(patsubst %/,%,$(dir $(CURDIR)))
srctree := $(patsubst %/,%,$(dir $(srctree)))
endif

# Do not use make's built-in rules
# (this improves performance and avoids hard-to-debug behaviour);
MAKEFLAGS += -r


override CFLAGS += -O3 -Wall -Wextra -g -D_GNU_SOURCE -shared -fPIC -ftls-model=local-dynamic
override AFLAGS += -O3 -Wall -Wextra -g -D_GNU_SOURCE -shared -fPIC -ftls-model=local-dynamic
override LDFLAGS += -lpthread

ALL_TARGETS := libritsu.so
ALL_SOS := $(patsubst %,$(OUTPUT)%,$(ALL_TARGETS))

all: $(ALL_SOS)

export srctree OUTPUT CC LD CFLAGS AFLAGS

include $(srctree)/tools/build/Makefile.include

LIBRITSU_IN := $(OUTPUT)libritsu-in.o
$(LIBRITSU_IN): FORCE
	$(Q)$(MAKE) $(build)=libritsu

$(OUTPUT)libritsu.so: $(LIBRITSU_IN)
	$(QUIET_LINK)$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(ALL_SOS)
	find $(or $(OUTPUT),.) -name '*.o' -delete -o -name '\.*.d' -delete -o -name '\.*.o.cmd' -delete

install: $(ALL_SOS)
	install -d -m 755 $(DESTDIR)$(libdir);		\
	for program in $(ALL_SOS); do		\
		install $$program $(DESTDIR)$(libdir);	\
	done

FORCE:

.PHONY: all install clean FORCE prepare
