VPATH=cybootloaderutils

CPPFLAGS = -I cybootloaderutils -I hidapi/hidapi
CFLAGS += -Wall -Wno-pointer-sign -O2
CXXFLAGS += -Wall -std=c++11 -O2

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	VPATH += hidapi/linux
	LDFLAGS += -ludev
	BUILD=build/linux
endif
ifeq ($(UNAME_S),Darwin)
	# Should match OSX
	VPATH += hidapi/mac
	LDFLAGS += -framework IOKit -framework CoreFoundation
	CPPFLAGS += -isysroot /Xcode3.1.4/SDKs/MacOSX10.5.sdk
	CFLAGS += -mmacosx-version-min=10.5 -arch x86_64 -arch i386 -arch ppc
	CXXFLAGS += -mmacosx-version-min=10.5 -arch x86_64 -arch i386 -arch ppc
	CC=/Xcode3.1.4/usr/bin/gcc
	CXX=/Xcode3.1.4/usr/bin/g++
	BUILD=build/mac
endif

all:  $(BUILD)/bootloaderhost

CYAPI = \
	$(BUILD)/cybtldr_api2.o \
	$(BUILD)/cybtldr_api.o \
	$(BUILD)/cybtldr_command.o \
	$(BUILD)/cybtldr_parse.o \


HIDAPI = \
	$(BUILD)/hid.o \


OBJ = \
	$(CYAPI) $(HIDAPI) \
	$(BUILD)/main.o \
	$(BUILD)/Firmware.o \
	$(BUILD)/SCSI2SD_Bootloader.o \
	$(BUILD)/SCSI2SD_HID.o \

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -c -o $@

$(BUILD)/%.o: %.cc
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -c -o $@

$(BUILD)/bootloaderhost: $(OBJ)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm $(BUILD)/bootloaderhost $(OBJ)

