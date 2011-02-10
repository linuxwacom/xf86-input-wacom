if UNITTESTS
include ../src/Makefile.am

check_PROGRAMS = wacom-tests
check_LTLIBRARRIES = @DRIVER_NAME@_drv.la
@DRIVER_NAME@_drv_la_CFLAGS = -DDISABLE_STATIC -I$(top_srcdir)/src $(XORG_CFLAGS) $(CWARNFLAGS) -fvisibility=default

TESTS=$(check_PROGRAMS)

INCLUDES = -I$(top_srcdir)/src
AM_CFLAGS += $(GLIB_CFLAGS)
TEST_LDADD=$(GLIB_LIBS) @DRIVER_NAME@_drv.la
COMMON_SOURCES=fake-symbols.c fake-symbols.h

wacom_tests_LDADD=$(TEST_LDADD)
wacom_tests_SOURCES=wacom-tests.c $(COMMON_SOURCES)

endif
