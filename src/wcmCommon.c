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
#include <xkbsrv.h>
#include <xf86_OSproc.h>

/* Tested result for setting the pressure threshold to a reasonable value */
#define THRESHOLD_TOLERANCE (FILTER_PRESSURE_RES / 125)
#define DEFAULT_THRESHOLD (FILTER_PRESSURE_RES / 75)

/*****************************************************************************
 * Static functions
 ****************************************************************************/

static void wcmSoftOutEvent(LocalDevicePtr local);
static void transPressureCurve(WacomDevicePtr pDev, WacomDeviceStatePtr pState);
static void commonDispatchDevice(WacomCommonPtr common, unsigned int channel, 
	const WacomChannelPtr pChannel, int suppress);
static void resetSampleCounter(const WacomChannelPtr pChannel);
static void sendAButton(LocalDevicePtr local, int button, int mask,
		int rx, int ry, int rz, int v3, int v4, int v5);

/*****************************************************************************
 * wcmMappingFactor --
 *   calculate the proper tablet to screen mapping factor according to the 
 *   screen/desktop size and the tablet size 
 ****************************************************************************/

void wcmMappingFactor(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;

	DBG(10, priv, "\n"); /* just prints function name */

	wcmVirtualTabletSize(local);
	
	if (!(priv->flags & ABSOLUTE_FLAG) || !priv->wcmMMonitor)
	{
		/* Get the current screen that the cursor is in */
		if (miPointerGetScreen(local->dev))
			priv->currentScreen = miPointerGetScreen(local->dev)->myNum;
	}
	else
	{
		if (priv->screen_no != -1)
			priv->currentScreen = priv->screen_no;
		else if (priv->currentScreen == -1)
		{
			/* Get the current screen that the cursor is in */
			if (miPointerGetScreen(local->dev))
				priv->currentScreen = miPointerGetScreen(local->dev)->myNum;
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
 * wcmSetScreen --
 *   set to the proper screen according to the converted (x,y).
 *   this only supports for horizontal setup now.
 *   need to know screen's origin (x,y) to support 
 *   combined horizontal and vertical setups
 ****************************************************************************/

static void wcmSetScreen(LocalDevicePtr local, int v0, int v1)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int screenToSet = -1, i, j, x, y, tabletSize = 0;

	DBG(6, priv, "v0=%d v1=%d "
		"currentScreen=%d\n", v0, v1, priv->currentScreen);

	if (priv->screen_no != -1 && priv->screen_no >= priv->numScreen)
	{
		xf86Msg(X_ERROR, "%s: wcmSetScreen Screen%d is larger than number of available screens (%d)\n",
			local->name, priv->screen_no, priv->numScreen);
		priv->screen_no = -1;
	}

	if (!(local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER))) return;

	if (priv->twinview != TV_NONE && priv->screen_no == -1 && (priv->flags & ABSOLUTE_FLAG))
	{
		if (priv->twinview == TV_LEFT_RIGHT)
		{
			tabletSize = priv->bottomX - priv->tvoffsetX;
			if (v0 > tabletSize && v0 <= priv->bottomX)
				priv->currentScreen = 1;
			if (v0 > priv->topX && v0 <= priv->topX + priv->tvoffsetX)
				priv->currentScreen = 0;
		}
		if (priv->twinview == TV_ABOVE_BELOW)
		{
			tabletSize = priv->bottomY - priv->tvoffsetY;
			if (v0 > tabletSize && v0 <= priv->bottomY)
				priv->currentScreen = 1;
			if (v0 > priv->topY && v0 <= priv->topY + priv->tvoffsetY)
				priv->currentScreen = 0;
		}
		if (priv->twinview == TV_RIGHT_LEFT)
		{
			tabletSize = priv->bottomX - priv->tvoffsetX;
			if (v0 > tabletSize && v0 <= priv->bottomX)
				priv->currentScreen = 0;
			if (v0 > priv->topX && v0 <= priv->topX + priv->tvoffsetX)
				priv->currentScreen = 1;
		}
		if (priv->twinview == TV_BELOW_ABOVE)
		{
			tabletSize = priv->bottomY - priv->tvoffsetY;
			if (v0 > tabletSize && v0 <= priv->bottomY)
				priv->currentScreen = 0;
			if (v0 > priv->topY && v0 <= priv->topY + priv->tvoffsetY)
				priv->currentScreen = 1;
		}
		DBG(10, priv, "TwinView setup screenToSet=%d\n",
			priv->currentScreen);
	}

	wcmMappingFactor(local);
	if (!(priv->flags & ABSOLUTE_FLAG) || screenInfo.numScreens == 1 || !priv->wcmMMonitor)
		return;

	v0 = v0 - priv->topX;
	v1 = v1 - priv->topY;

	if (priv->screen_no == -1)
	{
		for (i = 0; i < priv->numScreen; i++)
		{
			if (v0 * priv->factorX >= priv->screenTopX[i] && 
				v0 * priv->factorX < priv->screenBottomX[i] - 0.5)
			{
				
				for (j = 0; j < priv->numScreen; j++)
				{
					if (v1 * priv->factorY >= priv->screenTopY[j] && 
						v1 * priv->factorY <= priv->screenBottomY[j] - 0.5)
					{
						if (j == i)
						{
							screenToSet = i;
							break;
						}
					}
				}
					
				if (screenToSet != -1)
					break;
			}
		}
	}
	else
		screenToSet = priv->screen_no;

	if (screenToSet == -1)
	{
		DBG(3, priv, "Error: "
			"Can not find valid screen (currentScreen=%d)\n",
			priv->currentScreen);
		return;
	}

	wcmVirtualTabletPadding(local);
	x = ((double)(v0 + priv->leftPadding) * priv->factorX) - priv->screenTopX[screenToSet] + 0.5;
	y = ((double)(v1 + priv->topPadding) * priv->factorY) - priv->screenTopY[screenToSet] + 0.5;
		
	if (x >= screenInfo.screens[screenToSet]->width)
		x = screenInfo.screens[screenToSet]->width - 1;
	if (y >= screenInfo.screens[screenToSet]->height)
		y = screenInfo.screens[screenToSet]->height - 1;

	xf86XInputSetScreen(local, screenToSet, x, y);
	DBG(10, priv, "current=%d ToSet=%d\n",
			priv->currentScreen, screenToSet);
	priv->currentScreen = screenToSet;
}

/*****************************************************************************
 * wcmSendButtons --
 *   Send button events by comparing the current button mask with the
 *   previous one.
 ****************************************************************************/

static void wcmSendButtons(LocalDevicePtr local, int buttons, int rx, int ry,
		int rz, int v3, int v4, int v5)
{
	int button, mask;
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	DBG(6, priv, "buttons=%d\n", buttons);

	/* Tablet PC buttons only apply to penabled devices */
	if (common->wcmTPCButton && (priv->flags & STYLUS_ID))
	{
		if ( buttons & 1 )
		{
			if ( !(priv->flags & TPCBUTTONS_FLAG) )
			{
				priv->flags |= TPCBUTTONS_FLAG;

				if (buttons == 1) {
					/* Button 1 pressed */
					sendAButton(local, 0, 1, rx, ry, rz, v3, v4, v5);
				} else {
					/* send all pressed buttons down */
					for (button=2; button<=WCM_MAX_BUTTONS; button++)
					{
						mask = 1 << (button-1);
						if ( buttons & mask )
						{
							/* set to the configured button */
							sendAButton(local, button-1, 1, rx, ry,
									rz, v3, v4, v5);
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
			for (button=1; button<=WCM_MAX_BUTTONS; button++)
			{
				mask = 1 << (button-1);
				if ((mask & priv->oldButtons) != (mask & buttons) || (mask & buttons) )
				{
					/* set to the configured button */
					sendAButton(local, button-1, 0, rx, ry, 
						rz, v3, v4, v5);
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
				sendAButton(local, button-1, mask & buttons, rx, ry, 
					rz, v3, v4, v5);
			}
		}
	}
}

/*****************************************************************************
 * wcmEmitKeysym --
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

void wcmEmitKeysym (DeviceIntPtr keydev, int keysym, int state)
{
	int i, j, alt_keysym = 0;

	/* Now that we have the keycode look for key index */
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
	KeySymsRec *ksr = XkbGetCoreMap(keydev);
#else
	KeySymsRec *ksr = &keydev->key->curKeySyms;
#endif

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
			xf86Msg (X_WARNING, "%s: Couldn't find key with code %08x on keyboard device %s\n",
					keydev->name, keysym, keydev->name);
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
		xfree(ksr);
#endif
		return;
	}
	xf86PostKeyboardEvent (keydev, i, state);
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
	xfree(ksr);
#endif
}

static void toggleDisplay(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
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
					wcmChangeScreen(tmppriv->local, screen);
				}
			}
		}
		else /* toggle display only for the selected tool */
		{
			int screen = priv->screen_no;
			if (++screen >= priv->numScreen)
				screen = -1;
			wcmChangeScreen(local, screen);
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

/*****************************************************************************
 * sendAButton --
 *   Send one button event, called by wcmSendButtons
 ****************************************************************************/
static void sendAButton(LocalDevicePtr local, int button, int mask,
		int rx, int ry, int rz, int v3, int v4, int v5)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
#ifdef DEBUG
	WacomCommonPtr common = priv->common;
#endif
	int is_absolute = priv->flags & ABSOLUTE_FLAG;
	int i;

	int naxes = priv->naxes;

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
		xf86PostButtonEvent(local->dev, is_absolute, priv->button[button], (mask != 0), 0, naxes,
				    rx, ry, rz, v3, v4, v5);
		return;
	}

	/* Actions only trigger on press, not release */
	for (i = 0; mask && i < ARRAY_SIZE(priv->keys[button]); i++)
	{
		unsigned int action = priv->keys[button][i];

		if (!action)
			break;

		switch ((action & AC_TYPE))
		{
			case AC_BUTTON:
				{
					int btn_no = (action & AC_CODE);
					int is_press = (action & AC_KEYBTNPRESS);
					xf86PostButtonEvent(local->dev,
							    is_absolute, btn_no,
							    is_press, 0, naxes,
							    rx, ry, rz, v3, v4, v5);
				}
				break;
			case AC_KEY:
				{
					int key_sym = (action & AC_CODE);
					int is_press = (action & AC_KEYBTNPRESS);
					wcmEmitKeysym(local->dev, key_sym, is_press);
				}
				break;
			case AC_MODETOGGLE:
				if (mask)
					wcmDevSwitchModeCall(local,
							(is_absolute) ? Relative : Absolute); /* not a typo! */
				break;
			/* FIXME: this should be implemented as 4 values,
			 * there's no reason to have a DBLCLICK */
			case AC_DBLCLICK:
				xf86PostButtonEvent(local->dev, is_absolute,
						    1,1,0,naxes, rx,ry,rz,v3,v4,v5);
				xf86PostButtonEvent(local->dev, is_absolute,
						    1,0,0,naxes,rx,ry,rz,v3,v4,v5);
				xf86PostButtonEvent(local->dev, is_absolute,
						    1,1,0,naxes, rx,ry,rz,v3,v4,v5);
				xf86PostButtonEvent(local->dev, is_absolute,
						    1,0,0,naxes,rx,ry,rz,v3,v4,v5);
				break;
			case AC_DISPLAYTOGGLE:
				toggleDisplay(local);
				break;
		}
	}

	/* Release all non-released keys for this button. */
	for (i = 0; !mask && i < ARRAY_SIZE(priv->keys[button]); i++)
	{
		unsigned int action = priv->keys[button][i];

		switch ((action & AC_TYPE))
		{
			case AC_BUTTON:
				{
					int btn_no = (action & AC_CODE);

					/* don't care about releases here */
					if (!(action & AC_KEYBTNPRESS))
						break;

					if (countPresses(btn_no, &priv->keys[button][i],
							ARRAY_SIZE(priv->keys[button]) - i))
						xf86PostButtonEvent(local->dev,
								is_absolute, btn_no,
								0, 0, naxes,
								rx, ry, rz, v3, v4, v5);
				}
				break;
			case AC_KEY:
				{
					int key_sym = (action & AC_CODE);

					/* don't care about releases here */
					if (!(action & AC_KEYBTNPRESS))
						break;

					if (countPresses(key_sym, &priv->keys[button][i],
							ARRAY_SIZE(priv->keys[button]) - i))
						wcmEmitKeysym(local->dev, key_sym, 0);
				}
		}

	}
}

/*****************************************************************************
 * getWheelButton --
 *   Get the wheel button to be sent for the current device state.
 ****************************************************************************/

static int getWheelButton(LocalDevicePtr local, const WacomDeviceState* ds)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int fakeButton = 0, value;

	/* emulate events for relative wheel */
	if ( ds->relwheel )
	{
		value = ds->relwheel;
		if ( ds->relwheel > 0 )
			fakeButton = priv->relup;
		else
			fakeButton = priv->reldn;
	}

	/* emulate events for absolute wheel when needed */
	if ( ds->abswheel != priv->oldWheel )
	{
		value = priv->oldWheel - ds->abswheel;
		if ( value > 0 )
			fakeButton = priv->wheelup;
		else
			fakeButton = priv->wheeldn;
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
		if ( value > 0 )
			fakeButton = priv->striplup;
		else if ( value < 0 )
			fakeButton = priv->stripldn;
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
		if ( value > 0 )
			fakeButton = priv->striprup;
		else if ( value < 0 )
			fakeButton = priv->striprdn;
	}

	DBG(10, priv, "send fakeButton %x with value = %d \n",
		fakeButton, value);

	return fakeButton;
}
/*****************************************************************************
 * sendWheelStripEvents --
 *   Send events defined for relative/absolute wheels or strips
 ****************************************************************************/

static void sendWheelStripEvents(LocalDevicePtr local, const WacomDeviceState* ds,
		int x, int y, int z, int v3, int v4, int v5)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int fakeButton = 0, naxes = priv->naxes;
	int is_absolute = priv->flags & ABSOLUTE_FLAG;

	DBG(10, priv, "\n");

	fakeButton = getWheelButton(local, ds);

	if (!fakeButton)
		return;

	switch (fakeButton & AC_TYPE)
	{
	    case 0: /* no spec. action defined */
	    case AC_BUTTON:
		/* send both button on/off in the same event for pad */	
		xf86PostButtonEvent(local->dev, is_absolute, fakeButton & AC_CODE,
			1,0,naxes,x,y,z,v3,v4,v5);

		xf86PostButtonEvent(local->dev, is_absolute, fakeButton & AC_CODE,
			0,0,naxes,x,y,z,v3,v4,v5);
	    break;

	    case AC_KEY:
		    wcmEmitKeysym(local->dev, (fakeButton & AC_CODE), 1);
		    wcmEmitKeysym(local->dev, (fakeButton & AC_CODE), 0);
	    break;

	    default:
		xf86Msg(X_WARNING, "%s: unsupported event %x \n", local->name, fakeButton);
	}
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
		wcmSendButtons(local,buttons,x,y,z,v3,v4,v5);

	/* emulate wheel/strip events when defined */
	if ( ds->relwheel || ds->abswheel || 
		( (ds->stripx - priv->oldStripX) && ds->stripx && priv->oldStripX) || 
			((ds->stripy - priv->oldStripY) && ds->stripy && priv->oldStripY) )
		sendWheelStripEvents(local, ds, x, y, z, v3, v4, v5);
}

/* rotate x and y before post X inout events */
void wcmRotateCoordinates(LocalDevicePtr local, int* x, int* y)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int tmp_coord;

	/* rotation mixes x and y up a bit */
	if (common->wcmRotate == ROTATE_CW)
	{
		tmp_coord = *x;
		*x = *y;
		*y = priv->maxY - tmp_coord;
	}
	else if (common->wcmRotate == ROTATE_CCW)
	{
		tmp_coord = *y;
		*y = *x;
		*x = priv->maxX - tmp_coord;
	}
	else if (common->wcmRotate == ROTATE_HALF)
	{
		*x = priv->maxX - *x;
		*y = priv->maxY - *y;
	}
}

static void wcmUpdateOldState(const LocalDevicePtr local,
			      const WacomDeviceState *ds)
{
	const WacomDevicePtr priv = (WacomDevicePtr) local->private;
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

/*****************************************************************************
 * wcmSendEvents --
 *   Send events according to the device state.
 ****************************************************************************/

void wcmSendEvents(LocalDevicePtr local, const WacomDeviceState* ds)
{
#ifdef DEBUG
	int is_button = !!(ds->buttons);
#endif
	int type = ds->device_type;
	int id = ds->device_id;
	int serial = (int)ds->serial_num;
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
	int naxes = priv->naxes;
	int is_absolute = priv->flags & ABSOLUTE_FLAG;
	int v3, v4, v5;

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
		(type == STYLUS_ID) ? "stylus" :
			(type == CURSOR_ID) ? "cursor" : 
			(type == ERASER_ID) ? "eraser" :
			(type == TOUCH_ID) ? "touch" : "pad",
		priv->oldProximity ? "true" : "false",
		x, y, z, is_button ? "true" : "false", buttons,
		tx, ty, wheel, rot, throttle);

	if (ds->proximity)
		wcmRotateCoordinates(local, &x, &y);

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

	DBG(6, priv, "%s prox=%d\tx=%d"
		"\ty=%d\tz=%d\tv3=%d\tv4=%d\tv5=%d\tid=%d"
		"\tserial=%u\tbutton=%s\tbuttons=%d\n",
		is_absolute ? "abs" : "rel",
		is_proximity,
		x, y, z, v3, v4, v5, id, serial,
		is_button ? "true" : "false", buttons);

	priv->currentX = x;
	priv->currentY = y;

	/* update the old records */
	if(!priv->oldProximity)
	{
		wcmUpdateOldState(local, ds);
		priv->oldButtons = 0;
	}

	if (!is_absolute)
	{
		x -= priv->oldX;
		y -= priv->oldY;
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
				wcmSetScreen(local, x, y);
			}

			/* unify acceleration in both directions 
			 * for relative mode to draw a circle 
			 */
			if (!is_absolute)
				x *= priv->factorY / priv->factorX;
 			else
			{
				/* Padding virtual values */
				wcmVirtualTabletPadding(local);
				x += priv->leftPadding;
				y += priv->topPadding;
			}

			if (common->wcmScaling)
			{
				/* In the case that wcmDevConvert doesn't get called.
				 * The +-0.4 is to increase the sensitivity in relative mode.
				 * Must be sensitive to which way the tool is moved or one way
				 * will get a severe penalty for small movements.
				 */
 				if(is_absolute) {
					x -= priv->topX;
					y -= priv->topY;
					if (priv->currentScreen == 1 && priv->twinview != TV_NONE)
					{
						x -= priv->tvoffsetX;
						y -= priv->tvoffsetY;
					}
				}
				x = (int)((double)x * priv->factorX + (x>=0?0.4:-0.4));
				y = (int)((double)y * priv->factorY + (y>=0?0.4:-0.4));

				if ((priv->flags & ABSOLUTE_FLAG) && (priv->twinview == TV_NONE))
				{
					x -= priv->screenTopX[priv->currentScreen];
					y -= priv->screenTopY[priv->currentScreen];
				}

				if (priv->screen_no != -1)
				{
					if (x > priv->screenBottomX[priv->currentScreen] - priv->screenTopX[priv->currentScreen])
						x = priv->screenBottomX[priv->currentScreen];
					if (x < 0) x = 0;
					if (y > priv->screenBottomY[priv->currentScreen] - priv->screenTopY[priv->currentScreen])
						y = priv->screenBottomY[priv->currentScreen];
					if (y < 0) y = 0;
	
				}
				priv->currentSX = x;
				priv->currentSY = y;
			}

			/* don't emit proximity events if device does not support proximity */
			if ((local->dev->proximity && !priv->oldProximity))
				xf86PostProximityEvent(local->dev, 1, 0, naxes, x, y, z, v3, v4, v5);

			/* Move the cursor to where it should be before sending button events */
			if(!(priv->flags & BUTTONS_ONLY_FLAG))
				xf86PostMotionEvent(local->dev, is_absolute,
					0, naxes, x, y, z, v3, v4, v5);

			sendCommonEvents(local, ds, x, y, z, v3, v4, v5);
		}
		else /* not in proximity */
		{
			buttons = 0;

			if (common->wcmScaling)
			{
				/* In the case that wcmDevConvert doesn't called */
				x = priv->currentSX;
				y = priv->currentSY;
			}

			/* reports button up when the device has been
			 * down and becomes out of proximity */
			if (priv->oldButtons)
				wcmSendButtons(local,buttons,x,y,z,v3,v4,v5);

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
				wcmSetScreen(local, x, y);

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
				wcmSendButtons(local, buttons,
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
		wcmUpdateOldState(local, ds);
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
 * wcmCheckSuppress --
 *  Determine whether device state has changed enough - return 0
 *  if not.
 ****************************************************************************/

static int wcmCheckSuppress(WacomCommonPtr common, const WacomDeviceState* dsOrig,
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

	DBG(10, common, "level = %d"
		" return value = %d\n", suppress, returnV);
	return returnV;
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
 * wcmEvent -
 *   Handles suppression, transformation, filtering, and event dispatch.
 ****************************************************************************/

void wcmEvent(WacomCommonPtr common, unsigned int channel,
	const WacomDeviceState* pState)
{
	WacomDeviceState* pLast;
	WacomDeviceState ds;
	WacomChannelPtr pChannel;
	WacomFilterState* fs;
	int i, suppress = 0;

	pChannel = common->wcmChannel + channel;
	pLast = &pChannel->valid.state;

	DBG(10, common, "channel = %d\n", channel);

	/* sanity check the channel */
	if (channel >= MAX_CHANNELS)
		return;
	
	/* we must copy the state because certain types of filtering
	 * will need to change the values (ie. for error correction) */
	ds = *pState;

	/* timestamp the state for velocity and acceleration analysis */
	ds.sample = (int)GetTimeInMillis();
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

	fs = &pChannel->rawFilter;
	if (!fs->npoints && ds.proximity)
	{
		DBG(11, common, "initialize Channel data.\n");
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
		if (RAW_FILTERING(common) && common->wcmModel->FilterRaw && ds.device_type != PAD_ID)
		{
			if (common->wcmModel->FilterRaw(common,pChannel,&ds))
			{
				DBG(10, common,
					"Raw filtering discarded data.\n");
				resetSampleCounter(pChannel);
				return; /* discard */
			}
		}

		/* Discard unwanted data */
		suppress = wcmCheckSuppress(common, pLast, &ds);
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

	/* process second finger data if exists
	 * and both touch and geature are enabled */
	if ((ds.device_type == TOUCH_ID) &&
		common->wcmTouch && common->wcmGesture)
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
		if (!ds.proximity && !dsOther.proximity)
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
			if (!channel && common->wcmGestureMode)
				goto ret;

			/* process gesture */
			if (channel)
			{
				wcmFingerTapToClick(common);
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
	if (pDev)
	{
		WacomDeviceState filtered = pChannel->valid.state;

		/* Device transformations come first */
		/* button 1 Threshold test */
		int button = 1;
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
			LocalDevicePtr localDevices = xf86FirstLocalDevice();
			WacomCommonPtr tempcommon = NULL;
			WacomDevicePtr temppriv = NULL;

			/* Lookup to see if associated touch was enabled */
			for (; localDevices != NULL; localDevices = localDevices->next)
			{
				if (strstr(localDevices->drv->driverName, "wacom"))
				{
					temppriv = (WacomDevicePtr) localDevices->private;
					tempcommon = temppriv->common;

					if ((tempcommon->tablet_id == common->tablet_id) && 
						IsTouch(temppriv) && temppriv->oldProximity)
					{
						/* Send soft prox-out for touch first */
						wcmSoftOutEvent(localDevices);
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

		if ((!(priv->flags & ABSOLUTE_FLAG)) && (!IsPad(priv)))
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

	/* otherwise, if no device matched... */
	else
	{
		DBG(11, common, "no device matches with"
				" id=%d, serial=%u\n",
				ds->device_type, ds->serial_num);
	}
}

/*****************************************************************************
 * wcmInitTablet -- common initialization for all tablets
 ****************************************************************************/

int wcmInitTablet(LocalDevicePtr local, const char* id, float version)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomModelPtr model = common->wcmModel;

	/* Initialize the tablet */
	model->Initialize(common,id,version);

	/* Get tablet resolution */
	if (model->GetResolution)
		model->GetResolution(local);

	/* Get tablet range */
	if (model->GetRanges && (model->GetRanges(local) != Success))
		return !Success;
	
	/* Default threshold value if not set */
	if (common->wcmThreshold <= 0)
	{
		/* Threshold for counting pressure as a button */
		common->wcmThreshold = DEFAULT_THRESHOLD;

		xf86Msg(X_PROBED, "%s: using pressure threshold of %d for button 1\n",
			local->name, common->wcmThreshold);
	}

	/* output tablet state as probed */
	xf86Msg(X_PROBED, "%s: Wacom %s tablet speed=%d maxX=%d maxY=%d maxZ=%d "
			"resX=%d resY=%d  tilt=%s\n",
			local->name,
			model->name, common->wcmISDV4Speed, 
			common->wcmMaxX, common->wcmMaxY, common->wcmMaxZ,
			common->wcmResolX, common->wcmResolY,
			HANDLE_TILT(common) ? "enabled" : "disabled");
  
	/* start the tablet data */
	if (model->Start && (model->Start(local) != Success))
		return !Success;

	return Success;
}

/* Send a soft prox-out event for the device */
static void wcmSoftOutEvent(LocalDevicePtr local)
{
	WacomDeviceState out = { 0 };
	WacomDevicePtr priv = (WacomDevicePtr) local->private;

	out.device_type = DEVICE_ID(priv->flags);
	out.device_id = wcmGetPhyDeviceID(priv);
	DBG(2, priv->common, "send a soft prox-out\n");
	wcmSendEvents(local, &out);
}

/*****************************************************************************
** Transformations
*****************************************************************************/

static void transPressureCurve(WacomDevicePtr pDev, WacomDeviceStatePtr pState)
{
	if (pDev->pPressCurve)
	{
		/* clip the pressure */
		int p = max(0, pState->pressure);

		p = min(FILTER_PRESSURE_RES, p);

		/* apply pressure curve function */
		p = pDev->pPressCurve[p];
	}
}

/*****************************************************************************
 * wcmInitialTVScreens
 ****************************************************************************/

static void wcmInitialTVScreens(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	if (priv->twinview == TV_NONE)
		return;

	priv->numScreen = 2;

	if ((priv->twinview == TV_LEFT_RIGHT) || (priv->twinview == TV_RIGHT_LEFT))
	{
		/* it does not need the offset if always map to a specific screen */
		if (priv->screen_no == -1)
		{
			priv->tvoffsetX = 60;
			priv->tvoffsetY = 0;
		}

		/* default resolution */
		if(!priv->tvResolution[0])
		{
			priv->tvResolution[0] = screenInfo.screens[0]->width/2;
			priv->tvResolution[1] = screenInfo.screens[0]->height;
			priv->tvResolution[2] = priv->tvResolution[0];
			priv->tvResolution[3] = priv->tvResolution[1];
		}
	}
	else if ((priv->twinview == TV_ABOVE_BELOW) || (priv->twinview == TV_BELOW_ABOVE))
	{
		/* it does not need the offset if always map to a specific screen */
		if (priv->screen_no == -1)
		{
			priv->tvoffsetX = 0;
			priv->tvoffsetY = 60;
		}

		/* default resolution */
		if(!priv->tvResolution[0])
		{
			priv->tvResolution[0] = screenInfo.screens[0]->width;
			priv->tvResolution[1] = screenInfo.screens[0]->height/2;
			priv->tvResolution[2] = priv->tvResolution[0];
			priv->tvResolution[3] = priv->tvResolution[1];
		}
	}

	/* initial screen info */
	if (priv->twinview == TV_ABOVE_BELOW)
	{
		priv->screenTopX[0] = 0;
		priv->screenTopY[0] = 0;
		priv->screenBottomX[0] = priv->tvResolution[0];
		priv->screenBottomY[0] = priv->tvResolution[1];
		priv->screenTopX[1] = 0;
		priv->screenTopY[1] = priv->tvResolution[1];
		priv->screenBottomX[1] = priv->tvResolution[2];
		priv->screenBottomY[1] = priv->tvResolution[1] + priv->tvResolution[3];
	}
	if (priv->twinview == TV_LEFT_RIGHT)
	{
		priv->screenTopX[0] = 0;
		priv->screenTopY[0] = 0;
		priv->screenBottomX[0] = priv->tvResolution[0];
		priv->screenBottomY[0] = priv->tvResolution[1];
		priv->screenTopX[1] = priv->tvResolution[0];
		priv->screenTopY[1] = 0;
		priv->screenBottomX[1] = priv->tvResolution[0] + priv->tvResolution[2];
		priv->screenBottomY[1] = priv->tvResolution[3];
	}
	if (priv->twinview == TV_BELOW_ABOVE)
	{
		priv->screenTopX[0] = 0;
		priv->screenTopY[0] = priv->tvResolution[1];
		priv->screenBottomX[0] = priv->tvResolution[2];
		priv->screenBottomY[0] = priv->tvResolution[1] + priv->tvResolution[3];
		priv->screenTopX[1] = 0;
		priv->screenTopY[1] = 0;
		priv->screenBottomX[1] = priv->tvResolution[0];
		priv->screenBottomY[1] = priv->tvResolution[1];
	}
	if (priv->twinview == TV_RIGHT_LEFT)
	{
		priv->screenTopX[0] = priv->tvResolution[0];
		priv->screenTopY[0] = 0;
		priv->screenBottomX[0] = priv->tvResolution[0] + priv->tvResolution[2];
		priv->screenBottomY[0] = priv->tvResolution[3];
		priv->screenTopX[1] = 0;
		priv->screenTopY[1] = 0;
		priv->screenBottomX[1] = priv->tvResolution[0];
		priv->screenBottomY[1] = priv->tvResolution[1];
	}

	DBG(10, priv,
		"topX0=%d topY0=%d bottomX0=%d bottomY0=%d "
		"topX1=%d topY1=%d bottomX1=%d bottomY1=%d \n",
		priv->screenTopX[0], priv->screenTopY[0],
		priv->screenBottomX[0], priv->screenBottomY[0],
		priv->screenTopX[1], priv->screenTopY[1],
		priv->screenBottomX[1], priv->screenBottomY[1]);
}

/*****************************************************************************
 * wcmInitialScreens
 ****************************************************************************/

void wcmInitialScreens(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	int i;

	DBG(2, priv, "number of screen=%d \n", screenInfo.numScreens);
	priv->tvoffsetX = 0;
	priv->tvoffsetY = 0;
	if (priv->twinview != TV_NONE)
	{
		wcmInitialTVScreens(local);
		return;
	}

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
			priv->screenTopX[i] = dixScreenOrigins[i].x;
			priv->screenTopY[i] = dixScreenOrigins[i].y;
			priv->screenBottomX[i] = dixScreenOrigins[i].x;
			priv->screenBottomY[i] = dixScreenOrigins[i].y;

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
 * rotateOneTool
 ****************************************************************************/

static void rotateOneTool(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomToolAreaPtr area = priv->toolarea;
	int tmpTopX, tmpTopY, tmpBottomX, tmpBottomY, oldMaxX, oldMaxY;

	DBG(10, priv, "\n");

	oldMaxX = priv->maxX;
	oldMaxY = priv->maxY;

	tmpTopX = priv->topX;
	tmpBottomX = priv->bottomX;
	tmpTopY = priv->topY;
	tmpBottomY = priv->bottomY;

	if (common->wcmRotate == ROTATE_CW || common->wcmRotate == ROTATE_CCW)
	{
		priv->maxX = oldMaxY;
		priv->maxY = oldMaxX;
	}

	switch (common->wcmRotate) {
	      case ROTATE_CW:
		area->topX = priv->topX = tmpTopY;
		area->bottomX = priv->bottomX = tmpBottomY;
		area->topY = priv->topY = oldMaxX - tmpBottomX;
		area->bottomY = priv->bottomY =oldMaxX - tmpTopX;
		break;
	      case ROTATE_CCW:
		area->topX = priv->topX = oldMaxY - tmpBottomY;
		area->bottomX = priv->bottomX = oldMaxY - tmpTopY;
		area->topY = priv->topY = tmpTopX;
		area->bottomY = priv->bottomY = tmpBottomX;
		break;
	      case ROTATE_HALF:
		area->topX = priv->topX = oldMaxX - tmpBottomX;
		area->bottomX = priv->bottomX = oldMaxX - tmpTopX;
		area->topY = priv->topY= oldMaxY - tmpBottomY;
		area->bottomY = priv->bottomY = oldMaxY - tmpTopY;
		break;
	}
	wcmInitialCoordinates(priv->local, 0);
	wcmInitialCoordinates(priv->local, 1);

	if (tmpTopX != priv->topX)
		xf86ReplaceIntOption(priv->local->options, "TopX", priv->topX);
	if (tmpTopY != priv->topY)
		xf86ReplaceIntOption(priv->local->options, "TopY", priv->topY);
	if (tmpBottomX != priv->bottomX)
		xf86ReplaceIntOption(priv->local->options, "BottomX", priv->bottomX);
	if (tmpBottomY != priv->bottomY)
		xf86ReplaceIntOption(priv->local->options, "BottomY", priv->bottomY);
}

/*****************************************************************************
 * wcmRotateTablet
 ****************************************************************************/

void wcmRotateTablet(LocalDevicePtr local, int value)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomDevicePtr tmppriv;
	int oldRotation;
	int tmpTopX, tmpTopY, tmpBottomX, tmpBottomY, oldMaxX, oldMaxY;

	DBG(10, priv, "\n");

	if (common->wcmRotate == value) /* initialization */
	{
		rotateOneTool(priv);
	}
	else
	{
		oldRotation = common->wcmRotate;
		common->wcmRotate = value;

		/* rotate all devices at once! else they get misaligned */
		for (tmppriv = common->wcmDevices; tmppriv; tmppriv = tmppriv->next)
		{
		    oldMaxX = tmppriv->maxX;
		    oldMaxY = tmppriv->maxY;

		    if (oldRotation == ROTATE_CW || oldRotation == ROTATE_CCW)
		    {
			tmppriv->maxX = oldMaxY;
			tmppriv->maxY = oldMaxX;
		    }

		    tmpTopX = tmppriv->topX;
		    tmpBottomX = tmppriv->bottomX;
		    tmpTopY = tmppriv->topY;
		    tmpBottomY = tmppriv->bottomY;

		    /* recover to the unrotated xy-rectangles */
		    switch (oldRotation) {
		      case ROTATE_CW:
			tmppriv->topX = oldMaxY - tmpBottomY;
			tmppriv->bottomX = oldMaxY - tmpTopY;
			tmppriv->topY = tmpTopX;
			tmppriv->bottomY = tmpBottomX;
			break;
		      case ROTATE_CCW:
			tmppriv->topX = tmpTopY;
			tmppriv->bottomX = tmpBottomY;
			tmppriv->topY = oldMaxX - tmpBottomX;
			tmppriv->bottomY = oldMaxX - tmpTopX;
			break;
		      case ROTATE_HALF:
			tmppriv->topX = oldMaxX - tmpBottomX;
			tmppriv->bottomX = oldMaxX - tmpTopX;
			tmppriv->topY = oldMaxY - tmpBottomY;
			tmppriv->bottomY = oldMaxY - tmpTopY;
			break;
		    }

		    /* and rotate them to the new value */
		    rotateOneTool(tmppriv);

		    switch(value) {
			case ROTATE_NONE:
			    xf86ReplaceStrOption(local->options, "Rotate", "NONE");
			break;
			case ROTATE_CW:
			    xf86ReplaceStrOption(local->options, "Rotate", "CW");
			break;
			case ROTATE_CCW:
			    xf86ReplaceStrOption(local->options, "Rotate", "CCW");
			break;
			case ROTATE_HALF:
			    xf86ReplaceStrOption(local->options, "Rotate", "HALF");
			break;
		    }
		}
	}
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


/* vim: set noexpandtab shiftwidth=8: */
