/*
 * Copyright 1995-2003 by Frederic Lepied, France. <Lepied@XFree86.org>
 *                                                                            
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Frederic   Lepied not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Frederic  Lepied   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.                   
 *                                                                            
 * FREDERIC  LEPIED DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "xf86Wacom.h"
#include "wcmSerial.h"

/* Serial Support */
static Bool xf86WcmSerialDetect(LocalDevicePtr pDev);
static Bool xf86WcmSerialInit(LocalDevicePtr pDev);
static void xf86WcmSerialRead(LocalDevicePtr pDev);

	WacomDeviceClass wcmSerialDevice =
	{
		xf86WcmSerialDetect,
		xf86WcmSerialInit,
		xf86WcmSerialRead,
	};

static Bool InitTablet(LocalDevicePtr local, const char* buffer, float version);
static void ParseGraphire(WacomCommonPtr common);
static void ParseP4(WacomCommonPtr common);
static void ParseP5(WacomCommonPtr common);

static const char * setup_string = WC_MULTI WC_UPPER_ORIGIN
		WC_ALL_MACRO WC_NO_MACRO1 WC_RATE WC_NO_INCREMENT
		WC_STREAM_MODE WC_ZFILTER;

static const char * pl_setup_string = WC_UPPER_ORIGIN WC_RATE WC_STREAM_MODE;
static const char * penpartner_setup_string = WC_PRESSURE_MODE WC_START;
static const char * intuos_setup_string = WC_V_MULTI WC_V_ID WC_RATE WC_START;

	/* PROTOCOL 4 */

	/* Format of 7 bytes data packet for Wacom Tablets
	Byte 1
	bit 7  Sync bit always 1
	bit 6  Pointing device detected
	bit 5  Cursor = 0 / Stylus = 1
	bit 4  Reserved
	bit 3  1 if a button on the pointing device has been pressed
	bit 2  Reserved
	bit 1  X15
	bit 0  X14

	Byte 2
	bit 7  Always 0
	bits 6-0 = X13 - X7

	Byte 3
	bit 7  Always 0
	bits 6-0 = X6 - X0

	Byte 4
	bit 7  Always 0
	bit 6  B3
	bit 5  B2
	bit 4  B1
	bit 3  B0
	bit 2  P0
	bit 1  Y15
	bit 0  Y14

	Byte 5
	bit 7  Always 0
	bits 6-0 = Y13 - Y7

	Byte 6
	bit 7  Always 0
	bits 6-0 = Y6 - Y0

	Byte 7
	bit 7 Always 0
	bit 6  Sign of pressure data
	bit 5  P6
	bit 4  P5
	bit 3  P4
	bit 2  P3
	bit 1  P2
	bit 0  P1

	byte 8 and 9 are optional and present only
	in tilt mode.

	Byte 8
	bit 7 Always 0
	bit 6 Sign of tilt X
	bit 5  Xt6
	bit 4  Xt5
	bit 3  Xt4
	bit 2  Xt3
	bit 1  Xt2
	bit 0  Xt1

	Byte 9
	bit 7 Always 0
	bit 6 Sign of tilt Y
	bit 5  Yt6
	bit 4  Yt5
	bit 3  Yt4
	bit 2  Yt3
	bit 1  Yt2
	bit 0  Yt1
	*/

/*****************************************************************************
 * xf86WcmSendRequest --
 *   send a request and wait for the answer.
 *   the answer must begin with the first two chars of the request.
 *   The last character in the answer string is replaced by a \0.
 ****************************************************************************/

char* xf86WcmSendRequest(int fd, const char* request, char* answer, int maxlen)
{
	int len, nr;
	int maxtry = MAXTRY;

	if (maxlen < 3)
		return NULL;
  
	/* send request string */
	do
	{
		SYSCALL(len = xf86WcmWrite(fd, request, strlen(request)));
		if ((len == -1) && (errno != EAGAIN))
		{
			ErrorF("Wacom xf86WcmWrite error : %s", strerror(errno));
			return NULL;
		}
		maxtry--;
	} while ((len == -1) && maxtry);

	if (maxtry == 0)
	{
		ErrorF("Wacom unable to xf86WcmWrite request string '%s' "
				"after %d tries\n", request, MAXTRY);
		return NULL;
	}
  
	do
	{
		maxtry = MAXTRY;
    
		/* Read the first byte of the answer which must
		 * be equal to the first byte of the request.
		 */
		do
		{
			if ((nr = xf86WcmWaitForTablet(fd)) > 0)
			{
				SYSCALL(nr = xf86WcmRead(fd, answer, 1));
				if ((nr == -1) && (errno != EAGAIN))
				{
					ErrorF("Wacom xf86WcmRead error : %s\n",
							strerror(errno));
					return NULL;
				}
				DBG(10, ErrorF("%c err=%d [0]\n",
					answer[0], nr));
			}
			maxtry--;  
		} while ((answer[0] != request[0]) && maxtry);

		if (maxtry == 0)
		{
			ErrorF("Wacom unable to read first byte of "
					"request '%c%c' answer after %d tries\n",
					request[0], request[1], MAXTRY);
			return NULL;
		}

		/* Read the second byte of the answer which must be equal
		 * to the second byte of the request. */
		do
		{    
			maxtry = MAXTRY;
			do
			{
				if ((nr = xf86WcmWaitForTablet(fd)) > 0)
				{
					SYSCALL(nr = xf86WcmRead(fd, answer+1, 1));
					if ((nr == -1) && (errno != EAGAIN))
					{
						ErrorF("Wacom xf86WcmRead error : %s\n",
								strerror(errno));
						return NULL;
					}
					DBG(10, ErrorF("%c err=%d [1]\n",
							answer[1], nr));
				}
				maxtry--;  
			} while ((nr <= 0) && maxtry);
	      
			if (maxtry == 0)
			{
				ErrorF("Wacom unable to read second byte of "
					"request '%c%c' answer after %d "
					"tries\n", request[0], request[1],
					MAXTRY);
				return NULL;
			}

			if (answer[1] != request[1])
				answer[0] = answer[1];
	      
		} while ((answer[0] == request[0]) && (answer[1] != request[1]));

	} while ((answer[0] != request[0]) && (answer[1] != request[1]));

	/* Read until we don't get anything or timeout. */

	len = 2;
	maxtry = MAXTRY;
	do
	{ 
		do
		{
			if ((nr = xf86WcmWaitForTablet(fd)) > 0)
			{
				SYSCALL(nr = xf86WcmRead(fd, answer+len, 1));
				if ((nr == -1) && (errno != EAGAIN))
				{
					ErrorF("Wacom xf86WcmRead error : %s\n", strerror(errno));
					return NULL;
				}
				DBG(10, ErrorF("%c err=%d [%d]\n", answer[len], nr, len));
			}
			else
			{
				if (len == 2)
				{
					DBG(10, ErrorF("timeout remains %d tries\n", maxtry));
					maxtry--;
				}
			}
		} while ((nr <= 0) && len == 2 && maxtry);

		if (nr > 0)
		{
			len += nr;
			if (len >= (maxlen - 1))
				return NULL;
		}

		if (maxtry == 0)
		{
			ErrorF("Wacom unable to read last byte of request '%c%c' answer after %d tries\n",
					request[0], request[1], MAXTRY);
			break;
		}
	} while (nr > 0);

	if (len <= 3)
		return NULL;
    
	answer[len-1] = '\0';
  
	return answer;
}

static Bool xf86WcmSerialDetect(LocalDevicePtr pDev)
{
	return 1;
}

static Bool xf86WcmSerialInit(LocalDevicePtr local)
{
	char buffer[256];
	int err;
	int loop, idx;
	float version = 0.0;
    
	DBG(1, ErrorF("initializing serial tablet\n"));    

	/* Set the speed of the serial link to 38400 */
	if (xf86WcmSetSerialSpeed(local->fd, 38400) < 0)
		return !Success;
    
	/* Send reset to the tablet */
	SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET_BAUD,
		strlen(WC_RESET_BAUD)));

	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}
    
	/* Wait 250 mSecs */
	if (xf86WcmWait(250))
		return !Success;

	/* Send reset to the tablet */
	SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET)));
	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}
    
	/* Wait 75 mSecs */
	if (xf86WcmWait(75))
		return !Success;

	/* Set the speed of the serial link to 19200 */
	if (xf86WcmSetSerialSpeed(local->fd, 19200) < 0)
		return !Success;
    
	/* Send reset to the tablet */
	SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET_BAUD, strlen(WC_RESET_BAUD)));
	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}
    
	/* Wait 250 mSecs */
	if (xf86WcmWait(250))
		return !Success;

	/* Send reset to the tablet */
	SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET)));
	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}
    
	/* Wait 75 mSecs */
	if (xf86WcmWait(75))
		return !Success;

	/* Set the speed of the serial link to 9600 */
	if (xf86WcmSetSerialSpeed(local->fd, 9600) < 0)
		return !Success;
    
	/* Send reset to the tablet */
	SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET_BAUD, strlen(WC_RESET_BAUD)));
	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}
    
	/* Wait 250 mSecs */
	if (xf86WcmWait(250))
		return !Success;

	SYSCALL(err = xf86WcmWrite(local->fd, WC_STOP, strlen(WC_STOP)));
	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Wait 30 mSecs */
	if (xf86WcmWait(30))
		return !Success;

	xf86WcmFlushTablet(local->fd);

	DBG(2, ErrorF("reading model\n"));
	if (!xf86WcmSendRequest(local->fd, WC_MODEL, buffer, sizeof(buffer)))
		return !Success;

	DBG(2, ErrorF("%s\n", buffer));
  
	if (xf86Verbose)
		ErrorF("%s Wacom tablet model : %s\n",
				XCONFIG_PROBED, buffer+2);
    
	/* Answer is in the form ~#Tablet-Model VRom_Version */
	/* look for the first V from the end of the string */
	/* this seems to be the better way to find the version of the ROM */
	for(loop=strlen(buffer); loop>=0 && *(buffer+loop) != 'V'; loop--);
	for(idx=loop; idx<strlen(buffer) && *(buffer+idx) != '-'; idx++);
	*(buffer+idx) = '\0';

	/* Extract version numbers */
	sscanf(buffer+loop+1, "%f", &version);

	if (InitTablet(local,buffer,version) == !Success)
	{
		SYSCALL(xf86WcmClose(local->fd));
		local->fd = -1;
		return !Success;
	}

	return Success;
}

typedef struct _WACOMMODEL WACOMMODEL, *WACOMMODELPTR;

struct _WACOMMODEL
{
	const char* name;
	float coordRes;
	void (*Initialize)(LocalDevicePtr local, const char* id, float version);
	void (*EnableTilt)(LocalDevicePtr local);
	void (*GetResolution)(LocalDevicePtr local);
	int (*GetRanges)(WACOMMODELPTR model, LocalDevicePtr local);
	int (*Reset)(LocalDevicePtr local);
};

static void InitIntuos(LocalDevicePtr local, const char* id, float version);
static void InitIntuos2(LocalDevicePtr local, const char* id, float version);
static void InitCintiq(LocalDevicePtr local, const char* id, float version);
static void InitPenPartner(LocalDevicePtr local, const char* id, float version);
static void InitGraphire(LocalDevicePtr local, const char* id, float version);
static void InitProtocol4(LocalDevicePtr local, const char* id, float version);
static void EnableTilt(LocalDevicePtr local);
static void GetResolution(LocalDevicePtr local);
static int GetRanges(WACOMMODELPTR model, LocalDevicePtr local);
static int ResetIntuos(LocalDevicePtr local);
static int ResetCintiq(LocalDevicePtr local);
static int ResetPenPartner(LocalDevicePtr local);
static int ResetProtocol4(LocalDevicePtr local);

	WACOMMODEL modelIntuos =
	{
		"Intuos",
		1.0,            /* ranges reported in correct units */
		InitIntuos,
		NULL,           /* tilt automatically enabled */
		NULL,           /* resolution not queried */
		GetRanges,
		ResetIntuos,
	};

	WACOMMODEL modelIntuos2 =
	{
		"Intuos2",
		1.0,            /* ranges reported in correct units */
		InitIntuos2,
		NULL,           /* tilt automatically enabled */
		NULL,           /* resolution not queried */
		GetRanges,
		ResetIntuos,    /* same as Intuos */
	};

	WACOMMODEL modelCintiq =
	{
		"Cintiq",
		1270.0,         /* ranges report in 1270 lpi */
		InitCintiq,
		EnableTilt,
		GetResolution,
		GetRanges,
		ResetCintiq,
	};

	WACOMMODEL modelPenPartner =
	{
		"PenPartner",
		1.0,            /* ranges reported in correct units */
		InitPenPartner,
		EnableTilt,
		NULL,           /* resolution not supported */
		GetRanges,
		ResetPenPartner,
	};	

	WACOMMODEL modelGraphire =
	{
		"Graphire",
		1.0,                    /* ranges reported in correct units */
		InitGraphire,
		EnableTilt,
		NULL,                   /* resolution not supported */
		NULL,                   /* ranges not supported */
		ResetPenPartner,        /* functionally very similar */
	};

	WACOMMODEL modelProtocol4 =
	{
		"Protocol4",
		1270.0,         /* ranges report in 1270 lpi */
		InitProtocol4,
		EnableTilt,
		GetResolution,
		GetRanges,
		ResetProtocol4,
	};

static void InitIntuos(LocalDevicePtr local, const char* id, float version)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(2, ErrorF("detected an Intuos model\n"));

	common->Parse = ParseP5;
	common->wcmProtocolLevel = 5;
	common->wcmVersion = version;

	common->wcmMaxZ = 1023;  /* max Z value */
	common->wcmResolX = 2540; /* X resolution in points/inch */
	common->wcmResolY = 2540; /* Y resolution in points/inch */
	common->wcmResolZ = 1;  /* pressure resolution of tablet */
	common->wcmPktLength = 9; /* length of a packet */

	if (common->wcmThreshold == INVALID_THRESHOLD)
	{
		/* Threshold for counting pressure as a button */
		common->wcmThreshold = -480;
		if (xf86Verbose)
		{
			ErrorF("%s Wacom using pressure threshold of "
				"%d for button 1\n",
				XCONFIG_PROBED, common->wcmThreshold);
		}
	}
}

static void InitIntuos2(LocalDevicePtr local, const char* id, float version)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(2, ErrorF("detected an Intuos2 model\n"));

	common->Parse = ParseP5;
	common->wcmFlags |= INTUOS2_FLAG;
	common->wcmProtocolLevel = 5;
	common->wcmVersion = version;

	common->wcmMaxZ = 1023;  /* max Z value */
	common->wcmResolX = 2540; /* X resolution in points/inch */
	common->wcmResolY = 2540; /* Y resolution in points/inch */
	common->wcmResolZ = 1;  /* pressure resolution of tablet */
	common->wcmPktLength = 9; /* length of a packet */

	if (common->wcmThreshold == INVALID_THRESHOLD)
	{
		/* Threshold for counting pressure as a button */
		common->wcmThreshold = -480;
		if (xf86Verbose)
		{
			ErrorF("%s Wacom using pressure threshold of "
				"%d for button 1\n",
				XCONFIG_PROBED, common->wcmThreshold);
		}
	}
}

static void InitCintiq(LocalDevicePtr local, const char* id, float version)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(2, ErrorF("detected a Cintiq model\n"));

	common->Parse = ParseP4;
	common->wcmFlags |= PL_FLAG;
	common->wcmProtocolLevel = 4;
	common->wcmVersion = version;

	common->wcmResolX = 508; /* X resolution in points/inch */
	common->wcmResolY = 508; /* Y resolution in points/inch */
	common->wcmResolZ = 1;  /* pressure resolution of tablet */

	if (id[5] == '2')
	{
		/* PL-250  */
		if ( id[6] == '5' )
		{
			common->wcmMaxX = 9700;
			common->wcmMaxY = 7300;
			common->wcmMaxZ = 255;
		}
		/* PL-270  */
		else
		{
			common->wcmMaxX = 10560;
			common->wcmMaxY = 7920;
			common->wcmMaxZ = 255;
		}
	}
	else if (id[5] == '3')
	{
		/* PL-300  */
		common->wcmMaxX = 10560;
		common->wcmMaxY = 7920;
		common->wcmMaxZ = 255;
	}
	else if (id[5] == '4')
	{
		/* PL-400  */
		common->wcmMaxX = 13590;
		common->wcmMaxY = 10240;
		common->wcmMaxZ = 255;
	}
	else if (id[5] == '5')
	{
		/* PL-550  */
		if ( id[6] == '5' )
		{
			common->wcmMaxX = 15360;
			common->wcmMaxY = 11520;
			common->wcmMaxZ = 511;
		}
		/* PL-500  */
		else
		{
			common->wcmMaxX = 15360;
			common->wcmMaxY = 11520;
			common->wcmMaxZ = 255;
		}
	}
	else if (id[5] == '6')
	{
		/* PL-600SX  */
		if ( id[8] == 'S' )
		{
			common->wcmMaxX = 15650;
			common->wcmMaxY = 12540;
			common->wcmMaxZ = 255;
		}
		/* PL-600  */
		else
		{
			common->wcmMaxX = 15315;
			common->wcmMaxY = 11510;
			common->wcmMaxZ = 255;
		}
	}
	else if (id[5] == '8')
	{
		/* PL-800  */
		common->wcmMaxX = 18050;
		common->wcmMaxY = 14450;
		common->wcmMaxZ = 511;
	}
}

static void InitPenPartner(LocalDevicePtr local, const char* id, float version)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(2, ErrorF("detected a PenPartner model\n"));

	common->Parse = ParseP4;
	common->wcmProtocolLevel = 4;
	common->wcmVersion = version;

	common->wcmMaxZ = 256;
	common->wcmResolX = 1000;
	common->wcmResolY = 1000;
}

static void InitGraphire(LocalDevicePtr local, const char* id, float version)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(2, ErrorF("detected a Graphire model\n"));

	common->Parse = ParseGraphire;
	common->wcmFlags |= GRAPHIRE_FLAG;
	common->wcmProtocolLevel = 4;
	common->wcmVersion = version;

	/* Graphire models don't answer WC_COORD requests */
	common->wcmMaxX = 5103;
	common->wcmMaxY = 3711;
	common->wcmMaxZ = 512;
	common->wcmResolX = 1000;
	common->wcmResolY = 1000;
}

static void InitProtocol4(LocalDevicePtr local, const char* id, float version)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(2, ErrorF("detected a Protocol4 model\n"));

	common->Parse = ParseP4;
	common->wcmProtocolLevel = 4;
	common->wcmVersion = version;

	/* If no maxZ is set, determine from version */
	if (!common->wcmMaxZ)
	{
		/* the rom version determines the max z */
		if (version >= (float)1.2)
			common->wcmMaxZ = 255;
		else
			common->wcmMaxZ = 120;
	}
}

static void EnableTilt(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	/* Tilt works on ROM 1.4 and above */
	if (common->wcmVersion >= 1.4F)
	{
		common->wcmPktLength = 9;
		DBG(2, ErrorF("EnableTilt\n"));
	}
}

static void GetResolution(LocalDevicePtr local)
{
	int a, b;
	char buffer[BUFFER_SIZE], header[BUFFER_SIZE];
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	if (!(common->wcmResolX && common->wcmResolY))
	{
		DBG(2, ErrorF("reading config\n"));
		if (xf86WcmSendRequest(local->fd, WC_CONFIG, buffer,
			sizeof(buffer)))
		{
			DBG(2, ErrorF("%s\n", buffer));
			/* The header string is simply a place to put the
			 * unwanted config header don't use buffer+xx because
			 * the header size varies on different tablets */

			if (sscanf(buffer, "%[^,],%d,%d,%d,%d", header,
				 &a, &b, &common->wcmResolX,
				 &common->wcmResolY) == 5)
			{
				DBG(6, ErrorF("WC_CONFIG Header = %s\n",
					header));
			}
			else
			{
				ErrorF("WACOM: unable to parse resolution. "
					"Using default.\n");
				common->wcmResolX = common->wcmResolY = 1270;
			}
		}
		else
		{
			ErrorF("WACOM: unable to read resolution. "
				"Using default.\n");
			common->wcmResolX = common->wcmResolY = 1270;
		}
	}

	DBG(2, ErrorF("GetResolution: ResolX=%d ResolY=%d\n",
		common->wcmResolX, common->wcmResolY));
}

static int GetRanges(WACOMMODELPTR model, LocalDevicePtr local)
{
	char buffer[BUFFER_SIZE];
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	if (!(common->wcmMaxX && common->wcmMaxY))
	{
		DBG(2, ErrorF("reading max coordinates\n"));
		if (!xf86WcmSendRequest(local->fd, WC_COORD, buffer,
			sizeof(buffer)))
		{
			ErrorF("WACOM: unable to read max coordinates. "
				"Use the MaxX and MaxY options.\n");
			return !Success;
		}
		DBG(2, ErrorF("%s\n", buffer));
		if (sscanf(buffer+2, "%d,%d", &common->wcmMaxX,
			&common->wcmMaxY) != 2)
		{
			ErrorF("WACOM: unable to parse max coordinates. "
				"Use the MaxX and MaxY options.\n");
			return !Success;
		}

		/* Adjust the resolution for tablets reporting in units
		 * other than points. Typical value is 1270.0 lpi. */

		if (model->coordRes != 1.0)
		{
			DBG(2, ErrorF("GetRanges adjusting %d,%d by %g to "
				"%d,%d\n",
				common->wcmMaxX, common->wcmMaxY,
				model->coordRes,
				(common->wcmMaxX / model->coordRes) *
					common->wcmResolX,
				(common->wcmMaxY / model->coordRes) *
					common->wcmResolY));

			common->wcmMaxX = (common->wcmMaxX / MAX_COORD_RES) *
				common->wcmResolX;
			common->wcmMaxY = (common->wcmMaxY / MAX_COORD_RES) *
				common->wcmResolY;
		}
	}

	DBG(2, ErrorF("GetRanges: maxX=%d maxY=%d\n",
		common->wcmMaxX, common->wcmMaxY));

	return Success;
}

static int ResetIntuos(LocalDevicePtr local)
{
	int err;
	SYSCALL(err = xf86WcmWrite(local->fd, intuos_setup_string,
		strlen(intuos_setup_string)));
	return (err == -1) ? !Success : Success;
}

static int ResetCintiq(LocalDevicePtr local)
{
	int err;

	SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET)));
  
	if (xf86WcmWait(75)) return !Success;

	SYSCALL(err = xf86WcmWrite(local->fd, pl_setup_string,
		strlen(pl_setup_string)));
	if (err == -1) return !Success;

	SYSCALL(err = xf86WcmWrite(local->fd, penpartner_setup_string,
		strlen(penpartner_setup_string)));

	return (err == -1) ? !Success : Success;
}

static int ResetPenPartner(LocalDevicePtr local)
{
	int err;
	SYSCALL(err = xf86WcmWrite(local->fd, penpartner_setup_string,
		strlen(penpartner_setup_string)));
	return (err == -1) ? !Success : Success;
}

static int ResetProtocol4(LocalDevicePtr local)
{
	int err;

	SYSCALL(err = xf86WcmWrite(local->fd, WC_RESET, strlen(WC_RESET)));
  
	if (xf86WcmWait(75)) return !Success;

	SYSCALL(err = xf86WcmWrite(local->fd, setup_string,
		strlen(setup_string)));
	if (err == -1) return !Success;

	SYSCALL(err = xf86WcmWrite(local->fd, penpartner_setup_string,
		strlen(penpartner_setup_string)));
	return (err == -1) ? !Success : Success;
}

static Bool InitTablet(LocalDevicePtr local, const char* id, float version)
{
	int err;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	int is_a_penpartner = 0;
	WACOMMODEL* model = NULL;

	/* Intuos */
	if (id[2] == 'G' && id[3] == 'D')
		model = &modelIntuos;

	/* Intuos2 */
	else if (id[2] == 'X' && id[3] == 'D')
		model = &modelIntuos2;

	/* Cintiq */
	else if (id[2] == 'P' && id[3] == 'L')
		model = &modelCintiq;

	/* PenPartner */
	else if (id[2] == 'C' && id[3] == 'T')
	{
		model = &modelPenPartner;
		is_a_penpartner = 1;
	}

	/* Graphire */
	else if (id[2] == 'E' && id[3] == 'T')
	{
		model = &modelGraphire;
		is_a_penpartner = 1;
	}

	/* everything else */
	else
		model = &modelProtocol4;

	/* INITIALIZATION */

	if (model->Initialize)
		model->Initialize(local,id,version);

	if (model->EnableTilt && common->wcmFlags & TILT_FLAG)
		model->EnableTilt(local);

	if (model->GetResolution)
		model->GetResolution(local);

	if (model->GetRanges)
		if (model->GetRanges(model,local) != Success)
			return !Success;

	DBG(2, ErrorF("setup is maxX=%d maxY=%d resolX=%d resolY=%d\n",
			common->wcmMaxX, common->wcmMaxY, common->wcmResolX,
			common->wcmResolY));

	/* RESET TABLET */

	if (model->Reset && (model->Reset(local) != Success))
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Send the tilt mode command after setup because it must be enabled
	 * after multi-mode to take precedence */
	if (common->wcmProtocolLevel == 4 && HANDLE_TILT(common))
	{
		DBG(2, ErrorF("Sending tilt mode order\n"));
 
		SYSCALL(err = xf86WcmWrite(local->fd, WC_TILT_MODE,
			strlen(WC_TILT_MODE)));
		if (err == -1)
		{
			ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
			return !Success;
		}
	}

	if (common->wcmProtocolLevel == 4)
	{
		char buf[20];
      
		sprintf(buf, "%s%d\r", WC_SUPPRESS, common->wcmSuppress);
		SYSCALL(err = xf86WcmWrite(local->fd, buf, strlen(buf)));

		if (err == -1)
		{
			ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
			return !Success;
		}
	}
    
	if (xf86Verbose)
		ErrorF("%s Wacom %s tablet maximum X=%d maximum Y=%d "
			"X resolution=%d Y resolution=%d suppress=%d%s\n",
			XCONFIG_PROBED, common->wcmProtocolLevel == 4 ?
				"IV" : "V",
			common->wcmMaxX, common->wcmMaxY,
			common->wcmResolX, common->wcmResolY,
			common->wcmSuppress,
			HANDLE_TILT(common) ? " Tilt" : "");
  
	/* change the serial speed if requested */
	if (common->wcmLinkSpeed > 9600)
	{
		if (common->wcmProtocolLevel == 5)
		{
			char *speed_init_string = WC_V_19200;
			DBG(1, ErrorF("Switching serial link to %d\n",
				common->wcmLinkSpeed));

			if (common->wcmLinkSpeed == 38400 &&
				version < 2.0 &&
				!(common->wcmFlags&INTUOS2_FLAG))
			{
				ErrorF("Wacom: 38400 speed not supported "
					"with this Intuos firmware (%f)\n",
					version);
				ErrorF("Switching to 19200\n");
				common->wcmLinkSpeed = 19200;
			}

			switch (common->wcmLinkSpeed)
			{
				case 38400:
					speed_init_string = WC_V_38400;
					break;

				case 19200:
					speed_init_string = WC_V_19200;
					break;
			}
			/* Switch the tablet to the requested speed */
			SYSCALL(err = xf86WcmWrite(local->fd, speed_init_string,
				strlen(speed_init_string)));
			if (err == -1)
			{
				ErrorF("Wacom xf86WcmWrite error : %s\n",
					strerror(errno));
				return !Success;
			}
     
			/* Wait 75 mSecs */
			if (xf86WcmWait(75))
				return !Success;
    
			/* Set speed of serial link to requested speed */
			if (xf86WcmSetSerialSpeed(local->fd, common->wcmLinkSpeed) < 0)
			return !Success;
		} /* protocol 5 */
		else
		{
			ErrorF("Changing the speed of a wacom IV "
				"device is not yet implemented\n");
		}
	} /* change link speed */

	/* Tell the tablet to start sending coordinates */
	SYSCALL(err = xf86WcmWrite(local->fd, WC_START, strlen(WC_START)));

	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	return Success;
}

/*****************************************************************************
 * xf86WcmSerialRead --
 *   Read the new events from the device, and enqueue them.
 ****************************************************************************/

static void xf86WcmSerialRead(LocalDevicePtr local)
{
	WacomCommonPtr common = ((WacomDevicePtr)(local->private))->common;
	int len, loop;
	unsigned char buffer[BUFFER_SIZE];

	DBG(7, ErrorF("xf86WcmSerialRead BEGIN device=%s fd=%d\n",
		common->wcmDevice, local->fd));

	SYSCALL(len = xf86WcmRead(local->fd, buffer, sizeof(buffer)));

	if (len <= 0)
	{
		ErrorF("Error reading wacom device : %s\n", strerror(errno));
		return;
	}

	DBG(10, ErrorF("xf86WcmSerialRead read %d bytes\n", len));

	for (loop=0; loop<len; loop++)
	{
		/* magic bit is not OK */
		if ((common->wcmIndex == 0) && !(buffer[loop] & HEADER_BIT))
		{
			DBG(6, ErrorF("xf86WcmSerialRead bad magic "
				"number 0x%x (pktlength=%d) %d\n",
				buffer[loop], common->wcmPktLength, loop));
			continue;
		}

		/* magic bit at wrong place */
		else if ((common->wcmIndex != 0) && (buffer[loop] & HEADER_BIT))
		{
			DBG(6, ErrorF("xf86WcmSerialRead magic "
				"number 0x%x detetected at index %d loop=%d\n",
				(unsigned int)buffer[loop],common->wcmIndex,
				loop));
			common->wcmIndex = 0;
		}
 
		common->wcmData[common->wcmIndex++] = buffer[loop];

		/* if there are enough bytes, parse */
		if (common->wcmIndex == common->wcmPktLength)
		{
			if (common->Parse)
				common->Parse(common);

			/* reset for next packet */
			common->wcmIndex = 0;
		}
	} /* next data */

	DBG(7, ErrorF("xf86WcmSerialRead END   local=0x%x index=%d\n",
		local, common->wcmIndex));
}

/* ugly kludge - temporarily exploded out */

static void ParseGraphire(WacomCommonPtr common)
{
	int x, y, z, idx, buttons, tx = 0, ty = 0;
	int is_stylus, is_button, wheel=0;

	int is_proximity = (common->wcmData[0] & PROXIMITY_BIT);
     
	/* reset char count for next read */
	common->wcmIndex = 0;

	x = (((common->wcmData[0] & 0x3) << 14) +
		(common->wcmData[1] << 7) +
		common->wcmData[2]);
	y = (((common->wcmData[3] & 0x3) << 14) +
		(common->wcmData[4] << 7) +
		common->wcmData[5]);

	/* check which device we have */
	is_stylus = (common->wcmData[0] & POINTER_BIT);
	z = ((common->wcmData[6]&ZAXIS_BITS) << 2 ) +
		((common->wcmData[3]&ZAXIS_BIT) >> 1) +
		((common->wcmData[3]&PROXIMITY_BIT) >> 6) +
		((common->wcmData[6]&ZAXIS_SIGN_BIT) ? 0 : 0x100);

	if (is_stylus)
	{
		buttons = ((common->wcmData[3] & 0x30) >> 3) |
			(z >= common->wcmThreshold ? 1 : 0);
	}
	else
	{
		buttons = (common->wcmData[3] & 0x38) >> 3;
		wheel = (common->wcmData[6] & 0x30) >> 4;

		if (common->wcmData[6] & 0x40)
			wheel = -wheel;
	}
	is_button = (buttons != 0);

	DBG(10, ErrorF("graphire buttons=%d prox=%d "
		"wheel=%d\n", buttons, is_proximity,
		wheel));

	/* The stylus reports button 4 for the second side
	 * switch and button 4/5 for the eraser tip. We know
	 * how to choose when we come in proximity for the
	 * first time. If we are in proximity and button 4 then
	 * we have the eraser else we have the second side
	 * switch.
	 */
	if (is_stylus)
	{
		if (!common->wcmStylusProximity && is_proximity)
			common->wcmStylusSide = !(common->wcmData[3] & 0x40);
	}

	DBG(8, ErrorF("xf86WcmSerialRead %s side\n",
		common->wcmStylusSide ? "stylus" : "eraser"));
		common->wcmStylusProximity = is_proximity;

	/* handle tilt values only for stylus */
	if (HANDLE_TILT(common))
	{
		tx = (common->wcmData[7] & TILT_BITS);
		ty = (common->wcmData[8] & TILT_BITS);
		if (common->wcmData[7] & TILT_SIGN_BIT)
			tx -= (TILT_BITS + 1);
		if (common->wcmData[8] & TILT_SIGN_BIT)
			ty -= (TILT_BITS + 1);
	}

	/* split data amonst devices */
	for(idx=0; idx<common->wcmNumDevices; idx++)
	{
	 	LocalDevicePtr local_dev = common->wcmDevices[idx]; 
		WacomDevicePtr priv= (WacomDevicePtr)local_dev->private;
		int temp_buttons = buttons;
		int temp_is_proximity = is_proximity;
		int curDevice; 

		DBG(7, ErrorF("xf86WcmSerialRead trying "
			"to send to %s\n", local_dev->name));

		/* check for device type (STYLUS, ERASER
		 * or CURSOR) */

		if (is_stylus)
		{
			/* The eraser is reported as button 4
			 * and 5 of the stylus.  If we haven't
			 * an independent device for the
			 * eraser report the button as
			 * button 3 of the stylus. */

			if (is_proximity)
			{
				if (common->wcmData[3] & 0x40)
					curDevice = ERASER_ID;
				else
					curDevice = STYLUS_ID;
			}
			else
			{
				/* When we are out of proximity with
				 * the eraser the button 4 isn't reported
				 * so we must check the previous proximity
				 * device. */
				if (common->wcmHasEraser &&
					(!common->wcmStylusSide))
				{
					curDevice = ERASER_ID;
				}
				else
				{
					curDevice = STYLUS_ID;
				}
			}
      
	/* We check here to see if we changed between eraser and stylus
	 * without leaving proximity. The most likely cause is that
	 * we were fooled by the second side switch into thinking the
	 * stylus was the eraser. If this happens, we send
	 * a proximity-out for the old device.  */

			if (curDevice != DEVICE_ID(priv->flags))
			{
				if (priv->oldProximity)
				{
					if (is_proximity && DEVICE_ID(priv->flags) == ERASER_ID)
					{
						curDevice = DEVICE_ID(priv->flags);
						temp_buttons = 0;
						temp_is_proximity = 0;
						common->wcmStylusSide = 1;
						DBG(10, ErrorF("eraser and stylus mix\n"));
					}
				}
				else 
					continue;
			}
      
			DBG(10, ErrorF((DEVICE_ID(priv->flags) == ERASER_ID) ? 
					"Eraser\n" : "Stylus\n"));
		}
		else
		{
			if (DEVICE_ID(priv->flags) != CURSOR_ID)
				continue; 
			DBG(10, ErrorF("Cursor\n"));
			curDevice = CURSOR_ID;
		}
  
		if (DEVICE_ID(priv->flags) != curDevice)
		{
			DBG(7, ErrorF("xf86WcmSendEvents not the same "
				"device type (%u,%u)\n",
				DEVICE_ID(priv->flags), curDevice));
			continue;
		}

		/* Hardware filtering isn't working on Graphire so
		 * we do it here. */
		if (((temp_is_proximity && priv->oldProximity) ||
			((temp_is_proximity == 0) &&
				(priv->oldProximity == 0))) &&
				(temp_buttons == priv->oldButtons) &&
				(ABS(x - priv->oldX) <= common->wcmSuppress) &&
				(ABS(y - priv->oldY) <= common->wcmSuppress) &&
				(ABS(z - priv->oldZ) < 3) &&
				(ABS(tx - priv->oldTiltX) < 3) &&
				(ABS(ty - priv->oldTiltY) < 3))
		{
			DBG(10, ErrorF("Graphire filtered\n"));
			return;
		}

		{
			/* hack, we'll get this whole function working later */
			WacomDeviceState ds = { 0 };
			ds.device_type = curDevice;
			ds.buttons = temp_buttons;
			ds.proximity = temp_is_proximity;
			ds.x = x;
			ds.y = y;
			ds.pressure = z;
			ds.tiltx = tx;
			ds.tilty = ty;
			ds.wheel = wheel;
			
			xf86WcmSendEvents(common->wcmDevices[idx],&ds);
		}

		if (!priv->oldProximity && temp_is_proximity )
		{
			/* handle the two sides switches in the stylus */
			if (is_stylus && (temp_buttons == 4))
				priv->oldProximity = ERASER_PROX;
			else
				priv->oldProximity = OTHER_PROX;
		}
	} /* next device */
}

static void ParseP4(WacomCommonPtr common)
{
	WacomDeviceState* orig = &common->wcmChannel[0].state;
	WacomDeviceState ds = *orig;
	int is_stylus;

	ds.proximity = (common->wcmData[0] & PROXIMITY_BIT);
     
	ds.x = (((common->wcmData[0] & 0x3) << 14) +
		(common->wcmData[1] << 7) +
		common->wcmData[2]);
	ds.y = (((common->wcmData[3] & 0x3) << 14) +
		(common->wcmData[4] << 7) +
		common->wcmData[5]);

	/* how about using the protocol version, rather than
	 * a user configurable value, eh? */

	if (common->wcmMaxZ > 350)
	{
		/* which tablets use this? */
		/* PL550, PL800, and Graphire apparently */
		ds.pressure = ((common->wcmData[6]&ZAXIS_BITS) << 2 ) +
			((common->wcmData[3]&ZAXIS_BIT) >> 1) +
			((common->wcmData[3]&PROXIMITY_BIT) >> 6) +
			((common->wcmData[6]&ZAXIS_SIGN_BIT) ? 0 : 0x100);
	}
	else if (common->wcmMaxZ > 150)
	{
		ds.pressure = ((common->wcmData[6] & ZAXIS_BITS) << 1 ) |
			((common->wcmData[3] & ZAXIS_BIT) >> 2) |
			((common->wcmData[6] & ZAXIS_SIGN_BIT) ? 0 : 0x80);
	}
	else
	{
		ds.pressure = (common->wcmData[6] & ZAXIS_BITS) |
			(common->wcmData[6] & ZAXIS_SIGN_BIT) ? 0 : 0x40;
	}

	if (common->wcmFlags & PL_FLAG)
		ds.buttons = (common->wcmData[3] & 0x38) >> 3;
	else
		ds.buttons = (common->wcmData[3] & BUTTONS_BITS) >> 3;

	/* If stylus comes into focus, use button to determine if eraser */
	is_stylus = (common->wcmData[0] & POINTER_BIT);
	if (is_stylus && !orig->proximity && ds.proximity)
		ds.device_type = (ds.buttons & 4) ? ERASER_ID : STYLUS_ID;

	/* If it is not a stylus, it's a cursor */
	else if (!is_stylus && !orig->proximity && ds.proximity)
		ds.device_type = CURSOR_ID;

	/* If it is out of proximity, there is no device type */
	else if (!ds.proximity)
		ds.device_type = 0;

	DBG(8, ErrorF("ParseP4 %s\n",
		ds.device_type == CURSOR_ID ? "CURSOR" :
		ds.device_type == ERASER_ID ? "ERASER " :
		ds.device_type == STYLUS_ID ? "STYLUS" : "NONE"));

	/* handle tilt values only for stylus */
	if (HANDLE_TILT(common))
	{
		ds.tiltx = (common->wcmData[7] & TILT_BITS);
		ds.tilty = (common->wcmData[8] & TILT_BITS);
		if (common->wcmData[7] & TILT_SIGN_BIT)
			ds.tiltx -= 64;
		if (common->wcmData[8] & TILT_SIGN_BIT)
			ds.tilty -= 64;
	}

	xf86WcmEvent(common,0,&ds);
}

static void ParseP5(WacomCommonPtr common)
{
	int is_stylus=0, have_data=0;
	int channel;
	WacomDeviceState ds;

	/* start with previous state */
	channel = common->wcmData[0] & 0x01;
	ds = common->wcmChannel[channel].state;

	DBG(7, ErrorF("packet header = 0x%x\n",
			(unsigned int)common->wcmData[0]));
     
	/* Device ID packet */
	if ((common->wcmData[0] & 0xfc) == 0xc0)
	{
		/* reset channel */
		memset(&ds, 0, sizeof(ds));

		ds.proximity = 1;
		ds.device_id = (((common->wcmData[1] & 0x7f) << 5) |
				((common->wcmData[2] & 0x7c) >> 2));
		ds.serial_num = (((common->wcmData[2] & 0x03) << 30) |
				((common->wcmData[3] & 0x7f) << 23) |
				((common->wcmData[4] & 0x7f) << 16) |
				((common->wcmData[5] & 0x7f) << 9) |
				((common->wcmData[6] & 0x7f) << 2) |
				((common->wcmData[7] & 0x60) >> 5));

		if ((ds.device_id & 0xf06) != 0x802)
			ds.discard_first = 1;

		if (STYLUS_TOOL(&ds))
			ds.device_type = STYLUS_ID;
		else if (CURSOR_TOOL(&ds))
			ds.device_type = CURSOR_ID;
		else
			ds.device_type = ERASER_ID;
  
		DBG(6, ErrorF("device_id=0x%x serial_num=%u type=%s\n",
			ds.device_id, ds.serial_num,
			(ds.device_type == STYLUS_ID) ? "stylus" :
			(ds.device_type == CURSOR_ID) ? "cursor" :
			"eraser"));
	}

	/* Out of proximity packet */
	else if ((common->wcmData[0] & 0xfe) == 0x80)
	{
		ds.proximity = 0;
		have_data = 1;
	}

	/* General pen packet or eraser packet or airbrush first packet
	 * airbrush second packet */
	else if (((common->wcmData[0] & 0xb8) == 0xa0) ||
			((common->wcmData[0] & 0xbe) == 0xb4))
	{
		is_stylus = 1;
		ds.x = (((common->wcmData[1] & 0x7f) << 9) |
				((common->wcmData[2] & 0x7f) << 2) |
				((common->wcmData[3] & 0x60) >> 5));
		ds.y = (((common->wcmData[3] & 0x1f) << 11) |
				((common->wcmData[4] & 0x7f) << 4) |
				((common->wcmData[5] & 0x78) >> 3));
		if ((common->wcmData[0] & 0xb8) == 0xa0)
		{
			ds.pressure = (((common->wcmData[5] & 0x07) << 7) |
				(common->wcmData[6] & 0x7f));
				ds.buttons = (((common->wcmData[0]) & 0x06) |
				(ds.pressure >= common->wcmThreshold));
		}
		else
		{
			ds.wheel = (((common->wcmData[5] & 0x07) << 7) |
				(common->wcmData[6] & 0x7f));
		}
		ds.tiltx = (common->wcmData[7] & TILT_BITS);
		ds.tilty = (common->wcmData[8] & TILT_BITS);
		if (common->wcmData[7] & TILT_SIGN_BIT)
			ds.tiltx -= (TILT_BITS + 1);
		if (common->wcmData[8] & TILT_SIGN_BIT)
			ds.tilty -= (TILT_BITS + 1);
		ds.proximity = (common->wcmData[0] & PROXIMITY_BIT);
		have_data = 1;
	} /* end pen packet */

	/* 4D mouse 1st packet or Lens cursor packet or 2D mouse packet*/
	else if (((common->wcmData[0] & 0xbe) == 0xa8) ||
			((common->wcmData[0] & 0xbe) == 0xb0))
	{
		is_stylus = 0;
		ds.x = (((common->wcmData[1] & 0x7f) << 9) |
				((common->wcmData[2] & 0x7f) << 2) |
				((common->wcmData[3] & 0x60) >> 5));
		ds.y = (((common->wcmData[3] & 0x1f) << 11) |
				((common->wcmData[4] & 0x7f) << 4) |
				((common->wcmData[5] & 0x78) >> 3));
		ds.tilty = 0;

		/* 4D mouse */
		if (MOUSE_4D(&ds))
		{
			ds.wheel = (((common->wcmData[5] & 0x07) << 7) |
				(common->wcmData[6] & 0x7f));
			if (common->wcmData[8] & 0x08) ds.wheel = -ds.wheel;
			ds.buttons = (((common->wcmData[8] & 0x70) >> 1) |
				(common->wcmData[8] & 0x07));
			have_data = !ds.discard_first;
		}

		/* Lens cursor */
		else if (LENS_CURSOR(&ds))
		{
			ds.buttons = common->wcmData[8];
			have_data = 1;
		}

		/* 2D mouse */
		else if (MOUSE_2D(&ds))
		{
			ds.buttons = (common->wcmData[8] & 0x1C) >> 2;
			ds.wheel = - (common->wcmData[8] & 1) +
					((common->wcmData[8] & 2) >> 1);
			have_data = 1;
		}

		ds.proximity = (common->wcmData[0] & PROXIMITY_BIT);
	} /* end 4D mouse 1st packet */

	/* 4D mouse 2nd packet */
	else if ((common->wcmData[0] & 0xbe) == 0xaa)
	{
		is_stylus = 0;
		ds.x = (((common->wcmData[1] & 0x7f) << 9) |
			((common->wcmData[2] & 0x7f) << 2) |
			((common->wcmData[3] & 0x60) >> 5));
		ds.y = (((common->wcmData[3] & 0x1f) << 11) |
			((common->wcmData[4] & 0x7f) << 4) |
			((common->wcmData[5] & 0x78) >> 3));
		ds.rotation = (((common->wcmData[6] & 0x0f) << 7) |
			(common->wcmData[7] & 0x7f));
		ds.rotation = ((900 - ((ds.rotation + 900) % 1800)) >> 1);
		ds.proximity = (common->wcmData[0] & PROXIMITY_BIT);
		have_data = 1;
		ds.discard_first = 0;
	}
	else
	{
		DBG(10, ErrorF("unknown wacom V packet 0x%x\n",
				common->wcmData[0]));
	}

	/* if new data is available, send it */
	if (have_data)
	       	xf86WcmEvent(common,channel,&ds);

	/* otherwise, initialize channel and wait for next packet */
	else
	{
		common->wcmChannel[channel].state = ds;
		common->wcmChannel[channel].pDev = NULL;
	}
}
