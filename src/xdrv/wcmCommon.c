/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2006 by Ping Cheng, Wacom Technology. <pingc@wacom.com>		
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

/*
 #if XF86_VERSION_MAJOR < 4
 *
 * There is a bug in XFree86 for combined left click and
 * other button. It'll lost left up when releases.
 * This should be removed if XFree86 fixes the problem.
 * This bug happens on Xorg as well.
#  define XF86_BUTTON1_BUG
#endif
*/
#define XF86_BUTTON1_BUG

WacomDeviceClass* wcmDeviceClasses[] =
{
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
static void commonDispatchDevice(WacomCommonPtr common, unsigned int channel, 
	const WacomChannelPtr pChannel);
static void resetSampleCounter(const WacomChannelPtr pChannel);
static void sendAButton(LocalDevicePtr local, int button, int mask,
		int rx, int ry, int rz, int v3, int v4, int v5);
 
/*****************************************************************************
 * xf86WcmSetScreen --
 *   set to the proper screen according to the converted (x,y).
 *   this only supports for horizontal setup now.
 *   need to know screen's origin (x,y) to support 
 *   combined horizontal and vertical setups
 ****************************************************************************/

static void xf86WcmSetScreen(LocalDevicePtr local, int *value0, int *value1)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int screenToSet = 0;
	int totalWidth = 0, maxHeight = 0, leftPadding = 0;
	int i, x, y, v0 = *value0, v1 = *value1;
	double sizeX = priv->bottomX - priv->topX - 2*priv->tvoffsetX;
	double sizeY = priv->bottomY - priv->topY - 2*priv->tvoffsetY;

	DBG(6, ErrorF("xf86WcmSetScreen v0=%d v1=%d\n", *value0, *value1));

	if (priv->twinview == TV_NONE && (priv->flags & ABSOLUTE_FLAG))
	{
		v0 = v0 > priv->bottomX ? priv->bottomX - priv->topX :
			v0 < priv->topX ? 0 : v0 - priv->topX;
		v1 = v1 > priv->bottomY ? priv->bottomY - priv->topY :
			v1 < priv->topY ? 0 : v1 - priv->topY;
	}

	/* set factorX and factorY for single screen setup since
	 * Top X Y and Bottom X Y can be changed while driver is running
	 */
	if (screenInfo.numScreens == 1 || !priv->common->wcmMMonitor)
	{
		if (priv->twinview != TV_NONE && (priv->flags & ABSOLUTE_FLAG))
		{
			if (priv->screen_no == -1)
			{
				if (priv->twinview == TV_LEFT_RIGHT)
				{
					if (v0 > priv->bottomX - priv->tvoffsetX && v0 <= priv->bottomX)
						priv->currentScreen = 1;
					if (v0 > priv->topX && v0 <= priv->topX + priv->tvoffsetX)
						priv->currentScreen = 0;
				}
				if (priv->twinview == TV_ABOVE_BELOW)
				{
					if (v1 > priv->bottomY - priv->tvoffsetY && v1 <= priv->bottomY)
						priv->currentScreen = 1;
					if (v1 > priv->topY && v1 <= priv->topY + priv->tvoffsetY)
						priv->currentScreen = 0;
				}
			}
			else
				priv->currentScreen = priv->screen_no;
			priv->factorX = priv->tvResolution[2*priv->currentScreen] / sizeX;
			priv->factorY = priv->tvResolution[2*priv->currentScreen+1] / sizeY;
		}
		else
		{
			/* tool on the tablet when driver starts */
			if (miPointerCurrentScreen())
				priv->currentScreen = miPointerCurrentScreen()->myNum;
			priv->factorX = screenInfo.screens[priv->currentScreen]->width / sizeX;
			priv->factorY = screenInfo.screens[priv->currentScreen]->height / sizeY;
		}
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
			if (v0 * totalWidth <= (leftPadding + 
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
		v0 = (sizeX * leftPadding + v0
			* screenInfo.screens[screenToSet]->width) /
			(double)totalWidth + 0.5;
		v1 = v1 * screenInfo.screens[screenToSet]->height /
			(double)maxHeight + 0.5;
	}

	if (!noPanoramiXExtension && priv->common->wcmGimp)
	{
		priv->factorX = totalWidth/sizeX;
		priv->factorY = maxHeight/sizeY;
		x = (v0 - sizeX
			* leftPadding / totalWidth) * priv->factorX + 0.5;
		y = v1 * priv->factorY + 0.5;
		
		if (x >= screenInfo.screens[screenToSet]->width)
			x = screenInfo.screens[screenToSet]->width - 1;
		if (y >= screenInfo.screens[screenToSet]->height)
			y = screenInfo.screens[screenToSet]->height - 1;
	}
	else
#endif
	{
		if (priv->screen_no == -1)
			v0 = (v0 * totalWidth - sizeX * leftPadding)
				/ screenInfo.screens[screenToSet]->width;
		else
			screenToSet = priv->screen_no;
		priv->factorX = screenInfo.screens[screenToSet]->width / sizeX;
		priv->factorY = screenInfo.screens[screenToSet]->height / sizeY;

		x = v0 * priv->factorX + 0.5;
		y = v1 * priv->factorY + 0.5;
	}

	xf86XInputSetScreen(local, screenToSet, x, y);
	DBG(10, ErrorF("xf86WcmSetScreen current=%d ToSet=%d\n", 
		priv->currentScreen, screenToSet));
	priv->currentScreen = screenToSet;
}

/*****************************************************************************
 * xf86WcmSendButtons --
 *   Send button events by comparing the current button mask with the
 *   previous one.
 ****************************************************************************/

static void xf86WcmSendButtons(LocalDevicePtr local, int buttons, int rx, int ry, 
		int rz, int v3, int v4, int v5)
{
	int button, mask, bsent = 0;
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	DBG(6, ErrorF("xf86WcmSendButtons buttons=%d for %s\n", buttons, local->name));

	/* Tablet PC buttons. */
	if ( common->wcmTPCButton && !IsCursor(priv) && !IsPad(priv) && !IsEraser(priv) )
	{
		if ( buttons & 1 )
		{
			if ( !(priv->flags & TPCBUTTONS_FLAG) )
			{
				priv->flags |= TPCBUTTONS_FLAG;

				bsent = 0;

				/* send all pressed buttons down */
				for (button=2; button<=MAX_BUTTONS; button++)
				{
					mask = 1 << (button-1);
					if ( buttons & mask ) 
					{
						bsent = 1;
						/* set to the configured button */
						sendAButton(local, button-1, 1, rx, ry, 
							rz, v3, v4, v5);
					}
				}
				
#ifdef XF86_BUTTON1_BUG
				/* only send button one when nothing else was sent */
				if ( !bsent && (buttons & 1) )
				{
					priv->flags |= TPCBUTTONONE_FLAG;
					sendAButton(local, 0, 1, rx, ry, 
						rz, v3, v4, v5);
				}
#endif
			}
			else
			{
				bsent = 0;
				for (button=2; button<=MAX_BUTTONS; button++)
				{
					mask = 1 << (button-1);
					if ((mask & priv->oldButtons) != (mask & buttons))
					{
#ifdef XF86_BUTTON1_BUG
						/* Send button one up before any button down is sent.
						 * There is a bug in XFree86 for combined left click and 
						 * other button. It'll lost left up when releases.
						 * This should be removed if XFree86 fixes the problem.
						 */
						if (priv->flags & TPCBUTTONONE_FLAG && !bsent)
						{
							priv->flags &= ~TPCBUTTONONE_FLAG;
							sendAButton(local, 0, 0, rx, ry, rz, 
								v3, v4, v5);
							bsent = 1;
						}
#endif
						/* set to the configured buttons */
						sendAButton(local, button-1, mask & buttons, 
							rx, ry, rz, v3, v4, v5);
					}
				}
			}
		}
		else if ( priv->flags & TPCBUTTONS_FLAG )
		{
			priv->flags &= ~TPCBUTTONS_FLAG;

			/* send all pressed buttons up */
			for (button=2; button<=MAX_BUTTONS; button++)
			{
				mask = 1 << (button-1);
				if ((mask & priv->oldButtons) != (mask & buttons) || (mask & buttons) )
				{
					/* set to the configured button */
					sendAButton(local, button-1, 0, rx, ry, 
						rz, v3, v4, v5);
				}
			}
#ifdef XF86_BUTTON1_BUG
			/* This is also part of the workaround of the XFree86 bug mentioned above
			 */
			if (priv->flags & TPCBUTTONONE_FLAG)
			{
				priv->flags &= ~TPCBUTTONONE_FLAG;
				sendAButton(local, 0, 0, rx, ry, rz, v3, v4, v5);
			}
#endif
		}
	}
	else  /* normal buttons */
	{
		for (button=1; button<=MAX_BUTTONS; button++)
		{
			mask = 1 << (button-1);
			if ((mask & priv->oldButtons) != (mask & buttons))
			{
				/* set to the configured button */
				sendAButton(local, button-1, mask & buttons, rx, ry, 
					rz, v3, v4, v5);
			}
		}
	}
}

/*****************************************************************************
 * emitKeysym --
 *   Emit a keydown/keyup event
 ****************************************************************************/

static void emitKeysym (DeviceIntPtr keydev, int keysym, int state)
{
	int i;

	/* Now that we have the keycode look for key index */
	KeySymsRec *ksr = &keydev->key->curKeySyms;

	for (i = ksr->minKeyCode; i <= ksr->maxKeyCode; i++)
		if (ksr->map [(i - ksr->minKeyCode) * ksr->mapWidth] == keysym)
			break;

	if (i > ksr->maxKeyCode)
	{
		xf86Msg (X_WARNING, "Couldn't find key with code %08x on keyboard device %s\n",
			 keysym, keydev->name);
		return;
	}
	xf86PostKeyboardEvent (keydev, i, state);
}

/*****************************************************************************
 * sendAButton --
 *   Send one button event, called by xf86WcmSendButtons
 ****************************************************************************/

static void sendAButton(LocalDevicePtr local, int button, int mask,
		int rx, int ry, int rz, int v3, int v4, int v5)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int is_absolute = priv->flags & ABSOLUTE_FLAG;
	int is_core = local->flags & XI86_ALWAYS_CORE;
	int i, button_idx, naxes = priv->naxes;
	static struct
	{
		unsigned ac_mask;
		unsigned keysym;
	} mods [] =
	{
		{ AC_SHIFT, XK_Shift_L },
		{ AC_CONTROL, XK_Control_L },
		{ AC_META, XK_Meta_L },
		{ AC_ALT, XK_Alt_L },
		{ AC_SUPER, XK_Super_L },
		{ AC_HYPER, XK_Hyper_L },
	};

	if (IsPad(priv))
		button -= 8;

	if (button < 0 || button >= priv->nbuttons)
	{
		/* should never happen */
		ErrorF ("sendAButton: Invalid button index %d (number of defined buttons = %d)\n", button, priv->nbuttons);
		return; 
	}

	DBG(4, ErrorF("sendAButton TPCButton(%s) button=%d state=%d code=%08x, for %s\n",
		common->wcmTPCButton ? "on" : "off", button, mask, priv->button[button], local->name));

	button_idx = button;
	button = priv->button[button];
	if (!button)
		return;

	/* Switch the device to core mode, if required
	 */
	if (!is_core && (button & AC_CORE))
		xf86XInputSetSendCoreEvents (local, TRUE);

	/* Emulate modifier key presses, if required
	 */
	if (mask != 0)
		for (i = 0; i < sizeof (mods) / sizeof (mods [0]); i++)
			if (button & mods [i].ac_mask)
				emitKeysym (inputInfo.keyboard, mods [i].keysym, 1);

	switch (button & AC_TYPE)
	{
	case AC_BUTTON:
/*		xf86PostButtonEvent(local->dev, is_absolute, button & AC_CODE,
*/		/* Dynamically modify the button map as required --
		 * to be moved in the place where button mappings are changed
		 */
		local->dev->button->map [button_idx] = button & AC_CODE;

		xf86PostButtonEvent(local->dev, is_absolute, button_idx,
			mask != 0,0,naxes,rx,ry,rz,v3,v4,v5);

		break;

	case AC_KEY:
		if (button & AC_CORE)
			emitKeysym (inputInfo.keyboard, button & AC_CODE, mask != 0);
		else
			ErrorF ("WARNING: Devices without SendCoreEvents cannot emit key events!\n");
		break;

	case AC_MODETOGGLE:
		if (!mask)
			break;

		if (priv->flags & ABSOLUTE_FLAG)
		{
			priv->flags &= ~ABSOLUTE_FLAG;
			xf86ReplaceStrOption(local->options, "Mode", "Relative");
		}
		else
		{
			priv->flags |= ABSOLUTE_FLAG;
			xf86ReplaceStrOption(local->options, "Mode", "Absolute");
		}
		break;

	case AC_DBLCLICK:
		/* Dynamically modify the button map as required --
		 * to be moved in the place where button mappings are changed
		 */
		local->dev->button->map [button_idx] = button & AC_CODE;

		if (mask)
		{
			/* Left button down */
			xf86PostButtonEvent(local->dev, is_absolute,
				button_idx, 1,0,naxes, 
				rx,ry,rz,v3,v4,v5);

			/* Left button up */
			xf86PostButtonEvent(local->dev, is_absolute,
				button_idx,0,0,naxes,
				rx,ry,rz, v3,v4,v5);
		}

		/* Left button down/up upon mask is 1/0 */
		xf86PostButtonEvent(local->dev, is_absolute, button_idx, 
			mask != 0,0,naxes,rx,ry,rz,v3,v4,v5);
		break;
	}

	/* Release modifier keys in reverse order
	 */
	if (mask == 0)
		for (i = sizeof (mods) / sizeof (mods [0]) - 1; i >= 0; i--)
			if (button & mods [i].ac_mask)
				emitKeysym (inputInfo.keyboard, mods [i].keysym, 0);

	/* Switch the device out of the core mode, if required
	 */
	if (!is_core && (button & AC_CORE))
		xf86XInputSetSendCoreEvents (local, FALSE);
}

/*****************************************************************************
 * xf86WcmSendEvents --
 *   Send events according to the device state.
 ****************************************************************************/

void xf86WcmSendEvents(LocalDevicePtr local, const WacomDeviceState* ds, unsigned int channel)
{
	int type = ds->device_type;
	int id = ds->device_id;
	int serial = (int)ds->serial_num;
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
	int tmp_coord;
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int naxes = priv->naxes;

	int rx, ry, rz, v3, v4, v5, no_jitter;
	double param, relacc;
	int is_absolute;

	if (priv->serial && serial != priv->serial)
	{
		DBG(10, ErrorF("[%s] serial number is %d but your system configured %d", 
			local->name, serial, (int)priv->serial));
		return;
	}

	/* use tx and ty to report stripx and stripy */
	if (type == PAD_ID)
	{
		tx = ds->stripx;
		ty = ds->stripy;
	}

	DBG(7, ErrorF("[%s] prox=%s x=%d y=%d z=%d "
		"b=%s b=%d tx=%d ty=%d wl=%d rot=%d th=%d\n",
		(type == STYLUS_ID) ? "stylus" :
			(type == CURSOR_ID) ? "cursor" : 
			(type == ERASER_ID) ? "eraser" : "pad",
		is_proximity ? "true" : "false",
		x, y, z, is_button ? "true" : "false", buttons,
		tx, ty, wheel, rot, throttle));

	/* rotation mixes x and y up a bit */
	if (common->wcmRotate == ROTATE_CW)
	{
		tmp_coord = x;
		x = y;
		y = common->wcmMaxY - tmp_coord;
	}
	else if (common->wcmRotate == ROTATE_CCW)
	{
		tmp_coord = y;
		y = x;
		x = common->wcmMaxX - tmp_coord;
	}
	else if (common->wcmRotate == ROTATE_HALF)
	{
		x = common->wcmMaxX - x;
		y = common->wcmMaxY - y;
	}

	is_absolute = (priv->flags & ABSOLUTE_FLAG);

	/* sets rx and ry according to the mode */
	if (is_absolute)
	{
		rx = x;
		ry = y;
		rz = z;
		if (IsCursor(priv)) 
		{
			v3 = rot;
			v4 = throttle;
		}
		else
		{
			v3 = tx;
			v4 = ty;
		}
		v5 = wheel;
	}
	else
	{
		if (priv->oldProximity)
		{
			if (x > priv->bottomX || x < priv->topX)
				x = priv->oldX;
			if (y > priv->bottomY || y < priv->topY)
				y = priv->oldY;

			rx = x - priv->oldX;
			ry = y - priv->oldY;
			rz = z - priv->oldZ;
		}
		else
		{
			rx = 0;
			ry = 0;
			rz = 0;
		}

		/* don't apply speed for fairly small increments */
		no_jitter = (priv->speed*3 > 4) ? priv->speed*3 : 4;
		relacc = (MAX_ACCEL-priv->accel)*(MAX_ACCEL-priv->accel);
		if (ABS(rx) > no_jitter)
		{
			param = priv->speed;

			/* apply acceleration only when priv->speed > DEFAULT_SPEED */
			if (priv->speed > DEFAULT_SPEED )
			{
				param += priv->accel > 0 ? abs(rx)/relacc : 0;
				/* don't apply acceleration when too fast. */
				if (param < 20.00)
				{
					param = 20.00;
				}
			}
			rx *= param;
		}
		if (ABS(ry) > no_jitter)
		{
			param = priv->speed;
			/* apply acceleration only when priv->speed > DEFAULT_SPEED */
			if (priv->speed > DEFAULT_SPEED )
			{
				param += priv->accel > 0 ? abs(ry)/relacc : 0;
				/* don't apply acceleration when too fast. */
				if (param < 20.00)
				{
					ry *= param;
				}
			}
			ry *= param;
		}
		if (IsCursor(priv)) 
		{
			v3 = rot - priv->oldRot;
			v4 = throttle - priv->oldThrottle;
		}
		else
		{
			v3 = tx - priv->oldTiltX;
			v4 = ty - priv->oldTiltY;
		}
		v5 = wheel - priv->oldWheel;
	}

	priv->currentX = rx;
	priv->currentY = ry;

	DBG(6, ErrorF("[%s] %s prox=%d\tx=%d\ty=%d\tz=%d\tv3=%d\tv4=%d\tv5=%d\tid=%d"
		"\tserial=%d\tbutton=%s\tbuttons=%d\t on channel=%d\n",
		local->name,
		is_absolute ? "abs" : "rel",
		is_proximity,
		rx, ry, rz, v3, v4, v5, id, serial,
		is_button ? "true" : "false", buttons, channel));

	/* report tool id in the upper 2 bytes of 4th valuator 
	 * serial number in the upper 2 bytes of 5th and 6th 
	 * valuators for the 4 bytes of serial number .
	 * All tools except pad
	 */
	if (type != PAD_ID)
	{
		v3 = ((id<<16) & 0xffff0000) | (((short)v3)& 0xffff);
		v4 = (serial & 0xffff0000) | (((short)v4)& 0xffff);
		v5 = ((serial<<16) & 0xffff0000) | (((short)v5)& 0xffff);

		/* coordinates are ready we can send events */
		if (is_proximity)
		{
			/* for multiple monitor support, we need to set the proper 
			 * screen and modify the axes before posting events */
			if(!(priv->flags & BUTTONS_ONLY_FLAG))
			{
				xf86WcmSetScreen(local, &rx, &ry);
			}

			/* unify acceleration in both directions for relative mode to draw a circle */
			if (!is_absolute)
				rx *= priv->factorY / priv->factorX;

			/* don't emit proximity events if device does not support proximity */
			if ((local->dev->proximity && !priv->oldProximity))
				xf86PostProximityEvent(local->dev, 1, 0, naxes,
					rx, ry, rz, v3, v4, v5);


			if(!(priv->flags & BUTTONS_ONLY_FLAG))
				xf86PostMotionEvent(local->dev, is_absolute,
					0, naxes, rx, ry, rz, v3, v4, v5);

			if (priv->oldButtons != buttons)
			{
				xf86WcmSendButtons(local,buttons,rx,ry,rz,v3,v4,v5);
			}

			/* simulate button 4 and 5 for relative wheel */
			if ( ds->relwheel )
			{
				int fakeButton = ds->relwheel > 0 ? 5 : 4;
 				int i;
				for (i=0; i<abs(ds->relwheel); i++)
				{
					/* Dynamically modify the button map
					 */
					local->dev->button->map [fakeButton] = fakeButton;
					xf86PostButtonEvent(local->dev, is_absolute,
						fakeButton, 1, 0, naxes, rx, ry, rz,
						v3, v4, v5);
					xf86PostButtonEvent(local->dev, is_absolute,
						fakeButton, 0, 0, naxes, rx, ry, rz,
						v3, v4, v5);
				}
			}
		}

		/* not in proximity */
		else
		{
			buttons = 0;

			/* reports button up when the device has been
			 * down and becomes out of proximity */
			if (priv->oldButtons)
				xf86WcmSendButtons(local,0,rx,ry,rz,v3,v4,v5);
			if (priv->oldProximity && local->dev->proximity)
				xf86PostProximityEvent(local->dev,0,0,naxes,rx,ry,rz,v3,v4,v5);
		} /* not in proximity */
	}
	else
	{
		if ((v3 || v4 || buttons || ds->relwheel) && !priv->oldProximity)
		{
			xf86PostProximityEvent(local->dev, 1, 0, naxes,
				rx, ry, rz, v3, v4, v5);
			is_proximity = 1;
		}
		if ( v3 || v4 )
			xf86PostMotionEvent(local->dev, is_absolute,
				0, naxes, rx, ry, rz, v3, v4, v5);
		if (priv->oldButtons != buttons)
			xf86WcmSendButtons(local, buttons, 
				rx, ry, rz, v3, v4, v5);

		if ( ds->relwheel )
		{
			int fakeButton = ds->relwheel > 0 ? 5 : 4;
 			int i;
			for (i=0; i<abs(ds->relwheel); i++)
			{
				/* Dynamically modify the button map
				 */
				local->dev->button->map [fakeButton] = fakeButton;
				xf86PostButtonEvent(local->dev, is_absolute,
					fakeButton, 1, 0, naxes, rx, ry, rz,
					v3, v4, v5);
				xf86PostButtonEvent(local->dev, is_absolute,
					fakeButton, 0, 0, naxes, rx, ry, rz,
					v3, v4, v5);
			}
		}
		if ( !v3 && !v4 && !buttons)
		{
 			xf86PostProximityEvent(local->dev, 0, 0, naxes, 
				rx, ry, rz, v3, v4, v5);
			is_proximity = 0;
		}
	}
	priv->oldProximity = is_proximity;
	priv->oldButtons = buttons;
	priv->oldWheel = wheel;
	priv->oldX = x;
	priv->oldY = y;
	priv->oldZ = z;
	priv->oldTiltX = tx;
	priv->oldTiltY = ty;
	priv->oldStripX = ds->stripx;
	priv->oldStripY = ds->stripy;
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
	DBG(11, ErrorF("xf86WcmSuppress checking data (suppress=%d)\n", suppress));

	if (dsOrig->buttons != dsNew->buttons) return 0;
	if (dsOrig->proximity != dsNew->proximity) return 0;
	if (ABS(dsOrig->x - dsNew->x) > suppress) return 0;
	if (ABS(dsOrig->y - dsNew->y) > suppress) return 0;
	if (ABS(dsOrig->tiltx - dsNew->tiltx) > suppress) return 0;
	if (ABS(dsOrig->tilty - dsNew->tilty) > suppress) return 0;
	if (ABS(dsOrig->stripx - dsNew->stripx) > suppress) return 0;
	if (ABS(dsOrig->stripy - dsNew->stripy) > suppress) return 0;
	if (ABS(dsOrig->pressure - dsNew->pressure) > suppress) return 0;
	if (ABS(dsOrig->throttle - dsNew->throttle) > suppress) return 0;
	if (ABS(dsOrig->rotation - dsNew->rotation) > suppress &&
		(1800 - ABS(dsOrig->rotation - dsNew->rotation)) >  suppress) return 0;

	/* look for change in absolute wheel
	 * position or any relative wheel movement */
	if ((ABS(dsOrig->abswheel - dsNew->abswheel) > suppress) ||
		(dsNew->relwheel != 0)) return 0;

	DBG(11, ErrorF("xf86WcmSuppress discarded data\n"));
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
	WacomFilterState* fs;
	int i;

	/* tool on the tablet when driver starts */
	if (!miPointerCurrentScreen())
	{
		DBG(6, ErrorF("xf86WcmEvent: Wacom driver can not get Current Screen ID\n"));
		DBG(6, ErrorF("Please remove Wacom tool from the tablet.\n"));
		return;
	}

	/* sanity check the channel */
	if (channel >= MAX_CHANNELS)
		return;
	
	pChannel = common->wcmChannel + channel;
	pLast = &pChannel->valid.state;

	/* we must copy the state because certain types of filtering
	 * will need to change the values (ie. for error correction) */
	ds = *pState;

	/* timestamp the state for velocity and acceleration analysis */
	ds.sample = (int)GetTimeInMillis();

	DBG(10, ErrorF("xf86WcmEvent: c=%d i=%d t=%d s=%u x=%d y=%d b=%d "
		"p=%d rz=%d tx=%d ty=%d aw=%d rw=%d t=%d df=%d px=%d st=%d\n",
		channel,
		ds.device_id,
		ds.device_type,
		ds.serial_num,
		ds.x, ds.y, ds.buttons,
		ds.pressure, ds.rotation, ds.tiltx,
		ds.tilty, ds.abswheel, ds.relwheel, ds.throttle,
		ds.discard_first, ds.proximity, ds.sample));

#ifdef LINUX_INPUT
	/* Discard the first 2 USB packages due to events delay */
	if ( (pChannel->nSamples < 2) && (common->wcmDevCls == &gWacomUSBDevice) && ds.device_type != PAD_ID )
	{
		DBG(11, ErrorF("discarded %dth USB data.\n", pChannel->nSamples));
		++pChannel->nSamples;
		return; /* discard */
	}
#endif
	fs = &pChannel->rawFilter;
	if (!fs->npoints && ds.proximity)
	{
		DBG(11, ErrorF("initialize Channel data.\n"));
		/* store channel device state for later use */
		for (i=MAX_SAMPLES - 1; i>=0; i--)
		{
			fs->x[i]= ds.x;
			fs->y[i]= ds.y;
			fs->tiltx[i] = ds.tiltx;
			fs->tilty[i] = ds.tilty;
		}
		++fs->npoints;
	} else  {
		/* Filter raw data, fix hardware defects, perform error correction */
		for (i=MAX_SAMPLES - 1; i>0; i--)
		{
			fs->x[i]= fs->x[i-1];
			fs->y[i]= fs->y[i-1];
		}
		fs->x[0] = ds.x;
		fs->y[0] = ds.y;
		if (HANDLE_TILT(common) && (ds.device_type == STYLUS_ID || ds.device_type == ERASER_ID))
		{
			for (i=MAX_SAMPLES - 1; i>0; i--)
			{
				fs->tiltx[i]= fs->tiltx[i-1];
				fs->tilty[i]= fs->tilty[i-1];
			}
			fs->tiltx[0] = ds.tiltx;
			fs->tilty[0] = ds.tilty;
		}
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
	if (pChannel->nSamples < MAX_SAMPLES) ++pChannel->nSamples;

	commonDispatchDevice(common,channel,pChannel);
	resetSampleCounter(pChannel);
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
	const WacomChannelPtr pChannel)
{
	int id, idx;
	int threshold;
	WacomDevicePtr priv;
	LocalDevicePtr pDev = NULL;
	WacomDeviceState* ds = &pChannel->valid.states[0];

	DBG(10, ErrorF("commonDispatchEvents\n"));

	if (!ds->device_type)
	{
		/* Tool may be on the tablet when X starts. 
		 * Figure out device type by device id
		 */
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
			default:
				ds->device_type = idtotype(ds->device_id);
		}
		ds->proximity = 1;
		if (ds->serial_num)
			for (idx=0; idx<common->wcmNumDevices; idx++)
			{
				priv = common->wcmDevices[idx]->private;
				if (ds->serial_num == priv->serial)
				{
					ds->device_type = DEVICE_ID(priv->flags);
					break;
				}
			}
	}

	/* Find the device the current events are meant for */
	for (idx=0; idx<common->wcmNumDevices; idx++)
	{
		priv = common->wcmDevices[idx]->private;
		id = DEVICE_ID(priv->flags);

		if (id == ds->device_type &&
			((!priv->serial) || (ds->serial_num == priv->serial)))
		{
			DBG(11, ErrorF("tool id=%d for %s\n",
					id, common->wcmDevices[idx]->name));
			pDev = common->wcmDevices[idx];
			break;
		}
	}

	DBG(11, ErrorF("commonDispatchEvents: %p \n",(void *)pDev));

	/* if a device matched criteria, handle filtering per device
	 * settings, and send event to XInput */
	if (pDev)
	{
		WacomDeviceState filtered = pChannel->valid.state;
		WacomDevicePtr priv = pDev->private;

		/* Device transformations come first */

		/* button 1 Threshold test */
		int button = 1;
		if ( IsStylus(priv) || IsEraser(priv))
		{
			if (filtered.pressure < common->wcmThreshold )
				filtered.buttons &= ~button;
			else
				filtered.buttons |= button;
			/* transform pressure */
			transPressureCurve(priv,&filtered);
		}

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

		/* force out-prox when heighth is beyond threshold. */
		if (!(priv->flags & ABSOLUTE_FLAG) && IsCursor(priv))
		{
			if (strstr(common->wcmModel->name, "Intuos"))
			{
				if (common->wcmCursorProxoutDist > filtered.distance)
					common->wcmCursorProxoutDist = filtered.distance;
			}
			else
			{
				if (common->wcmCursorProxoutDist == PROXOUT_DISTANCE)
					common->wcmCursorProxoutDist = filtered.distance;
				if (common->wcmCursorProxoutDist < filtered.distance)
					common->wcmCursorProxoutDist = filtered.distance;
			}
					
			threshold = common->wcmCursorProxoutDist;

			/* Force prox out when distance is outside of ProxoutHyst */
			if (priv->oldProximity)
			{
				if (strstr(common->wcmModel->name, "Intuos"))
					threshold += common->wcmCursorProxoutHyst;
				else
					threshold -= common->wcmCursorProxoutHyst;
			}

			DBG(10, ErrorF("Distance over the tablet: %d, ProxoutDist: %d hard prox: %d\n",
				       filtered.distance, common->wcmCursorProxoutDist, ds->proximity));
			if ((filtered.distance > threshold && 
				strstr(common->wcmModel->name, "Intuos")) || 
				(filtered.distance < threshold && 
				!strstr(common->wcmModel->name, "Intuos")))
			{
				filtered.proximity = 0;
			}
		}

		xf86WcmSendEvents(pDev, &filtered, channel);
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

int xf86WcmInitTablet(LocalDevicePtr local, WacomModelPtr model,
	const char* id, float version)
{
	WacomCommonPtr common =	((WacomDevicePtr)(local->private))->common;
	int temp;

	/* Initialize the tablet */
	model->Initialize(common,id,version);

	/* Get tablet resolution */
	if (model->GetResolution)
		model->GetResolution(local);

	/* Get tablet range */
	if (model->GetRanges && (model->GetRanges(local) != Success))
		return !Success;
	
	/* Rotation rotates the Max Y and Y */
	if (common->wcmRotate==ROTATE_CW || common->wcmRotate==ROTATE_CCW)
	{
		temp = common->wcmMaxX;
		common->wcmMaxX = common->wcmMaxY;
		common->wcmMaxY = temp;
	}

	/* Default threshold value if not set */
	if (common->wcmThreshold <= 0)
	{
		/* Threshold for counting pressure as a button */
		common->wcmThreshold = common->wcmMaxZ * 3 / 50;
		ErrorF("%s Wacom using pressure threshold of %d for button 1\n",
			XCONFIG_PROBED, common->wcmThreshold);
	}

	/* Reset tablet to known state */
	if (model->Reset && (model->Reset(local) != Success))
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Enable tilt mode, if requested and available */
	if ((common->wcmFlags & TILT_REQUEST_FLAG) && model->EnableTilt)
	{
		if (model->EnableTilt(local) != Success)
			return !Success;
	}

	/* Enable hardware suppress, if requested and available */
	if ((common->wcmSuppress != 0) && model->EnableSuppress)
	{
		if (model->EnableSuppress(local) != Success)
			return !Success;
	}

	/* change the serial speed, if requested */
	if (common->wcmLinkSpeed != 9600)
	{
		if (model->SetLinkSpeed)
		{
			if (model->SetLinkSpeed(local) != Success)
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
	if (model->Start && (model->Start(local) != Success))
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
