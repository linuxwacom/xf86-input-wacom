SUBDIRS = . wacomxi @WCM_KERNEL_VER@

bin_PROGRAMS = @WCM_PROGS@ @WCM_XF86PROGS@
noinst_SCRIPTS = @WCM_XF86MODULES@
lib_LTLIBRARIES = libwacomcfg.la

wacomcfgdir = $(includedir)/wacomcfg
wacomcfg_HEADERS = wacomcfg.h

x86moduledir = /usr/X11R6/lib/modules/input
x86module_DATA = @WCM_XF86MODULES@

AM_CFLAGS = -Wall -pedantic
DEPFLAGS = @WCM_DEPFLAGS@

# These identify which programs, libraries, and headers could
# potentially be built or installed depending on the results of
# the configuration.
EXTRA_PROGRAMS = wacdump xidump xsetwacom 
EXTRA_SCRIPTS = wacom_drv.o wacom_drv-v3.o wacomcpl wacomcpl-exec

# Source dependencies
wacdump_SOURCES = wacdump.c wacscrn.c wacscrn.h \
		wactablet.c wactablet.h \
		wacserial.c wacserial.h \
		wacusb.c wacusb.h
wacdump_LDADD = -lncurses

xidump_SOURCES = xidump.c wacscrn.c wacscrn.h
xidump_LDFLAGS = @WCM_XIDUMP_LIBS@
xidump_LDADD = -lncurses

libwacomcfg_la_SOURCES = wacomcfg.c wacomcfg.h Xwacom.h
libwacomcfg_la_LDFLAGS = -no-undefined -version-info @WCM_LIBWACOMCFG_VER@
libwacomcfg_la_LIBADD = @WCM_LIBWACOMCFG_LIBS@

xsetwacom_SOURCES = xsetwacom.c wacomcfg.h Xwacom.h
xsetwacom_LDADD = libwacomcfg.la

EXTRA_DIST = xf86Wacom.c xf86Wacom.h \
		wcmSerial.c wcmISDV4.c wcmUSB.c \
		wcmCommon.c wcmCompat.c wcmConfig.c \
		wcmFilter.c wcmFilter.h wcmSerial.h

KERNEL_DIR=@WCM_KERNELDIR@
XF86_DIR=@WCM_XF86DIR@/xc
XF86_V3_DIR=@WCM_XF86V3DIR@/xc
DEBUG_FLAGS = -D__JEJ_DEBUG
ARCHITECTURE=@WCM_ARCH@

NO_MERGE_CONSTANTS=@WCM_NO_MERGE_CONSTANTS@

LINUX_INPUT=@WCM_LINUX_INPUT@

XF86OBJS = xf86Wacom.o wcmSerial.o wcmUSB.o wcmISDV4.o \
		wcmCommon.o wcmCompat.o wcmConfig.o wcmFilter.o
XF86V3OBJS = xf86Wacom-v3.o wcmSerial-v3.o wcmUSB-v3.o \
		wcmISDV4-v3.o wcmCommon-v3.o wcmCompat-v3.o \
		wcmConfig-v3.o wcmFilter-v3.o

xf86Wacom.o: xf86Wacom.c xf86Wacom.h Makefile
xf86Wacom-v3.o: xf86Wacom.c xf86Wacom.h Makefile
wcmSerial.o: wcmSerial.c wcmSerial.h xf86Wacom.h
wcmSerial-v3.o: wcmSerial.c wcmSerial.h xf86Wacom.h
wcmISDV4.o: wcmISDV4.c wcmSerial.h xf86Wacom.h
wcmISDV4-v3.o: wcmISDV4.c wcmSerial.h xf86Wacom.h
wcmUSB.o: wcmUSB.c xf86Wacom.h
wcmUSB-v3.o: wcmUSB.c xf86Wacom.h
wcmCommon.o: wcmCommon.c xf86Wacom.h
wcmCommon-v3.o: wcmCommon.c xf86Wacom.h
wcmCompat.o: wcmCompat.c xf86Wacom.h
wcmCompat-v3.o: wcmCompat.c xf86Wacom.h
wcmConfig.o: wcmConfig.c xf86Wacom.h
wcmConfig-v3.o: wcmConfig.c xf86Wacom.h
wcmFilter.o: wcmFilter.c xf86Wacom.h
wcmFilter-v3.o: wcmFilter.c xf86Wacom.h

$(XF86OBJS): xf86Wacom.c Makefile
	gcc -O2 $(DEPFLAGS) -march=i386 -mcpu=$(ARCHITECTURE) -pipe -ansi \
		-pedantic -Wall -Wpointer-arith $(NO_MERGE_CONSTANTS) \
		-I. -I$(XF86_DIR)/programs/Xserver/hw/xfree86/common \
		-I$(XF86_DIR)/programs/Xserver/hw/xfree86/loader \
		-I$(XF86_DIR)/programs/Xserver/hw/xfree86/os-support \
		-I$(XF86_DIR)/programs/Xserver/include \
		-I$(XF86_DIR)/programs/Xserver/mi \
		-I$(XF86_DIR)/exports/include/X11 \
		-I$(XF86_DIR)/include/extensions \
		-I$(XF86_DIR) \
		-I$(XF86_DIR)/exports/include \
		-Dlinux -D__i386__ -D_POSIX_C_SOURCE=199309L -D_POSIX_SOURCE \
		-D_XOPEN_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE  -D_GNU_SOURCE \
		-DSHAPE -DXINPUT -DXKB -DLBX -DXAPPGROUP -DXCSECURITY \
		-DTOGCUP -DXF86BIGFONT -DDPMSExtension -DPIXPRIV -DPANORAMIX \
		-DRENDER -DGCCUSESGAS -DAVOID_GLYPHBLT -DPIXPRIV \
		-DSINGLEDEPTH -DXFreeXDGA -DXvExtension -DXFree86LOADER \
		-DXFree86Server -DXF86VIDMODE -DXvMCExtension \
		-DSMART_SCHEDULE -DBUILDDEBUG -DXResExtension \
		-DX_BYTE_ORDER=X_LITTLE_ENDIAN -DNDEBUG -DFUNCPROTO=15 \
		-DNARROWPROTO -DIN_MODULE -DXFree86Module $(LINUX_INPUT) \
		-o $@ -c $(subst .o,.c,$@)

wacom_drv.o: $(XF86OBJS)
	ld -r $(XF86OBJS) -o wacom_drv.o

$(XF86V3OBJS):
	gcc -O2 -march=i386 -mcpu=$(ARCHITECTURE) -fno-strength-reduce -ansi \
		-pedantic \
		-I. -I$(XF86_V3_DIR)/programs/Xserver/hw/xfree86/common \
		-I$(XF86_V3_DIR)/programs/Xserver/hw/xfree86/os-support \
		-I$(XF86_V3_DIR)/programs/Xserver/hw/xfree86 \
		-I$(XF86_V3_DIR)/programs/Xserver/mfb \
		-I$(XF86_V3_DIR)/programs/Xserver/mi \
		-I$(XF86_V3_DIR)/programs/Xserver/include \
		-I$(XF86_V3_DIR)/programs/Xserver/os \
		-I$(XF86_V3_DIR)/exports/include/X11 \
		-I$(XF86_V3_DIR)/include/extensions \
		-I \ -I$(XF86_V3_DIR) \
		-I$(XF86_V3_DIR)/exports/include \
		-Dlinux -D__i386__ -D_POSIX_C_SOURCE=199309L \
		-D_POSIX_SOURCE -D_XOPEN_SOURCE=500L -D_BSD_SOURCE \
		-D_SVID_SOURCE -DSHAPE -DXINPUT -DXKB -DLBX -DXAPPGROUP \
		-DXCSECURITY -DDPMSExtension -DPIXPRIV  -DGCCUSESGAS \
		-DSTATIC_COLOR -DAVOID_GLYPHBLT -DPIXPRIV  -DXFreeXDGA \
		-DNDEBUG -DFUNCPROTO=15 -DNARROWPROTO -DPNP_MOUSE -fPIC \
		-DDYNAMIC_MODULE -DDEFAULT_MODULE_PATH=\"/usr/X11R6/lib/modules\" \
		-o $@ -c $(subst -v3.o,.c,$@)

wacom_drv-v3.o: $(XF86V3OBJS)
	ld -r $(XF86V3OBJS) -o wacom_drv-v3.o
