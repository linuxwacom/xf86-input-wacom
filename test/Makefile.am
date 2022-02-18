if UNITTESTS
include ../src/common.mk

check_PROGRAMS = wacom-tests
check_LTLIBRARIES = wacom_drv_test.la

wacom_drv_test_la_SOURCES = $(DRIVER_SOURCES) wacom-test-suite.c wacom-test-suite.h
wacom_drv_test_la_CFLAGS = $(AM_CFLAGS) $(XORG_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/src -DENABLE_TESTS -fvisibility=default
wacom_drv_test_la_LDFLAGS = -module -avoid-version -rpath $(abs_builddir)
wacom_drv_test_la_LIBADD = $(XORG_LIBS)

wacom_tests_LDADD = -ldl
wacom_tests_LDFLAGS = -rpath $(abs_builddir)/.libs
wacom_tests_CFLAGS = $(AM_CFLAGS) -DENABLE_TESTS
wacom_tests_SOURCES = wacom-tests.c

TESTS=$(check_PROGRAMS)

endif
