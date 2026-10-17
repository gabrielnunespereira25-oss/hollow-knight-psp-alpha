
TARGET = DarkBot
BUILD_PRX = 1

OBJS = \
    src/main.o \
    src/png_loader.o

INCDIR =
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lpspgu -lpspgum -lpng -lz -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Dark Bot

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
