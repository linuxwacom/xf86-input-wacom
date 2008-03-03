noinst_SCRIPTS = @WCM_XF86MODULES@

x86moduledir = @WCM_MODDIR@
x86module_DATA = @WCM_XF86MODULES@

# These identify which programs, libraries, and headers could
# potentially be built or installed depending on the results of
# the configuration.
EXTRA_SCRIPTS = wacom_drv.o wacom_drv.so

EXTRA_DIST = xf86Wacom.c wcmXCommand.c xf86Wacom.h \
		wcmSerial.c wcmISDV4.c wcmUSB.c \
		wcmCommon.c wcmCompat.c wcmConfig.c \
		wcmFilter.c wcmFilter.h wcmSerial.h \
		../include/Xwacom.h

if WCM_ENV_XORGSDK
DRIVER_INCLUDES = -I../include -I$(WCM_XORGSDK_DIR) $(X_CFLAGS)
endif

if WCM_ENV_XFREE86
DRIVER_INCLUDES = \
	-I$(WCM_XFREE86_DIR)/programs/Xserver/hw/xfree86/common \
	-I$(WCM_XFREE86_DIR)/programs/Xserver/hw/xfree86/loader \
	-I$(WCM_XFREE86_DIR)/programs/Xserver/hw/xfree86/os-support \
	-I$(WCM_XFREE86_DIR)/programs/Xserver/include \
	-I$(WCM_XFREE86_DIR)/programs/Xserver/mi \
	-I$(WCM_XFREE86_DIR)/exports/include/X11 \
	-I$(WCM_XFREE86_DIR)/include/extensions \
	-I$(WCM_XFREE86_DIR) \
	-I$(WCM_XFREE86_DIR)/exports/include
endif

NO_MERGE_CONSTANTS=@WCM_NO_MERGE_CONSTANTS@
NO_STACK_PROTECTOR=@WCM_NO_STACK_PROTECTOR@
WCM_CLEVEL=@WCM_CLEVEL@

if WCM_DLLOADER
DLLOADER=-fPIC
endif

XF86OBJS = xf86Wacom.o wcmSerial.o wcmUSB.o wcmISDV4.o \
	wcmXCommand.o wcmCommon.o wcmCompat.o wcmConfig.o \
	wcmFilter.o

$(XF86OBJS):
	$(CC) $(CFLAGS) $(WCM_DEPFLAGS) $(DLLOADER) -pipe $(WCM_CLEVEL) \
		-pedantic -Wall -Wpointer-arith $(NO_MERGE_CONSTANTS) \
		$(NO_STACK_PROTECTOR) -I$(srcdir) $(DRIVER_INCLUDES) \
		$(WCM_XSERVER64) $(XSERVER_CFLAGS) \
		-o $@ -c $(srcdir)/$(subst .o,.c,$@)

if WCM_DLLOADER
wacom_drv.so: .depend $(XF86OBJS)
	$(CC) -shared -nostdlib -o wacom_drv.so $(XF86OBJS) -Bstatic -lgcc
else
wacom_drv.o: .depend $(XF86OBJS)
	$(LD) -r $(XF86OBJS) -o wacom_drv.o
endif

depend: .depend

.depend: Makefile
	@rm -f .depend
	$(CC) -MM $(CFLAGS) $(DEPFLAGS) $(DRIVER_INCLUDES) $(XF86OBJS:%.o=$(srcdir)/%.c) > .depend

-include .depend

