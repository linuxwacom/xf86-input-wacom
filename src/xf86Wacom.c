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

/*
 * This driver is currently able to handle USB Wacom IV and V, serial ISDV4,
 * and bluetooth protocols.
 *
 * Wacom V protocol work done by Raph Levien <raph@gtk.org> and
 * Frédéric Lepied <lepied@xfree86.org>.
 *
 * Modified for Linux USB by MATSUMURA Namihiko,
 * Daniel Egger, Germany. <egger@suse.de>,
 * Frederic Lepied <lepied@xfree86.org>,
 * Brion Vibber <brion@pobox.com>,
 * Aaron Optimizer Digulla <digulla@hepe.com>,
 * Jonathan Layes <jonathan@layes.com>,
 * John Joganic <jej@j-arkadia.com>,
 * Magnus Vigerlöf <Magnus.Vigerlof@ipbo.se>,
 * Peter Hutterer <peter.hutterer@redhat.com>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/serial.h>

#include "xf86Wacom.h"
#include <xf86_OSproc.h>
#include <exevents.h>           /* Needed for InitValuator/Proximity stuff */

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
#include <xserver-properties.h>
#include <X11/extensions/XKB.h>
#include <xkbsrv.h>
#endif

static int wcmDevOpen(DeviceIntPtr pWcm);
static int wcmReady(LocalDevicePtr local);
static void wcmDevReadInput(LocalDevicePtr local);
static void wcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl);
int wcmDevChangeControl(LocalDevicePtr local, xDeviceCtl * control);
static void wcmDevClose(LocalDevicePtr local);
static int wcmDevProc(DeviceIntPtr pWcm, int what);

WacomModule gWacomModule =
{
	NULL,           /* input driver pointer */

	/* device procedures */
	wcmDevOpen,
	wcmDevReadInput,
	wcmDevControlProc,
	wcmDevChangeControl,
	wcmDevClose,
	wcmDevProc,
	wcmDevSwitchMode,
};

static void wcmKbdLedCallback(DeviceIntPtr di, LedCtrl * lcp)
{
}

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 5
static void wcmBellCallback(int pct, DeviceIntPtr di, pointer ctrl, int x)
{
}
#endif

static void wcmKbdCtrlCallback(DeviceIntPtr di, KeybdCtrl* ctrl)
{
}

/*****************************************************************************
 * wcmDesktopSize --
 *   calculate the whole desktop size 
 ****************************************************************************/
static void wcmDesktopSize(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	int i = 0, minX = 0, minY = 0, maxX = 0, maxY = 0;

	wcmInitialScreens(local);
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

static int wcmInitArea(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomToolAreaPtr area = priv->toolarea, inlist;
	WacomCommonPtr common = priv->common;
	double screenRatio, tabletRatio;
	int bottomx = priv->maxX, bottomy = priv->maxY;

	DBG(10, priv, "\n");

	/* the following 4 blocks verify the box and
	 * initialize the area */
	if (priv->topX > bottomx)
	{
		priv->topX = 0;
	}
	area->topX = priv->topX;

	if (priv->topY > bottomy)
	{
		priv->topY = 0;
	}
	area->topY = priv->topY;

	if (priv->bottomX < priv->topX || !priv->bottomX)
	{
		priv->bottomX = bottomx;
	}
	area->bottomX = priv->bottomX;

	if (priv->bottomY < priv->topY || !priv->bottomY)
	{
		priv->bottomY = bottomy;
	}
	area->bottomY = priv->bottomY;

	if (priv->twinview != TV_NONE)
		priv->numScreen = 2;

	if (priv->screen_no != -1 &&
		(priv->screen_no >= priv->numScreen || priv->screen_no < 0))
	{
		if (priv->twinview == TV_NONE || priv->screen_no != 1)
		{
			xf86Msg(X_ERROR, "%s: invalid screen number %d, resetting to default (-1) \n",
					local->name, priv->screen_no);
			priv->screen_no = -1;
		}
	}

	/* need maxWidth and maxHeight for keepshape */
	wcmDesktopSize(local);

	/* Maintain aspect ratio to the whole desktop
	 * May need to consider a specific screen in multimonitor settings
	 */
	if (priv->flags & KEEP_SHAPE_FLAG)
	{

		screenRatio = ((double)priv->maxWidth / (double)priv->maxHeight);
		tabletRatio = ((double)(bottomx - priv->topX) /
				(double)(bottomy - priv->topY));

		DBG(2, priv, "screenRatio = %.3g, "
			"tabletRatio = %.3g\n", screenRatio, tabletRatio);

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
	if (area != inlist && wcmAreaListOverlap(area, inlist))
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
	xf86Msg(X_PROBED, "%s: top X=%d top Y=%d "
			"bottom X=%d bottom Y=%d "
			"resol X=%d resol Y=%d\n",
			local->name, priv->topX,
			priv->topY, priv->bottomX, priv->bottomY,
			common->wcmResolX, common->wcmResolY);
	return TRUE;
}

/*****************************************************************************
 * wcmVirtualTabletPadding(LocalDevicePtr local)
 ****************************************************************************/

void wcmVirtualTabletPadding(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	/* in multi-screen settings, add the left/top offset given the
	 * current multimonitor setup.
	 */

	priv->leftPadding = 0;
	priv->topPadding = 0;

	if (!(priv->flags & ABSOLUTE_FLAG)) return;

	if ((priv->screen_no != -1) || (priv->twinview != TV_NONE) || (!priv->wcmMMonitor))
	{
		double width, height;	/* tablet width in device coords */
		double sw, sh;		/* screen width/height in screen coords */
		double offset;		/* screen x/y offset in screen coords */
		int screen;		/* screen number */

		screen = priv->currentScreen;

		width  = priv->bottomX - priv->topX -priv->tvoffsetX;
		height = priv->bottomY - priv->topY - priv->tvoffsetY;
		sw = priv->screenBottomX[screen] - priv->screenTopX[screen];
		sh = priv->screenBottomY[screen] - priv->screenTopY[screen];

		offset = priv->screenTopX[screen];

		priv->leftPadding = (int)(offset * width / sw  + 0.5);

		offset = priv->screenTopY[screen];

		priv->topPadding = (int)(offset * height / sh + 0.5);
	}
	DBG(10, priv, "x=%d y=%d \n", priv->leftPadding, priv->topPadding);
	return;
}

void
wcmAdjustArea(const LocalDevicePtr local, WacomToolArea *area)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	/* If we're bound to a specific screen, substract the screen's
	 * offset from the area coordinates we have here.
	 */
	if (priv->screen_no != -1)
	{
		wcmVirtualTabletPadding(local);
		DBG(10, priv, "padding is %d/%d\n", priv->leftPadding, priv->topPadding);
		area->topX -= priv->leftPadding;
		area->bottomX -= priv->leftPadding;
		area->topY -= priv->topPadding;
		area->bottomY -= priv->topPadding;
		DBG(10, priv, "updated area is %d/%d → %d/%d\n",
		    area->topX, area->topY, area->bottomX, area->bottomY);
	}
}

/*****************************************************************************
 * wcmVirtualTabletSize(LocalDevicePtr local)
 ****************************************************************************/

void wcmVirtualTabletSize(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	if (!(priv->flags & ABSOLUTE_FLAG))
	{
		priv->sizeX = priv->bottomX - priv->topX;
		priv->sizeY = priv->bottomY - priv->topY;
		DBG(10, priv, "relative device, using size of %d/%d\n", priv->sizeX, priv->sizeY);

		return;
	}
	/* given a three monitor setup
	 *	        offset
	 *  __________ /__________  __________
	 * |          ||          ||          |
	 * |          ||          ||          |
	 * |    A     ||    B     ||    C     |
	 * |__________||__________||__________|
	 *
	 * this function calculates the virtual size of the tablet by
	 * mapping the actual size into an axis range that is all three
	 * monitors in device coordinates taken together.
	 * in the simplest case, with 3 identical monitors, sizeX would be
	 * (3 * actual size).
	 *
	 * coments describe for example of screen_no = 1 (Screen B)
	 */

	/* This is the actual tablet size in device coords */
	priv->sizeX = priv->bottomX - priv->topX - priv->tvoffsetX;
	priv->sizeY = priv->bottomY - priv->topY - priv->tvoffsetY;

	if ((priv->screen_no != -1) || (priv->twinview != TV_NONE) || (!priv->wcmMMonitor))
	{
		double width, height; /* screen width, height */
		double offset; /* screen x or y offset from origin */
		double remainder; /* screen remainer on right-most screens */
		double tabletSize;
		int screen = priv->currentScreen;

		width = priv->screenBottomX[screen] - priv->screenTopX[screen];
		offset = priv->screenTopX[screen];
		tabletSize = priv->sizeX;
		/* width left over right of the screen */
		remainder = priv->maxWidth - priv->screenBottomX[screen];

		/* add screen A size in device coordinates */
		priv->sizeX += (int)((offset * tabletSize) / width + 0.5);

		/* add screen C size in device coordinates */
		priv->sizeX += (int)((remainder * tabletSize) / width + 0.5);

		tabletSize = priv->sizeY;

		offset = priv->screenTopY[screen];
		height = priv->screenBottomY[screen] - priv->screenTopY[screen];
		/* height left over bottom of the screen */
		remainder = priv->maxHeight - priv->screenBottomY[screen];

		priv->sizeY += (int)(offset * tabletSize / height + 0.5);
		priv->sizeY += (int)((remainder * tabletSize) / height + 0.5);
	}
	DBG(10, priv, "x=%d y=%d \n", priv->sizeX, priv->sizeY);
	return;
}

/*****************************************************************************
 * wcmInitialCoordinates
 ****************************************************************************/

void wcmInitialCoordinates(LocalDevicePtr local, int axis)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	int topx = 0, topy = 0, resolution_x, resolution_y;
	int bottomx = priv->maxX, bottomy = priv->maxY;

	wcmMappingFactor(local);

	/* wcmMappingFactor calls wcmVirtualTabletSize. so once we're here,
	 * sizeX contains the total width in device coordinates accounting
	 * for multiple screens (not the _actual width of the tablet, see
	 * wcmVirtualTabletSize)
	 */
	if (priv->flags & ABSOLUTE_FLAG)
	{
		topx = priv->topX;
		topy = priv->topY;
		bottomx = priv->sizeX + priv->topX;
		bottomy = priv->sizeY + priv->topY;

		if (priv->twinview != TV_NONE)
		{
			if (priv->currentScreen == 1)
			{
				topx += priv->tvoffsetX;
				topy += priv->tvoffsetY;
			} else if (priv->currentScreen == 0)
			{
				bottomx -= priv->tvoffsetX;
				bottomy -= priv->tvoffsetY;
			}
		}
	}
	resolution_x = priv->resolX;
	resolution_y = priv->resolY;

	if (common->wcmScaling)
	{
		/* In case wcmDevConvert didn't get called */
		topx = 0;
		bottomx = (int)((double)priv->sizeX * priv->factorX + 0.5);
		resolution_x = (int)((double)resolution_x * priv->factorX + 0.5);

		topy = 0;
		bottomy = (int)((double)priv->sizeY * priv->factorY + 0.5);
		resolution_y = (int)((double)resolution_y * priv->factorY + 0.5);
	}

	switch(axis)
	{
		case 0:
			InitValuatorAxisStruct(local->dev, 0,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					XIGetKnownProperty(AXIS_LABEL_PROP_ABS_X),
#endif
					topx, bottomx,
					resolution_x, 0, resolution_x);
			break;
		case 1:
			InitValuatorAxisStruct(local->dev, 1,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					XIGetKnownProperty(AXIS_LABEL_PROP_ABS_Y),
#endif
					topy, bottomy,
					resolution_y, 0, resolution_y);
			break;
		default:
			xf86Msg(X_ERROR, "%s: Cannot initialize axis %d.\n", local->name, axis);
			break;
	}

	return;
}

/* Define our own keymap so we can send key-events with our own device and not
 * rely on inputInfo.keyboard */
static KeySym keymap[] = {
	/* 0x00 */  NoSymbol,		NoSymbol,	XK_Escape,	NoSymbol,
	/* 0x02 */  XK_1,		XK_exclam,	XK_2,		XK_at,
	/* 0x04 */  XK_3,		XK_numbersign,	XK_4,		XK_dollar,
	/* 0x06 */  XK_5,		XK_percent,	XK_6,		XK_asciicircum,
	/* 0x08 */  XK_7,		XK_ampersand,	XK_8,		XK_asterisk,
	/* 0x0a */  XK_9,		XK_parenleft,	XK_0,		XK_parenright,
	/* 0x0c */  XK_minus,		XK_underscore,	XK_equal,	XK_plus,
	/* 0x0e */  XK_BackSpace,	NoSymbol,	XK_Tab,		XK_ISO_Left_Tab,
	/* 0x10 */  XK_q,		NoSymbol,	XK_w,		NoSymbol,
	/* 0x12 */  XK_e,		NoSymbol,	XK_r,		NoSymbol,
	/* 0x14 */  XK_t,		NoSymbol,	XK_y,		NoSymbol,
	/* 0x16 */  XK_u,		NoSymbol,	XK_i,		NoSymbol,
	/* 0x18 */  XK_o,		NoSymbol,	XK_p,		NoSymbol,
	/* 0x1a */  XK_bracketleft,	XK_braceleft,	XK_bracketright,	XK_braceright,
	/* 0x1c */  XK_Return,		NoSymbol,	XK_Control_L,	NoSymbol,
	/* 0x1e */  XK_a,		NoSymbol,	XK_s,		NoSymbol,
	/* 0x20 */  XK_d,		NoSymbol,	XK_f,		NoSymbol,
	/* 0x22 */  XK_g,		NoSymbol,	XK_h,		NoSymbol,
	/* 0x24 */  XK_j,		NoSymbol,	XK_k,		NoSymbol,
	/* 0x26 */  XK_l,		NoSymbol,	XK_semicolon,	XK_colon,
	/* 0x28 */  XK_quoteright,	XK_quotedbl,	XK_quoteleft,	XK_asciitilde,
	/* 0x2a */  XK_Shift_L,		NoSymbol,	XK_backslash,	XK_bar,
	/* 0x2c */  XK_z,		NoSymbol,	XK_x,		NoSymbol,
	/* 0x2e */  XK_c,		NoSymbol,	XK_v,		NoSymbol,
	/* 0x30 */  XK_b,		NoSymbol,	XK_n,		NoSymbol,
	/* 0x32 */  XK_m,		NoSymbol,	XK_comma,	XK_less,
	/* 0x34 */  XK_period,		XK_greater,	XK_slash,	XK_question,
	/* 0x36 */  XK_Shift_R,		NoSymbol,	XK_KP_Multiply,	NoSymbol,
	/* 0x38 */  XK_Alt_L,		XK_Meta_L,	XK_space,	NoSymbol,
	/* 0x3a */  XK_Caps_Lock,	NoSymbol,	XK_F1,		NoSymbol,
	/* 0x3c */  XK_F2,		NoSymbol,	XK_F3,		NoSymbol,
	/* 0x3e */  XK_F4,		NoSymbol,	XK_F5,		NoSymbol,
	/* 0x40 */  XK_F6,		NoSymbol,	XK_F7,		NoSymbol,
	/* 0x42 */  XK_F8,		NoSymbol,	XK_F9,		NoSymbol,
	/* 0x44 */  XK_F10,		NoSymbol,	XK_Num_Lock,	NoSymbol,
	/* 0x46 */  XK_Scroll_Lock,	NoSymbol,	XK_KP_Home,	XK_KP_7,
	/* 0x48 */  XK_KP_Up,		XK_KP_8,	XK_KP_Prior,	XK_KP_9,
	/* 0x4a */  XK_KP_Subtract,	NoSymbol,	XK_KP_Left,	XK_KP_4,
	/* 0x4c */  XK_KP_Begin,	XK_KP_5,	XK_KP_Right,	XK_KP_6,
	/* 0x4e */  XK_KP_Add,		NoSymbol,	XK_KP_End,	XK_KP_1,
	/* 0x50 */  XK_KP_Down,		XK_KP_2,	XK_KP_Next,	XK_KP_3,
	/* 0x52 */  XK_KP_Insert,	XK_KP_0,	XK_KP_Delete,	XK_KP_Decimal,
	/* 0x54 */  NoSymbol,		NoSymbol,	XK_F13,		NoSymbol,
	/* 0x56 */  XK_less,		XK_greater,	XK_F11,		NoSymbol,
	/* 0x58 */  XK_F12,		NoSymbol,	XK_F14,		NoSymbol,
	/* 0x5a */  XK_F15,		NoSymbol,	XK_F16,		NoSymbol,
	/* 0x5c */  XK_F17,		NoSymbol,	XK_F18,		NoSymbol,
	/* 0x5e */  XK_F19,		NoSymbol,	XK_F20,		NoSymbol,
	/* 0x60 */  XK_KP_Enter,	NoSymbol,	XK_Control_R,	NoSymbol,
	/* 0x62 */  XK_KP_Divide,	NoSymbol,	XK_Print,	XK_Sys_Req,
	/* 0x64 */  XK_Alt_R,		XK_Meta_R,	NoSymbol,	NoSymbol,
	/* 0x66 */  XK_Home,		NoSymbol,	XK_Up,		NoSymbol,
	/* 0x68 */  XK_Prior,		NoSymbol,	XK_Left,	NoSymbol,
	/* 0x6a */  XK_Right,		NoSymbol,	XK_End,		NoSymbol,
	/* 0x6c */  XK_Down,		NoSymbol,	XK_Next,	NoSymbol,
	/* 0x6e */  XK_Insert,		NoSymbol,	XK_Delete,	NoSymbol,
	/* 0x70 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x72 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x74 */  NoSymbol,		NoSymbol,	XK_KP_Equal,	NoSymbol,
	/* 0x76 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x78 */  XK_F21,		NoSymbol,	XK_F22,		NoSymbol,
	/* 0x7a */  XK_F23,		NoSymbol,	XK_F24,		NoSymbol,
	/* 0x7c */  XK_KP_Separator,	NoSymbol,	XK_Meta_L,	NoSymbol,
	/* 0x7e */  XK_Meta_R,		NoSymbol,	XK_Multi_key,	NoSymbol,
	/* 0x80 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x82 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x84 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x86 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x88 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x8a */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x8c */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x8e */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x90 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x92 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x94 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x96 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x98 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x9a */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x9c */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0x9e */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xa0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xa2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xa4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xa6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xa8 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xaa */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xac */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xae */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xb0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xb2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xb4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xb6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xb8 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xba */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xbc */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xbe */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xc0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xc2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xc4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xc6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xc8 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xca */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xcc */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xce */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xd0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xd2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xd4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xd6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xd8 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xda */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xdc */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xde */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xe0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xe2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xe4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xe6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xe8 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xea */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xec */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xee */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xf0 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xf2 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xf4 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol,
	/* 0xf6 */  NoSymbol,		NoSymbol,	NoSymbol,	NoSymbol
};

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 5
static struct { KeySym keysym; CARD8 mask; } keymod[] = {
	{ XK_Shift_L,	ShiftMask },
	{ XK_Shift_R,	ShiftMask },
	{ XK_Control_L,	ControlMask },
	{ XK_Control_R,	ControlMask },
	{ XK_Caps_Lock,	LockMask },
	{ XK_Alt_L,	Mod1Mask }, /*AltMask*/
	{ XK_Alt_R,	Mod1Mask }, /*AltMask*/
	{ XK_Num_Lock,	Mod2Mask }, /*NumLockMask*/
	{ XK_Scroll_Lock,	Mod5Mask }, /*ScrollLockMask*/
	{ XK_Mode_switch,	Mod3Mask }, /*AltMask*/
	{ NoSymbol,	0 }
};
#endif

/*****************************************************************************
 * wcmInitialToolSize --
 *    Initialize logical size and resolution for individual tool.
 ****************************************************************************/

static void wcmInitialToolSize(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomToolPtr toollist = common->wcmTool;
	WacomToolAreaPtr arealist;

	/* assign max and resolution here since we don't get them during
	 * the configuration stage */
	if (IsTouch(priv))
	{
		priv->maxX = common->wcmMaxTouchX;
		priv->maxY = common->wcmMaxTouchY;
		priv->resolX = common->wcmTouchResolX;
		priv->resolY = common->wcmTouchResolY;
	}
	else
	{
		priv->maxX = common->wcmMaxX;
		priv->maxY = common->wcmMaxY;
		priv->resolX = common->wcmResolX;
		priv->resolY = common->wcmResolY;
	}

	for (; toollist; toollist=toollist->next)
	{
		arealist = toollist->arealist;
		for (; arealist; arealist=arealist->next)
		{
			if (!arealist->bottomX) 
				arealist->bottomX = priv->maxX;
			if (!arealist->bottomY)
				arealist->bottomY = priv->maxY;
		}
	}

	return;
}

/*****************************************************************************
 * wcmRegisterX11Devices --
 *    Register the X11 input devices with X11 core.
 ****************************************************************************/

static int wcmRegisterX11Devices (LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	unsigned char butmap[WCM_MAX_BUTTONS+1];
	int nbaxes, nbbuttons, nbkeys;
	int loop;
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
        Atom btn_labels[WCM_MAX_BUTTONS] = {0};
        Atom axis_labels[MAX_VALUATORS] = {0};
#endif

	/* Detect tablet configuration, if possible */
	if (priv->common->wcmModel->DetectConfig)
		priv->common->wcmModel->DetectConfig (local);

	nbaxes = priv->naxes;       /* X, Y, Pressure, Tilt-X, Tilt-Y, Wheel */
	nbbuttons = priv->nbuttons; /* Use actual number of buttons, if possible */

	/* if more than 3 buttons, offset by the four scroll buttons,
	 * otherwise, alloc 7 buttons for scroll wheel. */
	nbbuttons = (nbbuttons > 3) ? nbbuttons + 4 : 7;

	/* make sure nbbuttons stays in the range */
	if (nbbuttons > WCM_MAX_BUTTONS)
		nbbuttons = WCM_MAX_BUTTONS;

	nbkeys = nbbuttons;         /* Same number of keys since any button may be 
	                             * configured as an either mouse button or key */

	if (!nbbuttons)
		nbbuttons = nbkeys = 1;	    /* Xserver 1.5 or later crashes when 
			            	     * nbbuttons = 0 while sending a beep 
			             	     * This is only a workaround. 
				     	     */

	DBG(10, priv,
		"(%s) %d buttons, %d keys, %d axes\n",
		IsStylus(priv) ? "stylus" :
		IsCursor(priv) ? "cursor" :
		IsPad(priv) ? "pad" : "eraser",
		nbbuttons, nbkeys, nbaxes);

	for(loop=1; loop<=nbbuttons; loop++)
		butmap[loop] = loop;

	/* FIXME: button labels would be nice */
	if (InitButtonClassDeviceStruct(local->dev, nbbuttons,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					btn_labels,
#endif
					butmap) == FALSE)
	{
		xf86Msg(X_ERROR, "%s: unable to allocate Button class device\n", local->name);
		return FALSE;
	}

	if (InitFocusClassDeviceStruct(local->dev) == FALSE)
	{
		xf86Msg(X_ERROR, "%s: unable to init Focus class device\n", local->name);
		return FALSE;
	}

	if (InitPtrFeedbackClassDeviceStruct(local->dev,
		wcmDevControlProc) == FALSE)
	{
		xf86Msg(X_ERROR, "%s: unable to init ptr feedback\n", local->name);
		return FALSE;
	}

	if (InitProximityClassDeviceStruct(local->dev) == FALSE)
	{
			xf86Msg(X_ERROR, "%s: unable to init proximity class device\n", local->name);
			return FALSE;
	}

	if (!nbaxes || nbaxes > 6)
		nbaxes = priv->naxes = 6;

	/* axis_labels is just zeros, we set up each valuator with the
	 * correct property later */
	if (InitValuatorClassDeviceStruct(local->dev, nbaxes,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
					  axis_labels,
#endif
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 3
					  GetMotionHistory,
#endif
					  GetMotionHistorySize(),
					  ((priv->flags & ABSOLUTE_FLAG) ?
					  Absolute : Relative) | 
					  OutOfProximity ) == FALSE)
	{
		xf86Msg(X_ERROR, "%s: unable to allocate Valuator class device\n", local->name);
		return FALSE;
	}


	/* only initial KeyClass and LedFeedbackClass once */
	if (!priv->wcmInitKeyClassCount)
	{
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 5
		if (nbkeys)
		{
			KeySymsRec wacom_keysyms;
			CARD8 modmap[MAP_LENGTH];
			int i,j;

			memset(modmap, 0, sizeof(modmap));
			for(i=0; keymod[i].keysym != NoSymbol; i++)
				for(j=8; j<256; j++)
					if(keymap[(j-8)*2] == keymod[i].keysym)
						modmap[j] = keymod[i].mask;

			/* There seems to be a long-standing misunderstanding about
			 * how a keymap should be defined. All tablet drivers from
			 * stock X11 source tree are doing it wrong: they leave first
			 * 8 keysyms as VoidSymbol's, and are passing 8 as minimum
			 * key code. But if you look at SetKeySymsMap() from
			 * programs/Xserver/dix/devices.c you will see that
			 * Xserver does not require first 8 keysyms; it supposes
			 * that the map begins at minKeyCode.
			 *
			 * It could be that this assumption is a leftover from
			 * earlier XFree86 versions, but that's out of our scope.
			 * This also means that no keys on extended input devices
			 * with their own keycodes (e.g. tablets) were EVER used.
			 */
			wacom_keysyms.map = keymap;
			/* minKeyCode = 8 because this is the min legal key code */
			wacom_keysyms.minKeyCode = 8;
			wacom_keysyms.maxKeyCode = 255;
			wacom_keysyms.mapWidth = 2;
			if (InitKeyClassDeviceStruct(local->dev, &wacom_keysyms, modmap) == FALSE)
			{
				xf86Msg(X_ERROR, "%s: unable to init key class device\n", local->name);
				return FALSE;
			}
		}

		if(InitKbdFeedbackClassDeviceStruct(local->dev, wcmBellCallback,
				wcmKbdCtrlCallback) == FALSE) {
			xf86Msg(X_ERROR, "%s: unable to init kbd feedback device struct\n", local->name);
			return FALSE;
		}
#elif GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
		if (InitKeyboardDeviceStruct(local->dev, NULL, NULL, wcmKbdCtrlCallback)) {
#define SYMS_PER_KEY 2
			KeySymsRec syms;
			CARD8 modmap[MAP_LENGTH];
			int num_keys = XkbMaxLegalKeyCode - XkbMinLegalKeyCode + 1;

			syms.map = keymap;
			syms.mapWidth = SYMS_PER_KEY;
			syms.minKeyCode = XkbMinLegalKeyCode;
			syms.maxKeyCode = XkbMaxLegalKeyCode;

			memset(modmap, 0, sizeof(modmap));
			modmap[XkbMinLegalKeyCode + 2] = ShiftMask;
			XkbApplyMappingChange(local->dev, &syms, syms.minKeyCode, num_keys, NULL, // modmap,
					serverClient);
		} else
		{
			xf86Msg(X_ERROR, "%s: unable to init kbd device struct\n", local->name);
			return FALSE;
		}
#endif
		if(InitLedFeedbackClassDeviceStruct (local->dev, wcmKbdLedCallback) == FALSE) {
			xf86Msg(X_ERROR, "%s: unable to init led feedback device struct\n", local->name);
			return FALSE;
		}
	}

	wcmInitialToolSize(local);

	if (wcmInitArea(local) == FALSE)
	{
		return FALSE;
	}

	/* Rotation rotates the Max X and Y */
	wcmRotateTablet(local, common->wcmRotate);

	/* pressure */
	InitValuatorAxisStruct(local->dev, 2,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
		XIGetKnownProperty(AXIS_LABEL_PROP_ABS_PRESSURE),
#endif
		0, common->wcmMaxZ, 1, 1, 1);

	if (IsCursor(priv))
	{
		/* z-rot and throttle */
		InitValuatorAxisStruct(local->dev, 3,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
		XIGetKnownProperty(AXIS_LABEL_PROP_ABS_RZ),
#endif
		-900, 899, 1, 1, 1);
		InitValuatorAxisStruct(local->dev, 4,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
		XIGetKnownProperty(AXIS_LABEL_PROP_ABS_THROTTLE),
#endif
		-1023, 1023, 1, 1, 1);
	}
	else if (IsPad(priv))
	{
		/* strip-x and strip-y */
		if (strstr(common->wcmModel->name, "Intuos3") || 
			strstr(common->wcmModel->name, "CintiqV5")) 
		{
			InitValuatorAxisStruct(local->dev, 3,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				0, /* XXX what is this axis?*/
#endif
				0, common->wcmMaxStripX, 1, 1, 1);
			InitValuatorAxisStruct(local->dev, 4,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				0, /* XXX what is this axis?*/
#endif
				0, common->wcmMaxStripY, 1, 1, 1);
		}
	}
	else
	{
		/* tilt-x and tilt-y */
		InitValuatorAxisStruct(local->dev, 3,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_X),
#endif
				-64, 63, 1, 1, 1);
		InitValuatorAxisStruct(local->dev, 4,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				XIGetKnownProperty(AXIS_LABEL_PROP_ABS_TILT_Y),
#endif
				-64, 63, 1, 1, 1);
	}

	if ((strstr(common->wcmModel->name, "Intuos3") || 
		strstr(common->wcmModel->name, "CintiqV5") ||
		strstr(common->wcmModel->name, "Intuos4")) 
			&& IsStylus(priv))
		/* Art Marker Pen rotation */
		InitValuatorAxisStruct(local->dev, 5,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				0, /* XXX what is this axis?*/
#endif
				-900, 899, 1, 1, 1);
	else if ((strstr(common->wcmModel->name, "Bamboo") ||
		strstr(common->wcmModel->name, "Intuos4"))
			&& IsPad(priv))
		/* Touch ring */
		InitValuatorAxisStruct(local->dev, 5,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				0, /* XXX what is this axis?*/
#endif
				0, 71, 1, 1, 1);
	else
	{
		/* absolute wheel */
		InitValuatorAxisStruct(local->dev, 5,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
				XIGetKnownProperty(AXIS_LABEL_PROP_ABS_WHEEL),
#endif
				0, 1023, 1, 1, 1);
	}

	if (IsTouch(priv))
	{
		/* hard prox out */
		priv->hardProx = 0;
	}

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3
	InitWcmDeviceProperties(local);
	XIRegisterPropertyHandler(local->dev, wcmSetProperty, NULL, NULL);
#endif

	return TRUE;
}

Bool wcmIsWacomDevice (char* fname)
{
	int fd = -1;
	struct input_id id;

	SYSCALL(fd = open(fname, O_RDONLY));
	if (fd < 0)
		return FALSE;

	if (ioctl(fd, EVIOCGID, &id) < 0)
	{
		SYSCALL(close(fd));
		return FALSE;
	}

	SYSCALL(close(fd));

	if (id.vendor == WACOM_VENDOR_ID)
		return TRUE;
	else
		return FALSE;
}

/*****************************************************************************
 * wcmEventAutoDevProbe -- Probe for right input device
 ****************************************************************************/
#define DEV_INPUT_EVENT "/dev/input/event%d"
#define EVDEV_MINORS    32
char *wcmEventAutoDevProbe (LocalDevicePtr local)
{
	/* We are trying to find the right eventX device */
	int i, wait = 0;
	const int max_wait = 2000;

	/* If device is not available after Resume, wait some ms */
	while (wait <= max_wait) 
	{
		for (i = 0; i < EVDEV_MINORS; i++) 
		{
			char fname[64];
			Bool is_wacom;

			sprintf(fname, DEV_INPUT_EVENT, i);
			is_wacom = wcmIsWacomDevice(fname);
			if (is_wacom) 
			{
				xf86Msg(X_PROBED, "%s: probed device is %s (waited %d msec)\n",
					local->name, fname, wait);
				xf86ReplaceStrOption(local->options, "Device", fname);

				/* this assumes there is only one Wacom device on the system */
				return xf86FindOptionValue(local->options, "Device");
			}
		}
		wait += 100;
		xf86Msg(X_ERROR, "%s: waiting 100 msec (total %dms) for device to become ready\n", local->name, wait);
		usleep(100*1000);
	}
	xf86Msg(X_ERROR, "%s: no Wacom event device found (checked %d nodes, waited %d msec)\n",
		local->name, i + 1, wait);
	return FALSE;
}

/*****************************************************************************
 * wcmOpen --
 ****************************************************************************/

static Bool wcmOpen(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	char id[BUFFER_SIZE];
	float version;
	int rc;
	struct serial_struct ser;

	DBG(1, priv, "opening device file\n");

	local->fd = xf86OpenSerial(local->options);
	if (local->fd < 0)
	{
		xf86Msg(X_ERROR, "%s: Error opening %s (%s)\n", local->name,
			common->wcmDevice, strerror(errno));
		return !Success;
	}

	rc = ioctl(local->fd, TIOCGSERIAL, &ser);

	/* we initialized wcmDeviceClasses to USB
	 * Bluetooth is also considered as USB */
	if (rc == 0) /* serial device */
	{
		/* only ISDV4 are supported on X server 1.7 and later */
		common->wcmForceDevice=DEVICE_ISDV4;
		common->wcmDevCls = &gWacomISDV4Device;

		/* Tablet PC buttons on by default */
		common->wcmTPCButtonDefault = 1;
	}
	else
	{
		/* Detect USB device class */
		if ((&gWacomUSBDevice)->Detect(local))
			common->wcmDevCls = &gWacomUSBDevice;
		else
		{
			xf86Msg(X_ERROR, "%s: wcmOpen found undetectable "
				" %s \n", local->name, common->wcmDevice);
			return !Success;
		}
	}

	/* Initialize the tablet */
	if(common->wcmDevCls->Init(local, id, &version) != Success ||
		wcmInitTablet(local, id, version) != Success)
	{
		xf86CloseSerial(local->fd);
		local->fd = -1;
		return !Success;
	}
	return Success;
}

/*****************************************************************************
 * wcmDevOpen --
 *    Open the physical device and init information structs.
 ****************************************************************************/

static int wcmDevOpen(DeviceIntPtr pWcm)
{
	LocalDevicePtr local = (LocalDevicePtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	struct stat st;

	DBG(10, priv, "\n");

	/* Device has been open and not autoprobed */
	if (priv->wcmDevOpenCount)
		return TRUE;

	/* open file, if not already open */
	if (common->fd_refs == 0)
	{
		/* Autoprobe if necessary */
		if ((common->wcmFlags & AUTODEV_FLAG) &&
		    !(common->wcmDevice = wcmEventAutoDevProbe (local)))
			xf86Msg(X_ERROR, "%s: Cannot probe device\n", local->name);

		if ((wcmOpen (local) != Success) || (local->fd < 0) ||
			!common->wcmDevice)
		{
			DBG(1, priv, "Failed to open "
				"device (fd=%d)\n", local->fd);
			if (local->fd >= 0)
			{
				DBG(1, priv, "Closing device\n");
				xf86CloseSerial(local->fd);
			}
			local->fd = -1;
			return FALSE;
		}

		if (fstat(local->fd, &st) == -1)
		{
			/* can not access major/minor */
			DBG(1, priv, "stat failed (%s). "
				"cannot check status.\n", strerror(errno));

			/* older systems don't support the required ioctl.
			 * So, we have to let it pass */
			common->min_maj = 0;
		}
		else
			common->min_maj = st.st_rdev;
		common->fd = local->fd;
		common->fd_refs = 1;
	}

	/* Grab the common descriptor, if it's available */
	if (local->fd < 0)
	{
		local->fd = common->fd;
		common->fd_refs++;
	}

	if (!wcmRegisterX11Devices (local))
		return FALSE;

	return TRUE;
}

static int wcmReady(LocalDevicePtr local)
{
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
#endif
	int n = xf86WaitForInput(local->fd, 0);
	DBG(10, priv, "%d numbers of data\n", n);

	if (n >= 0) return n ? 1 : 0;
	xf86Msg(X_ERROR, "%s: select error: %s\n", local->name, strerror(errno));
	return 0;
}

/*****************************************************************************
 * wcmDevReadInput --
 *   Read the device on IO signal
 ****************************************************************************/

static void wcmDevReadInput(LocalDevicePtr local)
{
	int loop=0;
	#define MAX_READ_LOOPS 10

	/* move data until we exhaust the device */
	for (loop=0; loop < MAX_READ_LOOPS; ++loop)
	{
		/* verify that there is still data in pipe */
		if (!wcmReady(local)) break;

		/* dispatch */
		wcmReadPacket(local);
	}

#ifdef DEBUG
	/* report how well we're doing */
	if (loop > 0)
	{
		WacomDevicePtr priv = (WacomDevicePtr)local->private;

		if (loop >= MAX_READ_LOOPS)
			DBG(1, priv, "Can't keep up!!!\n");
		else
			DBG(10, priv, "Read (%d)\n",loop);
	}
#endif
}

void wcmReadPacket(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	int len, pos, cnt, remaining;

	DBG(10, common, "fd=%d\n", local->fd);

	remaining = sizeof(common->buffer) - common->bufpos;

	DBG(1, common, "pos=%d"
		" remaining=%d\n", common->bufpos, remaining);

	/* fill buffer with as much data as we can handle */
	len = xf86ReadSerial(local->fd,
		common->buffer + common->bufpos, remaining);

	if (len <= 0)
	{
		/* In case of error, we assume the device has been
		 * disconnected. So we close it and iterate over all
		 * wcmDevices to actually close associated devices. */
		WacomDevicePtr wDev = common->wcmDevices;
		for(; wDev; wDev = wDev->next)
		{
			if (wDev->local->fd >= 0)
				wcmDevProc(wDev->local->dev, DEVICE_OFF);
		}
		xf86Msg(X_ERROR, "%s: Error reading wacom device : %s\n", local->name, strerror(errno));
		return;
	}

	/* account for new data */
	common->bufpos += len;
	DBG(10, common, "buffer has %d bytes\n", common->bufpos);

	len = common->bufpos;
	pos = 0;

	while (len > 0)
	{
		/* parse packet */
		cnt = common->wcmModel->Parse(local, common->buffer + pos, len);
		if (cnt <= 0)
		{
			if (cnt < 0)
				DBG(1, common, "Misbehaving parser returned %d\n",cnt);
			break;
		}
		pos += cnt;
		len -= cnt;
	}

	/* if half a packet remains, move it down */
	if (len)
	{
		DBG(7, common, "MOVE %d bytes\n", common->bufpos - pos);
		memmove(common->buffer,common->buffer+pos, len);
	}

	common->bufpos = len;
}

int wcmDevChangeControl(LocalDevicePtr local, xDeviceCtl * control)
{
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	DBG(3, priv, "\n");
#endif
	return Success;
}

/*****************************************************************************
 * wcmDevControlProc --
 ****************************************************************************/

static void wcmDevControlProc(DeviceIntPtr device, PtrCtrl* ctrl)
{
#ifdef DEBUG
	LocalDevicePtr local = (LocalDevicePtr)device->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	DBG(4, priv, "called\n");
#endif
	return;
}

/*****************************************************************************
 * wcmDevClose --
 ****************************************************************************/

static void wcmDevClose(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(4, priv, "Wacom number of open devices = %d\n", common->fd_refs);

	if (local->fd >= 0)
	{
		local->fd = -1;
		if (!--common->fd_refs)
		{
			DBG(1, common, "Closing device; uninitializing.\n");
			xf86CloseSerial (common->fd);
		}
	}
}
 
/*****************************************************************************
 * wcmDevProc --
 *   Handle the initialization, etc. of a wacom
 ****************************************************************************/

static int wcmDevProc(DeviceIntPtr pWcm, int what)
{
	LocalDevicePtr local = (LocalDevicePtr)pWcm->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;

	DBG(2, priv, "BEGIN dev=%p priv=%p "
			"type=%s flags=%d fd=%d what=%s\n",
			(void *)pWcm, (void *)priv,
			IsStylus(priv) ? "stylus" :
			IsCursor(priv) ? "cursor" :
			IsPad(priv) ? "pad" : "eraser", 
			priv->flags, local ? local->fd : -1,
			(what == DEVICE_INIT) ? "INIT" :
			(what == DEVICE_OFF) ? "OFF" :
			(what == DEVICE_ON) ? "ON" :
			(what == DEVICE_CLOSE) ? "CLOSE" : "???");

	switch (what)
	{
		/* All devices must be opened here to initialize and
		 * register even a 'pad' which doesn't "SendCoreEvents"
		 */
		case DEVICE_INIT:
			priv->wcmDevOpenCount = 0;
			priv->wcmInitKeyClassCount = 0;
			if (!wcmDevOpen(pWcm))
			{
				DBG(1, priv, "INIT FAILED\n");
				return !Success;
			}
			priv->wcmInitKeyClassCount++;
			priv->wcmDevOpenCount++;
			break; 

		case DEVICE_ON:
			if (!wcmDevOpen(pWcm))
			{
				DBG(1, priv, "ON FAILED\n");
				return !Success;
			}
			priv->wcmDevOpenCount++;
			xf86AddEnabledDevice(local);
			pWcm->public.on = TRUE;
			break;

		case DEVICE_OFF:
		case DEVICE_CLOSE:
			if (local->fd >= 0)
			{
				xf86RemoveEnabledDevice(local);
				wcmDevClose(local);
			}
			pWcm->public.on = FALSE;
			priv->wcmDevOpenCount = 0;
			break;

		default:
			xf86Msg(X_ERROR, "%s: wacom unsupported mode=%d\n", local->name, what);
			return !Success;
			break;
	} /* end switch */

	DBG(2, priv, "END Success \n");
	return Success;
}

/* vim: set noexpandtab shiftwidth=8: */
