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
**   2002-12-21 0.3.5 - moved refresh to start display immediately
**   2002-12-21 0.3.4 - changed to FILE* to file descriptors
**   2002-12-17 0.3.3 - split ncurses from main file to avoid namespace
**                      collision in linux/input.h
**
****************************************************************************/

#include "wacscrn.h"
#include "wacserial.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <assert.h>

#define WACDUMP_VER "wacdump v0.3.5"

/* from linux/input.h */
#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define BIT(x)  (1UL<<((x)%BITS_PER_LONG))
#define LONG(x) ((x)/BITS_PER_LONG)

static int InitUSB(int fd);
static void FetchUSB(int fd);
static int InitSerial(WACOMTABLET hTablet);
void FetchSerial(WACOMTABLET hTablet);
static const char* GetTypeCode(unsigned short wCode);
static const char* GetRelCode(unsigned short wCode);
static const char* GetAbsCode(unsigned short wCode);
static const char* GetKeyCode(unsigned short wCode);
static void DisplayEv(unsigned short wCode);
static void DisplayRel(unsigned short wCode);
static void DisplayAbs(unsigned short wCode);
static void DisplayKey(unsigned short wCode);
static void DisplayToolSerial(void);

static void DisplaySerialValue(unsigned int uField);
static void DisplaySerialButton(unsigned int uCode);

struct EV_STATE
{
	int bValid;
	int nRow;
	struct input_event ev;
};

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

	struct EV_STATE gEvState[EV_MAX];
	struct ABS_STATE gRelState[REL_MAX];
	struct ABS_STATE gAbsState[ABS_MAX];
	struct KEY_STATE gKeyState[KEY_MAX];
	struct SERIAL_STATE gSerialState;

static int InitUSB(int fd)
{
	int nCnt, nItem, nRow=0;
	unsigned short i;
	char chBuf[256];
	short sID[4];
	int nAbs[5];
	unsigned long evbits[NBITS(EV_MAX)];
	unsigned long absbits[NBITS(ABS_MAX)];
	unsigned long relbits[NBITS(REL_MAX)];
	unsigned long keybits[NBITS(KEY_MAX)];

	/* Identify program and version */
	wacscrn_standout();
	for (i=0; i<80; ++i) wacscrn_output(nRow,i," ");
	wacscrn_output(nRow,0,WACDUMP_VER);
	wacscrn_normal();
	nRow += 2;

	/* Get device name and id */
	if (ioctl(fd,EVIOCGNAME(sizeof(chBuf)),chBuf) < 0)
		strncpy(chBuf,"Unknown Device Name",sizeof(chBuf));
	wacscrn_output(nRow,0,chBuf);
	if (ioctl(fd,EVIOCGID,sID) < 0)
		strncpy(chBuf,"No ID",sizeof(chBuf));
	else
		snprintf(chBuf,sizeof(chBuf),"bus=%X, vndr=%X, prd=%X, ver=%X",
			sID[0], sID[1], sID[2], sID[3]);
	wacscrn_output(nRow,40,chBuf);
	nRow += 2;

	/* Get event types supported */
	nItem = 0;
	memset(gEvState,0,sizeof(gEvState));
	nCnt = ioctl(fd,EVIOCGBIT(0 /*EV*/,sizeof(evbits)),evbits);
	if (nCnt < 0) { perror("Failed to CGBIT"); return 1; }
	assert(nCnt == sizeof(evbits));
	for (i=0; i<EV_MAX; ++i)
	{
		if (evbits[LONG(i)] & BIT(i))
		{
			gEvState[i].bValid = 1;
			gEvState[i].nRow = nRow + nItem;
			DisplayEv(i);
			++nItem;
		}
	}
	nRow += nItem + 1;

	/* get absolute event types supported, ranges, and immediate values */
	nItem = 0;
	memset(gAbsState,0,sizeof(gAbsState));
	if (evbits[LONG(EV_ABS)] & BIT(EV_ABS))
	{
		nCnt = ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(absbits)),absbits);
		if (nCnt < 0) { perror("Failed to CGBIT"); return 1; }
		assert(nCnt == sizeof(absbits));
		for (i=0; i<ABS_MAX; ++i)
		{
			if (absbits[LONG(i)] & BIT(i))
			{
				ioctl(fd, EVIOCGABS(i), nAbs);
				gAbsState[i].bValid = 1;
				gAbsState[i].nValue = nAbs[0];
				gAbsState[i].nMin = nAbs[1];
				gAbsState[i].nMax = nAbs[2];
				gAbsState[i].nRow = nRow + nItem / 2;
				gAbsState[i].nCol = nItem % 2;
				DisplayAbs(i);
				++nItem;
			}
		}
	}
	nRow += ((nItem + 1) / 2) + 1;

	/* get relative event types supported, ranges, and immediate values */
	nItem = 0;
	memset(gRelState,0,sizeof(gRelState));
	if (evbits[LONG(EV_REL)] & BIT(EV_REL))
	{
		nCnt = ioctl(fd,EVIOCGBIT(EV_REL,sizeof(relbits)),relbits);
		if (nCnt < 0) { perror("Failed to CGBIT"); return 1; }
		assert(nCnt == sizeof(relbits));
		for (i=0; i<REL_MAX; ++i)
		{
			if (relbits[LONG(i)] & BIT(i))
			{
				gRelState[i].bValid = 1;
				gRelState[i].nValue = 0; /* start at zero */
				gRelState[i].nRow = nRow + nItem / 4;
				gRelState[i].nCol = nItem % 4;
				DisplayRel(i);
				++nItem;
			}
		}
	}
	nRow += ((nItem + 1) / 4) + 1;

	/* Get serial */
	gSerialState.nRow = nRow;
	gSerialState.nSerial = 0;
	nRow += 2;

	/* get key event types supported */
	nItem = 0;
	memset(gKeyState,0,sizeof(gKeyState));
	if (evbits[LONG(EV_KEY)] & BIT(EV_KEY))
	{
		nCnt = ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(keybits)),keybits);
		if (nCnt < 0) { perror("Failed to CGBIT"); return 1; }
		assert(nCnt == sizeof(keybits));
		for (i=0; i<KEY_MAX; ++i)
		{
			if (keybits[LONG(i)] & BIT(i))
			{
				/* fetching key state not possible :( */
				gKeyState[i].bValid = 1;
				gKeyState[i].nRow = nRow + nItem / 4;
				gKeyState[i].nCol = nItem % 4;
				DisplayKey(i);
				++nItem;
			}
		}
	}
	nRow += ((nItem + 3) / 4) + 1;
	return 0;
}

void DisplayEv(unsigned short wCode)
{
	static char xchBuf[256];
	if ((wCode >= EV_MAX) || !gEvState[wCode].bValid)
	{
		snprintf(xchBuf,sizeof(xchBuf),"Bad EV Code: 0x%X",wCode);
		wacscrn_output(23,0,xchBuf);
		return;
	}

	snprintf(xchBuf,sizeof(xchBuf),"%4s=%08lX.%08lX %04X %04X %08X",
			GetTypeCode(wCode),
			gEvState[wCode].ev.time.tv_sec,
			gEvState[wCode].ev.time.tv_usec,
			gEvState[wCode].ev.type,
			gEvState[wCode].ev.code,
			gEvState[wCode].ev.value);
	wacscrn_output(gEvState[wCode].nRow,0,xchBuf);
}


void DisplayRel(unsigned short wCode)
{
	static char xchBuf[256];
	if ((wCode >= REL_MAX) || !gRelState[wCode].bValid)
	{
		snprintf(xchBuf,sizeof(xchBuf),"Bad REL Code: 0x%X",wCode);
		wacscrn_output(24,0,xchBuf);
		return;
	}

	snprintf(xchBuf,sizeof(xchBuf),"%8s=%+06d", 
		GetRelCode(wCode), gRelState[wCode].nValue);

	wacscrn_output(gRelState[wCode].nRow,gRelState[wCode].nCol*20,xchBuf);
}

void DisplayAbs(unsigned short wCode)
{
	static char xchBuf[256];
	if ((wCode >= ABS_MAX) || !gAbsState[wCode].bValid)
	{
		snprintf(xchBuf,sizeof(xchBuf),"Bad ABS Code: 0x%X",wCode);
		wacscrn_output(24,0,xchBuf);
		return;
	}

	snprintf(xchBuf,sizeof(xchBuf),"%8s=%+06d (%+06d .. %+06d)",
		GetAbsCode(wCode),
		gAbsState[wCode].nValue,
		gAbsState[wCode].nMin,
		gAbsState[wCode].nMax);
	wacscrn_output(gAbsState[wCode].nRow,gAbsState[wCode].nCol*40,xchBuf);
}

static void DisplayToolSerial(void)
{
	static char xchBuf[256];
	snprintf(xchBuf,sizeof(xchBuf),"  SERIAL=%08X",gSerialState.nSerial);
	wacscrn_output(gSerialState.nRow,0,xchBuf);
}

static void DisplayKey(unsigned short wCode)
{
	int bDown;
	static char xchBuf[256];
	if ((wCode >= KEY_MAX) || !gKeyState[wCode].bValid)
	{
		snprintf(xchBuf,sizeof(xchBuf),"Bad Key Code: 0x%X",wCode);
		wacscrn_output(24,0,xchBuf);
		return;
	}

	bDown = gKeyState[wCode].nValue ? 1 : 0;

	snprintf(xchBuf,sizeof(xchBuf),"%8s=%s",
		GetKeyCode(wCode), bDown ? "DOWN" : "    ");

	if (bDown) wacscrn_standout();
	wacscrn_output(gKeyState[wCode].nRow,gKeyState[wCode].nCol*20,xchBuf);
	wacscrn_normal();
}

static const char* GetTypeCode(unsigned short wCode)
{
	static const char* xszEv[] =
	{
		"RST", "KEY", "REL", "ABS", "MSC"
	};

	return (wCode >= (sizeof(xszEv)/sizeof(*xszEv))) ?
		"EV?" : xszEv[wCode];
}

static const char* GetRelCode(unsigned short wCode)
{
	static const char* xszRel[] =
	{
		"X", "Y", "Z", "0x03", "0x04", "0x05", "HWHEEL",
		"DIAL", "WHEEL", "MISC"
	};

	return (wCode > REL_MISC) ?  "KEY?" : xszRel[wCode];
}

static const char* GetAbsCode(unsigned short wCode)
{
	static const char* xszAbs[] =
	{
		"X", "Y", "Z", "RX", "RY", "RZ",
		"THROTTLE", "RUDDER", "WHEEL", "GAS", "BRAKE",
		"0x0B", "0x0C", "0x0D", "0x0E", "0x0F",
		"HAT0X", "HAT0Y", "HAT1X", "HAT1Y",
		"HAT2X", "HAT2Y", "HAY3X", "HAT3Y",
		"PRESSURE", "DISTANCE", "TILT_X", "TILT_Y",
		"MISC"
	};

	return (wCode > ABS_MISC) ?  "KEY?" : xszAbs[wCode];
}

static const char* GetKeyCode(unsigned short wCode)
{
	static const char* xszMouseKey[] =
	{
		"LEFT", "RIGHT", "MIDDLE", "SIDE", "EXTRA", "FORWARD",
		"BACK"
	};

	static const char* xszToolKey[] =
	{
		"PEN", "RUBBER", "BRUSH", "PENCIL", "AIR",
		"FINGER", "MOUSE", "LENS",
		"0x148", "0x149", "TOUCH", "STYLUS", "STYLUS2"
	};

	static char xchBuf[16];

	if ((wCode >= BTN_LEFT) && (wCode <= BTN_BACK))
		return xszMouseKey[wCode - BTN_LEFT];

	if ((wCode >= BTN_TOOL_PEN) && (wCode <= BTN_STYLUS2))
		return xszToolKey[wCode - BTN_TOOL_PEN];

	snprintf(xchBuf,sizeof(xchBuf),"K_%X",wCode);
	return xchBuf;
}

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

static void FetchUSB(int fd)
{
	int nCnt, nXfer;
	struct input_event ev;

	while (1)
	{
		wacscrn_refresh();

		/* read one whole event record */
		nCnt=0;
		while (nCnt < sizeof(ev))
		{
			nXfer = read(fd,((char*)&ev)+nCnt,sizeof(ev)-nCnt);
			if (nXfer == 0) break;
			nCnt += nXfer;
		}

		if (ev.type < EV_MAX)
		{
			gEvState[ev.type].ev = ev;
			DisplayEv(ev.type);
		}

		if (ev.type == EV_MSC)
		{
			gSerialState.nSerial = ev.value;
			DisplayToolSerial();
		}
		else if ((ev.type == EV_KEY) && (ev.code < KEY_MAX))
		{
			gKeyState[ev.code].nValue = ev.value;
			DisplayKey(ev.code);
		}
		else if ((ev.type == EV_ABS) && (ev.code < ABS_MAX))
		{
			gAbsState[ev.code].nValue = ev.value;
			DisplayAbs(ev.code);
		}
		else if ((ev.type == EV_REL) && (ev.code < REL_MAX))
		{
			gRelState[ev.code].nValue += ev.value;
			DisplayRel(ev.code);
		}
	}
}

static int InitSerial(WACOMTABLET hTablet)
{
	int i, nCaps, nItem, nRow=0;
	int nMajor, nMinor, nRelease;
	char chBuf[256];
	WACOMMODEL model;
	WACOMSTATE min, max;
	const char* pszName;

	/* Identify program and version */
	wacscrn_standout();
	for (i=0; i<80; ++i) wacscrn_output(nRow,i," ");
	wacscrn_output(nRow,0,WACDUMP_VER);
	wacscrn_normal();
	nRow += 2;

	/* Get device name, ROM, and model */
	model = WacomGetModel(hTablet);
	pszName = WacomGetModelName(hTablet);
	WacomGetRomVersion(hTablet,&nMajor,&nMinor,&nRelease);
	wacscrn_output(nRow,0,pszName);
	snprintf(chBuf,sizeof(chBuf),"SERIAL model=0x%X ROM=%d.%d-%d",
			model, nMajor, nMinor, nRelease);
	wacscrn_output(nRow,40,chBuf);
	nRow += 2;

	gEvState[0].nRow = nRow++; /* data */
	nRow += 2;

	/* get event types supported, ranges, and immediate values */
	nCaps = WacomGetCapabilities(hTablet);
	WacomGetRanges(hTablet,&min,&max);

	nItem = 0;
	for (i=0; i<31; ++i)
	{
		if (nCaps & (1<<i))
		{
			gAbsState[i].bValid = 1;
			gAbsState[i].nValue = 0;
			gAbsState[i].nMin = ((int*)(&min))[i + 1];
			gAbsState[i].nMax = ((int*)(&max))[i + 1];
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
	if ((uField >= WACOMFIELD_MAX) || !gAbsState[uField].bValid)
	{
		snprintf(xchBuf,sizeof(xchBuf),"Bad FIELD Code: 0x%X",uField);
		wacscrn_output(24,0,xchBuf);
		return;
	}

	snprintf(xchBuf,sizeof(xchBuf),"%8s=%+06d (%+06d .. %+06d)",
		GetSerialField(uField),
		gAbsState[uField].nValue,
		gAbsState[uField].nMin,
		gAbsState[uField].nMax);
	wacscrn_output(gAbsState[uField].nRow,gAbsState[uField].nCol*40,xchBuf);
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

void FetchSerial(WACOMTABLET hTablet)
{
	char chOut[16];
	unsigned char uchBuf[16];
	int i, nLength, nRow=gEvState[0].nRow;
	WACOMSTATE state;

	while (1)
	{
		wacscrn_refresh();

		nLength = WacomReadRaw(hTablet,uchBuf,sizeof(uchBuf));
		if (nLength < 0) break;

		if (WacomParseData(hTablet,uchBuf,nLength,&state)) break;

		gAbsState[WACOMFIELD_PROXIMITY].nValue = state.nProximity;
		gAbsState[WACOMFIELD_BUTTONS].nValue = state.nButtons;
		gAbsState[WACOMFIELD_TOOLTYPE].nValue = state.nToolType;
		gAbsState[WACOMFIELD_POSITION_X].nValue = state.nPosX;
		gAbsState[WACOMFIELD_POSITION_Y].nValue = state.nPosY;
		gAbsState[WACOMFIELD_ROTATION_Z].nValue = state.nRotZ;
		gAbsState[WACOMFIELD_PRESSURE].nValue = state.nPressure;
		gAbsState[WACOMFIELD_SERIAL].nValue = state.nSerial;
		gAbsState[WACOMFIELD_TILT_X].nValue = state.nTiltX;
		gAbsState[WACOMFIELD_TILT_Y].nValue = state.nTiltY;
		gAbsState[WACOMFIELD_ABSWHEEL].nValue = state.nAbsWheel;
		gAbsState[WACOMFIELD_RELWHEEL].nValue += state.nRelWheel;
		gAbsState[WACOMFIELD_THROTTLE].nValue = state.nThrottle;
		DisplaySerialValue(WACOMFIELD_PROXIMITY);
		DisplaySerialValue(WACOMFIELD_BUTTONS);
		DisplaySerialValue(WACOMFIELD_TOOLTYPE);
		DisplaySerialValue(WACOMFIELD_POSITION_X);
		DisplaySerialValue(WACOMFIELD_POSITION_Y);
		DisplaySerialValue(WACOMFIELD_ROTATION_Z);
		DisplaySerialValue(WACOMFIELD_PRESSURE);
		DisplaySerialValue(WACOMFIELD_SERIAL);
		DisplaySerialValue(WACOMFIELD_TILT_X);
		DisplaySerialValue(WACOMFIELD_TILT_Y);
		DisplaySerialValue(WACOMFIELD_ABSWHEEL);
		DisplaySerialValue(WACOMFIELD_RELWHEEL);
		DisplaySerialValue(WACOMFIELD_THROTTLE);
		gKeyState[WACOMBUTTON_TOUCH].nValue = state.nButtons &
				(1 << WACOMBUTTON_TOUCH) ? 1 : 0;
		gKeyState[WACOMBUTTON_STYLUS].nValue = state.nButtons &
				(1 << WACOMBUTTON_STYLUS) ? 1 : 0;
		gKeyState[WACOMBUTTON_STYLUS2].nValue = state.nButtons &
				(1 << WACOMBUTTON_STYLUS2) ? 1 : 0;
		gKeyState[WACOMBUTTON_LEFT].nValue = state.nButtons &
				(1 << WACOMBUTTON_LEFT) ? 1 : 0;
		gKeyState[WACOMBUTTON_MIDDLE].nValue = state.nButtons &
				(1 << WACOMBUTTON_MIDDLE) ? 1 : 0;
		gKeyState[WACOMBUTTON_RIGHT].nValue = state.nButtons &
				(1 << WACOMBUTTON_RIGHT) ? 1 : 0;
		gKeyState[WACOMBUTTON_EXTRA].nValue = state.nButtons &
				(1 << WACOMBUTTON_EXTRA) ? 1 : 0;
		gKeyState[WACOMBUTTON_SIDE].nValue = state.nButtons &
				(1 << WACOMBUTTON_SIDE) ? 1 : 0;
		DisplaySerialButton(WACOMBUTTON_TOUCH);
		DisplaySerialButton(WACOMBUTTON_STYLUS);
		DisplaySerialButton(WACOMBUTTON_STYLUS2);
		DisplaySerialButton(WACOMBUTTON_LEFT);
		DisplaySerialButton(WACOMBUTTON_MIDDLE);
		DisplaySerialButton(WACOMBUTTON_RIGHT);
		DisplaySerialButton(WACOMBUTTON_EXTRA);
		DisplaySerialButton(WACOMBUTTON_SIDE);

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
	}
}

int main(int argc, char** argv)
{
	int fd;
	const char* pszFile = NULL;
	const char* arg;

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
	if (!pszFile) pszFile = "/dev/input/event0";
   
	/* open input device */
	fd = open(pszFile,O_RDONLY);
	if (fd < 0) { perror("Failed to open device"); exit(1); }

	/* if it is serial, close and try again */
	if (isatty(fd))
	{
		WACOMTABLET hTablet = NULL;

		close(fd);
		hTablet = WacomOpenSerial(pszFile);
		if (!hTablet) exit(1);

		/* begin curses window mode */
		wacscrn_init();
		atexit(termscr);

		/* get device capabilities, build screen */
		if (InitSerial(hTablet)) { WacomCloseSerial(hTablet); exit(1); }
		FetchSerial(hTablet);
		WacomCloseSerial(hTablet);
		return 0;
	}

	/* begin curses window mode */
	wacscrn_init();
	atexit(termscr);

	/* get device capabilities, build screen */
	if (InitUSB(fd)) { close(fd); exit(1); }
	FetchUSB(fd);
	close(fd);
	return 0;
}

