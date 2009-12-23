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
#include <math.h>

/* Defines for 2FC Gesture */
#define WACOM_DIST_IN_POINT         300
#define WACOM_APART_IN_POINT        350
#define WACOM_MOTION_IN_POINT        50
#define WACOM_PARA_MOTION_IN_POINT   50
#define WACOM_DOWN_TIME_IN_MS       800
#define WACOM_TIME_BETWEEN_IN_MS    400
#define WACOM_HORIZ_ALLOWED           1
#define WACOM_VERT_ALLOWED            2

#define GESTURE_TAP_MODE              1
#define GESTURE_SCROLL_MODE           2
#define GESTURE_ZOOM_MODE             4

#define WCM_SCROLL_UP                 5	/* vertical up */
#define WCM_SCROLL_DOWN               4	/* vertical down */
#define WCM_SCROLL_LEFT               6	/* horizontal left */
#define WCM_SCROLL_RIGHT              7	/* horizontal right */


/* Defines for Tap Add-a-Finger to Click */
#define WACOM_TAP_TIME_IN_MS        150

void xf86WcmFingerTapToClick(WacomCommonPtr common);

extern void xf86WcmRotateCoordinates(LocalDevicePtr local, int x, int y);
extern void emitKeysym (DeviceIntPtr keydev, int keysym, int state);

static void xf86WcmFingerScroll(WacomDevicePtr priv);
static void xf86WcmFingerZoom(WacomDevicePtr priv);

static double touchDistance(WacomDeviceState ds0, WacomDeviceState ds1)
{
	int xDelta = ds0.x - ds1.x;
	int yDelta = ds0.y - ds1.y;
	double distance = sqrt((double)(xDelta*xDelta + yDelta*yDelta));
	return distance;
}

static Bool pointsInLine(WacomDeviceState ds0, WacomDeviceState ds1,
				int *direction)
{
	Bool ret = FALSE;

	if (*direction == 0)
	{
		if (abs(ds0.x - ds1.x) < WACOM_PARA_MOTION_IN_POINT)
		{
			*direction = WACOM_VERT_ALLOWED;
			ret = TRUE;
		}
		else if (abs(ds0.y - ds1.y) < WACOM_PARA_MOTION_IN_POINT)
		{
			*direction = WACOM_HORIZ_ALLOWED;
			ret = TRUE;
		}
	}
	else if (*direction == WACOM_HORIZ_ALLOWED)
	{
		if (abs(ds0.y - ds1.y) < WACOM_PARA_MOTION_IN_POINT)
			ret = TRUE;
	}
	else if (*direction == WACOM_VERT_ALLOWED)
	{
		if (abs(ds0.x - ds1.x) < WACOM_PARA_MOTION_IN_POINT)
			ret = TRUE;
	}
	return ret;
}

static Bool pointsInLineAfter(int p1, int p2)
{
	Bool ret = FALSE;

	if (abs(p1 - p2) < WACOM_PARA_MOTION_IN_POINT)
		ret = TRUE;

	return ret;
}

static void xf86WcmSwitchLeftClick(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;

	if (common->wcmGestureMode)
	{
		/* send button one up */
		xf86PostButtonEvent(priv->local->dev,
					priv->flags & ABSOLUTE_FLAG,
					1,0,0,priv->naxes, priv->oldX,
					priv->oldY,0,0,0,0);
		priv->oldButtons = 0;
	}
}

/*****************************************************************************
 *   translate second finger tap to right click
 ****************************************************************************/

void xf86WcmFingerTapToClick(WacomCommonPtr common)
{
	WacomDevicePtr priv = common->wcmDevices;
	WacomChannelPtr firstChannel = common->wcmChannel;
	WacomChannelPtr secondChannel = common->wcmChannel + 1;
	WacomDeviceState ds[2] = { firstChannel->valid.states[0],
				   secondChannel->valid.states[0] };
	WacomDeviceState dsLast[2] = { firstChannel->valid.states[1],
					secondChannel->valid.states[1] };
	int direction = 0;

	DBG(10, priv->debugLevel, "\n");

	/* skip initial second finger event */
	if (!dsLast[1].proximity)
		goto skipGesture;

	if (!IsTouch(priv))
	{
		/* go through the shared port */
		for (; priv; priv = priv->next)
			if (IsTouch(priv))
				break;
	}

	if (priv)  /* found the first finger */
	{
		/* allow only second finger tap */
		if ((dsLast[0].sample < dsLast[1].sample) && ((GetTimeInMillis() -
						dsLast[1].sample) <= WACOM_TAP_TIME_IN_MS))
		{
			/* send right click when second finger taps within WACOM_TAP_TIMEms
			 * and both fingers stay within WACOM_DIST */
			if (!ds[1].proximity && dsLast[1].proximity)
			{
				if (touchDistance(ds[0], dsLast[1]) <= WACOM_DIST_IN_POINT)
				{
					/* send left up before sending right down */
					if (!common->wcmGestureMode)
					{
						common->wcmGestureMode = GESTURE_TAP_MODE;
						xf86WcmSwitchLeftClick(priv);
					}

					/* right button down */
					xf86PostButtonEvent(priv->local->dev,
							priv->flags & ABSOLUTE_FLAG,
							3,1,0,priv->naxes, priv->oldX,
							priv->oldY,0,0,0,0);
					/* right button up */
					xf86PostButtonEvent(priv->local->dev,
							priv->flags & ABSOLUTE_FLAG,
							3,0,0,priv->naxes, priv->oldX,
							priv->oldY,0,0,0,0);
				}
			}
		}
		else if ((WACOM_TAP_TIME_IN_MS < (GetTimeInMillis() - dsLast[0].sample))
				&& (WACOM_TAP_TIME_IN_MS < (GetTimeInMillis() - dsLast[1].sample))
				&& ds[0].proximity && ds[1].proximity)
		{
			if (abs(touchDistance(ds[0], ds[1])) >= WACOM_APART_IN_POINT &&
					common->wcmGestureMode != GESTURE_TAP_MODE &&
					common->wcmGestureMode != GESTURE_SCROLL_MODE)
			{
				/* fingers moved apart more than WACOM_APART_IN_POINT
				 * zoom mode is entered */
				if (!common->wcmGestureMode)
				{
					common->wcmGestureMode = GESTURE_ZOOM_MODE;
					xf86WcmSwitchLeftClick(priv);
				}
				xf86WcmFingerZoom(priv);
			}

			if ( pointsInLine(ds[0], dsLast[0], &direction) &&
					pointsInLine(ds[1], dsLast[1], &direction) &&
					common->wcmGestureMode != GESTURE_ZOOM_MODE &&
					common->wcmGestureMode != GESTURE_TAP_MODE)
			{
				/* send scroll event when both fingers move in
				 * the same direction */
				if (!common->wcmGestureMode)
				{
					common->wcmGestureMode = GESTURE_SCROLL_MODE;
					xf86WcmSwitchLeftClick(priv);
				}
				xf86WcmFingerScroll(priv);
			}
		}
	}
	else
		/* this should never happen */
		xf86Msg(X_ERROR, "WACOM: No touch device found for %s \n", common->wcmDevice);

skipGesture:
	/* keep the initial in-prox time */
	ds[0].sample = dsLast[0].sample;
	ds[1].sample = dsLast[1].sample;

	/* keep the initial states for both fingers */
	if ( !(common->wcmGestureMode && (GESTURE_SCROLL_MODE | GESTURE_ZOOM_MODE))
			&& ds[0].proximity && ds[1].proximity)
	{
		common->wcmGestureState[0] = ds[0];
		common->wcmGestureState[1] = ds[1];
	}
}

static void xf86WcmSendVerticalScrollEvent(WacomDevicePtr priv,
		int dist, int up, int dn)
{
	int i = 0;

	for (i=0; i<(int)(((double)abs(dist)/
			  (double)WACOM_MOTION_IN_POINT) + 0.5); i++)
	{
		if (dist > 0)
		{
			/* button down */
			xf86PostButtonEvent(priv->local->dev,
					priv->flags & ABSOLUTE_FLAG,
					up,1,0,priv->naxes, priv->oldX,
					priv->oldY,0,0,0,0);
			/* button up */
			xf86PostButtonEvent(priv->local->dev,
					priv->flags & ABSOLUTE_FLAG,
					up,0,0,priv->naxes, priv->oldX,
					priv->oldY,0,0,0,0);
		}
		else
		{
			/* button down */
			xf86PostButtonEvent(priv->local->dev,
					priv->flags & ABSOLUTE_FLAG,
					dn,1,0,priv->naxes, priv->oldX,
					priv->oldY,0,0,0,0);
			/* button up */
			xf86PostButtonEvent(priv->local->dev,
					priv->flags & ABSOLUTE_FLAG,
					dn,0,0,priv->naxes, priv->oldX,
					priv->oldY,0,0,0,0);
		}
	}
}

static void xf86WcmSendHorizontalScrollEvent(WacomDevicePtr priv,
					     int dist, int left, int right)
{
	int i = 0;

	for (i=0; i<(int)(((double)abs(dist)/
			   (double)WACOM_MOTION_IN_POINT) + 0.5); i++)
	{
		if (dist > 0)
		{
			/* button down */
			xf86PostButtonEvent(priv->local->dev,
					priv->flags & ABSOLUTE_FLAG,
					left,1,0,priv->naxes, priv->oldX,
					priv->oldY,0,0,0,0);
			/* button up */
			xf86PostButtonEvent(priv->local->dev,
					priv->flags & ABSOLUTE_FLAG,
					left,0,0,priv->naxes, priv->oldX,
					priv->oldY,0,0,0,0);
		}
		else
		{
			/* button down */
			xf86PostButtonEvent(priv->local->dev,
					priv->flags & ABSOLUTE_FLAG,
					right,1,0,priv->naxes, priv->oldX,
					priv->oldY,0,0,0,0);
			/* button up */
			xf86PostButtonEvent(priv->local->dev,
					priv->flags & ABSOLUTE_FLAG,
					right,0,0,priv->naxes, priv->oldX,
					priv->oldY,0,0,0,0);
		}
	}
}

static void xf86WcmFingerScroll(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomChannelPtr firstChannel = common->wcmChannel;
	WacomChannelPtr secondChannel = common->wcmChannel + 1;
	WacomDeviceState ds[2] = { firstChannel->valid.states[0],
		secondChannel->valid.states[0] };
	WacomDeviceState dsLast[2] = { firstChannel->valid.states[1],
		secondChannel->valid.states[1] };
	int midPoint_new = 0;
	int midPoint_old = 0;
	int i = 0, dist =0;
	int gesture = 0;
	WacomFilterState filterd;  /* borrow this struct */

	DBG(10, priv->debugLevel, "\n");

	/* initialize the points so we can rotate them */
	filterd.x[0] = ds[0].x;
	filterd.y[0] = ds[0].y;
	filterd.x[1] = ds[1].x;
	filterd.y[1] = ds[1].y;
	filterd.x[2] = common->wcmGestureState[0].x;
	filterd.y[2] = common->wcmGestureState[0].y;
	filterd.x[3] = common->wcmGestureState[1].x;
	filterd.y[3] = common->wcmGestureState[1].y;
	filterd.x[4] = dsLast[0].x;
	filterd.y[4] = dsLast[0].y;
	filterd.x[5] = dsLast[1].x;
	filterd.y[5] = dsLast[1].y;

	/* rotate the coordinates first */
	for (i=0; i<6; i++)
		xf86WcmRotateCoordinates(priv->local, filterd.x[i], filterd.y[i]);

	/* check vertical direction */
	midPoint_old = (((double)filterd.x[4] + (double)filterd.x[5]) / 2.);
	midPoint_new = (((double)filterd.x[0] + (double)filterd.x[1]) / 2.);
	if (pointsInLineAfter(midPoint_old, midPoint_new))
	{
		midPoint_old = (((double)filterd.y[2] + (double)filterd.y[3]) / 2.);
		midPoint_new = (((double)filterd.y[0] + (double)filterd.y[1]) / 2.);
		dist = midPoint_old - midPoint_new;

		if (abs(dist) > WACOM_PARA_MOTION_IN_POINT)
		{
			gesture = 1;
			xf86WcmSendVerticalScrollEvent(priv,  dist,
					WCM_SCROLL_UP, WCM_SCROLL_DOWN);
		}

		/* check horizontal direction */
		if (!gesture)
		{
			midPoint_old = (((double)filterd.y[4] + (double)filterd.y[5]) / 2.);
			midPoint_new = (((double)filterd.y[0] + (double)filterd.y[1]) / 2.);
			if (pointsInLineAfter(midPoint_old, midPoint_new))
			{
				midPoint_old = (((double)filterd.x[2] + (double)filterd.x[3]) / 2.);
				midPoint_new = (((double)filterd.x[0] + (double)filterd.x[1]) / 2.);
				dist = midPoint_old - midPoint_new;

				if (abs(dist) > WACOM_PARA_MOTION_IN_POINT)
				{
					gesture = 1;
					xf86WcmSendHorizontalScrollEvent(priv, dist,
						WCM_SCROLL_LEFT, WCM_SCROLL_RIGHT);
				}
			}
		}
		if (gesture)
		{
			/* reset initial states */
			common->wcmGestureState[0] = ds[0];
			common->wcmGestureState[1] = ds[1];
		}
	}
}

static void xf86WcmFingerZoom(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomChannelPtr firstChannel = common->wcmChannel;
	WacomChannelPtr secondChannel = common->wcmChannel + 1;
	WacomDeviceState ds[2] = { firstChannel->valid.states[0],
		secondChannel->valid.states[0] };
	int i = 0;
	int dist = touchDistance(common->wcmGestureState[0],
			common->wcmGestureState[1]);

	DBG(10, priv->debugLevel, "\n");

	dist = touchDistance(ds[0], ds[1]) - dist;

	/* zooming? */
	if (abs(dist) > WACOM_MOTION_IN_POINT)
	{
		for (i=0; i<(int)(((double)abs(dist)/
				(double)WACOM_MOTION_IN_POINT) + 0.5); i++)
		{
			emitKeysym (priv->local->dev, XK_Control_L, 1);
			/* zooming in */
			if (dist > 0)
			{
				emitKeysym (priv->local->dev, XK_plus, 1);
				emitKeysym (priv->local->dev, XK_plus, 0);
			}
			else /* zooming out */
			{
				emitKeysym (priv->local->dev, XK_minus, 1);
				emitKeysym (priv->local->dev, XK_minus, 0);
			}
			emitKeysym (priv->local->dev, XK_Control_L, 0);
		}

		/* reset initial states */
		common->wcmGestureState[0] = ds[0];
		common->wcmGestureState[1] = ds[1];
	}
}

/* vim: set noexpandtab shiftwidth=8: */
