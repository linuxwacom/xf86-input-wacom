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
 * Static functions
 ****************************************************************************/
 
static void transPressureCurve(WacomDevicePtr pDev, WacomDeviceStatePtr pState);
static void commonDispatchDevice(WacomCommonPtr common,
	const WacomChannelPtr pChannel);
static void resetSampleCounter(const WacomChannelPtr pChannel);
 
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
	int screenToSet = 0;
	int totalWidth = 0, maxHeight = 0, leftPadding = 0;
	int i, x, y;
	double sizeX = priv->bottomX - priv->topX;
	double sizeY = priv->bottomY - priv->topY;

	DBG(6, ErrorF("xf86WcmSetScreen\n"));

	/* set factorX and factorY for single screen setup since
	 * Top X Y and Bottom X Y can be changed while driver is running
	 */
	if (screenInfo.numScreens == 1 || !priv->common->wcmMMonitor)
	{
		/* set the current sreen in multi-monitor setup */
		screenToSet = 0;
		if (!priv->common->wcmMMonitor && priv->twinview == TV_NONE)
		{
			screenToSet = priv->currentScreen = 
				miPointerCurrentScreen()->myNum;
		}
		priv->factorX = screenInfo.screens[screenToSet]->width / sizeX;
		priv->factorY = screenInfo.screens[screenToSet]->height / sizeY;
		return;
	}

	if (!(local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER))) return;
	if (!(priv->flags & ABSOLUTE_FLAG))
	{
		/* screenToSet lags by one event, but not that important */
		screenToSet = miPointerCurrentScreen()->myNum;
		priv->factorX = screenInfo.screens[screenToSet]->width / sizeX;
		priv->factorY = screenInfo.screens[screenToSet]->height / sizeY;
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
			if (*v0 * totalWidth <= (leftPadding + 
				screenInfo.screens[i]->width) * sizeX)
			{
				screenToSet = i;
				break;
			}
			leftPadding += screenInfo.screens[i]->width;
		}
	}
#ifdef PANORAMIX
	else if (!noPanoramiXExtension && priv->common->wcmGimp)
	{
		screenToSet = priv->screen_no;
		for (i = 0; i < screenToSet; i++)
			leftPadding += screenInfo.screens[i]->width;
		*v0 = (sizeX * leftPadding + *v0
			* screenInfo.screens[screenToSet]->width) /
			(double)totalWidth + 0.5;
		*v1 = *v1 * screenInfo.screens[screenToSet]->height /
			(double)maxHeight + 0.5;
	}

	if (!noPanoramiXExtension && priv->common->wcmGimp)
	{
		priv->factorX = totalWidth/sizeX;
		priv->factorY = maxHeight/sizeY;
		x = (*v0 - sizeX
			* leftPadding / totalWidth) * priv->factorX + 0.5;
		y = *v1 * priv->factorY + 0.5;
		
		if (x >= screenInfo.screens[screenToSet]->width)
			x = screenInfo.screens[screenToSet]->width - 1;
		if (y >= screenInfo.screens[screenToSet]->height)
			y = screenInfo.screens[screenToSet]->height - 1;
	}
	else
#endif
	{
		if (priv->screen_no == -1)
			*v0 = (*v0 * totalWidth - sizeX * leftPadding)
				/ screenInfo.screens[screenToSet]->width;
		else
			screenToSet = priv->screen_no;
		priv->factorX = screenInfo.screens[screenToSet]->width / sizeX;
		priv->factorY = screenInfo.screens[screenToSet]->height / sizeY;

		x = *v0 * priv->factorX + 0.5;
		y = *v1 * priv->factorY + 0.5;
	}

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
	int button, newb;
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int is_absolute = priv->flags & ABSOLUTE_FLAG;

	for (button=1; button<=16; button++)
	{
		int mask = 1 << (button-1);
	
		if ((mask & priv->oldButtons) != (mask & buttons))
		{
			DBG(4, ErrorF("xf86WcmSendButtons button=%d "
				"state=%d, for %s\n", 
				button, (buttons & mask) != 0, local->name));
			/* set to the configured buttons */
			newb = button;
			if (priv->button[button-1] != button)
				newb = priv->button[button-1];

			/* translate into Left Double Click */
			if (newb == 17)
			{
				newb = 1;
				if (buttons & mask)
				{
					/* Left button down */
					if (IsCursor(priv))
						xf86PostButtonEvent(local->dev, is_absolute, newb, 1,
							0, 6, rx, ry, rz, rrot, rth, rwheel);
					else
						xf86PostButtonEvent(local->dev, is_absolute, newb, 1,
							0, 6, rx, ry, rz, rtx, rty, rwheel);
					/* Left button up */
					if (IsCursor(priv))
						xf86PostButtonEvent(local->dev, is_absolute, newb, 0,
							0, 6, rx, ry, rz, rrot, rth, rwheel);
					else
						xf86PostButtonEvent(local->dev, is_absolute, newb, 0,
							0, 6, rx, ry, rz, rtx, rty, rwheel);
				}
			}
			if (newb < 17)
			{
				if ( newb == 1 )
				{
					/* deal with double click delays */
					long sec, usec;
					if ( !priv->oldTime && (buttons & mask) )
					{
						xf86getsecs(&sec, &usec);
						priv->oldTime = (sec * 1000) + (usec / 1000);
						priv->oldClickX = rx;
						priv->oldClickY = ry;
					}
					else if (buttons & mask)
					{
						priv->oldTime = 0;
						xf86getsecs(&sec, &usec);
						if ( ((sec * 1000) + (usec / 1000) - 
							priv->oldTime > priv->doubleSpeed ) ||
							( priv->oldClickX * priv->oldClickX + 
							priv->oldClickY * priv->oldClickY >
							priv->doubleRadius) )
						continue;
					}
				}

				if (IsCursor(priv))
					xf86PostButtonEvent(local->dev, is_absolute,
						newb, (buttons & mask) != 0,
						0, 6, rx, ry, rz, rrot, rth, rwheel);
				else
				{
					/* deal with Tablet PC buttons. */
					if ( common->wcmTPCButton )
					{
						if (rz <= 0 && (buttons & mask) ) 
							continue;
					}
					xf86PostButtonEvent(local->dev, is_absolute,
						newb, (buttons & mask) != 0,
						0, 6, rx, ry, rz, rtx, rty, rwheel);
				}
			}
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
	int wheel = ds->abswheel;

	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int rx, ry, rz, rtx, rty, rrot, rth, rw;
	int is_core_pointer, is_absolute, doffsetX=0, doffsetY=0;
	int aboveBelowSwitch = (priv->twinview == TV_ABOVE_BELOW)
		? ((y < priv->topY) ? -1 : ((priv->bottomY < y) ? 1 : 0)) : 0;
	int leftRightSwitch = (priv->twinview == TV_LEFT_RIGHT)
		? ((x < priv->topX) ? -1 : ((priv->bottomX < x) ? 1 : 0)) : 0;

	DBG(7, ErrorF("[%s] prox=%s x=%d y=%d z=%d "
		"b=%s b=%d tx=%d ty=%d wl=%d rot=%d th=%d\n",
		(type == STYLUS_ID) ? "stylus" :
			(type == CURSOR_ID) ? "cursor" : "eraser",
		is_proximity ? "true" : "false",
		x, y, z, is_button ? "true" : "false", buttons,
		tx, ty, wheel, rot, throttle));

	is_absolute = (priv->flags & ABSOLUTE_FLAG);
	is_core_pointer = xf86IsCorePointer(local->dev);

	if ( is_proximity || x || y || z || buttons || tx || ty || wheel )
	{
		switch ( leftRightSwitch )
		{
			case -1:
				doffsetX = 0;
				break;
			case 1:
				doffsetX = common->wcmMaxX;
				break;
		}
		switch ( aboveBelowSwitch )
		{
			case -1:
				doffsetY = 0;
				break;
			case 1:
				doffsetY = common->wcmMaxY;
				break;
		}
	}

	x += doffsetX;
	y += doffsetY;

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
		if (priv->twinview == TV_NONE)
		{
			rx = x > priv->bottomX ? priv->bottomX - priv->topX :
				x < priv->topX ? 0 : x - priv->topX;
			ry = y > priv->bottomY ? priv->bottomY - priv->topY :
				y < priv->topY ? 0 : y - priv->topY;
		}
		else
		{
			rx = x;
			ry = y;
		}
		rz = z;
		rtx = tx;
		rty = ty;
		rrot = rot;
		rth = throttle;
		rw = wheel;
	}
	else
	{
		if (priv->oldProximity)
		{
			/* unify acceleration in both directions */
			rx = (x - priv->oldX) * priv->factorY / priv->factorX;
			ry = y - priv->oldY;
		}
		else
		{
			rx = 0;
			ry = 0;
		}
		if (priv->speed != DEFAULT_SPEED )
		{
			/* don't apply speed for fairly small increments */
			int no_jitter = priv->speed * 3;
			double param = priv->speed;
			double relacc = (MAX_ACCEL-priv->accel)*(MAX_ACCEL-priv->accel);
			if (ABS(rx) > no_jitter)
			{
				/* don't apply acceleration when too fast. */
				param += priv->accel > 0 ? rx/relacc : 0;
				if (param > 20.00)
				{
					rx *= param;
				}
			}
			if (ABS(ry) > no_jitter)
			{
				param += priv->accel > 0 ? ry/relacc : 0;
				if (param > 20.00)
				{
					ry *= param;
				}
			}
		}
		rz = z - priv->oldZ;
		rtx = tx - priv->oldTiltX;
		rty = ty - priv->oldTiltY;
		rrot = rot - priv->oldRot;
		rth = throttle - priv->oldThrottle;
		rw = wheel - priv->oldWheel;
	}
DBG(6, ErrorF("xf86WcmSetScreen calling\n"));

	/* for multiple monitor support, we need to set the proper 
	 * screen and modify the axes before posting events */
	xf86WcmSetScreen(local, &rx, &ry);

DBG(6, ErrorF("xf86WcmSetScreen back\n"));
	/* coordinates are ready we can send events */
	if (is_proximity)
	{
		long sec, usec;
		if (!priv->oldProximity)
		{
			if (IsCursor(priv))
				xf86PostProximityEvent(
					local->dev, 1, 0, 6,
					rx, ry, rz, rrot,
					rth, rw);
			else
				xf86PostProximityEvent(
					local->dev, 1, 0, 6,
					rx, ry, rz, rtx, rty,
					rw);
		}

		/* deal with double click delays */
		xf86getsecs(&sec, &usec);
		if ( (sec * 1000) + (usec / 1000) - 
				priv->oldTime > priv->doubleSpeed ) 
			priv->oldTime = 0;

		if ( priv->oldTime ) {
			if ( (priv->oldClickX - rx ) * (priv->oldClickX - rx ) + 
				(priv->oldClickY - ry ) * (priv->oldClickY - ry ) >
				priv->doubleRadius * priv->doubleRadius )
				priv->oldTime = 0;
		}

		/* don't move cursor if we are expecting a double click */
		if (priv->oldTime)
		{
			rx = priv->oldClickX;
			ry = priv->oldClickY;
		}

DBG(6, ErrorF("calling xf86PostMotionEvent\n"));

		if(!(priv->flags & BUTTONS_ONLY_FLAG))
		{
			if (IsCursor(priv))
				xf86PostMotionEvent(local->dev,
					is_absolute, 0, 6, rx, ry, rz,
					rrot, rth, rw);
			else
				xf86PostMotionEvent(local->dev,
					is_absolute, 0, 6, rx, ry, rz,
					rtx, rty, rw);
		}
DBG(6, ErrorF("calling xf86WcmSendButtons\n"));

		if (priv->oldButtons != buttons)
		{
			xf86WcmSendButtons (local, buttons, rx, ry, rz,
					rtx, rty, rrot, rth, rw);
		}

		/* simulate button 4 and 5 for relative wheel */
		if ( ds->relwheel )
		{
			int fakeButton = ds->relwheel > 0 ? 5 : 4;
			int i;
			for (i=0; i<abs(ds->relwheel); i++)
			{
				xf86PostButtonEvent(local->dev, 
					is_absolute,
					fakeButton, 1, 0, 6, rx, ry, rz, 
					rrot, rth, rw);
				xf86PostButtonEvent(local->dev, 
					is_absolute,
					fakeButton, 0, 0, 6, rx, ry, rz, 
					rrot, rth, rw);
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
			buttons = 0;
			xf86WcmSendButtons (local, buttons, rx, ry, rz,
				rtx, rty, rrot, rth, rw);
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
						0,0,buttons,rrot,rth,
						rw);
				else
					xf86PostKeyEvent(local->dev,macro+7,1,
						is_absolute,0,6,
						0,0,buttons,rtx,rty,rw);

				/* key up */
				if (IsCursor(priv))
					xf86PostKeyEvent(local->dev,macro+7,0,
						is_absolute,0,6,
						0,0,buttons,rrot,rth,
						rw);
				else
					xf86PostKeyEvent(local->dev,macro+7,0,
						is_absolute,0,6,
						0,0,buttons,rtx,rty,rw);

			}
		}
		if (priv->oldProximity)
		{
			if (IsCursor(priv))
				xf86PostProximityEvent(local->dev,
						0, 0, 6, rx, ry, rz,
						rrot, rth, rw);
			else
				xf86PostProximityEvent(local->dev,
						0, 0, 6, rx, ry, rz,
						rtx, rty, rw);
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
 * xf86WcmSuppress --
 *  Determine whether device state has changed enough - return 1
 *  if not.
 ****************************************************************************/

static int xf86WcmSuppress(int suppress, const WacomDeviceState* dsOrig,
	const WacomDeviceState* dsNew)
{
	/* NOTE: Suppression value of zero disables suppression. */

	if (dsOrig->buttons != dsNew->buttons) return 0;
	if (dsOrig->proximity != dsNew->proximity) return 0;
	if (ABS(dsOrig->x - dsNew->x) > suppress) return 0;
	if (ABS(dsOrig->y - dsNew->y) > suppress) return 0;
	if (ABS(dsOrig->pressure - dsNew->pressure) > suppress) return 0;
	if (ABS(dsOrig->throttle - dsNew->throttle) > suppress) return 0;

	if ((1800 + dsOrig->rotation - dsNew->rotation) % 1800 > suppress &&
		(1800 + dsNew->rotation - dsOrig->rotation) % 1800 > suppress)
		return 0;

	/* look for change in absolute wheel
	 * position or any relative wheel movement */
	if ((ABS(dsOrig->abswheel - dsNew->abswheel) > suppress) ||
		(dsNew->relwheel != 0)) return 0;

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

/* reset raw data counters for filters */
static void resetSampleCounter(const WacomChannelPtr pChannel)
{
	/* if out of proximity, reset hardware filter */
	if (!pChannel->valid.state.proximity)
	{
		pChannel->nSamples = 0;
		pChannel->rawFilter.npoints = 0;
		pChannel->rawFilter.statex = 0;
		pChannel->rawFilter.statey = 0;
	}
}

/*****************************************************************************
 * xf86WcmEvent -
 *   Handles suppression, transformation, filtering, and event dispatch.
 ****************************************************************************/

void xf86WcmEvent(WacomCommonPtr common, unsigned int channel,
	const WacomDeviceState* pState)
{
	WacomDeviceState* pLast;
	WacomDeviceState ds;
	WacomChannelPtr pChannel;

	/* sanity check the channel */
	if (channel >= MAX_CHANNELS)
		return;
	
	pChannel = common->wcmChannel + channel;
	pLast = &pChannel->valid.state;

	/* we must copy the state because certain types of filtering
	 * will need to change the values (ie. for error correction) */
	ds = *pState;

	/* timestamp the state for velocity and acceleration analysis */
	ds.sample = GetTimeInMillis();

	DBG(10, ErrorF("xf86WcmEvent: c=%d i=%d t=%d s=%P x=%d y=%d b=%P "
		"p=%d rz=%d tx=%d ty=%d aw=%d rw=%d t=%d df=%d px=%d st=%d\n",
		channel,
		ds.device_id,
		ds.device_type,
		ds.serial_num,
		ds.x, ds.y, ds.buttons,
		ds.pressure, ds.rotation, ds.tiltx,
		ds.tilty, ds.abswheel, ds.relwheel, ds.throttle,
		ds.discard_first, ds.proximity, ds.sample));

	DBG(11, ErrorF("filter %d, %p\n",RAW_FILTERING(common),
		common->wcmModel->FilterRaw));

	/* Filter raw data, fix hardware defects, perform error correction */
	if (RAW_FILTERING(common) && common->wcmModel->FilterRaw)
	{
		if (common->wcmModel->FilterRaw(common,pChannel,&ds))
		{
			DBG(10, ErrorF("Raw filtering discarded data.\n"));
			resetSampleCounter(pChannel);
			return; /* discard */
		}
	}

	/* Discard unwanted data */
	if (xf86WcmSuppress(common->wcmSuppress, pLast, &ds))
	{
		DBG(10, ErrorF("Suppressing data according to filter\n"));

		/* If throttle is not in use, discard data. */
		if (ABS(ds.throttle) < common->wcmSuppress)
		{
			resetSampleCounter(pChannel);
			return;
		}

		/* Otherwise, we need this event for time-rate-of-change
		 * values like the throttle-to-relative-wheel filter.
		 * To eliminate position change events, we reset all values
		 * to last unsuppressed position. */

		ds = *pLast;
		RESET_RELATIVE(ds);
	}

	/* JEJ - Do not move this code without discussing it with me.
	 * The device state is invariant of any filtering performed below.
	 * Changing the device state after this point can and will cause
	 * a feedback loop resulting in oscillations, error amplification,
	 * unnecessary quantization, and other annoying effects. */

	/* save channel device state and device to which last event went */
	memmove(pChannel->valid.states + 1,
		pChannel->valid.states,
		sizeof(WacomDeviceState) * (MAX_SAMPLES - 1));
	pChannel->valid.state = ds; /*save last raw sample */
	if (pChannel->nSamples < 4) ++pChannel->nSamples;

	commonDispatchDevice(common,pChannel);

	resetSampleCounter(pChannel);
}

static void commonDispatchDevice(WacomCommonPtr common,
	const WacomChannelPtr pChannel)
{
	int id, idx;
	WacomDevicePtr priv;
	LocalDevicePtr pDev = NULL;
	LocalDevicePtr pLastDev = pChannel->pDev;
	WacomDeviceState* ds = &pChannel->valid.states[0];
	WacomDeviceState* pLast = &pChannel->valid.states[1];

	DBG(10, ErrorF("commonDispatchEvents\n"));

	/* Find the device the current events are meant for */
	for (idx=0; idx<common->wcmNumDevices; idx++)
	{
		priv = common->wcmDevices[idx]->private;
		id = DEVICE_ID(priv->flags);

		if (id == ds->device_type &&
			((!priv->serial) || (ds->serial_num == priv->serial)))
		{
			if ((priv->topX <= ds->x && priv->bottomX > ds->x &&
				priv->topY <= ds->y && priv->bottomY > ds->y))
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

	DBG(11, ErrorF("commonDispatchEvents: %p %p\n",pDev,pLastDev));

	/* if the logical device of the same physical tool has changed,
	 * send proximity out to the previous one */
	if (pLastDev && (pLastDev != pDev) &&
		(pLast->serial_num == ds->serial_num))
	{
		pLast->proximity = 0;
		xf86WcmSendEvents(pLastDev, pLast);
	}

	/* if a device matched criteria, handle filtering per device
	 * settings, and send event to XInput */
	if (pDev)
	{
		WacomDeviceState filtered = pChannel->valid.state;
		WacomDevicePtr priv = pDev->private;

		/* Device transformations come first */

		/* button 1 Threshold test */
		int button = 1;
		if ( !IsCursor(priv) )
		{
			if (filtered.pressure < common->wcmThreshold )
				filtered.buttons &= ~button;
			else
				filtered.buttons |= button;
		}

		/* transform pressure */
		transPressureCurve(priv,&filtered);

		/* User-requested filtering comes next */

		/* User-requested transformations come last */

		#if 0

		/* not quite ready for prime-time;
		 * it needs to be possible to disable,
		 * and returning throttle to zero does
		 * not reset the wheel, yet. */

		int sampleTime, ticks;

		/* get the sample time */
		sampleTime = GetTimeInMillis(); 
		
		ticks = ThrottleToRate(ds->throttle);

		/* throttle filter */
		if (!ticks)
		{
			priv->throttleLimit = -1;
		}
		else if ((priv->throttleStart > sampleTime) ||
			(priv->throttleLimit == -1))
		{
			priv->throttleStart = sampleTime;
			priv->throttleLimit = sampleTime + ticks;
		}
		else if (priv->throttleLimit < sampleTime)
		{
			DBG(6, ErrorF("LIMIT REACHED: s=%d l=%d n=%d v=%d "
				"N=%d\n", priv->throttleStart,
				priv->throttleLimit, sampleTime,
				ds->throttle, sampleTime + ticks));

			ds->relwheel = (ds->throttle > 0) ? 1 :
					(ds->throttle < 0) ? -1 : 0;

			priv->throttleStart = sampleTime;
			priv->throttleLimit = sampleTime + ticks;
		}
		else
			priv->throttleLimit = priv->throttleStart + ticks;

		#endif /* throttle */

		/* force out-prox when height is greater than 112. 
		 * This only applies to USB protocol V tablets
		 * which aimed at improving relative movement support. 
		 */
		if (filtered.distance > 112 && !(priv->flags & ABSOLUTE_FLAG))
		{
			ds->proximity = 0;
			filtered.proximity = 0;
		}

		xf86WcmSendEvents(pDev, &filtered);
	}

	/* otherwise, if no device matched... */
	else
	{
		DBG(11, ErrorF("no device matches with id=%d, serial=%d\n",
				ds->device_type, ds->serial_num));
	}

	/* save the last device */
	pChannel->pDev = pDev;
}

/*****************************************************************************
 * xf86WcmInitTablet -- common initialization for all tablets
 ****************************************************************************/

int xf86WcmInitTablet(WacomCommonPtr common, WacomModelPtr model,
	int fd, const char* id, float version)
{
	/* Initialize the tablet */
	model->Initialize(common,fd,id,version);

	/* Get tablet resolution */
	if (model->GetResolution)
		model->GetResolution(common,fd);

	/* Get tablet range */
	if (model->GetRanges && (model->GetRanges(common,fd) != Success))
		return !Success;
	
	/* Default threshold value if not set */
	if (common->wcmThreshold <= 0)
	{
		/* Threshold for counting pressure as a button */
		common->wcmThreshold = common->wcmMaxZ * 3 / 50;
		ErrorF("%s Wacom using pressure threshold of %d for button 1\n",
			XCONFIG_PROBED, common->wcmThreshold);
	}

	/* Reset tablet to known state */
	if (model->Reset && (model->Reset(common,fd) != Success))
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Enable tilt mode, if requested and available */
	if ((common->wcmFlags & TILT_REQUEST_FLAG) && model->EnableTilt)
	{
		if (model->EnableTilt(common,fd) != Success)
			return !Success;
	}

	/* Enable hardware suppress, if requested and available */
	if ((common->wcmSuppress != 0) && model->EnableSuppress)
	{
		if (model->EnableSuppress(common,fd) != Success)
			return !Success;
	}

	/* change the serial speed, if requested */
	if (common->wcmLinkSpeed != 9600)
	{
		if (model->SetLinkSpeed)
		{
			if (model->SetLinkSpeed(common,fd) != Success)
				return !Success;
		}
		else
		{
			ErrorF("Tablet does not support setting link "
				"speed, or not yet implemented\n");
		}
	}

	/* output tablet state as probed */
	if (xf86Verbose)
		ErrorF("%s Wacom %s tablet speed=%d maxX=%d maxY=%d maxZ=%d "
			"resX=%d resY=%d suppress=%d tilt=%s\n",
			XCONFIG_PROBED,
			model->name, common->wcmLinkSpeed,
			common->wcmMaxX, common->wcmMaxY, common->wcmMaxZ,
			common->wcmResolX, common->wcmResolY,
			common->wcmSuppress,
			HANDLE_TILT(common) ? "enabled" : "disabled");
  
	/* start the tablet data */
	if (model->Start && (model->Start(common,fd) != Success))
		return !Success;

	/*set the model */
	common->wcmModel = model;

	return Success;
}


/*****************************************************************************
** Transformations
*****************************************************************************/

static void transPressureCurve(WacomDevicePtr pDev, WacomDeviceStatePtr pState)
{
	if (pDev->pPressCurve)
	{
		int p = pState->pressure;

		/* clip */
		p = (p < 0) ? 0 : (p > pDev->common->wcmMaxZ) ?
			pDev->common->wcmMaxZ : p;

		/* rescale pressure to FILTER_PRESSURE_RES */
		p = (p * FILTER_PRESSURE_RES) / pDev->common->wcmMaxZ;

		/* apply pressure curve function */
		p = pDev->pPressCurve[p];

		/* scale back to wcmMaxZ */
		pState->pressure = (p * pDev->common->wcmMaxZ) /
			FILTER_PRESSURE_RES;
	}
}
