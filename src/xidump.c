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

/*****************************************************************************
** XInput
*****************************************************************************/

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>

	int gnDevListCnt = 0;
	XDeviceInfoPtr gpDevList = NULL;

Display* InitXInput(void)
{
	Display* pDisp;
	int nMajor, nFEV, nFER;

	pDisp = XOpenDisplay(NULL);
	if (!pDisp)
	{
		fprintf(stderr,"Failed to connect to X server.\n");
		return NULL;
	}

	XSynchronize(pDisp,1);
	
	if (!XQueryExtension(pDisp,INAME,&nMajor,&nFEV,&nFER))
	{
		fprintf(stderr,"Server does not support XInput extension.\n");
		XCloseDisplay(pDisp);
		return NULL;
	}

	return pDisp;
}

int ListDevices(Display* pDisp)
{
	int i, j;

	/* get list of devices */
	if (!gpDevList)
	{
		gpDevList = (XDeviceInfoPtr) XListInputDevices(pDisp, &gnDevListCnt);
		if (!gpDevList)
		{
			fprintf(stderr,"Failed to get input device list.\n");
			return 1;
		}
	}

	for (i=0; i<gnDevListCnt; ++i)
	{
		printf("%3lu %-15s %s\n",
				gpDevList[i].id,
				gpDevList[i].name,
				(gpDevList[i].use == 0) ? "(disabled)" :
				(gpDevList[i].use == IsXKeyboard) ? "keyboard" :
				(gpDevList[i].use == IsXPointer) ? "pointer" :
				(gpDevList[i].use == IsXExtensionDevice) ? "extension" :
					"unknown");

		for (j=0; j<gpDevList[i].num_classes; ++j)
		{
		}
	}

	return 0;
}

XDeviceInfoPtr GetDevice(Display* pDisp, const char* pszDeviceName)
{
	int i;

	/* get list of devices */
	if (!gpDevList)
	{
		gpDevList = (XDeviceInfoPtr) XListInputDevices(pDisp, &gnDevListCnt);
		if (!gpDevList)
		{
			fprintf(stderr,"Failed to get input device list.\n");
			return NULL;
		}
	}

	/* find device by name */
	for (i=0; i<gnDevListCnt; ++i)
	{
		if (strcasecmp(gpDevList[i].name,pszDeviceName) == 0)
			return gpDevList + i;
	}

	return NULL;
}

/*****************************************************************************
** UI
*****************************************************************************/

typedef struct _UI UI;
struct _UI
{
	const char* pszName;
	int (*Init)(void);
	void (*Term)(void);
	int (*Run)(Display* pDisp, XDevice* pDev);
};

/*****************************************************************************
** GTK UI
*****************************************************************************/

#if WCM_ENABLE_GTK12 || WCM_ENABLE_GTK20
#define USE_GTK 1
#include <gtk/gtk.h>

static int GTKInit(void)
{
	return 1;
}

static void GTKTerm(void)
{
}

static int GTKRun(Display* pDisp, XDevice* pDev)
{
	return 1;
}

	UI gGTKUI = { "gtk", GTKInit, GTKTerm, GTKRun };
#else
#define USE_GTK 0
#endif

/*****************************************************************************
** Curses UI
*****************************************************************************/

static int CursesInit(void)
{
	return 1;
}

static void CursesTerm(void)
{
}

static int CursesRun(Display* pDisp, XDevice* pDev)
{
	return 1;
}

	UI gCursesUI = { "curses", CursesInit, CursesTerm, CursesRun };

/*****************************************************************************
** Raw UI
*****************************************************************************/

static int RawInit(void)
{
	return 0;
}

static void RawTerm(void)
{
}

static int RawRun(Display* pDisp, XDevice* pDev)
{
	return 0;
}

	UI gRawUI = { "raw", RawInit, RawTerm, RawRun };

/****************************************************************************/

void Usage(int rtn)
{
	fprintf(rtn ? stderr : stdout,
			"Usage: xidump [options] input_device\n"
			"  -h, --help          - usage\n"
			"  -v, --verbose       - verbose\n"
			"  -V, --version       - version\n"
			"  -l, --list          - list available input devices\n"
			"  -u, --ui ui_type    - use specified ui, see below\n"
			"\n"
			"Use --list option for input_device choices\n"
			"UI types: "
			#if USE_GTK
				"gtk, "
			#endif
			"curses, raw\n");
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

int Run(Display* pDisp, UI* pUI, const char* pszDeviceName)
{
	int nRtn;
	XDevice* pDev;
	XDeviceInfoPtr pDevInfo;

	/* get the device by name */
	pDevInfo = GetDevice(pDisp,pszDeviceName);
	if (!pDevInfo)
	{
		fprintf(stderr,"Unable to find input device '%s'\n",pszDeviceName);
		return 1;
	}

	/* open device */
	pDev = XOpenDevice(pDisp,pDevInfo->id);
	if (!pDev)
	{
		fprintf(stderr,"Unable to open input device '%s'\n",pszDeviceName);
		return 1;
	}
	
	/* fire up the UI */
	if ((nRtn=pUI->Init()) != 0)
		fprintf(stderr,"failed to initialize UI\n");
	else
	{
		if ((nRtn=pUI->Run(pDisp,pDev)) != 0)
			fprintf(stderr,"failed to run UI\n");
		pUI->Term();
	}

	XCloseDevice(pDisp,pDev);

	return nRtn;
}

/*****************************************************************************
** main
*****************************************************************************/

int main(int argc, char** argv)
{
	int nRtn;
	int bList = 0;
	UI* pUI = NULL;
	int nVerbose = 0;
	const char* pa;
	Display* pDisp = NULL;
	const char* pszDeviceName = NULL;

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
			else if ((strcmp(pa,"-l") == 0) || (strcmp(pa,"--list") == 0))
				bList = 1;
			else if ((strcmp(pa,"-u") == 0) || (strcmp(pa,"--ui") == 0))
			{
				pa = *(argv++);
				if (!pa) Fatal("Missing ui argument\n");
				if (strcmp(pa,"gtk") == 0)
				{
					#if USE_GTK
					pUI = &gGTKUI;
					#else
					Fatal("Not configured for GTK.\n");
					#endif
				}
				else if (strcmp(pa,"curses") == 0)
					pUI = &gCursesUI;
				else if (strcmp(pa,"raw") == 0)
					pUI = &gRawUI;
				else
					Fatal("Unknown ui option %s\n",pa);
			}
			else
				Fatal("Unknown option %s\n",pa);
		}
		else if (!pszDeviceName)
			pszDeviceName = pa;
		else
			Fatal("Unknown argument %s\n",pa);
	}

	/* device must be specified */
	if (!pszDeviceName && !bList)
	{
		fprintf(stderr,"input_device not specified\n");
		Usage(1);
	}

	/* default to a given UI */
	if (pUI == NULL)
	{
		pUI = &gRawUI;

//		#if USE_GTK
//		pUI = &gGTKUI;
//		#else
//		pUI = &gCursesUI;
//		#endif
	}
	
	/* open connection to XServer with XInput */
	pDisp = InitXInput();
	if (!pDisp) exit(1);

	if (bList)
		nRtn = ListDevices(pDisp);
	else
		nRtn = Run(pDisp,pUI,pszDeviceName);

	/* release device list */
	if (gpDevList)
		XFreeDeviceList(gpDevList);

	XCloseDisplay(pDisp);

	return nRtn;
}
