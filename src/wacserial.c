/*****************************************************************************
** wacserial.c
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
****************************************************************************/

#include "wacserial.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

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
	int nCaps;
} MODEL_INFO;

/*****************************************************************************
** Tablet object
*****************************************************************************/

typedef struct _TABLET TABLET;

typedef int (*PARSEFUNC)(TABLET* pTablet, const unsigned char* puchData,
		unsigned int uLength, WACOMSTATE* pState);

struct _TABLET
{
	int fd;
	MODEL_INFO* pInfo;
	unsigned int uPacketLength;
	int nVerMajor, nVerMinor, nVerRelease;
	int nCaps;
	PARSEFUNC pfnParse;
	int nToolID;
	WACOMSTATE state, max, min;
};

static int TabletIdentifyModel(TABLET* pTablet);
static int TabletInitializeModel(TABLET* pTablet);

static int TabletParseWacomV(TABLET* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);
static int TabletParseWacomIV_1_4(TABLET* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);
static int TabletParseWacomIV_1_3(TABLET* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState);
static int TabletParseWacomIV_1_2(TABLET* pTablet,
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

WACOMTABLET WacomOpenSerial(const char* pszDevice)
{
	int fd;
	struct termios tios;
	TABLET* pTablet = NULL;

	/* open device for read/write access */
	fd = open(pszDevice,O_RDWR);
	if (fd < 0) 
		{ perror("open"); return NULL; }

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
	pTablet = (TABLET*)malloc(sizeof(TABLET));
	memset(pTablet,0,sizeof(*pTablet));
	pTablet->fd = fd;

	/* Identify and initialize the model */
	if (TabletIdentifyModel(pTablet))
		{ perror("identify"); close(fd); free(pTablet); return NULL; }
	if (TabletInitializeModel(pTablet))
		{ perror("init_model"); close(fd); free(pTablet); return NULL; }

	/* Send start */
	WacomSendStart(fd);

	return (WACOMTABLET)pTablet;
}

void WacomCloseSerial(WACOMTABLET hTablet)
{
	TABLET* pTablet = (TABLET*)hTablet;
	if (!pTablet) return;

	close(pTablet->fd);
	free(pTablet);
}

WACOMMODEL WacomGetModel(WACOMTABLET hTablet)
{
	TABLET* pTablet = (TABLET*)hTablet;
	if (!pTablet) { errno=EBADF; return 0; }
	return pTablet->pInfo->uDeviceType | WACOMCLASS_SERIAL;
}

const char* WacomGetModelName(WACOMTABLET hTablet)
{
	TABLET* pTablet = (TABLET*)hTablet;
	if (!pTablet) { errno=EBADF; return 0; }
	return pTablet->pInfo->pszName;
}

int WacomGetRomVersion(WACOMTABLET hTablet, int* pnMajor, int* pnMinor,
		int* pnRelease)
{
	TABLET* pTablet = (TABLET*)hTablet;
	if (!pTablet) { errno=EBADF; return 0; }
	if (!pnMajor) { errno=EINVAL; return 1; }
	*pnMajor = pTablet->nVerMajor;
	if (pnMinor) *pnMinor = pTablet->nVerMinor;
	if (pnRelease) *pnRelease = pTablet->nVerRelease;
	return 0;
}

int WacomReadRaw(WACOMTABLET hTablet, unsigned char* puchData,
		unsigned int uSize)
{
	int nXfer;
	unsigned int uCnt, uPacketLength;
	TABLET* pTablet = (TABLET*)hTablet;
	if (!pTablet) { errno=EBADF; return 0; }
	uPacketLength = pTablet->uPacketLength;

	/* check size of buffer */
	if (uSize < uPacketLength) { errno=EINVAL; return 0; }
	
	for (uCnt=0; uCnt<uPacketLength; uCnt+=nXfer)
	{
		nXfer = read(pTablet->fd,puchData+uCnt,uPacketLength-uCnt);
		if (nXfer <= 0) return nXfer;
	}

	return (signed)uCnt;
}

int WacomGetCapabilities(WACOMTABLET hTablet)
{
	TABLET* pTablet = (TABLET*)hTablet;
	if (!pTablet) { errno=EBADF; return 0; }
	return pTablet->pInfo->nCaps;
}

int WacomGetRanges(WACOMTABLET hTablet, WACOMSTATE* pMin, WACOMSTATE* pMax)
{
	TABLET* pTablet = (TABLET*)hTablet;
	if (!pTablet) { errno=EBADF; return 0; }

	*pMin = pTablet->min;
	*pMax = pTablet->max;
	return 0;
}


int WacomParseData(WACOMTABLET hTablet, const unsigned char* puchData,
		unsigned int uLength, WACOMSTATE* pState)
{
	int i;
	TABLET* pTablet = (TABLET*)hTablet;
	if (!pTablet) { errno=EBADF; return 1; }

	/* check synchronization */
	if (!puchData[0] & 0x80) { errno=EINVAL; return 1; }
	for (i=1; i<uLength; ++i)
	{
		if (puchData[i] & 0x80)
			{ errno=EINVAL; return 1; }
	}

	/* dispatch to parser */
	if (pTablet->pfnParse)
		return (*pTablet->pfnParse)(pTablet,puchData,uLength,pState);

	errno = EINVAL;
	return 1;
}

/*****************************************************************************
** Tablet Functions
*****************************************************************************/

static int TabletIdentifyModel(TABLET* pTablet)
{
	char* pszPos;
	MODEL_INFO* pInfo;
	char chResp[64];

	if (WacomSendRequest(pTablet->fd,"~#\r",chResp,sizeof(chResp)))
		return 1;

	/* look through model table for information */
	for (pInfo=xModels; pInfo->pszID; ++pInfo)
	{
		if (strncmp(chResp,pInfo->pszID,strlen(pInfo->pszID)) == 0)
		{
			pTablet->pInfo = pInfo;
			pTablet->uPacketLength = pInfo->uPacketLength;

			/* get version number */
			pszPos = chResp;
			while (*pszPos) ++pszPos;
			while ((pszPos > chResp) && (pszPos[-1] != 'V')) --pszPos;
			if (sscanf(pszPos,"%d.%d-%d",&pTablet->nVerMajor,
					&pTablet->nVerMinor,&pTablet->nVerRelease) != 3)
			{
				pTablet->nVerRelease = 0;
				if (sscanf(pszPos,"%d.%d",&pTablet->nVerMajor,
						&pTablet->nVerMinor) != 2)
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

static int TabletInitializeModel(TABLET* pTablet)
{
	char chResp[32];

	/* Request tablet dimensions */
	if (WacomSendRequest(pTablet->fd,"~C\r",chResp,sizeof(chResp)))
		return 1;
	if (sscanf(chResp,"%d,%d",&pTablet->max.nPosX,&pTablet->max.nPosY) != 2)
		{ errno=EINVAL; perror("bad dim response"); return 1; }

	/* tablet specific initialization */
	switch (pTablet->pInfo->uDeviceType)
	{
		case WACOMDEVICE_PENPARTNER:
			/* pressure mode */
			WacomSend(pTablet->fd, "PH1\r");
			break;

		case WACOMDEVICE_INTUOS:
			/* multi-mode, max-rate */ 
			WacomSend(pTablet->fd, "MT1\rID1\rIT0\r");
	}
	
	if (pTablet->pInfo->nProtocol == 4)
	{
		/* multi-mode (MU), upper-origin (OC), all-macro (M0),
		 * no-macro1 (M1), max-rate (IT), no-inc (IN),
		 * stream-mode (SR), Z-filter (ZF) */

/*		if (WacomSend(pTablet->fd, "MU1\rOC1\r~M0\r~M1\rIT0\rIN0\rSR\rZF1\r"))
			return 1;
			*/

		if (pTablet->nVerMajor == 1)
		{
			if (pTablet->nVerMinor >= 4)
			{
				/* enable tilt mode */
				if (WacomSend(pTablet->fd,"FM1\r")) return 1;

				pTablet->pfnParse = TabletParseWacomIV_1_4;
				pTablet->uPacketLength = 9;
				pTablet->max.nPressure = 255;
				pTablet->min.nTiltX = -64;
				pTablet->max.nTiltX = 63;
				pTablet->min.nTiltY = -64;
				pTablet->max.nTiltY = 63;
			}
			else if (pTablet->nVerMinor == 3)
			{
				pTablet->pfnParse = TabletParseWacomIV_1_3;
				pTablet->max.nPressure = 255;
			}
			else if (pTablet->nVerMinor == 2)
			{
				pTablet->pfnParse = TabletParseWacomIV_1_2;
				pTablet->max.nPressure = 255;
			}
			else if (pTablet->nVerMinor < 2)
			{
				pTablet->pfnParse = TabletParseWacomIV_1_2;
				pTablet->max.nPressure = 120;
			}
		}
	}
	else if (pTablet->pInfo->nProtocol == 5)
	{
		pTablet->pfnParse = TabletParseWacomV;
		pTablet->max.nPressure = 1023;
		pTablet->max.nAbsWheel = 1023;
		pTablet->min.nRotZ = -899;
		pTablet->max.nRotZ = 900;
		pTablet->min.nThrottle = -1023;
		pTablet->max.nThrottle = 1023;
		pTablet->min.nTiltX = -64;
		pTablet->max.nTiltX = 63;
		pTablet->min.nTiltY = -64;
		pTablet->max.nTiltY = 63;
	}
	else { errno=EINVAL; return 1; }

	return 0;
}

static int TabletParseWacomV(TABLET* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState)
{
	int x=0, y=0, rot=0, tiltx=0, tilty=0, wheel=0,
			tool=WACOMTOOLTYPE_NONE, button=0, press=0, throttle=0;

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
			
		pTablet->nToolID = toolid;
		pTablet->state.nValid = pTablet->nCaps;
		pTablet->state.nProximity = 1;
		pTablet->state.nSerial = serial;
		pTablet->state.nToolType = tool;
		*pState = pTablet->state;
		return 0;
	}

	/* out */
	if ((puchData[0] & 0xFE) == 0x80)
	{
		pTablet->nToolID = 0;
		memset(&pTablet->state,0,sizeof(pTablet->state));
		pTablet->state.nValid = pTablet->nCaps;
		*pState = pTablet->state;
		return 0;
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

		pTablet->state.nValid = pTablet->nCaps;
		pTablet->state.nPosX = x;
		pTablet->state.nPosY = y;
		pTablet->state.nTiltX = tiltx;
		pTablet->state.nTiltY = tilty;
		pTablet->state.nPressure = press;
		pTablet->state.nButtons = button;
		pTablet->state.nAbsWheel = wheel;
		*pState = pTablet->state;
		return 0;
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
		if (pTablet->nToolID == 0x094)
		{
			button = (((puchData[8] & 0x70) >> 1) | (puchData[8] & 0x07));
		}
		/* lens cursor */
		else if (pTablet->nToolID == 0x096)
		{
			button = puchData[8] & 0x1F;
		}
		/* 2D mouse */
		else
		{
			button = (puchData[8] & 0x1C) >> 2;
			wheel = - (puchData[8] & 1) + ((puchData[8] & 2) >> 1);
		}

		pTablet->state.nValid = pTablet->nCaps;
		pTablet->state.nPosX = x;
		pTablet->state.nPosY = y;
		pTablet->state.nRelWheel = wheel;
		pTablet->state.nThrottle = throttle;
		pTablet->state.nButtons &= ~(BIT(WACOMBUTTON_LEFT) |
				BIT(WACOMBUTTON_RIGHT) | BIT(WACOMBUTTON_MIDDLE) |
				BIT(WACOMBUTTON_EXTRA) | BIT(WACOMBUTTON_SIDE));
		if (button & 1) pTablet->state.nButtons |= BIT(WACOMBUTTON_LEFT);
		if (button & 2) pTablet->state.nButtons |= BIT(WACOMBUTTON_MIDDLE);
		if (button & 4) pTablet->state.nButtons |= BIT(WACOMBUTTON_RIGHT);
		if (button & 8) pTablet->state.nButtons |= BIT(WACOMBUTTON_EXTRA);
		if (button & 16) pTablet->state.nButtons |= BIT(WACOMBUTTON_SIDE);

		*pState = pTablet->state;
		return 0;
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

		pTablet->state.nPosX = x;
		pTablet->state.nPosY = y;
		pTablet->state.nRotZ = rot;
		*pState = pTablet->state;
		return 0;
	}

	errno = EINVAL;
	return 1;
}

static int TabletParseWacomIV_1_4(TABLET* pTablet,
		const unsigned char* puchData, unsigned int uLength,
		WACOMSTATE* pState)
{
	/* Wacom IV, Rom 1.4
	 * Supports: 256 pressure, eraser, 2 side-switch, tilt */

	if ((uLength != 7) && (uLength != 9)) { errno=EINVAL; return 1; }

	if (TabletParseWacomIV_1_3(pTablet,puchData,7,pState))
		return 1;

	/* tilt mode */
	if (uLength == 9)
	{
		pTablet->state.nValid |= WACOMVALID(TILT_X) | WACOMVALID(TILT_Y);
		pTablet->state.nTiltX = puchData[7] & 0x3F;
		pTablet->state.nTiltY = puchData[8] & 0x3F;
		if (puchData[7] & 0x40) pTablet->state.nTiltX -= 64;
		if (puchData[8] & 0x40) pTablet->state.nTiltY -= 64;

		pState->nValid = pTablet->state.nValid;
		pState->nTiltX = pTablet->state.nTiltX;
		pState->nTiltY = pTablet->state.nTiltY;
	}

	return 0;
}

static int TabletParseWacomIV_1_3(TABLET* pTablet,
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
			if (pTablet->state.nProximity == 0)
			{
				if (eraser) tool = WACOMTOOLTYPE_ERASER;
				else tool = WACOMTOOLTYPE_PEN;
			}

			/* otherwise, keep the last tool */
			else tool = pTablet->state.nToolType;
			
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
	pTablet->state.nValid = WACOMVALID(PROXIMITY) |
			WACOMVALID(TOOLTYPE) | WACOMVALID(POSITION_X) |
			WACOMVALID(POSITION_Y) | WACOMVALID(PRESSURE) |
			WACOMVALID(BUTTONS);

	pTablet->state.nProximity = prox;
	pTablet->state.nToolType = tool;
	pTablet->state.nPosX = x;
	pTablet->state.nPosY = y;
	pTablet->state.nPressure = press;
	pTablet->state.nButtons = button;

	*pState = pTablet->state;
	return 0;
}


static int TabletParseWacomIV_1_2(TABLET* pTablet,
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
		if (pTablet->nVerMinor == 2)
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
	pTablet->state.nValid = WACOMVALID(PROXIMITY) |
			WACOMVALID(TOOLTYPE) | WACOMVALID(POSITION_X) |
			WACOMVALID(POSITION_Y) | WACOMVALID(PRESSURE) |
			WACOMVALID(BUTTONS);

	pTablet->state.nProximity = prox;
	pTablet->state.nToolType = tool;
	pTablet->state.nPosX = x;
	pTablet->state.nPosY = y;
	pTablet->state.nPressure = press;
	pTablet->state.nButtons = button;

	*pState = pTablet->state;
	return 0;
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
