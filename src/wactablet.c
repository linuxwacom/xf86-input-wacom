/*****************************************************************************
** wactablet.c
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

#include "wactablet.h"
#include "wacserial.h"
#include "wacusb.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WAC_ENABLE_LINUXINPUT
#include <linux/input.h>
#endif

typedef void (*FREEFUNC)(void* pv);

typedef struct
{
	FREEFUNC pfnFree;
} CLSLIST_INTERNAL;

typedef struct
{
	WACOMDEVICEREC* pSerialList;
	WACOMDEVICEREC* pUSBList;
	FREEFUNC pfnFree;
} DEVICELIST_INTERNAL;


/*****************************************************************************
** Implementation
*****************************************************************************/

static void FreeClassList(void* pv)
{
	CLSLIST_INTERNAL* pInt = ((CLSLIST_INTERNAL*)pv) - 1;
	free(pInt);
}

int WacomGetSupportedClassList(WACOMCLASSREC** ppList, int* pnSize)
{
	int nIndex=0, nCnt=0;
	CLSLIST_INTERNAL* pInt;
	WACOMCLASSREC* pRec;

	if (!ppList || !pnSize) { errno = EINVAL; return 1; }

	/* serial */
	++nCnt;

	/* USB */
	#ifdef WAC_ENABLE_LINUXINPUT
	++nCnt;
	#endif

	/* allocate enough memory to hold internal structure and all records */
	pInt = (CLSLIST_INTERNAL*)malloc(sizeof(CLSLIST_INTERNAL) +
					(sizeof(WACOMCLASSREC) * nCnt));

	pInt->pfnFree = FreeClassList;
	pRec = (WACOMCLASSREC*)(pInt + 1);

	/* serial */
	pRec[nIndex].pszName = "serial";
	pRec[nIndex].pszDesc = "Serial TTY interface";
	pRec[nIndex].uDeviceClass = WACOMCLASS_SERIAL;
	++nIndex;

	/* USB */
	#ifdef WAC_ENABLE_LINUXINPUT
	pRec[nIndex].pszName = "usb";
	pRec[nIndex].pszDesc = "Linux USB event interface";
	pRec[nIndex].uDeviceClass = WACOMCLASS_USB;
	++nIndex;
	#endif

	assert(nIndex == nCnt);
	*ppList = pRec;
	*pnSize = nCnt;
	return 0;
}

static void FreeDeviceList(void* pv)
{
	DEVICELIST_INTERNAL* pInt = ((DEVICELIST_INTERNAL*)pv) - 1;
	WacomFreeList(pInt->pSerialList);
	WacomFreeList(pInt->pUSBList);
	free(pInt);
}

int WacomGetSupportedDeviceList(unsigned int uDeviceClass,
		WACOMDEVICEREC** ppList, int* pnSize)
{
	int nSerialCnt=0, nUSBCnt=0, nTotalBytes;
	WACOMDEVICEREC* pSerial=NULL, *pUSB=NULL, *pList;
	DEVICELIST_INTERNAL* pInt;

	if (!ppList || !pnSize) { errno = EINVAL; return 1; }

	/* get serial list */
	if (((!uDeviceClass) || (uDeviceClass == WACOMCLASS_SERIAL)) &&
		WacomGetSupportedSerialDeviceList(&pSerial, &nSerialCnt)) return 1;

	/* get usb list */
	if (((!uDeviceClass) || (uDeviceClass == WACOMCLASS_USB)) &&
		WacomGetSupportedUSBDeviceList(&pUSB, &nUSBCnt))
	{
		if (pSerial) WacomFreeList(pSerial);
		return 1;
	}

	/* need memory for duplicate records and list internal structure */
	nTotalBytes = sizeof(WACOMDEVICEREC) * (nSerialCnt + nUSBCnt) +
			sizeof(DEVICELIST_INTERNAL);

	/* allocate memory */
	pInt = (DEVICELIST_INTERNAL*)malloc(nTotalBytes);

	/* copy initial list pointers */
	pInt->pSerialList = pSerial;
	pInt->pUSBList = pUSB;
	pInt->pfnFree = FreeDeviceList;

	/* copy records */
	pList = (WACOMDEVICEREC*)(pInt + 1);
	if (pSerial)
		memcpy(pList,pSerial,sizeof(WACOMDEVICEREC) * nSerialCnt);
	if (pUSB)
		memcpy(pList + nSerialCnt, pUSB, sizeof(WACOMDEVICEREC) * nUSBCnt);

	*ppList = pList;
	*pnSize = nSerialCnt + nUSBCnt;

	return 0;
}

void WacomFreeList(void* pvList)
{
	FREEFUNC pfnFree;
	if (!pvList) return;
	pfnFree = ((FREEFUNC*)pvList)[-1];
	(*pfnFree)(pvList);
}

unsigned int WacomGetClassFromName(const char* pszName)
{
	if (strcasecmp(pszName, "serial") == 0)
		return WACOMCLASS_SERIAL;
	else if (strcasecmp(pszName, "usb") == 0)
		return WACOMCLASS_USB;
	return 0;
}

unsigned int WacomGetDeviceFromName(const char* pszName,
		unsigned int uDeviceClass)
{
	unsigned int uDeviceType = 0;

	if (!uDeviceClass || (uDeviceClass == WACOMCLASS_SERIAL))
	{
		uDeviceType = WacomGetSerialDeviceFromName(pszName);
		if (uDeviceType) return uDeviceType;
	}

	if (!uDeviceClass || (uDeviceClass == WACOMCLASS_USB))
	{
		uDeviceType = WacomGetUSBDeviceFromName(pszName);
		if (uDeviceType) return uDeviceType;
	}

	errno = ENOENT;
	return 0;
}

static int WacomIsSerial(int fd)
{
	return isatty(fd);
}

static int WacomIsUSB(int fd)
{
#ifdef WAC_ENABLE_LINUXINPUT
	short sID[4];
	if (ioctl(fd,EVIOCGID,sID) < 0) return 0;
	return 1;
#else
	return 0;
#endif
}

WACOMTABLET WacomOpenTablet(const char* pszDevice, WACOMMODEL* pModel)
{
	int fd, e;
	WACOMTABLET hTablet = NULL;
	unsigned int uClass = pModel ? pModel->uClass : 0;

	/* open device for read/write access */
	fd = open(pszDevice,O_RDWR);
	if (fd < 0) 
		{ perror("open"); return NULL; }

	/* configure serial */
	if ((!uClass || (uClass == WACOMCLASS_SERIAL)) && WacomIsSerial(fd))
	{
		hTablet = WacomOpenSerialTablet(fd,pModel);
		if (!hTablet) { e=errno; close(fd); errno=e; return NULL; }
	}

	/* configure usb */
	else if ((!uClass || (uClass == WACOMCLASS_USB)) && WacomIsUSB(fd))
	{
		hTablet = WacomOpenUSBTablet(fd,pModel);
		if (!hTablet) { e=errno; close(fd); errno=e; return NULL; }
	}

	/* give up */
	else
	{
		close(fd);
		errno = EINVAL;
		return NULL;
	}

	return hTablet;
}

int WacomCopyState(WACOMSTATE* pDest, WACOMSTATE* pSrc)
{
	unsigned int uCnt;
	if (!pSrc || !pDest || !pSrc->uValueCnt || !pDest->uValueCnt)
		{ errno=EINVAL; return 1; }

	/* copy valid bits over */
	pDest->uValid = pSrc->uValid;

	/* determine how many values to transfer */
	uCnt = (pDest->uValueCnt < pSrc->uValueCnt) ?
			pDest->uValueCnt : pSrc->uValueCnt;

	/* copy them over */
	memcpy(pDest->values,pSrc->values,uCnt*sizeof(WACOMVALUE));

	return 0;
}

/*****************************************************************************
** Virtual Functions
*****************************************************************************/

void WacomCloseTablet(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->Close) return;
	pTablet->Close(pTablet);
}

WACOMMODEL WacomGetModel(WACOMTABLET hTablet)
{
	WACOMMODEL xBadModel = { 0 };
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetModel) { errno=EBADF; return xBadModel; }
	return pTablet->GetModel(pTablet);
}

const char* WacomGetVendorName(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetVendorName) { errno=EBADF; return NULL; }
	return pTablet->GetVendorName(pTablet);
}

const char* WacomGetClassName(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetClassName) { errno=EBADF; return NULL; }
	return pTablet->GetClassName(pTablet);
}

const char* WacomGetDeviceName(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetDeviceName) { errno=EBADF; return NULL; }
	return pTablet->GetDeviceName(pTablet);
}

const char* WacomGetSubTypeName(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetSubTypeName) { errno=EBADF; return NULL; }
	return pTablet->GetSubTypeName(pTablet);
}

const char* WacomGetModelName(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetModelName) { errno=EBADF; return NULL; }
	return pTablet->GetModelName(pTablet);
}

int WacomGetROMVersion(WACOMTABLET hTablet, int* pnMajor, int* pnMinor,
		int* pnRelease)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetROMVer) { errno=EBADF; return 0; }
	return pTablet->GetROMVer(pTablet,pnMajor,pnMinor,pnRelease);
}

int WacomGetCapabilities(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetCaps) { errno=EBADF; return 0; }
	return pTablet->GetCaps(pTablet);
}

int WacomGetState(WACOMTABLET hTablet, WACOMSTATE* pState)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetState) { errno=EBADF; return 0; }
	return pTablet->GetState(pTablet,pState);
}

int WacomGetFileDescriptor(WACOMTABLET hTablet)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->GetFD) { errno=EBADF; return -1; }
	return pTablet->GetFD(pTablet);
}

int WacomReadRaw(WACOMTABLET hTablet, unsigned char* puchData,
		unsigned int uSize)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->ReadRaw) { errno=EBADF; return 0; }
	return pTablet->ReadRaw(pTablet,puchData,uSize);
}

int WacomParseData(WACOMTABLET hTablet, const unsigned char* puchData,
		unsigned int uLength, WACOMSTATE* pState)
{
	WACOMTABLET_PRIV* pTablet = (WACOMTABLET_PRIV*)hTablet;
	if (!pTablet || !pTablet->ParseData) { errno=EBADF; return 0; }
	return pTablet->ParseData(pTablet,puchData,uLength,pState);
}
