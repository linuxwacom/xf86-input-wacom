/*****************************************************************************
** wacdump.c
**
** Copyright (C) 2002 - John E. Joganic
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
**   2003-01-25 0.3.6 - moved usb code to wacusb.c
**   2003-01-01 0.3.5 - moved refresh to start display immediately
**   2002-12-21 0.3.4 - changed to FILE* to file descriptors
**   2002-12-17 0.3.3 - split ncurses from main file to avoid namespace
**                      collision in linux/input.h
**
****************************************************************************/

#include "wactablet.h"
#include "wacscrn.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define WACDUMP_VER "wacdump v0.3.6"

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

struct REL_STATE
{
	int bValid;
	int nRow, nCol;
	int nValue;
};

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

struct SERIAL_STATE
{
	int nRow;
	int nSerial;
};

	struct ABS_STATE gAbsState[WACOMFIELD_MAX];
	struct KEY_STATE gKeyState[WACOMBUTTON_MAX];
	struct SERIAL_STATE gSerialState;
	int gnSerialDataRow = 0;

void Usage(void)
{
	fprintf(stderr,"Usage: wacdump [-d device]\n"
		"  -?, -h, --help       - usage\n"
		"  -d, --device device  - use specified device\n");
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

	/* Identify program and version */
	wacscrn_standout();
	for (i=0; i<80; ++i) wacscrn_output(nRow,i," ");
	wacscrn_output(nRow,0,WACDUMP_VER);
	wacscrn_normal();
	nRow += 2;

	/* get model */
	model = WacomGetModel(hTablet);

	/* vendor */
	switch (WACOMVENDOR(model))
	{
		case WACOMVENDOR_WACOM: pszVendor = "WACOM"; break;
	}

	/* class */
	switch (WACOMCLASS(model))
	{
		case WACOMCLASS_SERIAL: pszClass = "SERIAL"; break;
		case WACOMCLASS_USB: pszClass = "USB"; break;
	}

	/* device */
	switch (WACOMDEVICE(model))
	{
		case WACOMDEVICE_ARTPAD: pszDevice = "ARTPAD"; break;
		case WACOMDEVICE_ARTPADII: pszDevice = "ARTPADII"; break;
		case WACOMDEVICE_DIGITIZER: pszDevice = "DIGITIZER"; break;
		case WACOMDEVICE_DIGITIZERII: pszDevice = "DIGITIZERII"; break;
		case WACOMDEVICE_PENPARTNER: pszDevice = "PENPARTNER"; break;
		case WACOMDEVICE_GRAPHIRE: pszDevice = "GRAPHIRE"; break;
		case WACOMDEVICE_GRAPHIRE2: pszDevice = "GRAPHIRE2"; break;
		case WACOMDEVICE_INTUOS: pszDevice = "INTUOS"; break;
		case WACOMDEVICE_INTUOS2: pszDevice = "INTUOS2"; break;
		case WACOMDEVICE_CINTIQ: pszDevice = "CINTIQ"; break;
		case WACOMDEVICE_VOLITO: pszDevice = "VOLITO"; break;
	}

	/* get name, rom version */
	pszName = WacomGetModelName(hTablet);
	WacomGetROMVersion(hTablet,&nMajor,&nMinor,&nRelease);
	wacscrn_output(nRow,0,pszName);

	snprintf(chBuf,sizeof(chBuf),"%s %s %s 0x%X ROM=%d.%d-%d",
			pszVendor, pszClass, pszDevice, model, nMajor, nMinor, nRelease);
	wacscrn_output(nRow,30,chBuf);
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
		"MOUSE", "LENS"
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
	else if (uField == WACOMFIELD_SERIAL)
	{
		snprintf(xchBuf,sizeof(xchBuf),"%-3s",
				gAbsState[uField].nValue ? "in" : "out");
		bBold = gAbsState[uField].nValue;
	}
	else
	{
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
		"TOUCH", "STYLUS", "STYLUS2"
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
	unsigned char uchBuf[16];
	int i, nLength, nRow=gnSerialDataRow;
	WACOMSTATE state = WACOMSTATE_INIT;

	while (1)
	{
		wacscrn_refresh();

		nLength = WacomReadRaw(hTablet,uchBuf,sizeof(uchBuf));
		if (nLength < 0)
		{
			snprintf(chOut,sizeof(chOut),"ReadRaw returned %d",nLength);
			wacscrn_output(23,0,chOut);
			wacscrn_refresh();
			break;
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

		if (WacomParseData(hTablet,uchBuf,nLength,&state))
		{
			snprintf(chOut,sizeof(chOut),"ParseData returned error %s %d",
				strerror(errno),errno);
			wacscrn_output(23,0,chOut);
			wacscrn_refresh();
			break;
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

int main(int argc, char** argv)
{
	const char* pszFile = NULL;
	const char* arg;
	WACOMTABLET hTablet = NULL;

	/* parse args */
	++argv;
	while (*argv)
	{
		arg = *(argv++);
		if ((strcmp(arg,"-?") == 0) || (strcmp(arg,"-h") == 0) ||
			(strcmp(arg,"--help")==0))
		{
			Usage();
			exit(0);
		}
		if ((strcmp(arg,"-d") == 0) || (strcmp(arg,"--device") == 0))
		{
			arg = *(argv++);
			if (arg == NULL) { fprintf(stderr,"Missing device\n"); exit(1); }
			pszFile = arg;
		}
		else
		{
			fprintf(stderr,"Unknown argument %s\n",arg);
			exit(1);
		}
	}

	/* default device if not specified */
	if (!pszFile)
	{
		fprintf(stderr,"Please specify a device using -d parameter. Examples:\n"
			"  wacdump -d /dev/input/event0 - usb tablet device\n"
			"  wacdump -d /dev/ttyS0        - serial tablet on com1\n"
			"  wacdump -d /dev/ttyUSB0      - serial tablet on USB adapter\n");
		exit(1);
	}
   
	/* open tablet */
	hTablet = WacomOpenTablet(pszFile);
	if (!hTablet)
	{
		fprintf(stderr,"failed to open %s for read/writing: %s\n",
			pszFile, strerror(errno));
		exit(1);
	}

	/* begin curses window mode */
	wacscrn_init();
	atexit(termscr);

	/* get device capabilities, build screen */
	if (InitTablet(hTablet))
	{
		WacomCloseTablet(hTablet);
		fprintf(stderr,"failed to initialize input\n");
		exit(1);
	}

	/* read, parse, and display */
	FetchTablet(hTablet);

	/* close device */
	WacomCloseTablet(hTablet);

	return 0;
}

