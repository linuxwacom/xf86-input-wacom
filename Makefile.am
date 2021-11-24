#  Copyright 2005 Adam Jackson.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# Provide an sdk location that is known to be writable
DISTCHECK_CONFIGURE_FLAGS = --with-sdkdir='$${includedir}/xorg' \
			    --with-xorg-conf-dir='$${datadir}/X11/xorg.conf.d'

SUBDIRS = conf src man include tools test
MAINTAINERCLEANFILES = ChangeLog INSTALL
BUILT_SOURCES = config-ver.h
CLEANFILES = config-ver.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xorg-wacom.pc

EXTRA_DIST = GPL \
             version \
             config-ver.h.in

.PHONY: ChangeLog INSTALL FORCE

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL

config-ver.h: config-ver.h.in version
	sed -e 's/@GITVERSION@/$$(cat version)/' < $< > $@

version: FORCE
	if test -x $(srcdir)/git-version-gen; then $(srcdir)/git-version-gen -u; fi
