# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
#
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# Except as contained in this notice, the name of the copyright holders shall
# not be used in advertising or otherwise to promote the sale, use or
# other dealings in this Software without prior written authorization
# from the copyright holders.
#

drivermandir = $(DRIVER_MAN_DIR)
driverman_PRE = @DRIVER_NAME@.man
driverman_DATA = $(driverman_PRE:man=@DRIVER_MAN_SUFFIX@)

xsetwacommandir = $(APP_MAN_DIR)
xsetwacomman_PRE = xsetwacom.man
xsetwacomman_DATA = $(xsetwacomman_PRE:man=@APP_MAN_SUFFIX@)

isdv4debuggerdir = $(APP_MAN_DIR)
isdv4debugger_PRE = isdv4-serial-debugger.man
isdv4debugger_DATA = $(isdv4debugger_PRE:man=@APP_MAN_SUFFIX@)

isdv4inputattachdir = $(APP_MAN_DIR)
isdv4inputattach_PRE = isdv4-serial-inputattach.man
isdv4inputattach_DATA = $(isdv4inputattach_PRE:man=@APP_MAN_SUFFIX@)

EXTRA_DIST = @DRIVER_NAME@.man xsetwacom.man isdv4-serial-debugger.man isdv4-serial-inputattach.man

CLEANFILES = $(driverman_DATA) $(xsetwacomman_DATA) $(isdv4debugger_DATA) $(isdv4inputattach_DATA)

SUFFIXES = .$(DRIVER_MAN_SUFFIX) $(APP_MAN_SUFFIX) .man

# String replacements in MAN_SUBSTS now come from xorg-macros.m4 via configure
MAN_SUBSTS += \
         -e 's|\@VERSION\@|$(PACKAGE_NAME) $(PACKAGE_VERSION)|g'

.man.$(DRIVER_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@
.man.$(APP_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

