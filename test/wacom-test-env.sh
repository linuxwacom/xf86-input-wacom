#!/bin/sh
#
# Wrapper to set up the right environment variables and start a nested
# shell. Usage:
#
# $ sudo ./test/wacom-test-env.sh
# (nested shell) $ pytest
# (nested shell) $ exit

builddir=$(find $PWD -name meson-logs -printf "%h" -quit)

if [ -z "$builddir" ]; then
    echo "Unable to find meson builddir"
    exit 1
fi

echo "Using meson builddir: $builddir"

export LD_LIBRARY_PATH="$builddir:$LD_LIBRARY_PATH"
export GI_TYPELIB_PATH="$builddir:$GI_TYPELIB_PATH"

# Don't think this is portable, but oh well
${SHELL}
