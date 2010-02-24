/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2009 by Ping Cheng, Wacom Technology. <pingc@wacom.com>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <linux/serial.h>
#include "xf86Wacom.h"
#include <xf86_OSproc.h>
#include "wcmFilter.h"

#define WC_ISDV4_QUERY "*"       /* ISDV4 query command */
#define WC_ISDV4_TOUCH_QUERY "%" /* ISDV4 touch query command */
#define WC_ISDV4_STOP "0"        /* ISDV4 stop command */
#define WC_ISDV4_SAMPLING "1"    /* ISDV4 sampling command */

#define RESET_RELATIVE(ds) do { (ds).relwheel = 0; } while (0)

static Bool isdv4Detect(LocalDevicePtr);
static Bool isdv4Init(LocalDevicePtr, char* id, float *version);
static void isdv4InitISDV4(WacomCommonPtr, const char* id, float version);
static int isdv4GetRanges(LocalDevicePtr);
static int isdv4StartTablet(LocalDevicePtr);
static int isdv4Parse(LocalDevicePtr, const unsigned char* data);
static int wcmSerialValidate(WacomCommonPtr common, const unsigned char* data);
static int wcmWaitForTablet(int fd, char * data, int size);
static int wcmWriteWait(int fd, const char* request);


	WacomDeviceClass gWacomISDV4Device =
	{
		isdv4Detect,
		isdv4Init,
		wcmReadPacket,
	};

	static WacomModel isdv4General =
	{
		"General ISDV4",
		isdv4InitISDV4,
		NULL,                 /* resolution not queried */
		isdv4GetRanges,       /* query ranges */
		isdv4StartTablet,     /* start tablet */
		isdv4Parse,
	};

static int wcmWait(int t)
{
	int err = xf86WaitForInput(-1, ((t) * 1000));
	if (err != -1)
		return Success;

	xf86Msg(X_ERROR, "Wacom select error : %s\n", strerror(errno));
	return err;
}

/*****************************************************************************
 * wcmSerialValidate -- validates serial packet; returns 0 on success,
 *   positive number of bytes to skip on error.
 ****************************************************************************/

static int wcmSerialValidate(WacomCommonPtr common, const unsigned char* data)
{
	int i, bad = 0;

	/* check magic */
	for (i=0; i<common->wcmPktLength; ++i)
	{
		if ( ((i==0) && !(data[i] & HEADER_BIT)) || 
				((i!=0) && (data[i] & HEADER_BIT)) )
		{
			bad = 1;
			if (i!=0 && (data[i] & HEADER_BIT)) {
				xf86Msg(X_WARNING, "wcmSerialValidate: "
					"bad magic at %d v=%x l=%d\n", i,
					data[i], common->wcmPktLength);
				return i;
			}
		}
	}
	if (bad) return common->wcmPktLength;
	else return 0;
}

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

	DBG(1, priv, "initializing ISDV4 tablet\n");

	/* Initial baudrate is 38400 */
	if (xf86SetSerialSpeed(local->fd, common->wcmISDV4Speed) < 0)
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

	DBG(1, priv, "Querying ISDV4 tablet\n");

	/* Send stop command to the tablet */
	err = xf86WriteSerial(local->fd, WC_ISDV4_STOP, strlen(WC_ISDV4_STOP));
	if (err == -1)
	{
		xf86Msg(X_WARNING, "%s: xf86WriteSerial ISDV4_STOP error : %s\n",
			 local->name, strerror(errno));
		return !Success;
	}

	/* Wait 250 mSecs */
	if (wcmWait(250))
		return !Success;
		
	/* Send query command to the tablet */
	if (!wcmWriteWait(local->fd, query))
	{
		xf86Msg(X_WARNING, "%s: unable to xf86WriteSerial request %s "
			"ISDV4 query command after %d tries\n", local->name,
			 query, MAXTRY);
		return !Success;
	}

	/* Read the control data */
	if (!wcmWaitForTablet(local->fd, data, WACOM_PKGLEN_TPCCTL))
	{
		/* Try 19200 if it is not a touch query */
		if (common->wcmISDV4Speed != 19200 && strcmp(query, WC_ISDV4_TOUCH_QUERY))
		{
			common->wcmISDV4Speed = 19200;
			if (xf86SetSerialSpeed(local->fd, common->wcmISDV4Speed) < 0)
				return !Success;
 			return isdv4Query(local, query, data);
		}
		else
		{
			xf86Msg(X_WARNING, "%s: unable to read ISDV4 %s data "
				"after %d tries at (%d)\n", local->name, query,
				 MAXTRY, common->wcmISDV4Speed);
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
			if (xf86SetSerialSpeed(local->fd, common->wcmISDV4Speed) < 0)
				return !Success;
 			return isdv4Query(local, query, data);
		}
		else
		{
			/* Reread the control data since it may fail the first time */
			wcmWaitForTablet(local->fd, data, WACOM_PKGLEN_TPCCTL);
			if ( !(data[0] & 0x40) )
			{
				xf86Msg(X_WARNING, "%s: ISDV4 control data "
					"(%x) error in %s query\n",
					local->name, data[0], query);
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
	/* length of a packet */
	common->wcmPktLength = WACOM_PKGLEN_TPCPEN;

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
	int ret = Success;

	DBG(2, priv, "getting ISDV4 Ranges\n");

	/* Send query command to the tablet */
	ret = isdv4Query(local, WC_ISDV4_QUERY, data);
	if (ret == Success)
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

		/* default to no pen 2FGT if size is undefined */
		if (!common->wcmMaxX || !common->wcmMaxY)
			common->tablet_id = 0xE2;

		DBG(2, priv, "Pen speed=%d "
			"maxX=%d maxY=%d maxZ=%d resX=%d resY=%d \n",
			common->wcmISDV4Speed, common->wcmMaxX, common->wcmMaxY,
			common->wcmMaxZ, common->wcmResolX, common->wcmResolY);
	}

	/* Touch might be supported. Send a touch query command */
	common->wcmISDV4Speed = 38400;
	if (isdv4Query(local, WC_ISDV4_TOUCH_QUERY, data) == Success)
	{
		switch (data[2] & 0x07)
		{
			case 0x00: /* resistive touch & pen */
				common->wcmPktLength = WACOM_PKGLEN_TOUCH93;
				common->tablet_id = 0x93;
				break;
			case 0x01: /* capacitive touch & pen */
				common->wcmPktLength = WACOM_PKGLEN_TOUCH9A;
				common->tablet_id = 0x9A;
				break;
			case 0x02: /* resistive touch */
				common->wcmPktLength = WACOM_PKGLEN_TOUCH93;
				common->tablet_id = 0x93;
				break;
			case 0x03: /* capacitive touch */
				common->wcmPktLength = WACOM_PKGLEN_TOUCH9A;
				common->tablet_id = 0x9F;
				break;
			case 0x04: /* capacitive touch */
				common->wcmPktLength = WACOM_PKGLEN_TOUCH9A;
				common->tablet_id = 0x9F;
				break;
			case 0x05:
				common->wcmPktLength = WACOM_PKGLEN_TOUCH2FG;
				/* a penabled */
				if (common->tablet_id == 0x90)
					common->tablet_id = 0xE3;
				break;
		}

		switch(data[0] & 0x3f)
		{
				/* single finger touch */
			case 0x01:
				if ((common->tablet_id != 0x93) &&
					(common->tablet_id != 0x9A) &&
					(common->tablet_id != 0x9F))

				{
				    xf86Msg(X_WARNING, "WACOM: %s tablet id(%x)"
					    " mismatch with data id (0x01) \n",
					    local->name, common->tablet_id);
				    return ret;
				}
				break;
				/* 2FGT */
			case 0x03:
				if ((common->tablet_id != 0xE2) &&
						(common->tablet_id != 0xE3))
				{
				    xf86Msg(X_WARNING, "WACOM: %s tablet id(%x)"
					    " mismatch with data id (0x03) \n",
					    local->name, common->tablet_id);
				    return ret;
				}
				break;
		}

		/* don't overwrite the default */
		if (((data[2]& 0x78) | data[3] | data[4] | data[5] | data[6]))
		{
			common->wcmMaxTouchX = ((data[3] << 9) |
				 (data[4] << 2) | ((data[2] & 0x60) >> 5));
			common->wcmMaxTouchY = ((data[5] << 9) |
				 (data[6] << 2) | ((data[2] & 0x18) >> 3));
		}
		else if (data[1])
			common->wcmMaxTouchX = common->wcmMaxTouchY =
				 (int)(1 << data[1]);

		if (data[1])
			common->wcmTouchResolX = common->wcmTouchResolY = 10;

		common->wcmVersion = ( data[10] | (data[9] << 7) );
		ret = Success;

		DBG(2, priv, "touch speed=%d "
			"maxTouchX=%d maxTouchY=%d TouchresX=%d TouchresY=%d \n",
			common->wcmISDV4Speed, common->wcmMaxTouchX,
			common->wcmMaxTouchY, common->wcmTouchResolX,
			common->wcmTouchResolY);
	}
	return ret;
}

static int isdv4StartTablet(LocalDevicePtr local)
{
	int err;

	/* Tell the tablet to start sending coordinates */
	err = xf86WriteSerial(local->fd, WC_ISDV4_SAMPLING, (strlen(WC_ISDV4_SAMPLING)));

	if (err == -1)
	{
		xf86Msg(X_ERROR, "%s: xf86WriteSerial error : %s\n", local->name, strerror(errno));
		return !Success;
	}

	return Success;
}

static int isdv4Parse(LocalDevicePtr local, const unsigned char* data)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceState* last = &common->wcmChannel[0].valid.state;
	WacomDeviceState* lastTemp = &common->wcmChannel[1].valid.state;
	WacomDeviceState* ds;
	int n, cur_type, channel = 0;

	DBG(10, common, "\n");

	/* determine the type of message (touch or stylus) */
	if (data[0] & 0x10) /* a touch data */
	{
		if ((last->device_id != TOUCH_DEVICE_ID && last->device_id &&
				 last->proximity ) || !common->wcmTouch)
		{
			/* ignore touch event */
			return common->wcmPktLength;
		}
	}
	else
	{
		/* touch was in control */
		if (last->proximity && last->device_id == TOUCH_DEVICE_ID)
		{
			/* let touch go */
			WacomDeviceState out = { 0 };
			out.device_type = TOUCH_ID;
			wcmEvent(common, channel, &out);
		}
	}

	if (common->buffer + common->bufpos - data < common->wcmPktLength)
	{
		/* we can't handle this yet */
		return common->wcmPktLength;
	}

	/* Coordinate data bit check */
	if (data[0] & 0x40) /* control data */
		return common->wcmPktLength;
	else if ((n = wcmSerialValidate(common,data)) > 0)
		return n;

	/* pick up where we left off, minus relative values */
	ds = &common->wcmChannel[channel].work;
	RESET_RELATIVE(*ds);

	if (common->wcmPktLength != WACOM_PKGLEN_TPCPEN) /* a touch */
	{
		ds->x = (((int)data[1]) << 7) | ((int)data[2]);
		ds->y = (((int)data[3]) << 7) | ((int)data[4]);
		if (common->wcmPktLength == WACOM_PKGLEN_TOUCH9A)
		{
			ds->capacity = (((int)data[5]) << 7) | ((int)data[6]);
		}
		ds->buttons = ds->proximity = data[0] & 0x01;
		ds->device_type = TOUCH_ID;
		ds->device_id = TOUCH_DEVICE_ID;

		if (common->wcmPktLength == WACOM_PKGLEN_TOUCH2FG)
		{
			if ((data[0] & 0x02) || (!(data[0] & 0x02) &&
					 lastTemp->proximity))
			{
				/* Got 2FGT. Send the first one if received */
				if (ds->proximity || (!ds->proximity &&
						 last->proximity))
				{
					/* time stamp for 2FGT gesture events */
					if ((ds->proximity && !last->proximity) ||
						    (!ds->proximity && last->proximity))
						ds->sample = (int)GetTimeInMillis();
					wcmEvent(common, channel, ds);
				}

				channel = 1;
				ds = &common->wcmChannel[channel].work;
				RESET_RELATIVE(*ds);
				ds->x = (((int)data[7]) << 7) | ((int)data[8]);
				ds->y = (((int)data[9]) << 7) | ((int)data[10]);
				ds->device_type = TOUCH_ID;
				ds->device_id = TOUCH_DEVICE_ID;
				ds->proximity = data[0] & 0x02;
				/* time stamp for 2FGT gesture events */
				if ((ds->proximity && !lastTemp->proximity) ||
					    (!ds->proximity && lastTemp->proximity))
					ds->sample = (int)GetTimeInMillis();
			}
		}

		DBG(8, priv, "MultiTouch "
			"%s proximity \n", ds->proximity ? "in" : "out of");
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
				wcmEvent(common, 0, &out);
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

		DBG(8, priv, "%s\n",
			ds->device_type == ERASER_ID ? "ERASER " :
			ds->device_type == STYLUS_ID ? "STYLUS" : "NONE");
	}
	wcmEvent(common, channel, ds);
	return common->wcmPktLength;
}

/*****************************************************************************
 * wcmWriteWait --
 *   send a request
 ****************************************************************************/

static int wcmWriteWait(int fd, const char* request)
{
	int len, maxtry = MAXTRY;

	/* send request string */
	do
	{
		len = xf86WriteSerial(fd, request, strlen(request));
		if ((len == -1) && (errno != EAGAIN))
		{
			xf86Msg(X_ERROR, "Wacom wcmWriteWait error : %s", strerror(errno));
			return 0;
		}

		maxtry--;

	} while ((len <= 0) && maxtry);

	return maxtry;
}

/*****************************************************************************
 * wcmWaitForTablet --
 *   wait for tablet data
 ****************************************************************************/

static int wcmWaitForTablet(int fd, char* answer, int size)
{
	int len, maxtry = MAXTRY;

	/* Read size bytes of the answer */
	do
	{
		if ((len = xf86WaitForInput(fd, 1000000)) > 0)
		{
			len = xf86ReadSerial(fd, answer, size);
			if ((len == -1) && (errno != EAGAIN))
			{
				xf86Msg(X_ERROR, "Wacom xf86ReadSerial error : %s\n",
						strerror(errno));
				return 0;
			}
		}
		maxtry--;
	} while ((len <= 0) && maxtry);

	return maxtry;
}

/**
 * Query the device's fd for the key bits and the tablet ID. Returns the ID
 * on success or 0 on failure.
 * For serial devices, we set the BTN_TOOL_DOUBLETAP etc. bits based on the
 * device ID. This matching only works for wacom devices (serial ID of
 * WACf), all others are simply assumed to be pen + erasor.
 */
int isdv4ProbeKeys(LocalDevicePtr local, unsigned long *keys)
{
	int id, i;
	int tablet_id = 0;
	struct serial_struct tmp;
	const char *device = xf86SetStrOption(local->options, "Device", NULL);

	if (ioctl(local->fd, TIOCGSERIAL, &tmp) < 0)
		return 0;

	/* check device name for ID first */
	if (sscanf(local->name, "WACf%x", &id) <= 1)
	{
		/* id in file sys/class/tty/%str/device/id */
		FILE *file;
		char sysfs_id[256];
		char *str = strstr(device, "ttyS");
		snprintf(sysfs_id, sizeof(sysfs_id),
				"/sys/class/tty/%s/device/id", str);
		file = fopen(sysfs_id, "r");

		/* return true since it falls to default */
		if (file)
		{
			/* make sure we fall to default */
			if (fscanf(file, "WACf%x\n", &id) <= 0)
				id = 0;

			fclose(file);
		}
	}

	/* we have tried memset. it doesn't work */
	for (i=0; i<NBITS(KEY_MAX); i++)
		keys[i] = 0;

	/* default to penabled */
	keys[LONG(BTN_TOOL_PEN)] |= BIT(BTN_TOOL_PEN);
	keys[LONG(BTN_TOOL_RUBBER)] |= BIT(BTN_TOOL_RUBBER);

	/* id < 0x008 are only penabled */
	if (id > 0x007)
		keys[LONG(BTN_TOOL_DOUBLETAP)] |= BIT(BTN_TOOL_DOUBLETAP);
	if (id > 0x0a)
		keys[LONG(BTN_TOOL_TRIPLETAP)] |= BIT(BTN_TOOL_TRIPLETAP);

	/* no pen 2FGT */
	if (id == 0x010)
	{
		keys[LONG(BTN_TOOL_PEN)] &= ~BIT(BTN_TOOL_PEN);
		keys[LONG(BTN_TOOL_RUBBER)] &= ~BIT(BTN_TOOL_RUBBER);
	}

	/* 0x9a and 0x9f are only detected by communicating
	 * with device.  This means tablet_id will be updated/refined
	 * at later stage and true knowledge of capacitive
	 * support will be delayed until that point.
	 */
	if (id >= 0x0 && id <= 0x7)
		tablet_id = 0x90;
	else if (id >= 0x8 && id <= 0xa)
		tablet_id = 0x93;
	else if (id >= 0xb && id <= 0xe)
		tablet_id = 0xe3;
	else if (id == 0x10)
		tablet_id = 0xe2;

	return tablet_id;
}

/* vim: set noexpandtab shiftwidth=8: */
