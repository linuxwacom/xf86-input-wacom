# shared makefile between src/Makefile.am and test/Makefile.am

DRIVER_SOURCES= \
	$(top_srcdir)/src/WacomInterface.h \
	$(top_srcdir)/src/x11/xf86Wacom.c \
	$(top_srcdir)/src/x11/xf86WacomProperties.c \
	$(top_srcdir)/src/xf86Wacom.h \
	$(top_srcdir)/src/wcmCommon.c \
	$(top_srcdir)/src/wcmConfig.c \
	$(top_srcdir)/src/wcmFilter.c \
	$(top_srcdir)/src/wcmFilter.h \
	$(top_srcdir)/src/xf86WacomDefs.h \
	$(top_srcdir)/src/wcmUSB.c \
	$(top_srcdir)/src/wcmValidateDevice.c \
	$(top_srcdir)/src/wcmTouchFilter.c \
	$(top_srcdir)/src/wcmTouchFilter.h
