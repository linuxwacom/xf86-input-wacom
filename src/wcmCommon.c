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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xf86Wacom.h"
#include "Xwacom.h"
#include "wcmFilter.h"
#include <xkbsrv.h>
#include <xf86_OSproc.h>

/* Tested result for setting the pressure threshold to a reasonable value */
#define THRESHOLD_TOLERANCE (FILTER_PRESSURE_RES / 125)
#define DEFAULT_THRESHOLD (FILTER_PRESSURE_RES / 75)

/* X servers pre 1.9 didn't copy data passed into xf86Post*Event.
 * Data passed in would be modified, requiring the driver to copy the
 * data beforehand.
 */
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 11
static int v[MAX_VALUATORS];
static int *VCOPY(const int *valuators, int nvals)
{
	memcpy(v, valuators, nvals * sizeof(int));
	return v;
}
#else /* ABI >= 11 */
#define VCOPY(vals, nval) (vals)
#endif



/*****************************************************************************
 * Static functions
 ****************************************************************************/

static int applyPressureCurve(WacomDevicePtr pDev, const WacomDeviceStatePtr pState);
static void commonDispatchDevice(WacomCommonPtr common,
				 unsigned int channel,
				 const WacomChannelPtr pChannel,
				 enum WacomSuppressMode suppress);
static void sendAButton(InputInfoPtr pInfo, int button, int mask,
			int first_val, int num_vals, int *valuators);

/*****************************************************************************
 * Utility functions
 ****************************************************************************/

/**
 * @return TRUE if the device is set to abolute mode, or FALSE otherwise
 */
Bool is_absolute(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	return !!(priv->flags & ABSOLUTE_FLAG);
}

/**
 * Set the device to absolute or relative mode
 *
 * @param absolute TRUE to set the device to absolute mode.
 */
void set_absolute(InputInfoPtr pInfo, Bool absolute)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;

	if (absolute)
		priv->flags |= ABSOLUTE_FLAG;
	else
		priv->flags &= ~ABSOLUTE_FLAG;
}

/*****************************************************************************
 * wcmMappingFactor --
 *   calculate the proper tablet to screen mapping factor according to the 
 *   screen/desktop size and the tablet size 
 ****************************************************************************/

void wcmMappingFactor(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	double size_x, size_y;

	DBG(10, priv, "\n"); /* just prints function name */

	DBG(10, priv,
		"Active tablet area x=%d y=%d map"
		" to maxWidth =%d maxHeight =%d\n",
		priv->bottomX, priv->bottomY,
		priv->maxWidth, priv->maxHeight);

	/* bottomX/bottomY are scaled values of maxX/maxY such that it
	 * will scale tablet to screen ratio when passed to xf86AxisScale().
	 * Use this to compute similar factor for scaling in relative
	 * mode.  If screen:tablet are 1:1 ratio then no scaling.
	 */

	size_x = priv->bottomX - priv->topX;
	size_y = priv->bottomY - priv->topY;

	priv->factorX = size_x / priv->bottomX;
	priv->factorY = size_y / priv->bottomY;
	DBG(2, priv, "X factor = %.3g, Y factor = %.3g\n",
		priv->factorX, priv->factorY);
}

/*****************************************************************************
 * wcmSendButtons --
 *   Send button events by comparing the current button mask with the
 *   previous one.
 ****************************************************************************/

static void wcmSendButtons(InputInfoPtr pInfo, int buttons,
			   int first_val, int num_vals, int *valuators)
{
	int button, mask, first_button;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomCommonPtr common = priv->common;
	DBG(6, priv, "buttons=%d\n", buttons);

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
		else if ((buttons & 1) != (priv->oldButtons & 1))
			priv->oldButtons = 0;
		/* other button changed while tip is still down? release tip */
		else if ((buttons & 1) && (buttons != priv->oldButtons))
		{
			buttons &= ~1;
			first_button = 0;
		}
	}

	for (button = first_button; button < WCM_MAX_BUTTONS; button++)
	{
		mask = 1 << button;
		if ((mask & priv->oldButtons) != (mask & buttons))
			sendAButton(pInfo, button, (mask & buttons),
					first_val, num_vals, valuators);
	}

}

void wcmEmitKeycode (DeviceIntPtr keydev, int keycode, int state)
{
	xf86PostKeyboardEvent (keydev, keycode, state);
}

/*****************************************************************************
 * countPresses
 *   Count the number of key/button presses not released for the given key
 *   array.
 ****************************************************************************/
static int countPresses(int keybtn, unsigned int* keys, int size)
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

static void sendAction(InputInfoPtr pInfo, int press,
		       unsigned int *keys, int nkeys,
		       int first_val, int num_val, int *valuators)
{
	int i;

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
					xf86PostButtonEventP(pInfo->dev,
							    is_absolute(pInfo), btn_no,
							    is_press, first_val, num_val,
							    VCOPY(valuators, num_val));
				}
				break;
			case AC_KEY:
				{
					int key_code = (action & AC_CODE);
					int is_press = (action & AC_KEYBTNPRESS);
					wcmEmitKeycode(pInfo->dev, key_code, is_press);
				}
				break;
			case AC_MODETOGGLE:
				if (press)
					wcmDevSwitchModeCall(pInfo,
							(is_absolute(pInfo)) ? Relative : Absolute); /* not a typo! */
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
						xf86PostButtonEventP(pInfo->dev,
								is_absolute(pInfo), btn_no,
								0, first_val, num_val,
								VCOPY(valuators, num_val));
				}
				break;
			case AC_KEY:
				{
					int key_code = (action & AC_CODE);

					/* don't care about releases here */
					if (!(action & AC_KEYBTNPRESS))
						break;

					if (countPresses(key_code, &keys[i], nkeys - i))
						wcmEmitKeycode(pInfo->dev, key_code, 0);
				}
		}

	}
}

/*****************************************************************************
 * sendAButton --
 *   Send one button event, called by wcmSendButtons
 ****************************************************************************/
static void sendAButton(InputInfoPtr pInfo, int button, int mask,
			int first_val, int num_val, int *valuators)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
#ifdef DEBUG
	WacomCommonPtr common = priv->common;
#endif
	int mapped_button;

	if (!priv->button[button])  /* ignore this button event */
		return;

	mapped_button = priv->button[button];

	DBG(4, priv, "TPCButton(%s) button=%d state=%d "
		"mapped_button=%d, coreEvent=%s \n",
		common->wcmTPCButton ? "on" : "off",
		button, mask, mapped_button,
		(mapped_button & AC_CORE) ? "yes" : "no");

	if (!priv->keys[mapped_button][0])
	{
		/* No button action configured, send button */
		xf86PostButtonEventP(pInfo->dev, is_absolute(pInfo),
				     mapped_button, (mask != 0),
				     first_val, num_val,
				     VCOPY(valuators, num_val));
		return;
	}

	sendAction(pInfo, (mask != 0), priv->keys[mapped_button],
		   ARRAY_SIZE(priv->keys[mapped_button]),
		   first_val, num_val, valuators);
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
		current = (int)log2((current << 1) | 0x01);
		old = (int)log2((old << 1) | 0x01);
		wrap = (int)log2((wrap << 1) | 0x01);
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
 * @param button_up    Button event to send on scroll up
 * @param button_dn    Button event to send on scroll down
 * @param action_up    Action to send on scroll up
 * @param action_dn    Action to send on scroll down
 * @param[out] action  Action that should be performed
 * @return             Button that should be pressed
 */
static int getWheelButton(int delta, int button_up, int button_dn,
                          unsigned int *action_up, unsigned int *action_dn,
                          unsigned int **action)
{
	int button = 0;
	*action = NULL;

	if (delta)
	{
		button  = delta > 0 ? button_up : button_dn;
		*action = delta > 0 ? action_up : action_dn;
	}

	return button;
}

/**
 * Send button or actions for a scrolling axis.
 *
 * @param button     X button number to send if no action is defined
 * @param action     Action to send
 * @param pInfo
 * @param first_val  
 * @param num_vals
 * @param valuators
 */
static void sendWheelStripEvent(int button, unsigned int *action, InputInfoPtr pInfo,
                                 int first_val, int num_vals, int *valuators)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;

	unsigned int button_action[1] = {button | AC_BUTTON | AC_KEYBTNPRESS};
	if (!action || !(*action)) {
		DBG(10, priv, "No wheel/strip action set; sending button %d (action %d).\n", button, button_action[0]);
		action = &button_action[0];
	}

	sendAction(pInfo, 1, action, ARRAY_SIZE(action), first_val, num_vals, valuators);
	sendAction(pInfo, 0, action, ARRAY_SIZE(action), first_val, num_vals, valuators);
}

/*****************************************************************************
 * sendWheelStripEvents --
 *   Send events defined for relative/absolute wheels or strips
 ****************************************************************************/

static void sendWheelStripEvents(InputInfoPtr pInfo, const WacomDeviceState* ds,
				 int first_val, int num_vals, int *valuators)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int fakeButton = 0, delta = 0;
	unsigned int *fakeKey = NULL;

	DBG(10, priv, "\n");

	/* emulate events for left strip */
	delta = getScrollDelta(ds->stripx, priv->oldStripX, 0, AXIS_INVERT | AXIS_BITWISE);
	if (delta && IsPad(priv) && priv->oldProximity == ds->proximity)
	{
		DBG(10, priv, "Left touch strip scroll delta = %d\n", delta);
		fakeButton = getWheelButton(delta, priv->striplup, priv->stripldn,
		                            priv->strip_keys[0+1], priv->strip_keys[1+1], &fakeKey);
		sendWheelStripEvent(fakeButton, fakeKey, pInfo, first_val, num_vals, valuators);
	}

	/* emulate events for right strip */
	delta = getScrollDelta(ds->stripy, priv->oldStripY, 0, AXIS_INVERT | AXIS_BITWISE);
	if (delta && IsPad(priv) && priv->oldProximity == ds->proximity)
	{
		DBG(10, priv, "Right touch strip scroll delta = %d\n", delta);
		fakeButton = getWheelButton(delta, priv->striprup, priv->striprdn,
		                            priv->strip_keys[2+1], priv->strip_keys[3+1], &fakeKey);
		sendWheelStripEvent(fakeButton, fakeKey, pInfo, first_val, num_vals, valuators);
	}

	/* emulate events for relative wheel */
	delta = getScrollDelta(ds->relwheel, 0, 0, 0);
	if (delta && IsCursor(priv) && priv->oldProximity == ds->proximity)
	{
		DBG(10, priv, "Relative wheel scroll delta = %d\n", delta);
		fakeButton = getWheelButton(delta, priv->relup, priv->reldn,
		                            priv->wheel_keys[0+1], priv->wheel_keys[1+1], &fakeKey);
		sendWheelStripEvent(fakeButton, fakeKey, pInfo, first_val, num_vals, valuators);
	}

	/* emulate events for left touch ring */
	delta = getScrollDelta(ds->abswheel, priv->oldWheel, MAX_PAD_RING, AXIS_INVERT);
	if (delta && IsPad(priv) && priv->oldProximity == ds->proximity)
	{
		DBG(10, priv, "Left touch wheel scroll delta = %d\n", delta);
		fakeButton = getWheelButton(delta, priv->wheelup, priv->wheeldn,
		                            priv->wheel_keys[2+1], priv->wheel_keys[3+1], &fakeKey);
		sendWheelStripEvent(fakeButton, fakeKey, pInfo, first_val, num_vals, valuators);
	}

	/* emulate events for right touch ring */
	delta = getScrollDelta(ds->abswheel2, priv->oldWheel2, MAX_PAD_RING, AXIS_INVERT);
	if (delta && IsPad(priv) && priv->oldProximity == ds->proximity)
	{
		DBG(10, priv, "Right touch wheel scroll delta = %d\n", delta);
		fakeButton = getWheelButton(delta, priv->wheel2up, priv->wheel2dn,
		                            priv->wheel_keys[4+1], priv->wheel_keys[5+1], &fakeKey);
		sendWheelStripEvent(fakeButton, fakeKey, pInfo, first_val, num_vals, valuators);
	}
}

/*****************************************************************************
 * sendCommonEvents --
 *   Send events common between pad and stylus/cursor/eraser.
 ****************************************************************************/

static void sendCommonEvents(InputInfoPtr pInfo, const WacomDeviceState* ds,
			     int first_val, int num_vals, int *valuators)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int buttons = ds->buttons;

	/* send button events when state changed or first time in prox and button unpresses */
	if (priv->oldButtons != buttons || (!priv->oldProximity && !buttons))
		wcmSendButtons(pInfo,buttons, first_val, num_vals, valuators);

	/* emulate wheel/strip events when defined */
	if ( ds->relwheel || (ds->abswheel != priv->oldWheel) || (ds->abswheel2 != priv->oldWheel2) ||
		( (ds->stripx - priv->oldStripX) && ds->stripx && priv->oldStripX) || 
			((ds->stripy - priv->oldStripY) && ds->stripy && priv->oldStripY) )
		sendWheelStripEvents(pInfo, ds, first_val, num_vals, valuators);
}

/* rotate x and y before post X inout events */
void wcmRotateAndScaleCoordinates(InputInfoPtr pInfo, int* x, int* y)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomCommonPtr common = priv->common;
	DeviceIntPtr dev = pInfo->dev;
	AxisInfoPtr axis_x, axis_y;
	int tmp_coord;

	/* scale into on topX/topY area */
	axis_x = &dev->valuator->axes[0];
	axis_y = &dev->valuator->axes[1];

	/* Don't try to scale relative axes */
	if (axis_x->max_value > axis_x->min_value)
		*x = xf86ScaleAxis(*x, axis_x->max_value, axis_x->min_value,
				   priv->bottomX, priv->topX);

	if (axis_y->max_value > axis_y->min_value)
		*y = xf86ScaleAxis(*y, axis_y->max_value, axis_y->min_value,
				   priv->bottomY, priv->topY);

	/* coordinates are now in the axis rage we advertise for the device */

	if (common->wcmRotate == ROTATE_CW || common->wcmRotate == ROTATE_CCW)
	{
		tmp_coord = *x;

		*x = xf86ScaleAxis(*y,
				   axis_x->max_value, axis_x->min_value,
				   axis_y->max_value, axis_y->min_value);
		*y = xf86ScaleAxis(tmp_coord,
				   axis_y->max_value, axis_y->min_value,
				   axis_x->max_value, axis_x->min_value);
	}

	if (common->wcmRotate == ROTATE_CW)
		*y = axis_y->max_value - (*y - axis_y->min_value);
	else if (common->wcmRotate == ROTATE_CCW)
		*x = axis_x->max_value - (*x - axis_x->min_value);
	else if (common->wcmRotate == ROTATE_HALF)
	{
		*x = axis_x->max_value - (*x - axis_x->min_value);
		*y = axis_y->max_value - (*y - axis_y->min_value);
	}


	DBG(10, priv, "rotate/scaled to %d/%d\n", *x, *y);
}

static void wcmUpdateOldState(const InputInfoPtr pInfo,
			      const WacomDeviceState *ds)
{
	const WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int tx, ty;

	priv->oldWheel = ds->abswheel;
	priv->oldWheel2 = ds->abswheel2;
	priv->oldButtons = ds->buttons;

	if (IsPad(priv))
	{
		tx = ds->stripx;
		ty = ds->stripy;
	} else
	{
		tx = ds->tiltx;
		ty = ds->tilty;
	}

	priv->oldX = priv->currentX;
	priv->oldY = priv->currentY;
	priv->oldZ = ds->pressure;
	priv->oldTiltX = tx;
	priv->oldTiltY = ty;
	priv->oldStripX = ds->stripx;
	priv->oldStripY = ds->stripy;
	priv->oldRot = ds->rotation;
	priv->oldThrottle = ds->throttle;
}

static void
wcmSendPadEvents(InputInfoPtr pInfo, const WacomDeviceState* ds,
		 int first_val, int num_vals, int *valuators)
{
	int i;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;

	if (!priv->oldProximity && ds->proximity)
		xf86PostProximityEventP(pInfo->dev, 1, first_val, num_vals, VCOPY(valuators, num_vals));

	for (i = 0; i < num_vals; i++)
		if (valuators[i])
			break;
	if (i < num_vals || ds->buttons || ds->relwheel ||
	    (ds->abswheel != priv->oldWheel) || (ds->abswheel2 != priv->oldWheel2))
	{
		sendCommonEvents(pInfo, ds, first_val, num_vals, valuators);

		/* xf86PostMotionEvent is only needed to post the valuators
		 * It should NOT move the cursor.
		 */
		xf86PostMotionEventP(pInfo->dev, TRUE, first_val, num_vals,
				     VCOPY(valuators, num_vals));
	}
	else
	{
		if (priv->oldButtons)
			wcmSendButtons(pInfo, ds->buttons, first_val, num_vals, valuators);
	}

	if (priv->oldProximity && !ds->proximity)
		xf86PostProximityEventP(pInfo->dev, 0, first_val, num_vals,
					VCOPY(valuators, num_vals));
}

/* Send events for all tools but pads */
static void
wcmSendNonPadEvents(InputInfoPtr pInfo, const WacomDeviceState *ds,
		    int first_val, int num_vals, int *valuators)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;

	if (!is_absolute(pInfo))
	{
		valuators[0] -= priv->oldX;
		valuators[1] -= priv->oldY;
		valuators[2] -= priv->oldZ;
		if (IsCursor(priv))
		{
			valuators[3] -= priv->oldRot;
			valuators[4] -= priv->oldThrottle;
		} else
		{
			valuators[3] -= priv->oldTiltX;
			valuators[4] -= priv->oldTiltY;
		}
		valuators[5] -= priv->oldWheel;
		valuators[6] -= priv->oldWheel2;
	}

	/* coordinates are ready we can send events */
	if (ds->proximity)
	{
		/* unify acceleration in both directions
		 * for relative mode to draw a circle
		 */
		if (!is_absolute(pInfo))
			valuators[0] *= priv->factorY / priv->factorX;
		else
		{
			/* Padding virtual values */
			wcmVirtualTabletPadding(pInfo);
			valuators[0] += priv->leftPadding;
			valuators[1] += priv->topPadding;
		}

		/* don't emit proximity events if device does not support proximity */
		if ((pInfo->dev->proximity && !priv->oldProximity))
			xf86PostProximityEventP(pInfo->dev, 1, first_val, num_vals,
						VCOPY(valuators, num_vals));

		/* Move the cursor to where it should be before sending button events */
		if(!(priv->flags & BUTTONS_ONLY_FLAG))
		{
			xf86PostMotionEventP(pInfo->dev, is_absolute(pInfo),
					     first_val, num_vals,
					     VCOPY(valuators, num_vals));
			/* For relative events, do not repost
			 * the valuators.  Otherwise, a button
			 * event in sendCommonEvents will move the
			 * axes again.
			 */
			if (!is_absolute(pInfo))
			{
				first_val = 0;
				num_vals = 0;
			}
		}

		sendCommonEvents(pInfo, ds, first_val, num_vals, valuators);
	}
	else /* not in proximity */
	{
		int buttons = 0;

		/* reports button up when the device has been
		 * down and becomes out of proximity */
		if (priv->oldButtons)
			wcmSendButtons(pInfo, buttons, first_val, num_vals, valuators);

		if (priv->oldProximity)
			xf86PostProximityEventP(pInfo->dev, 0, first_val, num_vals,
						VCOPY(valuators, num_vals));
	} /* not in proximity */
}

#define IsArtPen(ds)    (ds->device_id == 0x885 || ds->device_id == 0x804 || ds->device_id == 0x100804)

/*****************************************************************************
 * wcmSendEvents --
 *   Send events according to the device state.
 ****************************************************************************/

void wcmSendEvents(InputInfoPtr pInfo, const WacomDeviceState* ds)
{
#ifdef DEBUG
	int is_button = !!(ds->buttons);
#endif
	int type = ds->device_type;
	int id = ds->device_id;
	unsigned int serial = ds->serial_num;
	int x = ds->x;
	int y = ds->y;
	int z = ds->pressure;
	int tx = ds->tiltx;
	int ty = ds->tilty;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int v3, v4, v5, v6;
	int valuators[priv->naxes];

	if (priv->serial && serial != priv->serial)
	{
		DBG(10, priv, "serial number"
				" is %u but your system configured %u",
				serial, (int)priv->serial);
		return;
	}

	if (priv->cur_serial != serial)
		wcmUpdateSerial(pInfo, serial);

	/* don't move the cursor when going out-prox */
	if (!ds->proximity)
	{
		x = priv->oldX;
		y = priv->oldY;
	}

	/* use tx and ty to report stripx and stripy */
	if (type == PAD_ID)
	{
		tx = ds->stripx;
		ty = ds->stripy;
	}

	DBG(7, priv, "[%s] o_prox=%s x=%d y=%d z=%d "
		"b=%s b=%d tx=%d ty=%d wl=%d wl2=%d rot=%d th=%d\n",
		pInfo->type_name,
		priv->oldProximity ? "true" : "false",
		x, y, z, is_button ? "true" : "false", ds->buttons,
		tx, ty, ds->abswheel, ds->abswheel2, ds->rotation, ds->throttle);

	if (ds->proximity)
		wcmRotateAndScaleCoordinates(pInfo, &x, &y);

	if (IsCursor(priv))
	{
		v3 = ds->rotation;
		v4 = ds->throttle;
	}
	else  /* Intuos styli have tilt */
	{
		v3 = tx;
		v4 = ty;
	}

	v5 = ds->abswheel;
	v6 = ds->abswheel2;
	if (IsStylus(priv) && !IsArtPen(ds))
	{
		/* Normalize abswheel airbrush data to Art Pen rotation range.
		 * We do not normalize Art Pen. They are already at the range.
		 */
		v5 = ds->abswheel * MAX_ROTATION_RANGE/
				(double)MAX_ABS_WHEEL + MIN_ROTATION;
	}

	DBG(6, priv, "%s prox=%d\tx=%d"
		"\ty=%d\tz=%d\tv3=%d\tv4=%d\tv5=%d\tv6=%d\tid=%d"
		"\tserial=%u\tbutton=%s\tbuttons=%d\n",
		is_absolute(pInfo) ? "abs" : "rel",
		ds->proximity,
		x, y, z, v3, v4, v5, v6, id, serial,
		is_button ? "true" : "false", ds->buttons);

	priv->currentX = x;
	priv->currentY = y;

	/* update the old records */
	if(!priv->oldProximity)
	{
		wcmUpdateOldState(pInfo, ds);
		priv->oldButtons = 0;
	}

	valuators[0] = x;
	valuators[1] = y;
	valuators[2] = z;
	valuators[3] = v3;
	valuators[4] = v4;
	valuators[5] = v5;
	valuators[6] = v6;

	if (type == PAD_ID)
		wcmSendPadEvents(pInfo, ds, 3, priv->naxes - 3, &valuators[3]); /* pad doesn't post x/y/z */
	else
		wcmSendNonPadEvents(pInfo, ds, 0, priv->naxes, valuators);

	priv->oldProximity = ds->proximity;
	if (ds->proximity)
		wcmUpdateOldState(pInfo, ds);
	else
	{
		priv->oldButtons = 0;
		priv->oldWheel = MAX_PAD_RING + 1;
		priv->oldWheel2 = MAX_PAD_RING + 1;
		priv->oldX = 0;
		priv->oldY = 0;
		priv->oldZ = 0;
		priv->oldTiltX = 0;
		priv->oldTiltY = 0;
		priv->oldStripX = 0;
		priv->oldStripY = 0;
		priv->oldRot = 0;
		priv->oldThrottle = 0;
		priv->devReverseCount = 0;
		priv->old_serial = serial;
		priv->old_device_id = id;
		wcmUpdateSerial(pInfo, 0);
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
		" return value = %d\n", suppress, returnV);
	return returnV;
}

/*****************************************************************************
 * wcmEvent -
 *   Handles suppression, transformation, filtering, and event dispatch.
 ****************************************************************************/

void wcmEvent(WacomCommonPtr common, unsigned int channel,
	const WacomDeviceState* pState)
{
	WacomDeviceState* pLast;
	WacomDeviceState ds;
	WacomChannelPtr pChannel;
	enum WacomSuppressMode suppress;
	WacomDevicePtr priv = common->wcmDevices;
	pChannel = common->wcmChannel + channel;
	pLast = &pChannel->valid.state;

	DBG(10, common, "channel = %d\n", channel);

	/* sanity check the channel */
	if (channel >= MAX_CHANNELS)
		return;
	
	/* we must copy the state because certain types of filtering
	 * will need to change the values (ie. for error correction) */
	ds = *pState;

	DBG(10, common,
		"c=%d i=%d t=%d s=%u x=%d y=%d b=%d "
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

	/* touch device is needed for gesture later */
	if ((ds.device_type == TOUCH_ID) && !IsTouch(priv) &&
			TabletHasFeature(common, WCM_2FGT))
	{

		for (; priv != NULL && !IsTouch(priv); priv = priv->next);

		if (priv == NULL || !IsTouch(priv))
		{
			priv = common->wcmDevices;
			xf86Msg(X_ERROR, "could not find touch device "
				"for device on %s.\n", common->device_path);
		}
	}

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

	/* Optionally filter values only while in proximity */
	if (ds.proximity && ds.device_type != PAD_ID)
	{
		/* Start filter fresh when entering proximity */
		if (!pLast->proximity)
			wcmResetSampleCounter(pChannel);

		wcmFilterCoord(common,pChannel,&ds);
	}

	/* skip event if we don't have enough movement */
	suppress = wcmCheckSuppress(common, pLast, &ds);
	if (suppress == SUPPRESS_ALL)
		return;

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

	if ((ds.device_type == TOUCH_ID) && common->wcmTouch)
		wcmGestureFilter(priv, channel);

	/* don't move the cursor if in gesture mode */
	if (common->wcmGestureMode)
		return;

	/* For touch, only first finger moves the cursor */
	if ((ds.device_type == TOUCH_ID && common->wcmTouch && !channel) ||
	    (ds.device_type != TOUCH_ID))
		commonDispatchDevice(common,channel,pChannel, suppress);
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
	if (!priv->oldProximity)
		min_pressure = ds->pressure;
	else
		min_pressure = min(priv->minPressure, ds->pressure);

	return min_pressure;
}

/**
 * Instead of reporting the raw pressure, we normalize
 * the pressure from 0 to FILTER_PRESSURE_RES. This is
 * mainly to deal with the case where heavily used
 * stylus may have a "pre-loaded" initial pressure. To
 * do so, we keep the in-prox pressure and subtract it
 * from the raw pressure to prevent a potential
 * left-click before the pen touches the tablet.
 *
 * @param priv The wacom device
 * @param ds Current device state
 *
 * @rebaes
 * @see rebasePressure
 */
static int
normalizePressure(const WacomDevicePtr priv, const WacomDeviceState *ds)
{
	WacomCommonPtr common = priv->common;
	double pressure;
	int p = ds->pressure;

	if (p < priv->minPressure)
	{
		xf86Msg(X_ERROR, "%s: Pressure %d lower than expected minimum %d. This is a bug.\n",
			priv->pInfo->name, ds->pressure, priv->minPressure);
		p = priv->minPressure;
	}

	/* normalize pressure to 0..FILTER_PRESSURE_RES */
	pressure = xf86ScaleAxis(p - priv->minPressure,
				 FILTER_PRESSURE_RES, 0,
				 common->wcmMaxZ - priv->minPressure,
				 0);

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
static int
setPressureButton(const WacomDevicePtr priv, const WacomDeviceState *ds)
{
	WacomCommonPtr common = priv->common;
	int button = 1;
	int buttons = ds->buttons;

	/* button 1 Threshold test */
	/* set button1 (left click) on/off */
	if (ds->pressure < common->wcmThreshold)
	{
		buttons &= ~button;
		if (priv->oldButtons & button) /* left click was on */
		{
			/* don't set it off if it is within the tolerance
			   and threshold is larger than the tolerance */
			if ((common->wcmThreshold > THRESHOLD_TOLERANCE) &&
			    (ds->pressure > common->wcmThreshold - THRESHOLD_TOLERANCE))
				buttons |= button;
		}
	}
	else
		buttons |= button;

	return buttons;
}

static void commonDispatchDevice(WacomCommonPtr common, unsigned int channel,
				 const WacomChannelPtr pChannel,
				 enum WacomSuppressMode suppress)
{
	InputInfoPtr pInfo = NULL;
	WacomToolPtr tool = NULL;
	WacomDeviceState* ds = &pChannel->valid.states[0];
	WacomDevicePtr priv = NULL;
	WacomDeviceState filtered;

	/* device_type should have been retrieved and set in the respective
	 * models, wcmISDV4.c or wcmUSB.c. Once it comes here, something
	 * must have been wrong. Ignore the events.
	 */
	if (!ds->device_type)
	{
		DBG(11, common, "no device type matches with"
				" serial=%u\n", ds->serial_num);
		return;
	}

	DBG(10, common, "device type = %d\n", ds->device_type);

	/* Find the device the current events are meant for */
	tool = findTool(common, ds);
	/* if a device matched criteria, handle filtering per device
	 * settings, and send event to XInput */
	if (!tool || !tool->device)
	{
		DBG(11, common, "no device matches with"
				" id=%d, serial=%u\n",
				ds->device_type, ds->serial_num);
		return;
	}

	/* Tool on the tablet when driver starts. This sometime causes
	 * access errors to the device */
	if (!tool->enabled) {
		xf86Msg(X_ERROR, "tool not initialized yet. Skipping event. \n");
		return;
	}

	pInfo = tool->device;
	DBG(11, common, "tool id=%d for %s\n", ds->device_type, pInfo->name);

	filtered = pChannel->valid.state;

	/* Device transformations come first */
	priv = pInfo->private;

	if (priv->serial && filtered.serial_num != priv->serial)
	{
		DBG(10, priv, "serial number"
			" is %u but your system configured %u",
			filtered.serial_num, priv->serial);
		return;
	}

	if (TabletHasFeature(common, WCM_PENTOUCH))
	{
		if (IsPen(priv))
		{
			/* send touch out when pen coming in-prox for devices
			 * that provideboth pen and touch events so system
			 * cursor won't jump between tools.
			 */
			if (common->wcmTouchDevice->oldProximity)
			{
				common->wcmGestureMode = 0;
				wcmSoftOutEvent(common->wcmTouchDevice->pInfo);
				return;
			}
		}
		else if (IsTouch(priv) && common->wcmPenInProx)
			/* Ignore touch events when pen is in prox */
			return;
	}

	if (IsPen(priv))
		common->wcmPenInProx = filtered.proximity;

	if ((IsPen(priv) || IsTouch(priv)) && common->wcmMaxZ)
	{
		priv->minPressure = rebasePressure(priv, &filtered);
		filtered.pressure = normalizePressure(priv, &filtered);
		if (IsPen(priv))
			filtered.buttons = setPressureButton(priv, &filtered);
		filtered.pressure = applyPressureCurve(priv,&filtered);
	}
	else if (IsCursor(priv) && !priv->oldCursorHwProx)
	{
		/* initial current max distance for Intuos series */
		if ((TabletHasFeature(common, WCM_ROTATION)) ||
				(TabletHasFeature(common, WCM_DUALINPUT)))
			common->wcmMaxCursorDist = common->wcmMaxDist;
		else
			common->wcmMaxCursorDist = 0;
	}

	/* Store cursor hardware prox for next use */
	if (IsCursor(priv))
		priv->oldCursorHwProx = ds->proximity;

	/* User-requested filtering comes next */

	/* User-requested transformations come last */

	if (!is_absolute(pInfo) && !IsPad(priv))
	{
		/* To improve the accuracy of relative x/y,
		 * don't send motion event when there is no movement.
		 */
		double deltx = filtered.x - priv->oldX;
		double delty = filtered.y - priv->oldY;
		deltx *= priv->factorX;
		delty *= priv->factorY;

		/* less than one device coordinate movement? */
		if (abs(deltx)<1 && abs(delty)<1)
		{
			/* We have no other data in this event, skip */
			if (suppress == SUPPRESS_NON_MOTION)
			{
				DBG(10, common, "Ignore non-movement relative data \n");
				return;
			}

			/* send other events, such as button/wheel */
			filtered.x = priv->oldX;
			filtered.y = priv->oldY;
		}
	}

	/* force out-prox when distance is outside wcmCursorProxoutDist for pucks */
	if (IsCursor(priv))
	{
		if (common->wcmProtocolLevel == WCM_PROTOCOL_5)
		{
			/* protocol 5 distance starts from the MaxDist
			 * when getting in the prox.
			 */
			if (common->wcmMaxCursorDist > filtered.distance)
				common->wcmMaxCursorDist = filtered.distance;
		}
		else
		{
			/* protocol 4 distance is 0 when getting in the prox */
			if (common->wcmMaxCursorDist < filtered.distance)
				common->wcmMaxCursorDist = filtered.distance;
		}
		DBG(10, common, "Distance over"
				" the tablet: %d, ProxoutDist: %d current"
				" min/max %d hard prox: %d\n",
				filtered.distance,
				common->wcmCursorProxoutDist,
				common->wcmMaxCursorDist,
				ds->proximity);

		if (priv->oldProximity)
		{
			if (abs(filtered.distance - common->wcmMaxCursorDist)
					> common->wcmCursorProxoutDist)
				filtered.proximity = 0;
		}
		/* once it is out. Don't let it in until a hard in */
		/* or it gets inside wcmCursorProxoutDist */
		else
		{
			if (abs(filtered.distance - common->wcmMaxCursorDist) >
					common->wcmCursorProxoutDist && ds->proximity)
				return;
			if (!ds->proximity)
				return;
		}
	}
	wcmSendEvents(pInfo, &filtered);
}

/*****************************************************************************
 * wcmInitTablet -- common initialization for all tablets
 ****************************************************************************/

int wcmInitTablet(InputInfoPtr pInfo, const char* id, float version)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	WacomModelPtr model = common->wcmModel;

	/* Initialize the tablet */
	model->Initialize(common,id,version);

	/* Get tablet resolution */
	if (model->GetResolution)
		model->GetResolution(pInfo);

	/* Get tablet range */
	if (model->GetRanges && (model->GetRanges(pInfo) != Success))
		return !Success;
	
	/* Default threshold value if not set */
	if (common->wcmThreshold <= 0)
	{
		/* Threshold for counting pressure as a button */
		common->wcmThreshold = DEFAULT_THRESHOLD;

		xf86Msg(X_PROBED, "%s: using pressure threshold of %d for button 1\n",
			pInfo->name, common->wcmThreshold);
	}

	/* output tablet state as probed */
	if (TabletHasFeature(common, WCM_PEN))
		xf86Msg(X_PROBED, "%s: Wacom %s tablet maxX=%d maxY=%d maxZ=%d "
			"resX=%d resY=%d  tilt=%s\n",
			pInfo->name,
			model->name,
			common->wcmMaxX, common->wcmMaxY, common->wcmMaxZ,
			common->wcmResolX, common->wcmResolY,
			HANDLE_TILT(common) ? "enabled" : "disabled");
	else
		xf86Msg(X_PROBED, "%s: Wacom %s tablet maxX=%d maxY=%d maxZ=%d "
			"resX=%d resY=%d \n",
			pInfo->name,
			model->name,
			common->wcmMaxTouchX, common->wcmMaxTouchY,
			common->wcmMaxZ,
			common->wcmTouchResolX, common->wcmTouchResolY);

	return Success;
}

/* Send a soft prox-out event for the device */
void wcmSoftOutEvent(InputInfoPtr pInfo)
{
	WacomDeviceState out = { 0 };
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;

	out.device_type = DEVICE_ID(priv->flags);
	out.device_id = wcmGetPhyDeviceID(priv);
	DBG(2, priv->common, "send a soft prox-out\n");
	wcmSendEvents(pInfo, &out);
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

	p = min(FILTER_PRESSURE_RES, p);

	/* apply pressure curve function */
	return pDev->pPressCurve[p];
}

/*****************************************************************************
 * wcmRotateTablet
 ****************************************************************************/

void wcmRotateTablet(InputInfoPtr pInfo, int value)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
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
	if (common)
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
	common->wcmMaxtiltX = 128;	   /* Max tilt in X directory */
	common->wcmMaxtiltY = 128;	   /* Max tilt in Y directory */
	common->wcmCursorProxoutDistDefault = PROXOUT_INTUOS_DISTANCE;
			/* default to Intuos */
	common->wcmSuppress = DEFAULT_SUPPRESS;
			/* transmit position if increment is superior */
	common->wcmRawSample = DEFAULT_SAMPLES;
			/* number of raw data to be used to for filtering */

	return common;
}


void wcmFreeCommon(WacomCommonPtr *ptr)
{
	WacomCommonPtr common = *ptr;

	DBG(10, common, "common refcount dec to %d\n", common->refcnt - 1);
	if (--common->refcnt == 0)
	{
		free(common->private);
		while (common->serials)
		{
			WacomToolPtr next;

			DBG(10, common, "Free common serial: %d %s\n",
					common->serials->serial,
					common->serials->name);

			next = common->serials->next;
			free(common->serials);
			common->serials = next;
		}
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

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
