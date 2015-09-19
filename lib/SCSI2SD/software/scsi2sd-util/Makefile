VPATH=cybootloaderutils ../SCSI2SD/src

VERSION=4.4.0
NAME=scsi2sd-util

ifeq ($(USE_SYSTEM), Yes)
USE_SYSTEM_HIDAPI = Yes
USE_SYSTEM_ZLIB = Yes
USE_SYSTEM_WX = Yes
endif

CPPFLAGS_HIDAPI=$(shell pkg-config hidapi-hidraw --cflags)
CPPFLAGS_ZLIB=$(shell pkg-config zlib --cflags)

CPPFLAGS = -I cybootloaderutils $(CPPFLAGS_HIDAPI) -I. -I ../include -Ilibzipper-1.0.4 $(CPPFLAGS_ZLIB)
CFLAGS += -Wall -Wno-pointer-sign -O2 -g
CXXFLAGS += -Wall -O2 -g -std=c++0x

LDFLAGS_ZLIB=$(shell pkg-config zlib --libs)
LDFLAGS_HIDAPI=$(shell pkg-config hidapi-hidraw --libs)

LDFLAGS += -L$(BUILD)/libzipper/.libs -lzipper $(LDFLAGS_ZLIB) $(LDFLAGS_HIDAPI)

LIBZIPPER_CONFIG = --disable-shared LDFLAGS="$(LDFLAGS_ZLIB)" CPPFLAGS="$(CPPFLAGS_ZLIB)"

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


ifneq ($(USE_SYSTEM_HIDAPI),Yes)
HIDAPI = \
	$(BUILD)/hid.o
endif


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



ifneq ($(USE_SYSTEM_ZLIB),Yes)
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
endif

ifneq ($(USE_SYSTEM_WX),Yes)
$(OBJ): $(BUILD)/wx.buildstamp
$(EXEOBJ): $(BUILD)/wx.buildstamp
ifneq ($(USE_SYSTEM_ZLIB),Yes)
$(BUILD)/wx.buildstamp: $(BUILD)/zlib/buildstamp
else
$(BUILD)/wx.buildstamp:
endif
	mkdir -p $(dir $@)
	( \
		cd $(BUILD) && \
		$(CURDIR)/wxWidgets/configure $(WX_CONFIG) CPPFLAGS="$(CPPFLAGS_ZLIB)" LDFLAGS="$(LDFLAGS_ZLIB)" && \
		$(MAKE) \
	) && \
	touch $@
endif

ifneq ($(USE_SYSTEM_ZLIB),Yes)
LIBZIPPER_STATIC=-enable-static
endif

$(OBJ): $(BUILD)/libzipper/buildstamp
$(EXEOBJ): $(BUILD)/libzipper/buildstamp
ifneq ($(USE_SYSTEM_ZLIB),Yes)
$(BUILD)/libzipper/buildstamp: $(BUILD)/zlib/buildstamp
else
$(BUILD)/libzipper/buildstamp:
endif
	mkdir -p $(dir $@)
	( \
		cd $(dir $@) && \
		$(CURDIR)/libzipper-1.0.4/configure ${LIBZIPPER_CONFIG} --disable-shared $(LIBZIPPER_STATIC) && \
		$(MAKE) libzipper.la \
	) && \
	touch $@

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -c -o $@

$(BUILD)/%.o: %.cc
	mkdir -p $(dir $@)
ifneq ($(USE_SYSTEM_WX),Yes)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) `$(BUILD)/wx-config --cxxflags` $< -c -o $@
else
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) `wx-config-3.0 --cxxflags` $< -c -o $@
endif

$(BUILD)/scsi2sd-util$(EXE): $(OBJ) $(BUILD)/scsi2sd-util.o
	mkdir -p $(dir $@)
ifneq ($(USE_SYSTEM_WX),Yes)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) `$(BUILD)/wx-config --libs` -o $@
else
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) `wx-config-3.0 --libs` -o $@
endif

$(BUILD)/scsi2sd-monitor$(EXE): $(OBJ) $(BUILD)/scsi2sd-monitor.o
	mkdir -p $(dir $@)
ifneq ($(USE_SYSTEM_WX),Yes)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) `$(BUILD)/wx-config --libs` -o $@
else
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) `wx-config-3.0 --libs` -o $@
endif

clean:
	rm $(BUILD)/scsi2sd-util$(EXE) $(OBJ) $(BUILD)/libzipper/buildstamp

PREFIX=/usr
install:
	install -d $(DESTDIR)/$(PREFIX)/bin
	install build/linux/scsi2sd-util $(DESTDIR)/$(PREFIX)/bin
	install build/linux/scsi2sd-monitor $(DESTDIR)/$(PREFIX)/bin

dist:
	rm -fr $(NAME)-$(VERSION)
	mkdir $(NAME)-$(VERSION)
	cp -pr build.sh ConfigUtil.cc ConfigUtil.hh scsi2sd-util.spec \
               ../SCSI2SD/src/hidpacket.c ../include/hidpacket.h ../include/scsi2sd.h \
	       cybootloaderutils Firmware.cc Firmware.hh libzipper-1.0.4 Makefile \
               SCSI2SD_Bootloader.cc SCSI2SD_Bootloader.hh SCSI2SD_HID.cc SCSI2SD_HID.hh \
	       scsi2sd-monitor.cc scsi2sd-util.cc TargetPanel.cc TargetPanel.hh \
	       $(NAME)-$(VERSION)
	tar jcvf $(NAME)-$(VERSION).tar.bz2 $(NAME)-$(VERSION)
