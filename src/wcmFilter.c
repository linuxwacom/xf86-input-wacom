/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2008 by Ping Cheng, Wacom. <pingc@wacom.com> 
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

#include <math.h>
#include "xf86Wacom.h"
#include "wcmFilter.h"

/*****************************************************************************
 * Static functions
 ****************************************************************************/

static void filterCurveToLine(int* pCurve, int nMax, double x0, double y0,
		double x1, double y1, double x2, double y2,
		double x3, double y3);
static int filterOnLine(double x0, double y0, double x1, double y1,
		double a, double b);
static void filterLine(int* pCurve, int nMax, int x0, int y0, int x1, int y1);


/*****************************************************************************
 * wcmCheckPressureCurveValues -- check pressure curve values for sanity.
 * Return TRUE if values are sane or FALSE otherwise.
 ****************************************************************************/
int wcmCheckPressureCurveValues(int x0, int y0, int x1, int y1)
{
	return !((x0 < 0) || (x0 > 100) || (y0 < 0) || (y0 > 100) ||
		 (x1 < 0) || (x1 > 100) || (y1 < 0) || (y1 > 100));
}


/*****************************************************************************
 * wcmSetPressureCurve -- apply user-defined curve to pressure values
 ****************************************************************************/
void wcmSetPressureCurve(WacomDevicePtr pDev, int x0, int y0,
	int x1, int y1)
{
	int i;

	/* sanity check values */
	if (!wcmCheckPressureCurveValues(x0, y0, x1, y1))
		return;

	/* linear by default */
	for (i=0; i<=FILTER_PRESSURE_RES; ++i)
		pDev->pPressCurve[i] = i;

	/* draw bezier line from bottom-left to top-right using ctrl points */
	filterCurveToLine(pDev->pPressCurve,
		FILTER_PRESSURE_RES,
		0.0, 0.0,               /* bottom left  */
		x0/100.0, y0/100.0,     /* control point 1 */
		x1/100.0, y1/100.0,     /* control point 2 */
		1.0, 1.0);              /* top right */

	pDev->nPressCtrl[0] = x0;
	pDev->nPressCtrl[1] = y0;
	pDev->nPressCtrl[2] = x1;
	pDev->nPressCtrl[3] = y1;
}

/*
 * wcmResetSampleCounter --
 * Device specific filter routines are responcable for storing raw data
 * as well as filtering.  wcmResetSampleCounter is called to reset
 * raw counters.
 */
void wcmResetSampleCounter(const WacomChannelPtr pChannel)
{
	pChannel->nSamples = 0;
	pChannel->rawFilter.npoints = 0;
}


static void filterNearestPoint(double x0, double y0, double x1, double y1,
		double a, double b, double* x, double* y)
{
	double vx, vy, wx, wy, d1, d2, c;

	wx = a - x0; wy = b - y0;
	vx = x1 - x0; vy = y1 - y0;

	d1 = vx * wx + vy * wy;
	if (d1 <= 0)
	{
		*x = x0;
		*y = y0;
	}
	else
	{
		d2 = vx * vx + vy * vy;
		if (d1 >= d2)
		{
			*x = x1;
			*y = y1;
		}
		else
		{
			c = d1 / d2;
			*x = x0 + c * vx;
			*y = y0 + c * vy;
		}
	}
}

static int filterOnLine(double x0, double y0, double x1, double y1,
		double a, double b)
{
        double x, y, d;
	filterNearestPoint(x0,y0,x1,y1,a,b,&x,&y);
	d = (x-a)*(x-a) + (y-b)*(y-b);
	return d < 0.00001; /* within 100th of a point (1E-2 squared) */
}

static void filterCurveToLine(int* pCurve, int nMax, double x0, double y0,
		double x1, double y1, double x2, double y2,
		double x3, double y3)
{
	double x01,y01,x32,y32,xm,ym;
	double c1,d1,c2,d2,e,f;

	/* check if control points are on line */
	if (filterOnLine(x0,y0,x3,y3,x1,y1) && filterOnLine(x0,y0,x3,y3,x2,y2))
	{
		filterLine(pCurve,nMax,
			(int)(x0*nMax),(int)(y0*nMax),
			(int)(x3*nMax),(int)(y3*nMax));
		return;
	}

	/* calculate midpoints */
	x01 = (x0 + x1) / 2; y01 = (y0 + y1) / 2;
	x32 = (x3 + x2) / 2; y32 = (y3 + y2) / 2;

	/* calc split point */
	xm = (x1 + x2) / 2; ym = (y1 + y2) / 2;
	
	/* calc control points and midpoint */
	c1 = (x01 + xm) / 2; d1 = (y01 + ym) / 2;
	c2 = (x32 + xm) / 2; d2 = (y32 + ym) / 2;
	e = (c1 + c2) / 2; f = (d1 + d2) / 2;

	/* do each side */
	filterCurveToLine(pCurve,nMax,x0,y0,x01,y01,c1,d1,e,f);
	filterCurveToLine(pCurve,nMax,e,f,c2,d2,x32,y32,x3,y3);
}

static void filterLine(int* pCurve, int nMax, int x0, int y0, int x1, int y1)
{
	int dx, dy, ax, ay, sx, sy, x, y, d;

	/* sanity check */
	if ((x0 < 0) || (y0 < 0) || (x1 < 0) || (y1 < 0) ||
		(x0 > nMax) || (y0 > nMax) || (x1 > nMax) || (y1 > nMax))
		return;

	dx = x1 - x0; ax = abs(dx) * 2; sx = (dx>0) ? 1 : -1;
	dy = y1 - y0; ay = abs(dy) * 2; sy = (dy>0) ? 1 : -1;
	x = x0; y = y0;

	/* x dominant */
	if (ax > ay)
	{
		d = ay - ax / 2;
		while (1)
		{
			pCurve[x] = y;
			if (x == x1) break;
			if (d >= 0)
			{
				y += sy;
				d -= ax;
			}
			x += sx;
			d += ay;
		}
	}

	/* y dominant */
	else
	{
		d = ax - ay / 2;
		while (1)
		{
			pCurve[x] = y;
			if (y == y1) break;
			if (d >= 0)
			{
				x += sx;
				d -= ay;
			}
			y += sy;
			d += ax;
		}
	}
}
static void storeRawSample(WacomCommonPtr common, WacomChannelPtr pChannel,
			   WacomDeviceStatePtr ds)
{
	WacomFilterState *fs;
	int i;

	fs = &pChannel->rawFilter;
	if (!fs->npoints)
	{
		DBG(10, common, "initialize channel data.\n");
		/* Store initial value over whole average window */
		for (i=common->wcmRawSample - 1; i>=0; i--)
		{
			fs->x[i]= ds->x;
			fs->y[i]= ds->y;
		}
		if (HANDLE_TILT(common) && (ds->device_type == STYLUS_ID ||
					    ds->device_type == ERASER_ID))
		{
			for (i=common->wcmRawSample - 1; i>=0; i--)
			{
				fs->tiltx[i] = ds->tiltx;
				fs->tilty[i] = ds->tilty;
			}
		}
		++fs->npoints;
	} else {
		/* Shift window and insert latest sample */
		for (i=common->wcmRawSample - 1; i>0; i--)
		{
			fs->x[i]= fs->x[i-1];
			fs->y[i]= fs->y[i-1];
		}
		fs->x[0] = ds->x;
		fs->y[0] = ds->y;
		if (HANDLE_TILT(common) && (ds->device_type == STYLUS_ID ||
					    ds->device_type == ERASER_ID))
		{
			for (i=common->wcmRawSample - 1; i>0; i--)
			{
				fs->tiltx[i]= fs->tiltx[i-1];
				fs->tilty[i]= fs->tilty[i-1];
			}
			fs->tiltx[0] = ds->tiltx;
			fs->tilty[0] = ds->tilty;
		}
		if (fs->npoints < common->wcmRawSample)
			++fs->npoints;
	}
}

/*****************************************************************************
 * wcmFilterCoord -- provide noise correction to all transducers
 ****************************************************************************/

int wcmFilterCoord(WacomCommonPtr common, WacomChannelPtr pChannel,
	WacomDeviceStatePtr ds)
{
	int x=0, y=0, tx=0, ty=0, i;
	WacomFilterState *state;

	DBG(10, common, "common->wcmRawSample = %d \n", common->wcmRawSample);

	storeRawSample(common, pChannel, ds);

	state = &pChannel->rawFilter;

	for ( i=0; i<common->wcmRawSample; i++ )
	{
		x += state->x[i];
		y += state->y[i];
		if (HANDLE_TILT(common) && (ds->device_type == STYLUS_ID ||
					    ds->device_type == ERASER_ID))
		{
			tx += state->tiltx[i];
			ty += state->tilty[i];
		}
	}
	ds->x = x / common->wcmRawSample;
	ds->y = y / common->wcmRawSample;

	if (HANDLE_TILT(common) && (ds->device_type == STYLUS_ID ||
				    ds->device_type == ERASER_ID))
	{
		ds->tiltx = tx / common->wcmRawSample;
		if (ds->tiltx > common->wcmMaxtiltX/2-1)
			ds->tiltx = common->wcmMaxtiltX/2-1;
		else if (ds->tiltx < -common->wcmMaxtiltX/2)
			ds->tiltx = -common->wcmMaxtiltX/2;

		ds->tilty = ty / common->wcmRawSample;
		if (ds->tilty > common->wcmMaxtiltY/2-1)
			ds->tilty = common->wcmMaxtiltY/2-1;
		else if (ds->tilty < -common->wcmMaxtiltY/2)
			ds->tilty = -common->wcmMaxtiltY/2;
	}

	return 0; /* lookin' good */
}

/***
 * Convert tilt X and Y to rotation
 *
 * This function is currently called for the Intuos4 mouse (cursor) tool
 * only, but it may be used for other devices in the future.
 *
 * Method used: rotation angle is calculated through the atan of the tiltx/y
 * coordinates, then converted to degrees and normalized into the rotation
 * range (MIN_ROTATION/MAX_ROTATION).

 * IMPORTANT: calculation inverts direction, the formula to get the target
 * rotation value in degrees is: 360 - offset - input-angle.
 *
 * Example table of return values for an offset of 0, assuming a left-handed
 * coordinate system:
 * input  0 degrees:	MIN
 *       90 degrees:	MAX - RANGE/4
 *      180 degrees:	RANGE/2
 *      270 degrees:	MIN + RANGE/4
 *
 * @param ds The current device state, will be modified to set to the
 * calculated rotation value.
 * @param offset Custom rotation offset in degrees. Offset is
 * applied in counterclockwise direction.
 *
 * @return The mapped rotation angle based on the device's tilt state.
 */
void wcmTilt2R(WacomDeviceStatePtr ds, double offset)
{
	short tilt_x = ds->tiltx;
	short tilt_y = ds->tilty;
	double rotation = 0.0;

	/* other tilt-enabled devices need to apply round() after this call */
	if (tilt_x || tilt_y)
		/* rotate in the inverse direction, changing CW to CCW
		 * rotation  and vice versa */
		rotation = ((180.0 * atan2(-tilt_x,tilt_y)) / M_PI);

	/* rotation is now in 0 - 360 deg value range, apply the offset. Use
	 * 360 to avoid getting into negative range, the normalization code
	 * below expects 0...360 */
	rotation = 360 + rotation - offset;

	/* normalize into the rotation range (0...MAX), then offset by MIN_ROTATION
	   we used 360 as base offset above, so %= MAX_ROTATION_RANGE brings us back.
	   Note: we can't use xf86ScaleAxis here because of rounding issues.
	 */
	ds->rotation = round(rotation * (MAX_ROTATION_RANGE / 360.0));
	ds->rotation %= MAX_ROTATION_RANGE;

	/* now scale back from 0...MAX to MIN..(MIN+MAX) */
	ds->rotation = xf86ScaleAxis(ds->rotation,
				     MIN_ROTATION + MAX_ROTATION_RANGE,
				     MIN_ROTATION,
				     MAX_ROTATION_RANGE, 0);

	/* FIXME: shouldn't we reset tilt? */
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
