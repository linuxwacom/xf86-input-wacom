/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2009 by Ping Cheng, Wacom Technology. <pingc@wacom.com>		
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#ifdef WCM_ENABLE_LINUXINPUT
	&gWacomUSBDevice,
#endif
	&gWacomISDV4Device,
	&gWacomSerialDevice,
	NULL
};

extern int xf86WcmDevSwitchModeCall(LocalDevicePtr local, int mode);
extern void xf86WcmChangeScreen(LocalDevicePtr local, int value);
extern void xf86WcmTilt2R(WacomDeviceStatePtr ds);
extern void xf86WcmFingerTapToClick(WacomCommonPtr common);
extern void xf86WcmSetScreen(LocalDevicePtr local, int v0, int v1);

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
	if ( common->wcmTPCButton && IsStylus(priv) )
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

#ifdef WCM_KEY_SENDING_SUPPORT
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

void emitKeysym (DeviceIntPtr keydev, int keysym, int state)
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
			if (!state)
				xf86PostKeyboardEvent (keydev, j, 0);
		}
		else
			xf86Msg (X_WARNING, "Couldn't find key with code %08x on keyboard device %s\n",
					keysym, keydev->name);
		return;
	}
	xf86PostKeyboardEvent (keydev, i, state);
}

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
#endif /*WCM_KEY_SENDING_SUPPORT*/

static void sendKeystroke(LocalDevicePtr local, int button, unsigned *keyP, int kPress)
{
#ifndef WCM_KEY_SENDING_SUPPORT
	ErrorF ("Error: [wacom] your X server doesn't support key events!\n");
#else /* WCM_KEY_SENDING_SUPPORT */
	if (button & AC_CORE)
	{
		int i = 0;

		for (i=0; i<((button & AC_NUM_KEYS)>>20); i++)
		{
			/* modifier and key down then key up events */
			if(kPress)
			{
				emitKeysym (local->dev, keyP[i], 1);
				if (!WcmIsModifier(keyP[i]))
					emitKeysym (local->dev, keyP[i], 0);
			}
			/* modifier up events */
			else if (WcmIsModifier(keyP[i]))
				emitKeysym (local->dev, keyP[i], 0);
		}
	}
	else
		ErrorF ("WARNING: [%s] without SendCoreEvents. Cannot emit key events!\n", local->name);
#endif /* WCM_KEY_SENDING_SUPPORT */
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
#if WCM_XINPUTABI_MAJOR == 0
	int is_core = local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER);
#endif
	int naxes = priv->naxes;

	if (!priv->button[button])  /* ignore this button event */
		return;

#if WCM_XINPUTABI_MAJOR == 0
	/* Switch the device to core mode, if required */
	if (!is_core && (button & AC_CORE))
		xf86XInputSetSendCoreEvents (local, TRUE);
#endif

	DBG(4, priv->debugLevel, ErrorF(
		"sendAButton TPCButton(%s) button=%d state=%d " 
		"code=%08x, for %s coreEvent=%s \n", 
		common->wcmTPCButton ? "on" : "off", 
		button, mask, priv->button[button], 
		local->name, (priv->button[button] & AC_CORE) ? "yes" : "no"));

	switch (priv->button[button] & AC_TYPE)
	{
	case AC_BUTTON:
		xf86PostButtonEvent(local->dev, is_absolute, priv->button[button] & AC_CODE,
			mask != 0,0,naxes,rx,ry,rz,v3,v4,v5);
		break;

	case AC_KEY:
		sendKeystroke(local, priv->button[button], priv->keys[button], mask);
		break;

	case AC_MODETOGGLE:
		if (mask)
		{
			int mode = Absolute;
			if (is_absolute)
				mode = Relative;
			xf86WcmDevSwitchModeCall(local, mode);
		}
		break;

	case AC_DISPLAYTOGGLE:
		if (mask && priv->numScreen > 1)
		{
			/* toggle display (individual screens plus the whole desktop)
			 * for all tools except pad */
			if (IsPad(priv))
			{
				WacomDevicePtr tmppriv;
				for (tmppriv = common->wcmDevices; tmppriv; tmppriv = tmppriv->next)
				{
					if (!IsPad(tmppriv))
					{
						int screen = tmppriv->screen_no;
						if (++screen >= tmppriv->numScreen)
							screen = -1;
						xf86WcmChangeScreen(tmppriv->local, screen);
					}
				}
			}
			else
			{
				int screen = priv->screen_no;
				if (++screen >= priv->numScreen)
					screen = -1;
				xf86WcmChangeScreen(local, screen);
			}
		}
		break;

	case AC_SCREENTOGGLE:
		if (mask && priv->numScreen > 1)
		{
			/* toggle screens for all tools except pad */
			if (IsPad(priv))
			{
				WacomDevicePtr tmppriv;
				for (tmppriv = common->wcmDevices; tmppriv; tmppriv = tmppriv->next)
				{
					if (!IsPad(tmppriv))
					{
						int screen = tmppriv->screen_no;
						if (++screen >= tmppriv->numScreen)
							screen = 0;
						xf86WcmChangeScreen(tmppriv->local, screen);
					}
				}
			}
			else /* toggle screens only for the selected tool */
			{
				int screen = priv->screen_no;
				if (++screen >= priv->numScreen)
					screen = 0;
				xf86WcmChangeScreen(local, screen);
			}
		}
		break;

	case AC_DBLCLICK:
		if (mask)
		{
			/* Left button down */
			xf86PostButtonEvent(local->dev, is_absolute,
				1,1,0,naxes, rx,ry,rz,v3,v4,v5);
			/* Left button up */
			xf86PostButtonEvent(local->dev, is_absolute,
				1,0,0,naxes,rx,ry,rz,v3,v4,v5);
		}

		/* Left button down/up upon mask is 1/0 */
		xf86PostButtonEvent(local->dev, is_absolute, 1, 
			mask != 0,0,naxes,rx,ry,rz,v3,v4,v5);
		break;
	}

#if WCM_XINPUTABI_MAJOR == 0
	/* Switch the device out of the core mode, if required */
	if (!is_core && (button & AC_CORE))
		xf86XInputSetSendCoreEvents (local, FALSE);
#endif
}

/*****************************************************************************
 * sendWheelStripEvents --
 *   Send events defined for relative/absolute wheels or strips
 ****************************************************************************/

static void sendWheelStripEvents(LocalDevicePtr local, const WacomDeviceState* ds, 
		int x, int y, int z, int v3, int v4, int v5)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int fakeButton = 0, i, value = 0, naxes = priv->naxes;
	unsigned  *keyP = 0;
	int is_absolute = priv->flags & ABSOLUTE_FLAG;
#if WCM_XINPUTABI_MAJOR == 0
	int is_core = local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER);
#endif
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
		if ((strstr(common->wcmModel->name, "Bamboo") ||
		strstr(common->wcmModel->name, "Intuos4"))
			&& IsPad(priv))
		{
			/* deal with MAX_FINGER_WHEEL to 0 and 0 to MAX_FINGER_WHEEL switching */
			if (abs(priv->oldWheel - ds->abswheel) > (MAX_FINGER_WHEEL/2))
			{
				if (priv->oldWheel > ds->abswheel)
					value -= MAX_FINGER_WHEEL;
				else
					value += MAX_FINGER_WHEEL;
			}
		}
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

	if (!fakeButton) return;

	DBG(10, priv->debugLevel, ErrorF("sendWheelStripEvents "
		"send fakeButton %x with value = %d \n", 
		fakeButton, value));

#if WCM_XINPUTABI_MAJOR == 0
	/* Switch the device to core mode, if required */
	if (!is_core && (fakeButton & AC_CORE))
		xf86XInputSetSendCoreEvents (local, TRUE);
#endif
	switch (fakeButton & AC_TYPE)
	{
	    case AC_BUTTON:
		/* send both button on/off in the same event for pad */	
		xf86PostButtonEvent(local->dev, is_absolute, fakeButton & AC_CODE,
			1,0,naxes,x,y,z,v3,v4,v5);

		xf86PostButtonEvent(local->dev, is_absolute, fakeButton & AC_CODE,
			0,0,naxes,x,y,z,v3,v4,v5);
	    break;

	    case AC_KEY:
		sendKeystroke(local, fakeButton, keyP, 1);
		sendKeystroke(local, fakeButton, keyP, 0);
	    break;

	    default:
		ErrorF ("WARNING: [%s] unsupported event %x \n", local->name, fakeButton);
	}

#if WCM_XINPUTABI_MAJOR == 0
	/* Switch the device out of the core mode, if required
	 */
	if (!is_core && (fakeButton & AC_CORE))
		xf86XInputSetSendCoreEvents (local, FALSE);
#endif
}

/*****************************************************************************
 * sendCommonEvents --
 *   Send events common between pad and stylus/cursor/eraser.
 ****************************************************************************/

static void sendCommonEvents(LocalDevicePtr local, const WacomDeviceState* ds, int x, int y, int z, int v3, int v4, int v5)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int buttons = ds->buttons;

	/* send button events when state changed or first time in prox and button unpresses */
	if (priv->oldButtons != buttons || (!priv->oldProximity && !buttons))
		xf86WcmSendButtons(local,buttons,x,y,z,v3,v4,v5);

	/* emulate wheel/strip events when defined */
	if ( ds->relwheel || ds->abswheel || 
		( (ds->stripx - priv->oldStripX) && ds->stripx && priv->oldStripX) || 
			((ds->stripy - priv->oldStripY) && ds->stripy && priv->oldStripY) )
		sendWheelStripEvents(local, ds, x, y, z, v3, v4, v5);
}

/* rotate the raw coordinates */
void xf86WcmRotateCoordinates(LocalDevicePtr local, int x, int y)	
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int tmp_coord;

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
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int naxes = priv->naxes;
	int is_absolute = priv->flags & ABSOLUTE_FLAG;
	int v3, v4, v5;
	int no_jitter; 
	double relacc, param;

	if (priv->serial && serial != priv->serial)
	{
		DBG(10, priv->debugLevel, ErrorF("[%s] serial number"
			" is %u but your system configured %u", 
			local->name, serial, (int)priv->serial));
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

	DBG(7, priv->debugLevel, ErrorF("[%s] o_prox=%s x=%d y=%d z=%d "
		"b=%s b=%d tx=%d ty=%d wl=%d rot=%d th=%d\n",
		(type == STYLUS_ID) ? "stylus" :
			(type == CURSOR_ID) ? "cursor" : 
			(type == ERASER_ID) ? "eraser" :
			(type == TOUCH_ID) ? "touch" : "pad",
		priv->oldProximity ? "true" : "false",
		x, y, z, is_button ? "true" : "false", buttons,
		tx, ty, wheel, rot, throttle));

	xf86WcmRotateCoordinates(local, x, y);

	if (IsCursor(priv)) 
	{
		v3 = rot;
		v4 = throttle;
	}
	else  /* Intuos styli have tilt */
	{
		v3 = tx;
		v4 = ty;
	}
	v5 = wheel;

	DBG(6, priv->debugLevel, ErrorF("[%s] %s prox=%d\tx=%d"
		"\ty=%d\tz=%d\tv3=%d\tv4=%d\tv5=%d\tid=%d"
		"\tserial=%u\tbutton=%s\tbuttons=%d\n",
		local->name,
		is_absolute ? "abs" : "rel",
		is_proximity,
		x, y, z, v3, v4, v5, id, serial,
		is_button ? "true" : "false", buttons));

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
		priv->oldCapacity = ds->capacity;
		priv->oldStripX = ds->stripx;
		priv->oldStripY = ds->stripy;
		priv->oldRot = rot;
		priv->oldThrottle = throttle;
		priv->oldButtons = 0;
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
				xf86WcmSetScreen(local, x, y);
			}

			/* unify acceleration in both directions 
			 * for relative mode to draw a circle 
			 */
			if (!is_absolute)
				x *= priv->factorY / priv->factorX;

#ifdef WCM_XORG_TABLET_SCALING
			/* Ugly hack for Xorg 7.3, which doesn't call xf86WcmDevConvert
			 * for coordinate conversion at the moment */
			/* The +-0.4 is to increase the sensitivity in relative mode.
			 * Must be sensitive to which way the tool is moved or one way
			 * will get a severe penalty for small movements. */
 			if(is_absolute) {
				x -= priv->topX;
				y -= priv->topY;
				if (priv->currentScreen == 1 && priv->twinview > TV_XINERAMA)
				{
					x -= priv->tvoffsetX;
					y -= priv->tvoffsetY;
				}
			}
			x = (int)((double)x * priv->factorX + (x>=0?0.4:-0.4));
			y = (int)((double)y * priv->factorY + (y>=0?0.4:-0.4));

			if ((priv->flags & ABSOLUTE_FLAG) && (priv->twinview <= TV_XINERAMA) && (priv->screen_no == -1))
			{
				x -= priv->screenTopX[priv->currentScreen];
				y -= priv->screenTopY[priv->currentScreen];
			}

			if (priv->screen_no != -1)
			{
				if (priv->twinview <= TV_XINERAMA)
				{
					if (x > priv->screenBottomX[priv->currentScreen] - priv->screenTopX[priv->currentScreen])
						x = priv->screenBottomX[priv->currentScreen] - priv->screenTopX[priv->currentScreen];
					if (y > priv->screenBottomY[priv->currentScreen] - priv->screenTopY[priv->currentScreen])
						y = priv->screenBottomY[priv->currentScreen] - priv->screenTopY[priv->currentScreen];
				}
				if (x < 0) x = 0;
				if (y < 0) y = 0;
	
			}
			if (priv->twinview > TV_XINERAMA)
			{
				x += priv->screenTopX[priv->currentScreen];
				y += priv->screenTopY[priv->currentScreen];
			}
			priv->currentSX = x;
			priv->currentSY = y;
			DBG(6, priv->debugLevel, ErrorF("WCM_XORG_TABLET_SCALING Convert"
				" v0=%d v1=%d to x=%d y=%d\n", ds->x, ds->y, x, y));
#endif
			/* don't emit proximity events if device does not support proximity */
			if ((local->dev->proximity && !priv->oldProximity))
				xf86PostProximityEvent(local->dev, 1, 0, naxes, x, y, z, v3, v4, v5);

			/* Move the cursor to where it should be before sending button events */
			if(!(priv->flags & BUTTONS_ONLY_FLAG))
				xf86PostMotionEvent(local->dev, is_absolute,
					0, naxes, x, y, z, v3, v4, v5);

			sendCommonEvents(local, ds, x, y, z, v3, v4, v5);
		}

		/* not in proximity */
		else
		{
			buttons = 0;

#ifdef WCM_XORG_TABLET_SCALING
			/* Ugly hack for Xorg 7.3, which doesn't call xf86WcmDevConvert
			 * for coordinate conversion at the moment */
			x = priv->currentSX;
			y = priv->currentSY;
#endif

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
				xf86WcmSetScreen(local, x, y);

			/* don't emit proximity events if device does not support proximity */
			if ((local->dev->proximity && !priv->oldProximity))
			xf86PostProximityEvent(local->dev, 1, 0, naxes, x, y, z, v3, v4, v5);

			sendCommonEvents(local, ds, x, y, z, v3, v4, v5);
			is_proximity = 1;
			/* xf86PostMotionEvent is only needed to post the valuators
			 * It should NOT move the cursor.
			 */
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
		priv->oldCapacity = ds->capacity;
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
	char id[BUFFER_SIZE];
	float version;

	DBG(1, priv->debugLevel, ErrorF("opening %s\n", common->wcmDevice));

	local->fd = xf86OpenSerial(local->options);
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
	if(common->wcmDevCls->Init(local, id, &version) != Success ||
		xf86WcmInitTablet(local, id, version) != Success)
	{
		xf86CloseSerial(local->fd);
		local->fd = -1;
		return !Success;
	}
	return Success;
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

	/* sanity check the channel */
	if (channel >= MAX_CHANNELS)
		return;
	
	/* we must copy the state because certain types of filtering
	 * will need to change the values (ie. for error correction) */
	ds = *pState;

	DBG(10, common->debugLevel, ErrorF("xf86WcmEvent: "
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
		pChannel->nSamples));

#ifdef WCM_ENABLE_LINUXINPUT
	/* Discard the first 2 USB packages due to events delay */
	if ( (pChannel->nSamples < 2) && (common->wcmDevCls == &gWacomUSBDevice) && 
		(ds.device_type != PAD_ID) && (ds.device_type != TOUCH_ID))
	{
		DBG(11, common->debugLevel, 
			ErrorF("discarded %dth USB data.\n", 
			pChannel->nSamples));
		++pChannel->nSamples;
		return; /* discard */
	}
#endif

	if (strstr(common->wcmModel->name, "Intuos4"))
	{
		/* convert Intuos4 mouse tilt to rotation */
		xf86WcmTilt2R(&ds);
	}

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
		if (HANDLE_TILT(common) && ((ds.device_type == STYLUS_ID) || (ds.device_type == ERASER_ID)))
		{
			for (i=common->wcmRawSample - 1; i>0; i--)
			{
				fs->tiltx[i]= fs->tiltx[i-1];
				fs->tilty[i]= fs->tilty[i-1];
			}
			fs->tiltx[0] = ds.tiltx;
			fs->tilty[0] = ds.tilty;
		}

		if (RAW_FILTERING(common) && common->wcmModel->FilterRaw && (ds.device_type != PAD_ID))
		{
			if (common->wcmModel->FilterRaw(common,pChannel,&ds))
			{
				DBG(10, common->debugLevel, ErrorF(
					"Raw filtering discarded data.\n"));
				goto ret; /* discard */
			}
		}
		/* Discard unwanted data */
		suppress = xf86WcmSuppress(common, pLast, &ds);
		if (!suppress)
			goto ret;
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
	pChannel->valid.state = ds;   /* save last raw sample */

	if (pChannel->nSamples < common->wcmRawSample) ++pChannel->nSamples;

	/* process second finger data if exists */
	if (ds.device_type == TOUCH_ID)
	{
		WacomChannelPtr pOtherChannel;
		WacomDeviceState dsOther;

		/* exit gesture mode when both fingers are out */
		if (channel)
			pOtherChannel = common->wcmChannel;
		else
			pOtherChannel = common->wcmChannel + 1;
		dsOther = pOtherChannel->valid.state;

		/* This is the only place to reset gesture mode
		 * once a gesture mode is entered */
		if (!ds.proximity && !dsOther.proximity && common->wcmGestureMode)
		{
			common->wcmGestureMode = 0;

			/* send a touch out-prox event here
			 * in case the FF was out before the SF */
			channel = 0;
		}
		else
		{
			/* don't move the cursor if in gesture mode
			 * wait for second finger data to process gestures */
			if (!channel && common->wcmTouch &&
				common->wcmGesture && common->wcmGestureMode)
				goto ret;

			/* process gesture when both touch and geature are enabled */
			if (channel && common->wcmTouch && common->wcmGesture)
			{
				xf86WcmFingerTapToClick(common);
				goto ret;
			}
		}
	}
	/* everything else falls here */
	commonDispatchDevice(common,channel,pChannel, suppress);
ret:
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

	if (!ds->device_type && ds->proximity)
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
			case TOUCH_DEVICE_ID:
				ds->device_type = TOUCH_ID;
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
	DBG(10, common->debugLevel, ErrorF("commonDispatchDevice device type = %d\n", ds->device_type));
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
				xf86WcmSoftOutEvent(outprox->device);
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

	/* Tool on the tablet when driver starts. This sometime causes
	 * access errors to the device */
#if WCM_XINPUTABI_MAJOR == 0
	if (!miPointerCurrentScreen())
#else
	if (pDev && !miPointerGetScreen(pDev->dev))
#endif
	{
		ErrorF("commonDispatchDevice: Wacom driver can not get Current Screen ID\n");
		ErrorF("Please remove Wacom tool from the tablet and bring it back again.\n");
		return;
	}

	/* if a device matched criteria, handle filtering per device
	 * settings, and send event to XInput */
	if (pDev)
	{
		WacomDeviceState filtered = pChannel->valid.state;

		/* Device transformations come first */
		/* button 1 Threshold test */
		int button = 1;
		priv = pDev->private;

#ifdef WCM_ENABLE_LINUXINPUT
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
			LocalDevicePtr localDevices = xf86FirstLocalDevice();
			WacomCommonPtr tempcommon = NULL;
			WacomDevicePtr temppriv = NULL;

			/* Lookup to see if associated touch was enabled */
			for (; localDevices != NULL; localDevices = localDevices->next)
			{
				/* we need to go through all enabled Wacom devices
				 * since USB pen and touch don't share the same logical port
				 */
				if (strstr(localDevices->drv->driverName, "wacom"))
				{
					temppriv = (WacomDevicePtr) localDevices->private;
					tempcommon = temppriv->common;

					if (((tempcommon->tablet_id == common->tablet_id) || /* same model */
						strstr(common->wcmModel->name, "ISDV4") || /* a serial Tablet PC */
						strstr(common->wcmModel->name, "TabletPC")) && /* an USB TabletPC */
						IsTouch(temppriv) && temppriv->oldProximity)
					{
						/* Send soft prox-out for touch first */
						xf86WcmSoftOutEvent(localDevices);
					}
				}
			}
		}
#endif /* WCM_ENABLE_LINUXINPUT */

		if (IsStylus(priv) || IsEraser(priv))
		{
			/* set button1 (left click) on/off */
			if (filtered.pressure >= common->wcmThreshold)
				filtered.buttons |= button;
			else
			{
				/* threshold tolerance */
				int tol = common->wcmMaxZ / 250;
				if (strstr(common->wcmModel->name, "Intuos4"))
					tol = common->wcmMaxZ / 125;
				if (filtered.pressure < common->wcmThreshold - tol)
					filtered.buttons &= ~button;
			}
			/* transform pressure */
			transPressureCurve(priv,&filtered);
		}

		/* touch capacity is supported */
		if (IsTouch(priv) && (common->wcmCapacityDefault >= 0) && !priv->hardProx)
		{
			if (((double)(filtered.capacity * 5) / 
					(double)common->wcmMaxZ) > 
					(5 - common->wcmCapacity))
				filtered.buttons |= button;
		}
		else if (IsCursor(priv) && !priv->hardProx)
		{
			/* initial current max distance */
			if (strstr(common->wcmModel->name, "Intuos"))
				common->wcmMaxCursorDist = 256;
			else
				common->wcmMaxCursorDist = 0;
		}

		/* Store current hard prox for next use */
		if (!IsTouch(priv))
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

		if (!(priv->flags & ABSOLUTE_FLAG) && !IsPad(priv))
		{
			/* To improve the accuracy of relative x/y,
			 * don't send motion event when there is no movement.
			 */
			double deltx = filtered.x - priv->oldX;
			double delty = filtered.y - priv->oldY;
			deltx *= priv->factorY*priv->speed;
			delty *= priv->factorY*priv->speed;
	
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
					DBG(10, common->debugLevel, ErrorF(
						"Ignore non-movement relative data \n"));
					return;
				}
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

		/* force out-prox when distance is outside wcmCursorProxoutDist for pucks */
		if (IsCursor(priv))
		{
			/* force out-prox when distance is outside wcmCursorProxoutDist. */
			if (common->wcmProtocolLevel == 5)
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
		xf86WcmSendEvents(pDev, &filtered);
		/* If out-prox, reset the current area pointer */
		if (!filtered.proximity)
			tool->current = NULL;
	}

	/* otherwise, if no device matched... */
	else
	{
		DBG(11, common->debugLevel, ErrorF("no device matches with"
				" id=%d, serial=%u\n",
				ds->device_type, ds->serial_num));
	}
}

void xf86WcmSoftOutEvent(LocalDevicePtr local)
{
	/* Send a soft prox-out event for the device */
	WacomDeviceState out = { 0 };
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;

	out.device_type = DEVICE_ID(priv->flags);
	DBG(2, common->debugLevel, ErrorF("Send soft prox-out for"
		" %s first\n", local->name));
	xf86WcmSendEvents(local, &out);
}

void xf86WcmSoftOut(WacomCommonPtr common, int channel)
{
	/* Send a soft prox-out for the device */
	WacomDeviceState out = { 0 };
	WacomChannelPtr pChannel = common->wcmChannel + channel;
	WacomDeviceState ds = pChannel->valid.state;

	out.device_type = ds.device_type;
	DBG(2, common->debugLevel, ErrorF("Send soft prox-out for"
		" %s at channel %d \n", common->wcmModel->name, channel));
	xf86WcmEvent(common, channel, &out);
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

