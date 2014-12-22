LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CFLAGS := -W -Wall -Werror -D_POSIX_C_SOURCE=200112

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../

# bionic /sys/types.h fails with -std=c++0x, as it doesn't include
# stdint.h, but tries to use uint64_t.
LOCAL_CPPFLAGS := -std=gnu++0x
LOCAL_EXPORT_CPPFLAGS := -std=gnu++0x

# libzipper throws exceptions
LOCAL_CPPFLAGS += -fexceptions -frtti
LOCAL_EXPORT_CPPFLAGS += -fexceptions -frtti

LOCAL_CPP_EXTENSION := .cc

LOCAL_LDLIBS := -lz
LOCAL_EXPORT_LDLIBS := -lz

LOCAL_MODULE    := zipper
LOCAL_MODULE_FILENAME    := libzipper

LOCAL_SRC_FILES :=\
	../../CompressedFile.cc \
	../../deflate.cc \
	../../gzip.cc \
	../../zip.cc \
	../../Compressor.cc \
	../../Exception.cc \
	../../Reader.cc \
	../../zipper.cc \
	../../Container.cc \
	../../FileReader.cc \
	../../Decompressor.cc \
	../../FileWriter.cc \
	../../Writer.cc \
	../../port/strerror_posix.cc \

include $(BUILD_STATIC_LIBRARY)
