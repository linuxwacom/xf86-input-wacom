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


int main(int argc, char** argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/common/refcounting", test_common_ref);
    return g_test_run();
}
