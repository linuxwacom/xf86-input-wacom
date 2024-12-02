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

#include <config.h>

#include <math.h>
#include "xf86Wacom.h"
#include "wcmFilter.h"
#include "wcmPressureCurve.h"


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
	/* sanity check values */
	if (!wcmCheckPressureCurveValues(x0, y0, x1, y1))
		return;

	/* A NULL pPressCurve indicates the (default) linear curve */
	if (x0 == 0 && y0 == 0 && x1 == 100 && y1 == 100) {
		free(pDev->pPressCurve);
		pDev->pPressCurve = NULL;
	}
	else if (!pDev->pPressCurve) {
		pDev->pPressCurve = calloc(FILTER_PRESSURE_RES+1, sizeof(*pDev->pPressCurve));

		if (!pDev->pPressCurve) {
			wcmLogSafe(pDev, W_WARNING,
			       "Unable to allocate memory for pressure curve; using default.\n");
			x0 = 0;
			y0 = 0;
			x1 = 100;
			y1 = 100;
		}
	}

	if (pDev->pPressCurve)
		filterCurveToLine(pDev->pPressCurve,
				pDev->maxCurve,
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

static int wcmFilterAverage(int *samples, int n)
{
	int x = 0;
	int i;

	for (i = 0; i < n; i++)
	{
		x += samples[i];
	}
	return x / n;
}

/*****************************************************************************
 * wcmFilterCoord -- provide noise correction to all transducers
 ****************************************************************************/

int wcmFilterCoord(WacomCommonPtr common, WacomChannelPtr pChannel,
	WacomDeviceStatePtr ds)
{
	WacomFilterState *state;

	DBG(10, common, "common->wcmRawSample = %d \n", common->wcmRawSample);

	storeRawSample(common, pChannel, ds);

	state = &pChannel->rawFilter;

	ds->x = wcmFilterAverage(state->x, common->wcmRawSample);
	ds->y = wcmFilterAverage(state->y, common->wcmRawSample);
	if (HANDLE_TILT(common) && (ds->device_type == STYLUS_ID ||
				    ds->device_type == ERASER_ID))
	{
		ds->tiltx = wcmFilterAverage(state->tiltx, common->wcmRawSample);
		if (ds->tiltx > common->wcmTiltMaxX)
			ds->tiltx = common->wcmTiltMaxX;
		else if (ds->tiltx < common->wcmTiltMinX)
			ds->tiltx = common->wcmTiltMinX;

		ds->tilty = wcmFilterAverage(state->tilty, common->wcmRawSample);
		if (ds->tilty > common->wcmTiltMaxY)
			ds->tilty = common->wcmTiltMaxY;
		else if (ds->tilty < common->wcmTiltMinY)
			ds->tilty = common->wcmTiltMinY;
	}

	return 0; /* lookin' good */
}

/***
 * Convert a point (X/Y) in a left-handed coordinate system to a normalized
 * rotation angle.
 *
 * This function is currently called for the Intuos4 mouse (cursor) tool
 * only (to convert tilt to rotation), but it may be used for other devices
 * in the future.
 *
 * Method used: rotation angle is calculated through the atan of x/y
 * then converted to degrees and normalized into the rotation
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
 * @param x X coordinate in left-handed coordiante system.
 * @param y Y coordiante in left-handed coordinate system.
 * @param offset Custom rotation offset in degrees. Offset is
 * applied in counterclockwise direction.
 *
 * @return The mapped rotation angle based on the device's tilt state.
 */
int wcmTilt2R(int x, int y, double offset)
{
	double angle = 0.0;
	int rotation;

	if (x || y)
		/* rotate in the inverse direction, changing CW to CCW
		 * rotation  and vice versa */
		angle = ((180.0 * atan2(-x, y)) / M_PI);

	/* rotation is now in 0 - 360 deg value range, apply the offset. Use
	 * 360 to avoid getting into negative range, the normalization code
	 * below expects 0...360 */
	angle = 360 + angle - offset;

	/* normalize into the rotation range (0...MAX), then offset by MIN_ROTATION
	   we used 360 as base offset above, so %= MAX_ROTATION_RANGE brings us back.
	   Note: we can't use xf86ScaleAxis here because of rounding issues.
	 */
	rotation = round(angle * (MAX_ROTATION_RANGE / 360.0));
	rotation %= MAX_ROTATION_RANGE;

	/* now scale back from 0...MAX to MIN..(MIN+MAX) */
	rotation = wcmScaleAxis(rotation,
				 MIN_ROTATION + MAX_ROTATION_RANGE,
				 MIN_ROTATION,
				 MAX_ROTATION_RANGE, 0);

	return rotation;
}

#ifdef ENABLE_TESTS

#include "wacom-test-suite.h"

TEST_CASE(test_tilt_to_rotation)
{
#if 0
	This table below was generated from wcmTilt2R with the following code

	for (angle = 0; angle < 360; angle++)
	{
		double rad = angle * M_PI / 180.0;
		double x, y;
		x = sin(rad);
		y = cos(rad);

		/* wcmTilt2R only uses it for the angle anyway, let's try to
		get as precise as possible */
		ds.tiltx = x * 1000;
		ds.tilty = y * 1000;
		ds.rotation = 0;

		wcmTilt2R(&ds);

		printf("{ %d, %d, %d},\n", ds.tiltx, ds.tilty, ds.rotation);
	}
#endif

	int rotation_table[][3] = {
		{ 17, 999, 20}, { 34, 999, 15}, { 52, 998, 10}, { 69, 997, 5}, { 87, 996, 0},
		{ 104, 994, -5}, { 121, 992, -10}, { 139, 990, -15}, { 156, 987, -20}, { 173, 984, -25},
		{ 190, 981, -30}, { 207, 978, -35}, { 224, 974, -40}, { 241, 970, -45}, { 258, 965, -50},
		{ 275, 961, -55}, { 292, 956, -60}, { 309, 951, -65}, { 325, 945, -70}, { 342, 939, -75},
		{ 358, 933, -80}, { 374, 927, -85}, { 390, 920, -90}, { 406, 913, -95}, { 422, 906, -100},
		{ 438, 898, -105}, { 453, 891, -110}, { 469, 882, -115}, { 484, 874, -120}, { 499, 866, -125},
		{ 515, 857, -130}, { 529, 848, -135}, { 544, 838, -140}, { 559, 829, -145}, { 573, 819, -150},
		{ 587, 809, -155}, { 601, 798, -160}, { 615, 788, -165}, { 629, 777, -170}, { 642, 766, -175},
		{ 656, 754, -180}, { 669, 743, -185}, { 681, 731, -190}, { 694, 719, -195}, { 707, 707, -200},
		{ 719, 694, -205}, { 731, 681, -210}, { 743, 669, -215}, { 754, 656, -220}, { 766, 642, -225},
		{ 777, 629, -230}, { 788, 615, -235}, { 798, 601, -240}, { 809, 587, -245}, { 819, 573, -250},
		{ 829, 559, -255}, { 838, 544, -260}, { 848, 529, -265}, { 857, 515, -270}, { 866, 500, -275},
		{ 874, 484, -280}, { 882, 469, -285}, { 891, 453, -290}, { 898, 438, -295}, { 906, 422, -300},
		{ 913, 406, -305}, { 920, 390, -310}, { 927, 374, -315}, { 933, 358, -320}, { 939, 342, -325},
		{ 945, 325, -330}, { 951, 309, -335}, { 956, 292, -340}, { 961, 275, -345}, { 965, 258, -350},
		{ 970, 241, -355}, { 974, 224, -360}, { 978, 207, -365}, { 981, 190, -370}, { 984, 173, -375},
		{ 987, 156, -380}, { 990, 139, -385}, { 992, 121, -390}, { 994, 104, -395}, { 996, 87, -400},
		{ 997, 69, -405}, { 998, 52, -410}, { 999, 34, -415}, { 999, 17, -420}, { 1000, 0, -425},
		{ 999, -17, -430}, { 999, -34, -435}, { 998, -52, -440}, { 997, -69, -445}, { 996, -87, -450},
		{ 994, -104, -455}, { 992, -121, -460}, { 990, -139, -465}, { 987, -156, -470}, { 984, -173, -475},
		{ 981, -190, -480}, { 978, -207, -485}, { 974, -224, -490}, { 970, -241, -495}, { 965, -258, -500},
		{ 961, -275, -505}, { 956, -292, -510}, { 951, -309, -515}, { 945, -325, -520}, { 939, -342, -525},
		{ 933, -358, -530}, { 927, -374, -535}, { 920, -390, -540}, { 913, -406, -545}, { 906, -422, -550},
		{ 898, -438, -555}, { 891, -453, -560}, { 882, -469, -565}, { 874, -484, -570}, { 866, -499, -575},
		{ 857, -515, -580}, { 848, -529, -585}, { 838, -544, -590}, { 829, -559, -595}, { 819, -573, -600},
		{ 809, -587, -605}, { 798, -601, -610}, { 788, -615, -615}, { 777, -629, -620}, { 766, -642, -625},
		{ 754, -656, -630}, { 743, -669, -635}, { 731, -681, -640}, { 719, -694, -645}, { 707, -707, -650},
		{ 694, -719, -655}, { 681, -731, -660}, { 669, -743, -665}, { 656, -754, -670}, { 642, -766, -675},
		{ 629, -777, -680}, { 615, -788, -685}, { 601, -798, -690}, { 587, -809, -695}, { 573, -819, -700},
		{ 559, -829, -705}, { 544, -838, -710}, { 529, -848, -715}, { 515, -857, -720}, { 499, -866, -725},
		{ 484, -874, -730}, { 469, -882, -735}, { 453, -891, -740}, { 438, -898, -745}, { 422, -906, -750},
		{ 406, -913, -755}, { 390, -920, -760}, { 374, -927, -765}, { 358, -933, -770}, { 342, -939, -775},
		{ 325, -945, -780}, { 309, -951, -785}, { 292, -956, -790}, { 275, -961, -795}, { 258, -965, -800},
		{ 241, -970, -805}, { 224, -974, -810}, { 207, -978, -815}, { 190, -981, -820}, { 173, -984, -825},
		{ 156, -987, -830}, { 139, -990, -835}, { 121, -992, -840}, { 104, -994, -845}, { 87, -996, -850},
		{ 69, -997, -855}, { 52, -998, -860}, { 34, -999, -865}, { 17, -999, -870}, { 0, -1000, -875},
		{ -17, -999, -880}, { -34, -999, -885}, { -52, -998, -890}, { -69, -997, -895}, { -87, -996, -900},
		{ -104, -994, 895}, { -121, -992, 890}, { -139, -990, 885}, { -156, -987, 880}, { -173, -984, 875},
		{ -190, -981, 870}, { -207, -978, 865}, { -224, -974, 860}, { -241, -970, 855}, { -258, -965, 850},
		{ -275, -961, 845}, { -292, -956, 840}, { -309, -951, 835}, { -325, -945, 830}, { -342, -939, 825},
		{ -358, -933, 820}, { -374, -927, 815}, { -390, -920, 810}, { -406, -913, 805}, { -422, -906, 800},
		{ -438, -898, 795}, { -453, -891, 790}, { -469, -882, 785}, { -484, -874, 780}, { -500, -866, 775},
		{ -515, -857, 770}, { -529, -848, 765}, { -544, -838, 760}, { -559, -829, 755}, { -573, -819, 750},
		{ -587, -809, 745}, { -601, -798, 740}, { -615, -788, 735}, { -629, -777, 730}, { -642, -766, 725},
		{ -656, -754, 720}, { -669, -743, 715}, { -681, -731, 710}, { -694, -719, 705}, { -707, -707, 700},
		{ -719, -694, 695}, { -731, -681, 690}, { -743, -669, 685}, { -754, -656, 680}, { -766, -642, 675},
		{ -777, -629, 670}, { -788, -615, 665}, { -798, -601, 660}, { -809, -587, 655}, { -819, -573, 650},
		{ -829, -559, 645}, { -838, -544, 640}, { -848, -529, 635}, { -857, -515, 630}, { -866, -500, 625},
		{ -874, -484, 620}, { -882, -469, 615}, { -891, -453, 610}, { -898, -438, 605}, { -906, -422, 600},
		{ -913, -406, 595}, { -920, -390, 590}, { -927, -374, 585}, { -933, -358, 580}, { -939, -342, 575},
		{ -945, -325, 570}, { -951, -309, 565}, { -956, -292, 560}, { -961, -275, 555}, { -965, -258, 550},
		{ -970, -241, 545}, { -974, -224, 540}, { -978, -207, 535}, { -981, -190, 530}, { -984, -173, 525},
		{ -987, -156, 520}, { -990, -139, 515}, { -992, -121, 510}, { -994, -104, 505}, { -996, -87, 500},
		{ -997, -69, 495}, { -998, -52, 490}, { -999, -34, 485}, { -999, -17, 480}, { -1000, 0, 475},
		{ -999, 17, 470}, { -999, 34, 465}, { -998, 52, 460}, { -997, 69, 455}, { -996, 87, 450},
		{ -994, 104, 445}, { -992, 121, 440}, { -990, 139, 435}, { -987, 156, 430}, { -984, 173, 425},
		{ -981, 190, 420}, { -978, 207, 415}, { -974, 224, 410}, { -970, 241, 405}, { -965, 258, 400},
		{ -961, 275, 395}, { -956, 292, 390}, { -951, 309, 385}, { -945, 325, 380}, { -939, 342, 375},
		{ -933, 358, 370}, { -927, 374, 365}, { -920, 390, 360}, { -913, 406, 355}, { -906, 422, 350},
		{ -898, 438, 345}, { -891, 453, 340}, { -882, 469, 335}, { -874, 484, 330}, { -866, 500, 325},
		{ -857, 515, 320}, { -848, 529, 315}, { -838, 544, 310}, { -829, 559, 305}, { -819, 573, 300},
		{ -809, 587, 295}, { -798, 601, 290}, { -788, 615, 285}, { -777, 629, 280}, { -766, 642, 275},
		{ -754, 656, 270}, { -743, 669, 265}, { -731, 681, 260}, { -719, 694, 255}, { -707, 707, 250},
		{ -694, 719, 245}, { -681, 731, 240}, { -669, 743, 235}, { -656, 754, 230}, { -642, 766, 225},
		{ -629, 777, 220}, { -615, 788, 215}, { -601, 798, 210}, { -587, 809, 205}, { -573, 819, 200},
		{ -559, 829, 195}, { -544, 838, 190}, { -529, 848, 185}, { -515, 857, 180}, { -500, 866, 175},
		{ -484, 874, 170}, { -469, 882, 165}, { -453, 891, 160}, { -438, 898, 155}, { -422, 906, 150},
		{ -406, 913, 145}, { -390, 920, 140}, { -374, 927, 135}, { -358, 933, 130}, { -342, 939, 125},
		{ -325, 945, 120}, { -309, 951, 115}, { -292, 956, 110}, { -275, 961, 105}, { -258, 965, 100},
		{ -241, 970, 95}, { -224, 974, 90}, { -207, 978, 85}, { -190, 981, 80}, { -173, 984, 75},
		{ -156, 987, 70}, { -139, 990, 65}, { -121, 992, 60}, { -104, 994, 55}, { -87, 996, 50},
		{ -69, 997, 45}, { -52, 998, 40}, { -34, 999, 35}, { -17, 999, 30},
	};

	for (size_t i = 0; i < ARRAY_SIZE(rotation_table); i++)
	{
		int rotation;
		int x, y;
		x = rotation_table[i][0];
		y = rotation_table[i][1];
		rotation = wcmTilt2R(x, y, INTUOS4_CURSOR_ROTATION_OFFSET);
		assert(rotation == rotation_table[i][2]);
	}
}
#endif

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
