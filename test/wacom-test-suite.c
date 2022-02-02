/*
 * Copyright 2022 Red Hat, Inc
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


#ifndef ENABLE_TESTS
#error "Expected ENABLE_TESTS to be defined"
#endif

#include <config.h>
#include <stdio.h>
#include "wacom-test-suite.h"

void wcm_run_tests(void);

extern const struct test_case_decl __start_test_section;
extern const struct test_case_decl __stop_test_section;

/* This one needs to be defined for dlopen to be able to load our test module,
 * RTLD_LAZY only applies to functions. */
void *serverClient;

/* The entry point: iterate through the tests and run them one-by-one. Any
 * test that doesn't assert is considered successful.
 */
void wcm_run_tests(void) {

	const struct test_case_decl *t;
	for (t = &__start_test_section; t < &__stop_test_section; t++) {
		printf("- running %-32s", t->name);
		fflush(stdout);
		t->func();
		printf("SUCCCESS\n");
	}
}
