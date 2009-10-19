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
 * 2007-05-25 0.1 - Initial release - span off from xf86Wacom.c
 * 2008-05-14 0.2 - Rotate through routine xf86WcmRotateTablet
 * 2008-06-26 0.3 - Added Capacity
 * 2009-03-16 0.4 - Added leftOF for TwinView
 * 2009-04-21 0.5 - Added set serial option
 * 2009-09-31 0.6 - Added dual touch
 */


/****************************************************************************/

#include "xf86Wacom.h"
#include "wcmFilter.h"

extern void xf86WcmInitialCoordinates(LocalDevicePtr local, int axes);
extern void xf86WcmRotateTablet(LocalDevicePtr local, int value);
extern void xf86WcmInitialScreens(LocalDevicePtr local);

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
	DBG(10, priv->debugLevel, ErrorF("xf86WcmSetParam Pad (%p)"
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
	WacomToolAreaPtr area = priv->toolarea;

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
	    case XWACOM_PARAM_DEBUGLEVEL:
		if ((value < 0) || (value > 100)) return BadValue;
		if (priv->debugLevel != value)
		{
			xf86ReplaceIntOption(local->options, "DebugLevel", value);
			priv->debugLevel = value;
		}
		break;
	    case XWACOM_PARAM_COMMONDBG:
		if ((value < 0) || (value > 100)) return BadValue;
		if (common->debugLevel != value)
		{
			xf86ReplaceIntOption(local->options, "CommonDBG", value);
			common->debugLevel = value;
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
	    case XWACOM_PARAM_MODE:
	    {
		int is_absolute = priv->flags & ABSOLUTE_FLAG;
		if ((value < 0) || (value > 1)) return BadValue;
		if (value != is_absolute)
		{
			xf86WcmDevSwitchModeCall(local, value);
		}
		break;
	    }
	    case XWACOM_PARAM_SPEEDLEVEL:
		if ((value < 1) || (value > 11)) return BadValue;
		if (value > 6) priv->speed = 2.00*((double)value - 6.00);
		else priv->speed = ((double)value) / 6.00;
		sprintf(st, "%.3f", priv->speed);
		xf86AddNewOption(local->options, "Speed", st);
		break;
	    case XWACOM_PARAM_ACCEL:
		if ((value < 1) || (value > MAX_ACCEL)) 
			return BadValue;
		else if (priv->accel != value-1)
		{
			priv->accel = value-1;
			xf86ReplaceIntOption(local->options, "Accel", priv->accel);
		}
		break;
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
	    case XWACOM_PARAM_XYDEFAULT:
		xf86WcmSetParam (local, XWACOM_PARAM_TOPX, 0);
		xf86WcmSetParam (local, XWACOM_PARAM_TOPY, 0);
		xf86WcmSetParam (local, XWACOM_PARAM_BOTTOMX, priv->wcmMaxX);
		xf86WcmSetParam (local, XWACOM_PARAM_BOTTOMY, priv->wcmMaxY);
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
	    case XWACOM_PARAM_GESTURE:
		if ((value != 0) && (value != 1)) 
			return BadValue;
		else if (common->wcmGesture != value)
		{
			common->wcmGesture = value;
			if (value)
				xf86ReplaceStrOption(local->options, "Gesture", "on");
			else
				xf86ReplaceStrOption(local->options, "Gesture", "off");
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
			if ((value > TV_MAX) || (value < TV_NONE))
				return BadValue;
			priv->twinview = value;

			/* Can not restrict the cursor to a particular screen for TwinView case here */
			if (!value && (screenInfo.numScreens == 1))
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
		if (priv->twinview <= TV_XINERAMA)
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

			if ((priv->twinview == TV_ABOVE_BELOW) || (priv->twinview == TV_BELOW_ABOVE))
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
	    case XWACOM_PARAM_COREEVENT:
		if ((value != 0) && (value != 1)) return BadValue;
		/* Change the local flags. But not the configure file */
		if (value)
		{
			local->flags |= XI86_ALWAYS_CORE;
/*			xf86XInputSetSendCoreEvents (local, TRUE);
*/		}
		else
		{
			local->flags &= ~XI86_ALWAYS_CORE;
/*			xf86XInputSetSendCoreEvents (local, FALSE);
*/		}
		break;
	   case XWACOM_PARAM_ROTATE:
		if ((value < 0) || (value > 3)) return BadValue;
		if (common->wcmRotate != value)
			xf86WcmRotateTablet(local, value);
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

/*****************************************************************************
 * xf86WcmGetButtonParam
 ****************************************************************************/

static int xf86WcmGetButtonParam(LocalDevicePtr local, int param)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	static int button_keys = 0, number_keys = 0;
	int retVal = 0, bn = param - XWACOM_PARAM_BUTTON1;
	unsigned *keyP = 0;

	if (param >= XWACOM_PARAM_BUTTON1 && param <= XWACOM_PARAM_BUTTON32)
	{
		if (bn > priv->nbuttons && bn > common->npadkeys)
			return BadValue;
		else
		{
			retVal = priv->button [bn];
			keyP = priv->keys[bn];
		}
	}

	switch (param)
	{
	   case XWACOM_PARAM_RELWUP:
		retVal = priv->relup;
		keyP = priv->rupk;
		break;
	   case XWACOM_PARAM_RELWDN:
		retVal = priv->reldn;
		keyP = priv->rdnk;
		break;
	   case XWACOM_PARAM_ABSWUP:
		retVal = priv->wheelup;
		keyP = priv->wupk;
		break;
	   case XWACOM_PARAM_ABSWDN:
		retVal = priv->wheeldn;
		keyP = priv->wdnk;
		break;
	   case XWACOM_PARAM_STRIPLUP:
		retVal = priv->striplup;
		keyP = priv->slupk;
		break;
	   case XWACOM_PARAM_STRIPLDN:
		retVal = priv->stripldn;
		keyP = priv->sldnk;
		break;
	   case XWACOM_PARAM_STRIPRUP:
		retVal = priv->striprup;
		keyP = priv->srupk;
		break;
	   case XWACOM_PARAM_STRIPRDN:
		retVal = priv->striprdn;
		keyP = priv->srdnk;
		break;
	}
	DBG(10, priv->debugLevel, ErrorF("xf86WcmGetButtonParam value = %x\n", retVal));
	if (keyP)
	{
		if (!number_keys)
		{
			number_keys = (retVal & AC_NUM_KEYS) >> 20;
			if (number_keys)
				button_keys++;
		}
		else
		{
			if (button_keys < number_keys)
			{
				retVal = keyP[button_keys++];
				retVal |=  ((button_keys - number_keys) ? (keyP[button_keys++] << 16) : 0);
			}
		}
		if (button_keys >= number_keys)
			button_keys = number_keys = 0;
	}
	else
		retVal = BadValue;
	return retVal;
}

/*****************************************************************************
 * xf86WcmGetParam
 ****************************************************************************/

static int xf86WcmGetParam(LocalDevicePtr local, int param)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	DBG(10, common->debugLevel, ErrorF("xf86WcmGetParam param = %d\n", param));

	switch (param)
	{
	    case 0:
		return 1;
	    case XWACOM_PARAM_TOPX:
		return priv->topX;
	    case XWACOM_PARAM_TOPY:
		return priv->topY;
	    case XWACOM_PARAM_BOTTOMX:
		return priv->bottomX;
	    case XWACOM_PARAM_BOTTOMY:
		return priv->bottomY;
	    case XWACOM_PARAM_STOPX0:
	    case XWACOM_PARAM_STOPX1:
	    case XWACOM_PARAM_STOPX2:
	    case XWACOM_PARAM_STOPX3:
	    case XWACOM_PARAM_STOPX4:
	    case XWACOM_PARAM_STOPX5:
	    case XWACOM_PARAM_STOPX6:
	    case XWACOM_PARAM_STOPX7:
	    {
		int sn = (param - XWACOM_PARAM_STOPX0) / 4; 
		if (sn >= priv->numScreen)
			return -1;
		else
			return priv->screenTopX[sn];
	    }
	    case XWACOM_PARAM_STOPY0:
	    case XWACOM_PARAM_STOPY1:
	    case XWACOM_PARAM_STOPY2:
	    case XWACOM_PARAM_STOPY3:
	    case XWACOM_PARAM_STOPY4:
	    case XWACOM_PARAM_STOPY5:
	    case XWACOM_PARAM_STOPY6:
	    case XWACOM_PARAM_STOPY7:
	    {
		int sn = (param - XWACOM_PARAM_STOPY0) / 4; 
		if (sn >= priv->numScreen)
			return -1;
		else
			return priv->screenTopY[sn];
	    }
	    case XWACOM_PARAM_SBOTTOMX0:
	    case XWACOM_PARAM_SBOTTOMX1:
	    case XWACOM_PARAM_SBOTTOMX2:
	    case XWACOM_PARAM_SBOTTOMX3:
	    case XWACOM_PARAM_SBOTTOMX4:
	    case XWACOM_PARAM_SBOTTOMX5:
	    case XWACOM_PARAM_SBOTTOMX6:
	    case XWACOM_PARAM_SBOTTOMX7:
	    {
		int sn = (param - XWACOM_PARAM_SBOTTOMX0) / 4; 
		if (sn >= priv->numScreen)
			return -1;
		else
			return priv->screenBottomX[sn];
	    }
	    case XWACOM_PARAM_SBOTTOMY0:
	    case XWACOM_PARAM_SBOTTOMY1:
	    case XWACOM_PARAM_SBOTTOMY2:
	    case XWACOM_PARAM_SBOTTOMY3:
	    case XWACOM_PARAM_SBOTTOMY4:
	    case XWACOM_PARAM_SBOTTOMY5:
	    case XWACOM_PARAM_SBOTTOMY6:
	    case XWACOM_PARAM_SBOTTOMY7:
	    {
		int sn = (param - XWACOM_PARAM_SBOTTOMY0) / 4; 
		if (sn >= priv->numScreen)
			return -1;
		else
			return priv->screenBottomY[sn];
	    }
	    case XWACOM_PARAM_DEBUGLEVEL:
		return priv->debugLevel;
	    case XWACOM_PARAM_COMMONDBG:
		return common->debugLevel;
	    case XWACOM_PARAM_ROTATE:
		return common->wcmRotate;
	    case XWACOM_PARAM_SUPPRESS:
		return common->wcmSuppress;
	    case XWACOM_PARAM_RAWFILTER:
		return (common->wcmFlags & RAW_FILTERING_FLAG) ? 1 : 0;
	    case XWACOM_PARAM_RAWSAMPLE:
		return common->wcmRawSample;
	    case XWACOM_PARAM_PRESSCURVE:
		if (!IsCursor (priv) && !IsPad (priv) && !IsTouch (priv))
			return (priv->nPressCtrl [0] << 24) |
			       (priv->nPressCtrl [1] << 16) |
			       (priv->nPressCtrl [2] << 8) |
			       (priv->nPressCtrl [3]);
		return -1;
	    case XWACOM_PARAM_MODE:
		return ((priv->flags & ABSOLUTE_FLAG) ? 1 : 0);
	    case XWACOM_PARAM_COREEVENT:
		return ((local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER)) ? 1 : 0);
	    case XWACOM_PARAM_SPEEDLEVEL:
		return (priv->speed > 1) ?
			(int) (priv->speed / 2) + 6 :
			(int) (priv->speed * 6);
	    case XWACOM_PARAM_ACCEL:
		return priv->accel + 1;
	    case XWACOM_PARAM_CLICKFORCE:
		return !common->wcmMaxZ ? 0 :
			(int) (((common->wcmThreshold + 0.5) * 100) / common->wcmMaxZ);
	    case XWACOM_PARAM_THRESHOLD:
		return !common->wcmMaxZ ? 0 : common->wcmThreshold;
	    case XWACOM_PARAM_XYDEFAULT:
		return -1;
	    case XWACOM_PARAM_MMT:
		return priv->wcmMMonitor;
	    case XWACOM_PARAM_TPCBUTTON:
		return common->wcmTPCButton;
	    case XWACOM_PARAM_TOUCH:
		return common->wcmTouch;
	    case XWACOM_PARAM_GESTURE:
		return common->wcmGesture;
	    case XWACOM_PARAM_CAPACITY:
		return common->wcmCapacity;
	    case XWACOM_PARAM_CURSORPROX:
		if (IsCursor (priv))
			return common->wcmCursorProxoutDist;
		return -1;
	    case XWACOM_PARAM_TID:
		return common->tablet_id;
	    case XWACOM_PARAM_TOOLID:
		return priv->old_device_id;
	    case XWACOM_PARAM_TOOLSERIAL:
		return priv->old_serial;
	    case XWACOM_PARAM_SERIAL:
		return priv->serial;
	    case XWACOM_PARAM_TWINVIEW:
		return priv->twinview;
	    case XWACOM_PARAM_TVRESOLUTION0:
	    case XWACOM_PARAM_TVRESOLUTION1:
	    {
		if (priv->twinview <= TV_XINERAMA)
			return -1;
		else
		{
			int sNum = 2 * (param - XWACOM_PARAM_TVRESOLUTION0);
			return priv->tvResolution[sNum] | (priv->tvResolution[sNum + 1] << 16);
		}
	    }
	    case XWACOM_PARAM_SCREEN_NO:
		return priv->screen_no;
	    case XWACOM_PARAM_NUMSCREEN:
		return priv->numScreen;
	    case XWACOM_PARAM_XSCALING:
#ifdef WCM_XORG_TABLET_SCALING
		return 1;
#else
		return 0;
#endif
	}
	DBG(10, priv->debugLevel, ErrorF("xf86WcmGetParam invalid param %d\n", param));
	return -1;
}

/*****************************************************************************
 * xf86WcmGetDefaultScreenInfo
 ****************************************************************************/

static int xf86WcmGetDefaultScreenInfo(LocalDevicePtr local, int param)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	int numS = priv->numScreen;

	switch (param)
	{
	case XWACOM_PARAM_STOPX0:
	case XWACOM_PARAM_STOPX1:
	case XWACOM_PARAM_STOPX2:
	case XWACOM_PARAM_STOPX3:
	case XWACOM_PARAM_STOPX4:
	case XWACOM_PARAM_STOPX5:
	case XWACOM_PARAM_STOPX6:
	case XWACOM_PARAM_STOPX7:
	   {
		int sn = (param - XWACOM_PARAM_STOPX0) / 4;
		if (sn >= numS)
			return -1;
		else
			return priv->screenTopX[sn];
	   }
	case XWACOM_PARAM_STOPY0:
	case XWACOM_PARAM_STOPY1:
	case XWACOM_PARAM_STOPY2:
	case XWACOM_PARAM_STOPY3:
	case XWACOM_PARAM_STOPY4:
	case XWACOM_PARAM_STOPY5:
	case XWACOM_PARAM_STOPY6:
	case XWACOM_PARAM_STOPY7:
	   {
		int sn = (param - XWACOM_PARAM_STOPY0) / 4; 
		if (sn >= numS)
			return -1;
		else
			return priv->screenTopY[sn];
	   }
	case XWACOM_PARAM_SBOTTOMX0:
	case XWACOM_PARAM_SBOTTOMX1:
	case XWACOM_PARAM_SBOTTOMX2:
	case XWACOM_PARAM_SBOTTOMX3:
	case XWACOM_PARAM_SBOTTOMX4:
	case XWACOM_PARAM_SBOTTOMX5:
	case XWACOM_PARAM_SBOTTOMX6:
	case XWACOM_PARAM_SBOTTOMX7:
	   {
		int sn = (param - XWACOM_PARAM_SBOTTOMX0) / 4; 
		if (sn >= numS)
			return -1;
		else
			return priv->screenBottomX[sn];
	   }
	case XWACOM_PARAM_SBOTTOMY0:
	case XWACOM_PARAM_SBOTTOMY1:
	case XWACOM_PARAM_SBOTTOMY2:
	case XWACOM_PARAM_SBOTTOMY3:
	case XWACOM_PARAM_SBOTTOMY4:
	case XWACOM_PARAM_SBOTTOMY5:
	case XWACOM_PARAM_SBOTTOMY6:
	case XWACOM_PARAM_SBOTTOMY7:
	   {
		int sn = (param - XWACOM_PARAM_SBOTTOMY0) / 4; 
		if (sn >= numS)
			return -1;
		else
			return priv->screenBottomY[sn];
	   }
	}
	DBG(10, priv->debugLevel, ErrorF("xf86WcmGetDefaultScreenInfo invalid param %d\n", param));
	return -1;
}

/*****************************************************************************
 * xf86WcmGetDefaultParam
 ****************************************************************************/

static int xf86WcmGetDefaultParam(LocalDevicePtr local, int param)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	DBG(10, common->debugLevel, ErrorF("xf86WcmGetDefaultParam param = %d\n",param));

	if ( param >= XWACOM_PARAM_STOPX0 && param <= XWACOM_PARAM_SBOTTOMY2)
		return xf86WcmGetDefaultScreenInfo(local, param);

	if (param >= XWACOM_PARAM_BUTTON6 && param <= XWACOM_PARAM_BUTTON32)
		return 0;

	switch (param)
	{
	case XWACOM_PARAM_TOPX:
		return 0;
	case XWACOM_PARAM_TOPY:
		return 0;
	case XWACOM_PARAM_BOTTOMX:
		return priv->wcmMaxX;
	case XWACOM_PARAM_BOTTOMY:
		return priv->wcmMaxY;		
	case XWACOM_PARAM_BUTTON1:
	case XWACOM_PARAM_BUTTON2:
	case XWACOM_PARAM_BUTTON3:
	case XWACOM_PARAM_BUTTON4:
	case XWACOM_PARAM_BUTTON5:
		return (param - XWACOM_PARAM_BUTTON1 + 1);
	case XWACOM_PARAM_MODE:
		if (IsCursor(priv) || (IsPad(priv) && (priv->flags & COREEVENT_FLAG)))
			return 0;
		else
			return 1;
	case XWACOM_PARAM_RELWUP:
	case XWACOM_PARAM_ABSWUP:
	case XWACOM_PARAM_STRIPLUP:
	case XWACOM_PARAM_STRIPRUP:
		return 4;
	case XWACOM_PARAM_RELWDN:
	case XWACOM_PARAM_ABSWDN:
	case XWACOM_PARAM_STRIPLDN:
	case XWACOM_PARAM_STRIPRDN:
		return 5;
	case XWACOM_PARAM_SPEEDLEVEL:
		return 6;
	case XWACOM_PARAM_ACCEL:
		return 0;
	case XWACOM_PARAM_CLICKFORCE:
		return 6;
	case XWACOM_PARAM_THRESHOLD:
		if (strstr(common->wcmModel->name, "Intuos4"))
			return (common->wcmMaxZ * 3 / 25);
		else
			return (common->wcmMaxZ * 3 / 50);
	case XWACOM_PARAM_MMT:
		return 1;
	case XWACOM_PARAM_TPCBUTTON:
		return common->wcmTPCButtonDefault;
	case XWACOM_PARAM_TOUCH:
		return common->wcmTouchDefault;
	case XWACOM_PARAM_CAPACITY:
		return common->wcmCapacityDefault;
	case XWACOM_PARAM_GESTURE:
		return common->wcmGestureDefault;
	case XWACOM_PARAM_PRESSCURVE:
		if (!IsCursor (priv) && !IsPad (priv) && !IsTouch (priv))
			return (0 << 24) | (0 << 16) | (100 << 8) | 100;
		return -1;
	case XWACOM_PARAM_SUPPRESS:
		return DEFAULT_SUPPRESS;
	case XWACOM_PARAM_RAWSAMPLE:
		return DEFAULT_SAMPLES;
	case XWACOM_PARAM_CURSORPROX:
		if (IsCursor (priv))
			return common->wcmCursorProxoutDistDefault;
		return -1;
	case XWACOM_PARAM_COREEVENT:
		return ((priv->flags & COREEVENT_FLAG) ? 1 : 0);
	}
	DBG(10, priv->debugLevel, ErrorF("xf86WcmGetDefaultParam invalid param %d\n",param));
	return -1;
}
 
/*****************************************************************************
 * xf86WcmDevChangeControl --
 ****************************************************************************/

int xf86WcmDevChangeControl(LocalDevicePtr local, xDeviceCtl* control)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	xDeviceResolutionCtl* res = (xDeviceResolutionCtl *)control;
	int i, rc = Success, *r = (int*)(res+1);

	if (control->control != DEVICE_RESOLUTION || (res->num_valuators < 1
			&& res->num_valuators > 3) || res->first_valuator != 0)
		return BadMatch;

	switch (res->num_valuators)
	{
		case  1:
		{
			AxisInfoPtr a;

			DBG (5, common->debugLevel, ErrorF(
				"xf86WcmQueryControl: dev %s query 0x%x at %d\n",
				local->dev->name, r [0], priv->naxes));
			/* Since X11 doesn't provide any sane protocol for querying
			 * device parameters, we have to do a dirty trick here:
			 * we set the resolution of last axis to asked value,
			 * then we query it via XGetDeviceControl().
			 * After the query is done, XChangeDeviceControl is called
			 * again with r [0] == 0, which restores default resolution.
			 */
			a = local->dev->valuator->axes + priv->naxes - 1;
			if (r [0] >= XWACOM_PARAM_BUTTON1 && r[0] <= XWACOM_PARAM_STRIPRDN)
				a->resolution = a->min_resolution = a->max_resolution = 
					xf86WcmGetButtonParam (local, r [0]);
			else
				a->resolution = a->min_resolution = a->max_resolution =
					xf86WcmGetParam (local, r [0]);
			break;
		}
		case 2:
		{
			DBG (5, common->debugLevel, ErrorF(
				"xf86WcmChangeControl: dev %s set 0x%x to 0x%x\n",
				local->dev->name, r [0], r [1]));
			if (r [0] >= XWACOM_PARAM_BUTTON1 && r[0] <= XWACOM_PARAM_STRIPRDN)
				rc = xf86WcmSetButtonParam (local, r [0], r[1]);
			else
				rc = xf86WcmSetParam (local, r [0], r[1]);
			break;
		}
		case 3:
		{
			AxisInfoPtr a;

			DBG (5, common->debugLevel, ErrorF(
				"xf86WcmQueryControl: dev %s query 0x%x at %d\n",
				local->dev->name, r [0], priv->naxes));
			/* Since X11 doesn't provide any sane protocol for querying
			 * device parameters, we have to do a dirty trick here:
			 * we set the resolution of last axis to asked value,
			 * then we query it via XGetDeviceControl().
			 * After the query is done, XChangeDeviceControl is called
			 * again with r [0] == 0, which restores default resolution.
			 */
			a = local->dev->valuator->axes + priv->naxes - 1;
			a->resolution = a->min_resolution = a->max_resolution =
				r [0] ? xf86WcmGetDefaultParam (local, r [0]) : 1;
			break;
		}
	}
	/* Set resolution to current values so that X core doesn't barf */
	for (i = 0; i < res->num_valuators; i++)
		r [i] = local->dev->valuator->axes [i].resolution;
        return rc;
}
