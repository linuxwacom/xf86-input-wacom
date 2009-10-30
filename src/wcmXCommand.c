/*
 * Copyright 2007-2009 by Ping Cheng, Wacom Technology. <pingc@wacom.com>
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

/*
 * REVISION HISTORY
 *
 * 2009-05-18 0.1 - Initial release as xf86-input-wacom project for xorg 1.6 and hal
 */


/****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "xf86Wacom.h"
#include "wcmFilter.h"

extern void xf86WcmInitialCoordinates(LocalDevicePtr local, int axes);
extern void xf86WcmRotateTablet(LocalDevicePtr local, int value);
extern void xf86WcmInitialScreens(LocalDevicePtr local);

int xf86WcmDevSwitchModeCall(LocalDevicePtr local, int mode);
int xf86WcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode);
void xf86WcmChangeScreen(LocalDevicePtr local, int value);

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3
	int xf86WcmSetProperty(DeviceIntPtr dev, Atom property, 
		XIPropertyValuePtr prop, BOOL checkonly);
	void InitWcmDeviceProperties(LocalDevicePtr local);
#endif

/*****************************************************************************
 * xf86WcmSetPadCoreMode
 ****************************************************************************/

int xf86WcmSetPadCoreMode(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	int is_core = local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER);

	/* Pad is always in relative mode when it's a core device.
	 * Always in absolute mode when it is not a core device.
	 */
	DBG(10, priv->debugLevel, ErrorF("xf86WcmSetPadCoreMode (%p)"
		" is always in %s mode when it %s core device\n",
		(void *)local->dev, 
		!is_core ? "absolute" : "relative", 
		is_core ? "is" : "isn't"));
	if (is_core)
		priv->flags &= ~ABSOLUTE_FLAG;
	else
		priv->flags |= ABSOLUTE_FLAG;
	return Success;
}

/*****************************************************************************
* xf86WcmDevSwitchModeCall --
*****************************************************************************/

int xf86WcmDevSwitchModeCall(LocalDevicePtr local, int mode)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	int is_absolute = priv->flags & ABSOLUTE_FLAG;

	DBG(3, priv->debugLevel, ErrorF("xf86WcmSwitchModeCall for %s to mode=%d\n", 
		local->name, mode));

	/* Pad is always in relative mode when it's a core device.
	 * Always in absolute mode when it is not a core device.
	 */
	if (IsPad(priv))
		return xf86WcmSetPadCoreMode(local);

	if ((mode == Absolute) && !is_absolute)
	{
		priv->flags |= ABSOLUTE_FLAG;
		xf86ReplaceStrOption(local->options, "Mode", "Absolute");
		xf86WcmMappingFactor(local);
		xf86WcmInitialCoordinates(local, 0);
		xf86WcmInitialCoordinates(local, 1);
	}
	else if ((mode == Relative) && is_absolute)
	{
		priv->flags &= ~ABSOLUTE_FLAG; 
		xf86ReplaceStrOption(local->options, "Mode", "Relative");
		xf86WcmMappingFactor(local);
		xf86WcmInitialCoordinates(local, 0);
		xf86WcmInitialCoordinates(local, 1);
	}
	else if ( (mode != Absolute) && (mode != Relative))
	{
		DBG(10, priv->debugLevel, ErrorF("xf86WcmSwitchModeCall"
			" for %s invalid mode=%d\n", local->name, mode));
		return BadMatch;
	}

	return Success;
}

/*****************************************************************************
* xf86WcmDevSwitchMode --
*****************************************************************************/

int xf86WcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
	LocalDevicePtr local = (LocalDevicePtr)dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	DBG(3, priv->debugLevel, ErrorF("xf86WcmSwitchMode dev=%p mode=%d\n", 
		(void *)dev, mode));

	/* Share this call with sendAButton in wcmCommon.c */
	return xf86WcmDevSwitchModeCall(local, mode);
}

/*****************************************************************************
 * xf86WcmChangeScreen
 ****************************************************************************/

void xf86WcmChangeScreen(LocalDevicePtr local, int value)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	if (priv->screen_no != value)
	{
		priv->screen_no = value;
		xf86ReplaceIntOption(local->options, "ScreenNo", value);
	}

	if (priv->screen_no != -1)
		priv->currentScreen = priv->screen_no;
	xf86WcmInitialScreens(local);
	xf86WcmInitialCoordinates(local, 0);
	xf86WcmInitialCoordinates(local, 1);
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

static Atom InitWcmAtom(DeviceIntPtr dev, char *name, int format, int nvalues, int *values)
{
    int i;
    Atom atom;
    uint8_t val_8[4];
    uint16_t val_16[4];
    uint32_t val_32[4];
    pointer converted = val_32;

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
    XIChangeDeviceProperty(dev, atom, XA_INTEGER, format,
                           PropModeReplace, nvalues,
                           converted, FALSE);
    XISetDevicePropertyDeletable(dev, atom, FALSE);
    return atom;
}

void InitWcmDeviceProperties(LocalDevicePtr local)
{
    WacomDevicePtr priv = (WacomDevicePtr) local->private;
    WacomCommonPtr common = priv->common;
    int values[9];

    DBG(10, priv->debugLevel, ErrorF("InitWcmDeviceProperties for %s \n", local->name));

    values[0] = priv->topX;
    values[1] = priv->topY;
    values[2] = priv->bottomX;
    values[3] = priv->bottomY;
    prop_tablet_area = InitWcmAtom(local->dev, "Wacom Tablet Area", 32, 4, values);

    values[0] = common->wcmRotate;
    prop_rotation = InitWcmAtom(local->dev, "Wacom Rotation", 8, 1, values);

    values[0] = 0;
    values[1] = 0;
    values[2] = 100;
    values[3] = 100;
    prop_pressurecurve = InitWcmAtom(local->dev, "Wacom Pressurecurve", 32, 4, values);

    values[0] = common->tablet_id;
    values[1] = priv->old_serial;
    values[2] = priv->old_device_id;
    values[3] = priv->serial;
    prop_serials = InitWcmAtom(local->dev, "Wacom Serial IDs", 32, 4, values);

    values[0] = priv->striplup;
    values[1] = priv->stripldn;
    values[2] = priv->striprup;
    values[3] = priv->striprdn;
    prop_strip_buttons = InitWcmAtom(local->dev, "Wacom Strip Buttons", 8, 4, values);

    values[0] = priv->relup;
    values[1] = priv->reldn;
    values[2] = priv->wheelup;
    values[3] = priv->wheeldn;
    prop_wheel_buttons = InitWcmAtom(local->dev, "Wacom Wheel Buttons", 8, 4, values);

    values[0] = priv->tvResolution[0];
    values[1] = priv->tvResolution[1];
    values[2] = priv->tvResolution[2];
    values[3] = priv->tvResolution[3];
    prop_tv_resolutions = InitWcmAtom(local->dev, "Wacom TwinView Resolution", 32, 4, values);


    values[0] = priv->screen_no;
    values[1] = priv->twinview;
    values[2] = priv->wcmMMonitor;
    prop_display = InitWcmAtom(local->dev, "Wacom Display Options", 8, 3, values);

    values[0] = priv->screenTopX[priv->currentScreen];
    values[1] = priv->screenTopY[priv->currentScreen];
    values[2] = priv->screenBottomX[priv->currentScreen];
    values[3] = priv->screenBottomY[priv->currentScreen];
    prop_screen = InitWcmAtom(local->dev, "Wacom Screen Area", 32, 4, values);

    values[0] = common->wcmCursorProxoutDist;
    prop_cursorprox = InitWcmAtom(local->dev, "Wacom Proximity Threshold", 32, 1, values);

    values[0] = common->wcmCapacity;
    prop_capacity = InitWcmAtom(local->dev, "Wacom Capacity", 32, 1, values);

    values[0] = (!common->wcmMaxZ) ? 0 : common->wcmThreshold;
    prop_threshold = InitWcmAtom(local->dev, "Wacom Pressure Threshold", 32, 1, values);

    values[0] = common->wcmSuppress;
    values[1] = common->wcmRawSample;
    prop_suppress = InitWcmAtom(local->dev, "Wacom Sample and Suppress", 32, 2, values);

    values[0] = common->wcmTouch;
    prop_touch = InitWcmAtom(local->dev, "Wacom Enable Touch", 8, 1, values);

    values[0] = !common->wcmTPCButton;
    prop_hover = InitWcmAtom(local->dev, "Wacom Hover Click", 8, 1, values);
}

int xf86WcmSetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop,
            BOOL checkonly)
{
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    WacomDevicePtr priv = (WacomDevicePtr) local->private;
    WacomCommonPtr common = priv->common;

    DBG(10, priv->debugLevel, ErrorF("xf86WcmSetProperty for %s \n", local->name));

    if (property == prop_tablet_area)
    {
        INT32 *values = (INT32*)prop->data;
        WacomToolArea area;

        if (prop->size != 4 || prop->format != 32)
            return BadValue;

        area.topX = values[0];
        area.topY = values[1];
        area.bottomX = values[2];
        area.bottomY = values[3];

        if (xf86WcmAreaListOverlap(&area, priv->tool->arealist))
            return BadValue;

        if (!checkonly)
        {
            /* Invalid range resets axis to defaults */
            if (values[0] >= values[2])
            {
                values[0] = 0;
                values[2] = priv->wcmMaxX;
            }

            if (values[1] >= values[3]);
            {
                values[1] = 0;
                values[3] = priv->wcmMaxY;
            }

            priv->topX = values[0];
            priv->topY = values[1];
            priv->bottomX = values[2];
            priv->bottomY = values[3];
            xf86WcmMappingFactor(local);
            xf86WcmInitialCoordinates(local, 0);
            xf86WcmInitialCoordinates(local, 1);
        }
    } else if (property == prop_pressurecurve)
    {
        INT32 *pcurve;

        if (prop->size != 4 || prop->format != 32)
            return BadValue;

        pcurve = (INT32*)prop->data;

        if ((pcurve[0] > 100) || (pcurve[1] > 100) ||
            (pcurve[2] > 100) || (pcurve[3] > 100))
            return BadValue;

        if (IsCursor(priv) || IsPad (priv) || IsTouch (priv))
            return BadValue;

        if (!checkonly)
            xf86WcmSetPressureCurve (priv, pcurve[0], pcurve[1],
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
            xf86WcmRotateTablet(local, value);
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
                xf86WcmChangeScreen(local, values[0]);
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
                    DBG(10, priv->debugLevel, ErrorF("xf86WcmSetParam TwinView sets to "
                        "TV_NONE: cann't change screen_no. \n"));
                }
                xf86WcmChangeScreen(local, screen);
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

        if ((value < 1) || (value > 21))
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

        if (!checkonly && common->wcmTouch != values[1])
	    common->wcmTouch = values[1];
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
        if ((priv->twinview == TV_NONE) || (values[0] < 0) ||
                (values[1] < 0) || (values[2] < 0) || (values[3] < 0) ||
		((values[0] + values[2]) != screenInfo.screens[0]->width) ||
		((values[1] + values[3]) != screenInfo.screens[0]->height))
            return BadValue;

        if (!checkonly)
        {
            priv->tvResolution[0] = values[0];
            priv->tvResolution[1] = values[1];
            priv->tvResolution[2] = values[2];
            priv->tvResolution[3] = values[3];

            /* reset screen info */
            xf86WcmChangeScreen(local, priv->screen_no);
        }
    }
    return Success;
}
#endif /* GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3 */
