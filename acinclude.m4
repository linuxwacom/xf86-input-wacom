dnl Macros for configuring the Linux Wacom package
dnl
AC_DEFUN([AC_WCM_SET_PATHS],[
if test "x$prefix" = xNONE
then WCM_PREFIX=$ac_default_prefix
else WCM_PREFIX=$prefix
fi
if test "x$exec_prefix" = xNONE
then WCM_EXECDIR=$WCM_PREFIX
else WCM_EXECDIR=$exec_prefix
fi
])
AC_DEFUN([AC_WCM_CHECK_ENVIRON],[
dnl Variables for various checks
WCM_KSTACK=-mpreferred-stack-boundary=2
WCM_KERNEL=unknown
WCM_KERNEL_VER=2.4
WCM_ISLINUX=no
WCM_ENV_KERNEL=no
WCM_OPTION_MODVER=no
WCM_KERNEL_WACOM_DEFAULT=no
WCM_ENV_XF86=no
WCM_ENV_XORGSDK=no
WCM_LINUX_INPUT=
WCM_PATCH_WACDUMP=
WCM_PATCH_WACOMDRV=
WCM_ENV_TCL=no
WCM_ENV_TK=no
WCM_XIDUMP_DEFAULT=yes
WCM_ENV_XLIB=no
dnl Check architecture
AC_MSG_CHECKING(for arch type)
AC_ARG_WITH(arch,
AC_HELP_STRING([--with-arch], [Use specified architecture]),
[ WCM_ARCH=$withval 
],
[
	dnl Try the compiler for the build arch first.
	dnl We may be cross compiling or building for
	dnl a 32bit system with a 64 bit kernel etc.
	WCM_ARCH=`$CC -dumpmachine 2> /dev/null`
	test $? = 0 || WCM_ARCH=`uname -m`
])
AC_MSG_RESULT($WCM_ARCH)

dnl Check for X server bit
AC_ARG_ENABLE(xserver64,
AC_HELP_STRING([--enable-xserver64], [Use specified X server bit [[default=usually]]]),
[ WCM_OPTION_XSERVER64=$enableval 
],
[
	if test "$WCM_OPTION_XSERVER64" = ""; then
		if test `uname -a | grep -c "64"` = 0; then
			WCM_OPTION_XSERVER64=no
		else
			WCM_OPTION_XSERVER64=yes
		fi
	fi
])

WCM_XLIBDIR_DEFAULT=/usr/X11R6/lib
WCM_XLIBDIR_DEFAULT2=/usr/lib
if test "$WCM_OPTION_XSERVER64" = "yes"; then
	CFLAGS="$CFLAGS -D__amd64__"
	WCM_XSERVER64="-D_XSERVER64"
	if test `uname -m | grep -c "x86_64"` != 0; then
		WCM_KSTACK="-mpreferred-stack-boundary=4 -mcmodel=kernel"
	fi
	WCM_XLIBDIR_DEFAULT=/usr/X11R6/lib64
	if test -d /usr/lib64; then
		WCM_XLIBDIR_DEFAULT2=/usr/lib64
	fi
fi
if test -f "$WCM_XLIBDIR_DEFAULT/Server/include/xf86Version.h" || 
test -f "$WCM_XLIBDIR_DEFAULT/Server/xf86Version.h"; then
	WCM_XORGSDK_DEFAULT=$WCM_XLIBDIR_DEFAULT/Server
else
	WCM_XORGSDK_DEFAULT=/usr
fi

WCM_TCLTKDIR_DEFAULT=/usr
XF86SUBDIR=programs/Xserver/hw/xfree86
WCM_LINUXWACOMDIR=`pwd`
WCM_ENV_NCURSES=no
dnl Check kernel type
AC_MSG_CHECKING(for kernel type)
WCM_KERNEL=`uname -s`
AC_MSG_RESULT($WCM_KERNEL)
dnl
dnl Check for linux
AC_MSG_CHECKING(for linux-based kernel)
islinux=`echo $WCM_KERNEL | grep -i linux | wc -l`
if test $islinux != 0; then
	WCM_ISLINUX=yes
fi
AC_MSG_RESULT($WCM_ISLINUX)
if test x$WCM_ISLINUX != xyes; then
	echo "***"
	echo "*** WARNING:"
	echo "*** Linux kernel not detected; linux-specific features will not"
	echo "*** be built including USB support in XFree86 and kernel drivers."
	echo "***"
fi
dnl
dnl Check for linux kernel override
AC_ARG_WITH(linux,
AS_HELP_STRING([--with-linux], [Override linux kernel check]),
[ WCM_ISLINUX=$withval ])
dnl
dnl Handle linux specific features
if test x$WCM_ISLINUX = xyes; then
	WCM_KERNEL_WACOM_DEFAULT=yes
	WCM_LINUX_INPUT="-DLINUX_INPUT"
	AC_DEFINE(WCM_ENABLE_LINUXINPUT,1,[Enable the Linux Input subsystem])
else
	WCM_PATCH_WACDUMP="(no USB) $WCM_PATCH_WACDUMP"
	WCM_PATCH_WACOMDRV="(no USB) $WCM_PATCH_WACOMDRV"
fi
])
dnl
dnl
dnl
AC_DEFUN([AC_WCM_CHECK_KERNELSOURCE],[
dnl Check for kernel build environment
AC_ARG_WITH(kernel,
AS_HELP_STRING([--with-kernel=dir], [Specify kernel source directory]),
[
	WCM_KERNELDIR="$withval"
	AC_MSG_CHECKING(for valid kernel source tree)
	if test -f "$WCM_KERNELDIR/include/linux/input.h"; then
		AC_MSG_RESULT(ok)
		WCM_ENV_KERNEL=yes
	else
		AC_MSG_RESULT(missing input.h)
		AC_MSG_RESULT("Unable to find $WCM_KERNELDIR/include/linux/input.h")
		WCM_ENV_KERNEL=no
	fi
],
[
	dnl guess directory
	AC_MSG_CHECKING(for kernel sources)
	WCM_KERNELDIR="/usr/src/linux /usr/src/linux-`uname -r` /usr/src/linux-2.4 /usr/src/linux-2.6 /lib/modules/`uname -r`/build"

	for i in $WCM_KERNELDIR; do
		if test -f "$i/include/linux/input.h"; then
			WCM_ENV_KERNEL=yes
			WCM_KERNELDIR=$i
			break
		fi
	done

	if test "x$WCM_ENV_KERNEL" = "xyes"; then
		WCM_ENV_KERNEL=yes
		AC_MSG_RESULT($WCM_KERNELDIR)
	else
		AC_MSG_RESULT(not found)
		WCM_KERNELDIR=""
		WCM_ENV_KERNEL=no
		echo "***"
		echo "*** WARNING:"
		echo "*** Unable to guess kernel source directory"
		echo "*** Looked at /usr/src/linux"
		echo "*** Looked at /usr/src/linux-`uname -r`"
		echo "*** Looked at /usr/src/linux-2.4"
		echo "*** Looked at /usr/src/linux-2.6"
		echo "*** Looked at /lib/modules/`uname -r`/build"
		echo "*** Kernel modules will not be built"
		echo "***"
	fi
])])
dnl
AC_DEFUN([AC_WCM_CHECK_MODVER],[
dnl Guess modversioning
if test x$WCM_ENV_KERNEL = xyes; then
	AC_MSG_CHECKING(for kernel module versioning)
	UTS_PATH=""
	MODUTS=""
	if test -f "$WCM_KERNELDIR/include/linux/version.h"; then
		UTS_PATH="$WCM_KERNELDIR/include/linux/version.h"
		MODUTS=`grep UTS_RELEASE $UTS_PATH`
	fi
	if test -f "$WCM_KERNELDIR/include/linux/utsrelease.h" && test "x$MODUTS" = x; then
		UTS_PATH="$WCM_KERNELDIR/include/linux/utsrelease.h"
		MODUTS=`grep UTS_RELEASE $UTS_PATH`
	fi
	if test "x$MODUTS" != x; then
		WCM_OPTION_MODVER=yes
		AC_MSG_RESULT(yes)
		ISVER=`echo $MODUTS | grep -c "\"2.4"` 
		if test "$ISVER" -gt 0; then
			MINOR=`echo $MODUTS | cut -f 1 -d- | cut -f3 -d. | cut -f1 -d\" | sed 's/\([[0-9]]*\).*/\1/'`
			if test $MINOR -ge 22; then
				WCM_KERNEL_VER="2.4.22"
			else
				WCM_KERNEL_VER="2.4"
			fi
		else
			ISVER=`echo $MODUTS | grep -c "2.6"` 
			if test "$ISVER" -gt 0; then
				MINOR=`echo $MODUTS | cut -f 1 -d- | cut -f3 -d. | cut -f1 -d\" | sed 's/\([[0-9]]*\).*/\1/'`
				if test $MINOR -ge 18; then
					WCM_KERNEL_VER="2.6.18"
				elif test $MINOR -eq 17; then
					WCM_KERNEL_VER="2.6.16"
				elif test $MINOR -eq 16; then
					WCM_KERNEL_VER="2.6.16"
				elif test $MINOR -eq 15; then
					WCM_KERNEL_VER="2.6.15"
				elif test $MINOR -eq 14; then
					WCM_KERNEL_VER="2.6.14"
				elif test $MINOR -eq 13; then
					WCM_KERNEL_VER="2.6.13"
				elif test $MINOR -eq 12; then
					WCM_KERNEL_VER="2.6.11"
				elif test $MINOR -eq 11; then
					WCM_KERNEL_VER="2.6.11"
				elif test $MINOR -eq 10; then
					WCM_KERNEL_VER="2.6.10"
				elif test $MINOR -eq 9; then
					WCM_KERNEL_VER="2.6.9"
				elif test $MINOR -eq 8; then
					WCM_KERNEL_VER="2.6.8"
				else
					WCM_KERNEL_VER="2.6"
				fi
			else
				echo "***"
				echo "*** WARNING:"
				echo "*** $MODUTS is not supported by this package"
				echo "*** Kernel modules will not be built"
				echo "***"
				WCM_OPTION_MODVER=no
				AC_MSG_RESULT(no)
				WCM_ENV_KERNEL=no
			fi
		fi
	else
		echo "***"
		echo "*** WARNING:"
		echo "*** Can not identify your kernel source version"
		echo "*** Kernel modules will not be built"
		echo "***"
		WCM_KERNELDIR=""
		WCM_ENV_KERNEL=no
		WCM_OPTION_MODVER=no
		AC_MSG_RESULT(no)
		
	fi
fi
])
dnl
AC_DEFUN([AC_WCM_CHECK_XORG_SDK],[
dnl Check for X11 sdk environment
dnl handle default case
AC_ARG_WITH(xorg-sdk,
AS_HELP_STRING([--with-xorg-sdk=dir], [Specify Xorg SDK directory]),
[ WCM_XORGSDK="$withval"; ])
if test x$WCM_ENV_XF86 != xyes; then
        dnl handle default case
	if test "$WCM_XORGSDK" = "yes" || test "$WCM_XORGSDK" == ""; then
		WCM_XORGSDK=$WCM_XORGSDK_DEFAULT
	fi
	if test -n "$WCM_XORGSDK"; then
		AC_MSG_CHECKING(for valid Xorg SDK)
		if test -f $WCM_XORGSDK/include/xf86Version.h; then
			WCM_XORGSDK=$WCM_XORGSDK/include
			WCM_ENV_XORGSDK=yes
			AC_MSG_RESULT(ok)
		elif test -f $WCM_XORGSDK/include/xorg/xf86Version.h; then
			WCM_ENV_XORGSDK=yes
			WCM_XORGSDK=$WCM_XORGSDK/include/xorg
			AC_MSG_RESULT(ok)
		elif test -f $WCM_XORGSDK/xc/include/xf86Version.h; then
			WCM_ENV_XORGSDK=yes
			WCM_XORGSDK=$WCM_XORGSDK/xc/include
			AC_MSG_RESULT(ok)
		else
			WCM_ENV_XORGSDK=no
			AC_MSG_RESULT("xf86Version.h missing")
			AC_MSG_RESULT([Tried $WCM_XORGSDK/include, $WCM_XORGSDK/include/xorg and $WCM_XORGSDK/xc/include])
		fi
	fi
fi
AM_CONDITIONAL(WCM_ENV_XORGSDK, [test x$WCM_ENV_XORGSDK = xyes])
])
AC_DEFUN([AC_WCM_CHECK_XSOURCE],[
dnl Check for X build environment
if test -d x-includes; then
	WCM_XF86DIR=x-includes
fi
AC_ARG_WITH(x-src,
AS_HELP_STRING([--with-x-src=dir], [Specify X driver build directory]),
[ WCM_XF86DIR="$withval"; ])
if test -n "$WCM_XF86DIR"; then
	AC_MSG_CHECKING(for valid XFree86/X.org build environment)
	if test -f $WCM_XF86DIR/xc/$XF86SUBDIR/xf86Version.h; then
		WCM_ENV_XF86=yes
		WCM_XF86DIR="$WCM_XF86DIR/xc"
		AC_MSG_RESULT(ok)
	elif test -f $WCM_XF86DIR/$XF86SUBDIR/xf86Version.h; then
		WCM_ENV_XF86=yes
		AC_MSG_RESULT(ok)
	else
		WCM_ENV_XF86=no
		AC_MSG_RESULT(xf86Version.h missing)
		AC_MSG_RESULT(Tried $WCM_XF86DIR/$XF86SUBDIR and $WCM_XF86DIR/xc/$XF86SUBDIR)
	fi
fi
AM_CONDITIONAL(WCM_ENV_XF86, [test x$WCM_ENV_XF86 = xyes])
])
AC_DEFUN([AC_WCM_CHECK_XLIB],[
dnl Check for XLib development environment
WCM_XLIBDIR=
AC_ARG_WITH(xlib,
AS_HELP_STRING([--with-xlib=dir], [uses a specified X11R6 directory]),
[WCM_XLIBDIR=$withval])

dnl handle default case
AC_MSG_CHECKING(for X lib directory)
if test "$WCM_XLIBDIR" == "" || test "$WCM_XLIBDIR" == "yes"; then
	if test -f $WCM_XLIBDIR_DEFAULT2/libX11.so; then
		WCM_ENV_XLIB=yes
		WCM_XLIBDIR=$WCM_XLIBDIR_DEFAULT2
		AC_MSG_RESULT(found)
	elif test -d $WCM_XLIBDIR_DEFAULT/X11 ||
		test -d $WCM_XLIBDIR_DEFAULT; then
		WCM_ENV_XLIB=yes
		WCM_XLIBDIR=$WCM_XLIBDIR_DEFAULT
		AC_MSG_RESULT(found)
	else
		AC_MSG_RESULT([not found, tried $WCM_XLIBDIR_DEFAULT/X11 and $WCM_XLIBDIR_DEFAULT2])
		WCM_ENV_XLIB=no
	fi
elif test -d $WCM_XLIBDIR; then
	WCM_ENV_XLIB=yes
	AC_MSG_RESULT(found)
else
	AC_MSG_RESULT([not found, tried $WCM_XLIBDIR])
	WCM_ENV_XLIB=no
fi
])
AC_DEFUN([AC_WCM_CHECK_TCL],[
dnl Check for TCL development environment
WCM_TCLDIR=
AC_ARG_WITH(tcl, 
AS_HELP_STRING([--with-tcl=dir], [uses a specified tcl directory  ]),
[ WCM_TCLDIR=$withval ])

dnl get tcl version
AC_PATH_PROG([TCLSH],[tclsh],[no])
if test "x$TCLSH" != "xno"; then
	AC_MSG_CHECKING([for tcl version])
	version=$(echo ["puts [set tcl_version]"] | $TCLSH)
	AC_MSG_RESULT([$version])
fi

dnl handle default case
if test "$WCM_TCLDIR" = "yes" || test "$WCM_TCLDIR" == ""; then
	AC_MSG_CHECKING([for tcl header files])
	dir="$WCM_TCLTKDIR_DEFAULT/include";
	for i in "" tcl/ "tcl$version/"; do
		if test "x$WCM_ENV_TCL" != "xyes"; then
			if test -f "$dir/$i/tcl.h"; then
				AC_MSG_RESULT([$dir/$i])
				WCM_ENV_TCL=yes
				WCM_TCLDIR="$dir/$i"
				CFLAGS="$CFLAGS -I$WCM_TCLDIR"
			fi
		fi
	done
	if test "x$WCM_ENV_TCL" != "xyes"; then
		AC_MSG_RESULT([not found; tried $WCM_TCLTKDIR_DEFAULT/include, tcl, and "tcl$version"; ])		
		echo "***"; echo "*** WARNING:"
		echo "*** The tcl development environment does not appear to"
		echo "*** be installed. The header file tcl.h does not appear"
		echo "*** in the include path. Do you have the tcl rpm or"
		echo "*** equivalent package properly installed?  Some build"
		echo "*** features will be unavailable."
		echo "***"
	fi

dnl handle specified case
elif test "$WCM_TCLDIR" != "no"; then
	AC_MSG_CHECKING([for tcl header files])
	if test -f "$WCM_TCLDIR/include/tcl.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TCL=yes
		if test "$WCM_TCLDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TCLDIR/include"
		fi
	elif test -f "$WCM_TCLDIR/tcl.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TCL=yes
		if test "$WCM_TCLDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TCLDIR"
		fi
	else
		AC_MSG_RESULT([not found; tried $WCM_TCLDIR/include/tcl.h and $WCM_TCLDIR/tcl.h])
		echo "***"; echo "*** WARNING:"
		echo "*** The tcl development environment does not appear to"
		echo "*** be installed. The header file tcl.h does not appear"
		echo "*** in the include path. Do you have the tcl rpm or"
		echo "*** equivalent package properly installed?  Some build"
		echo "*** features will be unavailable."
		echo "***"
	fi
fi
])
AC_DEFUN([AC_WCM_CHECK_TK],[
dnl Check for TK development environment
WCM_TKDIR=
AC_ARG_WITH(tk,
AS_HELP_STRING([--with-tk=dir], [uses a specified tk directory  ]), 
[ WCM_TKDIR=$withval ])

dnl handle default case
if test "$WCM_TKDIR" = "yes" || test "$WCM_TKDIR" == ""; then
	AC_MSG_CHECKING([for tk header files])
	if test -f "$WCM_TCLTKDIR_DEFAULT/include/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		WCM_TKDIR="$WCM_TCLTKDIR_DEFAULT/include"
	elif test -f "$WCM_TCLDIR/include/tk.h" || test -f "$WCM_TCLDIR/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		WCM_TKDIR="$WCM_TCLDIR"
	else
		AC_MSG_RESULT([not found; tried $WCM_TCLTKDIR_DEFAULT/include and $WCM_TCLDIR/include])
		echo "***"; echo "*** WARNING:"
		echo "*** The tk development environment does not appear to"
		echo "*** be installed. The header file tk.h does not appear"
		echo "*** in the include path. Do you have the tk rpm or"
		echo "*** equivalent package properly installed?  Some build"
		echo "*** features will be unavailable."
		echo "***"
	fi
dnl handle specified case
elif test "$WCM_TKDIR" != "no"; then
	AC_MSG_CHECKING([for tk header files])
	if test -f "$WCM_TKDIR/include/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		if test "$WCM_TCLDIR" != "$WCM_TKDIR" && test "$WCM_TKDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TKDIR/include"
		fi
	elif test -f "$WCM_TKDIR/tk.h"; then
		AC_MSG_RESULT(found)
		WCM_ENV_TK=yes
		if test "$WCM_TCLDIR" != "$WCM_TKDIR" && test "$WCM_TKDIR" != "/usr"; then
			CFLAGS="$CFLAGS -I$WCM_TKDIR"
		fi
	else
		AC_MSG_RESULT([not found; tried $WCM_TKDIR/include/tk.h and $WCM_TKDIR/tk.h])
		echo "***"; echo "*** WARNING:"
		echo "*** The tk library does not appear to be installed."
		echo "*** Do you have the tk rpm or equivalent package properly"
		echo "*** installed?  Some build features will be unavailable."
		echo "***"
	fi
fi
])
AC_DEFUN([AC_WCM_CHECK_NCURSES],[
dnl Check for ncurses development environment
AC_CHECK_HEADER(ncurses.h, [WCM_ENV_NCURSES=yes])
if test x$WCM_ENV_NCURSES != xyes; then
	AC_CHECK_HEADER(ncurses/ncurses.h, [WCM_ENV_NCURSES=yes])
fi
if test x$WCM_ENV_NCURSES != xyes; then
	echo "***"; echo "*** WARNING:"
	echo "*** The ncurses development library does not appear to be installed."
	echo "*** The header file ncurses.h does not appear in the include path."
	echo "*** Do you have the ncurses-devel rpm or equivalent package"
	echo "*** properly installed?  Some build features will be unavailable."
	echo "***"
	AC_DEFINE(WCM_ENABLE_NCURSES,0,[ncurses header files available])
else
	AC_DEFINE(WCM_ENABLE_NCURSES,1,[ncurses header files available])
fi
AM_CONDITIONAL(WCM_ENV_NCURSES, [test x$WCM_ENV_NCURSES = xyes])
])
