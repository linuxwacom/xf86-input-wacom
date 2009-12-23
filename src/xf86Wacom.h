/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2009 by Ping Cheng, Wacom Technology. <pingc@wacom.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __XF86_XF86WACOM_H
#define __XF86_XF86WACOM_H

#include <xorg-server.h>
#include <xorgVersion.h>

#include "Xwacom.h"

/*****************************************************************************
 * Linux Input Support
 ****************************************************************************/

#include <asm/types.h>
#include <linux/input.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

#define MAX_USB_EVENTS 32

/* max number of input events to read in one read call */
#define MAX_EVENTS 50

#include <misc.h>
#define inline __inline__
#include <xf86.h>
#include <xisb.h>
#include <string.h>
#include <errno.h>

#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <exevents.h>           /* Needed for InitValuator/Proximity stuff */
#include <X11/keysym.h>
#include <mipointer.h>
#include <fcntl.h>

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3
# include <X11/Xatom.h>
#endif

/*****************************************************************************
 * QNX support
 ****************************************************************************/

#if defined(__QNX__) || defined(__QNXNTO__)
#define POSIX_TTY
#endif

/******************************************************************************
 * Debugging support
 *****************************************************************************/

#ifdef DBG
#undef DBG
#endif

#if DEBUG
#define DBG(lvl, priv, ...) \
	do { \
		if ((lvl) <= priv->debugLevel) { \
			xf86Msg(X_INFO, "%s: ", __func__); \
			xf86Msg(X_NONE, __VA_ARGS__); \
		} \
	} while (0)
#else
#define DBG(lvl, priv, ...)
#endif

/*****************************************************************************
 * General Macros
 ****************************************************************************/

#define ABS(x) ((x) > 0 ? (x) : -(x))

/*****************************************************************************
 * General Defines
 ****************************************************************************/
#define XI_STYLUS "STYLUS"      /* X device name for the stylus */
#define XI_CURSOR "CURSOR"      /* X device name for the cursor */
#define XI_ERASER "ERASER"      /* X device name for the eraser */
#define XI_PAD    "PAD"         /* X device name for the Pad */
#define XI_TOUCH  "TOUCH"       /* X device name for the touch */

/* packet length for individual models */
#define WACOM_PKGLEN_TOUCH93    5
#define WACOM_PKGLEN_TOUCH9A    7
#define WACOM_PKGLEN_TPCPEN     9
#define WACOM_PKGLEN_TPCCTL     11
#define WACOM_PKGLEN_TOUCH2FG   13

/******************************************************************************
 * WacomModule - all globals are packed in a single structure to keep the
 *               global namespaces as clean as possible.
 *****************************************************************************/
typedef struct _WacomModule WacomModule;

struct _WacomModule
{
	InputDriverPtr wcmDrv;

	int (*DevOpen)(DeviceIntPtr pWcm);
	void (*DevReadInput)(LocalDevicePtr local);
	void (*DevControlProc)(DeviceIntPtr device, PtrCtrl* ctrl);
	int (*DevChangeControl)(LocalDevicePtr local, xDeviceCtl* control);
	void (*DevClose)(LocalDevicePtr local);
	int (*DevProc)(DeviceIntPtr pWcm, int what);
	int (*DevSwitchMode)(ClientPtr client, DeviceIntPtr dev, int mode);
	Bool (*DevConvert)(LocalDevicePtr local, int first, int num,
		int v0, int v1, int v2, int v3, int v4, int v5, int* x, int* y);
	Bool (*DevReverseConvert)(LocalDevicePtr local, int x, int y,
		int* valuators);
};

	extern WacomModule gWacomModule;

/* The rest are defined in a separate .h-file */
#include "xf86WacomDefs.h"

/*****************************************************************************
 * General Inlined functions and Prototypes
 ****************************************************************************/
/* BIG HAIRY WARNING:
 * Don't overuse SYSCALL(): use it ONLY when you call low-level functions such
 * as ioctl(), read(), write() and such. Otherwise you can easily lock up X11,
 * for example: you pull out the USB tablet, the handle becomes invalid,
 * xf86WcmRead() returns -1 AND errno is left as EINTR from hell knows where.
 * Then you'll loop forever, and even Ctrl+Alt+Backspace doesn't help.
 * xf86WcmReadSerial, WriteSerial, CloseSerial & company already use SYSCALL()
 * internally; there's no need to duplicate it outside the call.
 */
#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

#define RESET_RELATIVE(ds) do { (ds).relwheel = 0; } while (0)

/* device autoprobing */
char *wcmEventAutoDevProbe (LocalDevicePtr local);

/* common tablet initialization regime */
int xf86WcmInitTablet(LocalDevicePtr local, const char* id, float version);

/* standard packet handler */
void wcmReadPacket(LocalDevicePtr local);

/* handles suppression, filtering, and dispatch. */
void wcmEvent(WacomCommonPtr common, unsigned int channel, const WacomDeviceState* ds);

/* dispatches data to XInput event system */
void xf86WcmSendEvents(LocalDevicePtr local, const WacomDeviceState* ds);

/* generic area check for xf86Wacom.c, wcmCommon.c and wcmXCommand.c */
Bool WcmPointInArea(WacomToolAreaPtr area, int x, int y);
Bool WcmAreaListOverlap(WacomToolAreaPtr area, WacomToolAreaPtr list);

/* Change pad's mode according to it core event status */
int xf86WcmSetPadCoreMode(LocalDevicePtr local);

/* calculate the proper tablet to screen mapping factor */
void xf86WcmMappingFactor(LocalDevicePtr local);

/****************************************************************************/
#endif /* __XF86WACOM_H */
