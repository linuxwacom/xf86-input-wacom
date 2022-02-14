/*
 * Copyright 2022 © Red Hat, Inc.
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

#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>

#define TESTDRV "wacom_drv_test.so"
#define TESTFUNC "wcm_run_tests"

int main(void) {
	void *handle = dlopen(TESTDRV, RTLD_LAZY);
	void (*func)(void);

	if (handle == NULL) {
		fprintf(stderr, "Failed to open %s: %s\n", TESTDRV, dlerror());
		fprintf(stderr, "This test suite relies on dlopen(RTLD_LAZY) which may be disabled by your compiler/linker flags\n");
		return 77;
	}

	func = dlsym(handle, TESTFUNC);
	if (func == NULL) {
		fprintf(stderr, "Failed to load %s: %s\n", TESTFUNC, dlerror());
		return 1;
	}

	func();

	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
