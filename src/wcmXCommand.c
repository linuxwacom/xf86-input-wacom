/*
 * Copyright 2007-2010 by Ping Cheng, Wacom. <pingc@wacom.com>
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
#include <wacom-properties.h>

#include "xf86Wacom.h"
#include "wcmFilter.h"
#include <exevents.h>

/*****************************************************************************
* wcmDevSwitchModeCall --
*****************************************************************************/

int wcmDevSwitchModeCall(LocalDevicePtr local, int mode)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	int is_absolute = priv->flags & ABSOLUTE_FLAG;

	DBG(3, priv, "to mode=%d\n", mode);

	/* Pad is always in relative mode.*/
	if (IsPad(priv))
		return (mode == Relative) ? Success : XI_BadMode;

	if ((mode == Absolute) && !is_absolute)
	{
		priv->flags |= ABSOLUTE_FLAG;
		wcmInitialCoordinates(local, 0);
		wcmInitialCoordinates(local, 1);
	}
	else if ((mode == Relative) && is_absolute)
	{
		priv->flags &= ~ABSOLUTE_FLAG; 
		wcmInitialCoordinates(local, 0);
		wcmInitialCoordinates(local, 1);
	}
	else if ( (mode != Absolute) && (mode != Relative))
	{
		DBG(10, priv, "invalid mode=%d\n", mode);
		return XI_BadMode;
	}

	return Success;
}

/*****************************************************************************
* wcmDevSwitchMode --
*****************************************************************************/

int wcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
	LocalDevicePtr local = (LocalDevicePtr)dev->public.devicePrivate;
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	DBG(3, priv, "dev=%p mode=%d\n",
		(void *)dev, mode);
#endif
	/* Share this call with sendAButton in wcmCommon.c */
	return wcmDevSwitchModeCall(local, mode);
}

/*****************************************************************************
 * wcmChangeScreen
 ****************************************************************************/

void wcmChangeScreen(LocalDevicePtr local, int value)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	if (priv->screen_no != value)
	{
		priv->screen_no = value;
		xf86ReplaceIntOption(local->options, "ScreenNo", value);
	}

	if (priv->screen_no != -1)
		priv->currentScreen = priv->screen_no;
	wcmInitialScreens(local);
	wcmInitialCoordinates(local, 0);
	wcmInitialCoordinates(local, 1);
}

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3

Atom prop_rotation;
Atom prop_tablet_area;
Atom prop_screen_area;
Atom prop_pressurecurve;
Atom prop_serials;
Atom prop_strip_buttons;
Atom prop_wheel_buttons;
Atom prop_display;
Atom prop_tv_resolutions;
Atom prop_screen;
Atom prop_cursorprox;
Atom prop_capacity;
Atom prop_threshold;
Atom prop_suppress;
Atom prop_touch;
Atom prop_hover;
Atom prop_tooltype;
Atom prop_btnactions;
#ifdef DEBUG
Atom prop_debuglevels;
#endif

/* Special case: format -32 means type is XA_ATOM */
static Atom InitWcmAtom(DeviceIntPtr dev, char *name, int format, int nvalues, int *values)
{
	int i;
	Atom atom;
	uint8_t val_8[WCM_MAX_MOUSE_BUTTONS];
	uint16_t val_16[WCM_MAX_MOUSE_BUTTONS];
	uint32_t val_32[WCM_MAX_MOUSE_BUTTONS];
	pointer converted = val_32;
	Atom type = XA_INTEGER;

	if (format == -32)
	{
		type = XA_ATOM;
		format = 32;
	}

	for (i = 0; i < nvalues; i++)
	{
		switch(format)
		{
			case 8:  val_8[i]  = values[i]; break;
			case 16: val_16[i] = values[i]; break;
			case 32: val_32[i] = values[i]; break;
		}
	}

	switch(format)
	{
		case 8: converted = val_8; break;
		case 16: converted = val_16; break;
		case 32: converted = val_32; break;
	}

	atom = MakeAtom(name, strlen(name), TRUE);
	XIChangeDeviceProperty(dev, atom, type, format,
			PropModeReplace, nvalues,
			converted, FALSE);
	XISetDevicePropertyDeletable(dev, atom, FALSE);
	return atom;
}

void InitWcmDeviceProperties(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int values[WCM_MAX_MOUSE_BUTTONS];

	DBG(10, priv, "\n");

	values[0] = priv->topX;
	values[1] = priv->topY;
	values[2] = priv->bottomX;
	values[3] = priv->bottomY;
	prop_tablet_area = InitWcmAtom(local->dev, WACOM_PROP_TABLET_AREA, 32, 4, values);

	values[0] = common->wcmRotate;
	prop_rotation = InitWcmAtom(local->dev, WACOM_PROP_ROTATION, 8, 1, values);

	if (IsStylus(priv) || IsEraser(priv)) {
		values[0] = priv->nPressCtrl[0];
		values[1] = priv->nPressCtrl[1];
		values[2] = priv->nPressCtrl[2];
		values[3] = priv->nPressCtrl[3];
		prop_pressurecurve = InitWcmAtom(local->dev, WACOM_PROP_PRESSURECURVE, 32, 4, values);
	}

	values[0] = common->tablet_id;
	values[1] = priv->old_serial;
	values[2] = priv->old_device_id;
	values[3] = priv->serial;
	prop_serials = InitWcmAtom(local->dev, WACOM_PROP_SERIALIDS, 32, 4, values);

	if (IsPad(priv)) {
		values[0] = priv->striplup;
		values[1] = priv->stripldn;
		values[2] = priv->striprup;
		values[3] = priv->striprdn;
		prop_strip_buttons = InitWcmAtom(local->dev, WACOM_PROP_STRIPBUTTONS, 8, 4, values);

		values[0] = priv->relup;
		values[1] = priv->reldn;
		values[2] = priv->wheelup;
		values[3] = priv->wheeldn;
		prop_wheel_buttons = InitWcmAtom(local->dev, WACOM_PROP_WHEELBUTTONS, 8, 4, values);
	}

	values[0] = priv->tvResolution[0];
	values[1] = priv->tvResolution[1];
	values[2] = priv->tvResolution[2];
	values[3] = priv->tvResolution[3];
	prop_tv_resolutions = InitWcmAtom(local->dev, WACOM_PROP_TWINVIEW_RES, 32, 4, values);


	values[0] = priv->screen_no;
	values[1] = priv->twinview;
	values[2] = priv->wcmMMonitor;
	prop_display = InitWcmAtom(local->dev, WACOM_PROP_DISPLAY_OPTS, 8, 3, values);

	values[0] = priv->screenTopX[priv->currentScreen];
	values[1] = priv->screenTopY[priv->currentScreen];
	values[2] = priv->screenBottomX[priv->currentScreen];
	values[3] = priv->screenBottomY[priv->currentScreen];
	prop_screen = InitWcmAtom(local->dev, WACOM_PROP_SCREENAREA, 32, 4, values);

	values[0] = common->wcmCursorProxoutDist;
	prop_cursorprox = InitWcmAtom(local->dev, WACOM_PROP_PROXIMITY_THRESHOLD, 32, 1, values);

	values[0] = common->wcmCapacity;
	prop_capacity = InitWcmAtom(local->dev, WACOM_PROP_CAPACITY, 32, 1, values);

	values[0] = (!common->wcmMaxZ) ? 0 : common->wcmThreshold;
	prop_threshold = InitWcmAtom(local->dev, WACOM_PROP_PRESSURE_THRESHOLD, 32, 1, values);

	values[0] = common->wcmSuppress;
	values[1] = common->wcmRawSample;
	prop_suppress = InitWcmAtom(local->dev, WACOM_PROP_SAMPLE, 32, 2, values);

	values[0] = common->wcmTouch;
	prop_touch = InitWcmAtom(local->dev, WACOM_PROP_TOUCH, 8, 1, values);

	values[0] = !common->wcmTPCButton;
	prop_hover = InitWcmAtom(local->dev, WACOM_PROP_HOVER, 8, 1, values);


	values[0] = MakeAtom(local->type_name, strlen(local->type_name), TRUE);
	prop_tooltype = InitWcmAtom(local->dev, WACOM_PROP_TOOL_TYPE, -32, 1, values);

	/* default to no actions */
	memset(values, 0, sizeof(values));
	prop_btnactions = InitWcmAtom(local->dev, WACOM_PROP_BUTTON_ACTIONS, -32, WCM_MAX_MOUSE_BUTTONS, values);

#ifdef DEBUG
	values[0] = priv->debugLevel;
	values[1] = common->debugLevel;
	prop_debuglevels = InitWcmAtom(local->dev, WACOM_PROP_DEBUGLEVELS, 8, 2, values);
#endif
}

int wcmSetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop,
		BOOL checkonly)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;

	DBG(10, priv, "\n");

	if (property == prop_tablet_area)
	{
		INT32 *values = (INT32*)prop->data;
		WacomToolAreaPtr area = priv->toolarea;

		if (prop->size != 4 || prop->format != 32)
			return BadValue;

		/* value validation is unnecessary since we let utility programs, such as
		 * xsetwacom and userland control panel take care of the validation role.
		 * when all four values are set to -1, it is an area reset (xydefault) */
		if ((values[0] != -1) || (values[1] != -1) ||
				(values[2] != -1) || (values[3] != -1))
		{
			WacomToolArea tmp_area = *area;

			area->topX = values[0];
			area->topY = values[1];
			area->bottomX = values[2];
			area->bottomY = values[3];

			/* validate the area */
			if (wcmAreaListOverlap(area, priv->tool->arealist))
			{
				*area = tmp_area;
				return BadValue;
			}
			*area = tmp_area;
		}

		if (!checkonly)
		{
			if ((values[0] == -1) && (values[1] == -1) &&
					(values[2] == -1) && (values[3] == -1))
			{
				values[0] = 0;
				values[1] = 0;
				values[2] = priv->maxX;
				values[3] = priv->maxY;
			}

			priv->topX = area->topX = values[0];
			priv->topY = area->topY = values[1];
			priv->bottomX = area->bottomX = values[2];
			priv->bottomY = area->bottomY = values[3];
			wcmInitialCoordinates(local, 0);
			wcmInitialCoordinates(local, 1);
		}
	} else if (property == prop_pressurecurve)
	{
		INT32 *pcurve;

		if (prop->size != 4 || prop->format != 32)
			return BadValue;

		pcurve = (INT32*)prop->data;

		if (!wcmCheckPressureCurveValues(pcurve[0], pcurve[1],
						 pcurve[2], pcurve[3]))
			return BadValue;

		if (IsCursor(priv) || IsPad (priv) || IsTouch (priv))
			return BadValue;

		if (!checkonly)
			wcmSetPressureCurve (priv, pcurve[0], pcurve[1],
					pcurve[2], pcurve[3]);
	} else if (property == prop_suppress)
	{
		CARD32 *values;

		if (prop->size != 2 || prop->format != 32)
			return BadValue;

		values = (CARD32*)prop->data;

		if ((values[0] < 0) || (values[0] > 100))
			return BadValue;

		if ((values[1] < 0) || (values[1] > XWACOM_MAX_SAMPLES))
			return BadValue;

		if (!checkonly)
		{
			common->wcmSuppress = values[0];
			common->wcmRawSample = values[1];
		}
	} else if (property == prop_rotation)
	{
		CARD8 value;
		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		value = *(CARD8*)prop->data;

		if (value > 3)
			return BadValue;

		if (!checkonly && common->wcmRotate != value)
			wcmRotateTablet(local, value);
	} else if (property == prop_serials)
	{
		return BadValue; /* Read-only */
	} else if (property == prop_strip_buttons)
	{
		CARD8 *values;

		if (prop->size != 4 || prop->format != 8)
			return BadValue;

		values = (CARD8*)prop->data;

		if (values[0] > WCM_MAX_MOUSE_BUTTONS ||
				values[1] > WCM_MAX_MOUSE_BUTTONS ||
				values[2] > WCM_MAX_MOUSE_BUTTONS ||
				values[3] > WCM_MAX_MOUSE_BUTTONS)
			return BadValue;

		if (!checkonly)
		{
			/* FIXME: needs to take AC_* into account */
			priv->striplup = values[0];
			priv->stripldn = values[1];
			priv->striprup = values[2];
			priv->striprdn = values[3];
		}

	} else if (property == prop_wheel_buttons)
	{
		CARD8 *values;

		if (prop->size != 4 || prop->format != 8)
			return BadValue;

		values = (CARD8*)prop->data;

		if (values[0] > WCM_MAX_MOUSE_BUTTONS ||
				values[1] > WCM_MAX_MOUSE_BUTTONS ||
				values[2] > WCM_MAX_MOUSE_BUTTONS ||
				values[3] > WCM_MAX_MOUSE_BUTTONS)
			return BadValue;

		if (!checkonly)
		{
			/* FIXME: needs to take AC_* into account */
			priv->relup = values[0];
			priv->reldn = values[1];
			priv->wheelup = values[2];
			priv->wheeldn = values[3];
		}
	} else if (property == prop_screen)
	{
		/* Long-term, this property should be removed, there's other ways to
		 * get the screen resolution. For now, we leave it in for backwards
		 * compat */
		return BadValue; /* Read-only */
	} else if (property == prop_display)
	{
		INT8 *values;

		if (prop->size != 3 || prop->format != 8)
			return BadValue;

		values = (INT8*)prop->data;

		if (values[0] < -1 || values[0] >= priv->numScreen)
			return BadValue;

		if (values[1] < TV_NONE || values[1] > TV_MAX)
			return BadValue;

		if ((values[2] != 0) && (values[2] != 1))
			return BadValue;

		if (!checkonly)
		{
			if (priv->screen_no != values[0])
				wcmChangeScreen(local, values[0]);
			priv->screen_no = values[0];

			if (priv->twinview != values[1])
			{
				int screen = priv->screen_no;
				priv->twinview = values[1];

				/* Can not restrict the cursor to a particular screen */
				if (!values[1] && (screenInfo.numScreens == 1))
				{
					screen = -1;
					priv->currentScreen = 0;
					DBG(10, priv, "TwinView sets to "
							"TV_NONE: can't change screen_no. \n");
				}
				wcmChangeScreen(local, screen);
			}

			priv->wcmMMonitor = values[2];
		}
	} else if (property == prop_cursorprox)
	{
		CARD32 value;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		if (!IsCursor (priv))
			return BadValue;

		value = *(CARD32*)prop->data;

		if (value > 255)
			return BadValue;

		if (!checkonly)
			common->wcmCursorProxoutDist = value;
	} else if (property == prop_capacity)
	{
		INT32 value;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		value = *(INT32*)prop->data;

		if ((value < -1) || (value > 5))
			return BadValue;

		if (!checkonly)
			common->wcmCapacity = value;

	} else if (property == prop_threshold)
	{
		CARD32 value;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		value = *(CARD32*)prop->data;

		if ((value < 1) || (value > common->wcmMaxZ))
			return BadValue;

		if (!checkonly)
			common->wcmThreshold = value;
	} else if (property == prop_touch)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!checkonly && common->wcmTouch != values[0])
			common->wcmTouch = values[0];
	} else if (property == prop_hover)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!checkonly && common->wcmTPCButton != !values[0])
			common->wcmTPCButton = !values[0];
	} else if (property == prop_tv_resolutions)
	{
		CARD32 *values;

		if (prop->size != 4 || prop->format != 32)
			return BadValue;

		values = (CARD32*)prop->data;

		/* non-TwinView settings can not set TwinView RESOLUTION */
		switch(priv->twinview)
		{
			case TV_NONE:
				return BadValue;
			case TV_ABOVE_BELOW:
			case TV_BELOW_ABOVE:
				      if ((values[1] + values[3]) != screenInfo.screens[0]->height)
					      return BadValue;
				      break;
			case TV_LEFT_RIGHT:
			case TV_RIGHT_LEFT:
				      if ((values[0] + values[2]) != screenInfo.screens[0]->width)
					      return BadValue;
				      break;
		}

		if (!checkonly)
		{
			priv->tvResolution[0] = values[0];
			priv->tvResolution[1] = values[1];
			priv->tvResolution[2] = values[2];
			priv->tvResolution[3] = values[3];

			/* reset screen info */
			wcmChangeScreen(local, priv->screen_no);
		}
#ifdef DEBUG
	} else if (property == prop_debuglevels)
	{
		CARD8 *values;

		if (prop->size != 2 || prop->format != 8)
			return BadMatch;

		values = (CARD8*)prop->data;
		if (values[0] > 10 || values[1] > 10)
			return BadValue;

		if (!checkonly)
		{
			priv->debugLevel = values[0];
			common->debugLevel = values[1];
		}
#endif
	} else if (property == prop_btnactions)
	{
		Atom *values;
		int i, j;
		XIPropertyValuePtr val;

		if (prop->size != WCM_MAX_MOUSE_BUTTONS || prop->format != 32 ||
				prop->type != XA_ATOM)
			return BadMatch;

		/* How this works:
		 * prop_btnactions has a list of atoms stored. Any atom references
		 * another property on that device that contains the actual action.
		 * If this property changes, all action-properties are queried for
		 * their value and their value is stored in priv->key[button].
		 *
		 * If the button is pressed, the actions are executed.
		 *
		 * Any button action property needs to be monitored by this property
		 * handler too.
		 */

		values = (Atom*)prop->data;

		for (i = 0; i < prop->size; i++)
		{
			if (!values[i])
				continue;

			if (values[i] == property || !ValidAtom(values[i]))
				return BadValue;

			if (XIGetDeviceProperty(local->dev, values[i], &val) != Success)
				return BadValue;
		}

		if (!checkonly)
		{
			/* any action property needs to be registered for this handler. */
			for (i = 0; i < prop->size; i++)
				priv->btn_actions[i] = values[i];

			for (i = 0; i < prop->size; i++)
			{
				if (!values[i])
					continue;

				XIGetDeviceProperty(local->dev, values[i], &val);

				memset(priv->keys[i], 0, sizeof(priv->keys[i]));
				for (j = 0; j < val->size; j++)
					priv->keys[i][j] = ((unsigned int*)val->data)[j];
			}

		}
	} else
	{
		int i, j;

		/* check all properties used for button actions */
		for (i = 0; i < ARRAY_SIZE(priv->btn_actions); i++)
			if (priv->btn_actions[i] == property)
				break;

		if (i < ARRAY_SIZE(priv->btn_actions))
		{
			CARD32 *data;
			int code;
			int type;

			if (prop->size >= 255 || prop->format != 32 ||
					prop->type != XA_INTEGER)
				return BadMatch;

			data = (CARD32*)prop->data;

			for (j = 0;j < prop->size; j++)
			{
				code = data[j] & AC_CODE;
				type = data[j] & AC_TYPE;

				switch(type)
				{
					case AC_KEY:
						break;
					case AC_BUTTON:
						if (code > WCM_MAX_MOUSE_BUTTONS)
							return BadValue;
						break;
					case AC_DISPLAYTOGGLE:
					case AC_MODETOGGLE:
					case AC_DBLCLICK:
						break;
					default:
						return BadValue;
				}

				if (!checkonly)
				{
					memset(priv->keys[i], 0, sizeof(priv->keys[i]));
					for (j = 0; j < prop->size; j++)
						priv->keys[i][j] = data[j];
				}
			}
		}
	}

	return Success;
}
#endif /* GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3 */
/* vim: set noexpandtab shiftwidth=8: */
