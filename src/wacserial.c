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

/*****************************************************************************
** Structures
*****************************************************************************/

typedef struct
{
	const char* pszID;
	unsigned int uDeviceType;
	const char* pszName;
	unsigned int uPacketLength;
	int nProtocol;
	unsigned int uCaps;
} MODEL_INFO;

/*****************************************************************************
** Serial Tablet Object
*****************************************************************************/

typedef struct _SERIALTABLET SERIALTABLET;

typedef int (*PARSEFUNC)(SERIALTABLET* pSerial, const unsigned char* puchData,
		unsigned int uLength, WACOMSTATE* pState);

struct _SERIALTABLET
{
	WACOMTABLET_PRIV tablet;
	int fd;
	MODEL_INFO* pInfo;
	unsigned int uPacketLength;
	int nVerMajor, nVerMinor, nVerRelease;
	PARSEFUNC pfnParse;
	int nToolID;
	WACOMSTATE state;
};

static void SerialClose(WACOMTABLET_PRIV* pTablet);
static WACOMMODEL SerialGetModel(WACOMTABLET_PRIV* pTablet);
static const char* SerialGetVendorName(WACOMTABLET_PRIV* pTablet);
static const char* SerialGetModelName(WACOMTABLET_PRIV* pTablet);
static int SerialGetROMVer(WACOMTABLET_PRIV* pTablet, int* pnMajor,
		int* pnMinor, int* pnRelease);
static int SerialGetCaps(WACOMTABLET_PRIV* pTablet);
static int SerialGetState(WACOMTABLET_PRIV* pTablet, WACOMSTATE* pState);
static int SerialReadRaw(WACOMTABLET_PRIV* pTablet, unsigned char* puchData,
		unsigned int uSize);
static int SerialParseData(WACOMTABLET_PRIV* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);

static int SerialIdentifyModel(SERIALTABLET* pSerial);
static int SerialInitializeModel(SERIALTABLET* pSerial);

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

/*static void WacomTest(int fd);*/

#ifndef BIT
#undef BIT
#define BIT(x) (1<<(x))
#endif

#define WACOMVALID(x) BIT(WACOMFIELD_##x)

#define ARTPADII_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(PROXIMITY)| \
		WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)|WACOMVALID(POSITION_Y)| \
		WACOMVALID(PRESSURE))

#define DIGITIZERII_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(PROXIMITY)| \
		WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)|WACOMVALID(POSITION_Y)| \
		WACOMVALID(PRESSURE)|WACOMVALID(TILT_X)|WACOMVALID(TILT_Y))

#define INTUOS_CAPS (WACOMVALID(TOOLTYPE)|WACOMVALID(SERIAL)| \
		WACOMVALID(PROXIMITY)|WACOMVALID(BUTTONS)|WACOMVALID(POSITION_X)| \
		WACOMVALID(POSITION_Y)|WACOMVALID(ROTATION_Z)|WACOMVALID(DISTANCE)| \
		WACOMVALID(PRESSURE)|WACOMVALID(TILT_X)|WACOMVALID(TILT_Y)| \
		WACOMVALID(ABSWHEEL)|WACOMVALID(RELWHEEL)|WACOMVALID(THROTTLE))

#define INTUOS2_CAPS INTUOS_CAPS

/*****************************************************************************
** Globals
*****************************************************************************/

	static MODEL_INFO xModels[] =
	{
		{ "KT-0405-R", WACOMDEVICE_ARTPADII, "Wacom ArtPadII 4x5", 7, 4,
				ARTPADII_CAPS },

		{ "UD-0608-R", WACOMDEVICE_DIGITIZERII, "Wacom DigitizerII 6x8", 7, 4,
				DIGITIZERII_CAPS },
		{ "UD-1212-R", WACOMDEVICE_DIGITIZERII, "Wacom DigitizerII 12x12", 7, 4,
				DIGITIZERII_CAPS },
		{ "UD-1218-R", WACOMDEVICE_DIGITIZERII, "Wacom DigitizerII 12x18", 7, 4,
				DIGITIZERII_CAPS },
		{ "UD-1825-R", WACOMDEVICE_DIGITIZERII, "Wacom DigitizerII 18x25", 7, 4,
				DIGITIZERII_CAPS },
		{ "CT-0405-R", WACOMDEVICE_PENPARTNER, "Wacom PenPartner", 7, 4 },
		{ "ET-0405-R", WACOMDEVICE_GRAPHIRE, "Wacom Graphire", 7, 4 },

		/* There are no serial Graphire 2's */

		{ "GD-0405-R", WACOMDEVICE_INTUOS, "Wacom Intuos 4x5", 9, 5,
				INTUOS_CAPS },
		{ "GD-0608-R", WACOMDEVICE_INTUOS, "Wacom Intuos 6x8", 9, 5,
				INTUOS_CAPS },
		{ "GD-0912-R", WACOMDEVICE_INTUOS, "Wacom Intuos 9x12", 9, 5,
				INTUOS_CAPS },
		{ "GD-1212-R", WACOMDEVICE_INTUOS, "Wacom Intuos 12x12", 9, 5,
				INTUOS_CAPS },
		{ "GD-1218-R", WACOMDEVICE_INTUOS, "Wacom Intuos 12x18", 9, 5,
				INTUOS_CAPS },

		{ "XD-0405-R", WACOMDEVICE_INTUOS2, "Wacom Intuos2 4x5", 9, 5,
				INTUOS2_CAPS },
		{ "XD-0608-R", WACOMDEVICE_INTUOS2, "Wacom Intuos2 6x8", 9, 5,
				INTUOS2_CAPS },
		{ "XD-0912-R", WACOMDEVICE_INTUOS2, "Wacom Intuos2 9x12", 9, 5,
				INTUOS2_CAPS },
		{ "XD-1212-R", WACOMDEVICE_INTUOS2, "Wacom Intuos2 12x12", 9, 5,
				INTUOS2_CAPS },
		{ "XD-1218-R", WACOMDEVICE_INTUOS2, "Wacom Intuos2 12x18", 9, 5,
				INTUOS2_CAPS },

		/* There are no serial Volito's */

		{ NULL }
	};

/*****************************************************************************
** Static Prototypes
*****************************************************************************/

static int WacomSendReset(int fd);
static int WacomSendStop(int fd);
static int WacomSendStart(int fd);

static int WacomSend(int fd, const char* pszData);
static int WacomSendRaw(int fd, const unsigned char* puchData,
		unsigned int uSize);
static int WacomFlush(int fd);
static int WacomSendRequest(int fd, const char* pszRequest, char* pchResponse,
		unsigned int uSize);

/*****************************************************************************
** Public Functions
*****************************************************************************/

WACOMTABLET WacomOpenSerialTablet(int fd)
{
	struct termios tios;
	SERIALTABLET* pSerial = NULL;

	/* configure tty */
	if (isatty(fd))
	{
		/* set up default port parameters */
		if (tcgetattr (fd, &tios))
			{ perror("tcgetattr"); close(fd); return NULL; }

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

#if 0
	{
		int nValue;
		/* clear DTR */
		nValue = TIOCCDTR;
		if (ioctl(fd, TIOCMBIC, &nValue))
			{ perror("clear dtr"); close(fd); return NULL; }

		/* clear RTS */
		nValue = TIOCM_RTS;
		if (ioctl(fd, TIOCMBIC, &nValue))
			{ perror("clear rts"); close(fd); return NULL; }
	}
#endif

		tios.c_cc[VMIN] = 1;		/* vmin value */
		tios.c_cc[VTIME] = 0;		/* vtime value */

		if (tcsetattr (fd, TCSANOW, &tios))
			{ perror("tcsetattr"); close(fd); return NULL; }

		/* set 38400 baud and reset */
		cfsetispeed(&tios, B38400);
		cfsetospeed(&tios, B38400);
		if (tcsetattr (fd, TCSANOW, &tios))
			{ perror("tcsetattr"); close(fd); return NULL; }
		if (WacomSendReset(fd)) { close(fd); return NULL; }

		/* set 19200 baud and reset */
		cfsetispeed(&tios, B19200);
		cfsetospeed(&tios, B19200);
		if (tcsetattr (fd, TCSANOW, &tios))
			{ perror("tcsetattr"); close(fd); return NULL; }
		if (WacomSendReset(fd)) { close(fd); return NULL; }

		/* set 9600 baud and reset */
		cfsetispeed(&tios, B9600);
		cfsetospeed(&tios, B9600);
		if (tcsetattr (fd, TCSANOW, &tios))
			{ perror("tcsetattr"); close(fd); return NULL; }
		if (WacomSendReset(fd)) { close(fd); return NULL; }
	}
	else /* not tty */
	{
		if (WacomSendReset(fd)) { close(fd); return NULL; }
	}

	/* Test */
	/* WacomTest(fd); */

	/* Send stop */
	if (WacomSendStop(fd) || WacomFlush(fd))
		{ perror("stop"); close(fd); return NULL; }

	/* Allocate tablet */
	pSerial = (SERIALTABLET*)malloc(sizeof(SERIALTABLET));
	memset(pSerial,0,sizeof(*pSerial));
	pSerial->tablet.Close = SerialClose;
	pSerial->tablet.GetModel = SerialGetModel;
	pSerial->tablet.GetVendorName = SerialGetVendorName;
	pSerial->tablet.GetModelName = SerialGetModelName;
	pSerial->tablet.GetROMVer = SerialGetROMVer;
	pSerial->tablet.GetCaps = SerialGetCaps;
	pSerial->tablet.GetState = SerialGetState;
	pSerial->tablet.ReadRaw = SerialReadRaw;
	pSerial->tablet.ParseData = SerialParseData;

	pSerial->fd = fd;
	pSerial->state.uValueCnt = WACOMFIELD_MAX;

	/* Identify and initialize the model */
	if (SerialIdentifyModel(pSerial))
		{ perror("identify"); close(fd); free(pSerial); return NULL; }
	if (SerialInitializeModel(pSerial))
		{ perror("init_model"); close(fd); free(pSerial); return NULL; }

	/* Send start */
	WacomSendStart(fd);

	return (WACOMTABLET)pSerial;
}

/*****************************************************************************
** Serial Tablet Functions
*****************************************************************************/

static int SerialIdentifyModel(SERIALTABLET* pSerial)
{
	char* pszPos;
	MODEL_INFO* pInfo;
	char chResp[64];

	if (WacomSendRequest(pSerial->fd,"~#\r",chResp,sizeof(chResp)))
		return 1;

	/* look through model table for information */
	for (pInfo=xModels; pInfo->pszID; ++pInfo)
	{
		if (strncmp(chResp,pInfo->pszID,strlen(pInfo->pszID)) == 0)
		{
			pSerial->pInfo = pInfo;
			pSerial->state.uValid = pInfo->uCaps;
			pSerial->uPacketLength = pInfo->uPacketLength;

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

	fprintf(stderr,"UNIDENTIFIED TABLET: %s\n",chResp);
	return 1;
}

static int SerialInitializeModel(SERIALTABLET* pSerial)
{
	char chResp[32];

	/* Request tablet dimensions */
	if (WacomSendRequest(pSerial->fd,"~C\r",chResp,sizeof(chResp)))
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
	switch (pSerial->pInfo->uDeviceType)
	{
		case WACOMDEVICE_PENPARTNER:
			/* pressure mode */
			WacomSend(pSerial->fd, "PH1\r");
			break;

		case WACOMDEVICE_INTUOS:
			/* multi-mode, max-rate */ 
			WacomSend(pSerial->fd, "MT1\rID1\rIT0\r");
	}
	
	if (pSerial->pInfo->nProtocol == 4)
	{
		/* multi-mode (MU), upper-origin (OC), all-macro (M0),
		 * no-macro1 (M1), max-rate (IT), no-inc (IN),
		 * stream-mode (SR), Z-filter (ZF) */

/*		if (WacomSend(pSerial->fd, "MU1\rOC1\r~M0\r~M1\rIT0\rIN0\rSR\rZF1\r"))
			return 1;
			*/

		if (pSerial->nVerMajor == 1)
		{
			if (pSerial->nVerMinor >= 4)
			{
				/* enable tilt mode */
				if (WacomSend(pSerial->fd,"FM1\r")) return 1;

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
	else if (pSerial->pInfo->nProtocol == 5)
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

/*****************************************************************************
** Internal Functions
*****************************************************************************/

static int WacomSendReset(int fd)
{
	/* reset to Wacom II-S command set, and factory defaults */
	if (WacomSend(fd,"\r$\r")) return 1;
	usleep(250000); /* 250 milliseconds */

	/* reset tablet to Wacom IV command set */
	if (WacomSend(fd,"#\r")) return 1;
	usleep(75000); /* 75 milliseconds */

	return 0;
}

static int WacomSendStop(int fd)
{
	if (WacomSend(fd,"\rSP\r")) return 1;
	usleep(100000);
	return 0;
}

static int WacomSendStart(int fd)
{
	return WacomSend(fd,"ST\r");
}
 
static int WacomSend(int fd, const char* pszMsg)
{
	return WacomSendRaw(fd,pszMsg,strlen(pszMsg));
}

static int WacomSendRaw(int fd, const unsigned char* puchData,
		unsigned int uSize)
{
	int nXfer;
	unsigned int uCnt=0;

	while (uCnt < uSize)
	{
		nXfer = write(fd,puchData+uCnt,uSize-uCnt);
		if (!nXfer) { perror("sendraw confused"); return 1; }
		if (nXfer < 0) { perror("sendraw bad"); return 1; }
		uCnt += nXfer;
	}

	return 0;
}

static int WacomFlush(int fd)
{
	char ch[16];
	fd_set fdsRead;
	struct timeval timeout;

	if (tcflush(fd, TCIFLUSH) == 0)
		return 0;

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	while (1)
	{
		FD_ZERO(&fdsRead);
		FD_SET(fd, &fdsRead);
		if (select(FD_SETSIZE,&fdsRead,NULL,NULL,&timeout) <= 0)
			break;
		read(fd,&ch,sizeof(ch));
	}

	return 0;
}

static int WacomSendRequest(int fd, const char* pszRequest, char* pchResponse,
		unsigned int uSize)
{
	int nXfer;
	unsigned int uLen, uCnt;

	uLen = strlen(pszRequest);
	if (WacomSendRaw(fd,pszRequest,uLen)) return 1;
	--uLen;

	if (uSize < uLen) { errno=EINVAL; perror("bad size"); return 1; }

	/* read until first header character */
	while (1)
	{
		nXfer = read(fd,pchResponse,1);
		if (nXfer <= 0) { perror("trunc response header"); return 1; }
		if (*pchResponse == *pszRequest) break;
		fprintf(stderr,"Discarding %02X\n", *((unsigned char*)pchResponse));
	}

	/* read response header */
	for (uCnt=1; uCnt<uLen; uCnt+=nXfer)
	{
		nXfer = read(fd,pchResponse+uCnt,uLen-uCnt);
		if (nXfer <= 0) { perror("trunc response header"); return 1; }
	}

	/* check the header */
	if (strncmp(pszRequest,pchResponse,uLen) != 0)
		{ perror("bad response header"); return 1; }

	/* get the rest of the response */
	for (uCnt=0; uCnt<uSize; ++uCnt)
	{
		nXfer = read(fd,pchResponse+uCnt,1);
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
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	return WACOM_MAKEMODEL(WACOMVENDOR_WACOM,WACOMCLASS_SERIAL,
			pSerial->pInfo->uDeviceType);
}

static const char* SerialGetVendorName(WACOMTABLET_PRIV* pTablet)
{
	return "Wacom";
}

static const char* SerialGetModelName(WACOMTABLET_PRIV* pTablet)
{
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	return pSerial->pInfo->pszName;
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
	return pSerial->pInfo->uCaps;
}

static int SerialGetState(WACOMTABLET_PRIV* pTablet, WACOMSTATE* pState)
{
	SERIALTABLET* pSerial = (SERIALTABLET*)pTablet;
	return WacomCopyState(pState,&pSerial->state);
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

