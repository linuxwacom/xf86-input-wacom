/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2007 by Ping Cheng, Wacom Technology. <pingc@wacom.com>		
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "xf86Wacom.h"
#include "wcmSerial.h"
#include "wcmFilter.h"

static Bool isdv4Detect(LocalDevicePtr);
static Bool isdv4Init(LocalDevicePtr);
static void isdv4InitISDV4(WacomCommonPtr, const char* id, float version);
static int isdv4GetRanges(LocalDevicePtr);
static int isdv4StartTablet(LocalDevicePtr);
static int isdv4Parse(LocalDevicePtr, const unsigned char* data);

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

	DBG(1, priv->debugLevel, ErrorF("initializing ISDV4 tablet\n"));    

	/* Try 19200 first */
	if (xf86WcmSetSerialSpeed(local->fd, common->wcmISDV4Speed) < 0)
		return !Success;
   
	/* Send stop command to the tablet */
	err = xf86WcmWrite(local->fd, WC_ISDV4_STOP, strlen(WC_ISDV4_STOP));
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
	/* set parameters */
	common->wcmProtocolLevel = 4;
	common->wcmPktLength = 5;       /* length of a packet 
					 * device packets are 9 bytes long,
					 * multitouch are only 5 */
	common->wcmResolX = 2540; 	/* tablet X resolution in points/inch */
	common->wcmResolY = 2540; 	/* tablet Y resolution in points/inch */
	common->wcmTPCButton = 1;	/* Tablet PC buttons on by default */
	common->wcmTPCButtonDefault = 1;
	common->tablet_id = 0x90;
}
static int isdv4GetRanges(LocalDevicePtr local)
{
	char data[BUFFER_SIZE];
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	int maxtry = MAXTRY, nr;
	WacomCommonPtr common =	priv->common;

	DBG(2, priv->debugLevel, ErrorF("getting ISDV4 Ranges\n"));
	/* Send query command to the tablet */
	do
	{
		nr = xf86WcmWrite(local->fd, WC_ISDV4_QUERY, strlen(WC_ISDV4_QUERY));
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
			nr = xf86WcmRead(local->fd, data, 11);
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
		/* Try 38400 now */
		if (common->wcmISDV4Speed != 38400)
		{
			common->wcmISDV4Speed = 38400;
			return isdv4Init(local);
		}
		else
		{
			ErrorF("Wacom Query ISDV4 error magic error \n");
			return !Success;
		}
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
	err = xf86WcmWrite(local->fd, WC_ISDV4_SAMPLING, (strlen(WC_ISDV4_SAMPLING)));

	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite error : %s\n", strerror(errno));
		return !Success;
	}

	return Success;
}

static int isdv4Parse(LocalDevicePtr local, const unsigned char* data)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceState* last = &common->wcmChannel[0].valid.state;
	WacomDeviceState* ds;
	int n, cur_type, ismt = 0;
	static int lastismt = 0;

	DBG(10, common->debugLevel, ErrorF("isdv4Parse \n"));

	/* determine the type of message */
	if (data[0] & 0x10)
	{
		ismt = 1;
		common->wcmPktLength = 5;
	}
	else
	{
		common->wcmPktLength = 9;
		if (common->buffer + common->bufpos - data < common->wcmPktLength)
		{
			/* we can't handle this yet */
			return 0;
		}
	}

	if ((n = xf86WcmSerialValidate(common,data)) > 0)
		return n;
	else
	{
		/* Coordinate data bit check */
		if (data[0] & 0x40)
			return common->wcmPktLength;
	}
	/* pick up where we left off, minus relative values */
	ds = &common->wcmChannel[0].work;
	RESET_RELATIVE(*ds);

	if (ismt)
	{
		if (!lastismt && last->pressure)
		{
			/* pen sends both pen and MultiTouch input, 
			 * since pressing it creates pressure. 
			 * We only want the pen input though.
			 */
			return common->wcmPktLength;
		}
		lastismt = ismt;

		/* MultiTouch input is comparably simple */
		ds->proximity = 0;
		ds->x = (((((int)data[1]) << 7) | ((int)data[2])) - 18) * common->wcmMaxX / 926;
		ds->y = (((((int)data[3]) << 7) | ((int)data[4])) - 51) * common->wcmMaxY / 934;
		ds->pressure = (data[0] & 0x01) * common->wcmMaxZ;
		ds->buttons = 1;
		ds->device_id = STYLUS_DEVICE_ID;
		ds->device_type = 0;
		DBG(8, priv->debugLevel, ErrorF("isdv4Parse MultiTouch\n"));
	}
	else
	{
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
		/* check on previous proximity */
		else if (cur_type == STYLUS_ID && ds->proximity)
		{
			/* we were fooled by tip and second
			 * sideswitch when it came into prox */
			if ((ds->device_type != cur_type) &&
				(ds->device_type == ERASER_ID))
			{
				/* send a prox-out for old device */
				WacomDeviceState out = { 0 };
				xf86WcmEvent(common, 0, &out);
				ds->device_type = cur_type;
			}
		}

		ds->device_id = (ds->device_type == CURSOR_ID) ? 
			CURSOR_DEVICE_ID : STYLUS_DEVICE_ID;

		/* don't send button 3 event for eraser 
		 * button 1 event will be sent by testing presure level
		 */
		if (ds->device_type == ERASER_ID && ds->buttons&4)
		{
			ds->buttons = 0;
			ds->device_id = ERASER_DEVICE_ID;
		}

		DBG(8, priv->debugLevel, ErrorF("isdv4Parse %s\n",
			ds->device_type == ERASER_ID ? "ERASER " :
			ds->device_type == STYLUS_ID ? "STYLUS" : "NONE"));
	}
	xf86WcmEvent(common,0,ds);
	return common->wcmPktLength;
}

