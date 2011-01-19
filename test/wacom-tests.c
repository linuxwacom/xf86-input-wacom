#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include "fake-symbols.h"


int main(int argc, char** argv)
{
    g_test_init(&argc, &argv, NULL);
    return g_test_run();
}
