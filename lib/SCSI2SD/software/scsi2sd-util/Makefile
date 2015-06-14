VPATH=cybootloaderutils ../SCSI2SD/src

CPPFLAGS = -I cybootloaderutils -I hidapi/hidapi -I ../include -Ilibzipper-1.0.4 -I$(BUILD)/zlib
CFLAGS += -Wall -Wno-pointer-sign -O2 -g
CXXFLAGS += -Wall -O2 -g -std=c++0x
LDFLAGS += -L$(BUILD)/libzipper/.libs -lzipper -L$(BUILD)/zlib -lz

LIBZIPPER_CONFIG = --disable-shared LDFLAGS="-L../zlib" CPPFLAGS="-I../zlib"

# wxWidgets 3.0.2 uses broken Webkit headers under OSX Yosemeti
# liblzma not available on OSX 10.7
WX_CONFIG=--disable-webkit --disable-webviewwebkit  \
	--without-libtiff --without-libjbig --without-liblzma --without-opengl \
	--enable-monolithic --enable-stl --disable-shared

TARGET ?= $(shell uname -s)
ifeq ($(TARGET),Win32)
	VPATH += hidapi/windows
	LDFLAGS += -static -mconsole -mwindows -lsetupapi
	BUILD = build/windows/32bit
	CC=i686-w64-mingw32-gcc
	CXX=i686-w64-mingw32-g++
	LIBZIPPER_CONFIG+=--host=i686-w64-mingw32
	EXE=.exe
	WX_CONFIG+=--host=i686-w64-mingw32
endif
ifeq ($(TARGET),Win64)
	VPATH += hidapi/windows
	LDFLAGS += -static -mconsole -mwindows -lsetupapi
	BUILD = build/windows/64bit
	CC=x86_64-w64-mingw32-gcc
	CXX=x86_64-w64-mingw32-g++
	LIBZIPPER_CONFIG+=--host=x86_64-w64-mingw32
	EXE=.exe
	WX_CONFIG+=--host=x86_64-w64-mingw32
endif
ifeq ($(TARGET),Linux)
	VPATH += hidapi/linux
	LDFLAGS += -ludev -lexpat
	BUILD = build/linux
endif
ifeq ($(TARGET),Darwin)
	# Should match OSX
	VPATH += hidapi-mac
	LDFLAGS += -framework IOKit -framework CoreFoundation -lexpat
	CC=clang -mmacosx-version-min=10.7
	CXX=clang++ -stdlib=libc++ -mmacosx-version-min=10.7
	WX_CONFIG += --with-macosx-version-min=10.7
	BUILD=build/mac
endif

export CC CXX

all:  $(BUILD)/scsi2sd-util$(EXE) $(BUILD)/scsi2sd-monitor$(EXE)

CYAPI = \
	$(BUILD)/cybtldr_api2.o \
	$(BUILD)/cybtldr_api.o \
	$(BUILD)/cybtldr_command.o \
	$(BUILD)/cybtldr_parse.o \


HIDAPI = \
	$(BUILD)/hid.o \


OBJ = \
	$(CYAPI) $(HIDAPI) \
	$(BUILD)/ConfigUtil.o \
	$(BUILD)/Firmware.o \
	$(BUILD)/TargetPanel.o \
	$(BUILD)/SCSI2SD_Bootloader.o \
	$(BUILD)/SCSI2SD_HID.o \
	$(BUILD)/hidpacket.o \

EXEOBJ = \
	$(BUILD)/scsi2sd-util.o \
	$(BUILD)/scsi2sd-monitor.o \



$(OBJ): $(BUILD)/zlib/buildstamp
$(EXEOBJ): $(BUILD)/zlib/buildstamp
$(BUILD)/zlib/buildstamp:
	mkdir -p $(dir $@)
	( \
		cd $(dir $@) && \
		cp -a $(CURDIR)/zlib-1.2.8/* . && \
		./configure --static && \
		$(MAKE) \
	) && \
	touch $@

$(OBJ): $(BUILD)/wx.buildstamp
$(EXEOBJ): $(BUILD)/wx.buildstamp
$(BUILD)/wx.buildstamp: $(BUILD)/zlib/buildstamp
	mkdir -p $(dir $@)
	( \
		cd $(BUILD) && \
		$(CURDIR)/wxWidgets/configure $(WX_CONFIG) CPPFLAGS=-I../zlib LDFLAGS=-I../zlib && \
		$(MAKE) \
	) && \
	touch $@

$(OBJ): $(BUILD)/libzipper/buildstamp
$(EXEOBJ): $(BUILD)/libzipper/buildstamp
$(BUILD)/libzipper/buildstamp: $(BUILD)/zlib/buildstamp
	mkdir -p $(dir $@)
	( \
		cd $(dir $@) && \
		$(CURDIR)/libzipper-1.0.4/configure ${LIBZIPPER_CONFIG} --disable-shared --enable-static && \
		$(MAKE) libzipper.la \
	) && \
	touch $@

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -c -o $@

$(BUILD)/%.o: %.cc
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) `$(BUILD)/wx-config --cxxflags` $< -c -o $@

$(BUILD)/scsi2sd-util$(EXE): $(OBJ) $(BUILD)/scsi2sd-util.o
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) `$(BUILD)/wx-config --libs` -o $@

$(BUILD)/scsi2sd-monitor$(EXE): $(OBJ) $(BUILD)/scsi2sd-monitor.o
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) `$(BUILD)/wx-config --libs` -o $@

clean:
	rm $(BUILD)/scsi2sd-util$(EXE) $(OBJ) $(BUILD)/libzipper/buildstamp

