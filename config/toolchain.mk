ifeq ($(CPU),arm)
TOOLCHAIN = arm-none-eabi
TARGET_FORMAT = elf32-littlearm
ifeq ($(CPU_FLAVOR),lpc1768)
TARGET_CPPFLAGS += -mcpu=cortex-m3 -mtune=cortex-m3 -D__thumb2__=1 -march=armv7-m -mfix-cortex-m3-ldrd
endif
TARGET_CPPFLAGS += -Os -mapcs-frame -msoft-float -mno-sched-prolog -fno-hosted -ffunction-sections -fdata-sections -fshort-wchar
TARGET_CFLAGS += -mthumb
endif

TARGET_CC = $(TOOLCHAIN)-gcc
TARGET_CXX = $(TOOLCHAIN)-g++
TARGET_LD = $(TOOLCHAIN)-gcc
TARGET_RANLIB = $(TOOLCHAIN)-ranlib
TARGET_AR = $(TOOLCHAIN)-ar
TARGET_AS = $(TOOLCHAIN)-gcc
TARGET_OBJCOPY = $(TOOLCHAIN)-objcopy

TARGET_OBJCOPY_BIN = $(TARGET_OBJCOPY) -I binary -O $(TARGET_FORMAT) --binary-architecture $(CPU)
