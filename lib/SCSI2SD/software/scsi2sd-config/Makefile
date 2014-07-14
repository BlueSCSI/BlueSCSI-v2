
CPPFLAGS = -I ../bootloaderhost/hidapi/hidapi -I ../bootloaderhost
CFLAGS += -Wall -Wno-pointer-sign -O2
CXXFLAGS += -Wall -std=c++11 -O2
VPATH += ../bootloaderhost

TARGET ?= $(shell uname -s)
ifeq ($(TARGET),Win32)
	VPATH += hidapi/windows
	LDFLAGS += -mconsole -mwindows -lsetupapi -lws2_32
	BUILD = build/windows/32bit
	CC=i686-w64-mingw32-gcc
	CXX=i686-w64-mingw32-g++
	EXE=.exe
endif
ifeq ($(TARGET),Win64)
	VPATH += hidapi/windows
	LDFLAGS += -mconsole -mwindows -lsetupapi -lws2_32
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
	VPATH += ../bootloaderhost/hidapi/mac
	LDFLAGS += -framework IOKit -framework CoreFoundation
	CPPFLAGS += -isysroot /Xcode3.1.4/SDKs/MacOSX10.5.sdk
	CFLAGS += -mmacosx-version-min=10.5 -arch x86_64 -arch i386 -arch ppc
	CXXFLAGS += -mmacosx-version-min=10.5 -arch x86_64 -arch i386 -arch ppc
	CC=/Xcode3.1.4/usr/bin/gcc
	CXX=/Xcode3.1.4/usr/bin/g++
	BUILD=build/mac
endif

all:  $(BUILD)/scsi2sd-config$(EXE)

HIDAPI = \
	$(BUILD)/hid.o \

OBJ = \
	$(HIDAPI) \
	$(BUILD)/scsi2sd-config.o \
	$(BUILD)/SCSI2SD_HID.o \

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -c -o $@

$(BUILD)/%.o: %.cc
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -c -o $@

$(BUILD)/scsi2sd-config$(EXE): $(OBJ)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm $(BUILD)/scsi2sd-config$(EXE) $(OBJ)
