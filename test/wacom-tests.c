#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include "fake-symbols.h"
#include <xf86Wacom.h>


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

int main(int argc, char** argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/common/refcounting", test_common_ref);
	g_test_add_func("/common/rebase_pressure", test_rebase_pressure);
	g_test_add_func("/common/normalize_pressure", test_normalize_pressure);
	return g_test_run();
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
