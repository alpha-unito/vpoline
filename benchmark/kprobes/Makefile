obj-m += example.o

RUNNING_KDIR := /lib/modules/$(shell uname -r)/build
MATCHING_KDIR := $(if $(wildcard $(RUNNING_KDIR)/Makefile),$(RUNNING_KDIR))
KDIR ?= $(or $(MATCHING_KDIR),$(lastword $(sort $(wildcard /usr/src/kernels/*))))
BUILD := $(CURDIR)/build

ifeq ($(KDIR),)
$(error No kernel build tree found; install kernel-devel)
endif

all: $(BUILD)
	$(MAKE) -C $(KDIR) M=$(CURDIR) MO=$(BUILD) modules
	$(CC) -Wall -Wextra -O2 test.c -o $(BUILD)/test

$(BUILD):
	mkdir -p $@

clean:
	$(RM) -r $(BUILD)
