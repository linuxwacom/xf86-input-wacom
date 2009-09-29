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

#include "xf86Wacom.h"
#include "wcmFilter.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

/*****************************************************************************
 * xf86WcmSetParam
 ****************************************************************************/

static int xf86WcmSetParam(LocalDevicePtr local, int param, int value)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	char st[32];

	/* We don't reset options to the values that the driver are using.  
	 * This eliminates confusion when driver is running on default values.
	 */
	switch (param)
	{
	    case XWACOM_PARAM_TOPX:
		if ( priv->topX != value)
		{
			/* check if value overlaps with existing ones */
			area->topX = value;
			if (xf86WcmAreaListOverlap(area, priv->tool->arealist))
			{
				area->topX = priv->topX;
				DBG(10, priv->debugLevel, ErrorF("xf86WcmSetParam TopX overlaps with another area \n"));
				return BadValue;
			}

			/* Area definition is ok */
			xf86ReplaceIntOption(local->options, "TopX", value);
			priv->topX = xf86SetIntOption(local->options, "TopX", 0);
			xf86WcmMappingFactor(local);
			xf86WcmInitialCoordinates(local, 0);
		}
		break;
	    case XWACOM_PARAM_TOPY:
		if ( priv->topY != value)
		{
			/* check if value overlaps with existing ones */
			area->topY = value;
			if (xf86WcmAreaListOverlap(area, priv->tool->arealist))
			{
				area->topY = priv->topY;
				DBG(10, priv->debugLevel, ErrorF("xf86WcmSetParam TopY overlap with another area \n"));
				return BadValue;
			}

			/* Area definition is ok */
			xf86ReplaceIntOption(local->options, "TopY", value);
			priv->topY = xf86SetIntOption(local->options, "TopY", 0);
			xf86WcmMappingFactor(local);
			xf86WcmInitialCoordinates(local, 1);
		}
		break;
	    case XWACOM_PARAM_BOTTOMX:
		if ( priv->bottomX != value)
		{
			/* check if value overlaps with existing ones */
			area->bottomX = value;
			if (xf86WcmAreaListOverlap(area, priv->tool->arealist))
			{
				area->bottomX = priv->bottomX;
				DBG(10, priv->debugLevel, ErrorF("xf86WcmSetParam BottomX overlap with another area \n"));
				return BadValue;
			}

			/* Area definition is ok */
			xf86ReplaceIntOption(local->options, "BottomX", value);
			priv->bottomX = xf86SetIntOption(local->options, "BottomX", 0);
			xf86WcmMappingFactor(local);
			xf86WcmInitialCoordinates(local, 0);
		}
		break;
	    case XWACOM_PARAM_BOTTOMY:
		if ( priv->bottomY != value)
		{
			/* check if value overlaps with existing ones */
			area->bottomY = value;
			if (xf86WcmAreaListOverlap(area, priv->tool->arealist))
			{
				area->bottomY = priv->bottomY;
				DBG(10, priv->debugLevel, ErrorF("xf86WcmSetParam BottomY overlap with another area \n"));
				return BadValue;
			}

			/* Area definition is ok */
			xf86ReplaceIntOption(local->options, "BottomY", value);
			priv->bottomY = xf86SetIntOption(local->options, "BottomY", 0);
			xf86WcmMappingFactor(local);
			xf86WcmInitialCoordinates(local, 1);
		}
		break;
	    case XWACOM_PARAM_SUPPRESS:
		if ((value < 0) || (value > 100)) return BadValue;
		if (common->wcmSuppress != value)
		{
			xf86ReplaceIntOption(local->options, "Suppress", value);
			common->wcmSuppress = value;
		}
		break;
	    case XWACOM_PARAM_RAWSAMPLE:
		if ((value < 1) || (value > XWACOM_MAX_SAMPLES)) return BadValue;
		if (common->wcmRawSample != value)
		{
			common->wcmRawSample = value;
		}
		break;
	    case XWACOM_PARAM_RAWFILTER:
		if ((value < 0) || (value > 1)) return BadValue;
		if (value) 
		{
			common->wcmFlags |= RAW_FILTERING_FLAG;
			xf86ReplaceStrOption(local->options, "RawFilter", "On");
		}
		else 
		{
			common->wcmFlags &= ~(RAW_FILTERING_FLAG);
			xf86ReplaceStrOption(local->options, "RawFilter", "Off");
		}
		break;
	    case XWACOM_PARAM_PRESSCURVE:
	    {
		if ( !IsCursor(priv) && !IsPad (priv) && !IsTouch (priv)) 
		{
			char chBuf[64];
			int x0 = (value >> 24) & 0xFF;
			int y0 = (value >> 16) & 0xFF;
			int x1 = (value >> 8) & 0xFF;
			int y1 = value & 0xFF;
			if ((x0 > 100) || (y0 > 100) || (x1 > 100) || (y1 > 100))
			    return BadValue;
			snprintf(chBuf,sizeof(chBuf),"%d,%d,%d,%d",x0,y0,x1,y1);
			xf86ReplaceStrOption(local->options, "PressCurve",chBuf);
			xf86WcmSetPressureCurve(priv,x0,y0,x1,y1);
		}
		break;
	    }
	    case XWACOM_PARAM_CLICKFORCE:
		if ((value < 1) || (value > 21)) return BadValue;
		common->wcmThreshold = (int)((double)
				(value*common->wcmMaxZ)/100.00+0.5);
		xf86ReplaceIntOption(local->options, "Threshold", 
				common->wcmThreshold);
		break;
	    case XWACOM_PARAM_THRESHOLD:
		common->wcmThreshold = value;
		xf86ReplaceIntOption(local->options, "Threshold", 
				common->wcmThreshold);
		break;
	    case XWACOM_PARAM_SERIAL:
		if (common->wcmProtocolLevel != 5)
			return BadValue;
		if (priv->serial != value)
		{
			priv->serial = value; 
			xf86ReplaceIntOption(local->options, "Serial", priv->serial);
		}
		break;
	    case XWACOM_PARAM_MMT:
		if ((value != 0) && (value != 1)) 
			return BadValue;
		else if (priv->wcmMMonitor != value)
		{
			priv->wcmMMonitor = value;
			if (value)
				xf86ReplaceStrOption(local->options, "MMonitor", "on");
			else
				xf86ReplaceStrOption(local->options, "MMonitor", "off");
			
			xf86WcmMappingFactor(local);
		}
		break;
	    case XWACOM_PARAM_TPCBUTTON:
		if ((value != 0) && (value != 1)) 
			return BadValue;
		else if (common->wcmTPCButton != value)
		{
			common->wcmTPCButton = value;
			if (value)
				xf86ReplaceStrOption(local->options, "TPCButton", "on");
			else
				xf86ReplaceStrOption(local->options, "TPCButton", "off");
		}
		break;
	    case XWACOM_PARAM_TOUCH:
		if ((value != 0) && (value != 1)) 
			return BadValue;
		else if (common->wcmTouch != value)
		{
			common->wcmTouch = value;
			if (value)
				xf86ReplaceStrOption(local->options, "Touch", "on");
			else
				xf86ReplaceStrOption(local->options, "Touch", "off");
		}
		break;
	    case XWACOM_PARAM_CAPACITY:
		if ((value < -1) || (value > 5)) 
			return BadValue;
		else if (common->wcmCapacity != value)
		{
			common->wcmCapacity = value;
			xf86ReplaceIntOption(local->options, "Capacity", value);
		}
		break;
	    case XWACOM_PARAM_CURSORPROX:
		if (IsCursor (priv))
		{
			if ((value > 255) || (value < 0))
				return BadValue;
			else if (common->wcmCursorProxoutDist != value)
			{
				xf86ReplaceIntOption(local->options, "CursorProx",value);
				common->wcmCursorProxoutDist = value;
			}
		}
		break;
	    case XWACOM_PARAM_SCREEN_NO:
		if (value < -1 || value >= priv->numScreen) 
			return BadValue;
		else if (priv->screen_no != value)
			xf86WcmChangeScreen(local, value);
		break;
	    case XWACOM_PARAM_TWINVIEW:
		if (priv->twinview != value)
		{
			if ((value > TV_MAX) || (value < TV_NONE) || screenInfo.numScreens != 1)
				return BadValue;
			priv->twinview = value;

			/* Can not restrict the cursor to a particular screen */
			if (!value)
			{
				value = -1;
				priv->currentScreen = 0;
				DBG(10, priv->debugLevel, ErrorF("xf86WcmSetParam(TWINVIEW) TwinView sets to "
					"TV_NONE: cann't change screen_no. \n"));
			}
			else
				value = priv->screen_no;

			xf86WcmChangeScreen(local, value);
		}
		break;
	    case XWACOM_PARAM_TVRESOLUTION0:
	    case XWACOM_PARAM_TVRESOLUTION1:
	    {
		if (priv->twinview == TV_NONE)
			return -1;
		else
		{
			int sNum = param - XWACOM_PARAM_TVRESOLUTION0;
			int rX = value & 0xffff, rY = (value >> 16) & 0xffff;
			if (rX > screenInfo.screens[0]->width ||
					rY > screenInfo.screens[0]->height)
			{
				ErrorF("xf86WcmSetParam tvResolution out of range: " 
					"ResX=%d ResY=%d \n", rX, rY);
				return BadValue;
			}

			DBG(10, priv->debugLevel, ErrorF("xf86WcmSetParam " 
				"tvResolutionX from %d to ResX=%d tvResolutionY "
				"from %d toResY=%d \n",
				priv->tvResolution[0], rX, priv->tvResolution[1], rY));

			if ( priv->twinview == TV_ABOVE_BELOW )
			{
				if (sNum)
				{
					priv->tvResolution[1] = screenInfo.screens[0]->height - rY;
					priv->tvResolution[2] = rX;
					priv->tvResolution[3] = rY;
				}
				else
				{
					priv->tvResolution[0] = rX;
					priv->tvResolution[1] = rY;
					priv->tvResolution[3] = screenInfo.screens[0]->height - rY;
				}
			}
			else
			{
				if (sNum)
				{
					priv->tvResolution[0] = screenInfo.screens[0]->width - rX;
					priv->tvResolution[2] = rX;
					priv->tvResolution[3] = rY;
				}
				else
				{
					priv->tvResolution[0] = rX;
					priv->tvResolution[1] = rY;
					priv->tvResolution[2] = screenInfo.screens[0]->width - rX;
				}
			}
		}

		/* reset screen info */
		xf86WcmChangeScreen(local, priv->screen_no);
		break;
	    }
	   case XWACOM_PARAM_ROTATE:
		if ((value < 0) || (value > 3)) return BadValue;
		if (common->wcmRotate != value)
			xf86WcmRotateTablet(local, value);
		break;
	   case XWACOM_PARAM_XSCALING:
		if ((value != 0) || (value != 1)) return BadValue;
		if (common->wcmScaling != value)
			common->wcmScaling = value;
		break;
	   default:
		DBG(10, priv->debugLevel, ErrorF("xf86WcmSetParam invalid param %d\n",param));
		return BadMatch;
	}
	return Success;
}

/*****************************************************************************
 * xf86WcmSetButtonParam
 ****************************************************************************/

static int xf86WcmSetButtonParam(LocalDevicePtr local, int param, int value)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	static int button_keys = 0, number_keys = 0;
	int *setVal = 0, bn = param - XWACOM_PARAM_BUTTON1;
	unsigned  *keyP = 0;
	char st[32];

	if (param >= XWACOM_PARAM_BUTTON1 && param <= XWACOM_PARAM_BUTTON32)
	{
		if (bn > priv->nbuttons && bn > common->npadkeys)
			return BadValue;
		else
		{
			if (((value & AC_TYPE) == AC_BUTTON) && (value != priv->button[bn]) && !number_keys )
			{
				/* assign button */
				snprintf (st, sizeof (st), "Button%d", bn);
				xf86ReplaceIntOption (local->options, st, value);
				priv->button[bn] = xf86SetIntOption (local->options, st, bn); 
			} 
			else
			{
				setVal = &(priv->button [bn]);
				keyP = priv->keys[bn];
			}
		}
	}

	switch (param)
	{
	   case XWACOM_PARAM_RELWUP:
		setVal = &(priv->relup);
		keyP = priv->rupk;
		break;
	   case XWACOM_PARAM_RELWDN:
		setVal = &(priv->reldn);
		keyP = priv->rdnk;
		break;
	   case XWACOM_PARAM_ABSWUP:
		setVal = &(priv->wheelup);
		keyP = priv->wupk;
		break;
	   case XWACOM_PARAM_ABSWDN:
		setVal = &(priv->wheeldn);
		keyP = priv->wdnk;
		break;
	   case XWACOM_PARAM_STRIPLUP:
		setVal = &(priv->striplup);
		keyP = priv->slupk;
		break;
	   case XWACOM_PARAM_STRIPLDN:
		setVal = &(priv->stripldn);
		keyP = priv->sldnk;
		break;
	   case XWACOM_PARAM_STRIPRUP:
		setVal = &(priv->striprup);
		keyP = priv->srupk;
		break;
	   case XWACOM_PARAM_STRIPRDN:
		setVal = &(priv->striprdn);
		keyP = priv->srdnk;
		break;
	}
	/* assign keys */
	if (keyP)
	{
		if (!number_keys)
		{
			*setVal = value;
			number_keys = (value & AC_NUM_KEYS) >> 20;
			DBG(10, priv->debugLevel, ErrorF(
				"xf86WcmSetButtonParam value = %x number"
				" of keys = %d\n", *setVal, number_keys));
			if (number_keys)
				keyP[button_keys++] = value & 0xffff;
		}
		else
		{
			if (button_keys < number_keys)
			{
				keyP[button_keys++] = value & 0xffff;
				keyP[button_keys++] = (value & 0xffff0000) >> 16;
				DBG(10, priv->debugLevel, ErrorF(
					"xf86WcmSetButtonParam got %d values = %x \n",
					 button_keys, value));
			}
		}
		if (button_keys >= number_keys)
			button_keys = number_keys = 0;
	}
	return Success;
}

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3

static Atom InitWcmAtom(DeviceIntPtr dev, char *name, int format, int nvalues, int *values)
{
    int i;
    Atom atom;
    uint8_t val_8[4];
    uint16_t val_16[4];
    uint32_t val_32[4];
    pointer converted;

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
	int i = 0, j = 0;

	DBG(10, priv->debugLevel, ErrorF("InitWcmDeviceProperties for %s \n", local->name));

	priv->gPropInfo[i] = (PROPINFO) { 0, "TOPX", XWACOM_PARAM_TOPX, 32, 1, priv->topX };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "TOPY", XWACOM_PARAM_TOPY, 32, 1, priv->topY };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "BOTTOMX", XWACOM_PARAM_BOTTOMX, 32, 1, priv->bottomX };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "BOTTOMY", XWACOM_PARAM_BOTTOMY, 32, 1, priv->bottomY };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "PRESSCURVE", XWACOM_PARAM_PRESSCURVE, 32, 1, ((100 << 8) | 100) };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "TPCBUTTON", XWACOM_PARAM_TPCBUTTON, 8, 1, common->wcmTPCButton };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "TOUCH", XWACOM_PARAM_TOUCH, 8, 1, common->wcmTouch };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "CURSORPROX", XWACOM_PARAM_CURSORPROX, 16, 1, common->wcmCursorProxoutDist };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "ROTATE", XWACOM_PARAM_ROTATE, 8, 1, common->wcmRotate };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "TWINVIEW", XWACOM_PARAM_TWINVIEW, 8, 1, priv->twinview };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "SUPPRESS", XWACOM_PARAM_SUPPRESS, 8, 1, common->wcmSuppress };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "SCREEN_NO", XWACOM_PARAM_SCREEN_NO, 8, 1, priv->screen_no };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "RAWSAMPLE", XWACOM_PARAM_RAWSAMPLE, 8, 1, common->wcmRawSample };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "CAPACITY", XWACOM_PARAM_CAPACITY, 8, 1, common->wcmCapacity };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "RELWUP", XWACOM_PARAM_RELWUP, 32, 1, priv->relup };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "RELWDN", XWACOM_PARAM_RELWDN, 32, 1, priv->reldn };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "ABSWUP", XWACOM_PARAM_ABSWUP, 32, 1, priv->wheelup };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "ABSWDN", XWACOM_PARAM_ABSWDN, 32, 1, priv->wheeldn };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "STRIPLUP", XWACOM_PARAM_STRIPLUP, 32, 1, priv->striplup };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "STRIPLDN", XWACOM_PARAM_STRIPLDN, 32, 1, priv->stripldn };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "STRIPRUP", XWACOM_PARAM_STRIPRUP, 32, 1, priv->striprup };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "STRIPRDN", XWACOM_PARAM_STRIPRDN, 32, 1, priv->striprdn };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "CLICKFORCE", XWACOM_PARAM_CLICKFORCE, 8, 1, 6 };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "XYDEFAULT", XWACOM_PARAM_XYDEFAULT, 8, 1, -1 };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "MMT", XWACOM_PARAM_MMT, 8, 1, priv->wcmMMonitor  };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "RAWFILTE", XWACOM_PARAM_RAWFILTER, 8, 1,
		((common->wcmFlags & RAW_FILTERING_FLAG) ? 1 : 0) };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "TVRESOLUTION0", XWACOM_PARAM_TVRESOLUTION0, 32, 1, 
		(priv->tvResolution[0] | (priv->tvResolution[1] << 16)) };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "TVRESOLUTION1", XWACOM_PARAM_TVRESOLUTION1, 32, 1, 
		(priv->tvResolution[2] | (priv->tvResolution[3] << 16)) };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "THRESHOLD", XWACOM_PARAM_THRESHOLD, 8, 1, 
		(!common->wcmMaxZ ? 0 : common->wcmThreshold) };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "TOOLSERIAL", XWACOM_PARAM_TOOLSERIAL, 32, 1,  priv->old_serial };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "TOOLID", XWACOM_PARAM_TOOLID, 16, 1, priv->old_device_id };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "TABLETID", XWACOM_PARAM_TID, 16, 1, common->tablet_id };
	priv->gPropInfo[++i] = (PROPINFO) { 0, "SERIAL", XWACOM_PARAM_SERIAL, 32, 1, priv->serial };

	/* this property may be needed for Nvidia Xinerama setup, which doesn't call DevConvert */
	priv->gPropInfo[++i] = (PROPINFO) { 0, "XSCALING", XWACOM_PARAM_XSCALING, 32, 1, common->wcmScaling };

	for (i=0; i<XWACOM_PARAM_MAXPARAM; i++)
	{
		DBG(10, priv->debugLevel, ErrorF("InitWcmDeviceProperties for %dth entry %s \n", 
			i, priv->gPropInfo[i].paramName));
		priv->gPropInfo[i].wcmProp = InitWcmAtom(local->dev, 
			(char *)priv->gPropInfo[i].paramName, 
			priv->gPropInfo[i].nFormat, priv->gPropInfo[i].nSize, 
			&priv->gPropInfo[i].nDefault);
	}
}

int xf86WcmSetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop,
            BOOL checkonly)
{
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    WacomDevicePtr priv = (WacomDevicePtr) local->private;
    int i = 0;

    DBG(10, priv->debugLevel, ErrorF("xf86WcmSetProperty for %s \n", local->name));

    /* If checkonly is set, no parameters may be changed. So just return. */
    if (checkonly)
    {
	return Success;
    }

    while (i<XWACOM_PARAM_MAXPARAM)
    {
	if ( priv->gPropInfo[i].wcmProp == property )
	{
        	if (prop->size != priv->gPropInfo[i].nSize || prop->format != priv->gPropInfo[i].nFormat || prop->type != XA_INTEGER)
			return BadMatch;

		if (priv->gPropInfo[i].nParamID >= XWACOM_PARAM_BUTTON1 && 
				priv->gPropInfo[i].nParamID <= XWACOM_PARAM_STRIPRDN)
			xf86WcmSetButtonParam (local, priv->gPropInfo[i].nParamID, 
				*(CARD32*)prop->data);
		else if (prop->format == 8)
			xf86WcmSetParam (local, priv->gPropInfo[i].nParamID, *(CARD8*)prop->data);
		else if (prop->format == 16)
			xf86WcmSetParam (local, priv->gPropInfo[i].nParamID, *(CARD16*)prop->data);
		else if (prop->format == 32)
			xf86WcmSetParam (local, priv->gPropInfo[i].nParamID, *(CARD32*)prop->data);
		else
			return BadMatch;
		i = XWACOM_PARAM_MAXPARAM;
	}
	else
		i++;
    } 
    return Success;
}
#endif /* GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3 */
