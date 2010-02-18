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
static void filterIntuosStylus(WacomCommonPtr common, WacomFilterStatePtr state, WacomDeviceStatePtr ds);
void wcmTilt2R(WacomDeviceStatePtr ds);

/*****************************************************************************
 * wcmSetPressureCurve -- apply user-defined curve to pressure values
 ****************************************************************************/

void wcmSetPressureCurve(WacomDevicePtr pDev, int x0, int y0,
	int x1, int y1)
{
	int i;

	/* sanity check values */
	if ((x0 < 0) || (x0 > 100) || (y0 < 0) || (y0 > 100) ||
		(x1 < 0) || (x1 > 100) || (y1 < 0) || (y1 > 100)) return;

	/* if curve is not allocated, do it now. */
	if (!pDev->pPressCurve)
	{
		pDev->pPressCurve = (int*) xalloc(sizeof(int) *
			(FILTER_PRESSURE_RES + 1));
		if (!pDev->pPressCurve)
		{
			xf86Msg(X_ERROR, "%s: wcmSetPressureCurve: failed to "
				"allocate memory for curve\n", pDev->local->name);
			return;
		}
	}

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

/*****************************************************************************
 * filterIntuosStylus --
 *   Correct some hardware defects we've been seeing in Intuos pads,
 *   but also cuts down quite a bit on jitter.
 ****************************************************************************/

static void filterIntuosStylus(WacomCommonPtr common, WacomFilterStatePtr state, WacomDeviceStatePtr ds)
{
	int x=0, y=0, tx=0, ty=0, i;

	for ( i=0; i<common->wcmRawSample; i++ )
	{
		x += state->x[i];
		y += state->y[i];
		tx += state->tiltx[i];
		ty += state->tilty[i];
	}
	ds->x = x / common->wcmRawSample;
	ds->y = y / common->wcmRawSample;

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

/*****************************************************************************
 * wcmFilterCoord -- provide noise correction to all transducers
 ****************************************************************************/

int wcmFilterCoord(WacomCommonPtr common, WacomChannelPtr pChannel,
	WacomDeviceStatePtr ds)
{
	/* Only noise correction should happen here. If there's a problem that
	 * cannot be fixed, return 1 such that the data is discarded. */

	WacomDeviceState *pLast;
	int *x, *y, i; 

	DBG(10, common, "common->wcmRawSample = %d \n", common->wcmRawSample);
	x = pChannel->rawFilter.x;
	y = pChannel->rawFilter.y;

	pLast = &pChannel->valid.state;
	ds->x = 0;
	ds->y = 0;

	for ( i=0; i<common->wcmRawSample; i++ )
	{
		ds->x += x[i];
		ds->y += y[i];
	}
	ds->x /= common->wcmRawSample;
	ds->y /= common->wcmRawSample;

	return 0; /* lookin' good */
}

/*****************************************************************************
 * wcmFilterIntuos -- provide error correction to Intuos and Intuos2
 ****************************************************************************/

int wcmFilterIntuos(WacomCommonPtr common, WacomChannelPtr pChannel,
	WacomDeviceStatePtr ds)
{
	/* Only error correction should happen here. If there's a problem that
	 * cannot be fixed, return 1 such that the data is discarded. */

	if (ds->device_type != CURSOR_ID)
		filterIntuosStylus(common, &pChannel->rawFilter, ds);
	else
		wcmFilterCoord(common, pChannel, ds);

	return 0; /* lookin' good */
}

/*****************************************************************************
 *  wcmTilt2R -
 *   Converts tilt X and Y to rotation, for Intuos4 mouse for now.
 *   It can be used for other devices when necessary.
 ****************************************************************************/

void wcmTilt2R(WacomDeviceStatePtr ds)
{
	short tilt_x = ds->tiltx;
	short tilt_y = ds->tilty;
	double rotation = 0.0;

	/* other tilt-enabled devices need to apply round() after this call */
	if (tilt_x || tilt_y)
		rotation = ((180.0 * atan2(-tilt_x,tilt_y)) / M_PI) + 180.0;

	/* Intuos4 mouse has an (180-5) offset */
	ds->rotation = round((360.0 - rotation + 180.0 - 5.0) * 5.0);
		ds->rotation %= 1800;

	if (ds->rotation >= 900)
		ds->rotation = 1800 - ds->rotation;
	else
		ds->rotation = -ds->rotation;
}

/* vim: set noexpandtab shiftwidth=8: */
