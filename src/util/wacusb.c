/*****************************************************************************
** wacusb.c
**
** Copyright (C) 2002 - 2004 - John E. Joganic
** Copyright (C) 2003 - 2008 - Ping Cheng
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
****************************************************************************/

#include "../include/util-config.h"

#include "wacusb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <assert.h>

/*****************************************************************************
** Begin USB Linux Input Subsystem
*****************************************************************************/

#ifdef WCM_ENABLE_LINUXINPUT
#include <linux/input.h>

#ifndef EV_MSC
#define EV_MSC 0x04
#endif

#ifndef MSC_SERIAL
#define MSC_SERIAL 0x00
#endif

/*****************************************************************************
** Defines
*****************************************************************************/

/* from linux/input.h */
#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define BIT(x)  (1UL<<((x)%BITS_PER_LONG))
#define LONG(x) ((x)/BITS_PER_LONG)
#define ISBITSET(x,y) ((x)[LONG(y)] & BIT(y))

#define MAX_CHANNELS 2
#define MAX_USB_EVENTS 32

/*****************************************************************************
** Structures
*****************************************************************************/

typedef struct _USBSUBTYPE USBSUBTYPE;
typedef struct _USBDEVICE USBDEVICE;
typedef struct _USBVENDOR USBVENDOR;

struct _USBSUBTYPE
{
	const char* pszName;
	const char* pszDesc;
	unsigned int uSubType;
	unsigned int uProduct;
};

struct _USBDEVICE
{
	const char* pszName;
	const char* pszDesc;
	unsigned int uDevice;
	USBSUBTYPE* pSubTypes;
	unsigned int uChannelCnt;
};

struct _USBVENDOR
{
	const char* pszName;
	const char* pszDesc;
	unsigned int uVendor;
	USBDEVICE* pDevices;
};

/*****************************************************************************
** USB Tablet object
*****************************************************************************/

typedef struct _USBTABLET USBTABLET;

struct _USBTABLET
{
	WACOMTABLET_PRIV tablet;
	WACOMENGINE hEngine;
	int fd;
	USBVENDOR* pVendor;
	USBDEVICE* pDevice;
	USBSUBTYPE* pSubType;
	int nBus;
	int nVersion;
	WACOMSTATE state[MAX_CHANNELS];
	int nToolProx[MAX_CHANNELS];
	int nChannel;
	int nLastToolSerial;
	int nEventCnt;
	struct input_event events[MAX_USB_EVENTS];
};

/*****************************************************************************
** Autodetect pad key codes
*****************************************************************************/

static unsigned short padkey_codes [] =
{
	BTN_0, BTN_1, BTN_2, BTN_3, BTN_4,
	BTN_5, BTN_6, BTN_7, BTN_8, BTN_9,
	BTN_A, BTN_B, BTN_C, BTN_X, BTN_Y, BTN_Z,
	BTN_BASE, BTN_BASE2, BTN_BASE3,
	BTN_BASE4, BTN_BASE5, BTN_BASE6,
	BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT
};

int gPadKeys [WACOMBUTTON_MAX];
int gNumPadKeys;

/*****************************************************************************
** Static Prototypes
*****************************************************************************/

static void USBClose(WACOMTABLET_PRIV* pTablet);
static WACOMMODEL USBGetModel(WACOMTABLET_PRIV* pTablet);
static const char* USBGetVendorName(WACOMTABLET_PRIV* pTablet);
static const char* USBGetClassName(WACOMTABLET_PRIV* pTablet);
static const char* USBGetDeviceName(WACOMTABLET_PRIV* pTablet);
static const char* USBGetSubTypeName(WACOMTABLET_PRIV* pTablet);
static const char* USBGetModelName(WACOMTABLET_PRIV* pTablet);
static int USBGetROMVer(WACOMTABLET_PRIV* pTablet, int* pnMajor,
		int* pnMinor, int* pnRelease);
static int USBGetCaps(WACOMTABLET_PRIV* pTablet);
static int USBGetState(WACOMTABLET_PRIV* pTablet, WACOMSTATE* pState);
static int USBGetFD(WACOMTABLET_PRIV* pTablet);
static int USBReadRaw(WACOMTABLET_PRIV* pTablet, unsigned char* puchData,
		unsigned int uSize);
static int USBParseData(WACOMTABLET_PRIV* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);

static int USBFindModel(USBTABLET* pUSB, unsigned int uVendor,
		unsigned int uProduct);
static int USBIdentifyModel(USBTABLET* pUSB);

/*****************************************************************************
** Globals
*****************************************************************************/

	static USBSUBTYPE xPenPartner[] =
	{
		{ "MODEL_PP_0405", "Wacom PenPartner", 1, 0x00 },
		{ NULL }
	};

	static USBSUBTYPE xGraphire[] =
	{
		{ "ET_0405", "Wacom Graphire", 1, 0x10 },
		{ NULL }
	};

	static USBSUBTYPE xGraphire2[] =
	{
		{ "ET_0405", "Wacom Graphire2 4x5", 1, 0x11 },
		{ "ET_0507", "Wacom Graphire2 5x7", 2, 0x12 },
		{ NULL }
	};

	static USBSUBTYPE xGraphire3[] =
	{
		{ "ET_0405", "Wacom Graphire3 4x5", 1, 0x13 },
		{ "ET_0608", "Wacom Graphire3 6x8", 2, 0x14 },
		{ NULL }
	};

	static USBSUBTYPE xGraphire4[] =
	{
		{ "CTE_440", "Wacom Graphire4 4x5", 1, 0x15 },
		{ "CTE_640", "Wacom Graphire4 6x8", 2, 0x16 },
		{ NULL }
	};

	static USBSUBTYPE xIntuos[] =
	{
		{ "GD_0405-U", "Wacom Intuos 4x5",   1, 0x20 },
		{ "GD_0608-U", "Wacom Intuos 6x8",   2, 0x21 },
		{ "GD_0912-U", "Wacom Intuos 9x12",  3, 0x22 },
		{ "GD_1212-U", "Wacom Intuos 12x12", 4, 0x23 },
		{ "GD_1218-U", "Wacom Intuos 12x18", 5, 0x24 },
		{ NULL }
	};

	static USBSUBTYPE xCintiq[] =
	{
		{ "MODEL_PL400",   "Wacom PL400",    1, 0x30 },
		{ "MODEL_PL500",   "Wacom PL500",    2, 0x31 },
		{ "MODEL_PL600",   "Wacom PL600",    3, 0x32 },
		{ "MODEL_PL600SX", "Wacom PL600SX",  4, 0x33 },
		{ "MODEL_PL550",   "Wacom PL550",    5, 0x34 },
		{ "MODEL_PL800",   "Wacom PL800",    6, 0x35 },
		{ "MODEL_PL700",   "Wacom PL700",    7, 0x37 },
		{ "MODEL_PL510",   "Wacom PL510",    8, 0x38 },
		{ "MODEL_DTU710",  "Wacom PL710",    9, 0x39 },
		{ "MODEL_DTF720",  "Wacom DTF720",  10, 0xC0 },
		{ "MODEL_DTF521",  "Wacom DTF521",  11, 0xC4 },
		{ NULL }
	};

	static USBSUBTYPE xIntuos2[] =
	{
		{ "XD-0405-U", "Wacom Intuos2 4x5",   1, 0x41 },
		{ "XD-0608-U", "Wacom Intuos2 6x8",   2, 0x42 },
		{ "XD-0912-U", "Wacom Intuos2 9x12",  3, 0x43 },
		{ "XD-1212-U", "Wacom Intuos2 12x12", 4, 0x44 },
		{ "XD-1218-U", "Wacom Intuos2 12x18", 5, 0x45 },

		/* fix for I2 6x8's reporting as 0x47 */
		{ "XD-0608-U", "Wacom Intuos2 6x8", 2, 0x47 },

		{ NULL }
	};

	static USBSUBTYPE xVolito[] =
	{
		{ "MODEL-VOL", "Wacom Volito", 1, 0x60 },
		{ NULL }
	};

	static USBSUBTYPE xVolito2[] =
	{
		{ "FT-0203-U", "Wacom PenStation",  1, 0x61 },
		{ "CTF-420-U", "Wacom Volito2 4x5", 2, 0x62 },
		{ "CTF-220-U", "Wacom Volito2 2x3", 3, 0x63 },
		{ "CTF-421-U", "Wacom PenPartner2", 4, 0x64 },
		{ "CTF_430-U", "Wacom Bamboo1",     5, 0x69 },
		{ NULL }
	};

	static USBSUBTYPE xBamboo[] =
	{
		{ "MTE_450", "Wacom Bamboo", 1, 0x65 },
		{ "CTE_450", "Wacom BambooFun 4x5", 2, 0x17 },
		{ "CTE_650", "Wacom BambooFun 6x8", 3, 0x18 },
		{ NULL }
	};

	static USBSUBTYPE xCintiqPartner[] =
	{
		{ "PTU-600", "Wacom Cintiq Partner", 1, 0x03 },
		{ NULL }
	};

	static USBSUBTYPE xCintiqV5[] =
	{
		{ "DTZ-21ux",  "Wacom Cintiq 21UX",  1, 0x3F },
		{ "DTZ-20wsx", "Wacom Cintiq 20WSX", 2, 0xC5 },
		{ "DTZ-12wx",  "Wacom Cintiq 12WX",  3, 0xC6 },
		{ NULL }
	};

	static USBSUBTYPE xIntuos3[] =
	{
		{ "PTZ-430",   "Wacom Intuos3 4x5",   1, 0xB0 },
		{ "PTZ-630",   "Wacom Intuos3 6x8",   2, 0xB1 },
		{ "PTZ-930",   "Wacom Intuos3 9x12",  3, 0xB2 },
		{ "PTZ-1230",  "Wacom Intuos3 12x12", 4, 0xB3 },
		{ "PTZ-1231W", "Wacom Intuos3 12x19", 5, 0xB4 },
		{ "PTZ-631W",  "Wacom Intuos3 6x11",  6, 0xB5 },
		{ "PTZ-431W",  "Wacom Intuos3 4x6",   7, 0xB7 },
		{ NULL }
	};

	static USBDEVICE xWacomDevices[] =
	{
		{ "pp", "PenPartner", WACOMDEVICE_PENPARTNER, xPenPartner, 1 },
		{ "gr", "Graphire", WACOMDEVICE_GRAPHIRE, xGraphire, 1 },
		{ "gr2", "Graphire2", WACOMDEVICE_GRAPHIRE2, xGraphire2, 1 },
		{ "gr3", "Graphire3", WACOMDEVICE_GRAPHIRE3, xGraphire3, 1 },
		{ "gr4", "Graphire4", WACOMDEVICE_GRAPHIRE4, xGraphire4, 2 },
		{ "int", "Intuos", WACOMDEVICE_INTUOS, xIntuos, 2 },
		{ "int2", "Intuos2", WACOMDEVICE_INTUOS2, xIntuos2, 2 },
		{ "int3", "Intuos3", WACOMDEVICE_INTUOS3, xIntuos3, 2 },
		{ "ctq", "Cintiq (V5)", WACOMDEVICE_CINTIQV5, xCintiqV5, 2 },
		{ "pl", "Cintiq (PL)", WACOMDEVICE_CINTIQ, xCintiq, 1 },
		{ "ptu", "Cintiq Partner (PTU)", WACOMDEVICE_PTU, xCintiqPartner, 1 },
		{ "vol", "Volito", WACOMDEVICE_VOLITO, xVolito, 1 },
		{ "vol2", "Volito2", WACOMDEVICE_VOLITO2, xVolito2, 1 },
		{ "mo", "Bamboo", WACOMDEVICE_MO, xBamboo, 2 },
		{ NULL }
	};

	static USBVENDOR xVendors[] =
	{
		{ "wacom", "Wacom", WACOMVENDOR_WACOM, xWacomDevices },
		{ NULL },
	};

/*****************************************************************************
** Public Functions
*****************************************************************************/

typedef struct
{
	void (*pfnFree)(void* pv);
} DEVLIST_INTERNAL;

static void USBFreeDeviceList(void* pv)
{
	DEVLIST_INTERNAL* pInt = ((DEVLIST_INTERNAL*)pv) - 1;
	free(pInt);
}

int WacomGetSupportedUSBDeviceList(WACOMDEVICEREC** ppList, int* pnSize)
{
	int nIndex=0, nCnt=0;
	DEVLIST_INTERNAL* pInt;
	USBDEVICE* pDev;
	USBVENDOR* pVendor;
	WACOMDEVICEREC* pRec;

	if (!ppList || !pnSize) { errno = EINVAL; return 1; }

	/* for each vendor, count up devices */
	for (pVendor=xVendors; pVendor->pszName; ++pVendor)
	{
		/* count up devices */
		for (pDev=pVendor->pDevices; pDev->pszName; ++pDev, ++nCnt) ;
	}
   
	/* allocate enough memory to hold internal structure and all records */
	pInt = (DEVLIST_INTERNAL*)malloc(sizeof(DEVLIST_INTERNAL) +
					(sizeof(WACOMDEVICEREC) * nCnt));

	pInt->pfnFree = USBFreeDeviceList;
	pRec = (WACOMDEVICEREC*)(pInt + 1);

	/* for each vendor, add devices */
	for (pVendor=xVendors; pVendor->pszName; ++pVendor)
	{
		for (pDev=pVendor->pDevices; pDev->pszName; ++pDev, ++nIndex)
		{
			pRec[nIndex].pszName = pDev->pszName;
			pRec[nIndex].pszDesc = pDev->pszDesc;
			pRec[nIndex].pszVendorName = pVendor->pszName;
			pRec[nIndex].pszVendorDesc = pVendor->pszDesc;
			pRec[nIndex].pszClass = "usb";
			pRec[nIndex].model.uClass = WACOMCLASS_USB;
			pRec[nIndex].model.uVendor = pVendor->uVendor;
			pRec[nIndex].model.uDevice = pDev->uDevice;
			pRec[nIndex].model.uSubType = 0;
		}
	}
	assert(nIndex == nCnt);

	*ppList = pRec;
	*pnSize = nCnt;
	return 0;
}

unsigned int WacomGetUSBDeviceFromName(const char* pszName)
{
	USBDEVICE* pDev;
	USBVENDOR* pVendor;

	if (!pszName) { errno = EINVAL; return 0; }

	/* for each vendor, look for device */
	for (pVendor=xVendors; pVendor->pszName; ++pVendor)
	{
		/* count up devices */
		for (pDev=pVendor->pDevices; pDev->pszName; ++pDev)
		{
			if (strcasecmp(pszName,pDev->pszName) == 0)
				return pDev->uDevice;
		}
	}

	errno = ENOENT;
	return 0;
}

WACOMTABLET WacomOpenUSBTablet(WACOMENGINE hEngine, int fd, WACOMMODEL* pModel)
{
	USBTABLET* pUSB = NULL;

	/* Allocate tablet */
	pUSB = (USBTABLET*)malloc(sizeof(USBTABLET));
	memset(pUSB,0,sizeof(*pUSB));
	pUSB->tablet.Close = USBClose;
	pUSB->tablet.GetModel = USBGetModel;
	pUSB->tablet.GetVendorName = USBGetVendorName;
	pUSB->tablet.GetClassName = USBGetClassName;
	pUSB->tablet.GetDeviceName = USBGetDeviceName;
	pUSB->tablet.GetSubTypeName = USBGetSubTypeName;
	pUSB->tablet.GetModelName = USBGetModelName;
	pUSB->tablet.GetROMVer = USBGetROMVer;
	pUSB->tablet.GetCaps = USBGetCaps;
	pUSB->tablet.GetState = USBGetState;
	pUSB->tablet.GetFD = USBGetFD;
	pUSB->tablet.ReadRaw = USBReadRaw;
	pUSB->tablet.ParseData = USBParseData;
	pUSB->hEngine = hEngine;
	pUSB->fd = fd;

	/* Identify and initialize the model */
	if (USBIdentifyModel(pUSB))
	{
		WacomLog(pUSB->hEngine,WACOMLOGLEVEL_ERROR,
			"Failed to identify model: %s",strerror(errno));
		free(pUSB);
		return NULL;
	}

	return (WACOMTABLET)pUSB;
}

/*****************************************************************************
** Tablet Functions
*****************************************************************************/

static void USBClose(WACOMTABLET_PRIV* pTablet)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	close(pUSB->fd);
	free(pUSB);
}

static WACOMMODEL USBGetModel(WACOMTABLET_PRIV* pTablet)
{
	WACOMMODEL model = { 0 };
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	model.uClass = WACOMCLASS_USB;
	model.uVendor = pUSB->pVendor->uVendor;
	model.uDevice = pUSB->pDevice->uDevice;
	model.uSubType = pUSB->pSubType->uSubType;
	return model;
}

static int USBGetRange(USBTABLET* pUSB, unsigned long* pBits, int nAbsField,
		unsigned int uField)
{
	int nAbs[5];

	if  (!ISBITSET(pBits,nAbsField))
		return 0;
	if (ioctl(pUSB->fd, EVIOCGABS(nAbsField), nAbs) != 0)
		return 1;

	pUSB->state[0].values[uField].nMin = nAbs[1];
	pUSB->state[0].values[uField].nMax = nAbs[2];
	pUSB->state[0].uValid |= BIT(uField);
	return 0;
}

static int USBFindModel(USBTABLET* pUSB, unsigned int uVendor,
		unsigned int uProduct)
{
	USBVENDOR* pVendor = NULL;
	USBDEVICE* pDev = NULL;
	USBSUBTYPE* pSub = NULL;

	static USBSUBTYPE xUnkSub[] =
	{
		{ "UNKNOWN", "Unknown", 1, 0x00 },
		{ NULL }
	};

	static USBDEVICE xUnkDev[] =
	{
		{ "unk", "Unknown", WACOMDEVICE_UNKNOWN, xUnkSub, MAX_CHANNELS },
		{ NULL }
	};

	static USBVENDOR xUnkVendor =
	{ "unknown", "Unknown", WACOMVENDOR_UNKNOWN, xUnkDev };

	for (pVendor=xVendors; pVendor->pszName; ++pVendor)
	{
		if (pVendor->uVendor == uVendor)
		{
			for (pDev=pVendor->pDevices; pDev->pszName; ++pDev)
			{
				for (pSub=pDev->pSubTypes; pSub->pszName; ++pSub)
				{
					if (pSub->uProduct == uProduct)
					{
						pUSB->pVendor = pVendor;
						pUSB->pDevice = pDev;
						pUSB->pSubType = pSub;
						return 0;
					}
				}
			}

			/* only one vendor of this type, so we're done */
			break;
		}
	}

	/* unknown vendor */
	pUSB->pVendor = &xUnkVendor;
	pUSB->pDevice = pUSB->pVendor->pDevices;
	pUSB->pSubType = pUSB->pDevice->pSubTypes;
	return 0;
}

static int USBIdentifyModel(USBTABLET* pUSB)
{
	int nCnt, chcnt;
	short sID[4];
	unsigned int uVendor, uProduct;
	unsigned long evbits[NBITS(EV_MAX)];
	unsigned long absbits[NBITS(ABS_MAX)];
	unsigned long relbits[NBITS(REL_MAX)];
	unsigned long keybits[NBITS(KEY_MAX)];

	/* Get device name and id */
	if (ioctl(pUSB->fd,EVIOCGID,sID) < 0)
		return 1;

	/* initialize state structure */
	pUSB->state[0].uValueCnt = WACOMFIELD_MAX;

	/* Get event types supported */
	nCnt = ioctl(pUSB->fd,EVIOCGBIT(0 /*EV*/,sizeof(evbits)),evbits);
	if (nCnt < 0)
	{
		WacomLog(pUSB->hEngine,WACOMLOGLEVEL_ERROR,
			"Failed to CGBIT ev: %s",strerror(errno));
		return 1;
	}
	assert(nCnt == sizeof(evbits));

	/* absolute events */
	if (ISBITSET(evbits,EV_ABS))
	{
		nCnt = ioctl(pUSB->fd,EVIOCGBIT(EV_ABS,sizeof(absbits)),absbits);
		if (nCnt < 0)
		{
			WacomLog(pUSB->hEngine,WACOMLOGLEVEL_ERROR,
				"Failed to CGBIT abs: %s",strerror(errno));
			return 1;
		}

		/* the following line has problem on Debian systems
		assert(nCnt == sizeof(absbits));
		*/

		if (USBGetRange(pUSB,absbits,ABS_X,WACOMFIELD_POSITION_X) ||
			USBGetRange(pUSB,absbits,ABS_Y,WACOMFIELD_POSITION_Y) ||
			USBGetRange(pUSB,absbits,ABS_RZ,WACOMFIELD_ROTATION_Z) ||
			USBGetRange(pUSB,absbits,ABS_DISTANCE,WACOMFIELD_DISTANCE) ||
			USBGetRange(pUSB,absbits,ABS_PRESSURE,WACOMFIELD_PRESSURE) ||
			USBGetRange(pUSB,absbits,ABS_TILT_X,WACOMFIELD_TILT_X) ||
			USBGetRange(pUSB,absbits,ABS_TILT_Y,WACOMFIELD_TILT_Y) ||
			USBGetRange(pUSB,absbits,ABS_WHEEL,WACOMFIELD_ABSWHEEL) ||
			USBGetRange(pUSB,absbits,ABS_THROTTLE,WACOMFIELD_THROTTLE))
			return 1;

	}

	/* relative events */
	if (ISBITSET(evbits,EV_REL))
	{
		nCnt = ioctl(pUSB->fd,EVIOCGBIT(EV_REL,sizeof(relbits)),relbits);
		if (nCnt < 0)
		{
			WacomLog(pUSB->hEngine,WACOMLOGLEVEL_ERROR,
				"Failed to CGBIT rel: %s",strerror(errno));
			return 1;
		}
		assert(nCnt == sizeof(relbits));

		if (ISBITSET(relbits,REL_WHEEL))
		{
			pUSB->state[0].uValid |= BIT(WACOMFIELD_RELWHEEL);
			pUSB->state[0].values[WACOMFIELD_RELWHEEL].nMin = -1;
			pUSB->state[0].values[WACOMFIELD_RELWHEEL].nMax = 1;
		}
	}

	/* key events */
	gNumPadKeys = 0;
	if (ISBITSET(evbits,EV_KEY))
	{
		nCnt = ioctl(pUSB->fd,EVIOCGBIT(EV_KEY,sizeof(keybits)),keybits);
		if (nCnt < 0)
		{
			WacomLog(pUSB->hEngine,WACOMLOGLEVEL_ERROR,
				"Failed to CGBIT key: %s",strerror(errno));
			return 1;
		}
		assert(nCnt == sizeof(keybits));

		/* button events */
		if (ISBITSET(keybits,BTN_LEFT) ||
				ISBITSET(keybits,BTN_RIGHT) ||
				ISBITSET(keybits,BTN_MIDDLE) ||
				ISBITSET(keybits,BTN_SIDE) ||
				ISBITSET(keybits,BTN_EXTRA))
			pUSB->state[0].uValid |= BIT(WACOMFIELD_BUTTONS);

		/* tool events */
		if (ISBITSET(keybits,BTN_TOOL_PEN) ||
				ISBITSET(keybits,BTN_TOOL_RUBBER) ||
				ISBITSET(keybits,BTN_TOOL_BRUSH) ||
				ISBITSET(keybits,BTN_TOOL_PENCIL) ||
				ISBITSET(keybits,BTN_TOOL_AIRBRUSH) ||
				ISBITSET(keybits,BTN_TOOL_FINGER) ||
				ISBITSET(keybits,BTN_TOOL_MOUSE) ||
				ISBITSET(keybits,BTN_TOOL_LENS))
			pUSB->state[0].uValid |= BIT(WACOMFIELD_PROXIMITY) |
					BIT(WACOMFIELD_TOOLTYPE);

		/* Detect button codes */
		for (nCnt = 0; nCnt < WACOMBUTTON_MAX; nCnt++)
			if (ISBITSET (keybits, padkey_codes [nCnt]))
				gPadKeys [gNumPadKeys++] = padkey_codes [nCnt];
	}

	/* set identification */
	pUSB->nBus = sID[0];
	uVendor = sID[1];
	uProduct = sID[2];
	pUSB->nVersion = sID[3];

	if (USBFindModel(pUSB,uVendor,uProduct))
		return 1;

	/* add additional capabilities by device type */
	switch (pUSB->pDevice->uDevice)
	{
		case WACOMDEVICE_MO:
		case WACOMDEVICE_GRAPHIRE4:
		case WACOMDEVICE_INTUOS:
		case WACOMDEVICE_INTUOS2:
		case WACOMDEVICE_INTUOS3:
		case WACOMDEVICE_CINTIQV5:
			pUSB->state[0].uValid |= BIT(WACOMFIELD_SERIAL);
		default: ;
	}

	/* Initialize all channels with the same values */
	for(chcnt=1; chcnt<MAX_CHANNELS; chcnt++)
		pUSB->state[chcnt] = pUSB->state[0];

	return 0;
}

static const char* USBGetVendorName(WACOMTABLET_PRIV* pTablet)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	return pUSB->pVendor->pszDesc;
}

static const char* USBGetClassName(WACOMTABLET_PRIV* pTablet)
{
	return "USB";
}

static const char* USBGetDeviceName(WACOMTABLET_PRIV* pTablet)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	return pUSB->pDevice->pszDesc;
}

static const char* USBGetSubTypeName(WACOMTABLET_PRIV* pTablet)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	return pUSB->pSubType->pszName;
}

static const char* USBGetModelName(WACOMTABLET_PRIV* pTablet)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	return pUSB->pSubType->pszDesc;
}

static int USBGetROMVer(WACOMTABLET_PRIV* pTablet, int* pnMajor,
		int* pnMinor, int* pnRelease)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	if (!pnMajor) { errno=EINVAL; return 1; }

	/* how is the version number broken down?
	 * mine says 0x115.  is that 1.1.5 or 1.15? */
	*pnMajor = pUSB->nVersion >> 8;
	*pnMinor = (pUSB->nVersion >> 4) & 0xF;
	*pnRelease = (pUSB->nVersion & 0xF);
	return 0;
}

static int USBGetCaps(WACOMTABLET_PRIV* pTablet)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	return pUSB->state[0].uValid;
}

static int USBGetState(WACOMTABLET_PRIV* pTablet, WACOMSTATE* pState)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	return WacomCopyState(pState,&pUSB->state[pUSB->nChannel]);
}

static int USBGetFD(WACOMTABLET_PRIV* pTablet)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	return pUSB->fd;
}

static int USBReadRaw(WACOMTABLET_PRIV* pTablet, unsigned char* puchData,
		unsigned int uSize)
{
	int nXfer;
	unsigned int uCnt, uPacketLength;
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	uPacketLength = sizeof(struct input_event);

	/* check size of buffer */
	if (uSize < uPacketLength) { errno=EINVAL; return 0; }
	
	for (uCnt=0; uCnt<uPacketLength; uCnt+=nXfer)
	{
		nXfer = read(pUSB->fd,puchData+uCnt,uPacketLength-uCnt);
		if (nXfer <= 0) return nXfer;
	}

	return (signed)uCnt;
}

static int USBParseMSC(USBTABLET* pUSB, struct input_event* pEv)
{
	if (pEv->code == MSC_SERIAL && pEv->value)
		pUSB->state[pUSB->nChannel].values[WACOMFIELD_SERIAL].nValue = pEv->value;
	if (!pUSB->state[pUSB->nChannel].values[WACOMFIELD_PROXIMITY].nValue)
		pUSB->state[pUSB->nChannel].values[WACOMFIELD_SERIAL].nValue = 0;
	return 0;
}

static int USBParseKEY(USBTABLET* pUSB, struct input_event* pEv)
{
	int i, button=-1, tool=0;
	switch (pEv->code)
	{
		case BTN_LEFT: button = WACOMBUTTON_LEFT; break;
		case BTN_RIGHT: button = WACOMBUTTON_RIGHT; break;
		case BTN_MIDDLE: button = WACOMBUTTON_MIDDLE; break;
		case BTN_SIDE: button = WACOMBUTTON_SIDE; break;
		case BTN_EXTRA: button = WACOMBUTTON_EXTRA; break;
		case BTN_TOUCH: button = WACOMBUTTON_TOUCH; break;
		case BTN_STYLUS: button = WACOMBUTTON_STYLUS; break;
		case BTN_STYLUS2: button = WACOMBUTTON_STYLUS2; break;
		case BTN_TOOL_PEN: tool = WACOMTOOLTYPE_PEN; break;
		case BTN_TOOL_PENCIL: tool = WACOMTOOLTYPE_PENCIL; break;
		case BTN_TOOL_BRUSH: tool = WACOMTOOLTYPE_BRUSH; break;
		case BTN_TOOL_RUBBER: tool = WACOMTOOLTYPE_ERASER; break;
		case BTN_TOOL_AIRBRUSH: tool = WACOMTOOLTYPE_AIRBRUSH; break;
		case BTN_TOOL_MOUSE: tool = WACOMTOOLTYPE_MOUSE; break;
		case BTN_TOOL_FINGER: tool = WACOMTOOLTYPE_PAD; break;
		case BTN_TOOL_LENS: tool = WACOMTOOLTYPE_LENS; break;
		default:
			for (i = 0; i < gNumPadKeys; i++)
				if (pEv->code == gPadKeys [i])
				{
					button = WACOMBUTTON_BT0 + i;
					break;
				}
	}

	/* if button was specified */
	if (button != -1)
	{
		/* button state change */
		if (pEv->value)
			pUSB->state[pUSB->nChannel].values[WACOMFIELD_BUTTONS].nValue |= BIT(button);
		else
			pUSB->state[pUSB->nChannel].values[WACOMFIELD_BUTTONS].nValue &= ~BIT(button);
	}

	/* if a tool was specified */
	if (tool)
	{
		/* coming into proximity */
		if (pEv->value)
		{
			/* no prior tool in proximity */
			if (!(pUSB->nToolProx[pUSB->nChannel] & BIT(tool)))
			{
				pUSB->state[pUSB->nChannel].values[WACOMFIELD_PROXIMITY].nValue = 1;
			}

			/* remember tool in prox */
			pUSB->nToolProx[pUSB->nChannel] |= BIT(tool);
			pUSB->state[pUSB->nChannel].values[WACOMFIELD_TOOLTYPE].nValue = tool;
		}

		/* otherwise, going out of proximity */
		else
		{
			/* forget tool in prox */
			if (pUSB->nToolProx[pUSB->nChannel] & BIT(tool))
			{
				pUSB->nToolProx[pUSB->nChannel] &= ~BIT(tool);
			}

			/* single input */
			if (!(pUSB->state[pUSB->nChannel].uValid & BIT(WACOMFIELD_SERIAL)))
				pUSB->nToolProx[pUSB->nChannel] = 0;

			pUSB->state[pUSB->nChannel].values[WACOMFIELD_PROXIMITY].nValue = 0;
			pUSB->state[pUSB->nChannel].values[WACOMFIELD_TOOLTYPE].nValue = 0;

			/* nobody left? out of proximity */
			if (!pUSB->nToolProx[pUSB->nChannel])
				memset(pUSB->state[pUSB->nChannel].values, 0,
						pUSB->state[pUSB->nChannel].uValueCnt * sizeof(WACOMVALUE));
			/* otherwise, switch to next tool */
			else
			{
				for (i=WACOMTOOLTYPE_PEN; i<WACOMTOOLTYPE_MAX; ++i)
				{
					if (pUSB->nToolProx[pUSB->nChannel] & BIT(i))
					{
						pUSB->state[pUSB->nChannel].values[WACOMFIELD_TOOLTYPE].nValue = i;
					}
				}
			}
		} /* out of prox */
	} /* end if tool */

	return 0;
}

static int USBParseABS(USBTABLET* pUSB, struct input_event* pEv)
{
	int field = 0;
	switch (pEv->code)
	{
		case ABS_X: field = WACOMFIELD_POSITION_X; break;
		case ABS_Y: field = WACOMFIELD_POSITION_Y; break;
		case ABS_RZ: field = WACOMFIELD_ROTATION_Z; break;
		case ABS_DISTANCE: field = WACOMFIELD_DISTANCE; break;
		case ABS_PRESSURE: field = WACOMFIELD_PRESSURE; break;
		case ABS_TILT_X: field = WACOMFIELD_TILT_X; break;
		case ABS_TILT_Y: field = WACOMFIELD_TILT_Y; break;
		case ABS_WHEEL: field = WACOMFIELD_ABSWHEEL; break;
		case ABS_THROTTLE: field = WACOMFIELD_THROTTLE; break;
	}

	if (field)
		pUSB->state[pUSB->nChannel].values[field].nValue = pEv->value;

	return 0;
}

static int USBParseREL(USBTABLET* pUSB, struct input_event* pEv)
{
	int field = 0;
	switch (pEv->code)
	{
		case REL_WHEEL: field = WACOMFIELD_RELWHEEL; break;
	}

	if (field)
		pUSB->state[pUSB->nChannel].values[field].nValue = pEv->value;

	return 0;
}

static int USBParseData(WACOMTABLET_PRIV* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState)
{
	int evcnt, chcnt;
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	struct input_event* pEv = (struct input_event*)puchData;
	if (uLength != sizeof(struct input_event)) return 1;

	/* store event until we receive a MSC_SERIAL/SYN_REPORT
	 * so we can figure out with channel to use */
	if (pUSB->nEventCnt >= MAX_USB_EVENTS)
	{
		/* no more buffer space */
		pUSB->nEventCnt = 0;
		pUSB->nLastToolSerial = 0;
		errno = ENOBUFS;
		return -1;
	}
	pUSB->events[pUSB->nEventCnt++] = *pEv;

	if ((pEv->type == EV_MSC) && (pEv->code == MSC_SERIAL))
	{
		/* store the serial for the tool for later use */
		pUSB->nLastToolSerial = pEv->value;
#ifdef EV_SYN
		/* Kernel 2.4 uses MSC_SERIAL as an end-of-report, 2.6
		 * don't so we're not done yet */
		return 0;
	}
	else if ((pEv->type == EV_SYN) && (pEv->code == SYN_REPORT))
	{
		/* Kernel 2.6 used SYN_REPORT as an end-of-record,
		 * fall through */
#endif
	}
	else
	{
		/* Not a MSC_SERIAL or SYN_REPORT, we're not done yet */
		return 0;
	}

	/* Select channel here based on the serial number, tablets
	 * with only one channel will always use channel 0, so no
	 * check.
	 */
	if (pUSB->pDevice->uChannelCnt > 1)
	{
		pUSB->nChannel = -1;
		/* Find existing channel */
		for (chcnt=0; chcnt<pUSB->pDevice->uChannelCnt; chcnt++)
		{
			if (pUSB->state[chcnt].values[WACOMFIELD_SERIAL].nValue == pUSB->nLastToolSerial)
			{
				pUSB->nChannel = chcnt;
				break;
			}
		}

		/* Find en empty channel */
		if(pUSB->nChannel == -1)
		{
			for (chcnt=0; chcnt<pUSB->pDevice->uChannelCnt; chcnt++)
			{
				if(!pUSB->nToolProx[chcnt])
				{
					pUSB->nChannel = chcnt;
					break;
				}
			}
		}

		/* no more channels?? */
		if(pUSB->nChannel == -1)
		{
			pUSB->nChannel = 0;
			pUSB->nEventCnt = 0;
			pUSB->nLastToolSerial = 0;
			errno = ENOBUFS;
			return -1;
		}
	}

	/* Channel found/allocated, lets process events */
	pUSB->state[pUSB->nChannel].values[WACOMFIELD_RELWHEEL].nValue = 0;

	for (evcnt=0; evcnt<pUSB->nEventCnt; evcnt++)
	{
		pEv = pUSB->events + evcnt;
		/* dispatch event */
		switch (pEv->type)
		{
#ifdef EV_SYN
		case EV_SYN: /* kernel 2.6 */
#endif
		case EV_MSC: /* kernel 2.4 */
			     if (USBParseMSC(pUSB,pEv)) return pEv->type; break;
		case EV_KEY: if (USBParseKEY(pUSB,pEv)) return pEv->type; break;
		case EV_ABS: if (USBParseABS(pUSB,pEv)) return pEv->type; break;
		case EV_REL: if (USBParseREL(pUSB,pEv)) return pEv->type; break;
		default: errno = EINVAL; return pEv->type;
		}
	}

	pUSB->nEventCnt = 0;
	pUSB->nLastToolSerial = 0;
	return pState ? WacomCopyState(pState,pUSB->state + pUSB->nChannel) : 0;
}

/*****************************************************************************
** End USB Linux Input Subsystem
*****************************************************************************/

#else /* WCM_ENABLE_LINUXWACOM */

WACOMTABLET WacomOpenUSBTablet(WACOMENGINE hEngine, int fd, WACOMMODEL* pModel)
{
	errno = EPERM;
	return NULL;
}

unsigned int WacomGetUSBDeviceFromName(const char* pszName)
{
	errno = ENOENT;
	return 0;
}

int WacomGetSupportedUSBDeviceList(WACOMDEVICEREC** ppList, int* pnSize)
{
	if (!ppList || !pnSize) { errno = EINVAL; return 1; }
	*ppList = NULL;
	*pnSize = 0;
	return 0;
}

#endif /* WCM_ENABLE_LINUXWACOM */
