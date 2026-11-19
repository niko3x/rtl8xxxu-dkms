# SPDX-License-Identifier: GPL-2.0-only
#
# Out-of-tree build of rtl8xxxu with the rtl8xxxu-dkms patches.

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

KBUILD_ARGS := -C $(KDIR) M=$(CURDIR)/src \
	CONFIG_RTL8XXXU=m CONFIG_RTL8XXXU_UNTESTED=y

all: modules

modules:
	$(MAKE) $(KBUILD_ARGS) modules

install: modules
	$(MAKE) $(KBUILD_ARGS) INSTALL_MOD_DIR=updates modules_install
	depmod -a $(KVER)

clean:
	$(MAKE) $(KBUILD_ARGS) clean

tools:
	$(MAKE) -C tools

.PHONY: all modules install clean tools
