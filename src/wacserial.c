/*****************************************************************************
** wacserial.c
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

#include "wacserial.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <ctype.h>
#include <unistd.h>
#include <assert.h>

/*****************************************************************************
** Serial Tablet Object
*****************************************************************************/

typedef struct _SERIALTABLET SERIALTABLET;
typedef struct _SERIALSUBTYPE SERIALSUBTYPE;
typedef struct _SERIALDEVICE SERIALDEVICE;
typedef struct _SERIALVENDOR SERIALVENDOR;

typedef int (*IDENTFUNC)(SERIALTABLET* pSerial);
typedef int (*INITFUNC)(SERIALTABLET* pSerial);
typedef int (*PARSEFUNC)(SERIALTABLET* pSerial, const unsigned char* puchData,
		unsigned int uLength, WACOMSTATE* pState);

struct _SERIALTABLET
{
	WACOMTABLET_PRIV tablet;
	int fd;
	SERIALVENDOR* pVendor;
	SERIALDEVICE* pDevice;
	SERIALSUBTYPE* pSubType;
	unsigned int uPacketLength;
	int nVerMajor, nVerMinor, nVerRelease;
	IDENTFUNC pfnIdent;
	INITFUNC pfnInit;
	PARSEFUNC pfnParse;
	int nToolID;
	WACOMSTATE state;
};

/*****************************************************************************
** Internal structures
*****************************************************************************/

struct _SERIALSUBTYPE
{
	const char* pszName;
	const char* pszDesc;
	unsigned int uSubType;
	const char* pszIdent;
	INITFUNC pfnInit;
};

struct _SERIALDEVICE
{
	const char* pszName;
	const char* pszDesc;
	unsigned int uDevice;
	SERIALSUBTYPE* pSubTypes;
	int nProtocol;
	unsigned int uPacketLength;
	unsigned int uCaps;
	int nMinBaudRate;
	IDENTFUNC pfnIdent;
};

struct _SERIALVENDOR
{
	const char* pszName;
	const char* pszDesc;
	unsigned int uVendor;
	SERIALDEVICE* pDevices;
};

/*****************************************************************************
** Static operations
*****************************************************************************/

static void SerialClose(WACOMTABLET_PRIV* pTablet);
static WACOMMODEL SerialGetModel(WACOMTABLET_PRIV* pTablet);
static const char* SerialGetVendorName(WACOMTABLET_PRIV* pTablet);
static const char* SerialGetClassName(WACOMTABLET_PRIV* pTablet);
static const char* SerialGetDeviceName(WACOMTABLET_PRIV* pTablet);
static const char* SerialGetSubTypeName(WACOMTABLET_PRIV* pTablet);
static const char* SerialGetModelName(WACOMTABLET_PRIV* pTablet);
static int SerialGetROMVer(WACOMTABLET_PRIV* pTablet, int* pnMajor,
		int* pnMinor, int* pnRelease);
static int SerialGetCaps(WACOMTABLET_PRIV* pTablet);
static int SerialGetState(WACOMTABLET_PRIV* pTablet, WACOMSTATE* pState);
static int SerialGetFD(WACOMTABLET_PRIV* pTablet);
static int SerialReadRaw(WACOMTABLET_PRIV* pTablet, unsigned char* puchData,
		unsigned int uSize);
static int SerialParseData(WACOMTABLET_PRIV* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);

int SerialConfigTTY(SERIALTABLET* pSerial);
static int SerialResetAtBaud(SERIALTABLET* pSerial, struct termios* pTIOS,
		int nBaud);
static int SerialSetDevice(SERIALTABLET* pSerial, SERIALVENDOR* pVendor,
		SERIALDEVICE* pDevice, SERIALSUBTYPE* pSubType);

static int SerialIdentDefault(SERIALTABLET* pSerial);
static int SerialIdentAcerC100(SERIALTABLET* pSerial);
static int SerialInitAcerC100(SERIALTABLET* pSerial);
static int SerialIdentWacom(SERIALTABLET* pSerial);
static int SerialInitWacom(SERIALTABLET* pSerial);

static int SerialParseWacomV(SERIALTABLET* pSerial,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);
static int SerialParseWacomIV_1_4(SERIALTABLET* pSerial,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);
static int SerialParseWacomIV_1_3(SERIALTABLET* pSerial,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);
static int SerialParseWacomIV_1_2(SERIALTABLET* pSerial,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);
static int SerialParseAcerC100(SERIALTABLET* pSerial,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);

/*****************************************************************************
** Defines
*****************************************************************************/

#ifndef BIT
#undef BIT
#define BIT(x) (1<<(x))
#endif

#define WACOMVALID(x) BIT(WACOMFIELD_##x)

#define ARTPAD_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(PROXIMITY)| \
		WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)|WACOMVALID(POSITION_Y)| \
		WACOMVALID(PRESSURE))

#define ARTPADII_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(PROXIMITY)| \
		WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)|WACOMVALID(POSITION_Y)| \
		WACOMVALID(PRESSURE))

#define DIGITIZER_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(PROXIMITY)| \
		WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)|WACOMVALID(POSITION_Y)| \
		WACOMVALID(PRESSURE)|WACOMVALID(TILT_X)|WACOMVALID(TILT_Y))

#define DIGITIZERII_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(PROXIMITY)| \
		WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)|WACOMVALID(POSITION_Y)| \
		WACOMVALID(PRESSURE)|WACOMVALID(TILT_X)|WACOMVALID(TILT_Y))

#define PENPARTNER_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(PROXIMITY)| \
		WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)|WACOMVALID(POSITION_Y)| \
		WACOMVALID(PRESSURE))

#define GRAPHIRE_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(PROXIMITY)| \
		WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)|WACOMVALID(POSITION_Y)| \
		WACOMVALID(PRESSURE)|WACOMVALID(TILT_X)|WACOMVALID(TILT_Y))

#define CINTIQ_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(PROXIMITY)| \
		WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)|WACOMVALID(POSITION_Y)| \
		WACOMVALID(PRESSURE)|WACOMVALID(TILT_X)|WACOMVALID(TILT_Y))

#define INTUOS_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(SERIAL)| \
		WACOMVALID(PROXIMITY)|WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)| \
		WACOMVALID(POSITION_Y)|WACOMVALID(ROTATION_Z)|WACOMVALID(DISTANCE)| \
		WACOMVALID(PRESSURE)|WACOMVALID(TILT_X)|WACOMVALID(TILT_Y)| \
		WACOMVALID(ABSWHEEL)|WACOMVALID(RELWHEEL)|WACOMVALID(THROTTLE))

#define ACERC100_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(SERIAL)| \
		WACOMVALID(PROXIMITY)|WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)| \
		WACOMVALID(POSITION_Y)|WACOMVALID(PRESSURE))

#define INTUOS2_CAPS INTUOS_CAPS

#define PROTOCOL_4 4
#define PROTOCOL_5 5

#define WACOM_SUBTYPE(id,d,s) \
	{ id, d, s, id, SerialInitWacom }
#define ACER_SUBTYPE(id,d,s) \
	{ id, d, s, id, SerialInitAcerC100 }

#define WACOM_DEVICE_P4(n,d,i,s,c) \
	{ n, d, i, s, PROTOCOL_4, 7, c, 9600, SerialIdentWacom }
#define WACOM_DEVICE_P5(n,d,i,s,c) \
	{ n, d, i, s, PROTOCOL_5, 9, c, 9600, SerialIdentWacom }
#define ACER_DEVICE(n,d,i,s,c) \
	{ n, d, i, s, 0, 9, c, 19200, SerialIdentAcerC100 }

/*****************************************************************************
** Globals
*****************************************************************************/

	static SERIALSUBTYPE xArtPadII[] =
	{
		WACOM_SUBTYPE("KT-0405-R", "Wacom ArtPadII 4x5", 1),
		{ NULL }
	};

	static SERIALSUBTYPE xDigitizerII[] =
	{
		WACOM_SUBTYPE("UD-0608-R", "Wacom DigitizerII 6x8",   1),
		WACOM_SUBTYPE("UD-1212-R", "Wacom DigitizerII 12x12", 2),
		WACOM_SUBTYPE("UD-1218-R", "Wacom DigitizerII 12x18", 3),
		WACOM_SUBTYPE("UD-1825-R", "Wacom DigitizerII 18x25", 4),
		{ NULL }
	};

	static SERIALSUBTYPE xPenPartner[] =
	{
		WACOM_SUBTYPE("CT-0405-R", "Wacom PenPartner", 1),
		{ NULL }
	};

	static SERIALSUBTYPE xGraphire[] =
	{
		WACOM_SUBTYPE("ET-0405-R", "Wacom Graphire", 1),
		{ NULL }
	};

	static SERIALSUBTYPE xIntuos[] =
	{
		WACOM_SUBTYPE("GD-0405-R", "Wacom Intuos 4x5",   1),
		WACOM_SUBTYPE("GD-0608-R", "Wacom Intuos 6x8",   2),
		WACOM_SUBTYPE("GD-0912-R", "Wacom Intuos 9x12",  3),
		WACOM_SUBTYPE("GD-1212-R", "Wacom Intuos 12x12", 4),
		WACOM_SUBTYPE("GD-1218-R", "Wacom Intuos 12x18", 5),
		{ NULL }
	};

	static SERIALSUBTYPE xIntuos2[] =
	{
		WACOM_SUBTYPE("XD-0405-R", "Wacom Intuos2 4x5",   1),
		WACOM_SUBTYPE("XD-0608-R", "Wacom Intuos2 6x8",   2),
		WACOM_SUBTYPE("XD-0912-R", "Wacom Intuos2 9x12",  3),
		WACOM_SUBTYPE("XD-1212-R", "Wacom Intuos2 12x12", 4),
		WACOM_SUBTYPE("XD-1218-R", "Wacom Intuos2 12x18", 5),
		{ NULL }
	};

	static SERIALSUBTYPE xCintiq[] =
	{
		{ NULL }
	};

	static SERIALDEVICE xWacomDevices[] =
	{
		WACOM_DEVICE_P4("art", "ArtPad", WACOMDEVICE_ARTPAD,
				NULL, ARTPAD_CAPS),
		WACOM_DEVICE_P4("art2", "ArtPadII", WACOMDEVICE_ARTPADII,
				xArtPadII, ARTPADII_CAPS),
		WACOM_DEVICE_P4("dig", "Digitizer", WACOMDEVICE_DIGITIZER,
				NULL, DIGITIZERII_CAPS),
		WACOM_DEVICE_P4("dig2", "Digitizer II", WACOMDEVICE_DIGITIZERII,
				xDigitizerII, DIGITIZERII_CAPS),
		WACOM_DEVICE_P4("pp", "PenPartner", WACOMDEVICE_PENPARTNER,
				xPenPartner, PENPARTNER_CAPS),
		WACOM_DEVICE_P4("gr", "Graphire", WACOMDEVICE_GRAPHIRE,
				xGraphire, GRAPHIRE_CAPS),
		WACOM_DEVICE_P4("pl", "Cintiq (PL)", WACOMDEVICE_CINTIQ,
				xCintiq, CINTIQ_CAPS),
		WACOM_DEVICE_P5("int", "Intuos", WACOMDEVICE_INTUOS,
				xIntuos, INTUOS_CAPS),
		WACOM_DEVICE_P5("int2", "Intuos2", WACOMDEVICE_INTUOS2,
				xIntuos2, INTUOS2_CAPS),
		{ NULL }
	};

	/* This one is reverse engineered at this point */
	static SERIALSUBTYPE xAcerC100[] =
	{
		ACER_SUBTYPE("C100", "Acer C100 Tablet PC Screen", 1),
		{ NULL }
	};

	static SERIALDEVICE xAcerDevices[] =
	{
		ACER_DEVICE("c100", "C100", WACOMDEVICE_ACERC100,
				xAcerC100, ACERC100_CAPS),
		{ NULL }
	};

	static SERIALVENDOR xWacomVendor =
	{ "wacom", "Wacom", WACOMVENDOR_WACOM, xWacomDevices };

	static SERIALVENDOR xAcerVendor =
	{ "acer", "Acer", WACOMVENDOR_ACER, xAcerDevices };

	static SERIALVENDOR* xVendors[] =
	{
		&xWacomVendor,
		&xAcerVendor,
		NULL
	};

/*****************************************************************************
** Static Prototypes
*****************************************************************************/

static int SerialSendReset(SERIALTABLET* pSerial);
static int SerialSendStop(SERIALTABLET* pSerial);
static int SerialSendStart(SERIALTABLET* pSerial);

static int SerialSend(SERIALTABLET* pSerial, const char* pszData);
static int SerialSendRaw(SERIALTABLET* pSerial, const unsigned char* puchData,
		unsigned int uSize);
static int WacomFlush(SERIALTABLET* pSerial);
static int SerialSendRequest(SERIALTABLET* pSerial, const char* pszRequest,
		char* pchResponse, unsigned int uSize);

/*****************************************************************************
** Public Functions
*****************************************************************************/

typedef struct
{
	void (*pfnFree)(void* pv);
} DEVLIST_INTERNAL;

static void SerialFreeDeviceList(void* pv)
{
	DEVLIST_INTERNAL* pInt = ((DEVLIST_INTERNAL*)pv) - 1;
	free(pInt);
}

int WacomGetSupportedSerialDeviceList(WACOMDEVICEREC** ppList, int* pnSize)
{
	int nIndex=0, nCnt=0;
	DEVLIST_INTERNAL* pInt;
	SERIALDEVICE* pDev;
	SERIALVENDOR** ppVendor;
	WACOMDEVICEREC* pRec;

	if (!ppList || !pnSize) { errno = EINVAL; return 1; }

	/* for each vendor, count up devices */
	for (ppVendor=xVendors; *ppVendor; ++ppVendor)
	{
		/* count up devices */
		for (pDev=(*ppVendor)->pDevices; pDev->pszName; ++pDev, ++nCnt) ;
	}
   
	/* allocate enough memory to hold internal structure and all records */
	pInt = (DEVLIST_INTERNAL*)malloc(sizeof(DEVLIST_INTERNAL) +
					(sizeof(WACOMDEVICEREC) * nCnt));

	pInt->pfnFree = SerialFreeDeviceList;
	pRec = (WACOMDEVICEREC*)(pInt + 1);

	/* for each vendor, add devices */
	for (ppVendor=xVendors; *ppVendor; ++ppVendor)
	{
		for (pDev=(*ppVendor)->pDevices; pDev->pszName; ++pDev, ++nIndex)
		{
			pRec[nIndex].pszName = pDev->pszName;
			pRec[nIndex].pszDesc = pDev->pszDesc;
			pRec[nIndex].pszVendorName = (*ppVendor)->pszName;
			pRec[nIndex].pszVendorDesc = (*ppVendor)->pszDesc;
			pRec[nIndex].pszClass = "serial";
			pRec[nIndex].model.uClass = WACOMCLASS_SERIAL;
			pRec[nIndex].model.uVendor = (*ppVendor)->uVendor;
			pRec[nIndex].model.uDevice = pDev->uDevice;
			pRec[nIndex].model.uSubType = 0;
		}
	}
	assert(nIndex == nCnt);

	*ppList = pRec;
	*pnSize = nCnt;
	return 0;
}

unsigned int WacomGetSerialDeviceFromName(const char* pszName)
{
	SERIALDEVICE* pDev;
	SERIALVENDOR** ppVendor;

	if (!pszName) { errno = EINVAL; return 0; }

	/* for each vendor, look for device */
	for (ppVendor=xVendors; *ppVendor; ++ppVendor)
	{
		/* count up devices */
		for (pDev=(*ppVendor)->pDevices; pDev->pszName; ++pDev)
		{
			if (strcasecmp(pszName,pDev->pszName) == 0)
				return pDev->uDevice;
		}
	}

	errno = ENOENT;
	return 0;
}

static int SerialFindModel(WACOMMODEL* pModel, SERIALVENDOR** ppVendor,
		SERIALDEVICE** ppDevice, SERIALSUBTYPE** ppSubType)
{
	SERIALVENDOR** ppPos;
	SERIALDEVICE* pDev;
	SERIALSUBTYPE* pSub;

	/* device type must be specified */
	if (!pModel)
		{ errno = EINVAL; return 1; }

	/* no device specified, nothing found. */
	if (!pModel->uDevice)
	{
		*ppVendor = NULL;
		*ppDevice = NULL;
		*ppSubType = NULL;
		return 0;
	}

	/* for each vendor */
	for (ppPos=xVendors; *ppPos; ++ppPos)
	{
		/* check vendor */
		if (!pModel->uVendor || (pModel->uVendor == (*ppPos)->uVendor))
		{
			/* for each device */
			for (pDev=(*ppPos)->pDevices; pDev->pszName; ++pDev)
			{
				/* if device matches */
				if (pModel->uDevice == pDev->uDevice)
				{
					/* no subtype specified, use it */
					if (!pModel->uSubType)
					{
						*ppVendor = *ppPos;
						*ppDevice = pDev;
						*ppSubType = NULL;
						return 0;
					}
					
					/* for each subtype */
					for (pSub=pDev->pSubTypes; pSub->pszName; ++pSub)
					{
						/* if subtype matches */
						if (pModel->uSubType == pSub->uSubType)
						{
							*ppVendor = *ppPos;
							*ppDevice = pDev;
							*ppSubType = pSub;
							return 0;
						}
					}

					/* wrong subtype? maybe try another vendor */
					if (!pModel->uVendor) break;

					/* otherwise, no match. */
					errno = ENOENT;
					return 1;
				}
			} /* next device */

			/* if vendor matches, but device does not, no match. */
			if (pModel->uVendor)
			{
				errno = ENOENT;
				return 1;
			}
		}
	} /* next vendor */

	/* no match */
	errno = ENOENT;
	return 1;
}

WACOMTABLET WacomOpenSerialTablet(int fd, WACOMMODEL* pModel)
{
	SERIALTABLET* pSerial = NULL;
	SERIALVENDOR* pVendor = NULL;
	SERIALDEVICE* pDevice = NULL;
	SERIALSUBTYPE* pSubType = NULL;

	/* If model is specified, break it down into vendor, device, and subtype */
	if (pModel && SerialFindModel(pModel,&pVendor,&pDevice,&pSubType))
		return NULL;

	/* Allocate tablet */
	pSerial = (SERIALTABLET*)malloc(sizeof(SERIALTABLET));
	memset(pSerial,0,sizeof(*pSerial));
	pSerial->tablet.Close = SerialClose;
	pSerial->tablet.GetModel = SerialGetModel;
	pSerial->tablet.GetVendorName = SerialGetVendorName;
	pSerial->tablet.GetClassName = SerialGetClassName;
	pSerial->tablet.GetDeviceName = SerialGetDeviceName;
	pSerial->tablet.GetSubTypeName = SerialGetSubTypeName;
	pSerial->tablet.GetModelName = SerialGetModelName;
	pSerial->tablet.GetROMVer = SerialGetROMVer;
	pSerial->tablet.GetCaps = SerialGetCaps;
	pSerial->tablet.GetState = SerialGetState;
	pSerial->tablet.GetFD = SerialGetFD;
	pSerial->tablet.ReadRaw = SerialReadRaw;
	pSerial->tablet.ParseData = SerialParseData;

	pSerial->fd = fd;
	pSerial->state.uValueCnt = WACOMFIELD_MAX;

	/* Set the tablet device */
	if (SerialSetDevice(pSerial,pVendor,pDevice,pSubType))
		{ free(pSerial); return NULL; }

	/* configure the TTY for initial operation */
	if (SerialConfigTTY(pSerial))
		{ free(pSerial); return NULL; }

	/* Identify the tablet */
	if (!pSerial->pfnIdent || pSerial->pfnIdent(pSerial))
		{ perror("ident"); free(pSerial); return NULL; }

	/* Initialize the tablet */
	if (!pSerial->pfnInit || pSerial->pfnInit(pSerial))
		{ perror("init"); free(pSerial); return NULL; }

	/* Send start */
	SerialSendStart(pSerial);

	return (WACOMTABLET)pSerial;
}

/*****************************************************************************
** Serial Tablet Functions
*****************************************************************************/

int SerialConfigTTY(SERIALTABLET* pSerial)
{
	struct termios tios;
	int nBaudRate = 9600;

	/* configure tty */
	if (isatty(pSerial->fd))
	{
		/* set up default port parameters */
		if (tcgetattr (pSerial->fd, &tios))
			{ perror("tcgetattr"); return 1; }

		tios.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
		tios.c_oflag &= ~OPOST;
		tios.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
		tios.c_cflag &= ~(CSIZE|PARENB);
		tios.c_cflag |= CS8|CLOCAL;
		tios.c_cflag &= ~(CSTOPB); /* 1 stop bit */
		tios.c_cflag &= ~(CSIZE); /* 8 data bits */
		tios.c_cflag |= CS8;
    	tios.c_cflag &= ~(PARENB); /* no parity */
		tios.c_iflag |= IXOFF;		/* flow control XOff */
		tios.c_cc[VMIN] = 1;		/* vmin value */
		tios.c_cc[VTIME] = 0;		/* vtime value */

		if (tcsetattr (pSerial->fd, TCSANOW, &tios))
			{ perror("tcsetattr"); return 1; }

		/* get minumum baud rate for given device, if specified */
		if (pSerial->pDevice)
			nBaudRate = pSerial->pDevice->nMinBaudRate;

		/* set 38400 baud and reset */
		if (SerialResetAtBaud(pSerial,&tios,38400))
			return 1;

		/* if valid, set 19200 baud and reset */
		if ((nBaudRate <= 19200) && (SerialResetAtBaud(pSerial,&tios,19200)))
			return 1;
		
		/* if valid, set 9600 baud and reset */
		if ((nBaudRate <= 9600) && (SerialResetAtBaud(pSerial,&tios,9600)))
			return 1;

		/* lower than 9600 baud? for testing, maybe */
		if ((nBaudRate < 9600) && (SerialResetAtBaud(pSerial,&tios,nBaudRate)))
			return 1;
	}
	else /* not tty */
	{
		if (SerialSendReset(pSerial)) return 1;
	}

	/* Send stop */
	if (SerialSendStop(pSerial) || WacomFlush(pSerial))
		return 1;

	return 0;
}


static int SerialSetDevice(SERIALTABLET* pSerial, SERIALVENDOR* pVendor,
		SERIALDEVICE* pDevice, SERIALSUBTYPE* pSubType)
{
	pSerial->pVendor = pVendor;
	pSerial->pDevice = pDevice;
	pSerial->pSubType = pSubType;

	/* if we know the device, use its functions */
	if (pSerial->pDevice)
	{
		pSerial->pfnIdent = pSerial->pDevice->pfnIdent;
		if (!pSerial->pfnIdent) { errno = EPERM; return 1; }
	}
	else
		pSerial->pfnIdent = SerialIdentDefault;

	return 0;
}

static int SerialIdentDefault(SERIALTABLET* pSerial)
{
	return SerialIdentWacom(pSerial);
}

static int SerialIdentWacom(SERIALTABLET* pSerial)
{
	char* pszPos;
	SERIALVENDOR* pVendor = &xWacomVendor;
	SERIALDEVICE* pDev;
	SERIALSUBTYPE* pSub;
	char chResp[64];

	/* send wacom identification request */
	if (SerialSendRequest(pSerial,"~#\r",chResp,sizeof(chResp)))
	{
		if (errno != ETIMEDOUT) return 1;

		/* try again, sometimes the first one gets garbled */
		if (SerialSendRequest(pSerial,"~#\r",chResp,sizeof(chResp)))
			return 1;
	}

	/* look through device table for information */
	for (pDev=pVendor->pDevices; pDev->pszName; ++pDev)
	{
		for (pSub=pDev->pSubTypes; pSub && pSub->pszName; ++pSub)
		{
			if (strncmp(chResp,pSub->pszIdent, strlen(pSub->pszIdent)) == 0)
			{
				pSerial->pVendor = pVendor;
				pSerial->pDevice = pDev;
				pSerial->pSubType = pSub;
				pSerial->state.uValid = pDev->uCaps;
				pSerial->uPacketLength = pDev->uPacketLength;
				pSerial->pfnInit = pSub->pfnInit ?
						pSub->pfnInit : SerialInitWacom;
	
				/* get version number */
				pszPos = chResp;
				while (*pszPos) ++pszPos;
				while ((pszPos > chResp) && (pszPos[-1] != 'V')) --pszPos;
				if (sscanf(pszPos,"%d.%d-%d",&pSerial->nVerMajor,
						&pSerial->nVerMinor,&pSerial->nVerRelease) != 3)
				{
					pSerial->nVerRelease = 0;
					if (sscanf(pszPos,"%d.%d",&pSerial->nVerMajor,
						&pSerial->nVerMinor) != 2)
					{
						errno = EINVAL;
						fprintf(stderr,"bad version number: %s\n",pszPos);
						return 1;
					}
				}
				return 0;
			}
		}
	}

	fprintf(stderr,"UNIDENTIFIED TABLET: %s\n",chResp);
	return 1;
}

static int SerialInitWacom(SERIALTABLET* pSerial)
{
	char chResp[32];

	/* Request tablet dimensions */
	if (SerialSendRequest(pSerial,"~C\r",chResp,sizeof(chResp)))
		return 1;

	/* parse position range */
	if (sscanf(chResp,"%d,%d",
			&pSerial->state.values[WACOMFIELD_POSITION_X].nMax,
			&pSerial->state.values[WACOMFIELD_POSITION_Y].nMax) != 2)
	{
		errno=EINVAL;
		perror("bad dim response");
		return 1;
	}

	/* tablet specific initialization */
	switch (pSerial->pDevice->uDevice)
	{
		case WACOMDEVICE_PENPARTNER:
			/* pressure mode */
			SerialSend(pSerial, "PH1\r");
			break;

		case WACOMDEVICE_INTUOS:
			/* multi-mode, max-rate */ 
			SerialSend(pSerial, "MT1\rID1\rIT0\r");
	}
	
	if (pSerial->pDevice->nProtocol == PROTOCOL_4)
	{
		/* multi-mode (MU), upper-origin (OC), all-macro (M0),
		 * no-macro1 (M1), max-rate (IT), no-inc (IN),
		 * stream-mode (SR), Z-filter (ZF) */

/*		if (SerialSend(pSerial->fd, "MU1\rOC1\r~M0\r~M1\rIT0\rIN0\rSR\rZF1\r"))
			return 1;
			*/

		if (pSerial->nVerMajor == 1)
		{
			if (pSerial->nVerMinor >= 4)
			{
				/* enable tilt mode */
				if (SerialSend(pSerial,"FM1\r")) return 1;

				pSerial->pfnParse = SerialParseWacomIV_1_4;
				pSerial->uPacketLength = 9;
				pSerial->state.values[WACOMFIELD_PRESSURE].nMax = 255;
				pSerial->state.values[WACOMFIELD_TILT_X].nMin = -64;
				pSerial->state.values[WACOMFIELD_TILT_X].nMax = 63;
				pSerial->state.values[WACOMFIELD_TILT_Y].nMin = -64;
				pSerial->state.values[WACOMFIELD_TILT_Y].nMax = 63;
			}
			else if (pSerial->nVerMinor == 3)
			{
				pSerial->pfnParse = SerialParseWacomIV_1_3;
				pSerial->state.values[WACOMFIELD_PRESSURE].nMax = 255;
			}
			else if (pSerial->nVerMinor == 2)
			{
				pSerial->pfnParse = SerialParseWacomIV_1_2;
				pSerial->state.values[WACOMFIELD_PRESSURE].nMax = 255;
			}
			else if (pSerial->nVerMinor < 2)
			{
				pSerial->pfnParse = SerialParseWacomIV_1_2;
				pSerial->state.values[WACOMFIELD_PRESSURE].nMax = 120;
			}
		}
	}
	else if (pSerial->pDevice->nProtocol == PROTOCOL_5)
	{
		pSerial->pfnParse = SerialParseWacomV;
		pSerial->state.values[WACOMFIELD_PRESSURE].nMax = 1023;
		pSerial->state.values[WACOMFIELD_ABSWHEEL].nMax = 1023;
		pSerial->state.values[WACOMFIELD_ROTATION_Z].nMin = -899;
		pSerial->state.values[WACOMFIELD_ROTATION_Z].nMax = 900;
		pSerial->state.values[WACOMFIELD_THROTTLE].nMin = -1023;
		pSerial->state.values[WACOMFIELD_THROTTLE].nMax = 1023;
		pSerial->state.values[WACOMFIELD_TILT_X].nMin = -64;
		pSerial->state.values[WACOMFIELD_TILT_X].nMax = 63;
		pSerial->state.values[WACOMFIELD_TILT_Y].nMin = -64;
		pSerial->state.values[WACOMFIELD_TILT_Y].nMax = 63;
	}
	else { errno=EINVAL; return 1; }

	return 0;
}


static int SerialIdentAcerC100(SERIALTABLET* pSerial)
{
	/* sanity check */
	if ((pSerial->pVendor != &xAcerVendor) ||
		(pSerial->pDevice == NULL)) { return EPERM; return 1; }

	/* use first one */
	pSerial->pSubType = pSerial->pDevice->pSubTypes;

	/* sanity check again */
	if (pSerial->pSubType->pszName == NULL) { return EPERM; return 1; }

	pSerial->state.uValid = pSerial->pDevice->uCaps;
	pSerial->uPacketLength = pSerial->pDevice->uPacketLength;
	pSerial->pfnInit = pSerial->pSubType->pfnInit;
	pSerial->nVerMajor = 0;
	pSerial->nVerMinor = 0;
	pSerial->nVerRelease = 0;

	return 0;
}

static int SerialInitAcerC100(SERIALTABLET* pSerial)
{
	pSerial->pfnParse = SerialParseAcerC100;
	pSerial->state.values[WACOMFIELD_POSITION_X].nMax = 21136;
	pSerial->state.values[WACOMFIELD_POSITION_Y].nMax = 15900;
	pSerial->state.values[WACOMFIELD_PRESSURE].nMax = 255;
	return 0;
}

static int SerialParseWacomV(SERIALTABLET* pSerial,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState)
{
	int x=0, y=0, rot=0, tiltx=0, tilty=0, wheel=0,
			tool=WACOMTOOLTYPE_NONE, button=0, press=0, throttle=0,
			nButtonValue;

	/* Wacom V
	 * Supports: 1024 pressure, eraser, 2 side-switch, tilt, throttle, wheel
	 * Limitation: no tilt */

	if (uLength != 9) { errno=EINVAL; return 1; }

	/* in */
	if ((puchData[0] & 0xFC) == 0xC0)
	{
		int toolid = (((int)puchData[1]&0x7F) << 5) |
				(((int)puchData[2]&0x7C) >> 2);

		int serial = ((((int)puchData[2] & 0x03) << 30) |
	                (((int)puchData[3] & 0x7f) << 23) |
	                  (((int)puchData[4] & 0x7f) << 16) |
	                  (((int)puchData[5] & 0x7f) << 9) |
	                (((int)puchData[6] & 0x7f) << 23) |
                  (((int)puchData[7] & 0x60) >> 5));

		switch (toolid)
		{
			case 0x812: /* Intuos2 ink pen XP-110-00A */
			case 0x012: /* Inking pen */
				tool = WACOMTOOLTYPE_PENCIL; break;

			case 0x822: /* Intuos Pen GP-300E-01H */
			case 0x852: /* Intuos2 Grip Pen XP-501E-00A */
			case 0x842: /* added from Cheng */
			case 0x022:
				tool = WACOMTOOLTYPE_PEN; break;

			case 0x832: /* Intuos2 stroke pen XP-120-00A */
			case 0x032: /* Stroke pen */
				tool = WACOMTOOLTYPE_BRUSH; break;

			case 0x007: /* 2D Mouse */
			case 0x09C: /* ?? Mouse */
			case 0x094: /* 4D Mouse */
				tool = WACOMTOOLTYPE_MOUSE; break;

			case 0x096: /* Lens cursor */
				tool = WACOMTOOLTYPE_LENS; break;

			case 0x82a:
			case 0x85a:
			case 0x91a:
			case 0x0fa: /* Eraser */
				tool = WACOMTOOLTYPE_ERASER; break;

			case 0x112: /* Airbrush */
				tool = WACOMTOOLTYPE_AIRBRUSH; break;

			default: /* Unknown tool */
				tool = WACOMTOOLTYPE_PEN; break;
		}
			
		pSerial->nToolID = toolid;
		pSerial->state.values[WACOMFIELD_PROXIMITY].nValue = 1;
		pSerial->state.values[WACOMFIELD_SERIAL].nValue = serial;
		pSerial->state.values[WACOMFIELD_TOOLTYPE].nValue = tool;
		return pState ? WacomCopyState(pState,&pSerial->state) : 0;
	}

	/* out */
	if ((puchData[0] & 0xFE) == 0x80)
	{
		pSerial->nToolID = 0;
		memset(&pSerial->state.values, 0,
				pSerial->state.uValueCnt * sizeof(WACOMVALUE));
		return pState ? WacomCopyState(pState,&pSerial->state) : 0;
	}

	/* pen data */
	if (((puchData[0] & 0xB8) == 0xA0) || ((puchData[0] & 0xBE) == 0xB4))
	{
		x = ((((int)puchData[1] & 0x7f) << 9) |
				(((int)puchData[2] & 0x7f) << 2) |
				(((int)puchData[3] & 0x60) >> 5));
		y = ((((int)puchData[3] & 0x1f) << 11) |
				(((int)puchData[4] & 0x7f) << 4) |
				(((int)puchData[5] & 0x78) >> 3));
		tiltx = (puchData[7] & 0x3F);
		tilty = (puchData[8] & 0x3F);
		if (puchData[7] & 0x40) tiltx -= 0x40;
		if (puchData[8] & 0x40) tilty -= 0x40;

		/* pen packet */
		if ((puchData[0] & 0xB8) == 0xA0)
		{
			press = ((((int)puchData[5] & 0x07) << 7) | ((int)puchData[6] & 0x7f));
			button = (press > 10) ? BIT(WACOMBUTTON_TOUCH) : 0;
			button |= (puchData[0] & 0x02) ? BIT(WACOMBUTTON_STYLUS) : 0;
			button |= (puchData[0] & 0x04) ? BIT(WACOMBUTTON_STYLUS2) : 0;
		}

		/* 2nd airbrush packet */
		else
		{
			wheel = ((((int)puchData[5] & 0x07) << 7) |
					((int)puchData[6] & 0x7f));
		}

		pSerial->state.values[WACOMFIELD_POSITION_X].nValue = x;
		pSerial->state.values[WACOMFIELD_POSITION_Y].nValue = y;
		pSerial->state.values[WACOMFIELD_TILT_X].nValue = tiltx;
		pSerial->state.values[WACOMFIELD_TILT_Y].nValue = tilty;
		pSerial->state.values[WACOMFIELD_PRESSURE].nValue = press;
		pSerial->state.values[WACOMFIELD_BUTTONS].nValue = button;
		pSerial->state.values[WACOMFIELD_ABSWHEEL].nValue = wheel;
		return pState ? WacomCopyState(pState,&pSerial->state) : 0;
	}

	/* mouse packet */
	if (((puchData[0] & 0xBE) == 0xA8) || ((puchData[0] & 0xBE) == 0xB0))
	{
		x = ((((int)puchData[1] & 0x7f) << 9) |
				(((int)puchData[2] & 0x7f) << 2) |
				(((int)puchData[3] & 0x60) >> 5));
		y = ((((int)puchData[3] & 0x1f) << 11) |
				(((int)puchData[4] & 0x7f) << 4) |
				(((int)puchData[5] & 0x78) >> 3));
		throttle = ((((int)puchData[5] & 0x07) << 7) | (puchData[6] & 0x7f));
		if (puchData[8] & 0x08) throttle = -throttle;

		/* 4D mouse */
		if (pSerial->nToolID == 0x094)
		{
			button = (((puchData[8] & 0x70) >> 1) | (puchData[8] & 0x07));
		}
		/* lens cursor */
		else if (pSerial->nToolID == 0x096)
		{
			button = puchData[8] & 0x1F;
		}
		/* 2D mouse */
		else
		{
			button = (puchData[8] & 0x1C) >> 2;
			wheel = - (puchData[8] & 1) + ((puchData[8] & 2) >> 1);
		}

		pSerial->state.values[WACOMFIELD_POSITION_X].nValue = x;
		pSerial->state.values[WACOMFIELD_POSITION_Y].nValue = y;
		pSerial->state.values[WACOMFIELD_RELWHEEL].nValue = wheel;
		pSerial->state.values[WACOMFIELD_THROTTLE].nValue = throttle;

		/* button values */
		nButtonValue = pSerial->state.values[WACOMFIELD_BUTTONS].nValue &
				~(BIT(WACOMBUTTON_LEFT) |
				BIT(WACOMBUTTON_RIGHT) | BIT(WACOMBUTTON_MIDDLE) |
				BIT(WACOMBUTTON_EXTRA) | BIT(WACOMBUTTON_SIDE));
		if (button & 1) nButtonValue |= BIT(WACOMBUTTON_LEFT);
		if (button & 2) nButtonValue |= BIT(WACOMBUTTON_MIDDLE);
		if (button & 4) nButtonValue |= BIT(WACOMBUTTON_RIGHT);
		if (button & 8) nButtonValue |= BIT(WACOMBUTTON_EXTRA);
		if (button & 16) nButtonValue |= BIT(WACOMBUTTON_SIDE);
		pSerial->state.values[WACOMFIELD_BUTTONS].nValue = nButtonValue;

		return pState ? WacomCopyState(pState,&pSerial->state) : 0;
	}

	/* 2nd 4D mouse packet */
	if ((puchData[0] & 0xBE) == 0xAA)
	{
		x = ((((int)puchData[1] & 0x7f) << 9) |
				(((int)puchData[2] & 0x7f) << 2) |
				(((int)puchData[3] & 0x60) >> 5));
		y = ((((int)puchData[3] & 0x1f) << 11) |
				(((int)puchData[4] & 0x7f) << 4) |
				(((int)puchData[5] & 0x78) >> 3));
		rot = ((((int)puchData[6] & 0x0f) << 7) |
				((int)puchData[7] & 0x7f));

		/* FIX ROT */
		if (rot < 900) rot = -rot;
		else rot = 1800 - rot;

		pSerial->state.values[WACOMFIELD_POSITION_X].nValue = x;
		pSerial->state.values[WACOMFIELD_POSITION_Y].nValue = y;
		pSerial->state.values[WACOMFIELD_ROTATION_Z].nValue = rot;
		return pState ? WacomCopyState(pState,&pSerial->state) : 0;
	}

	errno = EINVAL;
	return 1;
}

static int SerialParseWacomIV_1_4(SERIALTABLET* pSerial,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState)
{
	/* Wacom IV, Rom 1.4
	 * Supports: 256 pressure, eraser, 2 side-switch, tilt */

	if ((uLength != 7) && (uLength != 9)) { errno=EINVAL; return 1; }

	if (SerialParseWacomIV_1_3(pSerial,puchData,7,pState))
		return 1;

	/* tilt mode */
	if (uLength == 9)
	{
		int tiltx, tilty;

		tiltx = puchData[7] & 0x3F;
		tilty = puchData[8] & 0x3F;
		if (puchData[7] & 0x40) tiltx -= 64;
		if (puchData[8] & 0x40) tilty -= 64;

		pSerial->state.values[WACOMFIELD_TILT_X].nValue = tiltx;
		pSerial->state.values[WACOMFIELD_TILT_Y].nValue = tilty;
		
		if (pState)
		{
			pState->values[WACOMFIELD_TILT_X].nValue = tiltx;
			pState->values[WACOMFIELD_TILT_Y].nValue = tilty;
		}
	}

	return 0;
}

static int SerialParseWacomIV_1_3(SERIALTABLET* pSerial,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState)
{
	int x=0, y=0, prox=0, tool=WACOMTOOLTYPE_NONE,
			button=0, press=0, stylus, eraser;

	/* Wacom IV, Rom 1.3 (ArtPadII)
	 * Supports: 256 pressure, eraser, 2 side-switch
	 * Limitation: no tilt */

	if (uLength != 7) { errno=EINVAL; return 1; }

	prox = puchData[0] & 0x40 ? 1 : 0;
	if (prox)
	{
		stylus = puchData[0] & 0x20 ? 1 : 0;
		press = (puchData[6] & 0x3F) << 1 | ((puchData[3] & 0x4) >> 2);
		press |= (puchData[6] & 0x40) ? 0 : 0x80;
		eraser = (puchData[3] & 0x20) ? 1 : 0;

		if (stylus)
		{
			/* if entering proximity, choose eraser or stylus2 for bit */
			if (pSerial->state.values[WACOMFIELD_PROXIMITY].nValue == 0)
			{
				if (eraser) tool = WACOMTOOLTYPE_ERASER;
				else tool = WACOMTOOLTYPE_PEN;
			}

			/* otherwise, keep the last tool */
			else tool = pSerial->state.values[WACOMFIELD_TOOLTYPE].nValue;
			
			button = (press > 10) ? BIT(WACOMBUTTON_TOUCH) : 0;

			/* pen has 2 side-switch, eraser has none */
			if (tool == WACOMTOOLTYPE_PEN)
			{
				button |= (puchData[3] & 0x10) ?
						BIT(WACOMBUTTON_STYLUS) : 0;
				button |= (eraser) ? BIT(WACOMBUTTON_STYLUS2) : 0;
			}
		}
		else
		{
			tool = WACOMTOOLTYPE_MOUSE;
			button = (puchData[3] & 0x78) >> 3; /* not tested */
		}

		x = puchData[2] | ((int)puchData[1] << 7) |
				(((int)puchData[0] & 0x3) << 14);
		y = puchData[5] | ((int)puchData[4] << 7) |
				(((int)puchData[3] & 0x3) << 14);
	}

	/* set valid fields */
	pSerial->state.values[WACOMFIELD_PROXIMITY].nValue = prox;
	pSerial->state.values[WACOMFIELD_TOOLTYPE].nValue = tool;
	pSerial->state.values[WACOMFIELD_POSITION_X].nValue = x;
	pSerial->state.values[WACOMFIELD_POSITION_Y].nValue = y;
	pSerial->state.values[WACOMFIELD_PRESSURE].nValue = press;
	pSerial->state.values[WACOMFIELD_BUTTONS].nValue = button;

	return pState ? WacomCopyState(pState,&pSerial->state) : 0;
}

static int SerialParseWacomIV_1_2(SERIALTABLET* pSerial,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState)
{
	int x=0, y=0, prox=0, tool=0, button=WACOMTOOLTYPE_NONE,
			press=0, stylus;

	/* Wacom IV, Rom 1.2, 1.1, and 1.0
	 * Supports: 256 pressure (120 for 1.1 and 1.0), multi-mode
	 * Limitation: no stylus2, no tilt, no eraser */

	if (uLength != 7) { errno=EINVAL; return 1; }

	prox = puchData[0] & 0x40 ? 1 : 0;
	if (prox)
	{
		stylus = puchData[0] & 0x20 ? 1 : 0;
		if (pSerial->nVerMinor == 2)
			press = (puchData[6] & 0x3F) << 1 | ((puchData[3] & 0x4) >> 2) |
					(puchData[6] & 0x40) ? 0 : 0x80;
		else
			press = (puchData[6] & 0x3F) + (puchData[6] & 0x40) ? 0 : 64;

		if (stylus)
		{
			tool = WACOMTOOLTYPE_PEN;
			button = (press > 10) ? BIT(WACOMBUTTON_TOUCH) : 0;
			button |= (puchData[3] & 0x10) ? BIT(WACOMBUTTON_STYLUS) : 0;
		}
		else
		{
			tool = WACOMTOOLTYPE_MOUSE;
			button = (puchData[3] & 0x78) >> 3; /* not tested */
		}

		x = puchData[2] | ((int)puchData[1] << 7) |
				(((int)puchData[0] & 0x3) << 14);
		y = puchData[5] | ((int)puchData[4] << 7) |
				(((int)puchData[3] & 0x3) << 14);
	}

	/* set valid fields */
	pSerial->state.values[WACOMFIELD_PROXIMITY].nValue = prox;
	pSerial->state.values[WACOMFIELD_TOOLTYPE].nValue = tool;
	pSerial->state.values[WACOMFIELD_POSITION_X].nValue = x;
	pSerial->state.values[WACOMFIELD_POSITION_Y].nValue = y;
	pSerial->state.values[WACOMFIELD_PRESSURE].nValue = press;
	pSerial->state.values[WACOMFIELD_BUTTONS].nValue = button;

	return pState ? WacomCopyState(pState,&pSerial->state) : 0;
}

static int SerialParseAcerC100(SERIALTABLET* pSerial,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState)
{
	int x=0, y=0, prox=0, tool=WACOMTOOLTYPE_NONE,
			button=0, press=0, eraser;

	/* Acer C100 Tablet PC (reverse-engineered)
	 * Supports: 256 pressure, eraser, 1 side-switch
	 * Limitation: no tilt (is all zeros, planned?)*/

	if (uLength != 9) { errno=EINVAL; return 1; }

	prox = puchData[0] & 0x20 ? 1 : 0;
	if (prox)
	{
		eraser = (puchData[0] & 0x04) ? 1 : 0;
		press = ((puchData[6] & 0x01) << 7) | (puchData[5] & 0x7F);

		/* tools are distinguishable */
		if (eraser) tool = WACOMTOOLTYPE_ERASER;
		else tool = WACOMTOOLTYPE_PEN;
		
		button = (puchData[0] & 0x01) ? BIT(WACOMBUTTON_TOUCH) : 0;

		/* pen has 1 side-switch, eraser has none */
		if (tool == WACOMTOOLTYPE_PEN)
		{
			button |= (puchData[0] & 0x02) ?
					BIT(WACOMBUTTON_STYLUS) : 0;
		}

		x = (((int)puchData[6] & 0x60) >> 5) |
			((int)puchData[2] << 2) |
			((int)puchData[1] << 9);
		y = (((int)puchData[6] & 0x18) >> 3) |
			((int)puchData[4] << 2) |
			((int)puchData[3] << 9);
	}

	/* set valid fields */
	pSerial->state.values[WACOMFIELD_PROXIMITY].nValue = prox;
	pSerial->state.values[WACOMFIELD_TOOLTYPE].nValue = tool;
	pSerial->state.values[WACOMFIELD_POSITION_X].nValue = x;
	pSerial->state.values[WACOMFIELD_POSITION_Y].nValue = y;
	pSerial->state.values[WACOMFIELD_PRESSURE].nValue = press;
	pSerial->state.values[WACOMFIELD_BUTTONS].nValue = button;

	return pState ? WacomCopyState(pState,&pSerial->state) : 0;
}



/*****************************************************************************
** Internal Functions
*****************************************************************************/

static int SerialSendReset(SERIALTABLET* pSerial)
{
	fprintf(stderr,"Sending reset\n");

	/* reset to Wacom II-S command set, and factory defaults */
	if (SerialSend(pSerial,"\r$\r")) return 1;
	usleep(250000); /* 250 milliseconds */

	/* reset tablet to Wacom IV command set */
	if (SerialSend(pSerial,"#\r")) return 1;
	usleep(75000); /* 75 milliseconds */

	return 0;
}

static int SerialSendStop(SERIALTABLET* pSerial)
{
	if (SerialSend(pSerial,"\rSP\r")) return 1;
	usleep(100000);
	return 0;
}

static int SerialSendStart(SERIALTABLET* pSerial)
{
	return SerialSend(pSerial,"ST\r");
}
 
static int SerialSend(SERIALTABLET* pSerial, const char* pszMsg)
{
	return SerialSendRaw(pSerial,pszMsg,strlen(pszMsg));
}

static int SerialSendRaw(SERIALTABLET* pSerial, const unsigned char* puchData,
		unsigned int uSize)
{
	int nXfer;
	unsigned int uCnt=0;

	while (uCnt < uSize)
	{
		nXfer = write(pSerial->fd,puchData+uCnt,uSize-uCnt);
		if (!nXfer) { perror("sendraw confused"); return 1; }
		if (nXfer < 0) { perror("sendraw bad"); return 1; }
		uCnt += nXfer;
	}

	return 0;
}

static int WacomFlush(SERIALTABLET* pSerial)
{
	char ch[16];
	fd_set fdsRead;
	struct timeval timeout;

	if (tcflush(pSerial->fd, TCIFLUSH) == 0)
		return 0;

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	while (1)
	{
		FD_ZERO(&fdsRead);
		FD_SET(pSerial->fd, &fdsRead);
		if (select(FD_SETSIZE,&fdsRead,NULL,NULL,&timeout) <= 0)
			break;
		read(pSerial->fd,&ch,sizeof(ch));
	}

	return 0;
}

static int SerialSendRequest(SERIALTABLET* pSerial, const char* pszRequest,
		char* pchResponse, unsigned int uSize)
{
	int nXfer;
	fd_set fdsRead;
	unsigned int uLen, uCnt;
	struct timeval timeout;

	uLen = strlen(pszRequest);
	if (SerialSendRaw(pSerial,pszRequest,uLen)) return 1;
	--uLen;

	if (uSize < uLen) { errno=EINVAL; perror("bad size"); return 1; }

	/* read until first header character */
	while (1)
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;

		FD_ZERO(&fdsRead);
		FD_SET(pSerial->fd, &fdsRead);
		if (select(FD_SETSIZE,&fdsRead,NULL,NULL,&timeout) <= 0)
		{
			errno = ETIMEDOUT;
			return 1;
		}

		nXfer = read(pSerial->fd,pchResponse,1);
		if (nXfer <= 0) { perror("trunc response header"); return 1; }
		if (*pchResponse == *pszRequest) break;
		fprintf(stderr,"Discarding %02X\n", *((unsigned char*)pchResponse));
	}

	/* read response header */
	for (uCnt=1; uCnt<uLen; uCnt+=nXfer)
	{
		nXfer = read(pSerial->fd,pchResponse+uCnt,uLen-uCnt);
		if (nXfer <= 0) { perror("trunc response header"); return 1; }
	}

	/* check the header */
	if (strncmp(pszRequest,pchResponse,uLen) != 0)
		{ perror("bad response header"); return 1; }

	/* get the rest of the response */
	for (uCnt=0; uCnt<uSize; ++uCnt)
	{
		nXfer = read(pSerial->fd,pchResponse+uCnt,1);
		if (nXfer <= 0) { perror("bad response read"); return 1; }

		/* stop on CR */
		if (pchResponse[uCnt] == '\r')
		{
			pchResponse[uCnt] = '\0';
			return 0;
		}
	}

	errno = EINVAL;
	perror("bad response");
	return 1;
}

#if 0
static void WacomDump(int fd)
{
	int x, r;
	unsigned char uch[16];
	while (1)
	{
		x = read(fd,&uch,sizeof(uch));
		if (x == 0) break;
		if (x < 0) { perror("WacomDump"); return; }
		for (r=0; r<sizeof(uch); ++r)
		{
			if (r<x) fprintf(stderr,"%02X ",uch[r]);
			else fprintf(stderr,"   ");
		}
		fprintf(stderr," - ");
		for (r=0; r<sizeof(uch); ++r)
		{
			if (r<x) fprintf(stderr,"%c",isprint(uch[r]) ? uch[r] : '.');
			else fprintf(stderr," ");
		}
		fprintf(stderr,"\n");
	}
}

static void WacomTest(int fd)
{
	fprintf(stderr,"SENDING\n");
	write(fd,"1234567890",10);
	write(fd,"1234567890",10);
	write(fd,"1234567890",10);
	write(fd,"1234567890",10);
	write(fd,"1234567890",10);
	fprintf(stderr,"SENT\n");
	write(fd,"\r$\r",3);
	usleep(250000);
	write(fd,"#\r",2);
	usleep(250000);
	write(fd,"~#\r",3);
	WacomDump(fd);
}
#endif

/*****************************************************************************
** Virtual Functions
*****************************************************************************/

static void SerialClose(WACOMTABLET_PRIV* pTablet)
{
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	close(pSerial->fd);
	free(pSerial);
}

static WACOMMODEL SerialGetModel(WACOMTABLET_PRIV* pTablet)
{
	WACOMMODEL model = { 0 };
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	model.uClass = WACOMCLASS_SERIAL;
	model.uVendor = pSerial->pVendor->uVendor;
	model.uDevice = pSerial->pDevice->uDevice;
	model.uSubType = pSerial->pSubType->uSubType;
	return model;
}

static const char* SerialGetVendorName(WACOMTABLET_PRIV* pTablet)
{
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	return pSerial->pVendor->pszDesc;
}

static const char* SerialGetClassName(WACOMTABLET_PRIV* pTablet)
{
	return "Serial";
}

static const char* SerialGetDeviceName(WACOMTABLET_PRIV* pTablet)
{
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	return pSerial->pDevice->pszDesc;
}

static const char* SerialGetSubTypeName(WACOMTABLET_PRIV* pTablet)
{
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	return pSerial->pSubType->pszName;
}

static const char* SerialGetModelName(WACOMTABLET_PRIV* pTablet)
{
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	return pSerial->pSubType->pszDesc;
}

static int SerialGetROMVer(WACOMTABLET_PRIV* pTablet, int* pnMajor,
		int* pnMinor, int* pnRelease)
{
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	if (!pnMajor) { errno=EINVAL; return 1; }
	*pnMajor = pSerial->nVerMajor;
	if (pnMinor) *pnMinor = pSerial->nVerMinor;
	if (pnRelease) *pnRelease = pSerial->nVerRelease;
	return 0;
}

static int SerialGetCaps(WACOMTABLET_PRIV* pTablet)
{
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	return pSerial->pDevice->uCaps;
}

static int SerialGetState(WACOMTABLET_PRIV* pTablet, WACOMSTATE* pState)
{
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	return WacomCopyState(pState,&pSerial->state);
}

static int SerialGetFD(WACOMTABLET_PRIV* pTablet)
{
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	return pSerial->fd;
}

static int SerialReadRaw(WACOMTABLET_PRIV* pTablet, unsigned char* puchData,
		unsigned int uSize)
{
	int nXfer;
	unsigned int uCnt, uPacketLength;
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	if (!pSerial) { errno=EBADF; return 0; }
	uPacketLength = pSerial->uPacketLength;

	/* check size of buffer */
	if (uSize < uPacketLength) { errno=EINVAL; return 0; }
	
	for (uCnt=0; uCnt<uPacketLength; uCnt+=nXfer)
	{
		nXfer = read(pSerial->fd,puchData+uCnt,uPacketLength-uCnt);
		if (nXfer <= 0) return nXfer;
	}

	return (signed)uCnt;
}

static int SerialParseData(WACOMTABLET_PRIV* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState)
{
	int i;
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	if (!pSerial) { errno=EBADF; return 1; }

	/* check synchronization */
	if (!puchData[0] & 0x80)
		{ errno=EINVAL; return 1; }
	for (i=1; i<uLength; ++i)
	{
		if (puchData[i] & 0x80)
			{ errno=EINVAL; return 1; }
	}

	/* dispatch to parser */
	if (pSerial->pfnParse)
		return (*pSerial->pfnParse)(pSerial,puchData,uLength,pState);

	errno=EINVAL;
	return 1;
}

static int SerialResetAtBaud(SERIALTABLET* pSerial, struct termios* pTIOS,
		int nBaud)
{
	/* conver baud rate to tios macro */
	int baudRate = B9600;
	switch (nBaud)
	{
		case 38400: baudRate = B38400; break;
		case 19200: baudRate = B19200; break;
		case 9600: baudRate = B9600; break;
		case 4800: baudRate = B4800; break; /* for testing, maybe */
		case 2400: baudRate = B2400; break; /* for testing, maybe */
		case 1200: baudRate = B1200; break; /* for testing, maybe */
	}

	fprintf(stderr,"Setting baud rate to %d\n",nBaud);

	/* change baud rate */
	cfsetispeed(pTIOS, baudRate);
	cfsetospeed(pTIOS, baudRate);
	if (tcsetattr (pSerial->fd, TCSANOW, pTIOS))
		return 1;

	/* send reset command */
	return SerialSendReset(pSerial);
}
