bin_PROGRAMS = @WCM_PROGS@
lib_LTLIBRARIES = libwacomcfg.la

wacomcfgdir = $(includedir)/wacomcfg
wacomcfg_HEADERS = wacomcfg.h

if WCM_ENV_XORGSDK
WACOMCFG_INCLUDES = -I$(WCM_XORGSDK_DIR) $(X_CFLAGS)
endif

if WCM_ENV_XFREE86
WACOMCFG_INCLUDES = -I$(WCM_XFREE86_DIR) $(HAL_CFLAGS)
endif

AM_CFLAGS = -Wall -pedantic $(WACOMCFG_INCLUDES)

# These identify which programs, libraries, and headers could
# potentially be built or installed depending on the results of
# the configuration.
EXTRA_PROGRAMS = wacdump xidump xsetwacom 
if WCM_HAVE_HAL
libexec_PROGRAMS = hal-setup-wacom
endif

# Source dependencies
wacdump_SOURCES = wacdump.c wacscrn.c wacscrn.h \
		wactablet.c wactablet.h \
		wacserial.c wacserial.h \
		wacusb.c wacusb.h 
if WCM_ENV_NCURSES
wacdump_LDADD = -lncurses
endif

xidump_SOURCES = xidump.c wacscrn.c wacscrn.h
xidump_LDFLAGS = @WCM_XIDUMP_LIBS@
if WCM_ENV_NCURSES
xidump_LDADD = -lncurses
endif

libwacomcfg_la_SOURCES = wacomcfg.c wacomcfg.h ../include/Xwacom.h
libwacomcfg_la_LDFLAGS = -no-undefined -version-info @WCM_LIBWACOMCFG_VER@
libwacomcfg_la_LIBADD = @WCM_LIBWACOMCFG_LIBS@

xsetwacom_SOURCES = xsetwacom.c wacomcfg.h wcmAction.c wcmAction.h ../include/Xwacom.h
xsetwacom_LDADD = libwacomcfg.la

if WCM_HAVE_HAL
hal_setup_wacom_SOURCES = hal-setup-wacom.c
hal_setup_wacom_LDADD = $(HAL_LIBS)
endif


