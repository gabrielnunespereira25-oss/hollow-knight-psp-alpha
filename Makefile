
TARGET = hkbmenu
OBJS = src/main.o

CC = psp-gcc
CFLAGS = -O2 -G0 -Wall

BUILD_PRX = 1
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Hollow Knight Beta Menu

LIBS = -lpspdebug -lpspdisplay -lpspctrl

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
