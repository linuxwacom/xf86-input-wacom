/*****************************************************************************
** xidump.c
**
** Copyright (C) 2003 - John E. Joganic
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**
** REVISION HISTORY
**   2003-02-23 0.0.1 - created for GTK1.2
**
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define XIDUMP_VERSION "0.0.1"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

typedef struct _GUI GUI;
struct _GUI
{
	const char* pszName;
};

/*****************************************************************************
** GTK GUI
*****************************************************************************/

#if WCM_ENABLE_GTK12 || WCM_ENABLE_GTK20
#define USE_GTK 1
#include <gtk/gtk.h>
	GUI gGTKGUI = { "gtk" };
#else
#define USE_GTK 0
#endif

/*****************************************************************************
** Curses GUI
*****************************************************************************/

	GUI gCursesGUI = { "curses" };

/*****************************************************************************
** Raw GUI
*****************************************************************************/

	GUI gRawGUI = { "raw" };

/****************************************************************************/

void Usage(int rtn)
{
	fprintf(rtn ? stderr : stdout,
			"Usage: xidump [options]\n"
			"  -h, --help          - usage\n"
			"  -v, --verbose       - verbose\n"
			"  -V, --version       - version\n"
			"  -g, --gui gui_type  - use specified gui, see below\n"
			"\n"
			"GUI types: gtk, curses, raw\n");
	exit(rtn);
}

void Version(void)
{
	fprintf(stdout,"%s\n",XIDUMP_VERSION);
}

void Fatal(const char* pszFmt, ...)
{
	va_list args;
	va_start(args,pszFmt);
	vfprintf(stderr,pszFmt,args);
	va_end(args);
	exit(1);
}

/****************************************************************************/

int main(int argc, char** argv)
{
	GUI* pGUI = NULL;
	int nVerbose = 0;
	const char* pa;

	++argv;
	while ((pa = *(argv++)) != NULL)
	{
		if (pa[0] == '-')
		{
			if ((strcmp(pa,"-h") == 0) || (strcmp(pa,"--help") == 0))
				Usage(0);
			else if ((strcmp(pa,"-v") == 0) || (strcmp(pa,"--verbose") == 0))
				++nVerbose;
			else if ((strcmp(pa,"-V") == 0) || (strcmp(pa,"--version") == 0))
				{ Version(); exit(0); }
			else if ((strcmp(pa,"-g") == 0) || (strcmp(pa,"--gui") == 0))
			{
				pa = *(argv++);
				if (!pa) Fatal("Missing gui argument\n");
				if (strcmp(pa,"gtk") == 0)
				{
					#if USE_GTK
					pGUI = &gGTKGUI;
					#else
					Fatal("Not configured for GTK GUI.\n");
					#endif
				}
				else if (strcmp(pa,"curses") == 0)
					pGUI = &gCursesGUI;
				else if (strcmp(pa,"raw") == 0)
					pGUI = &gRawGUI;
				else
					Fatal("Unknown gui option %s\n",pa);
			}
			else
				Fatal("Unknown option %s\n",pa);
		}
		else
			Fatal("Unknown argument %s\n",pa);
	}

	/* default to a given GUI */
	if (pGUI == NULL)
	{
		#if USE_GTK
		pGUI = &gGTKGUI;
		#else
		pGUI = &gCursesGUI;
		#endif
	}
	
	printf("xidump: using %s GUI\n",pGUI->pszName);
	
	return 0;
}
