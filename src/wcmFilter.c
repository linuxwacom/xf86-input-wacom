/*
 * Copyright 1995-2003 by Frederic Lepied, France. <Lepied@XFree86.org>
 *                                                                            
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Frederic   Lepied not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Frederic  Lepied   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.                   
 *                                                                            
 * FREDERIC  LEPIED DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

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
static void filterIntuosStylus(WacomFilterStatePtr state, WacomDeviceStatePtr ds);
static void filterIntuosCoord(int* coord, int* current, int tilt, int* state);
static void filterIntuosTilt(int* state, int* tilt);

/*****************************************************************************
 * xf86WcmSetPressureCurve -- apply user-defined curve to pressure values
 ****************************************************************************/

void xf86WcmSetPressureCurve(WacomDevicePtr pDev, int x0, int y0,
	int x1, int y1)
{
	int i;

	/* sanity check values */
	if ((x0 < 0) || (x0 > 100) || (y0 < 0) || (y0 > 100) ||
		(x1 < 0) || (x1 > 100) || (y1 < 0) || (y1 > 100)) return;

	xf86Msg(X_INFO, "xf86WcmSetPressureCurve: setting to %d,%d %d,%d\n",
		x0, y0, x1, y1);

	/* if curve is not allocated, do it now. */
	if (!pDev->pPressCurve)
	{
		pDev->pPressCurve = (int*) xalloc(sizeof(int) *
			(FILTER_PRESSURE_RES + 1));
		if (!pDev->pPressCurve)
		{
			xf86Msg(X_ERROR, "xf86WcmSetPressureCurve: failed to "
				"allocate memory for curve\n");
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

	for (i=0; i<=FILTER_PRESSURE_RES; i+=128)
		DBG(6, ErrorF("PRESSCURVE: %d=%d (%d)\n",i,pDev->pPressCurve[i],
			pDev->pPressCurve[i] - i));

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

static void filterIntuosStylus(WacomFilterStatePtr state, WacomDeviceStatePtr ds)
{
	if (!state->npoints)
	{
		++state->npoints;
		DBG(2,ErrorF("filterIntuosStylus: first sample NO_FILTER\n"));
		state->x[0] = state->x[1] = state->x[2] = ds->x;
		state->y[0] = state->y[1] = state->y[2] = ds->y;
		state->tiltx[0] = state->tiltx[1] = state->tiltx[2] = ds->tiltx;
		state->tilty[0] = state->tilty[1] = state->tilty[2] = ds->tilty;
		return;
	}

	/* filter x */
	filterIntuosCoord(state->x, &ds->x, ds->tiltx, &state->statex);
	/* filter y */
	filterIntuosCoord(state->y, &ds->y, ds->tilty, &state->statey);
	/* filter tiltx */
	filterIntuosTilt(state->tiltx, &ds->tiltx);
	/* filter tilty */
	filterIntuosTilt(state->tilty, &ds->tilty);
}

static void filterIntuosCoord(int* coord, int* current, int tilt, int* state)
{
	int ts;
	int x0_pred;
	int x0_pred1;
	int x0, x1, x2, x3;

	x0 = *current;
	x1 = coord[0];
	x2 = coord[1];
	x3 = coord[2];
	coord[0] = x0;
	coord[1] = x1;
	coord[2] = x2;
    
	ts = tilt >= 0 ? 1 : -1;
    
	if (*state == 0 || *state == 3)
	{
		if (ts * (x0 - 2 * x1 - x2) > 12 && 
			ts * (x0 - 3 * x2 - 2 * x3) > 12)
		{
			/* detected a jump at x0 */
			*state = 1;
			*current = x1;
		}
		else if (*state == 0)
		{
			x0_pred = 7 * x0 + 14 * x1 + 15 * x2 + 16;
			x0_pred1 = 4 * x3;
			if (x0_pred > x0_pred1)
				*current = ((__u32)(x0_pred - x0_pred1)) >> 5;
			else
				*current = 0;
		}
		else
		{
			/* state == 3 
			 * a jump at x3 was detected */
			*current = (x0 + 2 * x1 + x2 + 2) >> 2;
			*state = 0;
		}
	}
	else if (*state == 1)
	{
		/* a jump at x1 was detected */
		x0_pred = 3 * x0 + 7 * x2 + 4;
		x0_pred1 = 2 * x3;
		if (x0_pred > x0_pred1)
			*current = ((__u32)(x0_pred - x0_pred1)) >> 3;
		else
			*current = 0;
		*state = 2;
	}
	else
	{
		/* state == 2 
		 * a jump at x2 was detected */
		*current = x1;
		*state = 3;
	}
}

/*****************************************************************************
 * filterIntuosTilt --
 *   Correct some hardware defects we've been seeing in Intuos pads,
 *   but also cuts down quite a bit on jitter.
 ****************************************************************************/

static void filterIntuosTilt(int* state, int* tilt)
{
	int tx;

	tx = *tilt + state[0] + state[1] + state[2];

	state[2] = state[1];
	state[1] = state[0];
	state[0] = *tilt;

	tx /= MAX_SAMPLES;

	if (tx > 63)
   		tx = 63;	
	else if (tx < -64)
		tx = -64;

	*tilt = tx;
}

/*****************************************************************************
 * xf86WcmFilterIntuos -- provide error correction to all transducers except Intuos
 ****************************************************************************/

int xf86WcmFilterCoord(WacomCommonPtr common, WacomChannelPtr pChannel,
	WacomDeviceStatePtr ds)
{
	/* Only noise correction should happen here. If there's a problem that
	 * cannot be fixed, return 1 such that the data is discarded. */

	WacomDeviceState* pLast;
	int *x, *y; 
	int filter_x, filter_y;

	x = pChannel->rawFilter.x;
	y = pChannel->rawFilter.y;
	if (!pChannel->rawFilter.npoints)
	{
		++pChannel->rawFilter.npoints;
		DBG(2,ErrorF("xf86WcmFilterCoord: first sample NO_FILTER\n"));
		x[0] = x[1] = x[2] = ds->x;
		y[0] = y[1] = y[2] = ds->y;
		return 0;
	}

	pLast = &pChannel->valid.state;
	filter_x = (ds->x + x[0] + x[1] + x[2])/4;
	filter_y = (ds->y + y[0] + y[1] + y[2])/4;

	x[2] = x[1];
	y[2] = y[1];
	x[1] = x[0];
	y[1] = y[0];
	x[0] = ds->x;
	y[0] = ds->y;

	if (abs(filter_x - pLast->x) > 4)
		ds->x = filter_x;
	else
		ds->x = pLast->x;

	if (abs(filter_y - pLast->y) > 4)
		ds->y = filter_y;
	else
		ds->y = pLast->y;

	return 0; /* lookin' good */
}

/*****************************************************************************
 * xf86WcmFilterCoord -- provide error correction to Intuos and Intuos2
 ****************************************************************************/

int xf86WcmFilterIntuos(WacomCommonPtr common, WacomChannelPtr pChannel,
	WacomDeviceStatePtr ds)
{
	/* Only error correction should happen here. If there's a problem that
	 * cannot be fixed, return 1 such that the data is discarded. */

	if (ds->device_type != CURSOR_ID)
		filterIntuosStylus(&pChannel->rawFilter, ds);
	else
		xf86WcmFilterCoord(common, pChannel, ds);

	return 0; /* lookin' good */
}


