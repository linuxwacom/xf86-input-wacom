/*
 * Copyright 2023 Red Hat, Inc
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
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "wcmPressureCurve.h"

static void
usage(void) {
	printf("Usage: pressurecurve [OPTIONS] x1 y1 x2 y2 ...\n");
	printf("\n");
	printf("This tool takes four coordinates in the range [0.0, 1.0] representing \n"
	       "the driver's pressure curve configuration.\n"
	       "The output contains one line with input pressure and output pressure for\n"
	       "each normalized [0.0, 1.0] input pressure value\n"
	       "\n"
	       "Multiple sets of 4 coordinates may be given \n");
}

int main(int argc, char **argv)
{
	/* The driver pre-calculates the pressurecurve for each possible
	 * pressure value. Typically this is 2k or 8k for newer pens, here
	 * we use 1000 points to get smooth gnuplot output.
	 *
	 * The value in filterCurveToLine() is normalized into this range so
	 * the curve will have values in the range [0, 1000].
	 */
	const size_t npoints = 1000;

	/* These two are hardcoded by the driver */
	const double x0 = 0.0, y0 = 0.0;
	const double x3 = 1.0, y3 = 1.0;

	enum {
		OPT_HELP,
	};

	static struct option long_options[] = {
		{"help", no_argument, 0, OPT_HELP},
		{0, 0, 0, 0},
	};

	int c;
	while (1)
	{
		int option_index = 0;

		c = getopt_long(argc, argv, "", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case OPT_HELP:
			usage();
			return 0;
		default:
			break;
		}
	}

	if (optind + 4 > argc || (argc - optind) % 4 != 0) {
		usage();
		return 1;

	}

	size_t ncurves = (argc - optind) / 4;
	int curve[ncurves][npoints];
	int idx = 0;

	printf("# column 0: input pressure value [0,1]\n");
	while (optind < argc)
	{
		double x1 = atof(argv[optind++]);
		double y1 = atof(argv[optind++]);
		double x2 = atof(argv[optind++]);
		double y2 = atof(argv[optind++]);

		filterCurveToLine(curve[idx], npoints, x0, y0, x1, y1, x2, y2, x3, y3);

		printf("# column %d: %f/%f %f/%f %f/%f %f/%f\n", idx, x0, y0, x1, y1, x2, y2, x3, y3);
		idx++;
	}

	for (size_t i = 0; i < npoints; i++)
	{
		printf("%f", i/(double)npoints);
		for (size_t j = 0; j < ncurves; j++)
			printf(" %f", curve[j][i]/(double)npoints);
		printf("\n");
	}

	return 0;
}
