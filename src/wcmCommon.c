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
		&wcmUSBDevice,
		#endif

		&wcmISDV4Device,
		&wcmSerialDevice,
		NULL
	};

/*****************************************************************************
 * xf86WcmSetScreen --
 *   set to the proper screen according to the converted (x,y).
 *   this only supports for horizontal setup now.
 *   need to know screen's origin (x,y) to support 
 *   combined horizontal and vertical setups
 ****************************************************************************/

static void xf86WcmSetScreen(LocalDevicePtr local, int v0, int v1)
{
#if XFREE86_V4
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int screenToSet = miPointerCurrentScreen()->myNum;
	int i, x, y;

	DBG(6, ErrorF("xf86WcmSetScreen\n"));

	if (priv->screen_no != -1)
	{
		screenToSet = priv->screen_no;
		priv->factorX = (double)screenInfo.screens[screenToSet]->width /
			(double) (priv->bottomX - priv->topX);
		priv->factorY = (double)screenInfo.screens[screenToSet]->height/
			(double) (priv->bottomY - priv->topY);
	}
	else if (priv->flags & ABSOLUTE_FLAG)
	{
		for (i=0; i< priv->numScreen; i++ )
		{
			if ( v0 <= (priv->bottomX - priv->topX) /
					priv->numScreen * (i+1) )
			{
				v0 = v0 - (int)(((priv->bottomX -
					priv->topX) * i) / priv->numScreen);
				screenToSet = i;
				i = priv->numScreen;
			}
		}
		priv->factorX=(double)(screenInfo.screens[screenToSet]->width *
			priv->numScreen)/(double)(priv->bottomX - priv->topX);
		priv->factorY=(double)screenInfo.screens[screenToSet]->height /
			(double)(priv->bottomY - priv->topY);
	}

	x = v0 * priv->factorX + 0.5;
	y = v1 * priv->factorY + 0.5;
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
		int rx, int ry, int rz, int rtx, int rty, int rwheel)
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
 *   (YHJ - interface may change later since only local is really needed)
 ****************************************************************************/

void xf86WcmSendEvents(LocalDevicePtr local, int type,
		unsigned int serial, int is_stylus, int is_button,
		int is_proximity, int x, int y, int z, int buttons,
		int tx, int ty, int wheel)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int rx, ry, rz, rtx, rty, rwheel;
	int is_core_pointer, is_absolute;
	int set_screen_called = 0;

	DBG(7, ErrorF("[%s] prox=%s\tx=%d\ty=%d\tz=%d\t"
		"button=%s\tbuttons=%d\ttx=%d ty=%d\twl=%d\n",
		(type == STYLUS_ID) ? "stylus" : (type == CURSOR_ID) ?
			"cursor" : "eraser",
		is_proximity ? "true" : "false",
		x, y, z,
		is_button ? "true" : "false", buttons,
		tx, ty, wheel));

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
	}
	else
	{
		rx = x - priv->oldX;
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
		else if ((priv->oldX != x) || (priv->oldY != y) ||
			(priv->oldZ != z) || (is_stylus && HANDLE_TILT(common) &&
			(tx != priv->oldTiltX || ty != priv->oldTiltY)))
		{
			if (priv->flags & FIRST_TOUCH_FLAG)
			{
				if (priv->oldProximity)
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
					}
					/* to support multi-monitors, we need
					 * to set the proper screen before posting
					 * any events */
					xf86WcmSetScreen(local, rx, ry);
					set_screen_called = 1;
					xf86PostProximityEvent(local->dev, 1, 0, 6,
						rx, ry, z, tx, ty, rwheel);
				}
			}
		}

		/* don't send anything the first time we get data 
		 * since the x and y values may be invalid 
		 */
		if ( !(priv->flags & FIRST_TOUCH_FLAG))
		{
			/* to support multi-monitors, we need to set the proper 
			* screen before posting any events */
			if (!set_screen_called)
			{
				xf86WcmSetScreen(local, rx, ry);
			}
			if(!(priv->flags & BUTTONS_ONLY_FLAG))
			{
				xf86PostMotionEvent(local->dev, is_absolute,
					0, 6, rx, ry, rz, rtx, rty, rwheel);
			}
			if (priv->oldButtons != buttons)
			{
				xf86WcmSendButtons (local, buttons, rx, ry, rz,
					rtx, rty, rwheel);
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
				rtx, rty, rwheel);
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
				xf86PostKeyEvent(local->dev, macro+7, 1,
					is_absolute, 0, 6,
					0, 0, buttons, rtx, rty, rwheel);
				xf86PostKeyEvent(local->dev, macro+7, 0,
					 is_absolute, 0, 6,
					 0, 0, buttons, rtx, rty, rwheel);
			}
			if (priv->oldProximity)
			{
				xf86PostProximityEvent(local->dev, 0, 0, 6,
					rx, ry, rz, rtx, rty, rwheel);
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
}

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
	
	if (x < 128) return 0;          /* infinite */
	if (x < 256) return 1000;       /* 1 second */
	if (x < 512) return 500;        /* 0.5 seconds */
	if (x < 768) return 250;        /* 0.25 seconds */
	if (x < 896) return 100;        /* 0.1 seconds */
	if (x < 960) return 50;         /* 0.05 seconds */
	if (x < 1024) return 25;        /* 0.025 seconds */
	return 0;                       /* infinite */
}
#endif

/*****************************************************************************
 * xf86WcmDirectEvents --
 *   Handles device selection and directs events to the right devices
 *   (YHJ - interface may change later since only common is really needed)
 ****************************************************************************/

void xf86WcmDirectEvents(WacomCommonPtr common, const WacomDeviceState* ds)
{
	int type = ds->device_type;
	unsigned int serial = ds->serial_num;
	int is_proximity = ds->proximity;
	int x = ds->x;
	int y = ds->y;
	int pressure = ds->pressure;
	int buttons = ds->buttons;
	int tilt_x = ds->tiltx;
	int tilt_y = ds->tilty;
	int wheel = ds->wheel;
	int is_stylus = (type == STYLUS_ID || type == ERASER_ID);
	int is_button = !!(buttons);
	int found_device = -1;
	int idx;
	int sampleTime;

	/* get the sample time */
	sampleTime = GetTimeInMillis();
	
	/* Find the device the current events are meant for */
	for (idx=0; idx<common->wcmNumDevices; idx++)
	{
		WacomDevicePtr priv = common->wcmDevices[idx]->private;
		int id;

		id = DEVICE_ID(priv->flags);
		if (id == type &&
			((!priv->serial) || (serial == priv->serial)) &&
			(priv->topX <= x && priv->bottomX >= x &&
			priv->topY <= y && priv->bottomY >= y))
		{
			DBG(11, ErrorF("tool id=%d for %s\n",
				id, common->wcmDevices[idx]->name));
			found_device = idx;
			idx = common->wcmNumDevices;
		}
	}

	/* send to appropriate device */
	for (idx=0; idx<common->wcmNumDevices; idx++)
	{
		int id;
		WacomDevicePtr priv = common->wcmDevices[idx]->private;

		if (idx == found_device)
			continue;

		id = DEVICE_ID(priv->flags);
		if (id == type &&
			((!priv->serial) || (serial == priv->serial)) &&
			priv->oldProximity)
		{
			if (found_device == -1)
			{
				/* Outside every mapped area, so fall
				 * back to this, the previous one */
				found_device = idx;
			}
			else
			{
				/* Multiple mapped areas support:
				 * Send a fake proximity out to the device
				 * sharing the same physical tool, but
				 * mapped on another tablet area */
				DBG(6, ErrorF("Setting device %s fake "
					"proximity out\n",
					common->wcmDevices[idx]->name));
				xf86WcmSendEvents(common->wcmDevices[idx],
					type, serial, is_stylus,
					0, 0, 0, 0, 0, 0, 0, 0, 0);
			}
			idx = common->wcmNumDevices;
		}
	} /* next device */

	if (found_device != -1)
	{
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

		xf86WcmSendEvents(common->wcmDevices[found_device],
			type, serial, is_stylus, is_button, is_proximity,
			x, y, pressure, buttons, tilt_x, tilt_y, wheel);
	}

	/* might be useful for detecting new devices */
	else
	{
		DBG(11, ErrorF("no device matches with id=%d, serial=%d\n",
				type, serial));
	}
}

/*****************************************************************************
 * xf86WcmSuppress --
 *  Determine whether device state has changed enough - return 1
 *  if not.
 ****************************************************************************/

int xf86WcmSuppress(int suppress, WacomDeviceState* ds1, WacomDeviceState* ds2)
{
	if (ds1->buttons != ds2->buttons) return 0;
	if (ds1->proximity != ds2->proximity) return 0;
	if (ABS(ds1->x - ds2->x) >= suppress) return 0;
	if (ABS(ds1->y - ds2->y) >= suppress) return 0;
	if (ABS(ds1->pressure - ds2->pressure) >= suppress) return 0;
	if ((1800 + ds1->rotation - ds2->rotation) % 1800 >= suppress &&
		(1800 + ds2->rotation - ds1->rotation) % 1800 >= suppress)
		return 0;

	/* We don't want to miss the wheel's relative value */
	/* may need to check if it's a tool with relative wheel? */
	if ((ABS(ds1->wheel - ds2->wheel) >= suppress) ||
		(ABS(ds1->wheel - ds2->wheel) == 1)) return 0;

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
			common->pDevCls = *ppDevCls;
			break;
		}
	}

	/* Initialize the tablet */
	return common->pDevCls->Init(local);
}

void xf86WcmEvent(WacomCommonPtr common, int tool_index,
	const WacomDeviceState* ds)
{
	if (xf86WcmSuppress(common->wcmSuppress,
		common->wcmDevStat + tool_index, /* original */
		ds))                             /* new data */
	{
		DBG(10, ErrorF("Suppressing data according to filter\n"));
		return;
	}

	xf86WcmDirectEvents(common,ds);
	common->wcmDevStat[tool_index] = *ds;
}
