
CPPFLAGS = -I ../bootloaderhost/hidapi/hidapi -I ../bootloaderhost
CFLAGS += -Wall -Wno-pointer-sign -O2
CXXFLAGS += -Wall -O2
VPATH += ../bootloaderhost

TARGET ?= $(shell uname -s)
ifeq ($(TARGET),Win32)
	VPATH += hidapi/windows
	LDFLAGS += -static -mconsole -mwindows -lsetupapi -lws2_32
	BUILD = build/windows/32bit
	CC=i686-w64-mingw32-gcc
	CXX=i686-w64-mingw32-g++
	EXE=.exe
endif
ifeq ($(TARGET),Win64)
	VPATH += hidapi/windows
	LDFLAGS += -static -mconsole -mwindows -lsetupapi -lws2_32
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
	CFLAGS += -mmacosx-version-min=10.7
	CXXFLAGS += -stdlib=libc++ -mmacosx-version-min=10.7
	CC=clang
	CXX=clang++
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
