dnl Macros for configuring the Linux Wacom package
dnl
AC_DEFUN(AC_WCM_CHECK_ENVIRON,[
dnl Variables for various checks
WCM_ARCH=unknown
WCM_KERNEL=unknown
WCM_ISLINUX=no
WCM_OPTION_MODVER=no
WCM_ENV_KERNEL=no
WCM_KERNEL_WACOM_DEFAULT=no
WCM_ENV_XF86=no
WCM_LINUX_INPUT=
WCM_PATCH_WACDUMP=
WCM_PATCH_WACOMDRV=
dnl Check architecture
AC_MSG_CHECKING(for processor type)
WCM_ARCH=`uname -m`
AC_MSG_RESULT($WCM_ARCH)
dnl
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
[  --with-linux     Override linux kernel check],
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
AC_DEFUN(AC_WCM_CHECK_MODVER,[
dnl Guess modversioning
AC_MSG_CHECKING(for kernel module versioning)
kernelrel=`uname -r`
moddir="/lib/modules/$kernelrel/kernel/drivers/usb"

if test -f "$moddir/hid.o.gz"; then
	zcat $moddir/hid.o.gz >config.hid.o
	printk=`nm config.hid.o | grep printk`
	rm config.hid.o
elif test -f "$moddir/hid.o"; then
	printk=`nm $moddir/hid.o | grep printk`
else
	echo "***"; echo "*** WARNING:"
	echo "*** unable to find hid.o or hid.o.gz in kernel module"
	echo "*** directory.  Unable to determine kernel module versioning."
	echo "***"
	printk=""
fi

if test -n "$printk"; then
	printk=`echo "$printk" | grep printk_R`
	if test -n "$printk"; then
		WCM_OPTION_MODVER=yes
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi
else
	AC_MSG_RESULT([unknown; assuming no])
	WCM_OPTION_MODVER="unknown (assuming no)"
fi
])
AC_DEFUN(AC_WCM_CHECK_KERNELSOURCE,[
dnl Check for kernel build environment
AC_ARG_WITH(kernel,
[  --with-kernel=dir   Specify kernel source directory],
[
	WCM_KERNELDIR="$withval"
	AC_MSG_CHECKING(for valid kernel source tree)
	if test -f "$WCM_KERNELDIR/include/linux/input.h"; then
		AC_MSG_RESULT(ok)
		WCM_ENV_KERNEL=yes
	else
		AC_MSG_RESULT(missing input.h)
		AC_MSG_ERROR("Unable to find $WCM_KERNELDIR/include/linux/input.h")
	fi
],
[
	dnl guess directory
	AC_MSG_CHECKING(for kernel sources)
	WCM_KERNELDIR="/usr/src/linux-2.4"
	if test -f "$WCM_KERNELDIR/include/linux/input.h"; then
		WCM_ENV_KERNEL=yes
		AC_MSG_RESULT($WCM_KERNELDIR)
	else
		WCM_KERNELDIR="/usr/src/linux"
		if test -f "$WCM_KERNELDIR/include/linux/input.h"; then
			WCM_ENV_KERNEL=yes
			AC_MSG_RESULT($WCM_KERNELDIR)
		else
			AC_MSG_RESULT(not found)
			echo "***"
			echo "*** WARNING:"
			echo "*** Unable to guess kernel source directory"
			echo "*** Looked at /usr/src/linux-2.4"
			echo "*** Looked at /usr/src/linux"
			echo "*** Kernel modules will not be built"
			echo "***"
		fi
	fi
])])
AC_DEFUN(AC_WCM_CHECK_XFREE86SOURCE,[
dnl Check for XFree86 build environment
AC_ARG_WITH(xf86,
[  --with-xf86=dir   Specify XF86 build directory],
[
	WCM_XF86DIR="$withval";
	AC_MSG_CHECKING(for valid XFree86 build environment)
	if test -f $WCM_XF86DIR/$XF86SUBDIR/include/xf86Version.h; then
		WCM_ENV_XF86=yes
		AC_MSG_RESULT(ok)
	else
		AC_MSG_RESULT("xf86Version.h missing")
		AC_MSG_ERROR("Unable to find $WCM_XF86DIR/$XF86SUBDIR/include/xf86Version.h")
	fi
	WCM_XF86DIR=`(cd $WCM_XF86DIR; pwd)`
])])
