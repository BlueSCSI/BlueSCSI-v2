all: build/scsi2sd-config

CFLAGS += -Wall

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	HID_C = ../bootloaderhost/hidapi/linux/hid.c
	LDFLAGS += -ludev
endif
ifeq ($(UNAME_S),Darwin)
	# Should match OSX
	HID_C = ../bootloaderhost/hidapi/mac/hid.c
	LDFLAGS += -framework IOKit -framework CoreFoundation
endif


build/scsi2sd-config: main.c $(HID_C) $(CYAPI)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I ../bootloaderhost/hidapi/hidapi $^ $(LDFLAGS) -o $@

clean:
	rm build/scsi2sd-config
