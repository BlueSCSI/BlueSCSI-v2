VPATH=cybootloaderutils ../SCSI2SD/src

CPPFLAGS = -I cybootloaderutils -I hidapi/hidapi -I wxWidgets/include -I ../include
CFLAGS += -Wall -Wno-pointer-sign -O2 -g
CXXFLAGS += -Wall -O2 -g -std=c++0x

TARGET ?= $(shell uname -s)
ifeq ($(TARGET),Win32)
	VPATH += hidapi/windows
	LDFLAGS += -static -mconsole -mwindows -lsetupapi
	BUILD = build/windows/32bit
	CC=i686-w64-mingw32-gcc
	CXX=i686-w64-mingw32-g++
	EXE=.exe
	WX_CONFIG=--host=i686-w64-mingw32 --enable-monolithic --enable-stl --disable-shared
endif
ifeq ($(TARGET),Win64)
	VPATH += hidapi/windows
	LDFLAGS += -static -mconsole -mwindows -lsetupapi
	BUILD = build/windows/64bit
	CC=x86_64-w64-mingw32-gcc
	CXX=x86_64-w64-mingw32-g++
	EXE=.exe
	WX_CONFIG=--host=x86_64-w64-mingw32 --enable-monolithic --enable-stl --disable-shared
endif
ifeq ($(TARGET),Linux)
	VPATH += hidapi/linux
	LDFLAGS += -ludev
	BUILD = build/linux
	WX_CONFIG=--enable-monolithic --enable-stl
endif
ifeq ($(TARGET),Darwin)
	# Should match OSX
	VPATH += hidapi-mac
	LDFLAGS += -framework IOKit -framework CoreFoundation
	CFLAGS += -mmacosx-version-min=10.7
	CXXFLAGS += -stdlib=libc++ -mmacosx-version-min=10.7
	CC=clang
	CXX=clang++
	BUILD=build/mac
	WX_CONFIG=--enable-monolithic --enable-stl --disable-shared
endif

all:  $(BUILD)/scsi2sd-util$(EXE)

CYAPI = \
	$(BUILD)/cybtldr_api2.o \
	$(BUILD)/cybtldr_api.o \
	$(BUILD)/cybtldr_command.o \
	$(BUILD)/cybtldr_parse.o \


HIDAPI = \
	$(BUILD)/hid.o \


OBJ = \
	$(CYAPI) $(HIDAPI) \
	$(BUILD)/scsi2sd-util.o \
	$(BUILD)/ConfigUtil.o \
	$(BUILD)/Firmware.o \
	$(BUILD)/TargetPanel.o \
	$(BUILD)/SCSI2SD_Bootloader.o \
	$(BUILD)/SCSI2SD_HID.o \
	$(BUILD)/hidpacket.o \


$(OBJ): $(BUILD)/wx.buildstamp
$(BUILD)/wx.buildstamp:
	mkdir -p $(dir $@)
	( \
		cd $(BUILD) && \
		$(CURDIR)/wxWidgets/configure $(WX_CONFIG) && \
		$(MAKE) \
	) && \
	touch $@

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -c -o $@

$(BUILD)/%.o: %.cc
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) `$(BUILD)/wx-config --cxxflags` $< -c -o $@

$(BUILD)/scsi2sd-util$(EXE): $(OBJ)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) `$(BUILD)/wx-config --libs` -o $@

clean:
	rm $(BUILD)/scsi2sd-util$(EXE) $(OBJ)

