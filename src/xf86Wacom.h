/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2010 by Ping Cheng, Wacom. <pingc@wacom.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __XF86_XF86WACOM_H
#define __XF86_XF86WACOM_H

#include <xorg-server.h>
#include <xorgVersion.h>

#include <wacom-util.h>
#include "Xwacom.h"

/* max number of input events to read in one read call */
#define MAX_EVENTS 50

#define inline __inline__
#include <xf86.h>
#include <string.h>
#include <errno.h>

#include <xf86Xinput.h>
#include <mipointer.h>
#include <X11/Xatom.h>
/*****************************************************************************
 * Unit test hack
 ****************************************************************************/
#ifdef DISABLE_STATIC
#define static
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
			xf86Msg(X_INFO, "%s (%d:%s): ", \
				((WacomDeviceRec*)priv)->name, lvl, __func__); \
			xf86Msg(X_NONE, __VA_ARGS__); \
		} \
	} while (0)
#else
#define DBG(lvl, priv, ...)
#endif

/******************************************************************************
 * WacomModule - all globals are packed in a single structure to keep the
 *               global namespaces as clean as possible.
 *****************************************************************************/
typedef struct _WacomModule WacomModule;

struct _WacomModule
{
	InputDriverPtr wcmDrv;

	int (*DevOpen)(DeviceIntPtr pWcm);
	void (*DevReadInput)(InputInfoPtr pInfo);
	void (*DevControlProc)(DeviceIntPtr device, PtrCtrl* ctrl);
	int (*DevChangeControl)(InputInfoPtr pInfo, xDeviceCtl* control);
	void (*DevClose)(InputInfoPtr pInfo);
	int (*DevProc)(DeviceIntPtr pWcm, int what);
	int (*DevSwitchMode)(ClientPtr client, DeviceIntPtr dev, int mode);
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
 * xf86ReadSerial() returns -1 AND errno is left as EINTR from hell knows where.
 * Then you'll loop forever, and even Ctrl+Alt+Backspace doesn't help.
 * xf86ReadSerial, WriteSerial, CloseSerial & company already use SYSCALL()
 * internally; there's no need to duplicate it outside the call.
 */
#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

/* Open the device with the right serial parmeters */
extern Bool wcmOpen(InputInfoPtr pInfo);

/* device autoprobing */
char *wcmEventAutoDevProbe (InputInfoPtr pInfo);

/* common tablet initialization regime */
int wcmInitTablet(InputInfoPtr pInfo, const char* id, float version);

/* standard packet handler */
void wcmReadPacket(InputInfoPtr pInfo);

/* handles suppression, filtering, and dispatch. */
void wcmEvent(WacomCommonPtr common, unsigned int channel, const WacomDeviceState* ds);

/* dispatches data to XInput event system */
void wcmSendEvents(InputInfoPtr pInfo, const WacomDeviceState* ds);

/* generic area check for xf86Wacom.c, wcmCommon.c and wcmXCommand.c */
Bool wcmPointInArea(WacomToolAreaPtr area, int x, int y);
Bool wcmAreaListOverlap(WacomToolAreaPtr area, WacomToolAreaPtr list);

/* calculate the proper tablet to screen mapping factor */
void wcmMappingFactor(InputInfoPtr pInfo);

/* validation */
extern Bool wcmIsAValidType(InputInfoPtr pInfo, const char* type);
extern Bool wcmIsWacomDevice (char* fname);
extern int wcmIsDuplicate(char* device, InputInfoPtr pInfo);
extern int wcmDeviceTypeKeys(InputInfoPtr pInfo);

/* hotplug */
extern int wcmNeedAutoHotplug(InputInfoPtr pInfo, const char **type);
extern void wcmHotplugOthers(InputInfoPtr pInfo, const char *basename);

/* setup */
extern Bool wcmParseOptions(InputInfoPtr pInfo, Bool is_primary, Bool is_dependent);
extern int wcmParseSerials(InputInfoPtr pinfo);
extern void wcmInitialCoordinates(InputInfoPtr pInfo, int axes);
extern void wcmInitialScreens(InputInfoPtr pInfo);
extern void wcmInitialScreens(InputInfoPtr pInfo);

extern int wcmDevSwitchModeCall(InputInfoPtr pInfo, int mode);
extern int wcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode);

/* run-time modifications */
extern void wcmChangeScreen(InputInfoPtr pInfo, int value);
extern int wcmTilt2R(int x, int y, double offset);
extern void wcmGestureFilter(WacomDevicePtr priv, int channel);
extern void wcmEmitKeycode(DeviceIntPtr keydev, int keycode, int state);
extern void wcmSoftOutEvent(InputInfoPtr pInfo);

extern void wcmRotateTablet(InputInfoPtr pInfo, int value);
extern void wcmRotateAndScaleCoordinates(InputInfoPtr pInfo, int* x, int* y);
extern void wcmVirtualTabletSize(InputInfoPtr pInfo);
extern void wcmVirtualTabletPadding(InputInfoPtr pInfo);

extern int wcmCheckPressureCurveValues(int x0, int y0, int x1, int y1);
extern int wcmGetPhyDeviceID(WacomDevicePtr priv);

/* device properties */
extern int wcmSetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop, BOOL checkonly);
extern int wcmGetProperty(DeviceIntPtr dev, Atom property);
extern int wcmDeleteProperty(DeviceIntPtr dev, Atom property);
extern void InitWcmDeviceProperties(InputInfoPtr pInfo);
extern void wcmUpdateRotationProperty(WacomDevicePtr priv);
extern void wcmUpdateSerial(InputInfoPtr pInfo, unsigned int serial);

/* Utility functions */
extern Bool is_absolute(InputInfoPtr pInfo);
extern void set_absolute(InputInfoPtr pInfo, Bool absolute);
extern WacomCommonPtr wcmRefCommon(WacomCommonPtr common);
extern void wcmFreeCommon(WacomCommonPtr *common);
extern WacomCommonPtr wcmNewCommon(void);

enum WacomSuppressMode {
	SUPPRESS_NONE = 8,	/* Process event normally */
	SUPPRESS_ALL,		/* Supress and discard the whole event */
	SUPPRESS_NON_MOTION	/* Supress all events but x/y motion */
};

/****************************************************************************/
#endif /* __XF86WACOM_H */

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
