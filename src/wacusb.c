/*****************************************************************************
** wacusb.c
**
** Copyright (C) 2002, 2003 - John E. Joganic
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

#include "wacusb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*****************************************************************************
** Begin USB Linux Input Subsystem
*****************************************************************************/

#ifdef WAC_ENABLE_LINUXINPUT
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

/*****************************************************************************
** Structures
*****************************************************************************/

typedef struct
{
	int nProduct;
	unsigned int uDeviceType;
} USB_MODEL_INFO;

/*****************************************************************************
** USB Tablet object
*****************************************************************************/

typedef struct _USBTABLET USBTABLET;

struct _USBTABLET
{
	WACOMTABLET_PRIV tablet;
	int fd;
	unsigned int uDeviceType;
	const char* pszVendorName;
	char chName[64];
	int nBus;
	int nVendor;
	int nProduct;
	int nVersion;
	WACOMSTATE state;
	int nToolProx;
};

/*****************************************************************************
** Static Prototypes
*****************************************************************************/

static void USBClose(WACOMTABLET_PRIV* pTablet);
static WACOMMODEL USBGetModel(WACOMTABLET_PRIV* pTablet);
static const char* USBGetVendorName(WACOMTABLET_PRIV* pTablet);
static const char* USBGetModelName(WACOMTABLET_PRIV* pTablet);
static int USBGetROMVer(WACOMTABLET_PRIV* pTablet, int* pnMajor,
		int* pnMinor, int* pnRelease);
static int USBGetCaps(WACOMTABLET_PRIV* pTablet);
static int USBGetState(WACOMTABLET_PRIV* pTablet, WACOMSTATE* pState);
static int USBReadRaw(WACOMTABLET_PRIV* pTablet, unsigned char* puchData,
		unsigned int uSize);
static int USBParseData(WACOMTABLET_PRIV* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);

static int USBIdentifyModel(USBTABLET* pUSB);

/*****************************************************************************
** Globals
*****************************************************************************/

	static USB_MODEL_INFO xWacomModels[] =
	{
		{ 0x00, WACOMDEVICE_PENPARTNER },

		{ 0x10, WACOMDEVICE_GRAPHIRE },
		{ 0x11, WACOMDEVICE_GRAPHIRE },
		{ 0x12, WACOMDEVICE_GRAPHIRE },

		{ 0x20, WACOMDEVICE_INTUOS },
		{ 0x21, WACOMDEVICE_INTUOS },
		{ 0x22, WACOMDEVICE_INTUOS },
		{ 0x23, WACOMDEVICE_INTUOS },
		{ 0x24, WACOMDEVICE_INTUOS },

		{ 0x30, WACOMDEVICE_CINTIQ },
		{ 0x31, WACOMDEVICE_CINTIQ },
		{ 0x32, WACOMDEVICE_CINTIQ },
		{ 0x33, WACOMDEVICE_CINTIQ },
		{ 0x34, WACOMDEVICE_CINTIQ },
		{ 0x35, WACOMDEVICE_CINTIQ },

		{ 0x41, WACOMDEVICE_INTUOS2 },
		{ 0x42, WACOMDEVICE_INTUOS2 },
		{ 0x43, WACOMDEVICE_INTUOS2 },
		{ 0x44, WACOMDEVICE_INTUOS2 },
		{ 0x45, WACOMDEVICE_INTUOS2 },

		{ 0x60, WACOMDEVICE_VOLITO },

		{ 0, 0 }
	};

/*****************************************************************************
** Public Functions
*****************************************************************************/

WACOMTABLET WacomOpenUSBTablet(int fd)
{
	USBTABLET* pUSB = NULL;

	/* Allocate tablet */
	pUSB = (USBTABLET*)malloc(sizeof(USBTABLET));
	memset(pUSB,0,sizeof(*pUSB));
	pUSB->tablet.Close = USBClose;
	pUSB->tablet.GetModel = USBGetModel;
	pUSB->tablet.GetVendorName = USBGetVendorName;
	pUSB->tablet.GetModelName = USBGetModelName;
	pUSB->tablet.GetROMVer = USBGetROMVer;
	pUSB->tablet.GetCaps = USBGetCaps;
	pUSB->tablet.GetState = USBGetState;
	pUSB->tablet.ReadRaw = USBReadRaw;
	pUSB->tablet.ParseData = USBParseData;
	pUSB->fd = fd;

	/* Identify and initialize the model */
	if (USBIdentifyModel(pUSB))
		{ perror("identify"); close(fd); free(pUSB); return NULL; }

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
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	if (!pUSB) { errno=EBADF; return 0; }
	return WACOM_MAKEMODEL(pUSB->nVendor,WACOMCLASS_USB,pUSB->uDeviceType);
}

static int USBGetRange(USBTABLET* pUSB, long* pBits, int nAbsField,
		unsigned int uField)
{
	int nAbs[5];

	if  (!ISBITSET(pBits,nAbsField))
		return 0;
	if (ioctl(pUSB->fd, EVIOCGABS(nAbsField), nAbs) != 0)
		return 1;

	pUSB->state.values[uField].nMin = nAbs[1];
	pUSB->state.values[uField].nMax = nAbs[2];
	pUSB->state.uValid |= BIT(uField);
	return 0;
}

static int USBIdentifyModel(USBTABLET* pUSB)
{
	int nCnt;
	short sID[4];
	USB_MODEL_INFO* pInfo = NULL;
	unsigned long evbits[NBITS(EV_MAX)];
	unsigned long absbits[NBITS(ABS_MAX)];
	unsigned long relbits[NBITS(REL_MAX)];
	unsigned long keybits[NBITS(KEY_MAX)];

	/* Get device name and id */
	if (ioctl(pUSB->fd,EVIOCGNAME(sizeof(pUSB->chName)),
		pUSB->chName) < 0) return 1;
	if (ioctl(pUSB->fd,EVIOCGID,sID) < 0)
		return 1;

	/* initialize state structure */
	pUSB->state.uValueCnt = WACOMFIELD_MAX;

	/* Get event types supported */
	nCnt = ioctl(pUSB->fd,EVIOCGBIT(0 /*EV*/,sizeof(evbits)),evbits);
	if (nCnt < 0) { perror("Failed to CGBIT ev"); return 1; }
	assert(nCnt == sizeof(evbits));

	/* absolute events */
	if (ISBITSET(evbits,EV_ABS))
	{
		nCnt = ioctl(pUSB->fd,EVIOCGBIT(EV_ABS,sizeof(absbits)),absbits);
		if (nCnt < 0) { perror("Failed to CGBIT abs"); return 1; }
		assert(nCnt == sizeof(absbits));

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
		if (nCnt < 0) { perror("Failed to CGBIT rel"); return 1; }
		assert(nCnt == sizeof(relbits));

		if (ISBITSET(relbits,REL_WHEEL))
		{
			pUSB->state.uValid |= BIT(WACOMFIELD_RELWHEEL);
			pUSB->state.values[WACOMFIELD_RELWHEEL].nMin = -1;
			pUSB->state.values[WACOMFIELD_RELWHEEL].nMax = 1;
		}
	}

	/* key events */
	if (ISBITSET(evbits,EV_KEY))
	{
		nCnt = ioctl(pUSB->fd,EVIOCGBIT(EV_KEY,sizeof(keybits)),keybits);
		if (nCnt < 0) { perror("Failed to CGBIT key"); return 1; }
		assert(nCnt == sizeof(keybits));

		/* button events */
		if (ISBITSET(keybits,BTN_LEFT) ||
				ISBITSET(keybits,BTN_RIGHT) ||
				ISBITSET(keybits,BTN_MIDDLE) ||
				ISBITSET(keybits,BTN_SIDE) ||
				ISBITSET(keybits,BTN_EXTRA))
			pUSB->state.uValid |= BIT(WACOMFIELD_BUTTONS);

		/* tool events */
		if (ISBITSET(keybits,BTN_TOOL_PEN) ||
				ISBITSET(keybits,BTN_TOOL_RUBBER) ||
				ISBITSET(keybits,BTN_TOOL_BRUSH) ||
				ISBITSET(keybits,BTN_TOOL_PENCIL) ||
				ISBITSET(keybits,BTN_TOOL_AIRBRUSH) ||
				ISBITSET(keybits,BTN_TOOL_FINGER) ||
				ISBITSET(keybits,BTN_TOOL_MOUSE) ||
				ISBITSET(keybits,BTN_TOOL_LENS))
			pUSB->state.uValid |= BIT(WACOMFIELD_PROXIMITY) |
					BIT(WACOMFIELD_TOOLTYPE);
	}

	/* set identification */
	pUSB->nBus = sID[0];
	pUSB->nVendor = sID[1];
	pUSB->nProduct = sID[2];
	pUSB->nVersion = sID[3];

	/* set vendor */
	if (pUSB->nVendor == WACOMVENDOR_WACOM)
	{
		pUSB->pszVendorName = "Wacom";
		pInfo = xWacomModels;
	}
	else
	{
		pUSB->pszVendorName = "Unknown";
	}

	/* find device in vendor's model table */
	for (; pInfo && pInfo->uDeviceType; ++pInfo)
	{
		if (pUSB->nProduct == pInfo->nProduct)
		{
			pUSB->uDeviceType = pInfo->uDeviceType;
			break;
		}
	}

	/* add additional capabilities by device type */
	switch (pUSB->uDeviceType)
	{
		case WACOMDEVICE_INTUOS:
		case WACOMDEVICE_INTUOS2:
			pUSB->state.uValid |= BIT(WACOMFIELD_SERIAL);
		default: ;
	}

	return 0;
}

static const char* USBGetVendorName(WACOMTABLET_PRIV* pTablet)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	return pUSB->pszVendorName;
}

static const char* USBGetModelName(WACOMTABLET_PRIV* pTablet)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	return pUSB->chName;
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
	return pUSB->state.uValid;
}

static int USBGetState(WACOMTABLET_PRIV* pTablet, WACOMSTATE* pState)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	return WacomCopyState(pState,&pUSB->state);
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
	if (pEv->code == MSC_SERIAL)
		pUSB->state.values[WACOMFIELD_SERIAL].nValue = pEv->value;
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
		case BTN_TOOL_FINGER: break;
		case BTN_TOOL_LENS: tool = WACOMTOOLTYPE_LENS; break;
	}

	/* if button was specified */
	if (button != -1)
	{
		/* button state change */
		if (pEv->value)
			pUSB->state.values[WACOMFIELD_BUTTONS].nValue |= BIT(button);
		else
			pUSB->state.values[WACOMFIELD_BUTTONS].nValue &= ~BIT(button);
	}

	/* if a tool was specified */
	if (tool)
	{
		/* coming into proximity */
		if (pEv->value)
		{
			/* no prior tool in proximity */
			if (!pUSB->nToolProx)
			{
				pUSB->state.values[WACOMFIELD_PROXIMITY].nValue = 1;
				pUSB->state.values[WACOMFIELD_TOOLTYPE].nValue = tool;
			}

			/* remember tool in prox */
			pUSB->nToolProx |= BIT(tool);
		}

		/* otherwise, going out of proximity */
		else
		{
			/* forget tool in prox */
			pUSB->nToolProx &= ~BIT(tool);

			/* nobody left? out of proximity */
			if (!pUSB->nToolProx)
				memset(pUSB->state.values, 0,
						pUSB->state.uValueCnt * sizeof(WACOMVALUE));

			/* otherwise, switch to next tool */
			else
			{
				for (i=WACOMTOOLTYPE_PEN; i<WACOMTOOLTYPE_MAX; ++i)
				{
					if (pUSB->nToolProx & BIT(i))
					{
						pUSB->state.values[WACOMFIELD_TOOLTYPE].nValue = i;
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
		pUSB->state.values[field].nValue = pEv->value;

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
		pUSB->state.values[field].nValue = pEv->value;

	return 0;
}

static int USBParseData(WACOMTABLET_PRIV* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState)
{
	USBTABLET* pUSB = (USBTABLET*)pTablet;
	struct input_event* pEv = (struct input_event*)puchData;
	if (uLength != sizeof(struct input_event)) return 1;

	pUSB->state.values[WACOMFIELD_RELWHEEL].nValue = 0;

	/* dispatch event */
	switch (pEv->type)
	{
		case EV_MSC: if (USBParseMSC(pUSB,pEv)) return 1; break;
		case EV_KEY: if (USBParseKEY(pUSB,pEv)) return 1; break;
		case EV_ABS: if (USBParseABS(pUSB,pEv)) return 1; break;
		case EV_REL: if (USBParseREL(pUSB,pEv)) return 1; break;
	}

	return pState ? WacomCopyState(pState,&pUSB->state) : 0;
}

/*****************************************************************************
** End USB Linux Input Subsystem
*****************************************************************************/

#else /* WAC_ENABLE_LINUXWACOM */

WACOMTABLET WacomOpenUSBTablet(int fd)
{
	errno = EPERM;
	return NULL;
}

#endif /* WAC_ENABLE_LINUXWACOM */
