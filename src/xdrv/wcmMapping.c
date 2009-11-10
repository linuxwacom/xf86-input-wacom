/*
 * Copyright 2009 by Ping Cheng, Wacom Technology. <pingc@wacom.com>		
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

static void xf86WcmInitialScreens(LocalDevicePtr local);


/*****************************************************************************
 * xf86WcmDesktopSize --
 *   calculate the whole desktop size 
 ****************************************************************************/
static void xf86WcmDesktopSize(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int i = 0, minX = 0, minY = 0, maxX = 0, maxY = 0;

	xf86WcmInitialScreens(local);

	minX = priv->screenTopX[0];
	minY = priv->screenTopY[0];
	maxX = priv->screenBottomX[0];
	maxY = priv->screenBottomY[0];

	if (priv->numScreen != 1)
	{
		for (i = 1; i < priv->numScreen; i++)
		{
			if (priv->screenTopX[i] < minX)
				minX = priv->screenTopX[i];
			if (priv->screenTopY[i] < minY)
				minY = priv->screenTopY[i];
			if (priv->screenBottomX[i] > maxX)
				maxX = priv->screenBottomX[i];
			if (priv->screenBottomY[i] > maxY)
				maxY = priv->screenBottomY[i];
		}
	}

	priv->maxWidth = maxX - minX;
	priv->maxHeight = maxY - minY;
} 

int xf86WcmInitArea(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomToolAreaPtr area = priv->toolarea, inlist;
	WacomCommonPtr common = priv->common;
	double screenRatio, tabletRatio;
	int bottomx = common->wcmMaxX, bottomy = common->wcmMaxY;

	DBG(10, priv->debugLevel, ErrorF("xf86WcmInitArea\n"));

	if (IsTouch(priv))
	{
		bottomx = common->wcmMaxTouchX, 
		bottomy = common->wcmMaxTouchY;
	}

	/* Verify Box */
	if (priv->topX > bottomx)
	{
		area->topX = priv->topX = 0;
	}

	if (priv->topY > bottomy)
	{
		area->topY = priv->topY = 0;
	}

	/* set unconfigured bottom to max */
	priv->bottomX = xf86SetIntOption(local->options, "BottomX", 0);
	if (priv->bottomX < priv->topX || !priv->bottomX)
	{
		area->bottomX = priv->bottomX = bottomx;
	}

	/* set unconfigured bottom to max */
	priv->bottomY = xf86SetIntOption(local->options, "BottomY", 0);
	if (priv->bottomY < priv->topY || !priv->bottomY)
	{
		area->bottomY = priv->bottomY = bottomy;
	}

	if (priv->twinview > TV_XINERAMA)
		priv->numScreen = 2;

	if (priv->screen_no != -1 &&
		(priv->screen_no >= priv->numScreen || priv->screen_no < 0))
	{
		if (priv->twinview <= TV_XINERAMA)
		{
			ErrorF("%s: invalid screen number %d, resetting to default (-1) \n",
					local->name, priv->screen_no);
			priv->screen_no = -1;
		}
		else if (priv->screen_no > 1)
		{
			ErrorF("%s: invalid screen number %d, resetting to default (-1) \n",
					local->name, priv->screen_no);
			priv->screen_no = -1;
		}
	}

	/* need maxWidth and maxHeight for keepshape */
	xf86WcmDesktopSize(local);

	/* Maintain aspect ratio to the whole desktop
	 * May need to consider a specific screen in multimonitor settings
	 */
	if (priv->flags & KEEP_SHAPE_FLAG)
	{

		screenRatio = ((double)priv->maxWidth / (double)priv->maxHeight);
		tabletRatio = ((double)(bottomx - priv->topX) /
				(double)(bottomy - priv->topY));

		DBG(2, priv->debugLevel, ErrorF("screenRatio = %.3g, "
			"tabletRatio = %.3g\n", screenRatio, tabletRatio));

		if (screenRatio > tabletRatio)
		{
			area->bottomX = priv->bottomX = bottomx;
			area->bottomY = priv->bottomY = (bottomy - priv->topY) *
				tabletRatio / screenRatio + priv->topY;
		}
		else
		{
			area->bottomX = priv->bottomX = (bottomx - priv->topX) *
				screenRatio / tabletRatio + priv->topX;
			area->bottomY = priv->bottomY = bottomy;
		}
	}
	/* end keep shape */ 

	inlist = priv->tool->arealist;

	/* The first one in the list is always valid */
	if (area != inlist && xf86WcmAreaListOverlap(area, inlist))
	{
		inlist = priv->tool->arealist;

		/* remove this overlapped area from the list */
		for (; inlist; inlist=inlist->next)
		{
			if (inlist->next == area)
			{
				inlist->next = area->next;
				xfree(area);
				priv->toolarea = NULL;
 			break;
			}
		}

		/* Remove this device from the common struct */
		if (common->wcmDevices == priv)
			common->wcmDevices = priv->next;
		else
		{
			WacomDevicePtr tmp = common->wcmDevices;
			while(tmp->next && tmp->next != priv)
				tmp = tmp->next;
			if(tmp)
				tmp->next = priv->next;
		}
		xf86Msg(X_ERROR, "%s: Top/Bottom area overlaps with another devices.\n",
			local->conf_idev->identifier);
		return FALSE;
	}
	return TRUE;
}

/*****************************************************************************
 * xf86WcmVirtaulTabletSize(LocalDevicePtr local)
 ****************************************************************************/

void xf86WcmVirtaulTabletSize(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	if (!(priv->flags & ABSOLUTE_FLAG))
	{
		priv->sizeX = priv->bottomX - priv->topX;
		priv->sizeY = priv->bottomY - priv->topY;
		return;
	}

	priv->sizeX = priv->bottomX - priv->topX - priv->tvoffsetX;
	priv->sizeY = priv->bottomY - priv->topY - priv->tvoffsetY;

	DBG(10, priv->debugLevel, ErrorF("xf86WcmVirtaulTabletSize for \"%s\" "
		"x=%d y=%d \n", local->name, priv->sizeX, priv->sizeY));
	return;
}

/*****************************************************************************
 * xf86WcmInitialCoordinates
 ****************************************************************************/

void xf86WcmInitialCoordinates(LocalDevicePtr local, int axes)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	int topx = 0, topy = 0, resolution;
	WacomCommonPtr common = priv->common;
	int bottomx = common->wcmMaxX, bottomy = common->wcmMaxY;

	if (IsTouch(priv))
	{
		bottomx = common->wcmMaxTouchX, 
		bottomy = common->wcmMaxTouchY;
	}

	xf86WcmMappingFactor(local);

	/* x ax */
	if ( !axes )
	{
		if (priv->flags & ABSOLUTE_FLAG)
		{
			topx = priv->topX;
			bottomx = priv->sizeX + priv->topX;
			if ((priv->currentScreen == 1) && (priv->twinview > TV_XINERAMA))
				topx += priv->tvoffsetX;
			if ((priv->currentScreen == 0) && (priv->twinview > TV_XINERAMA))
				bottomx -= priv->tvoffsetX;
		}

		if (!IsTouch(priv))
			resolution = common->wcmResolX;
		else
			resolution = common->wcmTouchResolX;
#ifdef WCM_XORG_TABLET_SCALING
		/* Ugly hack for Xorg 7.3, which doesn't call xf86WcmDevConvert
		 * for coordinate conversion at the moment */
		topx = 0;
		bottomx = priv->sizeX;
		if ((priv->twinview == TV_LEFT_RIGHT) || (priv->twinview == TV_RIGHT_LEFT))
			bottomx *= 2;
		bottomx = (int)((double)bottomx * priv->factorX + 0.5);
		resolution = (int)((double)resolution * priv->factorX + 0.5);
#endif

		InitValuatorAxisStruct(local->dev, 0, topx, bottomx, 
			resolution, 0, resolution); 
	}
	else /* y ax */
	{
		if (priv->flags & ABSOLUTE_FLAG)
		{
			topy = priv->topY;
			bottomy = priv->sizeY + priv->topY;
			if ((priv->currentScreen == 1) && (priv->twinview > TV_XINERAMA))
				topy += priv->tvoffsetY;
			if ((priv->currentScreen == 0) && (priv->twinview > TV_XINERAMA))
				bottomy -= priv->tvoffsetY;
		}

		if (!IsTouch(priv))
			resolution = common->wcmResolY;
		else
			resolution = common->wcmTouchResolY;
#ifdef WCM_XORG_TABLET_SCALING
		/* Ugly hack for Xorg 7.3, which doesn't call xf86WcmDevConvert
		 * for coordinate conversion at the moment */
		topy = 0;
		bottomy = priv->sizeY;
		if ((priv->twinview == TV_ABOVE_BELOW) || (priv->twinview == TV_BELOW_ABOVE))
			bottomy *= 2;
		bottomy = (int)((double)bottomy * priv->factorY + 0.5);
		resolution = (int)((double)resolution * priv->factorY + 0.5);
#endif

		InitValuatorAxisStruct(local->dev, 1, topy, bottomy, 
			resolution, 0, resolution); 
	}
	return;
}

/*****************************************************************************
 * xf86WcmMappingFactor --
 *   calculate the proper tablet to screen mapping factor according to the 
 *   screen/desktop size and the tablet size 
 ****************************************************************************/

void xf86WcmMappingFactor(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
 	int screenX, screenY;

	DBG(10, priv->debugLevel, ErrorF("xf86WcmMappingFactor \n"));

	xf86WcmVirtaulTabletSize(local);
	
	if (!(priv->flags & ABSOLUTE_FLAG) || !priv->wcmMMonitor)
	{
		/* Get the current screen that the cursor is in */
#if WCM_XINPUTABI_MAJOR == 0
		if (miPointerCurrentScreen())
			priv->currentScreen = miPointerCurrentScreen()->myNum;
#else
		if (miPointerGetScreen(local->dev))
			priv->currentScreen = miPointerGetScreen(local->dev)->myNum;
#endif
	}
	else
	{
		if (priv->screen_no != -1)
			priv->currentScreen = priv->screen_no;
		else if (priv->currentScreen == -1)
		{
			/* Get the current screen that the cursor is in */
#if WCM_XINPUTABI_MAJOR == 0
			if (miPointerCurrentScreen())
				priv->currentScreen = miPointerCurrentScreen()->myNum;
#else
			if (miPointerGetScreen(local->dev))
				priv->currentScreen = miPointerGetScreen(local->dev)->myNum;
#endif
		}
	}
	if (priv->currentScreen == -1) /* tool on the tablet */
		priv->currentScreen = 0;

 	screenX = priv->maxWidth;
	screenY = priv->maxHeight;
	if (priv->screen_no != -1 || (priv->twinview > TV_XINERAMA) || (!priv->wcmMMonitor) )
	{
 		screenX = priv->screenBottomX[priv->currentScreen] - priv->screenTopX[priv->currentScreen];
		screenY = priv->screenBottomY[priv->currentScreen] - priv->screenTopY[priv->currentScreen];
	}
	DBG(10, priv->debugLevel, ErrorF("xf86WcmMappingFactor"
		" Active tablet area x=%d y=%d (virtual tablet area x=%d y=%d) map"
		" to maxWidth =%d maxHeight =%d\n", 
		priv->bottomX, priv->bottomY, priv->sizeX, priv->sizeY, 
		screenX, screenY));

	priv->factorX = (double)screenX / (double)priv->sizeX;
	priv->factorY = (double)screenY / (double)priv->sizeY;
	DBG(2, priv->debugLevel, ErrorF("X factor = %.3g, Y factor = %.3g\n",
		priv->factorX, priv->factorY));
}

/*****************************************************************************
 * xf86WcmSetScreen --
 *   set to the proper screen according to the converted (x,y).
 *   this only supports for horizontal setup now.
 *   need to know screen's origin (x,y) to support 
 *   combined horizontal and vertical setups
 ****************************************************************************/

void xf86WcmSetScreen(LocalDevicePtr local, int v0, int v1)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int screenToSet = -1, i, j, x, y, tabletSize = 0;

	DBG(6, priv->debugLevel, ErrorF("xf86WcmSetScreen v0=%d v1=%d "
		"currentScreen=%d\n", v0, v1, priv->currentScreen));

	if (priv->screen_no != -1 && priv->screen_no >= priv->numScreen)
	{
		ErrorF("xf86WcmSetScreen Screen%d is larger than number of available screens (%d)\n", 
			priv->screen_no, priv->numScreen);
		priv->screen_no = -1;
	}

	if (!(local->flags & (XI86_ALWAYS_CORE | XI86_CORE_POINTER))) return;

	if (priv->twinview > TV_XINERAMA && priv->screen_no == -1 && (priv->flags & ABSOLUTE_FLAG))
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
		DBG(10, priv->debugLevel, ErrorF("xf86WcmSetScreen TwinView setup screenToSet=%d\n", 
			priv->currentScreen));
	}

	xf86WcmMappingFactor(local);
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
		DBG(3, priv->debugLevel, ErrorF("xf86WcmSetScreen Error: "
			"Can not find valid screen (currentScreen=%d)\n", 
			priv->currentScreen));
		return;
	}

	priv->currentScreen = screenToSet;
	xf86WcmMappingFactor(local);
	x = ((double)v0 * priv->factorX) - priv->screenTopX[screenToSet] + 0.5;
	y = ((double)v1 * priv->factorY) - priv->screenTopY[screenToSet] + 0.5;

	if (x >= screenInfo.screens[screenToSet]->width)
		x = screenInfo.screens[screenToSet]->width - 1;
	if (y >= screenInfo.screens[screenToSet]->height)
		y = screenInfo.screens[screenToSet]->height - 1;

	xf86XInputSetScreen(local, screenToSet, x, y);
	DBG(10, priv->debugLevel, ErrorF("xf86WcmSetScreen current=%d ToSet=%d\n", 
			priv->currentScreen, screenToSet));
}

/*****************************************************************************
 * xf86WcmInitTablet -- common initialization for all tablets
 ****************************************************************************/

int xf86WcmInitTablet(LocalDevicePtr local, const char* id, float version)
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
		if (strstr(common->wcmModel->name, "Intuos4"))
			common->wcmThreshold = common->wcmMaxZ * 3 / 25;
		else
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
	if (model->SetLinkSpeed)
	{
		if (common->wcmLinkSpeed != 9600)
		{
			if (model->SetLinkSpeed(local) != Success)
				return !Success;
		}
	}
	else
	{
		DBG(2, common->debugLevel, ErrorF("Tablet does not support setting link "
			"speed, or not yet implemented\n"));
	}

	/* output tablet state as probed */
	if (xf86Verbose)
		ErrorF("%s Wacom %s tablet speed=%d (%d) maxX=%d maxY=%d maxZ=%d "
			"resX=%d resY=%d  tilt=%s\n",
			XCONFIG_PROBED,
			model->name, common->wcmLinkSpeed, common->wcmISDV4Speed, 
			common->wcmMaxX, common->wcmMaxY, common->wcmMaxZ,
			common->wcmResolX, common->wcmResolY,
			HANDLE_TILT(common) ? "enabled" : "disabled");
  
	/* start the tablet data */
	if (model->Start && (model->Start(local) != Success))
		return !Success;

	return Success;
}

/*****************************************************************************
 * xf86WcmInitialTVScreens
 ****************************************************************************/

static void xf86WcmInitialTVScreens(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	if (priv->twinview <= TV_XINERAMA)
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

	DBG(10, priv->debugLevel, ErrorF("xf86WcmInitialTVScreens for \"%s\" "
		"topX0=%d topY0=%d bottomX0=%d bottomY0=%d "
		"topX1=%d topY1=%d bottomX1=%d bottomY1=%d \n",
		local->name, priv->screenTopX[0], priv->screenTopY[0],
		priv->screenBottomX[0], priv->screenBottomY[0],
		priv->screenTopX[1], priv->screenTopY[1],
		priv->screenBottomX[1], priv->screenBottomY[1]));
}

/*****************************************************************************
 * xf86WcmInitialScreens
 ****************************************************************************/

static void xf86WcmInitialScreens(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	int i;

	DBG(2, priv->debugLevel, ErrorF("xf86WcmInitialScreens for \"%s\" "
		"number of screen=%d \n", local->name, screenInfo.numScreens));
	priv->tvoffsetX = 0;
	priv->tvoffsetY = 0;
	if (priv->twinview > TV_XINERAMA)
	{
		xf86WcmInitialTVScreens(local);
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
#ifdef WCM_HAVE_DIXSCREENORIGINS
		if (screenInfo.numScreens > 1)
		{
			priv->screenTopX[i] = dixScreenOrigins[i].x;
			priv->screenTopY[i] = dixScreenOrigins[i].y;
			priv->screenBottomX[i] = dixScreenOrigins[i].x;
			priv->screenBottomY[i] = dixScreenOrigins[i].y;

			DBG(10, priv->debugLevel, ErrorF("xf86WcmInitialScreens from dix for \"%s\" "
				"ScreenOrigins[%d].x=%d ScreenOrigins[%d].y=%d \n",
				local->name, i, priv->screenTopX[i], i, priv->screenTopY[i]));
		}
#else /* WCM_HAVE_DIXSCREENORIGINS */
		if (i > 0)
		{
			/* only support left to right in this case */
			priv->screenTopX[i] = priv->screenBottomX[i-1];
			priv->screenTopY[i] = 0;
			priv->screenBottomX[i] = priv->screenTopX[i];
			priv->screenBottomY[i] = 0;
		}
#endif /* WCM_HAVE_DIXSCREENORIGINS */
		priv->screenBottomX[i] += screenInfo.screens[i]->width;
		priv->screenBottomY[i] += screenInfo.screens[i]->height;

		DBG(10, priv->debugLevel, ErrorF("xf86WcmInitialScreens for \"%s\" "
			"topX[%d]=%d topY[%d]=%d bottomX[%d]=%d bottomY[%d]=%d \n",
			local->name, i, priv->screenTopX[i], i, priv->screenTopY[i],
			i, priv->screenBottomX[i], i, priv->screenBottomY[i]));
	}
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
 * rotateOneTool
 ****************************************************************************/

static void rotateOneTool(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomToolAreaPtr area = priv->toolarea;
	int tmpTopX, tmpTopY, tmpBottomX, tmpBottomY, oldMaxX, oldMaxY;

	DBG(10, priv->debugLevel, ErrorF("rotateOneTool for \"%s\" \n", priv->local->name));

	if ( !IsTouch(priv) )
	{
		oldMaxX = common->wcmMaxX;
		oldMaxY = common->wcmMaxY;
	}
	else
	{
		oldMaxX = common->wcmMaxTouchX;
		oldMaxY = common->wcmMaxTouchY;
	}

	tmpTopX = priv->topX;
	tmpBottomX = priv->bottomX;
	tmpTopY = priv->topY;
	tmpBottomY = priv->bottomY;

	if (common->wcmRotate == ROTATE_CW || common->wcmRotate == ROTATE_CCW)
	{
		if ( !IsTouch(priv) )
		{
		    common->wcmMaxX = oldMaxY;
		    common->wcmMaxY = oldMaxX;
		}
		else
		{
		    common->wcmMaxTouchX = oldMaxY;
		    common->wcmMaxTouchY = oldMaxX;
		}
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
	xf86WcmInitialCoordinates(priv->local, 0);
	xf86WcmInitialCoordinates(priv->local, 1);

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
 * xf86WcmRotateTablet
 ****************************************************************************/

void xf86WcmRotateTablet(LocalDevicePtr local, int value)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomDevicePtr tmppriv;
	int oldRotation;
	int tmpTopX, tmpTopY, tmpBottomX, tmpBottomY, oldMaxX, oldMaxY;

	DBG(10, priv->debugLevel, ErrorF("xf86WcmRotateTablet for \"%s\" \n", local->name));

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
		    if ( !IsTouch(priv) )
		    {
			oldMaxX = common->wcmMaxX;
			oldMaxY = common->wcmMaxY;
		    }
		    else
		    {
			oldMaxX = common->wcmMaxTouchX;
			oldMaxY = common->wcmMaxTouchY;
		    }

		    if (oldRotation == ROTATE_CW || oldRotation == ROTATE_CCW) 
		    {
			common->wcmMaxX = oldMaxY;
			common->wcmMaxY = oldMaxX;
		    }
		    else
		    {
			common->wcmMaxTouchX = oldMaxY;
			common->wcmMaxTouchY = oldMaxX;
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

