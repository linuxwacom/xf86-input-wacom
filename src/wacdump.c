/*****************************************************************************
** wacdump.c
**
** Copyright (C) 2002 - 2006 - John E. Joganic and Ping Cheng
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
**   2002-12-17 0.3.3 - split ncurses from main file to avoid namespace
**                      collision in linux/input.h
**   2002-12-21 0.3.4 - changed to FILE* to file descriptors
**   2003-01-01 0.3.5 - moved refresh to start display immediately
**   2003-01-25 0.3.6 - moved usb code to wacusb.c
**   2003-01-26 0.3.7 - applied Dean Townsley's Acer C100 patch
**   2003-01-27 0.3.8 - added logging, better error recovery
**   2003-01-31 0.5.0 - new release
**   2003-02-12 0.5.1 - added version option and fixed usage
**   2003-06-19 0.5.2 - added patch for I2 6x8 id 0x47
**   2005-02-17 0.6.6 - added I3 and support 64-bit system
**   2005-09-01 0.7.0 - Added Cintiq 21UX
**   2005-02-17 0.7.1 - added Graphire 4
**   2006-02-27 0.7.3 - added DTF 521, I3 12x12 & 12x19
**
****************************************************************************/

#include "wactablet.h"
#include "wacscrn.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define WACDUMP_VER "0.7.3"

/* from linux/input.h */
#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define BIT(x)  (1UL<<((x)%BITS_PER_LONG))
#define LONG(x) ((x)/BITS_PER_LONG)
#define ISBITSET(x,y) ((x)[LONG(y)] & BIT(y))

static int InitTablet(WACOMTABLET hTablet);
void FetchTablet(WACOMTABLET hTablet);
static void DisplaySerialValue(unsigned int uField);
static void DisplaySerialButton(unsigned int uCode);

struct ABS_STATE
{
	int bValid;
	int nRow, nCol;
	int nValue, nMin, nMax;
};

struct KEY_STATE
{
	int bValid;
	int nRow, nCol;
	int nValue;
};

struct ABS_STATE gAbsState[WACOMFIELD_MAX];
struct KEY_STATE gKeyState[WACOMBUTTON_MAX];
int gnSerialDataRow = 0;
int gbCursesRunning = 0;
FILE* gLogFile = NULL;

void Usage(void)
{
	int i, j, nClsSize, nDevSize;
	unsigned int uDeviceCls;
	WACOMCLASSREC* pCls;
	WACOMDEVICEREC* pDev;

	fprintf(stderr,"Usage: wacdump [options] device\n"
		"Options:\n"
		"  -h, --help               - usage\n"
		"  -c, --class device_cls   - use specified class (see below)\n"
		"  -f, --force device_name  - use specified device (see below)\n"
		"  -l, --list               - list all supported devices\n"
		"  -v, --verbose            - increase log output; multiple OK\n"
		"  -V, --version            - display version number\n"
		"  --logfile log_file       - output log to file\n"
		"\n");
	fprintf(stderr,
		"Example devices:\n"
#ifdef WCM_ENABLE_LINUXINPUT
		"  /dev/input/event0        - usb tablet device\n"
#endif
		"  /dev/ttyS0               - serial tablet on com1\n"
		"  /dev/ttyUSB0             - serial tablet on USB adapter\n"
		"\n"
		"Supported device classes:\n");

	if (!WacomGetSupportedClassList(&pCls,&nClsSize))
	{
		fprintf(stderr,"  ");
		for (i=0; i<nClsSize; ++i)
			fprintf(stderr,"%s%s", i ? ", " : "", pCls[i].pszName);
		fprintf(stderr,"\n");

		fprintf(stderr,"Supported device names:\n");
		for (i=0; i<nClsSize; ++i)
		{
			fprintf(stderr,"  %s: ",pCls[i].pszName);
			uDeviceCls = pCls[i].uDeviceClass;
			if (!WacomGetSupportedDeviceList(uDeviceCls,&pDev,&nDevSize))
			{
				for (j=0; j<nDevSize; ++j)
					fprintf(stderr,"%s%s", j ? ", " : "", pDev[j].pszName);
				fprintf(stderr,"\n");
				WacomFreeList(pDev);
			}
		}
		WacomFreeList(pCls);
	}
}

void Version(void)
{
	fprintf(stdout,"%s\n", WACDUMP_VER);
}

void ListSupportedDevices(void)
{
	int i, nListSize;
	WACOMDEVICEREC* pList;

	if (!WacomGetSupportedDeviceList(0,&pList,&nListSize))
	{
		for (i=0; i<nListSize; ++i)
		{
			fprintf(stdout,"%s\t%s\t%s\t\"%s\"\n",
				pList[i].pszName,
				pList[i].pszVendorName,
				pList[i].pszClass,
				pList[i].pszDesc);
		}
		WacomFreeList(pList);
	}
}

static void termscr(void)
{
	wacscrn_term();
}

static int InitTablet(WACOMTABLET hTablet)
{
	int i, nCaps, nItem, nRow=0;
	int nMajor, nMinor, nRelease;
	char chBuf[256];
	WACOMMODEL model;
	WACOMSTATE ranges = WACOMSTATE_INIT;
	const char* pszName;
	const char* pszClass = "UNK_CLS";
	const char* pszVendor = "UNK_VNDR";
	const char* pszDevice = "UNK_DEV";
	const char* pszSub = "UNK_SUB";

	/* Identify program and version */
	wacscrn_standout();
	for (i=0; i<80; ++i) wacscrn_output(nRow,i," ");
	wacscrn_output(nRow,0,"wacdump v" WACDUMP_VER);
	wacscrn_normal();
	nRow += 2;

	/* get model */
	model = WacomGetModel(hTablet);
	pszVendor = WacomGetVendorName(hTablet);
	pszClass = WacomGetClassName(hTablet);
	pszDevice = WacomGetDeviceName(hTablet);
	pszSub = WacomGetSubTypeName(hTablet);
	pszName = WacomGetModelName(hTablet);
	WacomGetROMVersion(hTablet,&nMajor,&nMinor,&nRelease);
	snprintf(chBuf,sizeof(chBuf),"MODEL=%s",pszName);
	wacscrn_output(nRow,0,chBuf);
	snprintf(chBuf,sizeof(chBuf),"ROM=%d.%d-%d",nMajor, nMinor, nRelease);
	wacscrn_output(nRow,40,chBuf);
	++nRow;
	snprintf(chBuf,sizeof(chBuf),"CLS=%s  VNDR=%s  DEV=%s  SUB=%s",
			pszClass, pszVendor, pszDevice, pszSub);
	wacscrn_output(nRow,0,chBuf);
	nRow += 2;

	gnSerialDataRow = nRow++; /* data */
	nRow += 2;

	/* get event types supported, ranges, and immediate values */
	nCaps = WacomGetCapabilities(hTablet);
	WacomGetState(hTablet,&ranges);

	nItem = 0;
	for (i=0; i<31; ++i)
	{
		if (nCaps & (1<<i))
		{
			gAbsState[i].bValid = 1;
			gAbsState[i].nValue = ranges.values[i].nValue;
			gAbsState[i].nMin = ranges.values[i].nMin;
			gAbsState[i].nMax = ranges.values[i].nMax;
			gAbsState[i].nRow = nRow + nItem / 2;
			gAbsState[i].nCol = nItem % 2;
			DisplaySerialValue(i);
			++nItem;
		}
	}
	nRow += ((nItem + 1) / 2) + 1;

	/* get key event types */
	nItem = 0;
	for (i=0; i<WACOMBUTTON_MAX; ++i)
	{
		gKeyState[i].bValid = 1;
		gKeyState[i].nRow = nRow + nItem / 4;
		gKeyState[i].nCol = nItem % 4;
		DisplaySerialButton(i);
		++nItem;
	}
	nRow += ((nItem + 3) / 4) + 1;
	return 0;
}

const char* GetSerialField(unsigned int uField)
{
	static const char* xszField[WACOMFIELD_MAX] =
	{
		"TOOLTYPE", "SERIAL", "IN_PROX", "BUTTON",
		"POS_X", "POS_Y", "ROT_Z", "DISTANCE",
		"PRESSURE", "TILT_X", "TILT_Y", "ABSWHEEL",
		"RELWHEEL", "THROTTLE"
	};

	return (uField >= WACOMFIELD_MAX) ?  "FIELD?" : xszField[uField];
}

static void DisplaySerialValue(unsigned int uField)
{
	static char xchBuf[256];
	static const char* xszTool[WACOMTOOLTYPE_MAX] =
	{
		"NONE", "PEN", "PENCIL", "BRUSH", "ERASER", "AIRBRUSH",
		"MOUSE", "LENS", "PAD"
	};

	int bBold = 0;
	if ((uField >= WACOMFIELD_MAX) || !gAbsState[uField].bValid)
	{
		snprintf(xchBuf,sizeof(xchBuf),"Bad FIELD Code: 0x%X",uField);
		wacscrn_output(24,0,xchBuf);
		return;
	}

	snprintf(xchBuf,sizeof(xchBuf),"%8s=",GetSerialField(uField));
	wacscrn_output(gAbsState[uField].nRow,gAbsState[uField].nCol*40,xchBuf);

	if (uField == WACOMFIELD_TOOLTYPE)
	{
		int nToolType = gAbsState[uField].nValue;
		if ((nToolType<WACOMTOOLTYPE_NONE) || (nToolType>WACOMTOOLTYPE_MAX))
			snprintf(xchBuf,sizeof(xchBuf),"UNKNOWN");
		else
			snprintf(xchBuf,sizeof(xchBuf),"%-10s",
					xszTool[nToolType]);
		bBold = (nToolType != WACOMTOOLTYPE_NONE);
	}
	else if (uField == WACOMFIELD_SERIAL)
	{
		snprintf(xchBuf,sizeof(xchBuf),"0x%08X",gAbsState[uField].nValue);
	}
	else if (uField == WACOMFIELD_PROXIMITY)
	{
		snprintf(xchBuf,sizeof(xchBuf),"%-3s",
				gAbsState[uField].nValue ? "in" : "out");
		bBold = gAbsState[uField].nValue;
	}
	else
	{
		/* deal with tool in prox before wacdump launched */
		if (gAbsState[uField].nValue && !gAbsState[WACOMFIELD_PROXIMITY].nValue)
		{
			gAbsState[WACOMFIELD_PROXIMITY].nValue |= BIT(WACOMTOOLTYPE_PEN);
			gAbsState[WACOMFIELD_TOOLTYPE].nValue = WACOMTOOLTYPE_PEN;
			wacscrn_standout();
			snprintf(xchBuf,sizeof(xchBuf),"%-3s","in");
			wacscrn_output(gAbsState[WACOMFIELD_PROXIMITY].
					nRow,gAbsState[WACOMFIELD_PROXIMITY].nCol*40+9,xchBuf);
			wacscrn_normal();
			wacscrn_standout();
			snprintf(xchBuf,sizeof(xchBuf),"%-10s",xszTool[WACOMTOOLTYPE_PEN]);
			wacscrn_output(gAbsState[WACOMFIELD_TOOLTYPE].
					nRow,gAbsState[WACOMFIELD_TOOLTYPE].nCol*40+9,xchBuf);
			wacscrn_normal();
		}

		snprintf(xchBuf,sizeof(xchBuf),"%+06d (%+06d .. %+06d)",
			gAbsState[uField].nValue,
			gAbsState[uField].nMin,
			gAbsState[uField].nMax);
	}

	if (bBold) wacscrn_standout();
	wacscrn_output(gAbsState[uField].nRow,gAbsState[uField].nCol*40+9,xchBuf);
	if (bBold) wacscrn_normal();
}

const char* GetSerialButton(unsigned int uButton)
{
	static const char* xszButton[WACOMBUTTON_MAX] =
	{
		"LEFT", "MIDDLE", "RIGHT", "EXTRA", "SIDE",
		"TOUCH", "STYLUS", "STYLUS2", "BT0", "BT1",
		"BT2", "BT3", "BT4", "BT5", "BT6", "BT7",
		"BT8", "BT9", "BT10", "BT11", "BT12", "BT13",
		"BT14", "BT15", "BT16", "BT17", "BT18", "BT19",
		"BT20", "BT21", "BT22", "BT23"
	};

	return (uButton >= WACOMBUTTON_MAX) ?  "FIELD?" : xszButton[uButton];
}

static void DisplaySerialButton(unsigned int uButton)
{
	int bDown;
	static char xchBuf[256];
	if ((uButton >= WACOMBUTTON_MAX) || !gKeyState[uButton].bValid)
	{
		snprintf(xchBuf,sizeof(xchBuf),"Bad Button Code: 0x%X",uButton);
		wacscrn_output(24,0,xchBuf);
		return;
	}

	bDown = gKeyState[uButton].nValue ? 1 : 0;

	snprintf(xchBuf,sizeof(xchBuf),"%8s=%s",
		GetSerialButton(uButton), bDown ? "DOWN" : "    ");

	if (bDown) wacscrn_standout();
	wacscrn_output(gKeyState[uButton].nRow,gKeyState[uButton].nCol*20,xchBuf);
	wacscrn_normal();
}

void FetchTablet(WACOMTABLET hTablet)
{
	char chOut[128];
	unsigned char uchBuf[64];
	int i, nLength, nRow=gnSerialDataRow, nErrors=0;
	WACOMSTATE state = WACOMSTATE_INIT;

	while (1)
	{
		wacscrn_refresh();

		nLength = WacomReadRaw(hTablet,uchBuf,sizeof(uchBuf));
		if (nLength < 0)
		{
			snprintf(chOut,sizeof(chOut),"ReadRaw: %10d (%d)",
					nLength,++nErrors);
			wacscrn_output(22,0,chOut);
			wacscrn_refresh();
			sleep(1);
			continue;
		}

		for (i=0; i<nLength; ++i)
		{
			snprintf(chOut,sizeof(chOut),"%02X",uchBuf[i]);
			wacscrn_output(nRow,i*3,chOut);
		}
		for (i=0; i<nLength; ++i)
		{
			snprintf(chOut,sizeof(chOut),"%c",isprint(uchBuf[i]) ?
					uchBuf[i] : '.');
			wacscrn_output(nRow,60 + i,chOut);
		}

		if ((i=WacomParseData(hTablet,uchBuf,nLength,&state)))
		{
			snprintf(chOut,sizeof(chOut),
				"ParseData: %10d %-40s (%d)",
				i,strerror(errno),++nErrors);
			wacscrn_output(23,0,chOut);
			wacscrn_refresh();
			continue;
		}

		for (i=WACOMFIELD_TOOLTYPE; i<WACOMFIELD_MAX; ++i)
		{
			if (state.uValid & BIT(i))
			{
				if (i == WACOMFIELD_RELWHEEL)
					gAbsState[i].nValue += state.values[i].nValue;
				else
					gAbsState[i].nValue = state.values[i].nValue;
				DisplaySerialValue(i);
			}
		}

		for (i=WACOMBUTTON_LEFT; i<WACOMBUTTON_MAX; ++i)
		{
			gKeyState[i].nValue = state.values[WACOMFIELD_BUTTONS].nValue &
					(1 << i) ? 1 : 0;
			DisplaySerialButton(i);
		}
	}
}

void LogCallback(struct timeval tv, WACOMLOGLEVEL level, const char* psz)
{
	int i;
	struct tm* pTM;

	static const char* xszLog[WACOMLOGLEVEL_MAX] =
	{
		"", "CRITICAL", "ERROR", "WARN",
		"INFO", "DEBUG", "TRACE"
	};

	if ((level < WACOMLOGLEVEL_CRITICAL) || (level >= WACOMLOGLEVEL_MAX))
		level = WACOMLOGLEVEL_NONE;

	/* get the time */
	pTM = localtime((time_t*)&tv.tv_sec);

	if (gbCursesRunning)
	{
		static char chBuf[1024];
		snprintf(chBuf,sizeof(chBuf),"%02d:%02d:%02d.%03d %s: %s",
			pTM->tm_hour,pTM->tm_min,pTM->tm_min,
			(int)(tv.tv_usec / 1000),xszLog[level],psz);
		for (i=0; i<78; ++i) wacscrn_output(24,i," ");
		wacscrn_output(24,0,chBuf);
		wacscrn_refresh();
	}
	else
	{
		fprintf(stderr,"%02d:%02d:%02d.%03d %s: %s\n",
			pTM->tm_hour,pTM->tm_min,pTM->tm_min,
			(int)(tv.tv_usec / 1000), xszLog[level], psz);
	}

	if (gLogFile)
	{
		fprintf(gLogFile,"%02d:%02d:%02d.%03d %s: %s\n",
			pTM->tm_hour,pTM->tm_min,pTM->tm_min,
			(int)(tv.tv_usec / 1000), xszLog[level], psz);
		fflush(gLogFile);
	}
}

int main(int argc, char** argv)
{
	const char* pszFile = NULL;
	const char* arg;
	WACOMENGINE hEngine = NULL;
	WACOMTABLET hTablet = NULL;
	int nLevel = (int)WACOMLOGLEVEL_WARN;
	const char* pszDeviceCls = NULL;
	const char* pszDeviceType = NULL;
	const char* pszLogFile = NULL;
	WACOMMODEL model = { 0 };

	/* parse args */
	++argv;
	while (*argv)
	{
		arg = *(argv++);

		/* handle options */
		if (arg[0] == '-')
		{
			/* information */
			if ((strcmp(arg,"-h") == 0) || (strcmp(arg,"--help")==0))
				{ Usage(); exit(0); }
			if ((strcmp(arg,"-V") == 0) || (strcmp(arg,"--version")==0))
				{ Version(); exit(0); }
			else if ((strcmp(arg,"-l") == 0) || (strcmp(arg,"--list") == 0))
				{ ListSupportedDevices(); exit(0); }

			/* force device type */
			else if ((strcmp(arg,"-f") == 0) || (strcmp(arg,"--force") == 0))
			{
				arg = *(argv++);
				if (arg == NULL)
					{ fprintf(stderr,"Missing device\n"); exit(1); }
				pszDeviceType = arg;
			}

			/* serial and usb class overrides */
			else if ((strcmp(arg,"-c") == 0) || (strcmp(arg,"--class") == 0))
			{
				arg = *(argv++);
				if (arg == NULL)
					{ fprintf(stderr,"Missing class\n"); exit(1); }
				pszDeviceCls = arg;
			}

			/* log file */
			else if (strcmp(arg,"--logfile") == 0)
			{
				arg = *(argv++);
				if (arg == NULL)
					{ fprintf(stderr,"Missing log file\n"); exit(1); }
				pszLogFile = arg;
			}
			else if ((strcmp(arg,"-v") == 0) || (strcmp(arg,"--verbose") == 0))
			{
				++nLevel;
			}

			/* unknown options */
			else
				{ fprintf(stderr,"Unknown option %s\n",arg); exit(1); }
		}

		/* device name */
		else
		{
			if (pszFile)
				{ fprintf(stderr,"Extra argument %s\n",arg); exit(1); }

			pszFile = arg;
		}
	}

	/* check for device */
	if (!pszFile)
	{
		Usage();
		exit(1);
	}

	/* check for log file */
	if (pszLogFile)
	{
		gLogFile = fopen(pszLogFile,"w");
		if (!gLogFile)
		{
			perror("failed to open log file for writing");
			exit(1);
		}
	}

	/* set device class, if provided */
	if (pszDeviceCls)
	{
		model.uClass = WacomGetClassFromName(pszDeviceCls);
		if (!model.uClass)
		{
			fprintf(stderr, "Unrecognized class: %s\n",pszDeviceCls);
			exit(1);
		}
	}

	/* set device type, if provided */
	if (pszDeviceType)
	{
		model.uDevice = WacomGetDeviceFromName(pszDeviceType,model.uClass);
		if (!model.uDevice)
		{
			fprintf(stderr, "Unrecognized device: %s\n",pszDeviceType);
			exit(1);
		}
	}

	/* open engine */
	hEngine = WacomInitEngine();
	if (!hEngine)
	{
		perror("failed to open tablet engine");
		exit(1);
	}

	WacomSetLogFunc(hEngine,LogCallback);
	WacomSetLogLevel(hEngine,(WACOMLOGLEVEL)nLevel);

	WacomLog(hEngine,WACOMLOGLEVEL_INFO,"Opening log");
   
	/* open tablet */
	hTablet = WacomOpenTablet(hEngine,pszFile,&model);
	if (!hTablet)
	{
		perror("WacomOpenTablet");
		exit(1);
	}

	/* begin curses window mode */
	wacscrn_init();
	atexit(termscr);

	gbCursesRunning = 1;

	/* get device capabilities, build screen */
	if (InitTablet(hTablet))
	{
		perror("InitTablet");
		WacomCloseTablet(hTablet);
		exit(1);
	}

	/* read, parse, and display */
	FetchTablet(hTablet);

	/* close device */
	WacomCloseTablet(hTablet);
	WacomTermEngine(hEngine);

	/* close log file, if open */
	if (gLogFile)
		fclose(gLogFile);

	return 0;
}
