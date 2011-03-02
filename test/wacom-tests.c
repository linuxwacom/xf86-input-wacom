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

#include <glib.h>
#include "fake-symbols.h"
#include <xf86Wacom.h>

/**
 * NOTE: this file may not contain tests that require static variables. The
 * current compiler undef static magic makes them local variables and thus
 * change the behaviour.
 */

/**
 * Test refcounting of the common struct.
 */
static void
test_common_ref(void)
{
	WacomCommonPtr common;
	WacomCommonPtr second;

	common = wcmNewCommon();
	g_assert(common);
	g_assert(common->refcnt == 1);

	second = wcmRefCommon(common);

	g_assert(second == common);
	g_assert(second->refcnt == 2);

	wcmFreeCommon(&second);
	g_assert(common);
	g_assert(!second);
	g_assert(common->refcnt == 1);

	second = wcmRefCommon(NULL);
	g_assert(common != second);
	g_assert(second->refcnt == 1);
	g_assert(common->refcnt == 1);

	wcmFreeCommon(&second);
	wcmFreeCommon(&common);
	g_assert(!second && !common);
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
	g_assert(pressure == ds.pressure);

	g_assert(memcmp(&priv, &base, sizeof(priv)) == 0);

	/* Pressure in-proximity means rebase to new minimum */
	priv.oldProximity = 1;

	base = priv;

	pressure = rebasePressure(&priv, &ds);
	g_assert(pressure == priv.minPressure);
	g_assert(memcmp(&priv, &base, sizeof(priv)) == 0);
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
			g_assert(pressure >= 0);
			g_assert(pressure <= FILTER_PRESSURE_RES);

			/* we count up, so assume normalised pressure goes up too */
			g_assert(prev_pressure < pressure);
			prev_pressure = pressure;
		}

		g_assert(pressure == FILTER_PRESSURE_RES);
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

		g_assert(pressure >= 0);
		g_assert(pressure < FILTER_PRESSURE_RES);

		/* we count up, so assume normalised pressure goes up too */
		g_assert(prev_pressure == pressure);
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

	g_assert(priv.topX == minx);
	g_assert(priv.topY == minx);
	g_assert(priv.bottomX == maxx);
	g_assert(priv.bottomY == maxy);
	g_assert(priv.resolX == xres);
	g_assert(priv.resolY == yres);

	/* Same thing for a touch-enabled device */
	memset(&common, 0, sizeof(common));

	priv.flags = TOUCH_ID;
	g_assert(IsTouch(&priv));

	common.wcmMaxTouchX = maxx;
	common.wcmMaxTouchY = maxy;
	common.wcmTouchResolX = xres;
	common.wcmTouchResolY = yres;

	wcmInitialToolSize(&info);

	g_assert(priv.topX == minx);
	g_assert(priv.topY == minx);
	g_assert(priv.bottomX == maxx);
	g_assert(priv.bottomY == maxy);
	g_assert(priv.resolX == xres);
	g_assert(priv.resolY == yres);

}


int main(int argc, char** argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/common/refcounting", test_common_ref);
	g_test_add_func("/common/rebase_pressure", test_rebase_pressure);
	g_test_add_func("/common/normalize_pressure", test_normalize_pressure);
	g_test_add_func("/xfree86/initial_size", test_initial_size);
	return g_test_run();
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
