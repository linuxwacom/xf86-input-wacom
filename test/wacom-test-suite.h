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

#ifndef __WACOM_TEST_SUITE_H
#define __WACOM_TEST_SUITE_H

#include <config.h>

struct test_case_decl {
	const char *name;
	void (*func)(void);
};

/**
 * For each test case with "tname", define a struct "_decl_tname" and put it
 * in the "test_section" of the resulting ELF object.
 *
 * wcm_run_tests() then iterates through these objects using a
 * compiler-provided variable and can call the actual function for each test.
 */
#define TEST_CASE(tname) \
        static void (tname)(void); \
        static const struct test_case_decl _decl_##tname \
        __attribute__((used)) \
        __attribute((section("test_section"))) = { \
           .name = #tname, \
           .func = tname, \
        }; \
        static void (tname)(void)


/**
 * These may be called by a test function - #define them so they are always
 * available.
 */
#define wcmLog(priv, type, ...) fprintf(stderr, __VA_ARGS__)
#define wcmLogSafe(priv, type, ...) fprintf(stderr, __VA_ARGS__)
#define wcmLogCommon(common, type, ...) fprintf(stderr, __VA_ARGS__)
#define wcmLogCommonSafe(common, type, ...) fprintf(stderr, __VA_ARGS__)
#define wcmLogDebugCommon(common, level, func, ...) fprintf(stderr, __VA_ARGS__)
#define wcmLogDebugDevice(priv, level, func, ...) fprintf(stderr, __VA_ARGS__)

#endif /* __WACOM_TEST_SUITE_H */
