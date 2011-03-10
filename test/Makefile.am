if UNITTESTS
include ../src/common.mk

check_PROGRAMS = wacom-tests
check_LTLIBRARIES = libwacom-test.la
libwacom_test_la_SOURCES =$(DRIVER_SOURCES)
libwacom_test_la_CFLAGS = -DDISABLE_STATIC -I$(top_srcdir)/src $(XORG_CFLAGS) $(CWARNFLAGS) -fvisibility=default

TESTS=$(check_PROGRAMS)

INCLUDES = -I$(top_srcdir)/src
AM_CFLAGS = $(GLIB_CFLAGS) $(XORG_CFLAGS) $(CWARNFLAGS)
TEST_LDADD=$(GLIB_LIBS) libwacom-test.la
COMMON_SOURCES=fake-symbols.c fake-symbols.h

wacom_tests_LDADD=$(TEST_LDADD)
wacom_tests_SOURCES=wacom-tests.c $(COMMON_SOURCES)

endif
