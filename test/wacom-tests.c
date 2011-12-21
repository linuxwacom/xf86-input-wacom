/*
 * Copyright 2011 © Red Hat, Inc.
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

#include "fake-symbols.h"
#include <xf86Wacom.h>

/**
 * NOTE: this file may not contain tests that require static variables. The
 * current compiler undef static magic makes them local variables and thus
 * change the behaviour.
 */

static void
test_get_scroll_delta(void)
{
	int test_table[][5] = {
		{ 100,  25, 0, 0,  75}, { 25,  100, 0, 0, -75},
		{-100, -25, 0, 0, -75}, {-25, -100, 0, 0,  75},
		{ 100, -25, 0, 0, 125}, {-25,  100, 0, 0,-125},
		{ 100, 100, 0, 0,   0}, {-25,  -25, 0, 0,   0},

		{23, 0, 50, 0,  23}, {0, 23, 50, 0, -23},
		{24, 0, 50, 0,  24}, {0, 24, 50, 0, -24},
		{25, 0, 50, 0,  25}, {0, 25, 50, 0, -25},
		{26, 0, 50, 0, -25}, {0, 26, 50, 0,  25},
		{27, 0, 50, 0, -24}, {0, 27, 50, 0,  24},
		{28, 0, 50, 0, -23}, {0, 28, 50, 0,  23},

		{1024, 0, 0, AXIS_BITWISE, 11}, {0, 1024, 0, AXIS_BITWISE, -11},

		{  0, 4, 256, AXIS_BITWISE, -3}, {4,   0, 256, AXIS_BITWISE,  3},
		{  1, 4, 256, AXIS_BITWISE, -2}, {4,   1, 256, AXIS_BITWISE,  2},
		{  2, 4, 256, AXIS_BITWISE, -1}, {4,   2, 256, AXIS_BITWISE,  1},
		{  4, 4, 256, AXIS_BITWISE,  0}, {4,   4, 256, AXIS_BITWISE,  0},
		{  8, 4, 256, AXIS_BITWISE,  1}, {4,   8, 256, AXIS_BITWISE, -1},
		{ 16, 4, 256, AXIS_BITWISE,  2}, {4,  16, 256, AXIS_BITWISE, -2},
		{ 32, 4, 256, AXIS_BITWISE,  3}, {4,  32, 256, AXIS_BITWISE, -3},
		{ 64, 4, 256, AXIS_BITWISE,  4}, {4,  64, 256, AXIS_BITWISE, -4},
		{128, 4, 256, AXIS_BITWISE,  5}, {4, 128, 256, AXIS_BITWISE, -5},
		{256, 4, 256, AXIS_BITWISE, -4}, {4, 256, 256, AXIS_BITWISE,  4}
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(test_table); i++)
	{
		int delta;
		int current, old, wrap, flags;
		current = test_table[i][0];
		old     = test_table[i][1];
		wrap    = test_table[i][2];
		flags   = test_table[i][3];

		delta = getScrollDelta(current, old, wrap, flags);
		assert(delta == test_table[i][4]);

		flags |= AXIS_INVERT;
		delta = getScrollDelta(current, old, wrap, flags);
		assert(delta == -1 * test_table[i][4]);
	}
}

static void
test_get_wheel_button(void)
{
	int delta;
	int button_up, button_dn, action_up, action_dn;

	button_up = 100;
	button_dn = 200;
	action_up = 300;
	action_dn = 400;

	for (delta = -32; delta <= 32; delta++)
	{
		int *action;
		int result = getWheelButton(delta, button_up, button_dn, &action_up, &action_dn, &action);
		if (delta < 0)
		{
			assert(result == button_dn);
			assert(action == &action_dn);
		}
		else if (delta == 0)
		{
			assert(result == 0);
			assert(action == NULL);
		}
		else
		{
			assert(result == button_up);
			assert(action == &action_up);
		}
	}
}

/**
 * Test refcounting of the common struct.
 */
static void
test_common_ref(void)
{
	WacomCommonPtr common;
	WacomCommonPtr second;

	common = wcmNewCommon();
	assert(common);
	assert(common->refcnt == 1);

	second = wcmRefCommon(common);

	assert(second == common);
	assert(second->refcnt == 2);

	wcmFreeCommon(&second);
	assert(common);
	assert(!second);
	assert(common->refcnt == 1);

	second = wcmRefCommon(NULL);
	assert(common != second);
	assert(second->refcnt == 1);
	assert(common->refcnt == 1);

	wcmFreeCommon(&second);
	wcmFreeCommon(&common);
	assert(!second && !common);
}


static void
test_rebase_pressure(void)
{
	WacomDeviceRec priv = {0};
	WacomDeviceRec base = {0};
	WacomDeviceState ds = {0};
	int pressure;

	priv.minPressure = 4;
	ds.pressure = 10;

	/* Pressure in out-of-proximity means get new preloaded pressure */
	priv.oldProximity = 0;

	/* make sure we don't touch priv, not really needed, the compiler should
	 * honor the consts but... */
	base = priv;

	pressure = rebasePressure(&priv, &ds);
	assert(pressure == ds.pressure);

	assert(memcmp(&priv, &base, sizeof(priv)) == 0);

	/* Pressure in-proximity means rebase to new minimum */
	priv.oldProximity = 1;

	base = priv;

	pressure = rebasePressure(&priv, &ds);
	assert(pressure == priv.minPressure);
	assert(memcmp(&priv, &base, sizeof(priv)) == 0);
}

static void
test_normalize_pressure(void)
{
	InputInfoRec pInfo = {0};
	WacomDeviceRec priv = {0};
	WacomCommonRec common = {0};
	WacomDeviceState ds = {0};
	int pressure, prev_pressure = -1;
	int i, j;

	priv.common = &common;
	priv.pInfo = &pInfo;
	pInfo.name = "Wacom test device";

	priv.minPressure = 0;

	/* Some random loop to check various maxZ pressure values. Starting at
	 * 1, because if wcmMaxZ is 0 we have other problems. */
	for (j = 1; j <= 256; j += 17)
	{
		common.wcmMaxZ = j;
		prev_pressure = -1;

		for (i = 0; i <= common.wcmMaxZ; i++)
		{
			ds.pressure = i;

			pressure = normalizePressure(&priv, &ds);
			assert(pressure >= 0);
			assert(pressure <= FILTER_PRESSURE_RES);

			/* we count up, so assume normalised pressure goes up too */
			assert(prev_pressure < pressure);
			prev_pressure = pressure;
		}

		assert(pressure == FILTER_PRESSURE_RES);
	}

	/* If minPressure is higher than ds->pressure, normalizePressure takes
	 * minPressure and ignores actual pressure. This would be a bug in the
	 * driver code, but we might as well test for it. */
	priv.minPressure = 10;
	ds.pressure = 0;

	prev_pressure = normalizePressure(&priv, &ds);
	for (i = 0; i < priv.minPressure; i++)
	{
		ds.pressure = i;

		pressure = normalizePressure(&priv, &ds);

		assert(pressure >= 0);
		assert(pressure < FILTER_PRESSURE_RES);

		/* we count up, so assume normalised pressure goes up too */
		assert(prev_pressure == pressure);
	}
}

/**
 * After a call to wcmInitialToolSize, the min/max and resolution must be
 * set up correctly.
 *
 * wcmInitialToolSize takes the data from the common rec, so test that the
 * priv has all the values of the common.
 */
static void
test_initial_size(void)
{
	InputInfoRec info = {0};
	WacomDeviceRec priv = {0};
	WacomCommonRec common = {0};

	int minx, maxx, miny, maxy, xres, yres;

	info.private = &priv;
	priv.common = &common;

	/* FIXME: we currently assume min of 0 in the driver. we cannot cope
	 * with non-zero devices */
	minx = miny = 0;

	common.wcmMaxX = maxx;
	common.wcmMaxY = maxy;
	common.wcmResolX = xres;
	common.wcmResolY = yres;

	wcmInitialToolSize(&info);

	assert(priv.topX == minx);
	assert(priv.topY == minx);
	assert(priv.bottomX == maxx);
	assert(priv.bottomY == maxy);
	assert(priv.resolX == xres);
	assert(priv.resolY == yres);

	/* Same thing for a touch-enabled device */
	memset(&common, 0, sizeof(common));

	priv.flags = TOUCH_ID;
	assert(IsTouch(&priv));

	common.wcmMaxTouchX = maxx;
	common.wcmMaxTouchY = maxy;
	common.wcmTouchResolX = xres;
	common.wcmTouchResolY = yres;

	wcmInitialToolSize(&info);

	assert(priv.topX == minx);
	assert(priv.topY == minx);
	assert(priv.bottomX == maxx);
	assert(priv.bottomY == maxy);
	assert(priv.resolX == xres);
	assert(priv.resolY == yres);

}

static void
test_suppress(void)
{
	enum WacomSuppressMode rc;
	WacomCommonRec common = {0};
	WacomDeviceState old = {0},
			 new = {0};

	common.wcmSuppress = 2;

	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_ALL);

	/* proximity, buttons and strip send for any change */

#define test_any_suppress(field) \
	old.field = 1; \
	rc = wcmCheckSuppress(&common, &old, &new); \
	assert(rc == SUPPRESS_NONE); \
	new.field = old.field;

	test_any_suppress(proximity);
	test_any_suppress(buttons);
	test_any_suppress(stripx);
	test_any_suppress(stripy);

#undef test_any_suppress

	/* pressure, capacity, throttle, rotation, abswheel only when
	 * difference is above suppress */

	/* test negative and positive transition */
#define test_above_suppress(field) \
	old.field = common.wcmSuppress; \
	rc = wcmCheckSuppress(&common, &old, &new); \
	assert(rc == SUPPRESS_ALL); \
	old.field = common.wcmSuppress + 1; \
	rc = wcmCheckSuppress(&common, &old, &new); \
	assert(rc == SUPPRESS_NONE); \
	old.field = -common.wcmSuppress; \
	rc = wcmCheckSuppress(&common, &old, &new); \
	assert(rc == SUPPRESS_ALL); \
	old.field = -common.wcmSuppress - 1; \
	rc = wcmCheckSuppress(&common, &old, &new); \
	assert(rc == SUPPRESS_NONE); \
	new.field = old.field;

	test_above_suppress(pressure);
	test_above_suppress(throttle);
	test_above_suppress(rotation);
	test_above_suppress(abswheel);

#undef test_above_suppress

	/* any movement on relwheel counts */
	new.relwheel = 1;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_NONE);
	new.relwheel = 0;

	/* x axis movement */

	/* not enough movement */
	new.x = common.wcmSuppress;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_ALL);
	assert(old.x == new.x);
	assert(old.y == new.y);

	/* only x axis above thresh */
	new.x = common.wcmSuppress + 1;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_NON_MOTION);

	/* x and other field above thres */
	new.pressure = ~old.pressure;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_NONE);

	new.pressure = old.pressure;
	new.x = old.x;

	/* y axis movement */
	new.y = common.wcmSuppress;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_ALL);
	assert(old.x == new.x);
	assert(old.y == new.y);

	new.y = common.wcmSuppress + 1;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_NON_MOTION);

	new.pressure = ~old.pressure;
	rc = wcmCheckSuppress(&common, &old, &new);
	assert(rc == SUPPRESS_NONE);
	new.pressure = old.pressure;
}

static void
test_tilt_to_rotation(void)
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
	int i;

	for (i = 0; i < ARRAY_SIZE(rotation_table); i++)
	{
		int rotation;
		int x, y;
		x = rotation_table[i][0];
		y = rotation_table[i][1];
		rotation = wcmTilt2R(x, y, INTUOS4_CURSOR_ROTATION_OFFSET);
		assert(rotation == rotation_table[i][2]);
	}
}


static void
test_mod_buttons(void)
{
	int i;
	for (i = 0; i < sizeof(int) * 8; i++)
	{
		int buttons = mod_buttons(0, i, 1);
		assert(buttons == (1 << i));
		buttons = mod_buttons(0, i, 0);
		assert(buttons == 0);
	}

	assert(mod_buttons(0, sizeof(int) * 8, 1) == 0);
}

static void test_set_type(void)
{
	InputInfoRec info = {0};
	WacomDeviceRec priv = {0};
	WacomTool tool = {0};
	WacomCommonRec common = {0};
	int rc;

#define reset(_info, _priv, _tool, _common) \
	memset(&(_info), 0, sizeof(_info)); \
	memset(&(_priv), 0, sizeof(_priv)); \
	memset(&(_tool), 0, sizeof(_tool)); \
	(_info).private = &(_priv); \
	(_priv).tool = &(_tool); \
	(_priv).common = &(_common);


	reset(info, priv, tool, common);
	rc = wcmSetType(&info, NULL);
	assert(rc == 0);

	reset(info, priv, tool, common);
	rc = wcmSetType(&info, "stylus");
	assert(rc == 1);
	assert(is_absolute(&info));
	assert(IsStylus(&priv));
	assert(!IsTouch(&priv));
	assert(!IsEraser(&priv));
	assert(!IsCursor(&priv));
	assert(!IsPad(&priv));

	reset(info, priv, tool, common);
	rc = wcmSetType(&info, "touch");
	assert(rc == 1);
	/* only some touch screens are absolute */
	assert(!is_absolute(&info));
	assert(!IsStylus(&priv));
	assert(IsTouch(&priv));
	assert(!IsEraser(&priv));
	assert(!IsCursor(&priv));
	assert(!IsPad(&priv));

	reset(info, priv, tool, common);
	rc = wcmSetType(&info, "eraser");
	assert(rc == 1);
	assert(is_absolute(&info));
	assert(!IsStylus(&priv));
	assert(!IsTouch(&priv));
	assert(IsEraser(&priv));
	assert(!IsCursor(&priv));
	assert(!IsPad(&priv));

	reset(info, priv, tool, common);
	rc = wcmSetType(&info, "cursor");
	assert(rc == 1);
	assert(!is_absolute(&info));
	assert(!IsStylus(&priv));
	assert(!IsTouch(&priv));
	assert(!IsEraser(&priv));
	assert(IsCursor(&priv));
	assert(!IsPad(&priv));

	reset(info, priv, tool, common);
	rc = wcmSetType(&info, "pad");
	assert(rc == 1);
	assert(is_absolute(&info));
	assert(!IsStylus(&priv));
	assert(!IsTouch(&priv));
	assert(!IsEraser(&priv));
	assert(!IsCursor(&priv));
	assert(IsPad(&priv));

	reset(info, priv, tool, common);
	rc = wcmSetType(&info, "foobar");
	assert(rc == 0);

#undef reset
}

static int test_flag_set(void)
{
	int i;
	unsigned int flags = 0;

	for (i = 0; i < sizeof(flags); i++)
	{
		int mask = 1 << i;
		flags = 0;

		assert(!MaskIsSet(flags, mask));
		MaskSet(flags, mask);
		assert(flags != 0);
		assert(MaskIsSet(flags, mask));
		MaskClear(flags, mask);
		assert(!MaskIsSet(flags, mask));
		assert(flags == 0);
	}
}

int main(int argc, char** argv)
{
	test_common_ref();
	test_rebase_pressure();
	test_normalize_pressure();
	test_suppress();
	test_initial_size();
	test_tilt_to_rotation();
	test_mod_buttons();
	test_set_type();
	test_flag_set();
	test_get_scroll_delta();
	test_get_wheel_button();
	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
