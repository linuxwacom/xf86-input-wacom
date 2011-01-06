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
static int *VCOPY(const int *valuators, int nvals)
{
	static int v[MAX_VALUATORS];
	memcpy(v, valuators, nvals * sizeof(int));
	return v;
}
#else /* ABI >= 11 */
#define VCOPY(vals, nval) (vals)
#endif



/*****************************************************************************
 * Static functions
 ****************************************************************************/

static void transPressureCurve(WacomDevicePtr pDev, WacomDeviceStatePtr pState);
static void commonDispatchDevice(WacomCommonPtr common, unsigned int channel, 
	const WacomChannelPtr pChannel, int suppress);
static void sendAButton(InputInfoPtr pInfo, int button, int mask,
			int first_val, int num_vals, int *valuators);

/*****************************************************************************
 * Utility functions
 ****************************************************************************/

Bool is_absolute(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	return !!(priv->flags & ABSOLUTE_FLAG);
}

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

	DBG(10, priv, "\n"); /* just prints function name */

	wcmVirtualTabletSize(pInfo);
	
	if (!is_absolute(pInfo) || !priv->wcmMMonitor)
	{
		/* Get the current screen that the cursor is in */
		if (miPointerGetScreen(pInfo->dev))
			priv->currentScreen = miPointerGetScreen(pInfo->dev)->myNum;
	}
	else
	{
		if (priv->screen_no != -1)
			priv->currentScreen = priv->screen_no;
		else if (priv->currentScreen == -1)
		{
			/* Get the current screen that the cursor is in */
			if (miPointerGetScreen(pInfo->dev))
				priv->currentScreen = miPointerGetScreen(pInfo->dev)->myNum;
		}
	}
	if (priv->currentScreen == -1) /* tool on the tablet */
		priv->currentScreen = 0;

	DBG(10, priv,
		"Active tablet area x=%d y=%d (virtual tablet area x=%d y=%d) map"
		" to maxWidth =%d maxHeight =%d\n",
		priv->bottomX, priv->bottomY, priv->sizeX, priv->sizeY, 
		priv->maxWidth, priv->maxHeight);

	priv->factorX = (double)priv->maxWidth / (double)priv->sizeX;
	priv->factorY = (double)priv->maxHeight / (double)priv->sizeY;
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
	int button, mask;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomCommonPtr common = priv->common;
	DBG(6, priv, "buttons=%d\n", buttons);

	/* Tablet PC buttons only apply to penabled devices */
	if (common->wcmTPCButton && IsStylus(priv))
	{
		if ( buttons & 1 )
		{
			if ( !(priv->flags & TPCBUTTONS_FLAG) )
			{
				priv->flags |= TPCBUTTONS_FLAG;

				if (buttons == 1) {
					/* Button 1 pressed */
					sendAButton(pInfo, 0, 1, first_val, num_vals, valuators);
				} else {
					/* send all pressed buttons down */
					for (button=2; button<=WCM_MAX_BUTTONS; button++)
					{
						mask = 1 << (button-1);
						if ( buttons & mask )
						{
							/* set to the configured button */
							sendAButton(pInfo, button-1, 1,
								    first_val, num_vals,
								    valuators);
						}
					}
				}
			}
			else
			{
				for (button=2; button<=WCM_MAX_BUTTONS; button++)
				{
					mask = 1 << (button-1);
					if ((mask & priv->oldButtons) != (mask & buttons))
					{
						/* set to the configured buttons */
						sendAButton(pInfo, button-1, mask & buttons,
							    first_val, num_vals, valuators);
					}
				}
			}
		}
		else if ( priv->flags & TPCBUTTONS_FLAG )
		{
			priv->flags &= ~TPCBUTTONS_FLAG;

			/* send all pressed buttons up */
			for (button=1; button<=WCM_MAX_BUTTONS; button++)
			{
				mask = 1 << (button-1);
				if ((mask & priv->oldButtons) != (mask & buttons) || (mask & buttons) )
				{
					/* set to the configured button */
					sendAButton(pInfo, button-1, 0,
						    first_val, num_vals,
						    valuators);
				}
			}
		}
	}
	else  /* normal buttons */
	{
		for (button=1; button<=WCM_MAX_BUTTONS; button++)
		{
			mask = 1 << (button-1);
			if ((mask & priv->oldButtons) != (mask & buttons))
			{
				/* set to the configured button */
				sendAButton(pInfo, button-1, mask & buttons,
					    first_val, num_vals, valuators);
			}
		}
	}
}

void wcmEmitKeycode (DeviceIntPtr keydev, int keycode, int state)
{
	xf86PostKeyboardEvent (keydev, keycode, state);
}

static void toggleDisplay(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomCommonPtr common = priv->common;

	if (priv->numScreen > 1)
	{
		if (IsPad(priv)) /* toggle display for all tools except pad */
		{
			WacomDevicePtr tmppriv;
			for (tmppriv = common->wcmDevices; tmppriv; tmppriv = tmppriv->next)
			{
				if (!IsPad(tmppriv))
				{
					int screen = tmppriv->screen_no;
					if (++screen >= tmppriv->numScreen)
						screen = -1;
					wcmChangeScreen(tmppriv->pInfo, screen);
				}
			}
		}
		else /* toggle display only for the selected tool */
		{
			int screen = priv->screen_no;
			if (++screen >= priv->numScreen)
				screen = -1;
			wcmChangeScreen(pInfo, screen);
		}
	}
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
			case AC_DISPLAYTOGGLE:
				toggleDisplay(pInfo);
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
						xf86PostButtonEvent(pInfo->dev,
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
	if (!priv->button[button])  /* ignore this button event */
		return;

	DBG(4, priv, "TPCButton(%s) button=%d state=%d "
		"code=%08x, coreEvent=%s \n",
		common->wcmTPCButton ? "on" : "off",
		button, mask, priv->button[button],
		(priv->button[button] & AC_CORE) ? "yes" : "no");

	if (!priv->keys[button][0])
	{
		/* No button action configured, send button */
		xf86PostButtonEventP(pInfo->dev, is_absolute(pInfo), priv->button[button], (mask != 0),
				     first_val, num_val, VCOPY(valuators, num_val));
		return;
	}

	sendAction(pInfo, (mask != 0), priv->keys[button],
		   ARRAY_SIZE(priv->keys[button]),
		   first_val, num_val, valuators);
}

/*****************************************************************************
 * getWheelButton --
 *   Get the wheel button to be sent for the current device state.
 ****************************************************************************/

static int getWheelButton(InputInfoPtr pInfo, const WacomDeviceState* ds,
			  unsigned int **fakeKey)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int fakeButton = 0, value;

	/* emulate events for relative wheel */
	if ( ds->relwheel )
	{
		value = ds->relwheel;
		fakeButton = (value > 0) ? priv->relup : priv->reldn;
		*fakeKey = (value > 0) ? priv->wheel_keys[0] : priv->wheel_keys[1];
	}

	/* emulate events for absolute wheel when it is a touch ring (on pad) */
	if ( (ds->abswheel != priv->oldWheel) && IsPad(priv) &&
	    (priv->oldProximity == ds->proximity))
	{
		int wrap_delta;
		value = priv->oldWheel - ds->abswheel;

		/* Wraparound detection. If the distance oldvalue..value is
		 * larger than the oldvalue..value considering the
		 * wraparound, assume wraparound and readjust */
		if (value < 0)
			wrap_delta = ((MAX_PAD_RING + 1) + priv->oldWheel) - ds->abswheel;
		else
			wrap_delta = priv->oldWheel - ((MAX_PAD_RING + 1) + ds->abswheel);

		DBG(12, priv, "wrap detection for %d (old %d): %d (wrap %d)\n",
		    ds->abswheel, priv->oldWheel, value, wrap_delta);

		if (abs(wrap_delta) < abs(value))
			value = wrap_delta;

		fakeButton = (value > 0) ? priv->wheelup : priv->wheeldn;
		*fakeKey = (value > 0) ? priv->wheel_keys[2] : priv->wheel_keys[3];
	}

	/* emulate events for left strip */
	if ( ds->stripx != priv->oldStripX )
	{
		int temp = 0, n, i;
		for (i=1; i<14; i++)
		{
			n = 1 << (i-1);
			if ( ds->stripx & n )
				temp = i;
			if ( priv->oldStripX & n )
				value = i;
			if ( temp & value) break;
		}

		value -= temp;

		fakeButton = (value > 0) ? priv->striplup : priv->stripldn;
		*fakeKey = (value > 0) ? priv->strip_keys[0] : priv->strip_keys[1];
	}

	/* emulate events for right strip */
	if ( ds->stripy != priv->oldStripY )
	{
		int temp = 0, n, i;
		for (i=1; i<14; i++)
		{
			n = 1 << (i-1);
			if ( ds->stripy & n )
				temp = i;
			if ( priv->oldStripY & n )
				value = i;
			if ( temp & value) break;
		}

		value -= temp;
		fakeButton = (value > 0) ? priv->striprup : priv->striprdn;
		*fakeKey = (value > 0) ? priv->strip_keys[2] : priv->strip_keys[2];
	}

	DBG(10, priv, "send fakeButton %x with value = %d \n",
		fakeButton, value);

	return fakeButton;
}
/*****************************************************************************
 * sendWheelStripEvents --
 *   Send events defined for relative/absolute wheels or strips
 ****************************************************************************/

static void sendWheelStripEvents(InputInfoPtr pInfo, const WacomDeviceState* ds,
				 int first_val, int num_vals, int *valuators)
{
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int fakeButton = 0;
	unsigned int *fakeKey = NULL;

	DBG(10, priv, "\n");

	fakeButton = getWheelButton(pInfo, ds, &fakeKey);

	if (!fakeButton && (!fakeKey || !(*fakeKey)))
		return;

	if (!fakeKey || !(*fakeKey))
	{
		/* send both button on/off in the same event for pad */
		xf86PostButtonEventP(pInfo->dev, is_absolute(pInfo), fakeButton & AC_CODE,
				     1, first_val, num_vals, VCOPY(valuators, num_vals));

		xf86PostButtonEventP(pInfo->dev, is_absolute(pInfo), fakeButton & AC_CODE,
				     0, first_val, num_vals, VCOPY(valuators, num_vals));
		return;
	}

	sendAction(pInfo, 1, fakeKey, ARRAY_SIZE(priv->wheel_keys[0]),
		   first_val, num_vals, valuators);
	sendAction(pInfo, 0, fakeKey, ARRAY_SIZE(priv->wheel_keys[0]),
		   first_val, num_vals, valuators);
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
	if ( ds->relwheel || (ds->abswheel != priv->oldWheel) ||
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

	if (common->wcmRotate == ROTATE_CCW)
		*y = axis_y->max_value - (*y - axis_y->min_value);
	else if (common->wcmRotate == ROTATE_CW)
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
	priv->oldCapacity = ds->capacity;
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
	    (ds->abswheel != priv->oldWheel))
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
			/* For relative events, reset the axes as
			 * we've already moved the device by the
			 * relative amount. Otherwise, a button
			 * event in sendCommonEvents will move the
			 * axes again.
			 */
			if (!is_absolute(pInfo))
				memset(valuators, 0, num_vals);
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

#define IsArtPen(ds)    (ds->device_id == 0x885 || ds->device_id == 0x804)

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
	int serial = (int)ds->serial_num;
	int x = ds->x;
	int y = ds->y;
	int z = ds->pressure;
	int tx = ds->tiltx;
	int ty = ds->tilty;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int v3, v4, v5;
	int valuators[priv->naxes];

	if (priv->serial && serial != priv->serial)
	{
		DBG(10, priv, "serial number"
			" is %u but your system configured %u", 
			serial, (int)priv->serial);
		return;
	}

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
		"b=%s b=%d tx=%d ty=%d wl=%d rot=%d th=%d\n",
		pInfo->type_name,
		priv->oldProximity ? "true" : "false",
		x, y, z, is_button ? "true" : "false", ds->buttons,
		tx, ty, ds->abswheel, ds->rotation, ds->throttle);

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
	if (IsStylus(priv) && !IsArtPen(ds))
	{
		/* Normalize abswheel airbrush data to Art Pen rotation range.
		 * We do not normalize Art Pen. They are already at the range.
		 */
		v5 = ds->abswheel * MAX_ROTATION_RANGE/
				(double)MAX_ABS_WHEEL + MIN_ROTATION;
	}

	DBG(6, priv, "%s prox=%d\tx=%d"
		"\ty=%d\tz=%d\tv3=%d\tv4=%d\tv5=%d\tid=%d"
		"\tserial=%u\tbutton=%s\tbuttons=%d\n",
		is_absolute(pInfo) ? "abs" : "rel",
		ds->proximity,
		x, y, z, v3, v4, v5, id, serial,
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

	if (type == PAD_ID)
		wcmSendPadEvents(pInfo, ds, 3, 3, &valuators[3]); /* pad doesn't post x/y/z */
	else
		wcmSendNonPadEvents(pInfo, ds, 0, priv->naxes, valuators);

	priv->oldProximity = ds->proximity;
	priv->old_device_id = id;
	priv->old_serial = serial;
	if (ds->proximity)
		wcmUpdateOldState(pInfo, ds);
	else
	{
		priv->oldButtons = 0;
		priv->oldWheel = MAX_PAD_RING + 1;
		priv->oldX = 0;
		priv->oldY = 0;
		priv->oldZ = 0;
		priv->oldCapacity = ds->capacity;
		priv->oldTiltX = 0;
		priv->oldTiltY = 0;
		priv->oldStripX = 0;
		priv->oldStripY = 0;
		priv->oldRot = 0;
		priv->oldThrottle = 0;
		priv->devReverseCount = 0;
	}
}

/*****************************************************************************
 * wcmCheckSuppress --
 *  Determine whether device state has changed enough - return 0
 *  if not.
 ****************************************************************************/

static int wcmCheckSuppress(WacomCommonPtr common,
			    const WacomDeviceState* dsOrig,
			    WacomDeviceState* dsNew)
{
	int suppress = common->wcmSuppress;
	/* NOTE: Suppression value of zero disables suppression. */
	int returnV = 0;

	/* Ignore all other changes that occur after initial out-of-prox. */
	if (!dsNew->proximity && !dsOrig->proximity)
		return 0;

	/* Never ignore proximity changes. */
	if (dsOrig->proximity != dsNew->proximity) returnV = 1;

	if (dsOrig->buttons != dsNew->buttons) returnV = 1;
	if (dsOrig->stripx != dsNew->stripx) returnV = 1;
	if (dsOrig->stripy != dsNew->stripy) returnV = 1;
	if (ABS(dsOrig->tiltx - dsNew->tiltx) > suppress) returnV = 1;
	if (ABS(dsOrig->tilty - dsNew->tilty) > suppress) returnV = 1;
	if (ABS(dsOrig->pressure - dsNew->pressure) > suppress) returnV = 1;
	if (ABS(dsOrig->capacity - dsNew->capacity) > suppress) returnV = 1;
	if (ABS(dsOrig->throttle - dsNew->throttle) > suppress) returnV = 1;
	if (ABS(dsOrig->rotation - dsNew->rotation) > suppress &&
		(1800 - ABS(dsOrig->rotation - dsNew->rotation)) >  suppress) returnV = 1;

	/* look for change in absolute wheel position 
	 * or any relative wheel movement
	 */
	if ((ABS(dsOrig->abswheel - dsNew->abswheel) > suppress) 
		|| (dsNew->relwheel != 0)) returnV = 1;

	/* cursor moves or not? */
	if ((ABS(dsOrig->x - dsNew->x) > suppress) || 
			(ABS(dsOrig->y - dsNew->y) > suppress)) 
	{
		if (!returnV) /* need to check if cursor moves or not */
			returnV = 2;
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
	int suppress = 0;
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
		"p=%d rz=%d tx=%d ty=%d aw=%d rw=%d "
		"t=%d df=%d px=%d st=%d cs=%d \n",
		channel,
		ds.device_id,
		ds.device_type,
		ds.serial_num,
		ds.x, ds.y, ds.buttons,
		ds.pressure, ds.rotation, ds.tiltx,
		ds.tilty, ds.abswheel, ds.relwheel, ds.throttle,
		ds.discard_first, ds.proximity, ds.sample,
		pChannel->nSamples);

	/* touch device is needed for gesture later */
	if ((ds.device_type == TOUCH_ID) && !IsTouch(priv) &&
			TabletHasFeature(common, WCM_2FGT))
	{

		for (; priv != NULL && !IsTouch(priv); priv = priv->next);

		if (priv == NULL || !IsTouch(priv))
		{
			priv = common->wcmDevices;
			/* this error will likely cause the driver crash */
			xf86Msg(X_ERROR, "%s: wcmEvent could not "
				"find touch device.\n", priv->name);
		}
	}

	/* Discard the first 2 USB packages due to events delay */
	if ( (pChannel->nSamples < 2) && (common->wcmDevCls == &gWacomUSBDevice) && 
		ds.device_type != PAD_ID && (ds.device_type != TOUCH_ID) )
	{
		DBG(11, common,
			"discarded %dth USB data.\n",
			pChannel->nSamples);
		++pChannel->nSamples;
		return; /* discard */
	}

	if (TabletHasFeature(common, WCM_ROTATION) &&
		TabletHasFeature(common, WCM_RING)) /* I4 */
	{
		/* convert Intuos4 mouse tilt to rotation */
		wcmTilt2R(&ds);
	}

	/* Optionally filter values only while in proximity */
	if (RAW_FILTERING(common) && common->wcmModel->FilterRaw &&
	    ds.proximity && ds.device_type != PAD_ID)
	{
		/* Start filter fresh when entering proximity */
		if (!pLast->proximity)
			wcmResetSampleCounter(pChannel);

		common->wcmModel->FilterRaw(common,pChannel,&ds);
	}

	/* Discard unwanted data */
	suppress = wcmCheckSuppress(common, pLast, &ds);
	if (!suppress)
	{
		return;
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

static int idtotype(int id)
{
	int type = CURSOR_ID;

	/* tools with id, such as Intuos series and Cintiq 21UX */
	switch (id)
	{
		case 0x812: /* Inking pen */
		case 0x801: /* Intuos3 Inking pen */
		case 0x012: 
		case 0x822: /* Pen */
		case 0x842:
		case 0x852:
		case 0x823: /* Intuos3 Grip Pen */
		case 0x813: /* Intuos3 Classic Pen */
		case 0x885: /* Intuos3 Marker Pen */
		case 0x022: 
		case 0x832: /* Stroke pen */
		case 0x032: 
		case 0xd12: /* Airbrush */
		case 0x912:
		case 0x112: 
		case 0x913: /* Intuos3 Airbrush */
			type = STYLUS_ID;
			break;
		case 0x82a: /* Eraser */
		case 0x85a:
		case 0x91a:
		case 0xd1a:
		case 0x0fa: 
		case 0x82b: /* Intuos3 Grip Pen Eraser */
		case 0x81b: /* Intuos3 Classic Pen Eraser */
		case 0x91b: /* Intuos3 Airbrush Eraser */
			type = ERASER_ID;
			break;
	}
	return type;
}

static void commonDispatchDevice(WacomCommonPtr common, unsigned int channel,
	const WacomChannelPtr pChannel, int suppress)
{
	InputInfoPtr pDev = NULL;
	WacomToolPtr tool = NULL;
	WacomToolPtr tooldef = NULL;
	WacomDeviceState* ds = &pChannel->valid.states[0];
	WacomDevicePtr priv = NULL;
	WacomDeviceState filtered;
	int button;

	if (!ds->device_type && ds->proximity)
	{
		/* something went wrong. Figure out device type by device id */
		switch (ds->device_id)
		{
			case STYLUS_DEVICE_ID:
				ds->device_type = STYLUS_ID;
				break;
			case ERASER_DEVICE_ID:
				ds->device_type = ERASER_ID;
				break;
			case CURSOR_DEVICE_ID:
				ds->device_type = CURSOR_ID;
				break;
			case TOUCH_DEVICE_ID:
				ds->device_type = TOUCH_ID;
				break;
			case PAD_DEVICE_ID:
				ds->device_type = PAD_ID;
				break;
			default:
				ds->device_type = idtotype(ds->device_id);
		}
		if (ds->serial_num)
			for (tool = common->wcmTool; tool; tool = tool->next)
				if (ds->serial_num == tool->serial)
				{
					ds->device_type = tool->typeid;
					break;
				}
	}

	DBG(10, common, "device type = %d\n", ds->device_type);
	/* Find the device the current events are meant for */
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
				tooldef = tool;
		}
	}

	/* pad does not need area check. Skip the unnecessary steps */
	if (tool && (tool->typeid == PAD_ID) && tool->arealist)
	{
		wcmSendEvents(tool->arealist->device, ds);
		return;
	}

	/* Use default tool (serial == 0) if no specific was found */
	if (!tool)
		tool = tooldef;

	/* 2: Find the associated area, and its InputDevice */
	if (tool)
	{
		/* if the current area is not in-prox anymore, we
		 * might want to use another area. So move the
		 * current-pointer away for a moment while we have a
		 * look if there's a better area defined.
		 * Skip this if only one area is defined 
		 */
		WacomToolAreaPtr outprox = NULL;
		if (tool->current && tool->arealist->next && 
			!wcmPointInArea(tool->current, ds->x, ds->y))
		{
			outprox = tool->current;
			tool->current = NULL;
		}

		/* If only one area is defined for the tool, always
		 * use this area even if we're not inside it
		 */
		if (!tool->current && !tool->arealist->next)
			tool->current = tool->arealist;

		/* If no current area in-prox, find a matching area */
		if(!tool->current)
		{
			WacomToolAreaPtr area = tool->arealist;
			for(; area; area = area->next)
				if (wcmPointInArea(area, ds->x, ds->y))
					break;
			tool->current = area;
		}

		/* If a better area was found, send a soft prox-out
		 * for the current in-prox area, else use the old one. */
		if (outprox)
		{
			if (tool->current)
			{
				/* Send soft prox-out for the old area */
				wcmSoftOutEvent(outprox->device);
			}
			else
				tool->current = outprox;
		}

		/* If there was one already in use or we found one */
		if(tool->current)
		{
			pDev = tool->current->device;
			DBG(11, common, "tool id=%d for %s\n",
				       ds->device_type, pDev->name);
		}
	}
	/* X: InputDevice selection done! */

	/* Tool on the tablet when driver starts. This sometime causes
	 * access errors to the device */
	if (pDev && !miPointerGetScreen(pDev->dev))
	{
		xf86Msg(X_ERROR, "wcmEvent: Wacom driver can not get Current Screen ID\n");
		xf86Msg(X_ERROR, "Please remove Wacom tool from the tablet and bring it back again.\n");
		return;
	}

	/* if a device matched criteria, handle filtering per device
	 * settings, and send event to XInput */
	if (!pDev)
	{
		DBG(11, common, "no device matches with"
				" id=%d, serial=%u\n",
				ds->device_type, ds->serial_num);
		return;
	}

	filtered = pChannel->valid.state;

	/* Device transformations come first */
	/* button 1 Threshold test */
	button = 1;
	priv = pDev->private;

	if (common->wcmDevCls == &gWacomUSBDevice && IsTouch(priv) && !ds->proximity)
	{
		priv->hardProx = 0;
	}

	if (common->wcmDevCls == &gWacomUSBDevice && (IsStylus(priv) || IsEraser(priv)))
	{
		priv->hardProx = 1;
	}

	/* send a touch out for USB Tablet PCs */
	if (common->wcmDevCls == &gWacomUSBDevice && !IsTouch(priv)
			&& common->wcmTouchDefault && !priv->oldProximity)
	{
		InputInfoPtr device = xf86FirstLocalDevice();
		WacomCommonPtr tempcommon = NULL;
		WacomDevicePtr temppriv = NULL;

		/* Lookup to see if associated touch was enabled */
		for (; device != NULL; device = device->next)
		{
			if (strstr(device->drv->driverName, "wacom"))
			{
				temppriv = (WacomDevicePtr) device->private;
				tempcommon = temppriv->common;

				if ((tempcommon->tablet_id == common->tablet_id) &&
						IsTouch(temppriv) && temppriv->oldProximity)
				{
					/* Send soft prox-out for touch first */
					wcmSoftOutEvent(device);
				}
			}
		}
	}

	if (IsStylus(priv) || IsEraser(priv))
	{
		/* Instead of reporting the raw pressure, we normalize
		 * the pressure from 0 to FILTER_PRESSURE_RES. This is
		 * mainly to deal with the case where heavily used
		 * stylus may have a "pre-loaded" initial pressure. To
		 * do so, we keep the in-prox pressure and subtract it
		 * from the raw pressure to prevent a potential
		 * left-click before the pen touches the tablet.
		 */
		double tmpP;

		/* set the minimum pressure when in prox */
		if (!priv->oldProximity)
			priv->minPressure = filtered.pressure;
		else
			priv->minPressure = min(priv->minPressure, filtered.pressure);

		/* normalize pressure to FILTER_PRESSURE_RES */
		tmpP = (filtered.pressure - priv->minPressure)
			* FILTER_PRESSURE_RES;
		tmpP /= (common->wcmMaxZ - priv->minPressure);
		filtered.pressure = (int)tmpP;

		/* set button1 (left click) on/off */
		if (filtered.pressure < common->wcmThreshold)
		{
			filtered.buttons &= ~button;
			if (priv->oldButtons & button) /* left click was on */
			{
				/* don't set it off if it is within the tolerance
				   and threshold is larger than the tolerance */
				if ((common->wcmThreshold > THRESHOLD_TOLERANCE) &&
						(filtered.pressure > common->wcmThreshold -
						 THRESHOLD_TOLERANCE))
					filtered.buttons |= button;
			}
		}
		else
			filtered.buttons |= button;

		/* transform pressure */
		transPressureCurve(priv,&filtered);
	}

	else if (IsCursor(priv) && !priv->hardProx)
	{
		/* initial current max distance for Intuos series */
		if ((TabletHasFeature(common, WCM_ROTATION)) ||
				(TabletHasFeature(common, WCM_DUALINPUT)))
			common->wcmMaxCursorDist = 256;
		else
			common->wcmMaxCursorDist = 0;
	}

	/* Store current hard prox for next use */
	if (!IsTouch(priv))
		priv->hardProx = ds->proximity;

	/* User-requested filtering comes next */

	/* User-requested transformations come last */

	if (!is_absolute(pDev) && !IsPad(priv))
	{
		/* To improve the accuracy of relative x/y,
		 * don't send motion event when there is no movement.
		 */
		double deltx = filtered.x - priv->oldX;
		double delty = filtered.y - priv->oldY;
		deltx *= priv->factorX;
		delty *= priv->factorY;

		if (ABS(deltx)<1 && ABS(delty)<1)
		{
			/* don't move the cursor */
			if (suppress == 1)
			{
				/* send other events, such as button/wheel */
				filtered.x = priv->oldX;
				filtered.y = priv->oldY;
			}
			else /* no other events to send */
			{
				DBG(10, common, "Ignore non-movement relative data \n");
				return;
			}
		}
		else
		{
			int temp = deltx;
			deltx = (double)temp/(priv->factorX);
			temp = delty;
			delty = (double)temp/(priv->factorY);
			filtered.x = deltx + priv->oldX;
			filtered.y = delty + priv->oldY;
		}
	}

	/* force out-prox when distance is outside wcmCursorProxoutDist for pucks */
	if (IsCursor(priv))
	{
		/* force out-prox when distance is outside wcmCursorProxoutDist. */
		if (common->wcmProtocolLevel == WCM_PROTOCOL_5)
		{
			if (common->wcmMaxCursorDist > filtered.distance)
				common->wcmMaxCursorDist = filtered.distance;
		}
		else
		{
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
	wcmSendEvents(pDev, &filtered);
	/* If out-prox, reset the current area pointer */
	if (!filtered.proximity)
		tool->current = NULL;
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
		xf86Msg(X_PROBED, "%s: Wacom %s tablet maxX=%d maxY=%d "
			"resX=%d resY=%d \n",
			pInfo->name,
			model->name,
			common->wcmMaxTouchX, common->wcmMaxTouchY,
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

	if (out.device_type == TOUCH_ID)
		priv->common->wcmTouchpadMode = 0;
}

/*****************************************************************************
** Transformations
*****************************************************************************/

static void transPressureCurve(WacomDevicePtr pDev, WacomDeviceStatePtr pState)
{
	/* clip the pressure */
	int p = max(0, pState->pressure);

	p = min(FILTER_PRESSURE_RES, p);

	/* apply pressure curve function */
	pState->pressure = pDev->pPressCurve[p];
}

/*****************************************************************************
 * wcmInitialScreens
 ****************************************************************************/

void wcmInitialScreens(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	int i;

	DBG(2, priv, "number of screen=%d \n", screenInfo.numScreens);

	/* initial screen info */
	priv->numScreen = screenInfo.numScreens;
	priv->screenTopX[0] = 0;
	priv->screenTopY[0] = 0;
	priv->screenBottomX[0] = 0;
	priv->screenBottomY[0] = 0;
	for (i=0; i<screenInfo.numScreens; i++)
	{
		if (screenInfo.numScreens > 1)
		{
/* dixScreenOrigins was removed from xserver without bumping the ABI.
 * 1.8.99.901 is the first release after the break. thanks. */
#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1, 8, 99, 901, 0)
			priv->screenTopX[i] = dixScreenOrigins[i].x;
			priv->screenTopY[i] = dixScreenOrigins[i].y;
			priv->screenBottomX[i] = dixScreenOrigins[i].x;
			priv->screenBottomY[i] = dixScreenOrigins[i].y;
#else
			priv->screenTopX[i] = screenInfo.screens[i]->x;
			priv->screenTopY[i] = screenInfo.screens[i]->y;
			priv->screenBottomX[i] = screenInfo.screens[i]->x;
			priv->screenBottomY[i] = screenInfo.screens[i]->y;

#endif

			DBG(10, priv, "from dix: "
				"ScreenOrigins[%d].x=%d ScreenOrigins[%d].y=%d \n",
				i, priv->screenTopX[i], i, priv->screenTopY[i]);
		}

		priv->screenBottomX[i] += screenInfo.screens[i]->width;
		priv->screenBottomY[i] += screenInfo.screens[i]->height;

		DBG(10, priv,
			"topX[%d]=%d topY[%d]=%d bottomX[%d]=%d bottomY[%d]=%d \n",
			i, priv->screenTopX[i], i, priv->screenTopY[i],
			i, priv->screenBottomX[i], i, priv->screenBottomY[i]);
	}
}

/*****************************************************************************
 * wcmRotateTablet
 ****************************************************************************/

void wcmRotateTablet(InputInfoPtr pInfo, int value)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;

	DBG(10, priv, "\n");
	common->wcmRotate = value;
}

/* wcmPointInArea - check whether the point is within the area */

Bool wcmPointInArea(WacomToolAreaPtr area, int x, int y)
{
	if (area->topX <= x && x <= area->bottomX &&
	    area->topY <= y && y <= area->bottomY)
		return 1;
	return 0;
}

/* wcmAreasOverlap - check if two areas are overlapping */

static Bool wcmAreasOverlap(WacomToolAreaPtr area1, WacomToolAreaPtr area2)
{
	if (wcmPointInArea(area1, area2->topX, area2->topY) ||
	    wcmPointInArea(area1, area2->topX, area2->bottomY) ||
	    wcmPointInArea(area1, area2->bottomX, area2->topY) ||
	    wcmPointInArea(area1, area2->bottomX, area2->bottomY))
		return 1;
	if (wcmPointInArea(area2, area1->topX, area1->topY) ||
	    wcmPointInArea(area2, area1->topX, area1->bottomY) ||
	    wcmPointInArea(area2, area1->bottomX, area1->topY) ||
	    wcmPointInArea(area2, area1->bottomX, area1->bottomY))
	        return 1;
	return 0;
}

/* wcmAreaListOverlap - check if the area overlaps any area in the list */
Bool wcmAreaListOverlap(WacomToolAreaPtr area, WacomToolAreaPtr list)
{
	for (; list; list=list->next)
		if (area != list && wcmAreasOverlap(list, area))
			return 1;
	return 0;
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

	return common;
}


void wcmFreeCommon(WacomCommonPtr *ptr)
{
	WacomCommonPtr common = *ptr;

	DBG(10, common, "common refcount dec to %d\n", common->refcnt - 1);
	if (--common->refcnt == 0)
	{
		free(common->private);
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
