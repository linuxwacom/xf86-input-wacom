/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2005 by Ping Cheng, Wacom Technology. <pingc@wacom.com>		
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
#include "wcmFilter.h"

static Bool isdv4Detect(LocalDevicePtr);
static Bool isdv4Init(LocalDevicePtr);
static void isdv4InitISDV4(WacomCommonPtr, const char* id, float version);
static int isdv4GetRanges(LocalDevicePtr);
static int isdv4StartTablet(LocalDevicePtr);
static int isdv4Parse(WacomCommonPtr common, const unsigned char* data);

	WacomDeviceClass gWacomISDV4Device =
	{
		isdv4Detect,
		isdv4Init,
		xf86WcmReadPacket,
	};

	static WacomModel isdv4General =
	{
		"General ISDV4",
		isdv4InitISDV4,
		NULL,                 /* resolution not queried */
		isdv4GetRanges,       /* query ranges */
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		isdv4StartTablet,     /* start tablet */
		isdv4Parse,
		xf86WcmHysteresisFilter,   /* input filtering */
	};

/*****************************************************************************
 * isdv4Detect -- Test if the attached device is ISDV4.
 ****************************************************************************/

static Bool isdv4Detect(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	return (common->wcmForceDevice == DEVICE_ISDV4) ? 1 : 0;
}

/*****************************************************************************
 * isdv4Init --
 ****************************************************************************/

static Bool isdv4Init(LocalDevicePtr local)
{
	int err;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(1, ErrorF("initializing ISDV4 tablet\n"));    

	/* Set the speed of the serial link to 19200 */
	if (xf86WcmSetSerialSpeed(local->fd, 19200) < 0)
		return !Success;
   
	/* Send stop command to the tablet */
	SYSCALL(err = xf86WcmWrite(local->fd, WC_ISDV4_STOP, strlen(WC_ISDV4_STOP)));
	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	/* Wait 250 mSecs */
	if (xf86WcmWait(250))
		return !Success;

	return xf86WcmInitTablet(local,&isdv4General,"ISDV4", common->wcmVersion);
}

/*****************************************************************************
 * isdv4InitISDV4 -- Setup the device
 ****************************************************************************/

static void isdv4InitISDV4(WacomCommonPtr common, const char* id, float version)
{
	DBG(2, ErrorF("initializing as ISDV4 model\n"));
  
	/* set parameters */
	common->wcmProtocolLevel = 4;
	common->wcmPktLength = 9;       /* length of a packet */
	common->wcmResolX = 2540; 	/* tablet X resolution in points/inch */
	common->wcmResolY = 2540; 	/* tablet Y resolution in points/inch */
	common->wcmTPCButton = 1;	/* Tablet PC buttons on by default */
}
static int isdv4GetRanges(LocalDevicePtr local)
{
	char data[BUFFER_SIZE];
	int maxtry = MAXTRY, nr;
	WacomCommonPtr common =	((WacomDevicePtr)(local->private))->common;

	DBG(2, ErrorF("getting ISDV4 Ranges\n"));
	/* Send query command to the tablet */
	do
	{
		SYSCALL(nr = xf86WcmWrite(local->fd, WC_ISDV4_QUERY, strlen(WC_ISDV4_QUERY)));
		if ((nr == -1) && (errno != EAGAIN))
		{
			ErrorF("Wacom xf86WcmWrite error : %s", strerror(errno));
			return !Success;
		}
		maxtry--;
	} while ((nr == -1) && maxtry);

	if (maxtry == 0)
	{
		ErrorF("Wacom unable to xf86WcmWrite request query command "
				"after %d tries\n", MAXTRY);
		return !Success;
	}

	/* Read the control data */
	maxtry = MAXTRY;
	do
	{
		if ((nr = xf86WcmWaitForTablet(local->fd)) > 0)
		{
			SYSCALL(nr = xf86WcmRead(local->fd, data, 11));
			if ((nr == -1) && (errno != EAGAIN))
			{
				ErrorF("Wacom xf86WcmRead error : %s\n", strerror(errno));
				return !Success;
			}
		}
		maxtry--;  
	} while ( nr <= 0 && maxtry );

	if (maxtry == 0 && nr <= 0 )
	{
		ErrorF("Wacom unable to read ISDV4 control data "
				"after %d tries\n", MAXTRY);
		return !Success;
	}

	/* Control data bit check */
	if ( !(data[0] & 0x40) )
	{
		ErrorF("Wacom Query ISDV4 error magic error \n");
		return !Success;
	}
	
	common->wcmMaxZ = ( data[5] | ((data[6] & 0x07) << 7) );
	common->wcmMaxX = ( (data[1] << 9) | (data[2] << 2) 
				| ( (data[6] & 0x60) >> 5) );      
	common->wcmMaxY = ( (data[3] << 9) | (data[4] << 2 ) 
				| ( (data[6] & 0x18) >> 3) );
	common->wcmVersion = ( data[10] | (data[9] << 7) );

	return Success;
}

static int isdv4StartTablet(LocalDevicePtr local)
{
	int err;

	/* Tell the tablet to start sending coordinates */
	SYSCALL(err = xf86WcmWrite(local->fd, WC_ISDV4_SAMPLING, (strlen(WC_ISDV4_SAMPLING))));

	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	return Success;
}

static int isdv4Parse(WacomCommonPtr common, const unsigned char* data)
{
	WacomDeviceState* last = &common->wcmChannel[0].valid.state;
	WacomDeviceState* ds;
	int n, cur_type;

	if ((n = xf86WcmSerialValidate(common,data)) > 0)
	{
		return n;
	}
	else
	{
		/* Coordinate data bit check */
		if (data[0] & 0x40)
			return common->wcmPktLength;
	}
	/* pick up where we left off, minus relative values */
	ds = &common->wcmChannel[0].work;
	RESET_RELATIVE(*ds);

	ds->proximity = (data[0] & 0x20);

	/* x and y in "normal" orientetion (wide length is X) */
	ds->x = (((int)data[6] & 0x60) >> 5) | ((int)data[2] << 2) |
		((int)data[1] << 9);
	ds->y = (((int)data[6] & 0x18) >> 3) | ((int)data[4] << 2) |
		((int)data[3] << 9);

	/* pressure */
	ds->pressure = (((data[6] & 0x07) << 7) | data[5] );

	/* buttons */
	ds->buttons = (data[0] & 0x07);

	/* check which device we have */
	cur_type = (ds->buttons & 4) ? ERASER_ID : STYLUS_ID;

	/* first time into prox */
	if (!last->proximity && ds->proximity) 
		ds->device_type = cur_type;

	/* out of prox */
	else if (!ds->proximity)
		memset(ds,0,sizeof(*ds));

	/* check on previous proximity */
	else
	{
		/* we were fooled by tip and second
		 * sideswitch when it came into prox */
		if ((ds->device_type != cur_type) &&
			(ds->device_type == ERASER_ID))
		{
			/* send a prox-out for old device */
			WacomDeviceState out = { 0 };
			xf86WcmEvent(common,0,&out);
			ds->device_type = cur_type;
		}
	}

	DBG(8, ErrorF("isdv4Parse %s\n",
		ds->device_type == ERASER_ID ? "ERASER " :
		ds->device_type == STYLUS_ID ? "STYLUS" : "NONE"));

	xf86WcmEvent(common,0,ds);

	return common->wcmPktLength;
}

