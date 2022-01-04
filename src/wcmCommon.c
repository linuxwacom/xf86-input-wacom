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

#include <config.h>

#include <unistd.h>
#include "xf86Wacom.h"
#include "Xwacom.h"
#include "wcmFilter.h"
#include "wcmTouchFilter.h"
#include <xkbsrv.h>
#include <xf86_OSproc.h>

#ifdef ENABLE_TESTS
#include "wacom-test-suite.h"
#endif

static struct _WacomDriverRec
{
	WacomDevicePtr active;     /* Arbitrate motion through this pointer */
} WACOM_DRIVER = {
	.active = NULL,
};

void wcmRemoveActive(WacomDevicePtr priv)
{
	if (WACOM_DRIVER.active == priv)
		WACOM_DRIVER.active = NULL;
}

/*****************************************************************************
 * Static functions
 ****************************************************************************/

static int applyPressureCurve(WacomDevicePtr pDev, const WacomDeviceStatePtr pState);
static void commonDispatchDevice(WacomDevicePtr priv,
				 const WacomChannelPtr pChannel);
static void sendAButton(WacomDevicePtr pDev, const WacomDeviceState* ds, int button,
			int mask, const WacomAxisData *axes);

/*****************************************************************************
 * Utility functions
 ****************************************************************************/

/**
 * @return TRUE if the device is set to abolute mode, or FALSE otherwise
 */
Bool is_absolute(WacomDevicePtr priv)
{
	return !!(priv->flags & ABSOLUTE_FLAG);
}

/**
 * Set the device to absolute or relative mode
 *
 * @param absolute TRUE to set the device to absolute mode.
 */
void set_absolute(WacomDevicePtr priv, Bool absolute)
{
	if (absolute)
		priv->flags |= ABSOLUTE_FLAG;
	else
		priv->flags &= ~ABSOLUTE_FLAG;
}

/*****************************************************************************
* wcmDevSwitchModeCall --
*****************************************************************************/

Bool wcmDevSwitchModeCall(WacomDevicePtr priv, Bool absolute)
{
	DBG(3, priv, "to mode=%s\n", absolute ? "absolute" : "relative");

	/* Pad is always in absolute mode.*/
	if (IsPad(priv))
		return absolute;
	else
		set_absolute(priv, absolute);
	return TRUE;
}


static int wcmButtonPerNotch(WacomDevicePtr priv, int value, int threshold, int btn_positive, int btn_negative)
{
	int mode = is_absolute(priv);
	int notches = value / threshold;
	int button = (notches > 0) ? btn_positive : btn_negative;
	int i;
	WacomAxisData axes = {0};

	for (i = 0; i < abs(notches); i++) {
		wcmEmitButton(priv, mode, button, 1, &axes);
		wcmEmitButton(priv, mode, button, 0, &axes);
	}

	return value % threshold;
}

static void wcmPanscroll(WacomDevicePtr priv, const WacomDeviceState *ds, int x, int y)
{
	WacomCommonPtr common = priv->common;
	int threshold = common->wcmPanscrollThreshold;
	int *accumulated_x, *accumulated_y;
	int delta_x, delta_y;

	if (!(priv->flags & SCROLLMODE_FLAG) || !(ds->buttons & 1))
		return;

	/* Tip has gone down down; store state for dragging */
	if (!(priv->oldState.buttons & 1)) {
		priv->wcmPanscrollState = *ds;
		priv->wcmPanscrollState.x = 0;
		priv->wcmPanscrollState.y = 0;
		return;
	}

	if (!is_absolute(priv)) {
		delta_x = x;
		delta_y = y;
	}
	else {
		delta_x = (x - priv->oldState.x);
		delta_y = (y - priv->oldState.y);
	}

	accumulated_x = &priv->wcmPanscrollState.x;
	accumulated_y = &priv->wcmPanscrollState.y;
	*accumulated_x += delta_x;
	*accumulated_y += delta_y;

	DBG(6, priv, "pan x = %d, pan y = %d\n", *accumulated_x, *accumulated_y);

	*accumulated_x = wcmButtonPerNotch(priv, *accumulated_x, threshold, 6, 7);
	*accumulated_y = wcmButtonPerNotch(priv, *accumulated_y, threshold, 4, 5);
}

void wcmResetButtonAction(WacomDevicePtr priv, int button)
{
	WacomAction new_action = {};
	int x11_button = priv->button_default[button];
	char name[64];

	sprintf(name, "Wacom button action %d", button);
	wcmActionSet(&new_action, 0, AC_BUTTON | AC_KEYBTNPRESS | x11_button);
	wcmActionCopy(&priv->key_actions[button], &new_action);
}

void wcmResetStripAction(WacomDevicePtr priv, int index)
{
	WacomAction new_action = {};
	char name[64];

	sprintf(name, "Wacom strip action %d", index);
	wcmActionSet(&new_action, 0, AC_BUTTON | AC_KEYBTNPRESS | (priv->strip_default[index]));
	wcmActionSet(&new_action, 1, AC_BUTTON | (priv->strip_default[index]));
	wcmActionCopy(&priv->strip_actions[index], &new_action);
}

void wcmResetWheelAction(WacomDevicePtr priv, int index)
{
	WacomAction new_action = {};
	char name[64];

	sprintf(name, "Wacom wheel action %d", index);
	wcmActionSet(&new_action, 0, AC_BUTTON | AC_KEYBTNPRESS | (priv->wheel_default[index]));
	wcmActionSet(&new_action, 1, AC_BUTTON | (priv->wheel_default[index]));
	wcmActionCopy(&priv->wheel_actions[index], &new_action);
}

/* Main event hanlding function */
int wcmReadPacket(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	int len, pos, cnt, remaining;

	DBG(10, common, "fd=%d\n", wcmGetFd(priv));

	remaining = sizeof(common->buffer) - common->bufpos;

	DBG(1, common, "pos=%d remaining=%d\n", common->bufpos, remaining);

	/* fill buffer with as much data as we can handle */
	SYSCALL((len = read(wcmGetFd(priv), common->buffer + common->bufpos, remaining)));

	if (len <= 0)
	{
		if (errno == EAGAIN || errno == EINTR)
			return 0;
		return -errno;
	}

	/* account for new data */
	common->bufpos += len;
	DBG(10, common, "buffer has %d bytes\n", common->bufpos);

	len = common->bufpos;
	pos = 0;

	while (len > 0)
	{
		/* parse packet */
		cnt = common->wcmModel->Parse(priv, common->buffer + pos, len);
		if (cnt <= 0)
		{
			if (cnt < 0)
				DBG(1, common, "Misbehaving parser returned %d\n",cnt);
			break;
		}
		pos += cnt;
		len -= cnt;
	}

	/* if half a packet remains, move it down */
	if (len)
	{
		DBG(7, common, "MOVE %d bytes\n", common->bufpos - pos);
		memmove(common->buffer,common->buffer+pos, len);
	}

	common->bufpos = len;

	return pos;
}


/*****************************************************************************
 * wcmSendButtons --
 *   Send button events by comparing the current button mask with the
 *   previous one.
 ****************************************************************************/

static void wcmSendButtons(WacomDevicePtr priv, const WacomDeviceState* ds, unsigned int buttons,
			   const WacomAxisData *axes)
{
	unsigned int button, mask, first_button;
	WacomCommonPtr common = priv->common;
	DBG(6, priv, "buttons=%u\n", buttons);

	 /* button behaviour (TPC button on):
		if only tip is pressed/released, send button 1 events
		if button N is pressed and tip is pressed/released, send
		button N events.
		if tip is already down and button N is pressed/released,
		send button 1 release, then button N events.
	 */

	first_button = 0; /* zero-indexed because of mask */

	/* Tablet PC buttons only apply to penabled devices */
	if (common->wcmTPCButton && IsStylus(priv))
	{
		first_button = (buttons <= 1) ? 0 : 1;

		/* tip released? release all buttons */
		if ((buttons & 1) == 0)
			buttons = 0;
		/* tip pressed? send all other button presses */
		else if ((buttons & 1) != (priv->oldState.buttons & 1))
			priv->oldState.buttons = 0;
		/* other button changed while tip is still down? release tip */
		else if ((buttons & 1) && (buttons != priv->oldState.buttons))
		{
			buttons &= ~1;
			first_button = 0;
		}
	}

	for (button = first_button; button < WCM_MAX_BUTTONS; button++)
	{
		mask = 1u << button;
		if ((mask & priv->oldState.buttons) != (mask & buttons))
			sendAButton(priv, ds, button, (mask & buttons), axes);
	}

}

static void wcmSendKeys (WacomDevicePtr priv, unsigned int current, unsigned int previous)
{
	unsigned int mask, idx;

	DBG(6, priv, "current=%u previous=%u\n", current, previous);

	for (idx = 0, mask = 0x1;
	     mask && (mask <= current || mask <= previous);
	     idx++, mask <<= 1)
	{
		if ((mask & previous) != (mask & current))
		{
			int key = 0;
			int state = !!(mask & current);

			switch (idx) {
				/* Note: the evdev keycodes are > 255 and
				 * get dropped by the server. So let's remap
				 * those to KEY_PROG1-3 instead */
				case IDX_KEY_CONTROLPANEL:
					key = KEY_PROG1;
					break;
				case IDX_KEY_ONSCREEN_KEYBOARD:
					key = KEY_PROG2;
					break;
				case IDX_KEY_BUTTONCONFIG:
					key = KEY_PROG3;
					break;
				case IDX_KEY_INFO:
					key = KEY_PROG4;
					break;
				default:
					break;
			}
			if (key)
				wcmEmitKeycode(priv, key + 8, state);
		}
	}
}

/*****************************************************************************
 * countPresses
 *   Count the number of key/button presses not released for the given key
 *   array.
 ****************************************************************************/
static int countPresses(int keybtn, const unsigned int* keys, int size)
{
	int i, act, count = 0;

	for (i = 0; i < size; i++)
	{
		act = keys[i];
		if ((act & AC_CODE) == keybtn)
			count += (act & AC_KEYBTNPRESS) ? 1 : -1;
	}

	return count;
}

static void sendAction(WacomDevicePtr priv,  const WacomDeviceState* ds,
		       int press, const WacomAction *act,
		       const WacomAxisData *axes)
{
	int i;
	int nkeys = wcmActionSize(act);
	const unsigned *keys = wcmActionData(act);

	/* Actions only trigger on press, not release */
	for (i = 0; press && i < nkeys; i++)
	{
		unsigned int action = keys[i];

		if (!action)
			break;

		switch ((action & AC_TYPE))
		{
			case AC_BUTTON:
				{
					int btn_no = (action & AC_CODE);
					int is_press = (action & AC_KEYBTNPRESS);
					if (btn_no == 1 && (priv->flags & SCROLLMODE_FLAG)) {
						/* Don't send clicks in scroll mode */
					}
					else {
						wcmEmitButton(priv, is_absolute(priv), btn_no,
							      is_press, axes);
					}
				}
				break;
			case AC_KEY:
				{
					int key_code = (action & AC_CODE);
					int is_press = (action & AC_KEYBTNPRESS);
					wcmEmitKeycode(priv, key_code, is_press);
				}
				break;
			case AC_MODETOGGLE:
				if (press)
					wcmDevSwitchModeCall(priv,
							(is_absolute(priv)) ? Relative : Absolute); /* not a typo! */
				break;
			case AC_PANSCROLL:
				priv->flags |= SCROLLMODE_FLAG;
				priv->wcmPanscrollState = *ds;
				priv->wcmPanscrollState.x = 0;
				priv->wcmPanscrollState.y = 0;
				break;
		}
	}

	/* Release all non-released keys for this button. */
	for (i = 0; !press && i < nkeys; i++)
	{
		unsigned int action = keys[i];

		switch ((action & AC_TYPE))
		{
			case AC_BUTTON:
				{
					int btn_no = (action & AC_CODE);

					/* don't care about releases here */
					if (!(action & AC_KEYBTNPRESS))
						break;

					if (countPresses(btn_no, &keys[i], nkeys - i))
						wcmEmitButton(priv, is_absolute(priv), btn_no,
							      FALSE, axes);
				}
				break;
			case AC_KEY:
				{
					int key_code = (action & AC_CODE);

					/* don't care about releases here */
					if (!(action & AC_KEYBTNPRESS))
						break;

					if (countPresses(key_code, &keys[i], nkeys - i))
						wcmEmitKeycode(priv, key_code, 0);
				}
				break;
			case AC_PANSCROLL:
				priv->flags &= ~SCROLLMODE_FLAG;
				break;
		}
	}
}

/*****************************************************************************
 * sendAButton --
 *   Send one button event, called by wcmSendButtons
 ****************************************************************************/
static void sendAButton(WacomDevicePtr priv, const WacomDeviceState* ds, int button,
			int mask, const WacomAxisData  *axes)
{
#ifdef DEBUG
	WacomCommonPtr common = priv->common;
#endif

	DBG(4, priv, "TPCButton(%s) button=%d state=%d\n",
	    common->wcmTPCButton ? "on" : "off", button, mask);

	if (wcmActionSize(&priv->key_actions[button]) == 0)
		return;

	sendAction(priv, ds, (mask != 0), &priv->key_actions[button], axes);
}

/**
 * Get the distance an axis was scrolled. This function is aware
 * of the different ways different scrolling axes work and strives
 * to produce a common representation of relative change.
 *
 * @param current  Current value of the axis
 * @param old      Previous value of the axis
 * @param wrap     Maximum value before wraparound occurs (0 if axis does not wrap)
 * @param flags    Flags defining axis attributes: AXIS_INVERT and AXIS_BITWISE
 * @return         Relative change in axis value
 */
static int getScrollDelta(int current, int old, int wrap, int flags)
{
	int delta;

	if (flags & AXIS_BITWISE)
	{
		current = log2((current << 1) | 0x01);
		old = log2((old << 1) | 0x01);
		wrap = log2((wrap << 1) | 0x01);
	}

	delta = current - old;

	if (flags & AXIS_INVERT)
		delta = -delta;

	if (wrap != 0)
	{
		/* Wraparound detection. If the distance old..current
		 * is larger than the old..current considering the
		 * wraparound, assume wraparound and readjust */
		int wrap_delta;

		if (delta < 0)
			wrap_delta =  (wrap + 1) + delta;
		else
			wrap_delta = -((wrap + 1) - delta);

		if (abs(wrap_delta) < abs(delta))
			delta = wrap_delta;
	}

	return delta;
}

/**
 * Get the scroll button/action to send given the delta of
 * the scrolling axis and the possible events that can be
 * sent.
 *
 * @param delta        Amount of change in the scrolling axis
 * @param action_up    Array index of action to send on scroll up
 * @param action_dn    Array index of action to send on scroll down
 * @return             Array index of action that should be performed, or -1 if none.
 */
static int getWheelButton(int delta, int action_up, int action_dn)
{
	if (delta > 0)
		return action_up;
	else if (delta < 0)
		return action_dn;
	else
		return -1;
}

/**
 * Send button or actions for a scrolling axis.
 *
 * @param button     X button number to send if no action is defined
 * @param action     Action to send
 * @param nactions   Length of action array
 */
static void sendWheelStripEvent(WacomDevicePtr priv, const WacomAction *action,
				const WacomDeviceState* ds, const WacomAxisData *axes)
{
	sendAction(priv, ds, 1, action, axes);
	sendAction(priv, ds, 0, action, axes);
}

/*****************************************************************************
 * sendWheelStripEvents --
 *   Send events defined for relative/absolute wheels or strips
 ****************************************************************************/

static void sendWheelStripEvents(WacomDevicePtr priv, const WacomDeviceState* ds,
				 const WacomAxisData *axes)
{
	WacomCommonPtr common = priv->common;
	int delta = 0, idx = 0;

	DBG(10, priv, "\n");

	/* emulate events for left strip */
	delta = getScrollDelta(ds->stripx, priv->oldState.stripx, 0, AXIS_INVERT | AXIS_BITWISE);
	idx = getWheelButton(delta, STRIP_LEFT_UP, STRIP_LEFT_DN);
	if (idx >= 0 && IsPad(priv) && priv->oldState.proximity == ds->proximity)
	{
		DBG(10, priv, "Left touch strip scroll delta = %d\n", delta);
		sendWheelStripEvent(priv, &priv->strip_actions[idx], ds, axes);
	}

	/* emulate events for right strip */
	delta = getScrollDelta(ds->stripy, priv->oldState.stripy, 0, AXIS_INVERT | AXIS_BITWISE);
	idx = getWheelButton(delta, STRIP_RIGHT_UP, STRIP_RIGHT_DN);
	if (idx >= 0 && IsPad(priv) && priv->oldState.proximity == ds->proximity)
	{
		DBG(10, priv, "Right touch strip scroll delta = %d\n", delta);
		sendWheelStripEvent(priv, &priv->strip_actions[idx], ds, axes);
	}

	/* emulate events for relative wheel */
	delta = getScrollDelta(ds->relwheel, 0, 0, 0);
	idx = getWheelButton(delta, WHEEL_REL_UP, WHEEL_REL_DN);
	if (idx >= 0 && (IsCursor(priv) || IsPad(priv)) && priv->oldState.proximity == ds->proximity)
	{
		DBG(10, priv, "Relative wheel scroll delta = %d\n", delta);
		sendWheelStripEvent(priv, &priv->wheel_actions[idx], ds, axes);
	}

	/* emulate events for left touch ring */
	delta = getScrollDelta(ds->abswheel, priv->oldState.abswheel, common->wcmMaxRing, AXIS_INVERT);
	idx = getWheelButton(delta, WHEEL_ABS_UP, WHEEL_ABS_DN);
	if (idx >= 0 && IsPad(priv) && priv->oldState.proximity == ds->proximity)
	{
		DBG(10, priv, "Left touch wheel scroll delta = %d\n", delta);
		sendWheelStripEvent(priv, &priv->wheel_actions[idx], ds, axes);
	}

	/* emulate events for right touch ring */
	delta = getScrollDelta(ds->abswheel2, priv->oldState.abswheel2, common->wcmMaxRing, AXIS_INVERT);
	idx = getWheelButton(delta, WHEEL2_ABS_UP, WHEEL2_ABS_DN);
	if (idx >= 0 && IsPad(priv) && priv->oldState.proximity == ds->proximity)
	{
		DBG(10, priv, "Right touch wheel scroll delta = %d\n", delta);
		sendWheelStripEvent(priv, &priv->wheel_actions[idx], ds, axes);
	}
}

/*****************************************************************************
 * sendCommonEvents --
 *   Send events common between pad and stylus/cursor/eraser.
 ****************************************************************************/

static void sendCommonEvents(WacomDevicePtr priv, const WacomDeviceState* ds,
			     const WacomAxisData *axes)
{
	unsigned int buttons = ds->buttons;
	int x = 0, y = 0;

	wcmAxisGet(axes, WACOM_AXIS_X, &x);
	wcmAxisGet(axes, WACOM_AXIS_Y, &y);

	/* send scrolling events if necessary */
	wcmPanscroll(priv, ds, x, y);

	/* send button events when state changed or first time in prox and button unpresses */
	if (priv->oldState.buttons != buttons || (!priv->oldState.proximity && !buttons))
		wcmSendButtons(priv, ds, buttons, axes);

	/* emulate wheel/strip events when defined */
	if ( ds->relwheel || (ds->abswheel != priv->oldState.abswheel) || (ds->abswheel2 != priv->oldState.abswheel2) ||
		( (ds->stripx - priv->oldState.stripx) && ds->stripx && priv->oldState.stripx) ||
			((ds->stripy - priv->oldState.stripy) && ds->stripy && priv->oldState.stripy) )
		sendWheelStripEvents(priv, ds, axes);
}

/* 1:1 copy from xf86ScaleAxis */
int wcmScaleAxis(int Cx, int to_max, int to_min, int from_max, int from_min)
{
	int X;
	int64_t to_width = to_max - to_min;
	int64_t from_width = from_max - from_min;

	if (from_width) {
		X = (int)(((to_width * (Cx - from_min)) / from_width) + to_min);
	}
	else {
		X = 0;
		/*ErrorF ("Divide by Zero in xf86ScaleAxis\n");*/
	}

	if (X > to_max)
		X = to_max;
	if (X < to_min)
		X = to_min;

	return X;
}


/* rotate x and y before post X inout events */
void wcmRotateAndScaleCoordinates(WacomDevicePtr priv, int* x, int* y)
{
	WacomCommonPtr common = priv->common;
	int tmp_coord;
	int xmax, xmin, ymax, ymin;

	/* scale into the topX/topY area we had when we initialized the
	 * valuator */
	xmin = priv->valuatorMinX;
	xmax = priv->valuatorMaxX;
	ymin = priv->valuatorMinY;
	ymax = priv->valuatorMaxY;

	/* Don't try to scale relative axes */
	if (xmax > xmin)
		*x = wcmScaleAxis(*x, xmax, xmin, priv->bottomX, priv->topX);

	if (ymax > ymin)
		*y = wcmScaleAxis(*y, ymax, ymin, priv->bottomY, priv->topY);

	/* coordinates are now in the axis rage we advertise for the device */

	if (common->wcmRotate == ROTATE_CW || common->wcmRotate == ROTATE_CCW)
	{
		tmp_coord = *x;

		*x = wcmScaleAxis(*y, xmax, xmin, ymax, ymin);
		*y = wcmScaleAxis(tmp_coord, ymax, ymin, xmax, xmin);
	}

	if (common->wcmRotate == ROTATE_CW)
		*y = ymax - (*y - ymin);
	else if (common->wcmRotate == ROTATE_CCW)
		*x = xmax - (*x - xmin);
	else if (common->wcmRotate == ROTATE_HALF)
	{
		*x = xmax - (*x - xmin);
		*y = ymax - (*y - ymin);
	}


	DBG(10, priv, "rotate/scaled to %d/%d\n", *x, *y);
}

static void wcmUpdateOldState(WacomDevicePtr priv,
			      const WacomDeviceState *ds, int currentX, int currentY)
{
	priv->oldState = *ds;
	priv->oldState.x = currentX;
	priv->oldState.y = currentY;
}

static void
wcmSendPadEvents(WacomDevicePtr priv, const WacomDeviceState* ds, const WacomAxisData *axes)
{
	if (!priv->oldState.proximity && ds->proximity)
		wcmEmitProximity(priv, TRUE, axes);

	if (axes->mask || ds->buttons || ds->relwheel ||
	    (ds->abswheel != priv->oldState.abswheel) || (ds->abswheel2 != priv->oldState.abswheel2))
	{
		sendCommonEvents(priv, ds, axes);

		wcmEmitMotion(priv, TRUE, axes);
	}
	else
	{
		if (priv->oldState.buttons)
			wcmSendButtons(priv, ds, ds->buttons, axes);
	}

	wcmSendKeys(priv, ds->keys, priv->oldState.keys);

	if (priv->oldState.proximity && !ds->proximity)
		wcmEmitProximity(priv, FALSE, axes);
}

/* Send events for all tools but pads */
static void
wcmSendNonPadEvents(WacomDevicePtr priv, const WacomDeviceState *ds,
		    WacomAxisData *axes)
{
	if (!is_absolute(priv))
	{
		int val;

		val = 0;
		wcmAxisGet(axes, WACOM_AXIS_X, &val);
		wcmAxisSet(axes, WACOM_AXIS_X, val - priv->oldState.x);
		val = 0;
		wcmAxisGet(axes, WACOM_AXIS_Y, &val);
		wcmAxisSet(axes, WACOM_AXIS_Y, val - priv->oldState.y);
		val = 0;
		wcmAxisGet(axes, WACOM_AXIS_PRESSURE, &val);
		wcmAxisSet(axes, WACOM_AXIS_PRESSURE, val - priv->oldState.pressure);

		if (IsCursor(priv))
		{
			val = 0;
			wcmAxisGet(axes, WACOM_AXIS_ROTATION, &val);
			wcmAxisSet(axes, WACOM_AXIS_ROTATION, val - priv->oldState.rotation);
			val = 0;
			wcmAxisGet(axes, WACOM_AXIS_THROTTLE, &val);
			wcmAxisSet(axes, WACOM_AXIS_THROTTLE, val - priv->oldState.throttle);
		} else
		{
			val = 0;
			wcmAxisGet(axes, WACOM_AXIS_TILT_X, &val);
			wcmAxisSet(axes, WACOM_AXIS_TILT_X, val - priv->oldState.tilty);
			val = 0;
			wcmAxisGet(axes, WACOM_AXIS_TILT_Y, &val);
			wcmAxisSet(axes, WACOM_AXIS_TILT_Y, val - priv->oldState.tilty);
		}
		val = 0;
		if (wcmAxisGet(axes, WACOM_AXIS_RING, &val))
			wcmAxisSet(axes, WACOM_AXIS_RING, val - priv->oldState.abswheel);
		val = 0;
		if (wcmAxisGet(axes, WACOM_AXIS_RING2, &val))
			wcmAxisSet(axes, WACOM_AXIS_RING2, val - priv->oldState.abswheel2);
	}

	/* coordinates are ready we can send events */
	if (ds->proximity)
	{
		if (!priv->oldState.proximity)
			wcmEmitProximity(priv, TRUE, axes);

		/* Move the cursor to where it should be before sending button events */
		if(!(priv->flags & BUTTONS_ONLY_FLAG) &&
		   !(priv->flags & SCROLLMODE_FLAG && (!is_absolute(priv) || priv->oldState.buttons & 1)))
		{
			wcmEmitMotion(priv, is_absolute(priv), axes);
			/* For relative events, do not repost
			 * the valuators.  Otherwise, a button
			 * event in sendCommonEvents will move the
			 * axes again.
			 */
			if (!is_absolute(priv))
			{
				memset(axes, 0, sizeof(*axes));
			}
		}

		sendCommonEvents(priv, ds, axes);
	}
	else /* not in proximity */
	{
		int buttons = 0;

		/* reports button up when the device has been
		 * down and becomes out of proximity */
		if (priv->oldState.buttons)
			wcmSendButtons(priv, ds, buttons, axes);

		if (priv->oldState.proximity)
			wcmEmitProximity(priv, FALSE, axes);
	} /* not in proximity */
}

#define IsArtPen(ds)    (ds->device_id == 0x885 || ds->device_id == 0x804 || ds->device_id == 0x100804)

static void
wcmUpdateSerial(WacomDevicePtr priv, unsigned int serial, int id)
{
	if (priv->cur_serial == serial && priv->cur_device_id == id)
		return;

	priv->cur_serial = serial;
	priv->cur_device_id = id;

	wcmUpdateSerialProperty(priv);
}

/*****************************************************************************
 * wcmSendEvents --
 *   Send events according to the device state.
 ****************************************************************************/

void wcmSendEvents(WacomDevicePtr priv, const WacomDeviceState* ds)
{
#ifdef DEBUG
	int is_button = !!(ds->buttons);
#endif
	int type = ds->device_type;
	int id = ds->device_id;
	unsigned int serial = ds->serial_num;
	int x = ds->x;
	int y = ds->y;
	WacomAxisData axes = {0};
	char dump[1024];

	if (priv->serial && serial != priv->serial)
	{
		DBG(10, priv, "serial number"
				" is %u but your system configured %u",
				serial, priv->serial);
		return;
	}

	wcmUpdateSerial(priv, serial, id);

	/* don't move the cursor when going out-prox */
	if (!ds->proximity)
	{
		x = priv->oldState.x;
		y = priv->oldState.y;
	}

	if (ds->proximity)
		wcmRotateAndScaleCoordinates(priv, &x, &y);

	if (!IsPad(priv)) { /* pad doesn't post x/y */
		wcmAxisSet(&axes, WACOM_AXIS_X, x);
		wcmAxisSet(&axes, WACOM_AXIS_Y, y);
		wcmAxisSet(&axes, WACOM_AXIS_PRESSURE, ds->pressure);
	}

	if (type == PAD_ID)
	{
		wcmAxisSet(&axes, WACOM_AXIS_STRIP_X, ds->stripx);
		wcmAxisSet(&axes, WACOM_AXIS_STRIP_Y, ds->stripy);
		wcmAxisSet(&axes, WACOM_AXIS_RING, ds->abswheel);
		wcmAxisSet(&axes, WACOM_AXIS_RING2, ds->abswheel2);
	} else if (IsCursor(priv))
	{
		wcmAxisSet(&axes, WACOM_AXIS_ROTATION, ds->rotation);
		wcmAxisSet(&axes, WACOM_AXIS_THROTTLE, ds->throttle);
	} else
	{
		wcmAxisSet(&axes, WACOM_AXIS_TILT_X, ds->tiltx);
		wcmAxisSet(&axes, WACOM_AXIS_TILT_Y, ds->tilty);
	}


	/* cancel panscroll */
	if (!ds->proximity)
		priv->flags &= ~SCROLLMODE_FLAG;

	if (IsStylus(priv) && !IsArtPen(ds))
	{
		/* Normalize abswheel airbrush data to Art Pen rotation range.
		 * We do not normalize Art Pen. They are already at the range.
		 */
		int wheel = ds->abswheel * MAX_ROTATION_RANGE/
				(double)MAX_ABS_WHEEL + MIN_ROTATION;
		wcmAxisSet(&axes, WACOM_AXIS_WHEEL, wheel);
	} else if (IsStylus(priv) && IsArtPen(ds))
	{
		wcmAxisSet(&axes, WACOM_AXIS_WHEEL, ds->abswheel);
	}

	wcmAxisDump(&axes, dump, sizeof(dump));
	DBG(6, priv, "%s o_prox=%d\tprox=%d\t%s\tid=%d"
		"\tserial=%u\tbutton=%s\tbuttons=%u\n",
		is_absolute(priv) ? "abs" : "rel", priv->oldState.proximity,
		ds->proximity, dump, id, serial, is_button ? "true" : "false",
		ds->buttons);

	/* when entering prox, replace the zeroed-out oldState with a copy of
	 * the current state to prevent jumps. reset the prox and button state
	 * to zero to properly detect changes.
	 */
	if(!priv->oldState.proximity)
	{
		int old_key_state = priv->oldState.keys;

		wcmUpdateOldState(priv, ds, x, y);
		priv->oldState.proximity = 0;
		priv->oldState.buttons = 0;

		/* keys can happen without proximity */
		priv->oldState.keys = old_key_state;
	}

	if (type == PAD_ID)
		wcmSendPadEvents(priv, ds, &axes);
	else {
		/* don't move the cursor if in gesture mode (except drag mode) */
		if ((type != TOUCH_ID) || wcmTouchNeedSendEvents(priv->common))
			wcmSendNonPadEvents(priv, ds, &axes);
	}

	if (ds->proximity)
		wcmUpdateOldState(priv, ds, x, y);
	else
	{
		priv->oldState = OUTPROX_STATE;
		priv->oldState.serial_num = serial;
		priv->oldState.device_id = id;
		wcmUpdateSerial(priv, 0, 0);
	}
}

/**
 * Determine whether device state has changed enough to warrant further
 * processing. The driver's "suppress" setting decides how much
 * movement/state change must occur before we process events to avoid
 * overloading the server with minimal changes (and getting fuzzy events).
 * wcmCheckSuppress ensures that events meet this standard.
 *
 * @param dsOrig Previous device state
 * @param dsNew Current device state
 *
 * @retval SUPPRESS_ALL Ignore this event completely.
 * @retval SUPPRESS_NONE Process event normally.
 * @retval SUPPRESS_NON_MOTION Suppress all data but motion data.
 */
static enum WacomSuppressMode
wcmCheckSuppress(WacomCommonPtr common,
		 const WacomDeviceState* dsOrig,
		 WacomDeviceState* dsNew)
{
	int suppress = common->wcmSuppress;
	enum WacomSuppressMode returnV = SUPPRESS_NONE;

	/* Ignore all other changes that occur after initial out-of-prox. */
	if (!dsNew->proximity && !dsOrig->proximity)
		return SUPPRESS_ALL;

	/* Never ignore proximity changes. */
	if (dsOrig->proximity != dsNew->proximity) goto out;

	if (dsOrig->buttons != dsNew->buttons) goto out;
	if (dsOrig->keys != dsNew->keys) goto out;
	if (dsOrig->stripx != dsNew->stripx) goto out;
	if (dsOrig->stripy != dsNew->stripy) goto out;

	/* FIXME: we should have different suppress values for different
	 * axes with vastly different ranges.
	 */
	if (abs(dsOrig->tiltx - dsNew->tiltx) > suppress) goto out;
	if (abs(dsOrig->tilty - dsNew->tilty) > suppress) goto out;
	if (abs(dsOrig->pressure - dsNew->pressure) > suppress) goto out;
	if (abs(dsOrig->throttle - dsNew->throttle) > suppress) goto out;
	if (abs(dsOrig->rotation - dsNew->rotation) > suppress &&
	    (1800 - abs(dsOrig->rotation - dsNew->rotation)) >  suppress) goto out;

	/* look for change in absolute wheel position
	 * or any relative wheel movement
	 */
	if (abs(dsOrig->abswheel  - dsNew->abswheel)  > suppress) goto out;
	if (abs(dsOrig->abswheel2 - dsNew->abswheel2) > suppress) goto out;
	if (dsNew->relwheel != 0) goto out;

	returnV = SUPPRESS_ALL;

out:
	/* Special handling for cursor: if nothing else changed but the
	 * pointer x/y, suppress all but cursor movement. This return value
	 * is used in commonDispatchDevice to short-cut event processing.
	 */
	if ((abs(dsOrig->x - dsNew->x) > suppress) ||
			(abs(dsOrig->y - dsNew->y) > suppress))
	{
		if (returnV == SUPPRESS_ALL)
			returnV = SUPPRESS_NON_MOTION;
	}
	else /* don't move cursor */
	{
		dsNew->x = dsOrig->x;
		dsNew->y = dsOrig->y;
	}

	DBG(10, common, "level = %d"
		" return value = %u\n", suppress, returnV);
	return returnV;
}

/**
 * Find the device the current events are meant for. If multiple tools are
 * configured on this tablet, the one that matches the serial number for the
 * current device state is returned. If none match, the tool that has a
 * serial of 0 is returned.
 *
 * @param ds The current device state as read from the fd
 * @return The tool that should be used to emit the current events.
 */
static WacomToolPtr findTool(const WacomCommonPtr common,
			     const WacomDeviceState *ds)
{
	WacomToolPtr tooldefault = NULL;
	WacomToolPtr tool = NULL;

	/* 1: Find the tool (the one with correct serial or in second
	 * hand, the one with serial set to 0 if no match with the
	 * specified serial exists) that is used for this event */
	for (tool = common->wcmTool; tool; tool = tool->next)
	{
		if (tool->typeid == ds->device_type)
		{
			if (tool->serial == ds->serial_num)
				break;
			else if (!tool->serial)
				tooldefault = tool;
		}
	}

	/* Use default tool (serial == 0) if no specific was found */
	if (!tool)
		tool = tooldefault;

	return tool;
}

/**
 * Check if the given device should grab control of the pointer in
 * preference to whatever tool currently has access.
 *
 * @param priv   The device to check for access
 * @param ds     The current state of the device
 * @returns      'TRUE' if control of the pointer should be granted, FALSE otherwise
 */
static Bool check_arbitrated_control(WacomDevicePtr priv, WacomDeviceStatePtr ds)
{
	WacomDevicePtr active = WACOM_DRIVER.active;

	if (IsPad(priv)) {
		/* Pad may never be the "active" pointer controller */
		DBG(6, priv, "Event from pad; not yielding pointer control\n.");
		return FALSE;
	}

	if (active == NULL || active->oldState.device_id == ds->device_id) {
		DBG(11, priv, "Event from active device; maintaining pointer control.\n");
		return TRUE;
	}
	else if (IsCursor(active)) {
		/* Cursor devices are often left idle in range, so allow other devices
		 * to grab control if the tool has not been used for some time.
		 */
		Bool yield = (ds->time - active->oldState.time > 100) && (active->oldState.buttons == 0);
		DBG(6, priv, "Currently-active cursor %s idle; %s pointer control.\n",
		    yield ? "is" : "is not", yield ? "yielding" : "not yielding");
		return yield;
	}
	else if (IsCursor(priv)) {
		/* An otherwise idle cursor may still occasionally jitter and send
		 * events while the user is actively using other tools or touching
		 * the device. Do not allow the cursor to grab control in this
		 * particular case.
		 */
		DBG(6, priv, "Event from non-active cursor; not yielding pointer control.\n");
		return FALSE;
	}
	else {
		/* Non-touch input has priority over touch in general */
		Bool yield = !IsTouch(priv);
		DBG(6, priv, "Event from non-active %s device; %s pointer control.\n",
		    yield ? "non-touch" : "touch", yield ? "yielding" : "not yielding");
		return yield;
	}
}

/*****************************************************************************
 * wcmEvent -
 *   Handles suppression, transformation, filtering, and event dispatch.
 ****************************************************************************/

void wcmEvent(WacomCommonPtr common, unsigned int channel,
	const WacomDeviceState* pState)
{
	WacomDeviceState ds;
	WacomChannelPtr pChannel;
	WacomToolPtr tool;
	WacomDevicePtr priv;
	pChannel = common->wcmChannel + channel;

	DBG(10, common, "channel = %u\n", channel);

	/* sanity check the channel */
	if (channel >= MAX_CHANNELS)
		return;

	/* we must copy the state because certain types of filtering
	 * will need to change the values (ie. for error correction) */
	ds = *pState;

	DBG(10, common,
		"c=%u i=%d t=%d s=0x%x x=%d y=%d b=%u "
		"p=%d rz=%d tx=%d ty=%d aw=%d aw2=%d rw=%d "
		"t=%d px=%d st=%d cs=%d \n",
		channel,
		ds.device_id,
		ds.device_type,
		ds.serial_num,
		ds.x, ds.y, ds.buttons,
		ds.pressure, ds.rotation, ds.tiltx,
		ds.tilty, ds.abswheel, ds.abswheel2, ds.relwheel, ds.throttle,
		ds.proximity, ds.sample,
		pChannel->nSamples);

	/* Find the device the current events are meant for */
	tool = findTool(common, &ds);
	if (!tool || !tool->device)
	{
		DBG(11, common, "no device matches with id=%d, serial=%u\n",
		    ds.device_type, ds.serial_num);
		return;
	}

	priv = tool->device;
	/* Tool on the tablet when driver starts. This sometime causes
	 * access errors to the device */
	if (!tool->enabled) {
		wcmLogSafe(priv, W_ERROR, "tool not initialized yet. Skipping event. \n");
		return;
	}

	DBG(11, common, "tool id=%d for %s\n", ds.device_type, priv->name);

	if (TabletHasFeature(common, WCM_ROTATION) &&
		TabletHasFeature(common, WCM_RING) &&
		ds.device_type == CURSOR_ID) /* I4 mouse */
	{
		/* convert Intuos4 mouse tilt to rotation */
		ds.rotation = wcmTilt2R(ds.tiltx, ds.tilty,
					INTUOS4_CURSOR_ROTATION_OFFSET);
		ds.tiltx = 0;
		ds.tilty = 0;
	}

	/* JEJ - Do not move this code without discussing it with me.
	 * The device state is invariant of any filtering performed below.
	 * Changing the device state after this point can and will cause
	 * a feedback loop resulting in oscillations, error amplification,
	 * unnecessary quantization, and other annoying effects. */

	/* save channel device state and device to which last event went */
	memmove(pChannel->valid.states + 1,
		pChannel->valid.states,
		sizeof(WacomDeviceState) * (common->wcmRawSample - 1));
	pChannel->valid.state = ds; /*save last raw sample */
	if (pChannel->nSamples < common->wcmRawSample) ++pChannel->nSamples;

	/* arbitrate pointer control */
	if (check_arbitrated_control(priv, &ds)) {
		if (WACOM_DRIVER.active != NULL && priv != WACOM_DRIVER.active) {
			wcmSoftOutEvent(WACOM_DRIVER.active);
			wcmCancelGesture(WACOM_DRIVER.active);
		}
		if (ds.proximity)
			WACOM_DRIVER.active = priv;
		else
			WACOM_DRIVER.active = NULL;
	}
	else if (!IsPad(priv)) {
		return;
	}

	if ((ds.device_type == TOUCH_ID) && common->wcmTouch)
	{
		wcmGestureFilter(priv, ds.serial_num - 1);
		/*
		 * When using XI 2.2 multitouch events don't do common dispatching
		 * for direct touch devices
		 */
		if (!common->wcmGesture && TabletHasFeature(common, WCM_LCD))
			return;
	}

	/* For touch, only first finger moves the cursor */
	if ((common->wcmTouch && ds.device_type == TOUCH_ID && ds.serial_num == 1) ||
	    (ds.device_type != TOUCH_ID))
		commonDispatchDevice(priv, pChannel);
}


/**
 * Return the minimum pressure based on the current minimum pressure and the
 * hardware state. This is mainly to deal with the case where heavily used
 * stylus may have a "pre-loaded" initial pressure. In that case, the tool
 * comes into proximity with a pressure > 0 to begin with and thus offsets
 * the pressure values. This preloaded pressure must be known for pressure
 * normalisation to work.
 *
 * @param priv The wacom device
 * @param ds Current device state
 *
 * @return The minimum pressure value for this tool.
 *
 * @see normalizePressure
 */
static int
rebasePressure(const WacomDevicePtr priv, const WacomDeviceState *ds)
{
	int min_pressure;

	/* set the minimum pressure when in prox */
	if (!priv->oldState.proximity)
		min_pressure = ds->pressure;
	else
		min_pressure = min(priv->minPressure, ds->pressure);

	return min_pressure;
}

/**
 * Instead of reporting the raw pressure, we normalize
 * the pressure from 0 to maxCurve. This is
 * mainly to deal with the case where heavily used
 * stylus may have a "pre-loaded" initial pressure. To
 * do so, we keep the in-prox pressure and subtract it
 * from the raw pressure to prevent a potential
 * left-click before the pen touches the tablet.
 *
 * @param priv The wacom device
 * @param ds Current device state
 *
 * @return normalized pressure
 * @see rebasePressure
 */
static int
normalizePressure(const WacomDevicePtr priv, const int raw_pressure)
{
	WacomCommonPtr common = priv->common;
	double pressure;
	int p = raw_pressure;
	int range_left = common->wcmMaxZ;

	if (common->wcmPressureRecalibration) {
		p -= priv->minPressure;
		range_left -= priv->minPressure;
	}
	/* normalize pressure to 0..maxCurve */
	if (range_left >= 1)
		pressure = wcmScaleAxis(p, priv->maxCurve, 0, range_left, 0);
	else
		pressure = priv->maxCurve;

	return (int)pressure;
}

/*
 * Based on the current pressure, return the button state with Button1
 * either set or unset, depending on whether the pressure threshold
 * conditions have been met.
 *
 * Returns the state of all buttons, but buttons other than button 1 are
 * unmodified.
 */
#define PRESSURE_BUTTON 1
static int
setPressureButton(const WacomDevicePtr priv, int buttons, const int pressure)
{
	WacomCommonPtr common = priv->common;
	int button = PRESSURE_BUTTON;

	/* button 1 Threshold test */
	/* set button1 (left click) on/off */
	if (pressure < common->wcmThreshold)
	{
		buttons &= ~button;
		if (priv->oldState.buttons & button) /* left click was on */
		{
			/* don't set it off if it is within the tolerance
			   and threshold is larger than the tolerance */
			if ((common->wcmThreshold > (priv->maxCurve * THRESHOLD_TOLERANCE)) &&
			    (pressure > common->wcmThreshold - (priv->maxCurve * THRESHOLD_TOLERANCE)))
				buttons |= button;
		}
	}
	else
		buttons |= button;

	return buttons;
}

/*
 * Broken pen with a broken tip might give high pressure values
 * all the time. We want to warn about this. To avoid getting
 * spurious warnings when the tablet is hit quickly will wait
 * until the device goes out of proximity and check if the minimum
 * pressure is still above a threshold of 20 percent of the maximum
 * pressure. Also we make sure the device has seen a sufficient number
 * of events while in proximity that it had a chance to see decreasing
 * pressure values.
 */
#define LIMIT_LOW_PRESSURE 20 /* percentage of max value */
#define MIN_EVENT_COUNT 15

static void detectPressureIssue(WacomDevicePtr priv,
				WacomCommonPtr common,
				WacomDeviceStatePtr ds)
{
	/* pen is just going out of proximity */
	if (priv->oldState.proximity && !ds->proximity) {

		int pressureThreshold = common->wcmMaxZ * LIMIT_LOW_PRESSURE / 100;
		/* check if minPressure has persisted all the time
		   and is too close to the maximum pressure */
		if (priv->oldMinPressure > pressureThreshold &&
		    priv->eventCnt > MIN_EVENT_COUNT)
			wcmLogSafe(priv, W_WARNING,
				"On %s(%u) a base pressure of %d persists while the pen is in proximity.\n"
				"\tThis is > %d percent of the maximum value (%d).\n"
				"\tThis indicates a worn out pen, it is time to change your tool. Also see:\n"
				"\thttps://github.com/linuxwacom/xf86-input-wacom/wiki/Pen-Wear.\n",
				priv->name, priv->serial, priv->minPressure, LIMIT_LOW_PRESSURE, common->wcmMaxZ);
	} else if (!priv->oldState.proximity)
		priv->eventCnt = 0;

	priv->oldMinPressure = priv->minPressure;
	priv->eventCnt++;
}

static void commonDispatchDevice(WacomDevicePtr priv,
				 const WacomChannelPtr pChannel)
{
	WacomDeviceState* ds = &pChannel->valid.states[0];
	WacomCommonPtr common = priv->common;
	WacomDeviceState filtered;
	enum WacomSuppressMode suppress;
	int raw_pressure = 0;

	/* device_type should have been retrieved and set in the respective
	 * models, e.g. wcmUSB.c. Once it comes here, something
	 * must have been wrong. Ignore the events.
	 */
	if (!ds->device_type)
	{
		DBG(11, common, "no device type matches with"
				" serial=%u\n", ds->serial_num);
		return;
	}

	DBG(10, common, "device type = %d\n", ds->device_type);

	filtered = pChannel->valid.state;

	/* Device transformations come first */
	if (priv->serial && filtered.serial_num != priv->serial)
	{
		DBG(10, priv, "serial number"
			" is %u but your system configured %u\n",
			filtered.serial_num, priv->serial);
		return;
	}

	if ((IsPen(priv) || IsTouch(priv)) && common->wcmMaxZ)
	{
		int prev_min_pressure = priv->oldState.proximity ? priv->minPressure : 0;

		detectPressureIssue(priv, common, &filtered);

		raw_pressure = filtered.pressure;
		if (!priv->oldState.proximity)
			priv->maxRawPressure = raw_pressure;

		priv->minPressure = rebasePressure(priv, &filtered);

		filtered.pressure = normalizePressure(priv, filtered.pressure);
		if (IsPen(priv)) {
			filtered.buttons = setPressureButton(priv,
							     filtered.buttons,
							     filtered.pressure);

			/* Here we run some heuristics to avoid losing button events if the
			 * pen gets pushed onto the tablet so quickly that the first pressure
			 * event read is non-zero and is thus interpreted as a pressure bias */
			if (filtered.buttons & PRESSURE_BUTTON) {
				/* If we triggered 'normally' reset max pressure to
				 * avoid to trigger again while this device is in proximity */
				priv->maxRawPressure = 0;
			} else if (priv->maxRawPressure) {
				int norm_max_pressure;

				/* If we haven't triggered normally we record the maximal pressure
				 * and see if this would have triggered with a lowered bias. */
				if (priv->maxRawPressure < raw_pressure)
					priv->maxRawPressure = raw_pressure;
				norm_max_pressure = normalizePressure(priv, priv->maxRawPressure);
				filtered.buttons = setPressureButton(priv, filtered.buttons,
								     norm_max_pressure);

				/* If minPressure is not decrementing any more or a button
				 * press has been generated or minPressure has just become zero
				 * reset maxRawPressure to avoid that worn devices
				 * won't report a button release until going out of proximity */
				if ((filtered.buttons & PRESSURE_BUTTON &&
				     priv->minPressure == prev_min_pressure) ||
				    !priv->minPressure)
					priv->maxRawPressure = 0;

			}
		}
		filtered.pressure = applyPressureCurve(priv,&filtered);
	}

	/* Optionally filter values only while in proximity */
	if (filtered.proximity && filtered.device_type != PAD_ID)
	{
		/* Start filter fresh when entering proximity */
		if (!priv->oldState.proximity)
			wcmResetSampleCounter(pChannel);

		/* Reset filter whenever the tip is touched to the
		 * screen to ensure clicks are sent from the pen's
		 * actual position. Don't reset on other buttons or
		 * tip-up, or else there may be a noticible jump/
		 * hook produced in the middle/end of the stroke.
		 */
		if ((filtered.buttons & PRESSURE_BUTTON) && !(priv->oldState.buttons & PRESSURE_BUTTON))
			wcmResetSampleCounter(pChannel);

		wcmFilterCoord(common,pChannel,&filtered);
	}

	/* skip event if we don't have enough movement */
	suppress = wcmCheckSuppress(common, &priv->oldState, &filtered);
	if (suppress == SUPPRESS_ALL)
		return;

	/* Store cursor hardware prox for next use */
	if (IsCursor(priv))
		priv->oldCursorHwProx = ds->proximity;

	/* User-requested filtering comes next */

	/* User-requested transformations come last */

	if (!is_absolute(priv) && !IsPad(priv))
	{
		/* To improve the accuracy of relative x/y,
		 * don't send motion event when there is no movement.
		 */
		double deltx = filtered.x - priv->oldState.x;
		double delty = filtered.y - priv->oldState.y;

		/* less than one device coordinate movement? */
		if (fabs(deltx)<1 && fabs(delty)<1)
		{
			/* We have no other data in this event, skip */
			if (suppress == SUPPRESS_NON_MOTION)
			{
				DBG(10, common, "Ignore non-movement relative data \n");
				return;
			}

			filtered.x = priv->oldState.x;
			filtered.y = priv->oldState.y;
		}
	}

	/* force out-prox when distance from surface exceeds wcmProxoutDist */
	if (IsTablet(priv) && !is_absolute(priv))
	{
		/* Assume the the user clicks the puck buttons while
		 * it is resting on the tablet (and taps styli onto
		 * the tablet surface). This works for both
		 * tablets that have a normal distance scale (protocol
		 * 5) as well as those with an inverted scale (protocol
		 * 4 for many many kernel versions).
		 */
		if ((IsCursor(priv) && filtered.buttons) ||
		    (IsStylus(priv) && filtered.buttons & 0x01))
			priv->wcmSurfaceDist = filtered.distance;

		DBG(10, priv, "Distance over"
				" the tablet: %d, ProxoutDist: %d current"
				" surface %d hard prox: %d\n",
				filtered.distance,
				priv->wcmProxoutDist,
				priv->wcmSurfaceDist,
				ds->proximity);

		if (priv->wcmSurfaceDist >= 0) {
			if (priv->oldState.proximity)
			{
				if (abs(filtered.distance - priv->wcmSurfaceDist)
						> priv->wcmProxoutDist)
					filtered.proximity = 0;
			}
			/* once it is out. Don't let it in until a hard in */
			/* or it gets inside wcmProxoutDist */
			else
			{
				if (abs(filtered.distance - priv->wcmSurfaceDist) >
						priv->wcmProxoutDist && ds->proximity)
					return;
				if (!ds->proximity)
					return;
			}
		}
	}
	wcmSendEvents(priv, &filtered);
}

/*****************************************************************************
 * wcmInitTablet -- common initialization for all tablets
 ****************************************************************************/

int wcmInitTablet(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomModelPtr model = common->wcmModel;

	/* Initialize the tablet */
	if (model->Initialize(priv) != Success)
		return !Success;

	/* Default threshold value if not set */
	if (common->wcmThreshold <= 0 && IsPen(priv))
	{
		/* Threshold for counting pressure as a button */
		common->wcmThreshold = priv->maxCurve * DEFAULT_THRESHOLD;

		wcmLog(priv, W_PROBED, "using pressure threshold of %d for button 1\n",
			    common->wcmThreshold);
	}

	/* Calculate default panscroll threshold if not set */
	wcmLog(priv, W_CONFIG, "panscroll is %d\n", common->wcmPanscrollThreshold);
	if (common->wcmPanscrollThreshold < 1) {
		common->wcmPanscrollThreshold = common->wcmResolY * 13 / 1000; /* 13mm */
	}
	if (common->wcmPanscrollThreshold < 1) {
		common->wcmPanscrollThreshold = 1000;
	}
	wcmLog(priv, W_CONFIG, "panscroll modified to %d\n", common->wcmPanscrollThreshold);

	/* output tablet state as probed */
	if (IsPen(priv))
		wcmLog(priv, W_PROBED, "maxX=%d maxY=%d maxZ=%d "
			"resX=%d resY=%d  tilt=%s\n",
			common->wcmMaxX, common->wcmMaxY, common->wcmMaxZ,
			common->wcmResolX, common->wcmResolY,
			HANDLE_TILT(common) ? "enabled" : "disabled");
	else if (IsTouch(priv))
		wcmLog(priv, W_PROBED, "maxX=%d maxY=%d maxZ=%d "
			"resX=%d resY=%d \n",
			common->wcmMaxTouchX, common->wcmMaxTouchY,
			common->wcmMaxZ,
			common->wcmTouchResolX, common->wcmTouchResolY);

	return Success;
}

/* Send a soft prox-out event for the device */
void wcmSoftOutEvent(WacomDevicePtr priv)
{
	WacomDeviceState out = OUTPROX_STATE;

	out.device_type = DEVICE_ID(priv->flags);
	out.device_id = wcmGetPhyDeviceID(priv);
	DBG(2, priv->common, "send a soft prox-out\n");
	wcmSendEvents(priv, &out);
}

/*****************************************************************************
** Transformations
*****************************************************************************/

/**
 * Apply the current pressure curve to the current pressure.
 *
 * @return The modified pressure value.
 */
static int applyPressureCurve(WacomDevicePtr pDev, const WacomDeviceStatePtr pState)
{
	/* clip the pressure */
	int p = max(0, pState->pressure);

	p = min(pDev->maxCurve, p);

	/* apply pressure curve function */
	if (pDev->pPressCurve == NULL)
		return p;
	else
		return pDev->pPressCurve[p];
}

/*****************************************************************************
 * wcmRotateTablet
 ****************************************************************************/

void wcmRotateTablet(WacomDevicePtr priv, int value)
{
	WacomCommonPtr common = priv->common;
	WacomToolPtr tool;

	DBG(10, priv, "\n");
	common->wcmRotate = value;

	/* Only try updating properties once we're enabled, no point
	 * otherwise. */
	tool = priv->tool;
	if (tool->enabled)
		wcmUpdateRotationProperty(priv);
}

/* Common pointer refcounting utilities.
 * Common is shared across all wacom devices off the same port. These
 * functions implement basic refcounting to avoid double-frees and memleaks.
 *
 * Usage:
 *  wcmNewCommon() to create a new struct.
 *  wcmRefCommon() to get a new reference to an already exiting one.
 *  wcmFreeCommon() to unref. After the last ref has been unlinked, the
 *  struct is freed.
 *
 */

WacomCommonPtr wcmNewCommon(void)
{
	WacomCommonPtr common;
	common = calloc(1, sizeof(WacomCommonRec));
	if (!common)
		return NULL;;

	common->is_common_rec = true;
	common->refcnt = 1;
	common->wcmFlags = 0;               /* various flags */
	common->wcmProtocolLevel = WCM_PROTOCOL_4; /* protocol level */
	common->wcmTPCButton = 0;          /* set Tablet PC button on/off */
	common->wcmGestureParameters.wcmScrollDirection = 0;
	common->wcmGestureParameters.wcmTapTime = 250;
	common->wcmRotate = ROTATE_NONE;   /* default tablet rotation to off */
	common->wcmMaxX = 0;               /* max digitizer logical X value */
	common->wcmMaxY = 0;               /* max digitizer logical Y value */
	common->wcmMaxTouchX = 1024;       /* max touch X value */
	common->wcmMaxTouchY = 1024;       /* max touch Y value */
	common->wcmMaxStripX = 4096;       /* Max fingerstrip X */
	common->wcmMaxStripY = 4096;       /* Max fingerstrip Y */
	common->wcmProxoutDistDefault = PROXOUT_INTUOS_DISTANCE;
			/* default to Intuos */
	common->wcmSuppress = DEFAULT_SUPPRESS;
			/* transmit position if increment is superior */
	common->wcmRawSample = DEFAULT_SAMPLES;
			/* number of raw data to be used to for filtering */
	common->wcmPanscrollThreshold = 0;
	common->wcmPressureRecalibration = 1;
	return common;
}


void wcmFreeCommon(WacomCommonPtr *ptr)
{
	WacomCommonPtr common = *ptr;

	if (!common)
		return;

	DBG(10, common, "common refcount dec to %d\n", common->refcnt - 1);
	if (--common->refcnt == 0)
	{
		free(common->private);
		while (common->serials)
		{
			WacomToolPtr next;

			DBG(10, common, "Free common serial: %u %s\n",
					common->serials->serial,
					common->serials->name);

			free(common->serials->name);
			next = common->serials->next;
			free(common->serials);
			common->serials = next;
		}
		free(common->device_path);
		free(common->touch_mask);
		free(common);
	}
	*ptr = NULL;
}

WacomCommonPtr wcmRefCommon(WacomCommonPtr common)
{
	if (!common)
		common = wcmNewCommon();
	else
		common->refcnt++;
	DBG(10, common, "common refcount inc to %d\n", common->refcnt);
	return common;
}

#ifdef ENABLE_TESTS

TEST_CASE(test_get_scroll_delta)
{
	int test_table[][5] = {
		{ 100,  25, 0, 0,  75}, { 25,  100, 0, 0, -75},
		{-100, -25, 0, 0, -75}, {-25, -100, 0, 0,  75},
		{ 100, -25, 0, 0, 125}, {-25,  100, 0, 0,-125},
		{ 100, 100, 0, 0,   0}, {-25,  -25, 0, 0,   0},

		{23, 0, 50, 0,  23}, {0, 23, 50, 0, -23},
		{24, 0, 50, 0,  24}, {0, 24, 50, 0, -24},
		{25, 0, 50, 0,  25}, {0, 25, 50, 0, -25},
		{26, 0, 50, 0, -25}, {0, 26, 50, 0,  25},
		{27, 0, 50, 0, -24}, {0, 27, 50, 0,  24},
		{28, 0, 50, 0, -23}, {0, 28, 50, 0,  23},

		{1024, 0, 0, AXIS_BITWISE, 11}, {0, 1024, 0, AXIS_BITWISE, -11},

		{  0, 4, 256, AXIS_BITWISE, -3}, {4,   0, 256, AXIS_BITWISE,  3},
		{  1, 4, 256, AXIS_BITWISE, -2}, {4,   1, 256, AXIS_BITWISE,  2},
		{  2, 4, 256, AXIS_BITWISE, -1}, {4,   2, 256, AXIS_BITWISE,  1},
		{  4, 4, 256, AXIS_BITWISE,  0}, {4,   4, 256, AXIS_BITWISE,  0},
		{  8, 4, 256, AXIS_BITWISE,  1}, {4,   8, 256, AXIS_BITWISE, -1},
		{ 16, 4, 256, AXIS_BITWISE,  2}, {4,  16, 256, AXIS_BITWISE, -2},
		{ 32, 4, 256, AXIS_BITWISE,  3}, {4,  32, 256, AXIS_BITWISE, -3},
		{ 64, 4, 256, AXIS_BITWISE,  4}, {4,  64, 256, AXIS_BITWISE, -4},
		{128, 4, 256, AXIS_BITWISE,  5}, {4, 128, 256, AXIS_BITWISE, -5},
		{256, 4, 256, AXIS_BITWISE, -4}, {4, 256, 256, AXIS_BITWISE,  4}
	};

	for (size_t i = 0; i < ARRAY_SIZE(test_table); i++)
	{
		int delta;
		int current, old, wrap, flags;
		current = test_table[i][0];
		old     = test_table[i][1];
		wrap    = test_table[i][2];
		flags   = test_table[i][3];

		delta = getScrollDelta(current, old, wrap, flags);
		assert(delta == test_table[i][4]);

		flags |= AXIS_INVERT;
		delta = getScrollDelta(current, old, wrap, flags);
		assert(delta == -1 * test_table[i][4]);
	}
}

TEST_CASE(test_get_wheel_button)
{
	int delta;
	int action_up, action_dn;

	action_up = 300;
	action_dn = 400;

	for (delta = -32; delta <= 32; delta++)
	{
		int action;
		action = getWheelButton(delta, action_up, action_dn);
		if (delta < 0)
		{
			assert(action == action_dn);
		}
		else if (delta == 0)
		{
			assert(action == -1);
		}
		else
		{
			assert(action == action_up);
		}
	}
}


TEST_CASE(test_common_ref)
{
	WacomCommonPtr common;
	WacomCommonPtr second;

	common = wcmNewCommon();
	assert(common);
	assert(common->refcnt == 1);

	second = wcmRefCommon(common);

	assert(second == common);
	assert(second->refcnt == 2);

	wcmFreeCommon(&second);
	assert(common);
	assert(!second);
	assert(common->refcnt == 1);

	second = wcmRefCommon(NULL);
	assert(common != second);
	assert(second->refcnt == 1);
	assert(common->refcnt == 1);

	wcmFreeCommon(&second);
	wcmFreeCommon(&common);
	assert(!second && !common);
}

TEST_CASE(test_rebase_pressure)
{
	WacomDeviceRec priv = {0};
	WacomDeviceRec base = {0};
	WacomDeviceState ds = {0};
	int pressure;

	priv.minPressure = 4;
	ds.pressure = 10;

	/* Pressure in out-of-proximity means get new preloaded pressure */
	priv.oldState.proximity = 0;

	/* make sure we don't touch priv, not really needed, the compiler should
	 * honor the consts but... */
	base = priv;

	pressure = rebasePressure(&priv, &ds);
	assert(pressure == ds.pressure);

	assert(memcmp(&priv, &base, sizeof(priv)) == 0);

	/* Pressure in-proximity means rebase to new minimum */
	priv.oldState.proximity = 1;

	base = priv;

	pressure = rebasePressure(&priv, &ds);
	assert(pressure == priv.minPressure);
	assert(memcmp(&priv, &base, sizeof(priv)) == 0);
}

TEST_CASE(test_normalize_pressure)
{
	InputInfoRec pInfo = {0};
	WacomDeviceRec priv = {0};
	WacomCommonRec common = {0};
	int normalized_max = 65536;
	int pressure = 0, prev_pressure = -1;
	int i, j, k;

	priv.common = &common;
	priv.frontend = &pInfo;
	pInfo.name = strdupa("Wacom test device");
	common.wcmPressureRecalibration = 1;

	priv.minPressure = 0;

	/* Check various maxCurve values */
	for (k = 512; k <= normalized_max; k += 239) {
		priv.maxCurve = k;

		/* Some random loop to check various maxZ pressure values. Starting at
		 * 1, because if wcmMaxZ is 0 we have other problems. */
		for (j = 1; j <= 256; j += 17)
		{
			common.wcmMaxZ = j;
			prev_pressure = -1;

			for (i = 0; i <= common.wcmMaxZ; i++)
			{
				pressure = i;

				pressure = normalizePressure(&priv, pressure);
				assert(pressure >= 0);
				assert(pressure <= k);

				/* we count up, so assume normalised pressure goes up too */
				assert(prev_pressure < pressure);
				prev_pressure = pressure;
			}

			assert(pressure == k);
		}
	}

	/* If minPressure is higher than ds->pressure, normalizePressure takes
	 * minPressure and ignores actual pressure. This would be a bug in the
	 * driver code, but we might as well test for it. */
	priv.minPressure = 10;
	priv.maxCurve = normalized_max;

	prev_pressure = normalizePressure(&priv, 0);
	for (i = 0; i < priv.minPressure; i++)
	{

		pressure = normalizePressure(&priv, i);

		assert(pressure >= 0);
		assert(pressure < normalized_max);

		/* we count up, so assume normalised pressure goes up too */
		assert(prev_pressure == pressure);
	}
}

TEST_CASE(test_suppress)
{
	enum WacomSuppressMode rc;
	WacomCommonRec common = {0};
	WacomDeviceState old = {0},
			 new = {0};

	common.wcmSuppress = 2;

	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_ALL);

	/* proximity, buttons and strip send for any change */

#define test_any_suppress(field) \
	old.field = 1; \
	rc = wcmCheckSuppress(&common, &old, &new); \
	assert(rc == SUPPRESS_NONE); \
	new.field = old.field;

	test_any_suppress(proximity);
	test_any_suppress(buttons);
	test_any_suppress(stripx);
	test_any_suppress(stripy);

#undef test_any_suppress

	/* pressure, capacity, throttle, rotation, abswheel only when
	 * difference is above suppress */

	/* test negative and positive transition */
#define test_above_suppress(field) \
	old.field = common.wcmSuppress; \
	rc = wcmCheckSuppress(&common, &old, &new); \
	assert(rc == SUPPRESS_ALL); \
	old.field = common.wcmSuppress + 1; \
	rc = wcmCheckSuppress(&common, &old, &new); \
	assert(rc == SUPPRESS_NONE); \
	old.field = -common.wcmSuppress; \
	rc = wcmCheckSuppress(&common, &old, &new); \
	assert(rc == SUPPRESS_ALL); \
	old.field = -common.wcmSuppress - 1; \
	rc = wcmCheckSuppress(&common, &old, &new); \
	assert(rc == SUPPRESS_NONE); \
	new.field = old.field;

	test_above_suppress(pressure);
	test_above_suppress(throttle);
	test_above_suppress(rotation);
	test_above_suppress(abswheel);

#undef test_above_suppress

	/* any movement on relwheel counts */
	new.relwheel = 1;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_NONE);
	new.relwheel = 0;

	/* x axis movement */

	/* not enough movement */
	new.x = common.wcmSuppress;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_ALL);
	assert(old.x == new.x);
	assert(old.y == new.y);

	/* only x axis above thresh */
	new.x = common.wcmSuppress + 1;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_NON_MOTION);

	/* x and other field above thres */
	new.pressure = ~old.pressure;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_NONE);

	new.pressure = old.pressure;
	new.x = old.x;

	/* y axis movement */
	new.y = common.wcmSuppress;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_ALL);
	assert(old.x == new.x);
	assert(old.y == new.y);

	new.y = common.wcmSuppress + 1;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_NON_MOTION);

	new.pressure = ~old.pressure;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_NONE);
	new.pressure = old.pressure;
}


#endif

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
