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

#include <wacom-util.h>

#include <WacomInterface.h>

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
			if (((WacomDeviceRec*)(priv))->is_common_rec) { \
				wcmLogDebugCommon((WacomCommonRec*)priv, lvl, __func__, __VA_ARGS__); \
			} else { \
				wcmLogDebugDevice((WacomDeviceRec*)priv, lvl, __func__, __VA_ARGS__); \
			} \
		} \
	} while (0)
#else
#define DBG(lvl, priv, ...) do {} while(0)
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

WacomDevicePtr wcmAllocate(void *frontend, const char *name);
int wcmPreInit(WacomDevicePtr priv);
void wcmUnInit(WacomDevicePtr priv);
/* Open the **shared** fd, if necessary */
int wcmDevOpen(WacomDevicePtr priv);
Bool wcmDevInit(WacomDevicePtr priv);
/* Close the **shared** fd, if necessary */
void wcmDevClose(WacomDevicePtr priv);
Bool wcmDevStart(WacomDevicePtr priv);
void wcmDevStop(WacomDevicePtr priv);

void wcmRemoveActive(WacomDevicePtr priv);

/* device autoprobing */
char *wcmEventAutoDevProbe (WacomDevicePtr priv);

/* common tablet initialization regime */
int wcmInitTablet(WacomDevicePtr priv);

/* standard packet handler */
int wcmReadPacket(WacomDevicePtr priv);

/* handles suppression, filtering, and dispatch. */
void wcmEvent(WacomCommonPtr common, unsigned int channel, const WacomDeviceState* ds);

/* dispatches data to XInput event system */
void wcmSendEvents(WacomDevicePtr priv, const WacomDeviceState* ds);

/* validation */
extern Bool wcmIsAValidType(WacomDevicePtr priv, const char* type);
extern int wcmIsDuplicate(const char* device, WacomDevicePtr priv);

/* hotplug */
extern int wcmNeedAutoHotplug(WacomDevicePtr priv, char **type);
extern void wcmHotplugOthers(WacomDevicePtr priv, const char *basename);

/* setup */
extern Bool wcmPreInitParseOptions(WacomDevicePtr priv, Bool is_primary, Bool is_dependent);
extern Bool wcmPostInitParseOptions(WacomDevicePtr priv, Bool is_primary, Bool is_dependent);
extern int wcmParseSerials(WacomDevicePtr priv);

extern Bool wcmDevSwitchModeCall(WacomDevicePtr priv, Bool absolute);

extern void wcmResetButtonAction(WacomDevicePtr priv, int button);
extern void wcmResetStripAction(WacomDevicePtr priv, int index);
extern void wcmResetWheelAction(WacomDevicePtr priv, int index);

extern void wcmEnableTool(WacomDevicePtr priv);
extern void wcmDisableTool(WacomDevicePtr priv);
extern void wcmUnlinkTouchAndPen(WacomDevicePtr priv);

/* run-time modifications */
extern int wcmTilt2R(int x, int y, double offset);
extern void wcmSoftOutEvent(WacomDevicePtr priv);
extern void wcmCancelGesture(WacomDevicePtr priv);

extern void wcmRotateTablet(WacomDevicePtr priv, int value);
extern void wcmRotateAndScaleCoordinates(WacomDevicePtr priv, int* x, int* y);

extern int wcmCheckPressureCurveValues(int x0, int y0, int x1, int y1);
extern int wcmGetPhyDeviceID(WacomDevicePtr priv);

/* device properties */
extern void InitWcmDeviceProperties(WacomDevicePtr priv);

/* Utility functions */
extern Bool is_absolute(WacomDevicePtr priv);
extern void set_absolute(WacomDevicePtr priv, Bool absolute);
extern WacomCommonPtr wcmRefCommon(WacomCommonPtr common);
extern void wcmFreeCommon(WacomCommonPtr *common);
extern WacomCommonPtr wcmNewCommon(void);
extern size_t wcmListModels(const char **names, size_t len);
extern int wcmScaleAxis(int Cx, int to_max, int to_min, int from_max, int from_min);

static inline void wcmActionCopy(WacomAction *dest, WacomAction *src)
{
	memset(dest, 0, sizeof(*dest));
	memcpy(dest, src, sizeof(*src));
}
static inline const unsigned* wcmActionData(const WacomAction *action)
{
	return action->action;
}
static inline size_t wcmActionSize(const WacomAction *action)
{
	return action->nactions;
}
static inline void wcmActionSet(WacomAction *action, unsigned idx, unsigned act)
{
	if (idx >= ARRAY_SIZE(action->action))
		return;
	action->action[idx] = act;
	action->nactions = idx + 1;
}

enum WacomSuppressMode {
	SUPPRESS_NONE = 8,	/* Process event normally */
	SUPPRESS_ALL,		/* Supress and discard the whole event */
	SUPPRESS_NON_MOTION	/* Supress all events but x/y motion */
};

/****************************************************************************/

#endif /* __XF86WACOM_H */

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
