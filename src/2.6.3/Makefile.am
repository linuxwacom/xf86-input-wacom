noinst_SCRIPTS = @WCM_MODULES@

EXTRA_SCRIPTS = wacom.o hid.o mousedev.o usbmouse.o evdev.o

KERNEL_DIR=@WCM_KERNELDIR@

WCM_WACOM_DIR=@WCM_LINUXWACOMDIR@

wacom.o :
	if test ! -f $(WCM_WACOM_DIR)/src/2.6.3/wacom.o; then \
		cd $(KERNEL_DIR); make; cd $(WCM_WACOM_DIR)/src/2.6.3; \
		cp -f $(KERNEL_DIR)/drivers/usb/input/wacom.o .; \
	fi
hid.o :
	if test ! -f $(WCM_WACOM_DIR)/src/2.6.3/wacom.o; then \
		cd $(KERNEL_DIR); make; cd $(WCM_WACOM_DIR)/src/2.6.3; \
		cp -f $(KERNEL_DIR)/drivers/usb/input/wacom.o .; \
	fi
usbmouse.o :
	if test ! -f $(WCM_WACOM_DIR)/src/2.6.3/wacom.o; then \
		cd $(KERNEL_DIR); make; cd $(WCM_WACOM_DIR)/src/2.6.3; \
		cp -f $(KERNEL_DIR)/drivers/usb/input/wacom.o .; \
	fi

mousedev.o :
	if test ! -f $(WCM_WACOM_DIR)/src/2.6.3/wacom.o; then \
		cd $(KERNEL_DIR); make; cd $(WCM_WACOM_DIR)/src/2.6.3; \
		cp -f $(KERNEL_DIR)/drivers/usb/input/wacom.o .; \
	fi

evdev.o :
	if test ! -f $(WCM_WACOM_DIR)/src/2.6.3/wacom.o; then \
		cd $(KERNEL_DIR); make; cd $(WCM_WACOM_DIR)/src/2.6.3; \
		cp -f $(KERNEL_DIR)/drivers/usb/input/wacom.o .; \
	fi

clean:
	rm -f *.o core *.core