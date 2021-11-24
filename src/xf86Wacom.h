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

#include "Xwacom.h"

#include <string.h>
#include <errno.h>

#include <xf86.h>
#include <xf86Xinput.h>
#include <X11/Xatom.h>

#include <wacom-util.h>

#ifndef _fallthrough_
#define _fallthrough_ __attribute__((fallthrough))
#endif

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 23
#define HAVE_THREADED_INPUT 1
#endif

#ifndef SW_MUTE_DEVICE
#define SW_MUTE_DEVICE	0x0e
#endif

/******************************************************************************
 * Debugging support
 *****************************************************************************/

#ifdef DBG
#undef DBG
#endif

#ifdef DEBUG
#define DBG(lvl, priv, ...) \
	do { \
		if ((lvl) <= priv->debugLevel) { \
			LogMessageVerbSigSafe(X_INFO, -1, "%s (%d:%s): ", \
				((WacomDeviceRec*)priv)->name, lvl, __func__); \
			LogMessageVerbSigSafe(X_NONE, -1, __VA_ARGS__); \
		} \
	} while (0)
#else
#define DBG(lvl, priv, ...)
#endif

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

/* Close the device */
extern void wcmClose(InputInfoPtr pInfo);

/* device autoprobing */
char *wcmEventAutoDevProbe (InputInfoPtr pInfo);

/* common tablet initialization regime */
int wcmInitTablet(InputInfoPtr pInfo, const char* id, float version);

/* standard packet handler */
Bool wcmReadPacket(InputInfoPtr pInfo);

/* handles suppression, filtering, and dispatch. */
void wcmEvent(WacomCommonPtr common, unsigned int channel, const WacomDeviceState* ds);

/* dispatches data to XInput event system */
void wcmSendEvents(InputInfoPtr pInfo, const WacomDeviceState* ds);

/* validation */
extern Bool wcmIsAValidType(InputInfoPtr pInfo, const char* type);
extern Bool wcmIsWacomDevice (char* fname);
extern int wcmIsDuplicate(const char* device, InputInfoPtr pInfo);
extern int wcmDeviceTypeKeys(InputInfoPtr pInfo);

/* hotplug */
extern int wcmNeedAutoHotplug(InputInfoPtr pInfo, char **type);
extern void wcmHotplugOthers(InputInfoPtr pInfo, const char *basename);

/* setup */
extern Bool wcmPreInitParseOptions(InputInfoPtr pInfo, Bool is_primary, Bool is_dependent);
extern Bool wcmPostInitParseOptions(InputInfoPtr pInfo, Bool is_primary, Bool is_dependent);
extern int wcmParseSerials(InputInfoPtr pinfo);

extern int wcmDevSwitchModeCall(InputInfoPtr pInfo, int mode);

extern int wcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode);
extern int wcmDevChangeControl(InputInfoPtr pInfo, xDeviceCtl * control);
extern void wcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl);
extern int wcmDevProc(DeviceIntPtr pWcm, int what);
extern void wcmDevReadInput(InputInfoPtr pInfo);

/* run-time modifications */
extern int wcmTilt2R(int x, int y, double offset);
extern void wcmEmitKeycode(DeviceIntPtr keydev, int keycode, int state);
extern void wcmSoftOutEvent(InputInfoPtr pInfo);
extern void wcmCancelGesture(InputInfoPtr pInfo);

extern void wcmRotateTablet(InputInfoPtr pInfo, int value);
extern void wcmRotateAndScaleCoordinates(InputInfoPtr pInfo, int* x, int* y);

extern int wcmCheckPressureCurveValues(int x0, int y0, int x1, int y1);
extern int wcmGetPhyDeviceID(WacomDevicePtr priv);

/* device properties */
extern int wcmSetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop, BOOL checkonly);
extern int wcmGetProperty(DeviceIntPtr dev, Atom property);
extern int wcmDeleteProperty(DeviceIntPtr dev, Atom property);
extern void InitWcmDeviceProperties(InputInfoPtr pInfo);
extern void wcmUpdateRotationProperty(WacomDevicePtr priv);
extern void wcmUpdateSerial(InputInfoPtr pInfo, unsigned int serial, int id);
extern void wcmUpdateHWTouchProperty(WacomDevicePtr priv, int touch);

/* Utility functions */
extern Bool is_absolute(InputInfoPtr pInfo);
extern void set_absolute(InputInfoPtr pInfo, Bool absolute);
extern WacomCommonPtr wcmRefCommon(WacomCommonPtr common);
extern void wcmFreeCommon(WacomCommonPtr *common);
extern WacomCommonPtr wcmNewCommon(void);
extern void usbListModels(void);

enum WacomSuppressMode {
	SUPPRESS_NONE = 8,	/* Process event normally */
	SUPPRESS_ALL,		/* Supress and discard the whole event */
	SUPPRESS_NON_MOTION	/* Supress all events but x/y motion */
};

/****************************************************************************/

#ifndef UNIT_TESTS

# define TEST_NON_STATIC static

#else

# define TEST_NON_STATIC

/* For test suite */
/* xf86Wacom.c */
extern void wcmInitialToolSize(InputInfoPtr pInfo);

/* wcmConfig.c */
extern int wcmSetType(InputInfoPtr pInfo, const char *type);

/* wcmCommon.c */
extern int getScrollDelta(int current, int old, int wrap, int flags);
extern int getWheelButton(int delta, int action_up, int action_dn);
extern int rebasePressure(const WacomDevicePtr priv, const WacomDeviceState *ds);
extern int normalizePressure(const WacomDevicePtr priv, const int raw_pressure);
extern enum WacomSuppressMode wcmCheckSuppress(WacomCommonPtr common,
						const WacomDeviceState* dsOrig,
						WacomDeviceState* dsNew);

/* wcmUSB.c */
extern int mod_buttons(int buttons, int btn, int state);
#endif /* UNIT_TESTS */

#endif /* __XF86WACOM_H */

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
