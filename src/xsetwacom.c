/*****************************************************************************
** xsetwacom.c
**
** Copyright (C) 2003 - John E. Joganic
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**
** REVISION HISTORY
**   2003-05-02 0.0.1 - JEJ - created
**
****************************************************************************/

#define XSETWACOM_VERSION "0.0.1"

#include "wacomcfg.h"
#include "Xwacom.h" /* give use raw access to parameter values */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <assert.h>

	int gnVerbose = 0;
	int gnLastXError = 0;
	const char* gpszDisplayName = NULL;

typedef struct _PARAMINFO PARAMINFO;

typedef enum
{
	COMMAND_NONE,
	COMMAND_LIST,
	COMMAND_SET,
} COMMAND;

struct _PARAMINFO
{
	const char* pszParam;
	const char* pszDesc;
	int nParamID;
	int bEmptyOK;
	int bRange;
	int nMin, nMax;
};

/*****************************************************************************
** Parameters
*****************************************************************************/

	#define VALUE_REQUIRED 0
	#define VALUE_OPTIONAL 1
	#define RANGE 1

	static PARAMINFO gParamInfo[] =
	{
		{ "TopX",
			"Bounding rect left coordinate in tablet units",
			XWACOM_PARAM_TOPX },

		{ "TopY",
			"Bounding rect top coordinate in tablet units",
			XWACOM_PARAM_TOPY },

		{ "BottomX",
			"Bounding rect right coordinate in tablet units",
			XWACOM_PARAM_BOTTOMX },

		{ "BottomY",
			"Bounding rect bottom coordinate in tablet units",
			XWACOM_PARAM_BOTTOMY },

		{ "Button1",
			"Button number to which button 1 should be mapped",
			XWACOM_PARAM_BUTTON1,
			VALUE_REQUIRED, RANGE, 1, 5 },

		{ "Button2",
			"Button number to which button 2 should be mapped",
			XWACOM_PARAM_BUTTON2,
			VALUE_REQUIRED, RANGE, 1, 5 },

		{ "Button3",
			"Button number to which button 3 should be mapped",
			XWACOM_PARAM_BUTTON3,
			VALUE_REQUIRED, RANGE, 1, 5 },

		{ "Button4",
			"Button number to which button 4 should be mapped",
			XWACOM_PARAM_BUTTON4,
			VALUE_REQUIRED, RANGE, 1, 5 },

		{ "Button5",
			"Button number to which button 5 should be mapped",
			XWACOM_PARAM_BUTTON5,
			VALUE_REQUIRED, RANGE, 1, 5 },

		{ NULL }
	};


/*****************************************************************************
** Implementation
*****************************************************************************/

void Usage(FILE* f)
{
	fprintf(f,
	"Usage: xsetwacom [options] [command [arguments...]]\n"
	"Options:\n"
	"  -h, --help                 - usage\n"
	"  -v, --verbose              - verbose output\n"
	"  -V, --version              - version info\n"
	"  -d, --display disp_name    - override default display\n"
	"\n"
	"Commands:\n"
	"  list [dev|param]           - display known devices, parameters\n"
	"  set dev_name param value   - set param=value on device\n");
}

static int ErrorHandler(Display* pDisp, XErrorEvent* pEvent)
{
	char chBuf[64];
	XGetErrorText(pDisp,pEvent->error_code,chBuf,sizeof(chBuf));
	fprintf(stderr,"X Error: %d %s\n", pEvent->error_code, chBuf);
	gnLastXError  = pEvent->error_code;
	return 0;
}

/*static int GetLastXError(void)
{
	return gnLastXError;
}*/

static void WacomConfigError(int err, const char* pszText)
{
	fprintf(stderr,"WacomConfigError: %d %s\n",err,pszText);
}

typedef struct { WACOMDEVICETYPE type; const char* pszText; } TYPEXLAT;

static int ListDev(WACOMCONFIG hConfig, char** argv)
{
	const char* pszType;
	WACOMDEVICEINFO* pInfo;
	unsigned int i, j, uCount;

	static TYPEXLAT xTypes[] =
	{
		{ WACOMDEVICETYPE_CURSOR, "cursor" },
		{ WACOMDEVICETYPE_STYLUS, "stylus" },
		{ WACOMDEVICETYPE_ERASER, "eraser" }
	};

	if (*argv != NULL)
		fprintf(stderr,"ListDev: Ignoring extraneous arguments.\n");

	if (WacomConfigListDevices(hConfig,&pInfo,&uCount))
		return 1;

	for (i=0; i<uCount; ++i)
	{
		pszType = "unknown";
		for (j=0; j<sizeof(xTypes)/sizeof(*xTypes); ++j)
			if (xTypes[j].type == pInfo[i].type)
				pszType = xTypes[j].pszText;

		fprintf(stdout,"%-16s %-10s\n",pInfo[i].pszName,pszType);
	}

	WacomConfigFree(pInfo);
	return 0;
}

static int ListParam(WACOMCONFIG hConfig, char** argv)
{
	PARAMINFO* p;

	if (*argv != NULL)
		fprintf(stderr,"ListParam: Ignoring extraneous arguments.\n");

	for (p=gParamInfo; p->pszParam; ++p)
		fprintf(stdout,"%-16s - %s\n",
			p->pszParam, p->pszDesc);
	return 0;
}

static int List(WACOMCONFIG hConfig, char** argv)
{
	if (*argv == NULL)
		return ListDev(hConfig,argv);

	if (!strcmp(*argv,"dev"))
		return ListDev(hConfig,argv+1);

	if (!strcmp(*argv,"param"))
		return ListParam(hConfig,argv+1);

	fprintf(stderr,"List: Unknown argument '%s'\n",*argv);
	return 1;
}

static int Set(WACOMCONFIG hConfig, char** argv)
{
	int nValue;
	PARAMINFO* p;
	char* a, *pszEnd;
	const char* pszDev = NULL;
	const char* pszParam = NULL;
	const char* pszValue = NULL;

	while ((a=*argv++) != NULL)
	{
		if (!pszDev) pszDev = a;
		else if (!pszParam) pszParam = a;
		else if (!pszValue) pszValue = a;
		else
		{
			fprintf(stderr,"Set: Unknown argument '%s'\n",a);
			return 1;
		}
	}

	/* No device or param? Error. */
	if (!pszDev || !pszParam)
		{ Usage(stderr); return 1; }

	/* Find param. */
	for (p=gParamInfo; p->pszParam; ++p)
		if (strcasecmp(p->pszParam,pszParam) == 0)
			break;

	/* Unknown? Complain. */
	if (!p->pszParam)
	{
		fprintf(stderr,"Set: Unknown parameter '%s'\n",pszParam);
		return 1;
	}

	/* If value is empty, do we support this? */
	if (!pszValue && !p->bEmptyOK)
	{
		fprintf(stderr,"Set: Value for '%s' must be specified.\n",
			pszParam);
		return 1;
	}

	/* Convert value to 32 bit integer; dec, hex, octal are all OK. */
	nValue = strtol(pszValue,&pszEnd,0);
	if ((pszEnd == pszValue) || (*pszEnd != '\0'))
	{
		fprintf(stderr,"Set: Value '%s' is invalid.\n",pszValue);
		return 1;
	}

	/* Is there a range and are we in it? */
	if (p->bRange && ((nValue < p->nMin) || (nValue > p->nMax)))
	{
		fprintf(stderr,"Set: Value for '%s' out of range (%d - %d)\n",
			pszParam, p->nMin, p->nMax);
		return 1;
	}

	/* Looks good, send it. */
	if (gnVerbose)
		printf("Set: sending %d %d (0x%X)\n",p->nParamID,nValue,nValue);
	
	return 0;
}

int DoCommand(COMMAND cmd, char** argv)
{
	int nReturn;
	Display* pDisp;
	WACOMCONFIG hConf;

	pDisp = XOpenDisplay(gpszDisplayName);
	if (!pDisp)
	{
		fprintf(stderr,"Failed to open display (%s)\n",
			gpszDisplayName ? gpszDisplayName : "");
		return 1;
	}

	XSetErrorHandler(ErrorHandler);
	XSynchronize(pDisp,1 /*sync on*/);

	hConf = WacomConfigInit(pDisp,WacomConfigError);
	if (!hConf)
	{
		fprintf(stderr,"Failed to init WacomConfig\n");
		XCloseDisplay(pDisp);
		return 1;
	}

	switch (cmd)
	{
		case COMMAND_LIST:
			nReturn = List(hConf,argv);
			break;

		case COMMAND_SET:
			nReturn = Set(hConf,argv);
			break;

		default:
			assert(0);
	}

	WacomConfigTerm(hConf);
	XCloseDisplay(pDisp);
	return nReturn;
}

int main(int argc, char** argv)
{
	char* a;

	++argv;
	while ((a=*argv++) != NULL)
	{
		if (!strcmp(a,"-h") || !strcmp(a,"--help"))
		{
			Usage(stdout);
			return 0;
		}
		else if (!strcmp(a,"-v") || !strcmp(a,"--verbose"))
			++gnVerbose;
		else if (!strcmp(a,"-V") || !strcmp(a,"--version"))
		{
			fprintf(stdout,"%s\n",XSETWACOM_VERSION);
			return 0;
		}
		else if (!strcmp(a,"-d") || !strcmp(a,"--display"))
		{
			a = *argv++;
			if (!a)
			{
				fprintf(stderr,"Missing display name\n");
				return 1;
			}
			gpszDisplayName = a;
		}

		/* commands */
		else if (!strcmp(a,"list"))
			return DoCommand(COMMAND_LIST,argv);
		else if (!strcmp(a,"set"))
			return DoCommand(COMMAND_SET,argv);
		else
		{
			fprintf(stderr,"Unknown command '%s'\n", a);
			Usage(stderr);
			return 1;
		}
	}

	Usage(stdout);
	return 0;
}
