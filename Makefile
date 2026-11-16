# PROJECT NOISE - native Nintendo DSi homebrew
# Build with BlocksDS / Wonderful Toolchain.
BLOCKSDS ?= /opt/blocksds/core
BLOCKSDSEXT ?= /opt/blocksds/external

NAME := PROJECT_NOISE_DSi
GAME_TITLE := PROJECTNOISE
GAME_SUBTITLE := Premium DSi Music Player
GAME_AUTHOR := MADE BY GRIMZZZZ
GAME_ICON := resources/icon.png

SOURCEDIRS := source
INCLUDEDIRS := source libs/libmad/include
NITROFSDIR := nitrofs

# ARM7 image with both DSWiFi and Maxmod support. DSWiFi internet mode needs
# the cooperative thread scheduler in the ARM9 main loop (see gfx_present()).
ARM7ELF := $(BLOCKSDS)/sys/arm7/main_core/arm7_dswifi_maxmod.elf

LIBS += -lmbedcrypto -lmbedtls -lmbedx509 -lcurl -lz -ldswifi9 -lmm9 -lmad -lnds9 -lc
LIBDIRS += $(BLOCKSDSEXT)/mbedtls \
           $(BLOCKSDSEXT)/libcurl \
           $(BLOCKSDS)/libs/dswifi \
           $(BLOCKSDS)/libs/maxmod \
           $(BLOCKSDS)/libs/libnds \
           $(CURDIR)/libs/libmad/lib

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
