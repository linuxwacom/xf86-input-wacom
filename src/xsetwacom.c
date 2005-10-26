/*****************************************************************************
** xsetwacom.c
**
** Copyright (C) 2003 - John E. Joganic
** Copyright (C) 2004-2005 - Ping Cheng
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
**   2005-10-24 0.0.4 - PC - added Pad
*
****************************************************************************/

#define XSETWACOM_VERSION "0.0.4"

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
	COMMAND_SET
} COMMAND;

struct _PARAMINFO
{
	const char* pszParam;
	const char* pszDesc;
	int nParamID;
	int bEmptyOK;
	int bRange;
	int nMin, nMax;
	int nType;
	int nDefault;
};

/*****************************************************************************
** Parameters
*****************************************************************************/

	#define VALUE_REQUIRED 0
	#define VALUE_OPTIONAL 1
	#define RANGE 1
	#define SINGLE_VALUE 0
	#define PACKED_CURVE 1
	#define BOOLEAN_VALUE 2

	static PARAMINFO gParamInfo[] =
	{
		{ "TopX",
			"Bounding rect left coordinate in tablet units",
			XWACOM_PARAM_TOPX, VALUE_REQUIRED },

		{ "TopY",
			"Bounding rect top coordinate in tablet units",
			XWACOM_PARAM_TOPY, VALUE_REQUIRED },

		{ "BottomX",
			"Bounding rect right coordinate in tablet units",
			XWACOM_PARAM_BOTTOMX, VALUE_REQUIRED },

		{ "BottomY",
			"Bounding rect bottom coordinate in tablet units",
			XWACOM_PARAM_BOTTOMY, VALUE_REQUIRED },

		{ "Button1",
			"Button number to which button 1 should be mapped",
			XWACOM_PARAM_BUTTON1,
			VALUE_OPTIONAL, RANGE, 
			1, 19, SINGLE_VALUE, 1 },

		{ "Button2",
			"Button number to which button 2 should be mapped",
			XWACOM_PARAM_BUTTON2,
			VALUE_OPTIONAL, RANGE, 
			1, 19, SINGLE_VALUE, 2 },

		{ "Button3",
			"Button number to which button 3 should be mapped",
			XWACOM_PARAM_BUTTON3,
			VALUE_OPTIONAL, RANGE, 
			1, 19, SINGLE_VALUE, 3 },

		{ "Button4",
			"Button number to which button 4 should be mapped",
			XWACOM_PARAM_BUTTON4,
			VALUE_OPTIONAL, RANGE, 
			1, 19, SINGLE_VALUE, 4 },

		{ "Button5",
			"Button number to which button 5 should be mapped",
			XWACOM_PARAM_BUTTON5,
			VALUE_OPTIONAL, RANGE, 
			1, 19, SINGLE_VALUE, 5 },

		{ "DebugLevel",
			"Level of debugging trace, default is 1",
			XWACOM_PARAM_DEBUGLEVEL,
			VALUE_OPTIONAL, RANGE, 
			1, 100, SINGLE_VALUE, 1 },

		{ "PressCurve",
			"Bezier curve for pressure (default is 0 0 100 100)",
			XWACOM_PARAM_PRESSCURVE,
			VALUE_OPTIONAL, RANGE, 0, 100,
			PACKED_CURVE },

		{ "RawFilter",
			"Enables and disables filtering of raw data, "
			"default is true.",
			XWACOM_PARAM_RAWFILTER,
			VALUE_OPTIONAL, RANGE, 
			0, 1, BOOLEAN_VALUE, 1 },	

		{ "Mode",
			"Switches cursor movement mode (default is absolute)",
			XWACOM_PARAM_MODE,
			VALUE_OPTIONAL, RANGE, 
			0, 1, BOOLEAN_VALUE, 1 },	

		{ "SpeedLevel",
			"Sets relative cursor movement speed (default is 6)",
			XWACOM_PARAM_SPEEDLEVEL,
			VALUE_OPTIONAL, RANGE, 
			1, 11, SINGLE_VALUE, 6 },	

		{ "ClickForce",
			"Sets tip/eraser pressure threshold = ClickForce*MaxZ/100"
			"(default is 6)",
			XWACOM_PARAM_CLICKFORCE,
			VALUE_OPTIONAL, RANGE, 
			1, 21, SINGLE_VALUE, 6 },	

		{ "Accel",
			"Sets relative cursor movement acceleration"
			"(default is 1)",
			XWACOM_PARAM_ACCEL,
			VALUE_OPTIONAL, RANGE, 
			1, 7, SINGLE_VALUE, 1 },
	
		{ "xyDefault",
			"Resets the bounding coordinates to default in tablet units",
			XWACOM_PARAM_XYDEFAULT, VALUE_OPTIONAL },

		{ "gimp",
			"Turns on/off Gimp support in Xinerama-enabled "
			"multi-monitor desktop, default is on",
			XWACOM_PARAM_GIMP, VALUE_OPTIONAL, 
			RANGE, 0, 1, BOOLEAN_VALUE, 1 },

		{ "mmonitor",
			"Turns on/off across monitor movement in"
			"multi-monitor desktop, default is on",
			XWACOM_PARAM_MMT, VALUE_OPTIONAL, 
			RANGE, 0, 1, BOOLEAN_VALUE, 1 },

		{ "TPCButton",
			"Turns on/off Tablet PC buttons."
			"default is off for regular tablets, "
			"on for Tablet PC. ",
			XWACOM_PARAM_TPCBUTTON, VALUE_OPTIONAL, 
			RANGE, 0, 1, BOOLEAN_VALUE, 1 },

		{ "FileModel",
			"Writes tablet models to /etc/wacom.dat",
			XWACOM_PARAM_FILEMODEL, VALUE_OPTIONAL },

		{ "FileOption",
			"Writes configuration options to /etc/X11/wcm.dev_name",
			XWACOM_PARAM_FILEOPTION, VALUE_OPTIONAL },

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
	"  set dev_name param [values...]   - set device parameter by name\n");
}

static int XError(Display* pDisp, XErrorEvent* pEvent)
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

static void Error(int err, const char* pszText)
{
	fprintf(stderr,"Error (%d): %s\n",err,pszText);
}

typedef struct { WACOMDEVICETYPE type; const char* pszText; } TYPEXLAT;

static int ListDev(WACOMCONFIG *hConfig, char** argv)
{
	const char* pszType;
	WACOMDEVICEINFO* pInfo;
	unsigned int i, j, uCount;

	static TYPEXLAT xTypes[] =
	{
		{ WACOMDEVICETYPE_CURSOR, "cursor" },
		{ WACOMDEVICETYPE_STYLUS, "stylus" },
		{ WACOMDEVICETYPE_ERASER, "eraser" },
		{ WACOMDEVICETYPE_STYLUS, "pad" }
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

static int ListParam(WACOMCONFIG *hConfig, char** argv)
{
	PARAMINFO* p;

	if (*argv != NULL)
		fprintf(stderr,"ListParam: Ignoring extraneous arguments.\n");

	for (p=gParamInfo; p->pszParam; ++p)
		fprintf(stdout,"%-16s - %s\n",
			p->pszParam, p->pszDesc);
	return 0;
}

static int List(WACOMCONFIG *hConfig, char** argv)
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

static int Pack(int nCount, int* pnValues, int* pnResult)
{
	if (nCount == 4)
	{
		*pnResult =
			((pnValues[0] & 0xFF) << 24) |
			((pnValues[1] & 0xFF) << 16) |
			((pnValues[2] & 0xFF) << 8) |
			(pnValues[3] & 0xFF);
		return 0;
	}
	else if (nCount == 0)
	{
		*pnResult = 0x00006464; /* 0 0 100 100 */
		return 0;
	}

	fprintf(stderr,
	"Set: Incorrect number of values for bezier curve.\n"
	"Bezier curve is specified by two control points x0,y0 and x1,y1\n"
	"Ranges are 0 to 100 for each value corresponding to 0.00 to 1.00\n"
	"A straight line would be: 0 0 100 100\n"
	"A slightly depressed line might be: 15 0 100 85\n"
	"A slightly amplified line might be: 0 15 85 100\n");

	return 1;
}

static int Set(WACOMCONFIG * hConfig, char** argv)
{
	PARAMINFO* p;
	int nValues[4];
	WACOMDEVICE * hDev;
	char* a, *pszEnd;
	const char* pszValues[4];
	const char* pszDevName = NULL;
	const char* pszParam = NULL;
	int i, nValue=0, nReturn, nCount = 0;

	while ((a=*argv++) != NULL)
	{
		if (!pszDevName) pszDevName = a;
		else if (!pszParam) pszParam = a;
		else if (nCount < 4) pszValues[nCount++] = a;
		else
		{
			fprintf(stderr,"Set: Unknown argument '%s'\n",a);
			return 1;
		}
	}

	/* No device or param? Error. */
	if (!pszDevName || !pszParam)
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

	/* Set case correctly for error messages below. */
	pszParam = p->pszParam;

	/* If value is empty, do we support this? */
	if (!nCount && !p->bEmptyOK)
	{
		fprintf(stderr,"Set: Value for '%s' must be specified.\n",
			pszParam);
		return 1;
	}

	/* process all the values we received */
	for (i=0; i<nCount; ++i)
	{
		/* Convert value to 32 bit integer; hex OK, octal is not. */
		if (strncasecmp(pszValues[i],"0x",2) == 0)
			nValues[i] = strtol(pszValues[i],&pszEnd,16);
		else
			nValues[i] = strtol(pszValues[i],&pszEnd,10);

		if ((pszEnd == pszValues[i]) || (*pszEnd != '\0'))
		{
			if ((p->nType == BOOLEAN_VALUE) &&
				(!strcasecmp(pszValues[i],"on") ||
				!strcasecmp(pszValues[i],"true") ||
				!strcasecmp(pszValues[i],"absolute")))
					nValues[i] = 1;
			else if ((p->nType == BOOLEAN_VALUE) &&
				(!strcasecmp(pszValues[i],"off") ||
				!strcasecmp(pszValues[i],"false") ||
				!strcasecmp(pszValues[i],"relative")))
					nValues[i] = 0;
			else
			{
				fprintf(stderr,"Set: Value '%s' is "
					"invalid.\n",pszValues[i]);
				return 1;
			}
		}

		/* Is there a range and are we in it? */
		if (p->bRange &&
			((nValues[i] < p->nMin) || (nValues[i]> p->nMax)))
		{
			fprintf(stderr,"Set: Value for '%s' out of range "
				"(%d - %d)\n", pszParam, p->nMin, p->nMax);
			return 1;
		}
	}

	/* Is value count correct? */
	if (p->nType == PACKED_CURVE)
	{
		if (Pack(nCount,nValues,&nValue))
			return 1;
	}
	else if (p->nType == BOOLEAN_VALUE)
	{
		nValue = nCount ? nValues[0] : p->nDefault;
	}
	else if (p->nType == SINGLE_VALUE)
	{
		nValue = nCount ? nValues[0] : p->nDefault;
	}

	/* Looks good, send it. */
	if (gnVerbose)
		printf("Set: sending %d %d (0x%X)\n",p->nParamID,nValue,nValue);

	/* Open device */
	hDev = WacomConfigOpenDevice(hConfig,pszDevName);
	if (!hDev)
	{
		fprintf(stderr,"Set: Failed to open device '%s'\n",
			pszDevName);
		return 1;
	}

	/* Send request */
	nReturn = WacomConfigSetRawParam(hDev,p->nParamID,nValue);

	if (nReturn)
		fprintf(stderr,"Set: Failed to set %s value for '%s'\n",
			pszDevName, pszParam);

	/* Close device and return */
	(void)WacomConfigCloseDevice(hDev);
	return nReturn ? 1 : 0;
}

int DoCommand(COMMAND cmd, char** argv)
{
	int nReturn;
	Display* pDisp;
	WACOMCONFIG * hConf;

	pDisp = XOpenDisplay(gpszDisplayName);
	if (!pDisp)
	{
		fprintf(stderr,"Failed to open display (%s)\n",
			gpszDisplayName ? gpszDisplayName : "");
		return 1;
	}

	XSetErrorHandler(XError);
	XSynchronize(pDisp,1 /*sync on*/);

	hConf = WacomConfigInit(pDisp,Error);
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
