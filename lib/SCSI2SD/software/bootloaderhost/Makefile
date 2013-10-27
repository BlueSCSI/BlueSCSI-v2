all:  bootloaderhost

CYAPI = \
	cybootloaderutils/cybtldr_api2.c \
	cybootloaderutils/cybtldr_api.c \
	cybootloaderutils/cybtldr_command.c \
	cybootloaderutils/cybtldr_parse.c \


UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	HID_C = hidapi/linux/hid.c
	LDFLAGS += -ludev
endif
ifeq ($(UNAME_S),Darwin)
	# Should match OSX
	HID_C = hidapi/mac/hid.c
endif


bootloaderhost: main.c $(HID_C) $(CYAPI)
	$(CC) $(CFLAGS) -g -I cybootloaderutils -I hidapi/hidapi $^ $(LDFLAGS) -o $@

clean:
	rm bootloaderhost
