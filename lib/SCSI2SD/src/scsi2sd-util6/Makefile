VPATH=../firmware TerminalWx/src TerminalWx/src/taTelnet TerminalWx/src/GTerm

VERSION=6.0
NAME=scsi2sd-util6

ifeq ($(USE_SYSTEM), Yes)
USE_SYSTEM_HIDAPI = Yes
USE_SYSTEM_ZLIB = Yes
USE_SYSTEM_WX = Yes
USE_SYSTEM_LIBUSB = Yes
USE_SYSTEM_DFU_UTIL = Yes
endif

ifeq ($(USE_SYSTEM_LIBUSB), Yes)
CPPFLAGS_LIBUSB=$(shell pkg-config libusb-1.0 --cflags)
LDFLAGS_LIBUSB=$(shell pkg-config libusb-1.0 --libs)
else
CPPFLAGS_LIBUSB=-I $(PWD)/libusb-1.0.23/libusb
LDFLAGS_LIBUSB=-L $(BUILD)/libusb/libusb/.libs -lusb-1.0
endif

ifeq ($(USE_SYSTEM_HIDAPI), Yes)
CPPFLAGS_HIDAPI=$(shell pkg-config hidapi-hidraw --cflags)
LDFLAGS_HIDAPI=$(shell pkg-config hidapi-hidraw --libs)
else
CPPFLAGS_HIDAPI=-I hidapi/hidapi
LDFLAGS_HIDAPI=
endif

ifeq ($(USE_SYSTEM_ZLIB), Yes)
CPPFLAGS_ZLIB=$(shell pkg-config zlib --cflags)
LDFLAGS_ZLIB=$(shell pkg-config zlib --libs)
LIBZIPPER_CONFIG = --disable-shared LDFLAGS="$(LDFLAGS_ZLIB)" CPPFLAGS="$(CPPFLAGS_ZLIB)"
else
CPPFLAGS_ZLIB=-I$(BUILD)/zlib
LDFLAGS_ZLIB=-L$(BUILD)/zlib -lz
LIBZIPPER_CONFIG = --disable-shared LDFLAGS="-L../zlib" CPPFLAGS="-I../zlib"
endif

ifeq ($(USE_SYSTEM_WX),Yes)
LDFLAGS_WX=$(shell wx-config-3.0 --libs)
else
LDFLAGS_WX=$(shell $(BUILD)/wx/wx-config --libs)
endif

CPPFLAGS = $(CPPFLAGS_HIDAPI) -I. -I ../../include -ITerminalWx/src \
	-Ilibzipper-1.0.4 \
	$(CPPFLAGS_ZLIB) \
	$(CPPFLAGS_LIBUSB) \
	-DHAVE_LIBUSB_1_0 \


CFLAGS += -Wall -Wno-pointer-sign -O2 -g -fPIC
CXXFLAGS += -Wall -O2 -g -std=c++0x -fPIC

LDFLAGS += -L$(BUILD)/libzipper/.libs -lzipper \
	$(LDFLAGS_ZLIB) \
	$(LDFLAGS_HIDAPI) \
	$(LDFLAGS_LIBUSB)


# wxWidgets 3.0.2 uses broken Webkit headers under OSX Yosemeti
# liblzma not available on OSX 10.7
# --disable-mediactrl for missing Quicktime.h on Mac OSX Sierra
WX_CONFIG=--disable-webkit --disable-webviewwebkit  --disable-mediactrl \
	--without-libtiff --without-libjbig --without-liblzma --without-opengl \
	--enable-monolithic --enable-stl --disable-shared

TARGET ?= $(shell uname -s)
ifeq ($(TARGET),Win32)
	VPATH += hidapi/windows
	LDFLAGS += -static -mconsole -mwindows -lsetupapi
	BUILD := $(PWD)/build/windows/32bit
	CC=i686-w64-mingw32-gcc
	CXX=i686-w64-mingw32-g++
	LIBZIPPER_CONFIG+=--host=i686-w64-mingw32
	EXE=.exe
	WX_CONFIG+=--host=i686-w64-mingw32
	LIBUSB_CONFIG+=--host=i686-w64-mingw32 --disable-shared
	DFU-UTIL_CONFIG+=--host=i686-w64-mingw32
endif
ifeq ($(TARGET),Win64)
	VPATH += hidapi/windows
	LDFLAGS += -static -mconsole -mwindows -lsetupapi
	BUILD := $(PWD)/build/windows/64bit
	CC=x86_64-w64-mingw32-gcc
	CXX=x86_64-w64-mingw32-g++
	LIBZIPPER_CONFIG+=--host=x86_64-w64-mingw32
	EXE=.exe
	WX_CONFIG+=--host=x86_64-w64-mingw32
	LIBUSB_CONFIG+=--host=x86_64-w64-mingw32 --disable-shared
	DFU-UTIL_CONFIG+=--host=x86_64-w64-mingw32
endif
ifeq ($(TARGET),Linux)
	VPATH += hidapi/linux
	LDFLAGS += -ludev -lexpat -lusb-1.0
	BUILD := $(PWD)/build/linux
	LIBUSB_CONFIG+=--disable-shared
	LDFLAGS_LIBUSB+= -ludev -lpthread
	USE_SYSTEM_DFU_UTIL = Yes
endif
ifeq ($(TARGET),Darwin)
	# Should match OSX
	VPATH += hidapi/mac
	LDFLAGS += -framework IOKit -framework CoreFoundation -lexpat
	CC=clang -mmacosx-version-min=10.7
	CXX=clang++ -stdlib=libc++ -mmacosx-version-min=10.7
	WX_CONFIG += --with-macosx-version-min=10.7
	CPPFLAGS_WXBUILD += -D__ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES=1
	LIBUSB_CONFIG += --with-macosx-version-min=10.7 --disable-shared
	LDFLAGS_LIBUSB += -lobjc
	DFU-UTIL_CONFIG += --with-macosx-version-min=10.7 --disable-shared
	BUILD := $(PWD)/build/mac
all: $(BUILD)/scsi2sd-util6.dmg

$(BUILD)/scsi2sd-util6.dmg: $(BUILD)/scsi2sd-util6 $(BUILD)/dfu-util/buildstamp
	rm -rf $(dir $@)/dmg $@
	mkdir -p $(dir $@)/dmg
	cp $(BUILD)/scsi2sd-util6 $(BUILD)/dfu-util/src/dfu-util $(dir $@)/dmg
	chmod a+rx $(dir $@)/dmg/*
	hdiutil create -volname scsi2sd-util6 -srcfolder $(dir $@)/dmg $@
endif

ifeq ($(TARGET),osxcross)
	# TODO osxcross tools must be in path for the wx configure to find the
	# correct AR binary
	VPATH += hidapi/mac
	LDFLAGS += -framework IOKit -framework CoreFoundation -lexpat
	CC=/home/michael/osx/osxcross/target/bin/x86_64-apple-darwin19-cc -mmacosx-version-min=10.7
	CXX=/home/michael/osx/osxcross/target/bin/x86_64-apple-darwin19-c++ -stdlib=libc++ -mmacosx-version-min=10.7
	LIBZIPPER_CONFIG+=--host=x86_64-apple-darwin19 "ZLIB_CFLAGS=-I$(BUILD)/zlib" "ZLIB_LIBS=-L$(BUILD)/zlib -lz"
	CROSS_PREFIX=/home/michael/osx/osxcross/target/bin/x86_64-apple-darwin19-
	WX_CONFIG += --with-macosx-version-min=10.7 --host=x86_64-apple-darwin19 SETFILE=/bin/true
	CPPFLAGS_WXBUILD += -D__ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES=1
	LIBUSB_CONFIG += --with-macosx-version-min=10.7 --disable-shared --host=x86_64-apple-darwin19
	LDFLAGS_LIBUSB += -lobjc
	DFU-UTIL_CONFIG += --with-macosx-version-min=10.7 --disable-shared --host=x86_64-apple-darwin19 "USB_CFLAGS=-I$(BUILD)/libusb/libusb/include" "USB_LIBS=-L$(BUILD)/libusb/libusb/.libs -lusb-1.0 -lobjc -Wl,-framework,IOKit -Wl,-framework,CoreFoundation"
	BUILD := $(PWD)/build/mac
all: $(BUILD)/scsi2sd-util6.dmg

$(BUILD)/scsi2sd-util6.dmg: $(BUILD)/scsi2sd-util6 $(BUILD)/dfu-util/buildstamp
	rm -rf $(dir $@)/dmg $@
	mkdir -p $(dir $@)/dmg
	cp $(BUILD)/scsi2sd-util6 $(BUILD)/dfu-util/src/dfu-util $(dir $@)/dmg
	chmod a+rx $(dir $@)/dmg/*
	hdiutil create -volname scsi2sd-util6 -srcfolder $(dir $@)/dmg $@
endif

export CC CXX

all:  $(BUILD)/scsi2sd-util6$(EXE)


ifneq ($(USE_SYSTEM_HIDAPI),Yes)
HIDAPI = \
	$(BUILD)/hid.o
endif

WXOBJ =\
	$(BUILD)/ConfigUtil.o \
	$(BUILD)/BoardPanel.o \
	$(BUILD)/TargetPanel.o \
	$(BUILD)/terminalwx.o \
	$(BUILD)/terminalinputevent.o \
	$(BUILD)/wxterm.o \
	$(BUILD)/gterm.o \
	$(BUILD)/actions.o \
	$(BUILD)/keytrans.o \
	$(BUILD)/states.o \
	$(BUILD)/utils.o \
	$(BUILD)/vt52_states.o \

OBJ = \
	$(HIDAPI) \
	$(BUILD)/SCSI2SD_HID.o \
	$(BUILD)/hidpacket.o \
	$(BUILD)/Dfu.o \


EXEOBJ = \
	$(BUILD)/scsi2sd-util6.o \


ifneq ($(USE_SYSTEM_ZLIB),Yes)
$(OBJ): $(BUILD)/zlib/buildstamp
$(WXOBJ): $(BUILD)/zlib/buildstamp
$(EXEOBJ): $(BUILD)/zlib/buildstamp
$(BUILD)/zlib/buildstamp:
	mkdir -p $(dir $@)
	( \
		cd $(dir $@) && \
		cp -a $(CURDIR)/zlib-1.2.8/* . && \
		CROSS_PREFIX=${CROSS_PREFIX} ./configure --static && \
		$(MAKE) \
	) && \
	touch $@
endif

ifneq ($(USE_SYSTEM_LIBUSB),Yes)
$(OBJ): $(BUILD)/libusb/buildstamp
$(WXOBJ): $(BUILD)/libusb/buildstamp
$(EXEOBJ): $(BUILD)/libusb/buildstamp
$(BUILD)/libusb/buildstamp:
	mkdir -p $(dir $@)
	( \
		cd $(dir $@) && \
		$(CURDIR)/libusb-1.0.23/configure $(LIBUSB_CONFIG) && \
		$(MAKE) \
	) && \
	touch $@
else
$(BUILD)/libusb/buildstamp:
	mkdir -p $(dir $@)
	touch $@
endif

ifneq ($(USE_SYSTEM_WX),Yes)
$(OBJ): $(BUILD)/wx/buildstamp
$(WXOBJ): $(BUILD)/wx/buildstamp
$(EXEOBJ): $(BUILD)/wx/buildstamp
ifneq ($(USE_SYSTEM_ZLIB),Yes)
$(BUILD)/wx/buildstamp: $(BUILD)/zlib/buildstamp
else
$(BUILD)/wx/buildstamp:
endif
	mkdir -p $(dir $@)
	( \
		cd $(dir $@) && \
		$(CURDIR)/wxWidgets/configure $(WX_CONFIG) CPPFLAGS="$(CPPFLAGS_ZLIB) $(CPPFLAGS_WXBUILD)" LDFLAGS="$(LDFLAGS_ZLIB)" && \
		$(MAKE) \
	) && \
	touch $@
endif

ifneq ($(USE_SYSTEM_ZLIB),Yes)
LIBZIPPER_STATIC=-enable-static
endif

$(OBJ): $(BUILD)/libzipper/buildstamp
$(WXOBJ): $(BUILD)/libzipper/buildstamp
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
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) `$(BUILD)/wx/wx-config --cxxflags` $< -c -o $@
else
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) `wx-config-3.0 --cxxflags` $< -c -o $@
endif

$(BUILD)/%.o: %.cpp
	mkdir -p $(dir $@)
ifneq ($(USE_SYSTEM_WX),Yes)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) `$(BUILD)/wx/wx-config --cxxflags` $< -c -o $@
else
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) `wx-config-3.0 --cxxflags` $< -c -o $@
endif

$(BUILD)/scsi2sd-util6$(EXE): $(OBJ) $(WXOBJ) $(BUILD)/scsi2sd-util.o
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS_WX) $(LDFLAGS) -o $@

all: $(BUILD)/scsi2sd-test$(EXE)
$(BUILD)/scsi2sd-test$(EXE): $(OBJ) $(BUILD)/scsi2sd-test.o
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS_WX) $(LDFLAGS) -o $@

ifneq ($(USE_SYSTEM_DFU_UTIL),Yes)
all: $(BUILD)/dfu-util/buildstamp
endif
$(BUILD)/dfu-util/buildstamp: $(BUILD)/libusb/buildstamp
	mkdir -p $(dir $@)
	( \
		cd $(dir $@) && \
		$(CURDIR)/dfu-util/configure ${DFU-UTIL_CONFIG} CPPFLAGS="${CPPFLAGS_LIBUSB}" LDFLAGS="${LDFLAGS_LIBUSB} ${LDFLAGS}" && \
		$(MAKE) \
	) && \
	touch $@

clean:
	rm $(BUILD)/scsi2sd-util6$(EXE) $(OBJ) $(BUILD)/libzipper/buildstamp

PREFIX=/usr
install:
	install -d $(DESTDIR)/$(PREFIX)/bin
	install build/linux/scsi2sd-util6 $(DESTDIR)/$(PREFIX)/bin

dist:
	rm -fr $(NAME)-$(VERSION)
	mkdir $(NAME)-$(VERSION)
	cp -pr ConfigUtil.cc ConfigUtil.hh scsi2sd-util6.spec \
               ../SCSI2SD/src/hidpacket.c ../../include/hidpacket.h ../../include/scsi2sd.h \
	       libzipper-1.0.4 Makefile \
               SCSI2SD_Bootloader.cc SCSI2SD_Bootloader.hh SCSI2SD_HID.cc SCSI2SD_HID.hh \
	       scsi2sd-util.cc TargetPanel.cc TargetPanel.hh \
	       BoardPanel.cc BoardPanel.hh \
	       $(NAME)-$(VERSION)
	tar jcvf $(NAME)-$(VERSION).tar.bz2 $(NAME)-$(VERSION)
