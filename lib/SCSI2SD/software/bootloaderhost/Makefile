VPATH=cybootloaderutils

CPPFLAGS = -I cybootloaderutils -I hidapi/hidapi
CFLAGS += -Wall -Wno-pointer-sign -O2
CXXFLAGS += -Wall -O2

TARGET ?= $(shell uname -s)
ifeq ($(TARGET),Win32)
	VPATH += hidapi/windows
	LDFLAGS += -static -mconsole -mwindows -lsetupapi
	BUILD = build/windows/32bit
	CC=i686-w64-mingw32-gcc
	CXX=i686-w64-mingw32-g++
	EXE=.exe
endif
ifeq ($(TARGET),Win64)
	VPATH += hidapi/windows
	LDFLAGS += -static -mconsole -mwindows -lsetupapi
	BUILD = build/windows/64bit
	CC=x86_64-w64-mingw32-gcc
	CXX=x86_64-w64-mingw32-g++
	EXE=.exe
endif
ifeq ($(TARGET),Linux)
	VPATH += hidapi/linux
	LDFLAGS += -ludev
	BUILD = build/linux
endif
ifeq ($(TARGET),Darwin)
	# Should match OSX
	VPATH += hidapi-mac
	LDFLAGS += -framework IOKit -framework CoreFoundation
	CFLAGS += -mmacosx-version-min=10.7
	CXXFLAGS += -stdlib=libc++ -mmacosx-version-min=10.7 -std=c++0x
	CC=clang
	CXX=clang++
	BUILD=build/mac
endif

all:  $(BUILD)/bootloaderhost$(EXE)

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

$(BUILD)/bootloaderhost$(EXE): $(OBJ)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm $(BUILD)/bootloaderhost$(EXE) $(OBJ)

