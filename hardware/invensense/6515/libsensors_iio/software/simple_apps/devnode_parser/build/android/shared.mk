EXEC = inv_devnode_parser$(SHARED_APP_SUFFIX)

MK_NAME = $(notdir $(CURDIR)/$(firstword $(MAKEFILE_LIST)))

# ANDROID version check
BUILD_ANDROID_LOLLIPOP = $(shell test -d $(ANDROID_ROOT)/bionic/libc/kernel/uapi && echo 1)
$(info YD>>BUILD_ANDROID_LOLLIPOP = $(BUILD_ANDROID_LOLLIPOP))
#ANDROID version check END

ifeq ($(BUILD_ANDROID_LOLLIPOP),1)
CFLAGS += -DANDROID_LOLLIPOP
else
CFLAGS += -DANDROID_KITKAT
endif

#--yd CROSS ?= $(ANDROID_ROOT)/prebuilt/linux-x86/toolchain/arm-eabi-4.4.0/bin/arm-eabi-
COMP  ?= $(CROSS)gcc
LINK  ?= $(CROSS)gcc

OBJFOLDER = $(CURDIR)/obj

INV_ROOT   = ../../../../..
APP_DIR    = $(CURDIR)/../..
MLLITE_DIR = $(INV_ROOT)/software/core/mllite

include $(INV_ROOT)/software/build/android/common.mk

CFLAGS += $(CMDLINE_CFLAGS)
CFLAGS += $(ANDROID_COMPILE)
CFLAGS += -Wall
ifeq ($(BUILD_ANDROID_LOLLIPOP),1)
else
CFLAGS += -fpic
endif
CFLAGS += -nostdlib
CFLAGS += -DNDEBUG
CFLAGS += -D_REENTRANT
CFLAGS += -DLINUX
CFLAGS += -DANDROID
ifeq ($(ARCH),arm)
CFLAGS += -mthumb-interwork
endif
CFLAGS += -fno-exceptions
CFLAGS += -ffunction-sections
CFLAGS += -funwind-tables
CFLAGS += -fstack-protector
CFLAGS += -fno-short-enums
CFLAGS += -fmessage-length=0
CFLAGS += -I$(MLLITE_DIR)
CFLAGS += -I$(MPL_DIR)
CFLAGS += $(INV_INCLUDES)
CFLAGS += $(INV_DEFINES)

LLINK  = -lc
LLINK += -lm
LLINK += -lutils
LLINK += -lcutils
LLINK += -lgcc
LLINK += -ldl
LLINK += -lstdc++
LLINK += -llog
LLINK += -lz

LFLAGS += $(CMDLINE_LFLAGS)
LFLAGS += $(ANDROID_LINK_EXECUTABLE)

ifeq ($(ARCH),arm64)
LRPATH  = -Wl,-rpath,$(ANDROID_ROOT)/out/target/product/$(PRODUCT)/obj/lib
else
LRPATH  = -Wl,-rpath,$(ANDROID_ROOT)/out/target/product/$(PRODUCT)/obj/lib:$(ANDROID_ROOT)/out/target/product/$(PRODUCT)/system/lib
endif

####################################################################################################
## sources

INV_LIBS = $(MLLITE_DIR)/build/$(TARGET)/$(LIB_PREFIX)$(MLLITE_LIB_NAME).$(SHARED_LIB_EXT)

#INV_SOURCES and VPATH provided by Makefile.filelist
include ../filelist.mk

INV_OBJS := $(addsuffix .o,$(INV_SOURCES))
INV_OBJS_DST = $(addprefix $(OBJFOLDER)/,$(addsuffix .o, $(notdir $(INV_SOURCES))))

####################################################################################################
## rules

.PHONY: all clean cleanall install

all: $(EXEC) $(MK_NAME)

$(EXEC) : $(OBJFOLDER) $(INV_OBJS_DST) $(INV_LIBS) $(MK_NAME)
	@$(call echo_in_colors, "\n<linking $(EXEC) with objects $(INV_OBJS_DST) $(PREBUILT_OBJS) and libraries $(INV_LIBS)\n")
	$(LINK) $(INV_OBJS_DST) -o $(EXEC) $(LFLAGS) $(LLINK) $(INV_LIBS) $(LLINK) $(LRPATH)

$(OBJFOLDER) :
	@$(call echo_in_colors, "\n<creating object's folder 'obj/'>\n")
	mkdir obj

$(INV_OBJS_DST) : $(OBJFOLDER)/%.c.o : %.c  $(MK_NAME)
	@$(call echo_in_colors, "\n<compile $< to $(OBJFOLDER)/$(notdir $@)>\n")
	$(COMP) $(ANDROID_INCLUDES) $(KERNEL_INCLUDES) $(INV_INCLUDES) $(CFLAGS) -o $@ -c $<

clean : 
	rm -fR $(OBJFOLDER)

cleanall : 
	rm -fR $(EXEC) $(OBJFOLDER)

install : $(EXEC)
	cp -f $(EXEC) $(INSTALL_DIR)


