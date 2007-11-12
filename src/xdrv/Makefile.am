noinst_SCRIPTS = @WCM_XF86MODULES@

x86moduledir = @WCM_MODDIR@
x86module_DATA = @WCM_XF86MODULES@

AM_CFLAGS = -Wall -pedantic
DEPFLAGS = @WCM_DEPFLAGS@

# These identify which programs, libraries, and headers could
# potentially be built or installed depending on the results of
# the configuration.
EXTRA_SCRIPTS = wacom_drv.o wacom_drv.so

EXTRA_DIST = xf86Wacom.c wcmXCommand.c xf86Wacom.h \
		wcmSerial.c wcmISDV4.c wcmUSB.c \
		wcmCommon.c wcmCompat.c wcmConfig.c \
		wcmFilter.c wcmFilter.h wcmSerial.h \
		../include/Xwacom.h

KERNEL_DIR	=@WCM_KERNELDIR@
XF86_DIR	=@WCM_XF86DIR@
XORGSDK_DIR	=@WCM_XORGSDK@
DEBUG_FLAGS	= -D__JEJ_DEBUG
ARCHITECTURE	=@WCM_ARCH@
XSERVER64	=@WCM_XSERVER64@

if WCM_ENV_XORGSDK
DRIVER_INCLUDES = -I$(XORGSDK_DIR) $(X_CFLAGS) -DWCM_XORG
endif

if WCM_ENV_XF86
DRIVER_INCLUDES = \
	-I$(XF86_DIR)/programs/Xserver/hw/xfree86/common \
	-I$(XF86_DIR)/programs/Xserver/hw/xfree86/loader \
	-I$(XF86_DIR)/programs/Xserver/hw/xfree86/os-support \
	-I$(XF86_DIR)/programs/Xserver/include \
	-I$(XF86_DIR)/programs/Xserver/mi \
	-I$(XF86_DIR)/exports/include/X11 \
	-I$(XF86_DIR)/include/extensions \
	-I$(XF86_DIR) \
	-I$(XF86_DIR)/exports/include -DWCM_XFREE86
endif

NO_MERGE_CONSTANTS=@WCM_NO_MERGE_CONSTANTS@

LINUX_INPUT=@WCM_LINUX_INPUT@

if WCM_DLLOADER
DLLOADER=-fPIC
endif

CFLAGS += $(WCM_CFLAGS_STACK_PROTECTOR)

XF86OBJS = xf86Wacom.o wcmSerial.o wcmUSB.o wcmISDV4.o \
	wcmXCommand.o wcmCommon.o wcmCompat.o wcmConfig.o \
	wcmFilter.o

xf86Wacom.o: xf86Wacom.c wcmFilter.h ../include/Xwacom.h
wcmXCommand.o: wcmXCommand.c wcmFilter.h ../include/Xwacom.h
wcmSerial.o: wcmSerial.c wcmSerial.h wcmFilter.h
wcmISDV4.o: wcmISDV4.c wcmSerial.h wcmFilter.h
wcmUSB.o: wcmUSB.c wcmFilter.h
wcmCommon.o: wcmCommon.c
wcmCompat.o: wcmCompat.c
wcmConfig.o: wcmConfig.c wcmFilter.h 
wcmFilter.o: wcmFilter.c wcmFilter.h

$(XF86OBJS): xf86Wacom.h ../include/Xwacom.h Makefile
	$(CC) $(CFLAGS) $(DEPFLAGS) $(DLLOADER) -pipe -ansi \
		-pedantic -Wall -Wpointer-arith $(NO_MERGE_CONSTANTS) \
		-I$(srcdir) $(DRIVER_INCLUDES) -Dlinux \
		-D_POSIX_C_SOURCE=199309L -D_POSIX_SOURCE \
		-D_XOPEN_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_GNU_SOURCE \
		-DSHAPE -DXINPUT -DXKB -DLBX -DXAPPGROUP -DXCSECURITY \
		-DTOGCUP -DXF86BIGFONT -DDPMSExtension -DPIXPRIV \
		-DRENDER -DGCCUSESGAS -DAVOID_GLYPHBLT -DSINGLEDEPTH \
		-DXFreeXDGA -DXvExtension -DXFree86LOADER \
		-DXFree86Server -DXF86VIDMODE -DXvMCExtension \
		-DSMART_SCHEDULE -DBUILDDEBUG -DXResExtension \
		-DX_BYTE_ORDER=X_LITTLE_ENDIAN $(XSERVER64) \
		-DNDEBUG -DFUNCPROTO=15 \
		 $(XSERVER_CFLAGS) \
		-DNARROWPROTO -DIN_MODULE -DXFree86Module $(LINUX_INPUT) \
		-o $@ -c $(srcdir)/$(subst .o,.c,$@)

if WCM_DLLOADER
wacom_drv.so: $(XF86OBJS)
	$(CC) -shared -nostdlib -o wacom_drv.so $(XF86OBJS) -Bstatic -lgcc
else
wacom_drv.o: $(XF86OBJS)
	$(LD) -r $(XF86OBJS) -o wacom_drv.o
endif
