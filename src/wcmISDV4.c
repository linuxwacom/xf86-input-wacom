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

#include "xf86Wacom.h"
#include <xf86_OSproc.h>
#include "wcmFilter.h"
#include <linux/serial.h>
#include "wcmISDV4.h"

#define ISDV4_QUERY "*"       /* ISDV4 query command */
#define ISDV4_TOUCH_QUERY "%" /* ISDV4 touch query command */
#define ISDV4_STOP "0"        /* ISDV4 stop command */
#define ISDV4_SAMPLING "1"    /* ISDV4 sampling command */

#define RESET_RELATIVE(ds) do { (ds).relwheel = 0; } while (0)

static Bool isdv4Detect(LocalDevicePtr);
static Bool isdv4Init(LocalDevicePtr, char* id, float *version);
static void isdv4InitISDV4(WacomCommonPtr, const char* id, float version);
static int isdv4GetRanges(LocalDevicePtr);
static int isdv4StartTablet(LocalDevicePtr);
static int isdv4StopTablet(LocalDevicePtr);
static int isdv4Parse(LocalDevicePtr, const unsigned char* data, int len);
static int wcmSerialValidate(LocalDevicePtr local, const unsigned char* data);
static int wcmWaitForTablet(LocalDevicePtr local, char * data, int size);
static int wcmWriteWait(LocalDevicePtr local, const char* request);

static inline int isdv4ParseQuery(const char *buffer, const size_t len,
				  ISDV4QueryReply *reply);
static inline int isdv4ParseTouchQuery(const char *buffer, const size_t len,
					ISDV4TouchQueryReply *reply);

static inline int isdv4ParseTouchData(const unsigned char *buffer, const size_t len,
				      const size_t pktlen, ISDV4TouchData *touchdata);

static inline int isdv4ParseCoordinateData(const unsigned char *buffer, const size_t len,
					   ISDV4CoordinateData *coord);

	WacomDeviceClass gWacomISDV4Device =
	{
		isdv4Detect,
		isdv4Init,
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
 * wcmSkipInvalidBytes - returns the number of bytes to skip if the first
 * byte of data does not denote a valid header byte.
 * The ISDV protocol requires that the first byte of a new packet has the
 * HEADER_BIT set and subsequent packets do not.
 ****************************************************************************/
static int wcmSkipInvalidBytes(const unsigned char* data, int len)
{
	int n = 0;

	while(n < len && !(data[n] & HEADER_BIT))
		n++;

	return n;
}



/*****************************************************************************
 * wcmSerialValidate -- validates serial packet; returns 0 on success,
 *   positive number of bytes to skip on error.
 ****************************************************************************/

static int wcmSerialValidate(LocalDevicePtr local, const unsigned char* data)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;

	int n;

	/* First byte must have header bit set, if not, skip until next
	 * header byte */
	if (!(data[0] & HEADER_BIT))
	{
		int n = wcmSkipInvalidBytes(data, common->wcmPktLength);
		xf86Msg(X_WARNING,
			"%s: missing header bit. skipping %d bytes.\n",
			local->name, n);
		return n;
	}

	/* Remainder must _not_ have header bit set, if not, skip to first
	 * header byte. wcmSkipInvalidBytes gives us the number of bytes
	 * without the header bit set, so use the next one.
	 */
	n = wcmSkipInvalidBytes(&data[1], common->wcmPktLength - 1);
	n += 1; /* the header byte we already checked */
	if (n != common->wcmPktLength) {
		xf86Msg(X_WARNING, "%s: bad data at %d v=%x l=%d\n", local->name,
			n, data[n], common->wcmPktLength);
		return n;
	}

	return 0;
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
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common =	priv->common;

	DBG(1, priv, "Querying ISDV4 tablet\n");

	if (isdv4StopTablet(local) != Success)
		return !Success;

	/* Send query command to the tablet */
	if (!wcmWriteWait(local, query))
		return !Success;

	/* Read the control data */
	if (!wcmWaitForTablet(local, data, ISDV4_PKGLEN_TPCCTL))
	{
		/* Try 19200 if it is not a touch query */
		if (common->wcmISDV4Speed != 19200 && strcmp(query, ISDV4_TOUCH_QUERY))
		{
			common->wcmISDV4Speed = 19200;
			if (xf86SetSerialSpeed(local->fd, common->wcmISDV4Speed) < 0)
				return !Success;
 			return isdv4Query(local, query, data);
		}
		else
			return !Success;
	}

	/* Control data bit check */
	if ( !(data[0] & 0x40) )
	{
		/* Try 19200 if it is not a touch query */
		if (common->wcmISDV4Speed != 19200 && strcmp(query, ISDV4_TOUCH_QUERY))
		{
			common->wcmISDV4Speed = 19200;
			if (xf86SetSerialSpeed(local->fd, common->wcmISDV4Speed) < 0)
				return !Success;
 			return isdv4Query(local, query, data);
		}
		else
		{
			/* Reread the control data since it may fail the first time */
			wcmWaitForTablet(local, data, ISDV4_PKGLEN_TPCCTL);
			if ( !(data[0] & 0x40) )
				return !Success;
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
	common->wcmPktLength = ISDV4_PKGLEN_TPCPEN;

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
	ret = isdv4Query(local, ISDV4_QUERY, data);
	if (ret == Success)
	{
		ISDV4QueryReply reply;
		int rc;

		rc = isdv4ParseQuery(data, sizeof(data), &reply);
		if (rc <= 0)
		{
			xf86Msg(X_ERROR, "Error while parsing ISDV4 query.\n");
			return BadAlloc;
		}

		/* transducer data */
		common->wcmMaxZ = reply.pressure_max;
		common->wcmMaxX = reply.x_max;
		common->wcmMaxY = reply.y_max;
		if (reply.tilt_x_max && reply.tilt_y_max)
		{
			common->wcmMaxtiltX = reply.tilt_x_max;
			common->wcmMaxtiltY = reply.tilt_y_max;
			common->wcmFlags |= TILT_ENABLED_FLAG;
		}

		common->wcmVersion = reply.version;

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
	if (isdv4Query(local, ISDV4_TOUCH_QUERY, data) == Success)
	{
		ISDV4TouchQueryReply reply;
		int rc;

		rc = isdv4ParseTouchQuery(data, sizeof(data), &reply);
		if (rc <= 0)
		{
			xf86Msg(X_ERROR, "Error while parsing ISDV4 touch query.\n");
			return BadAlloc;
		}

		switch (reply.sensor_id)
		{
			case 0x00: /* resistive touch & pen */
				common->wcmPktLength = ISDV4_PKGLEN_TOUCH93;
				common->tablet_id = 0x93;
				break;
			case 0x01: /* capacitive touch & pen */
				common->wcmPktLength = ISDV4_PKGLEN_TOUCH9A;
				common->tablet_id = 0x9A;
				break;
			case 0x02: /* resistive touch */
				common->wcmPktLength = ISDV4_PKGLEN_TOUCH93;
				common->tablet_id = 0x93;
				break;
			case 0x03: /* capacitive touch */
				common->wcmPktLength = ISDV4_PKGLEN_TOUCH9A;
				common->tablet_id = 0x9F;
				break;
			case 0x04: /* capacitive touch */
				common->wcmPktLength = ISDV4_PKGLEN_TOUCH9A;
				common->tablet_id = 0x9F;
				break;
			case 0x05:
				common->wcmPktLength = ISDV4_PKGLEN_TOUCH2FG;
				/* a penabled */
				if (common->tablet_id == 0x90)
					common->tablet_id = 0xE3;
				break;
		}

		switch(reply.data_id)
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
		if (reply.x_max | reply.y_max)
		{
			common->wcmMaxTouchX = reply.x_max;
			common->wcmMaxTouchY = reply.y_max;
		}
		else if (reply.panel_resolution)
			common->wcmMaxTouchX = common->wcmMaxTouchY =
				(1 << reply.panel_resolution);

		if (reply.panel_resolution)
			common->wcmTouchResolX = common->wcmTouchResolY = 10;

		common->wcmVersion = reply.version;
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
	/* Tell the tablet to start sending coordinates */
	if (!wcmWriteWait(local, ISDV4_SAMPLING))
		return !Success;

	return Success;
}

static int isdv4StopTablet(LocalDevicePtr local)
{
	/* Send stop command to the tablet */
	if (!wcmWriteWait(local, ISDV4_STOP))
		return !Success;

	/* Wait 250 mSecs */
	if (wcmWait(250))
		return !Success;

	return Success;
}

static int isdv4Parse(LocalDevicePtr local, const unsigned char* data, int len)
{
	WacomDevicePtr priv = (WacomDevicePtr)local->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceState* last = &common->wcmChannel[0].valid.state;
	WacomDeviceState* lastTemp = &common->wcmChannel[1].valid.state;
	WacomDeviceState* ds;
	int n, cur_type, channel = 0;

	DBG(10, common, "\n");

	if ((n = wcmSkipInvalidBytes(data, len)) > 0)
		return n;

	/* choose wcmPktLength if it is not an out-prox event */
	if (data[0])
		common->wcmPktLength = ISDV4_PKGLEN_TPCPEN;

	if ( data[0] & 0x10 )
	{
		/* set touch PktLength */
		common->wcmPktLength = ISDV4_PKGLEN_TOUCH93;
		if ((common->tablet_id == 0x9A) || (common->tablet_id == 0x9F))
			common->wcmPktLength = ISDV4_PKGLEN_TOUCH9A;
		if ((common->tablet_id == 0xE2) || (common->tablet_id == 0xE3))
			common->wcmPktLength = ISDV4_PKGLEN_TOUCH2FG;
	}

	if (len < common->wcmPktLength)
		return 0;

	/* determine the type of message (touch or stylus) */
	if (data[0] & TOUCH_CONTROL_BIT) /* a touch data */
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

	/* Coordinate data bit check */
	if (data[0] & CONTROL_BIT) /* control data */
		return common->wcmPktLength;
	else if ((n = wcmSerialValidate(local,data)) > 0)
		return n;

	/* pick up where we left off, minus relative values */
	ds = &common->wcmChannel[channel].work;
	RESET_RELATIVE(*ds);

	if (common->wcmPktLength != ISDV4_PKGLEN_TPCPEN) /* a touch */
	{
		ISDV4TouchData touchdata;
		int rc;

		rc = isdv4ParseTouchData(data, len, common->wcmPktLength, &touchdata);
		if (rc == -1)
		{
			xf86Msg(X_ERROR, "%s: failed to parse touch data.\n",
				local->name);
			return 0;
		}


		ds->x = touchdata.x;
		ds->y = touchdata.y;
		ds->capacity = touchdata.capacity;
		ds->buttons = ds->proximity = touchdata.status;
		ds->device_type = TOUCH_ID;
		ds->device_id = TOUCH_DEVICE_ID;

		if (common->wcmPktLength == ISDV4_PKGLEN_TOUCH2FG)
		{
			if (touchdata.finger2.status ||
			    (!touchdata.finger2.status && lastTemp->proximity))
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
				ds->x = touchdata.finger2.x;
				ds->y = touchdata.finger2.y;
				ds->device_type = TOUCH_ID;
				ds->device_id = TOUCH_DEVICE_ID;
				ds->proximity = touchdata.finger2.status;
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
		int rc;
		ISDV4CoordinateData coord;

		rc = isdv4ParseCoordinateData(data, ISDV4_PKGLEN_TPCPEN, &coord);

		if (rc == -1)
		{
			xf86Msg(X_ERROR, "%s: failed to parse coordinate data.\n", local->name);
			return 0;
		}

		ds->proximity = coord.proximity;

		/* x and y in "normal" orientetion (wide length is X) */
		ds->x = coord.x;
		ds->y = coord.y;

		/* pressure */
		ds->pressure = coord.pressure;

		/* buttons */
		ds->buttons = coord.tip | (coord.side << 1) | (coord.eraser << 2);

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

static int wcmWriteWait(LocalDevicePtr local, const char* request)
{
	int len, maxtry = MAXTRY;

	/* send request string */
	do
	{
		len = xf86WriteSerial(local->fd, request, strlen(request));
		if ((len == -1) && (errno != EAGAIN))
		{
			xf86Msg(X_ERROR, "%s: wcmWriteWait error : %s",
					local->name, strerror(errno));
			return 0;
		}

		maxtry--;

	} while ((len <= 0) && maxtry);

	if (!maxtry)
		xf86Msg(X_WARNING, "%s: Failed to issue command '%s' "
				   "after %d tries.\n", local->name, request, MAXTRY);

	return maxtry;
}

/*****************************************************************************
 * wcmWaitForTablet --
 *   wait for tablet data
 ****************************************************************************/

static int wcmWaitForTablet(LocalDevicePtr local, char* answer, int size)
{
	int len, maxtry = MAXTRY;

	/* Read size bytes of the answer */
	do
	{
		if ((len = xf86WaitForInput(local->fd, 1000000)) > 0)
		{
			len = xf86ReadSerial(local->fd, answer, size);
			if ((len == -1) && (errno != EAGAIN))
			{
				xf86Msg(X_ERROR, "%s: xf86ReadSerial error : %s\n",
						local->name, strerror(errno));
				return 0;
			}
		}
		maxtry--;
	} while ((len <= 0) && maxtry);

	if (!maxtry)
		xf86Msg(X_WARNING, "%s: Waited too long for answer "
				   "(failed after %d tries).\n",
				   local->name, MAXTRY);

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

/* Convert buffer data of buffer sized len into a query reply.
 * Returns the number of bytes read from buffer or 0 if the buffer was on
 * insufficient length. Returns -1 on parsing or internal errors.
 */
static inline int isdv4ParseQuery(const char *buffer, const size_t len,
				  ISDV4QueryReply *reply)
{
	int header, control;

	if (!reply || len < ISDV4_PKGLEN_TPCCTL)
		return 0;

	header = !!(buffer[0] & HEADER_BIT);
	control = !!(buffer[0] & CONTROL_BIT);

	if (!header || !control)
		return -1;

	reply->data_id = buffer[0] & DATA_ID_MASK;

	/* FIXME: big endian? */
	reply->x_max = (buffer[1] << 9) | (buffer[2] << 2) | ((buffer[6] >> 5) & 0x3);
	reply->y_max = (buffer[3] << 9) | (buffer[4] << 2) | ((buffer[6] >> 3) & 0x3);
	reply->pressure_max = buffer[5] | (buffer[6] & 0x7);
	reply->tilt_y_max = buffer[7];
	reply->tilt_x_max = buffer[8];
	reply->version = buffer[9] << 7 | buffer[10];

	return ISDV4_PKGLEN_TPCCTL;
}

static inline int isdv4ParseTouchQuery(const char *buffer, const size_t len,
					ISDV4TouchQueryReply *reply)
{
	int header, control;

	if (!reply || len < ISDV4_PKGLEN_TPCCTL)
		return 0;

	header = !!(buffer[0] & HEADER_BIT);
	control = !!(buffer[0] & CONTROL_BIT);

	if (!header || !control)
		return -1;

	reply->data_id = buffer[0] & DATA_ID_MASK;
	reply->sensor_id = buffer[2] & 0x7;
	reply->panel_resolution = buffer[1];
	/* FIXME: big endian? */
	reply->x_max = (buffer[3] << 9) | (buffer[4] << 2) | ((buffer[2] >> 5) & 0x3);
	reply->y_max = (buffer[5] << 9) | (buffer[6] << 2) | ((buffer[2] >> 3) & 0x3);
	reply->capacity_resolution = buffer[7];
	reply->version = buffer[9] << 7 | buffer[10];

	return ISDV4_PKGLEN_TPCCTL;
}

/* pktlen defines what touch type we parse */
static inline int isdv4ParseTouchData(const unsigned char *buffer, const size_t buff_len,
				      const size_t pktlen, ISDV4TouchData *touchdata)
{
	int header, touch;

	if (!touchdata || buff_len < pktlen)
		return 0;

	header = !!(buffer[0] & HEADER_BIT);
	touch = !!(buffer[0] & TOUCH_CONTROL_BIT);

	if (header != 1 || touch != 1)
		return -1;

	memset(touchdata, 0, sizeof(*touchdata));

	touchdata->status = buffer[0] & 0x1;
	/* FIXME: big endian */
	touchdata->x = buffer[1] << 7 | buffer[2];
	touchdata->y = buffer[3] << 7 | buffer[4];
	if (pktlen == ISDV4_PKGLEN_TOUCH9A)
		touchdata->capacity = buffer[5] << 7 | buffer[6];

	if (pktlen == ISDV4_PKGLEN_TOUCH2FG)
	{
		touchdata->finger2.x = buffer[7] << 7 | buffer[8];
		touchdata->finger2.y = buffer[9] << 7 | buffer[10];
		touchdata->finger2.status = !!(buffer[0] & 0x2);
		/* FIXME: is there a fg2 capacity? */
	}

	return pktlen;
}

static inline int isdv4ParseCoordinateData(const unsigned char *buffer, const size_t len,
					   ISDV4CoordinateData *coord)
{
	int header, control;

	if (!coord || len < ISDV4_PKGLEN_TPCPEN)
		return 0;

	header = !!(buffer[0] & HEADER_BIT);
	control = !!(buffer[0] & TOUCH_CONTROL_BIT);

	if (header != 1 || control != 0)
		return -1;

	coord->proximity = (buffer[0] >> 5) & 0x1;
	coord->tip = buffer[0] & 0x1;
	coord->side = (buffer[0] >> 1) & 0x1;
	coord->eraser = (buffer[0] >> 2) & 0x1;
	/* FIXME: big endian */
	coord->x = (buffer[1] << 9) | (buffer[2] << 2) | ((buffer[6] >> 5) & 0x3);
	coord->y = (buffer[3] << 9) | (buffer[4] << 2) | ((buffer[6] >> 3) & 0x3);

	coord->pressure = ((buffer[6] & 0x7) << 7) | buffer[5];
	coord->tilt_x = buffer[7];
	coord->tilt_y = buffer[8];

	return ISDV4_PKGLEN_TPCPEN;
}

/* vim: set noexpandtab shiftwidth=8: */
