noinst_SCRIPTS = @WCM_MODULES@ 

EXTRA_SCRIPTS = wacom.o hid.o usbmouse.o evdev.o input.o mousedev.o

DEBUG_FLAGS = -D__JEJ_DEBUG
USBDIR=$(WCM_KERNEL_DIR)/drivers/usb

KCFLAGS = -Wall $(DEBUG_FLAGS) -D__KERNEL__ \
		-DMODULE -DEXPORT_SYMTAB $(WCM_MODVER) \
		-Wstrict-prototypes -Wno-trigraphs -O2 \
		-fno-strict-aliasing \
		-fno-common -fomit-frame-pointer -pipe \
		$(WCM_KSTACK) -march=$(WCM_ARCH)

NO_MERGE_CONSTANTS=@WCM_NO_MERGE_CONSTANTS@

wacom.o: wacom.c Makefile
	$(CC) -I$(WCM_KERNEL_DIR)/include $(KCFLAGS) \
		-DKBUILD_BASENAME=wacom -c -o wacom.o wacom.c

HID_OBJS = hid-core.o hiddev.o hid-input.o

hid.o: $(HID_OBJS) Makefile
	$(LD) -r -o $@ $(HID_OBJS)

hid-core.o: hid-core.c Makefile
	$(CC) -I$(WCM_KERNEL_DIR)/include -I$(USBDIR) $(KCFLAGS) \
		-DKBUILD_BASENAME=hid-core -c -o hid-core.o hid-core.c

hiddev.o: $(USBDIR)/hiddev.c Makefile
	$(CC) -I$(WCM_KERNEL_DIR)/include -I$(USBDIR) $(KCFLAGS) \
		-DKBUILD_BASENAME=hiddev -c -o hiddev.o hiddev.c

hid-input.o: $(USBDIR)/hid-input.c Makefile
	$(CC) -I$(WCM_KERNEL_DIR)/include -I$(USBDIR) $(KCFLAGS) \
		-DKBUILD_BASENAME=hid-input -c \
		-o hid-input.o hid-input.c

usbmouse.o: usbmouse.c Makefile
	$(CC) -I$(WCM_KERNEL_DIR)/include -I$(USBDIR) $(KCFLAGS) \
		-DKBUILD_BASENAME=usbmouse -c \
		-o usbmouse.o usbmouse.c

evdev.o: evdev.c Makefile
	$(CC) -I$(WCM_KERNEL_DIR)/include $(KCFLAGS) \
		-DKBUILD_BASENAME=evdev -c -o evdev.o evdev.c

mousedev.o: mousedev.c Makefile
	$(CC) -I$(WCM_KERNEL_DIR)/include $(KCFLAGS) \
		-DKBUILD_BASENAME=mousedev -c -o mousedev.o mousedev.c

input.o: input.c Makefile
	$(CC) -I$(WCM_KERNEL_DIR)/include $(KCFLAGS) \
		-DKBUILD_BASENAME=input -c -o input.o input.c

clean:
	rm -f *.o core *.core
