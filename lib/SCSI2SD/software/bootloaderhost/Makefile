all:  build/bootloaderhost

CYAPI = \
	cybootloaderutils/cybtldr_api2.c \
	cybootloaderutils/cybtldr_api.c \
	cybootloaderutils/cybtldr_command.c \
	cybootloaderutils/cybtldr_parse.c \

CFLAGS += -Wall -Wno-pointer-sign

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	HID_C = hidapi/linux/hid.c
	LDFLAGS += -ludev
endif
ifeq ($(UNAME_S),Darwin)
	# Should match OSX
	HID_C = hidapi/mac/hid.c
	LDFLAGS += -framework IOKit -framework CoreFoundation
	CFLAGS += -mmacosx-version-min=10.5 -arch x86_64 -arch i386 -arch ppc -isysroot /Xcode3.1.4/SDKs/MacOSX10.5.sdk
	CC=/Xcode3.1.4/usr/bin/gcc
endif


build/bootloaderhost: main.c $(HID_C) $(CYAPI)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I cybootloaderutils -I hidapi/hidapi $^ $(LDFLAGS) -o $@

clean:
	rm build/bootloaderhost
