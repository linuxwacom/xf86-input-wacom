/*
 * Copyright 1995-2003 by Frederic Lepied, France. <Lepied@XFree86.org>
 *									    
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Frederic   Lepied not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Frederic  Lepied   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.		   
 *									    
 * FREDERIC  LEPIED DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "xf86Wacom.h"

	WacomDeviceClass* wcmDeviceClasses[] =
	{
		/* Current USB implementation requires LINUX_INPUT */
		#ifdef LINUX_INPUT
		&gWacomUSBDevice,
		#endif

		&gWacomISDV4Device,
		&gWacomSerialDevice,
		NULL
	};

/*****************************************************************************
 * xf86WcmSetScreen --
 *   set to the proper screen according to the converted (x,y).
 *   this only supports for horizontal setup now.
 *   need to know screen's origin (x,y) to support 
 *   combined horizontal and vertical setups
 ****************************************************************************/

static void xf86WcmSetScreen(LocalDevicePtr local, int *v0, int *v1)
{
#if XFREE86_V4
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int screenToSet = miPointerCurrentScreen()->myNum;
	int totalWidth = 0, maxHeight = 0, leftPadding = 0;
	int i, x, y;

	DBG(6, ErrorF("xf86WcmSetScreen\n"));
	if (!(priv->flags & ABSOLUTE_FLAG))
	{
		priv->currentScreen = screenToSet;
		return;
	}

	/* YHJ - these don't need to be calculated every time */
	for (i = 0; i < priv->numScreen; i++)
	{
		totalWidth += screenInfo.screens[i]->width;
		if (maxHeight < screenInfo.screens[i]->height)
			maxHeight = screenInfo.screens[i]->height;
	}
	/* YHJ - looks nasty. sorry. */	
	if (priv->screen_no == -1)
	{
		for (i = 0; i < priv->numScreen; i++)
		{
			if (*v0 * totalWidth <=
				(leftPadding + screenInfo.screens[i]->width)
				* (priv->bottomX - priv->topX))
			{
				screenToSet = i;
				break;
			}
			leftPadding += screenInfo.screens[i]->width;
		}
	}
	else if (priv->XineramaEnabled)
	{
		screenToSet = priv->screen_no;
		for (i = 0; i < screenToSet; i++)
			leftPadding += screenInfo.screens[i]->width;
		*v0 = ((priv->bottomX - priv->topX) * leftPadding + *v0
			* screenInfo.screens[screenToSet]->width) /
			(double)totalWidth + 0.5;
		*v1 = *v1 * screenInfo.screens[screenToSet]->height /
			(double)maxHeight + 0.5;

	}

	if (priv->XineramaEnabled)
	{
		priv->factorX = totalWidth/(double)(priv->bottomX - priv->topX);
		priv->factorY = maxHeight/(double)(priv->bottomY - priv->topY);
		x = (*v0 - (priv->bottomX - priv->topX)
			* leftPadding / totalWidth) * priv->factorX + 0.5;
	}
	else
	{
		if (priv->screen_no == -1)
			*v0 = (*v0 * totalWidth - (priv->bottomX - priv->topX)
				* leftPadding)
				/ screenInfo.screens[screenToSet]->width;
		else
			screenToSet = priv->screen_no;
		priv->factorX = screenInfo.screens[screenToSet]->width
			/ (double)(priv->bottomX - priv->topX);
		priv->factorY = screenInfo.screens[screenToSet]->height
			/ (double)(priv->bottomY - priv->topY);
		x = *v0 * priv->factorX + 0.5;
	}
	y = *v1 * priv->factorY + 0.5;

	xf86XInputSetScreen(local, screenToSet, x, y);
	DBG(10, ErrorF("xf86WcmSetScreen current=%d ToSet=%d\n", 
		priv->currentScreen, screenToSet));
	priv->currentScreen = screenToSet;
#endif
}

/*****************************************************************************
 * xf86WcmSendButtons --
 *   Send button events by comparing the current button mask with the
 *   previous one.
 ****************************************************************************/

static void xf86WcmSendButtons(LocalDevicePtr local, int buttons,
		int rx, int ry, int rz, int rtx, int rty, int rrot,
		int rth, int rwheel)
{
	int button;
	WacomDevicePtr priv = (WacomDevicePtr) local->private;

	for (button=1; button<=16; button++)
	{
		int mask = 1 << (button-1);
	
		if ((mask & priv->oldButtons) != (mask & buttons))
		{
			DBG(4, ErrorF("xf86WcmSendButtons button=%d "
				"state=%d, for %s\n", 
				button, (buttons & mask) != 0, local->name));
			if (IsCursor(priv))
				xf86PostButtonEvent(local->dev, 
					(priv->flags & ABSOLUTE_FLAG),
					button, (buttons & mask) != 0,
					0, 6, rx, ry, rz, rrot, rth, rwheel);
			else
				xf86PostButtonEvent(local->dev, 
					(priv->flags & ABSOLUTE_FLAG),
					button, (buttons & mask) != 0,
					0, 6, rx, ry, rz, rtx, rty, rwheel);
		}
	}
}

/*****************************************************************************
 * xf86WcmSendEvents --
 *   Send events according to the device state.
 ****************************************************************************/

void xf86WcmSendEvents(LocalDevicePtr local, const WacomDeviceState* ds)
{
	int type = ds->device_type;
	int is_button = !!(ds->buttons);
	int is_proximity = ds->proximity;
	int x = ds->x;
	int y = ds->y;
	int z = ds->pressure;
	int buttons = ds->buttons;
	int tx = ds->tiltx;
	int ty = ds->tilty;
	int rot = ds->rotation;
	int throttle = ds->throttle;
	int wheel = ds->wheel;

	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int rx, ry, rz, rtx, rty, rwheel, rrot, rthrottle;
	int is_core_pointer, is_absolute;

	DBG(7, ErrorF("[%s] prox=%s x=%d y=%d z=%d "
		"b=%s b=%d tx=%d ty=%d wl=%d rot=%d th=%d\n",
		(type == STYLUS_ID) ? "stylus" :
			(type == CURSOR_ID) ? "cursor" : "eraser",
		is_proximity ? "true" : "false",
		x, y, z, is_button ? "true" : "false", buttons,
		tx, ty, wheel, rot, throttle));

	is_absolute = (priv->flags & ABSOLUTE_FLAG);
	is_core_pointer = xf86IsCorePointer(local->dev);

	DBG(6, ErrorF("[%s] %s prox=%d\tx=%d\ty=%d\tz=%d\t"
		"button=%s\tbuttons=%d\n",
		local->name,
		is_absolute ? "abs" : "rel",
		is_proximity,
		x, y, z,
		is_button ? "true" : "false", buttons));

	/* sets rx and ry according to the mode */
	if (is_absolute)
	{
		rx = x > priv->bottomX ? priv->bottomX - priv->topX :
			x < priv->topX ? 0 : x - priv->topX;
		ry = y > priv->bottomY ? priv->bottomY - priv->topY :
			y < priv->topY ? 0 : y - priv->topY;
		rz = z;
		rtx = tx;
		rty = ty;
		rwheel = wheel;
		rrot = rot;
		rthrottle = throttle;
	}
	else
	{
		/* unify acceleration in both directions */
		rx = (x - priv->oldX) * priv->factorY / priv->factorX;
		ry = y - priv->oldY;
		if (priv->speed != DEFAULT_SPEED )
		{
			/* don't apply acceleration for fairly small
			* increments (but larger than speed setting). */

			int no_jitter = priv->speed * 3;
			if (ABS(rx) > no_jitter)
				rx *= priv->speed;
			if (ABS(ry) > no_jitter)
				ry *= priv->speed;
		}
		rz = z - priv->oldZ;
		rtx = tx - priv->oldTiltX;
		rty = ty - priv->oldTiltY;
		rwheel = wheel - priv->oldWheel;
		rrot = rot - priv->oldRot;
		rthrottle = throttle - priv->oldThrottle;
	}

	/* coordinates are ready we can send events */
	if (is_proximity)
	{

		if (!priv->oldProximity)
		{
			priv->flags |= FIRST_TOUCH_FLAG;
			DBG(4, ErrorF("xf86WcmSendEvents FIRST_TOUCH_FLAG "
				"set for %s\n", local->name));
		}
		/* don't send anything the first time we get data 
		 * since the x and y values may be invalid */
		else if (priv->flags & FIRST_TOUCH_FLAG)
		{
			priv->flags ^= FIRST_TOUCH_FLAG;
			DBG(4, ErrorF("xf86WcmSendEvents "
				"FIRST_TOUCH_FLAG unset for %s\n",
				local->name));
			if (!is_absolute)
			{
				/* don't move the cursor the
				 * first time we send motion event */
				rx = 0;
				ry = 0;
				rz = 0;
				rtx = 0;
				rty = 0;
				rwheel = 0;
				rrot = 0;
				rthrottle = 0;
			}
			/* to support multi-monitors, we need
			 * to set the proper screen before posting
			 * any events */
			xf86WcmSetScreen(local, &rx, &ry);
			if (IsCursor(priv))
				xf86PostProximityEvent(
					local->dev, 1, 0, 6,
					rx, ry, z, rrot,
					rthrottle, rwheel);
			else
				xf86PostProximityEvent(
					local->dev, 1, 0, 6,
					rx, ry, z, tx, ty,
					rwheel);
		}
		else
		{
			/* to support multi-monitors, we need to set the proper 
			* screen before posting any events */
			xf86WcmSetScreen(local, &rx, &ry);
			if(!(priv->flags & BUTTONS_ONLY_FLAG))
			{
				if (IsCursor(priv))
					xf86PostMotionEvent(local->dev,
						is_absolute, 0, 6, rx, ry, rz,
						rrot, rthrottle, rwheel);
				else
					xf86PostMotionEvent(local->dev,
						is_absolute, 0, 6, rx, ry, rz,
						rtx, rty, rwheel);
			}
			if (priv->oldButtons != buttons)
			{
				xf86WcmSendButtons (local, buttons, rx, ry, rz,
					rtx, rty, rrot, rthrottle, rwheel);
			}
		}
	}

	/* not in proximity */
	else
	{
		/* reports button up when the device has been
		 * down and becomes out of proximity */
		if (priv->oldButtons)
		{
			xf86WcmSendButtons (local, 0, rx, ry, rz,
				rtx, rty, rrot, rthrottle, rwheel);
			buttons = 0;
		}
		if (!is_core_pointer)
		{
			/* macro button management */
			if (common->wcmProtocolLevel == 4 && buttons)
			{
				int macro = z / 2;

				DBG(6, ErrorF("macro=%d buttons=%d "
					"wacom_map[%d]=%x\n",
					macro, buttons, macro,
					gWacomModule.keymap[macro]));

				/* First available Keycode begins at 8
				 * therefore macro+7 */

				/* key down */
				if (IsCursor(priv))
					xf86PostKeyEvent(local->dev,macro+7,1,
						is_absolute,0,6,
						0,0,buttons,rrot,rthrottle,
						rwheel);
				else
					xf86PostKeyEvent(local->dev,macro+7,1,
						is_absolute,0,6,
						0,0,buttons,rtx,rty,rwheel);

				/* key up */
				if (IsCursor(priv))
					xf86PostKeyEvent(local->dev,macro+7,0,
						is_absolute,0,6,
						0,0,buttons,rrot,rthrottle,
						rwheel);
				else
					xf86PostKeyEvent(local->dev,macro+7,0,
						is_absolute,0,6,
						0,0,buttons,rtx,rty,rwheel);

			}
			if (priv->oldProximity)
			{
				if (IsCursor(priv))
					xf86PostProximityEvent(local->dev,
						0, 0, 6, rx, ry, rz,
						rrot, rthrottle, rwheel);
				else
					xf86PostProximityEvent(local->dev,
						0, 0, 6, rx, ry, rz,
						rtx, rty, rwheel);
			}
		}
	} /* not in proximity */

	priv->oldProximity = is_proximity;
	priv->oldButtons = buttons;
	priv->oldWheel = wheel;
	priv->oldX = x;
	priv->oldY = y;
	priv->oldZ = z;
	priv->oldTiltX = tx;
	priv->oldTiltY = ty;
	priv->oldRot = rot;
	priv->oldThrottle = throttle;
}

/*****************************************************************************
 * xf86WcmIntuosFilter --
 *   Correct some hardware defects we've been seeing in Intuos pads,
 *   but also cuts down quite a bit on jitter.
 ****************************************************************************/

#if 0
static int xf86WcmIntuosFilter(WacomFilterState* state, int coord, int tilt)
{
	int tilt_filtered;
	int ts;
	int x0_pred;
	int x0_pred1;
	int x0, x1, x2, x3;
	int x;
    
	tilt_filtered = tilt + state->tilt[0] + state->tilt[1] + state->tilt[2];
	state->tilt[2] = state->tilt[1];
	state->tilt[1] = state->tilt[0];
	state->tilt[0] = tilt;
    
	x0 = coord;
	x1 = state->coord[0];
	x2 = state->coord[1];
	x3 = state->coord[2];
	state->coord[0] = x0;
	state->coord[1] = x1;
	state->coord[2] = x2;
    
	ts = tilt_filtered >= 0 ? 1 : -1;
    
	if (state->state == 0 || state->state == 3)
	{
		x0_pred = 2 * x1 - x2;
		x0_pred1 = 3 * x2 - 2 * x3;
		if (ts * (x0 - x0_pred) > 12 && ts * (x0 - x0_pred1) > 12)
		{
			/* detected a jump at x0 */
			state->state = 1;
			x = x1;
		}
		else if (state->state == 0)
		{
			x = (7 * x0 + 14 * x1 + 15 * x2 - 4 * x3 + 16) >> 5;
		}
		else
		{
			/* state->state == 3 
			 * a jump at x3 was detected */
			x = (x0 + 2 * x1 + x2 + 2) >> 2;
			state->state = 0;
		}
	}
	else if (state->state == 1)
	{
		/* a jump at x1 was detected */
		x = (3 * x0 + 7 * x2 - 2 * x3 + 4) >> 3;
		state->state = 2;
	}
	else
	{
		/* state->state == 2 
		 * a jump at x2 was detected */
		x = x1;
		state->state = 3;
	}

	return x;
}
#endif

/*****************************************************************************
 * ThrottleToRate - converts throttle position to wheel rate
 ****************************************************************************/

#if 0
static int ThrottleToRate(int x)
{
	if (x<0) x=-x;

	/* piece-wise exponential function */
	
	if (x < 128) return 0;		/* infinite */
	if (x < 256) return 1000;	/* 1 second */
	if (x < 512) return 500;	/* 0.5 seconds */
	if (x < 768) return 250;	/* 0.25 seconds */
	if (x < 896) return 100;	/* 0.1 seconds */
	if (x < 960) return 50;		/* 0.05 seconds */
	if (x < 1024) return 25;	/* 0.025 seconds */
	return 0;			/* infinite */
}
#endif

/*****************************************************************************
 * xf86WcmSuppress --
 *  Determine whether device state has changed enough - return 1
 *  if not.
 ****************************************************************************/

static int xf86WcmSuppress(int suppress, const WacomDeviceState* dsOrig,
	const WacomDeviceState* dsNew)
{
	if (dsOrig->buttons != dsNew->buttons) return 0;
	if (dsOrig->proximity != dsNew->proximity) return 0;
	if (ABS(dsOrig->x - dsNew->x) >= suppress) return 0;
	if (ABS(dsOrig->y - dsNew->y) >= suppress) return 0;
	if (ABS(dsOrig->pressure - dsNew->pressure) >= suppress) return 0;
	if (ABS(dsOrig->throttle - dsNew->throttle) >= suppress) return 0;
	if ((1800 + dsOrig->rotation - dsNew->rotation) % 1800 >= suppress &&
		(1800 + dsNew->rotation - dsOrig->rotation) % 1800 >= suppress)
		return 0;

	/* We don't want to miss the wheel's relative value */
	/* may need to check if it's a tool with relative wheel? */
	if ((ABS(dsOrig->wheel - dsNew->wheel) >= suppress) ||
		(ABS(dsOrig->wheel - dsNew->wheel) == 1)) return 0;

	return 1;
}

/*****************************************************************************
 * xf86WcmOpen --
 ****************************************************************************/

Bool xf86WcmOpen(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceClass** ppDevCls;

	DBG(1, ErrorF("opening %s\n", common->wcmDevice));

	local->fd = xf86WcmOpenTablet(local);
	if (local->fd < 0)
	{
		ErrorF("Error opening %s : %s\n", common->wcmDevice,
			strerror(errno));
		return !Success;
	}

	/* Detect device class; default is serial device */
	for (ppDevCls=wcmDeviceClasses; *ppDevCls!=NULL; ++ppDevCls)
	{
		if ((*ppDevCls)->Detect(local))
		{
			common->wcmDevCls = *ppDevCls;
			break;
		}
	}

	/* Initialize the tablet */
	return common->wcmDevCls->Init(local);
}

/*****************************************************************************
 * xf86WcmEvent -
 *   Handles suppression, filtering, and event dispatch.
 ****************************************************************************/

void xf86WcmEvent(WacomCommonPtr common, unsigned int channel,
	const WacomDeviceState* ds)
{
	WacomDeviceState* pOrigState;
	LocalDevicePtr pOrigDev;
	LocalDevicePtr pDev = NULL;
	WacomDevicePtr priv;
	int id, idx;

	/* sanity check the channel */
	if (channel >= MAX_CHANNELS)
		return;

	pOrigState = &common->wcmChannel[channel].state;
	pOrigDev = common->wcmChannel[channel].pDev;

	DBG(10, ErrorF("xf86WcmEvent: c=%d i=%d t=%d s=%u x=%d y=%d b=0x%X "
		"p=%d rz=%d tx=%d ty=%d w=%d t=%d df=%d px=%d\n",
		channel,
		ds->device_id,
		ds->device_type,
		ds->serial_num,
		ds->x, ds->y, ds->buttons,
		ds->pressure, ds->rotation, ds->tiltx,
		ds->tilty, ds->wheel, ds->throttle,
		ds->discard_first, ds->proximity));

	/* Check suppression */
	if (xf86WcmSuppress(common->wcmSuppress, pOrigState, ds))
	{
		DBG(10, ErrorF("Suppressing data according to filter\n"));
		return;
	}

	/* pre-filtering */

	/* Find the device the current events are meant for */
	for (idx=0; idx<common->wcmNumDevices; idx++)
	{
		priv = common->wcmDevices[idx]->private;
		id = DEVICE_ID(priv->flags);

		if (id == ds->device_type &&
			((!priv->serial) || (ds->serial_num == priv->serial)))
		{
			if ((priv->topX <= ds->x && priv->bottomX >= ds->x &&
			priv->topY <= ds->y && priv->bottomY >= ds->y))
			{
				DBG(11, ErrorF("tool id=%d for %s\n",
					id, common->wcmDevices[idx]->name));
				pDev = common->wcmDevices[idx];
				break;
			}
			/* Fallback to allow the cursor to move
			 * smoothly along screen edges */
			else if (priv->oldProximity)
			{
				pDev = common->wcmDevices[idx];
			}
		}
	}

	/* if the logical device of the same physical tool has changed,
	 * send proximity out to the previous one */
	if (pOrigDev && (pOrigDev != pDev) &&
		(pOrigState->serial_num == ds->serial_num))
	{
		pOrigState->proximity = 0;
		xf86WcmSendEvents(pOrigDev, pOrigState);
	}

	/* if a device matched criteria, handle filtering per device
	 * settings, and send event to XInput */
	if (pDev)
	{
		WacomDeviceState filtered = *ds;

		/* WacomDevicePtr priv = common->wcmDevices[idx]->private; */

		/* The if-else statement should be used after the device is 
	 	* selected since is_absolute = priv->flags | ABSOLUTE_FLAG
	 	* I removed the declaration of is_absolute at the beginning
	 	* of this routine */

		#if 0
		/* Intuos filter */
		if (priv->flags & ABSOLUTE_FLAG)
		{
			x = xf86WcmIntuosFilter (&ds->x_filter, ds->x,
				ds->tiltx);
			y = xf86WcmIntuosFilter (&ds->y_filter, ds->y,
				ds->tilty);
		} 
		else
		{
			x = ds->x;
			y = ds->y;
		}
		#endif

		#if 0
		/* get the sample time */
		sampleTime = GetTimeInMillis(); 
	
		/* throttle filter */
		if ((priv->throttleStart > sampleTime) ||
			(!priv->throttleStart))
		{
			priv->throttleStart = sampleTime;
			priv->throttleLimit = -1;
		}
	
		ticks = ThrottleToRate(ds->throttle);
		priv->throttleLimit = ticks ?  priv->throttleStart + ticks : -1;

		if ((priv->throttleLimit >= 0) &&
			(priv->throttleLimit < sampleTime))
		{
			DBG(6, ErrorF("LIMIT REACHED: s=%d l=%d n=%d v=%d "
				"N=%d\n", priv->throttleStart,
				priv->throttleLimit, sampleTime,
				priv->throttleValue,
				sampleTime +
					ThrottleToRate(priv->throttleValue)));

			ds.wheel += (priv->throttleValue > 0) ? 1 :
					(priv->throttleValue < 0) ? -1 : 0;
		}

		priv->throttleStart = sampleTime;
		priv->throttleLimit = sampleTime +
			ThrottleToRate(priv->throttleValue);
		#endif /* throttle */

		xf86WcmSendEvents(pDev,&filtered);
	}

	/* otherwise, if no device matched... */
	else
	{
		DBG(11, ErrorF("no device matches with id=%d, serial=%d\n",
				ds->device_type, ds->serial_num));
	}

	/* save channel device state and device to which last event went */
	common->wcmChannel[channel].state = *ds;
	common->wcmChannel[channel].pDev = pDev;
}
