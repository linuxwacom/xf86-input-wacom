/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2007 by Ping Cheng, Wacom Technology. <pingc@wacom.com>		
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
#include "../include/Xwacom.h"

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
	const WacomChannelPtr pChannel, int suppress);
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

	DBG(6, priv->debugLevel, ErrorF("xf86WcmSetScreen "
		"v0=%d v1=%d\n", *value0, *value1));

	if (!(local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER))) return;

	if (!(priv->flags & ABSOLUTE_FLAG))
	{
		/* screenToSet lags by one event, but not that important */
		priv->currentScreen = miPointerCurrentScreen()->myNum;
		for (i = 0; i < priv->numScreen; i++)
			totalWidth += screenInfo.screens[i]->width;

		maxHeight = screenInfo.screens[priv->currentScreen]->height;
		priv->factorX = totalWidth / sizeX;
		priv->factorY = maxHeight / sizeY;
		DBG(10, priv->debugLevel, ErrorF(
			"xf86WcmSetScreen current=%d ToSet=%d\n", 
			priv->currentScreen, screenToSet));
		return;
	}

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

	if (priv->screen_no == -1)
	{
		for (i = 0; i < priv->numScreen; i++)
		{
			totalWidth += screenInfo.screens[i]->width;
			if (maxHeight < screenInfo.screens[i]->height)
				maxHeight = screenInfo.screens[i]->height;
		}
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
	else 
	{
		screenToSet = priv->screen_no;
		totalWidth = screenInfo.screens[screenToSet]->width;
		maxHeight = screenInfo.screens[screenToSet]->height;
	}
	priv->factorX = totalWidth/sizeX;
	priv->factorY = maxHeight/sizeY;
	x = (v0 - sizeX * leftPadding / totalWidth) * priv->factorX + 0.5;
	y = v1 * priv->factorY + 0.5;
		
	if (x >= screenInfo.screens[screenToSet]->width)
		x = screenInfo.screens[screenToSet]->width - 1;
	if (y >= screenInfo.screens[screenToSet]->height)
		y = screenInfo.screens[screenToSet]->height - 1;

	xf86XInputSetScreen(local, screenToSet, x, y);
	DBG(10, priv->debugLevel, ErrorF("xf86WcmSetScreen"
		" current=%d ToSet=%d\n", 
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
	DBG(6, priv->debugLevel, ErrorF("xf86WcmSendButtons "
		"buttons=%d for %s\n", buttons, local->name));

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
static int ODDKEYSYM [][2] = 
{
	{ XK_asciitilde, XK_grave },
	{ XK_exclam, XK_1 },
	{ XK_at, XK_2 },
	{ XK_numbersign, XK_3 },
	{ XK_dollar, XK_4 },
	{ XK_percent, XK_5 },
	{ XK_asciicircum, XK_6 },
	{ XK_ampersand, XK_7 },
	{ XK_asterisk, XK_8 },
	{ XK_parenleft, XK_9 },
	{ XK_parenright, XK_0 },
	{ XK_underscore, XK_minus },
	{ XK_plus, XK_equal },
	{ XK_braceleft, XK_bracketleft },
	{ XK_braceright, XK_bracketright },
	{ XK_colon, XK_semicolon },
	{ XK_quotedbl, XK_quoteright },
	{ XK_less, XK_comma },
	{ XK_greater, XK_period },
	{ XK_question, XK_slash },
	{ XK_bar, XK_backslash },
	{ 0, 0}
};

static void emitKeysym (DeviceIntPtr keydev, int keysym, int state)
{
	int i, j, alt_keysym = 0;

	/* Now that we have the keycode look for key index */
	KeySymsRec *ksr = &keydev->key->curKeySyms;

	for (i = ksr->minKeyCode; i <= ksr->maxKeyCode; i++)
		if (ksr->map [(i - ksr->minKeyCode) * ksr->mapWidth] == keysym)
			break;

	if (i > ksr->maxKeyCode)
	{
		if (isupper(keysym))
			alt_keysym = tolower(keysym);
		else
		{
			j = 0;
			while (ODDKEYSYM [j][0])
			{
				if (ODDKEYSYM [j][0] == keysym)
				{
					alt_keysym = ODDKEYSYM [j][1];
					break;
				}
				j++;
			}
		}
		if ( alt_keysym )
		{
			for (j = ksr->minKeyCode; j <= ksr->maxKeyCode; j++)
				if (ksr->map [(j - ksr->minKeyCode) * ksr->mapWidth] == XK_Shift_L)
				break;
			if (state)
				xf86PostKeyboardEvent (keydev, j, 1);
			for (i = ksr->minKeyCode; i <= ksr->maxKeyCode; i++)
				if (ksr->map [(i - ksr->minKeyCode) * ksr->mapWidth] == alt_keysym)
				break;
			xf86PostKeyboardEvent (keydev, i, state);
			if (state)
				xf86PostKeyboardEvent (keydev, j, 0);
		}
		else
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
static int wcm_modifier [ ] = 
{
	XK_Shift_L,
	XK_Control_L,
	XK_Meta_L,
	XK_Alt_L,
	XK_Super_L,
	XK_Hyper_L,
	0
};

static int WcmIsModifier(int keysym)
{
	int j = 0, match = 0;
	while (wcm_modifier[j])
		if (wcm_modifier[j++] == keysym)
		{
			match = 1;
			break;
		}
	return match;
}

static void sendAButton(LocalDevicePtr local, int button, int mask,
		int rx, int ry, int rz, int v3, int v4, int v5)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int is_absolute = priv->flags & ABSOLUTE_FLAG;
	int is_core = local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER);
	int i, button_idx, naxes = priv->naxes;

	if (IsPad(priv))
		button -= 8;

	if (button < 0 || button >= priv->nbuttons)
	{
		/* should never happen */
		ErrorF ("sendAButton: Invalid button index %d (number of defined buttons = %d)\n", button, priv->nbuttons);
		return; 
	}

	button_idx = button;
	button = priv->button[button];
	if (!button)
		return;

	/* Switch the device to core mode, if required */
	if (!is_core && (button & AC_CORE))
		xf86XInputSetSendCoreEvents (local, TRUE);

	DBG(4, priv->debugLevel, ErrorF(
		"sendAButton TPCButton(%s) button=%d state=%d " 
		"code=%08x, for %s coreEvent=%s \n", 
		common->wcmTPCButton ? "on" : "off", 
		button_idx, mask, button, 
		local->name, (button & AC_CORE) ? "yes" : "no"));

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
		{
			for (i=0; i<((button & AC_NUM_KEYS)>>20); i++)
			{
				/* button down to send modifier and key down then key up */
				if (mask)
				{
					emitKeysym (inputInfo.keyboard, priv->keys[button_idx][i], 1);
					if (!WcmIsModifier(priv->keys[button_idx][i]))
						emitKeysym (inputInfo.keyboard, priv->keys[button_idx][i], 0);
				}
				/* button up to send modifier up */
				else if (WcmIsModifier(priv->keys[button_idx][i]))
					emitKeysym (inputInfo.keyboard, priv->keys[button_idx][i], 0);
			}
		}
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
				rx,ry,rz,v3,v4,v5);
		}

		/* Left button down/up upon mask is 1/0 */
		xf86PostButtonEvent(local->dev, is_absolute, button_idx, 
			mask != 0,0,naxes,rx,ry,rz,v3,v4,v5);
		break;
	}

	/* Switch the device out of the core mode, if required
	 */
	if (!is_core && (button & AC_CORE))
		xf86XInputSetSendCoreEvents (local, FALSE);
}

/*****************************************************************************
 * sendWheelStripEvents --
 *   Send events defined for relative/absolute wheels or strips
 ****************************************************************************/

static void sendWheelStripEvents(LocalDevicePtr local, const WacomDeviceState* ds, int x, int y, int z, int v3, int v4, int v5)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int fakeButton = 0, i, value = 0, naxes = priv->naxes;
	unsigned  *keyP = 0;
	int is_absolute = priv->flags & ABSOLUTE_FLAG;
	int is_core = local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER);

	DBG(10, priv->debugLevel, ErrorF("sendWheelStripEvents for %s \n", local->name));

	/* emulate events for relative wheel */
	if ( ds->relwheel )
	{
		value = ds->relwheel;
		if ( ds->relwheel > 0 )
		{
			fakeButton = priv->relup;
			keyP = priv->rupk;
		}
		else
		{
			fakeButton = priv->reldn;
			keyP = priv->rdnk;
		}
	}

	/* emulate events for absolute wheel when needed */
	if ( ds->abswheel != priv->oldWheel )
	{
		value = priv->oldWheel - ds->abswheel;
		if ( value > 0 )
		{
			fakeButton = priv->wheelup;
			keyP = priv->wupk;
		}
		else
		{
			fakeButton = priv->wheeldn;
			keyP = priv->wdnk;
		}
	}

	/* emulate events for left strip */
	if ( ds->stripx != priv->oldStripX )
	{
		int temp = 0, n;
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
		if ( value > 0 )
		{
			fakeButton = priv->striplup;
			keyP = priv->slupk;
		}
		else if ( value < 0 )
		{
			fakeButton = priv->stripldn;
			keyP = priv->sldnk;
		}
	}

	/* emulate events for right strip */
	if ( ds->stripy != priv->oldStripY )
	{
		int temp = 0, n;
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
		if ( value > 0 )
		{
			fakeButton = priv->striprup;
			keyP = priv->srupk;
		}
		else if ( value < 0 )
		{
			fakeButton = priv->striprdn;
			keyP = priv->srdnk;
		}
	}

	DBG(10, priv->debugLevel, ErrorF("sendWheelStripEvents "
		"send fakeButton %x with value = %d \n", 
		fakeButton, value));

	if (!fakeButton) return;

	/* Switch the device to core mode, if required */
	if (!is_core && (fakeButton & AC_CORE))
		xf86XInputSetSendCoreEvents (local, TRUE);

	switch (fakeButton & AC_TYPE)
	{
	    case AC_BUTTON:
		/* pad may only have 2 buttons */
		local->dev->button->map [0] = fakeButton & AC_CODE;

		xf86PostButtonEvent(local->dev, is_absolute, 0,
			1,0,naxes,x,y,z,v3,v4,v5);
		xf86PostButtonEvent(local->dev, is_absolute, 0,
			0,0,naxes,x,y,z,v3,v4,v5);

	    break;

	    case AC_KEY:
		if (fakeButton & AC_CORE)
		{
			/* modifier and key down then key up events */
			for (i=0; i<((fakeButton & AC_NUM_KEYS)>>20); i++)
			{
				emitKeysym (inputInfo.keyboard, keyP[i], 1);
				if (!WcmIsModifier(keyP[i]))
					emitKeysym (inputInfo.keyboard, keyP[i], 0);
			}
			/* modifier up events */
			for (i=0; i<((fakeButton & AC_NUM_KEYS)>>20); i++)
				if (WcmIsModifier(keyP[i]))
					emitKeysym (inputInfo.keyboard, keyP[i], 0);
		}
		else
			ErrorF ("WARNING: [%s] without SendCoreEvents. Cannot emit key events!\n", local->name);
	    break;

	    default:
		ErrorF ("WARNING: [%s] unsupported event %x \n", local->name, fakeButton);
	}

	/* Switch the device out of the core mode, if required
	 */
	if (!is_core && (fakeButton & AC_CORE))
		xf86XInputSetSendCoreEvents (local, FALSE);
}

/*****************************************************************************
 * sendCommonEvents --
 *   Send events common between pad and stylus/cursor/eraser.
 ****************************************************************************/

static void sendCommonEvents(LocalDevicePtr local, const WacomDeviceState* ds, int x, int y, int z, int v3, int v4, int v5)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int buttons = ds->buttons;
	int naxes = priv->naxes;

	/* don't emit proximity events if device does not support proximity */
	if ((local->dev->proximity && !priv->oldProximity))
		xf86PostProximityEvent(local->dev, 1, 0, naxes,
			x, y, z, v3, v4, v5);

	if (priv->oldButtons != buttons)
		xf86WcmSendButtons(local,buttons,x,y,z,v3,v4,v5);

	/* emulate wheel/strip events when defined */
	if ( ds->relwheel || ds->abswheel || 
		( (ds->stripx - priv->oldStripX) && ds->stripx && priv->oldStripX) || 
			((ds->stripy - priv->oldStripY) && ds->stripy && priv->oldStripY) )
		sendWheelStripEvents(local, ds, x, y, z, v3, v4, v5);
}

/*****************************************************************************
 * xf86WcmSendEvents --
 *   Send events according to the device state.
 ****************************************************************************/

void xf86WcmSendEvents(LocalDevicePtr local, const WacomDeviceState* ds)
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
	int is_absolute = priv->flags & ABSOLUTE_FLAG;

	int v3, v4, v5;
	int no_jitter; 
	double relacc, param;

	if (priv->serial && serial != priv->serial)
	{
		DBG(10, priv->debugLevel, ErrorF("[%s] serial number"
			" is %d but your system configured %d", 
			local->name, serial, (int)priv->serial));
		return;
	}

	/* use tx and ty to report stripx and stripy */
	if (type == PAD_ID)
	{
		tx = ds->stripx;
		ty = ds->stripy;
	}

	DBG(7, priv->debugLevel, ErrorF("[%s] o_prox=%s x=%d y=%d z=%d "
		"b=%s b=%d tx=%d ty=%d wl=%d rot=%d th=%d\n",
		(type == STYLUS_ID) ? "stylus" :
			(type == CURSOR_ID) ? "cursor" : 
			(type == ERASER_ID) ? "eraser" : "pad",
		priv->oldProximity ? "true" : "false",
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

	DBG(6, priv->debugLevel, ErrorF("[%s] %s prox=%d\tx=%d"
		"\ty=%d\tz=%d\tv3=%d\tv4=%d\tv5=%d\tid=%d"
		"\tserial=%d\tbutton=%s\tbuttons=%d\n",
		local->name,
		is_absolute ? "abs" : "rel",
		is_proximity,
		x, y, z, v3, v4, v5, id, serial,
		is_button ? "true" : "false", buttons));

	if (x > priv->bottomX)
		x = priv->bottomX;
	if (x < priv->topX)
		x = priv->topX;
	if (y > priv->bottomY)
		y = priv->bottomY;
	if (y < priv->topY)
		y = priv->topY;
	priv->currentX = x;
	priv->currentY = y;

	/* update the old records */
	if(!priv->oldProximity)
	{
		priv->oldWheel = wheel;
		priv->oldX = priv->currentX;
		priv->oldY = priv->currentY;
		priv->oldZ = z;
		priv->oldTiltX = tx;
		priv->oldTiltY = ty;
		priv->oldStripX = ds->stripx;
		priv->oldStripY = ds->stripy;
		priv->oldRot = rot;
		priv->oldThrottle = throttle;
	}
	if (!is_absolute)
	{
		x -= priv->oldX;
		y -= priv->oldY;

		/* don't apply speed for fairly small increments */
		no_jitter = (priv->speed*3 > 4) ? priv->speed*3 : 4;
		relacc = (MAX_ACCEL-priv->accel)*(MAX_ACCEL-priv->accel);
		if (ABS(x) > no_jitter)
		{
			param = priv->speed;

			/* apply acceleration only when priv->speed > DEFAULT_SPEED */
			if (priv->speed > DEFAULT_SPEED )
			{
				param += priv->accel > 0 ? abs(x)/relacc : 0;
			}
			/* don't apply acceleration when too fast. */
			x *= param > 20.00 ? 20.00 : param;
		}
		if (ABS(y) > no_jitter)
		{
			param = priv->speed;
			/* apply acceleration only when priv->speed > DEFAULT_SPEED */
			if (priv->speed > DEFAULT_SPEED )
			{
				param += priv->accel > 0 ? abs(y)/relacc : 0;

			}
			/* don't apply acceleration when too fast. */
			y *= param > 20.00 ? 20.00 : param;
		}		
	}
	if (type != PAD_ID)
	{
		/* coordinates are ready we can send events */
		if (is_proximity)
		{
			/* for multiple monitor support, we need to set the proper 
			 * screen and modify the axes before posting events */
			if(!(priv->flags & BUTTONS_ONLY_FLAG))
			{
				xf86WcmSetScreen(local, &x, &y);
			}

			/* unify acceleration in both directions 
			 * for relative mode to draw a circle 
			 */
			if (!is_absolute)
				x *= priv->factorY / priv->factorX;

			sendCommonEvents(local, ds, x, y, z, v3, v4, v5);

			if(!(priv->flags & BUTTONS_ONLY_FLAG))
				xf86PostMotionEvent(local->dev, is_absolute,
					0, naxes, x, y, z, v3, v4, v5);
		}

		/* not in proximity */
		else
		{
			buttons = 0;

			/* reports button up when the device has been
			 * down and becomes out of proximity */
			if (priv->oldButtons)
				xf86WcmSendButtons(local,0,x,y,z,v3,v4,v5);
			if (priv->oldProximity && local->dev->proximity)
				xf86PostProximityEvent(local->dev,0,0,naxes,x,y,z,v3,v4,v5);
		} /* not in proximity */
	}
	else
	{
		if (v3 || v4 || v5 || buttons || ds->relwheel)
		{
			x = 0;
			y = 0;
			if ( v3 || v4 || v5 )
				xf86WcmSetScreen(local, &x, &y);
			sendCommonEvents(local, ds, x, y, z, v3, v4, v5);
			is_proximity = 1;
			if ( v3 || v4 || v5 )
			{
	 			xf86PostMotionEvent(local->dev, is_absolute,
					0, naxes, x, y, z, v3, v4, v5);
			}
		}
		else
		{
			if (priv->oldButtons)
				xf86WcmSendButtons(local, buttons, 
					x, y, z, v3, v4, v5);
			if (priv->oldProximity && local->dev->proximity)
 				xf86PostProximityEvent(local->dev, 0, 0, naxes, 
				x, y, z, v3, v4, v5);
			is_proximity = 0;
		}
	}
	priv->oldProximity = is_proximity;
	priv->old_device_id = id;
	priv->old_serial = serial;
	if (is_proximity)
	{
		priv->oldButtons = buttons;
		priv->oldWheel = wheel;
		priv->oldX = priv->currentX;
		priv->oldY = priv->currentY;
		priv->oldZ = z;
		priv->oldTiltX = tx;
		priv->oldTiltY = ty;
		priv->oldStripX = ds->stripx;
		priv->oldStripY = ds->stripy;
		priv->oldRot = rot;
		priv->oldThrottle = throttle;
	}
	else
	{
		priv->oldButtons = 0;
		priv->oldWheel = 0;
		priv->oldX = 0;
		priv->oldY = 0;
		priv->oldZ = 0;
		priv->oldTiltX = 0;
		priv->oldTiltY = 0;
		priv->oldStripX = 0;
		priv->oldStripY = 0;
		priv->oldRot = 0;
		priv->oldThrottle = 0;
	}
}

/*****************************************************************************
 * xf86WcmSuppress --
 *  Determine whether device state has changed enough - return 0
 *  if not.
 ****************************************************************************/

static int xf86WcmSuppress(WacomCommonPtr common, const WacomDeviceState* dsOrig, 
	WacomDeviceState* dsNew)
{
	int suppress = common->wcmSuppress;
	/* NOTE: Suppression value of zero disables suppression. */
	int returnV = 0;

	if (dsOrig->buttons != dsNew->buttons) returnV = 1;
	if (dsOrig->proximity != dsNew->proximity) returnV = 1;
	if (dsOrig->stripx != dsNew->stripx) returnV = 1;
	if (dsOrig->stripy != dsNew->stripy) returnV = 1;
	if (ABS(dsOrig->tiltx - dsNew->tiltx) > suppress) returnV = 1;
	if (ABS(dsOrig->tilty - dsNew->tilty) > suppress) returnV = 1;
	if (ABS(dsOrig->pressure - dsNew->pressure) > suppress) returnV = 1;
	if (ABS(dsOrig->throttle - dsNew->throttle) > suppress) returnV = 1;
	if (ABS(dsOrig->rotation - dsNew->rotation) > suppress &&
		(1800 - ABS(dsOrig->rotation - dsNew->rotation)) >  suppress) returnV = 1;

	/* look for change in absolute wheel
	 * position or any relative wheel movement */
	if ((ABS(dsOrig->abswheel - dsNew->abswheel) > suppress) ||
		(dsNew->relwheel != 0)) returnV = 1;

	/* need to check if cursor moves or not */
	if ((ABS(dsOrig->x - dsNew->x) > suppress) || 
			(ABS(dsOrig->y - dsNew->y) > suppress)) 
		returnV = 2;
	else if (returnV == 1) /* don't move the cursor */
	{
		dsNew->x = dsOrig->x;
		dsNew->y = dsOrig->y;
	}

	DBG(10, common->debugLevel, ErrorF("xf86WcmSuppress at level = %d"
		" return value = %d\n", suppress, returnV));
	return returnV;
}

/*****************************************************************************
 * xf86WcmOpen --
 ****************************************************************************/

Bool xf86WcmOpen(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceClass** ppDevCls;

	DBG(1, priv->debugLevel, ErrorF("opening %s\n", common->wcmDevice));

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
	int i, suppress = 0;

	pChannel = common->wcmChannel + channel;
	pLast = &pChannel->valid.state;

	DBG(10, common->debugLevel, ErrorF("xf86WcmEvent at channel = %d\n", channel));

	/* tool on the tablet when driver starts */
	if (!miPointerCurrentScreen())
	{
		DBG(1, common->debugLevel, ErrorF("xf86WcmEvent: "
			"Wacom driver can not get Current Screen ID\n"));
		DBG(1, common->debugLevel, ErrorF(
			"Please remove Wacom tool from the tablet.\n"));
		return;
	}

	/* sanity check the channel */
	if (channel >= MAX_CHANNELS)
		return;
	
	/* we must copy the state because certain types of filtering
	 * will need to change the values (ie. for error correction) */
	ds = *pState;

	/* timestamp the state for velocity and acceleration analysis */
	ds.sample = (int)GetTimeInMillis();
	DBG(10, common->debugLevel, ErrorF("xf86WcmEvent: "
		"c=%d i=%d t=%d s=%u x=%d y=%d b=%d "
		"p=%d rz=%d tx=%d ty=%d aw=%d rw=%d "
		"t=%d df=%d px=%d st=%d\n",
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
		DBG(11, common->debugLevel, 
			ErrorF("discarded %dth USB data.\n", 
			pChannel->nSamples));
		++pChannel->nSamples;
		return; /* discard */
	}
#endif
	fs = &pChannel->rawFilter;
	if (!fs->npoints && ds.proximity)
	{
		DBG(11, common->debugLevel, ErrorF("initialize Channel data.\n"));
		/* store channel device state for later use */
		for (i=common->wcmRawSample - 1; i>=0; i--)
		{
			fs->x[i]= ds.x;
			fs->y[i]= ds.y;
			fs->tiltx[i] = ds.tiltx;
			fs->tilty[i] = ds.tilty;
		}
		++fs->npoints;
	} else  {
		/* Filter raw data, fix hardware defects, perform error correction */
		for (i=common->wcmRawSample - 1; i>0; i--)
		{
			fs->x[i]= fs->x[i-1];
			fs->y[i]= fs->y[i-1];
		}
		fs->x[0] = ds.x;
		fs->y[0] = ds.y;
		if (HANDLE_TILT(common) && (ds.device_type == STYLUS_ID || ds.device_type == ERASER_ID))
		{
			for (i=common->wcmRawSample - 1; i>0; i--)
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
				DBG(10, common->debugLevel, ErrorF(
					"Raw filtering discarded data.\n"));
				resetSampleCounter(pChannel);
				return; /* discard */
			}
		}

		/* Discard unwanted data */
		suppress = xf86WcmSuppress(common, pLast, &ds);
		if (!suppress)
		{
			resetSampleCounter(pChannel);
			return;
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
		sizeof(WacomDeviceState) * (common->wcmRawSample - 1));
	pChannel->valid.state = ds; /*save last raw sample */
	if (pChannel->nSamples < common->wcmRawSample) ++pChannel->nSamples;

	commonDispatchDevice(common,channel,pChannel, suppress);
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
	const WacomChannelPtr pChannel, int suppress)
{
	LocalDevicePtr pDev = NULL;
	WacomToolPtr tool = NULL;
	WacomToolPtr tooldef = NULL;
	WacomDeviceState* ds = &pChannel->valid.states[0];
	WacomDevicePtr priv = NULL;

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
			for (tool = common->wcmTool; tool; tool = tool->next)
				if (ds->serial_num == tool->serial)
				{
					ds->device_type = tool->typeid;
					break;
				}
	}

	DBG(10, common->debugLevel, ErrorF("commonDispatchDevice device type = %d\n", ds->device_type));
	/* Find the device the current events are meant for */
	/* 1: Find the tool (the one with correct serial or in second
	 * hand, the one with serial set to 0 if no match with the
	 * specified serial exists) that is used for this event */
	for (tool = common->wcmTool; tool; tool = tool->next)
		if (tool->typeid == ds->device_type)
		{
			if (tool->serial == ds->serial_num)
				break;
			else if (!tool->serial)
				tooldef = tool;
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
			!xf86WcmPointInArea(tool->current, ds->x, ds->y))
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
				if (xf86WcmPointInArea(area, ds->x, ds->y))
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
				LocalDevicePtr oDev = outprox->device;
				WacomDeviceState out = { 0 };
				DBG(2, common->debugLevel, ErrorF("Soft prox-out for %s\n",
					outprox->device->name));
				xf86WcmSendEvents(oDev, &out);
			}
			else
				tool->current = outprox;
		}

		/* If there was one already in use or we found one */
		if(tool->current)
		{
			pDev = tool->current->device;
			DBG(11, common->debugLevel, ErrorF("tool id=%d for %s\n",
				       ds->device_type, pDev->name));
		}
	}
	/* X: InputDevice selection done! */

	/* if a device matched criteria, handle filtering per device
	 * settings, and send event to XInput */
	if (pDev)
	{
		WacomDeviceState filtered = pChannel->valid.state;

		/* Device transformations come first */
		/* button 1 Threshold test */
		int button = 1;
		priv = pDev->private;
		if ( IsStylus(priv) || IsEraser(priv))
		{
			if (filtered.pressure < common->wcmThreshold )
				filtered.buttons &= ~button;
			else
				filtered.buttons |= button;
			/* transform pressure */
			transPressureCurve(priv,&filtered);
		}

		if (!(priv->flags & ABSOLUTE_FLAG) && !priv->hardProx)
		{
			/* initial current max distance */
			if (strstr(common->wcmModel->name, "Intuos"))
				common->wcmMaxCursorDist = 256;
			else
				common->wcmMaxCursorDist = 0;
		}
		/* Store current hard prox for next use */
		priv->hardProx = ds->proximity;		

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
			DBG(6, priv->debugLevel, ErrorF("LIMIT REACHED: "
				"s=%d l=%d n=%d v=%d "
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

		if (!(priv->flags & ABSOLUTE_FLAG) && (suppress == 2))
		{
			/* To improve the accuracy of relative x/y,
			 * don't send event when there is no movement.
			 */
			double deltx = filtered.x - priv->oldX;
			double delty = filtered.y - priv->oldY;
			deltx *= priv->factorY*priv->speed;
			delty *= priv->factorY*priv->speed;

			if (ABS(deltx)<1 && ABS(delty)<1) 
			{
				DBG(10, common->debugLevel, ErrorF(
					"Ignore non-movement relative data \n"));
				return;
			}
			else
			{
				int temp = deltx;
				deltx = (double)temp/(priv->factorY*priv->speed);
				temp = delty;
				delty = (double)temp/(priv->factorY*priv->speed);
				filtered.x = deltx + priv->oldX;
				filtered.y = delty + priv->oldY;
			}
		}

		/* force out-prox when distance is outside wcmCursorProxoutDist. */
		if (!(priv->flags & ABSOLUTE_FLAG))
		{
			if (strstr(common->wcmModel->name, "Intuos"))
			{
				if (common->wcmMaxCursorDist > filtered.distance)
					common->wcmMaxCursorDist = filtered.distance;
			}
			else
			{
				if (common->wcmMaxCursorDist < filtered.distance)
					common->wcmMaxCursorDist = filtered.distance;
			}
			DBG(10, common->debugLevel, ErrorF("Distance over"
				" the tablet: %d, ProxoutDist: %d current"
				" min/max %d hard prox: %d\n",
				filtered.distance, 
				common->wcmCursorProxoutDist, 
				common->wcmMaxCursorDist, 
				ds->proximity));

			if (priv->oldProximity)
			{
				if (abs(filtered.distance - common->wcmMaxCursorDist) > common->wcmCursorProxoutDist)
					filtered.proximity = 0;
			}
			/* once it is out. Don't let it in until a hard in */
			/* or it gets inside wcmCursorProxoutDist */
			else
			{
				if (abs(filtered.distance - common->wcmMaxCursorDist) > common->wcmCursorProxoutDist && ds->proximity)
					return;
				if (!ds->proximity)
					return;	
			}
		}
		xf86WcmSendEvents(pDev, &filtered);
		/* If out-prox, reset the current area pointer */
		if (!filtered.proximity)
			tool->current = NULL;
	}

	/* otherwise, if no device matched... */
	else
	{
		DBG(11, common->debugLevel, ErrorF("no device matches with"
				" id=%d, serial=%d\n",
				ds->device_type, ds->serial_num));
	}
}

/*****************************************************************************
 * xf86WcmInitTablet -- common initialization for all tablets
 ****************************************************************************/

int xf86WcmInitTablet(LocalDevicePtr local, WacomModelPtr model,
	const char* id, float version)
{
	WacomCommonPtr common =	((WacomDevicePtr)(local->private))->common;
	WacomToolPtr toollist = common->wcmTool;
	WacomToolAreaPtr arealist;
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

	for (; toollist; toollist=toollist->next)
	{
		arealist = toollist->arealist;
		for (; arealist; arealist=arealist->next)
		{
			if (!arealist->bottomX) 
				arealist->bottomX = common->wcmMaxX;
			if (!arealist->bottomY)
				arealist->bottomY = common->wcmMaxY;
		}
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
	if (model->EnableSuppress)
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
			"resX=%d resY=%d  tilt=%s\n",
			XCONFIG_PROBED,
			model->name, common->wcmLinkSpeed,
			common->wcmMaxX, common->wcmMaxY, common->wcmMaxZ,
			common->wcmResolX, common->wcmResolY,
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
