/*
 * Copyright 2009 - 2010 by Ping Cheng, Wacom. <pingc@wacom.com>
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
#include <math.h>

/* Defines for 2FC Gesture */
#define WACOM_INLINE_DISTANCE        40
#define WACOM_HORIZ_ALLOWED           1
#define WACOM_VERT_ALLOWED            2
#define WACOM_GESTURE_LAG_TIME       10

#define GESTURE_TAP_MODE              1
#define GESTURE_SCROLL_MODE           2
#define GESTURE_ZOOM_MODE             4
#define GESTURE_LAG_MODE              8

#define WCM_SCROLL_UP                 5	/* vertical up */
#define WCM_SCROLL_DOWN               4	/* vertical down */
#define WCM_SCROLL_LEFT               6	/* horizontal left */
#define WCM_SCROLL_RIGHT              7	/* horizontal right */

static void wcmFingerScroll(WacomDevicePtr priv);
static void wcmFingerZoom(WacomDevicePtr priv);

static double touchDistance(WacomDeviceState ds0, WacomDeviceState ds1)
{
	int xDelta = ds0.x - ds1.x;
	int yDelta = ds0.y - ds1.y;
	double distance = sqrt((double)(xDelta*xDelta + yDelta*yDelta));
	return distance;
}

static Bool pointsInLine(WacomCommonPtr common, WacomDeviceState ds0,
		WacomDeviceState ds1)
{
	Bool ret = FALSE;

	if (!common->wcmGestureParameters.wcmScrollDirection)
	{
		if ((abs(ds0.x - ds1.x) < WACOM_INLINE_DISTANCE) &&
			(abs(ds0.y - ds1.y) > WACOM_INLINE_DISTANCE))
		{
			common->wcmGestureParameters.wcmScrollDirection = WACOM_VERT_ALLOWED;
			ret = TRUE;
		}
		if ((abs(ds0.y - ds1.y) < WACOM_INLINE_DISTANCE) &&
			(abs(ds0.x - ds1.x) > WACOM_INLINE_DISTANCE))
		{
			common->wcmGestureParameters.wcmScrollDirection = WACOM_HORIZ_ALLOWED;
			ret = TRUE;
		}
	}
	else if (common->wcmGestureParameters.wcmScrollDirection == WACOM_HORIZ_ALLOWED)
	{
		if (abs(ds0.y - ds1.y) < WACOM_INLINE_DISTANCE)
			ret = TRUE;
	}
	else if (common->wcmGestureParameters.wcmScrollDirection == WACOM_VERT_ALLOWED)
	{
		if (abs(ds0.x - ds1.x) < WACOM_INLINE_DISTANCE)
			ret = TRUE;
	}
	return ret;
}

/* send a button event */
static void wcmSendButtonClick(WacomDevicePtr priv, int button, int state)
{
	int x = 0;
	int y = 0;
	int mode = is_absolute(priv->local);

	if (mode)
	{
		x = priv->oldX;
		y = priv->oldY;
	}

	/* send button event in state */
	xf86PostButtonEvent(priv->local->dev, mode,button,
		state,0,priv->naxes,x,y,0,0,0,0);

	/* We have changed the button state (from down to up) for the device
	 * so we need to update the record */
	if (button == 1)
		priv->oldButtons = 0;
}

/*****************************************************************************
 *   translate second finger tap to right click
 ****************************************************************************/

static void wcmFingerTapToClick(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomChannelPtr firstChannel = common->wcmChannel;
	WacomChannelPtr secondChannel = common->wcmChannel + 1;
	WacomDeviceState ds[2] = { firstChannel->valid.states[0],
				   secondChannel->valid.states[0] };
	WacomDeviceState dsLast[2] = { firstChannel->valid.states[1],
					secondChannel->valid.states[1] };

	DBG(10, priv, "\n");

	/* process second finger tap if matched */
	if ((ds[0].sample < ds[1].sample) &&
	    ((GetTimeInMillis() -
	    dsLast[1].sample) <= common->wcmGestureParameters.wcmTapTime) &&
	    !ds[1].proximity && dsLast[1].proximity)
	{
		/* send left up before sending right down */
		wcmSendButtonClick(priv, 1, 0);
		common->wcmGestureMode = GESTURE_TAP_MODE;

		/* right button down */
		wcmSendButtonClick(priv, 3, 1);

		/* right button up */
		wcmSendButtonClick(priv, 3, 0);
	}
}


/* process single finger Relative mode events
 * if touch is not in an active gesture mode.
 */
static void wcmFirstFingerClick(WacomCommonPtr common)
{
	static int tmpStamp = 0;
	WacomChannelPtr aChannel = common->wcmChannel;
	WacomDeviceState ds = aChannel->valid.states[0];
	WacomDeviceState dsLast = aChannel->valid.states[1];
	if (ds.proximity)
	{
		if (common->wcmTouchpadMode)
			/* continuing left button down */
			aChannel->valid.states[0].buttons |= 1;
		else if (!dsLast.proximity &&
		    (abs(tmpStamp - ds.sample) <= common->wcmGestureParameters.wcmTapTime))
		{
			/* initial left button down */
			aChannel->valid.states[0].buttons |= 1;
			common->wcmTouchpadMode = 1;
		}
	} else {
		tmpStamp = GetTimeInMillis();
		if (common->wcmTouchpadMode)
			aChannel->valid.states[0].buttons &= ~1;
		common->wcmTouchpadMode = 0;
	}
}

/* parsing gesture mode according to 2FGT data */
void wcmGestureFilter(WacomDevicePtr priv, int channel)
{
	WacomCommonPtr common = priv->common;
	WacomChannelPtr firstChannel = common->wcmChannel;
	WacomChannelPtr secondChannel = common->wcmChannel + 1;
	WacomDeviceState ds[2] = { firstChannel->valid.states[0],
				secondChannel->valid.states[0] };
	WacomDeviceState dsLast[2] = { firstChannel->valid.states[1],
				secondChannel->valid.states[1] };

	DBG(10, priv, "\n");

	if (!IsTouch(priv))
	{
		/* this should never happen */
		xf86Msg(X_ERROR, "WACOM: No touch device found for %s \n",
			 common->device_path);
		return;
	}

	if (!common->wcmGesture)
		goto ret;

	/* second finger in prox. wait for gesture event if first finger
	 * was in in prox */
	if (ds[1].proximity && !common->wcmGestureMode && dsLast[0].proximity)
	{
		common->wcmTouchpadMode = 0;
		common->wcmGestureMode = GESTURE_LAG_MODE;
	}

	/* first finger recently came in prox. But not the first time
	 * wait for the second one for a certain time */
	else if (dsLast[0].proximity &&
	    ((GetTimeInMillis() - ds[0].sample) < WACOM_GESTURE_LAG_TIME))
	{
		if (!common->wcmGestureMode)
			common->wcmGestureMode = GESTURE_LAG_MODE;
	}

	/* we've waited enough time */
	else if (common->wcmGestureMode == GESTURE_LAG_MODE)
		common->wcmGestureMode = 0;

	if  (ds[1].proximity && !dsLast[1].proximity)
	{
		/* keep the initial states for gesture mode */
		common->wcmGestureState[1] = ds[1];

		/* reset the initial count for a new getsure */
		common->wcmGestureParameters.wcmGestureUsed  = 0;
	}

	if (ds[0].proximity && !dsLast[0].proximity)
	{
		/* keep the initial states for gesture mode */
		common->wcmGestureState[0] = ds[0];

		/* reset the initial count for a new getsure */
		common->wcmGestureParameters.wcmGestureUsed  = 0;

		/* initialize the cursor position */
		if (!common->wcmGestureMode && !channel)
			goto ret;
	}

	if (!ds[0].proximity && !ds[1].proximity)
	{
		/* first finger was out-prox when GestureMode was still on */
		if (!dsLast[0].proximity && common->wcmGestureMode)
			/* send first finger out prox */
			wcmSoftOutEvent(priv->local);

		/* exit gesture mode when both fingers are out */
		common->wcmGestureMode = 0;
		common->wcmGestureParameters.wcmScrollDirection = 0;

		common->wcmTouchpadMode = 0;
		goto ret;
	}

	if (!(common->wcmGestureMode & (GESTURE_SCROLL_MODE | GESTURE_ZOOM_MODE)))
		wcmFingerTapToClick(priv);

	/* Change mode happens only when both fingers are out */
	if (common->wcmGestureMode & GESTURE_TAP_MODE)
		goto ret;

	/* skip initial finger event for scroll and zoom */
	if (!dsLast[0].proximity || !dsLast[1].proximity)
		goto ret;

	/* was in zoom mode no time check needed */
	if ((common->wcmGestureMode & GESTURE_ZOOM_MODE) &&
	    ds[0].proximity && ds[1].proximity)
		wcmFingerZoom(priv);

	/* was in scroll mode no time check needed */
	else if (common->wcmGestureMode & GESTURE_SCROLL_MODE)
		    wcmFingerScroll(priv);

	/* process complex two finger gestures */
	else if ((2*common->wcmGestureParameters.wcmTapTime <
	    (GetTimeInMillis() - ds[0].sample)) &&
	    (2*common->wcmGestureParameters.wcmTapTime <
	    (GetTimeInMillis() - ds[1].sample))
	    && ds[0].proximity && ds[1].proximity)
	{
		/* scroll should be considered first since it requires
		 * a finger distance check */
		wcmFingerScroll(priv);

		if (!(common->wcmGestureMode & GESTURE_SCROLL_MODE))
		    wcmFingerZoom(priv);
	}
ret:
	if (!common->wcmGestureMode && !channel && !is_absolute(priv->local))
		wcmFirstFingerClick(common);
}

static void wcmSendScrollEvent(WacomDevicePtr priv, int dist,
			 int buttonUp, int buttonDn)
{
	int button = (dist > 0) ? buttonUp : buttonDn;
	WacomCommonPtr common = priv->common;
	int count = (int)(((double)abs(dist)/
		common->wcmGestureParameters.wcmScrollDistance) + 0.5);
	WacomChannelPtr firstChannel = common->wcmChannel;
	WacomChannelPtr secondChannel = common->wcmChannel + 1;
	WacomDeviceState ds[2] = { firstChannel->valid.states[0],
				secondChannel->valid.states[0] };

	/* user might have changed from up to down or vice versa */
	if (count < common->wcmGestureParameters.wcmGestureUsed)
	{
		common->wcmGestureState[0] = ds[0];
		common->wcmGestureState[1] = ds[1];
		common->wcmGestureParameters.wcmGestureUsed  = 0;
		return;
	}

	count -= common->wcmGestureParameters.wcmGestureUsed;
	common->wcmGestureParameters.wcmGestureUsed += count;
	while (count--)
	{
		wcmSendButtonClick(priv, button, 1);
		wcmSendButtonClick(priv, button, 0);
		DBG(10, priv, "loop count = %d \n", count);
	}
}

static void wcmFingerScroll(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomChannelPtr firstChannel = common->wcmChannel;
	WacomChannelPtr secondChannel = common->wcmChannel + 1;
	WacomDeviceState ds[2] = { firstChannel->valid.states[0],
		secondChannel->valid.states[0] };

	int midPoint_new = 0;
	int midPoint_old = 0;
	int i = 0, dist = 0;
	WacomFilterState filterd;  /* borrow this struct */

	DBG(10, priv, "\n");

	if (common->wcmGestureMode != GESTURE_SCROLL_MODE)
	{
		if (abs(touchDistance(ds[0], ds[1]) -
			touchDistance(common->wcmGestureState[0],
			common->wcmGestureState[1])) < WACOM_INLINE_DISTANCE)
		{
			/* two fingers stay close to each other all the time and
			 * move in vertical or horizontal direction together
			 */
			if (pointsInLine(common, ds[0], common->wcmGestureState[0])
			    && pointsInLine(common, ds[1], common->wcmGestureState[1])
			    && common->wcmGestureParameters.wcmScrollDirection)
			{
				/* left button might be down. Send it up first */
				wcmSendButtonClick(priv, 1, 0);
				common->wcmGestureMode = GESTURE_SCROLL_MODE;
			}
		}
	}

	/* still not a scroll event yet? */
	if (common->wcmGestureMode != GESTURE_SCROLL_MODE)
		return;

	/* initialize the points so we can rotate them */
	filterd.x[0] = ds[0].x;
	filterd.y[0] = ds[0].y;
	filterd.x[1] = ds[1].x;
	filterd.y[1] = ds[1].y;
	filterd.x[2] = common->wcmGestureState[0].x;
	filterd.y[2] = common->wcmGestureState[0].y;
	filterd.x[3] = common->wcmGestureState[1].x;
	filterd.y[3] = common->wcmGestureState[1].y;

	/* scrolling has directions so rotation has to be considered first */
	for (i=0; i<6; i++)
		wcmRotateCoordinates(priv->local, &filterd.x[i], &filterd.y[i]);

	/* check vertical direction */
	if (common->wcmGestureParameters.wcmScrollDirection == WACOM_VERT_ALLOWED)
	{
		midPoint_old = (((double)filterd.y[2] + (double)filterd.y[3]) / 2.);
		midPoint_new = (((double)filterd.y[0] + (double)filterd.y[1]) / 2.);

		/* allow one finger scroll */
		if (!ds[0].proximity)
		{
			midPoint_old = filterd.y[3];
			midPoint_new = filterd.y[1];
		}

		if (!ds[1].proximity)
		{
			midPoint_old = filterd.y[2];
			midPoint_new = filterd.y[0];
		}

		dist = midPoint_old - midPoint_new;
		wcmSendScrollEvent(priv, dist, WCM_SCROLL_UP, WCM_SCROLL_DOWN);
	}

	if (common->wcmGestureParameters.wcmScrollDirection == WACOM_HORIZ_ALLOWED)
	{
		midPoint_old = (((double)filterd.x[2] + (double)filterd.x[3]) / 2.);
		midPoint_new = (((double)filterd.x[0] + (double)filterd.x[1]) / 2.);

		/* allow one finger scroll */
		if (!ds[0].proximity)
		{
			midPoint_old = filterd.x[3];
			midPoint_new = filterd.x[1];
		}

		if (!ds[1].proximity)
		{
			midPoint_old = filterd.x[2];
			midPoint_new = filterd.x[0];
		}

		dist = midPoint_old - midPoint_new;
		wcmSendScrollEvent(priv, dist, WCM_SCROLL_RIGHT, WCM_SCROLL_LEFT);
	}
}

static void wcmFingerZoom(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomChannelPtr firstChannel = common->wcmChannel;
	WacomChannelPtr secondChannel = common->wcmChannel + 1;
	WacomDeviceState ds[2] = { firstChannel->valid.states[0],
		secondChannel->valid.states[0] };
	int count, button;
	int dist = touchDistance(common->wcmGestureState[0],
			common->wcmGestureState[1]);

	DBG(10, priv, "\n");

	if (common->wcmGestureMode != GESTURE_ZOOM_MODE)
	{
		/* two fingers moved apart from each other */
		if (abs(touchDistance(ds[0], ds[1]) -
			touchDistance(common->wcmGestureState[0],
				      common->wcmGestureState[1])) >
			(3 * WACOM_INLINE_DISTANCE))
		{
			/* left button might be down, send it up first */
			wcmSendButtonClick(priv, 1, 0);

			/* fingers moved apart more than 3 times
			 * WACOM_INLINE_DISTANCE, zoom mode is entered */
			common->wcmGestureMode = GESTURE_ZOOM_MODE;
		}
	}

	if (common->wcmGestureMode != GESTURE_ZOOM_MODE)
		return;

	dist = touchDistance(ds[0], ds[1]) - dist;
	count = (int)(((double)abs(dist)/common->wcmGestureParameters.wcmZoomDistance) + 0.5);

	/* user might have changed from left to right or vice versa */
	if (count < common->wcmGestureParameters.wcmGestureUsed)
	{
		/* reset the initial states for the new getsure */
		common->wcmGestureState[0] = ds[0];
		common->wcmGestureState[1] = ds[1];
		common->wcmGestureParameters.wcmGestureUsed  = 0;
		return;
	}

	/* zooming? Send ctrl + scroll up/down event.
	FIXME: this hardcodes the positions of ctrl and assumes that ctrl is
	actually a modifier. Tough luck. The alternative is to run through
	the XKB table and figure out where the key for the ctrl modifier is
	hiding. Good luck.
	Gesture support is not supposed to be in the driver...
	 */
	button = (dist > 0) ? 4 : 5;

	count -= common->wcmGestureParameters.wcmGestureUsed;
	common->wcmGestureParameters.wcmGestureUsed += count;
	while (count--)
	{
		wcmEmitKeycode (priv->local->dev, 37 /*XK_Control_L*/, 1);
		wcmSendButtonClick (priv, button, 1);
		wcmSendButtonClick (priv, button, 0);
		wcmEmitKeycode (priv->local->dev, 37 /*XK_Control_L*/, 0);
	}
}

/* vim: set noexpandtab shiftwidth=8: */
