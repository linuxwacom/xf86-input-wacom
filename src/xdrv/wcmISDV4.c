/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2008 by Ping Cheng, Wacom Technology. <pingc@wacom.com>		
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
static Bool isdv4Init(LocalDevicePtr, char* id, float *version);
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

static Bool isdv4Init(LocalDevicePtr local, char* id, float *version)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	DBG(1, priv->debugLevel, ErrorF("initializing ISDV4 tablet\n"));

	/* Initial baudrate is 38400 */
	if (xf86WcmSetSerialSpeed(local->fd, common->wcmISDV4Speed) < 0)
		return !Success;

	if(id)
		strcpy(id, "ISDV4");
	if(version)
		*version = common->wcmVersion;

	/*set the model */
	common->wcmModel = &isdv4General;

	return Success;
}

/*****************************************************************************
 * isdv4Query -- Query the device
 ****************************************************************************/

static int isdv4Query(LocalDevicePtr local, const char* query, char* data)
{
	int err;
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common =	priv->common;

	DBG(1, priv->debugLevel, ErrorF("Querying ISDV4 tablet\n"));

	/* Send stop command to the tablet */
	err = xf86WcmWrite(local->fd, WC_ISDV4_STOP, strlen(WC_ISDV4_STOP));
	if (err == -1)
	{
		ErrorF("Wacom xf86WcmWrite ISDV4_STOP error : %s\n", strerror(errno));
		return !Success;
	}

	/* Wait 250 mSecs */
	if (xf86WcmWait(250))
		return !Success;
		
	/* Send query command to the tablet */
	if (!xf86WcmWriteWait(local->fd, query))
	{
		ErrorF("Wacom unable to xf86WcmWrite request %s ISDV4 query command "
			"after %d tries\n", query, MAXTRY);
		return !Success;
	}

	/* Read the control data */
	if (!xf86WcmWaitForTablet(local->fd, data, 11))
	{
		/* Try 19200 if it is not a touch query */
		if (common->wcmISDV4Speed != 19200 && strcmp(query, WC_ISDV4_TOUCH_QUERY))
		{
			common->wcmISDV4Speed = 19200;
			if (xf86WcmSetSerialSpeed(local->fd, common->wcmISDV4Speed) < 0)
				return !Success;
 			return isdv4Query(local, query, data);
		}
		else
		{
			ErrorF("Wacom unable to read ISDV4 %s data "
				"after %d tries at (%d)\n", query, MAXTRY, common->wcmISDV4Speed);
			return !Success;
		}
	}

	/* Control data bit check */
	if ( !(data[0] & 0x40) )
	{
		/* Try 19200 if it is not a touch query */
		if (common->wcmISDV4Speed != 19200 && strcmp(query, WC_ISDV4_TOUCH_QUERY))
		{
			common->wcmISDV4Speed = 19200;
			if (xf86WcmSetSerialSpeed(local->fd, common->wcmISDV4Speed) < 0)
				return !Success;
 			return isdv4Query(local, query, data);
		}
		else
		{
			/* Reread the control data since with some vendors it fails the first time */
			xf86WcmWaitForTablet(local->fd, data, 11);
			if ( !(data[0] & 0x40) )
			{
				ErrorF("Wacom ISDV4 control data (%x) error in %s query\n", data[0], query);
				return !Success;
			}
		}
	}

	return Success;
}

/*****************************************************************************
 * isdv4InitISDV4 -- Setup the device
 ****************************************************************************/

static void isdv4InitISDV4(WacomCommonPtr common, const char* id, float version)
{
	/* set parameters */
	common->wcmProtocolLevel = 4;
	common->wcmPktLength = 9;       /* length of a packet 
					 * device packets are 9 bytes
					 * resistive touch is 5 bytes
					 * capacitive touch is 7 bytes 
					 */

	/* digitizer X resolution in points/inch */
	common->wcmResolX = 2540; 	
	/* digitizer Y resolution in points/inch */
	common->wcmResolY = 2540; 	

	/* no touch */
	common->tablet_id = 0x90;

	/* tilt disabled */
	common->wcmFlags &= ~TILT_ENABLED_FLAG;
}

/*****************************************************************************
 * isdv4GetRanges -- get ranges of the device
 ****************************************************************************/

static int isdv4GetRanges(LocalDevicePtr local)
{
	char data[BUFFER_SIZE];
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common =	priv->common;
	char * s;

	DBG(2, priv->debugLevel, ErrorF("getting ISDV4 Ranges\n"));

	/* Send query command to the tablet */
	if (isdv4Query(local, WC_ISDV4_QUERY, data) == Success)
	{
		/* transducer data */
		common->wcmMaxZ = ( data[5] | ((data[6] & 0x07) << 7) );
		common->wcmMaxX = ( (data[1] << 9) | 
			(data[2] << 2) | ( (data[6] & 0x60) >> 5) );      
		common->wcmMaxY = ( (data[3] << 9) | (data[4] << 2 ) 
			| ( (data[6] & 0x18) >> 3) );
		if (data[7] && data[8])
		{
			common->wcmMaxtiltX = data[7] + 1;
			common->wcmMaxtiltY = data[8] + 1;
			common->wcmFlags |= TILT_ENABLED_FLAG;
		}
			
		common->wcmVersion = ( data[10] | (data[9] << 7) );
	}
	else
		return !Success;

	if (common->wcmISDV4Speed != 19200)
	{
		/* default to 0x93 (resistive touch) */
		common->wcmPktLength = 5;
		common->tablet_id = 0x93;

		/* Touch might be supported. Send a touch query command */
		if (isdv4Query(local, WC_ISDV4_TOUCH_QUERY, data) == Success)
		{
			/* (data[2] & 0x07) == 0 is for resistive touch */
			if ((data[0] & 0x41) && (data[2] & 0x07))
			{
				/* tablet model */
				switch (data[2] & 0x07)
				{
					case 0x01:
						common->wcmPktLength = 7;
						common->tablet_id = 0x9A;
					break;
					case 0x02:
					case 0x04:
						common->wcmPktLength = 7;
						common->tablet_id = 0x9F;
					break;
				}

				/* touch logical size for tablet PC with touch */
				if (data[1])
				{
					common->wcmMaxTouchX = common->wcmMaxTouchY = (int)(1 << data[1]);
				}

				/* Max capacity */
				common->wcmMaxCapacity = (int)(1 << data[7]);

				if (common->wcmMaxCapacity)
				{
					common->wcmCapacityDefault = 3;
					common->wcmCapacity = 3;
					common->wcmTouchResolX = common->wcmMaxTouchX / ( 2540 * 
						((data[3] << 9) | (data[4] << 2) | ((data[2] & 0x60) >> 5)));
					common->wcmTouchResolX = common->wcmMaxTouchX / ( 2540 * 
						((data[5] << 9) | (data[6] << 2) | ((data[2] & 0x18) >> 3)));
				}
				else
				{
					common->wcmCapacityDefault = -1;
					common->wcmCapacity = -1;
				}
			}
		}

		s = xf86FindOptionValue(local->options, "Touch");
		if ( !s || (strstr(s, "on")) )  /* touch option is on */
		{
			common->wcmTouch = 1;
		}

		/* TouchDefault was off for all devices
		 * defaults to enable when touch is supported 
		 */
		if (common->wcmTouch)
		{
			common->wcmTouchDefault = 1;
		}

		if (common->wcmMaxX && common->wcmMaxY && !common->wcmTouchResolX)
		{
			/* Some touch tablet don't report physical size */
			common->wcmTouchResolX = common->wcmMaxTouchX / 
				(common->wcmResolX * common->wcmMaxX);
			common->wcmTouchResolY = common->wcmMaxTouchY / 
				(common->wcmResolY * common->wcmMaxY);
		}
	}

	DBG(2, priv->debugLevel, ErrorF("isdv4GetRanges speed=%d maxX=%d maxY=%d "
		"maxZ=%d TouchresX=%d TouchresY=%d \n", common->wcmISDV4Speed, 
		common->wcmMaxX, common->wcmMaxY, common->wcmMaxZ,
		common->wcmTouchResolX, common->wcmTouchResolY));
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
	int n, cur_type, channel = 0;
	static int touchInProx;

	DBG(10, common->debugLevel, ErrorF("isdv4Parse \n"));

	/* determine the type of message (touch or stylus)*/
	if (data[0] & 0x18) /* not a pen */
	{
		if ((last->device_id != TOUCH_DEVICE_ID && last->device_id && last->proximity ) || 
				!common->wcmTouch )
		{
			if ((data[0] & 0x10) && (!(data[0] & 0x01))) /* a touch out-prox data */
				touchInProx = 0;
			else
				touchInProx = 1;

			/* ignore touch event */
			return common->wcmPktLength;
		}
		else
		{
			if (data[0] & 0x10) /* a touch data */
			{
				if (!touchInProx)
				{
					channel = 1;
				} 
				else if (!(data[0] & 0x01)) /* touch out-prox */
				{
					touchInProx = 0;
					channel = 1;
				} 
				else
				{
					/* ignore touch event */
					return common->wcmPktLength;
				}
			}
			else
			{
				/* ignore touch event */
				return common->wcmPktLength;
			}
		}
	}
	else
	{
		/* touch was in control */
		if (common->wcmChannel[1].valid.state.proximity)
		{
			/* let touch go */
			WacomDeviceState out = { 0 };
			out.device_type = TOUCH_ID;
			xf86WcmEvent(common, 1, &out);
			return 0;
		}
		common->wcmPktLength = 9;
		channel = 0;
	}

	if (common->buffer + common->bufpos - data < common->wcmPktLength)
	{
		/* we can't handle this yet */
		return common->wcmPktLength;
	}

	if ((n = xf86WcmSerialValidate(common,data)) > 0)
		return n;
	else
	{
		/* Coordinate data bit check */
		if (data[0] & 0x40) /* control data */
			return common->wcmPktLength;
	}

	/* pick up where we left off, minus relative values */
	ds = &common->wcmChannel[channel].work;
	RESET_RELATIVE(*ds);

	if (common->wcmPktLength == 5 || common->wcmPktLength == 7) /* a touch */
	{
		/* touch without capacity has 5 bytes of data 
		 * touch with capacity has 7 bytes of data
		 */
		ds->x = (((int)data[1]) << 7) | ((int)data[2]);
		ds->y = (((int)data[3]) << 7) | ((int)data[4]);
		if (common->wcmPktLength == 7)
		{
			ds->capacity = (((int)data[5]) << 7) | ((int)data[6]);
		}
		ds->buttons = ds->proximity = data[0] & 0x01;
		ds->device_type = TOUCH_ID;
		ds->device_id = TOUCH_DEVICE_ID;
		DBG(8, priv->debugLevel, ErrorF("isdv4Parse MultiTouch "
			"%s proximity \n", ds->proximity ? "in" : "out of"));
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
		else if (ds->buttons && ds->proximity)
		{
			/* we might have been fooled by tip and second
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

		ds->device_id = (ds->device_type == ERASER_ID) ? 
			ERASER_DEVICE_ID : STYLUS_DEVICE_ID;

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
	xf86WcmEvent(common, channel, ds);
	return common->wcmPktLength;
}

