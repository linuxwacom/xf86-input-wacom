/*
 * Copyright 1995-2002 by Frederic Lepied, France. <Lepied@XFree86.org>
 * Copyright 2002-2011 by Ping Cheng, Wacom. <pingc@wacom.com>
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
#include "isdv4.h"
#include <unistd.h>
#include <fcntl.h>
#include <libudev.h>

#define RESET_RELATIVE(ds) do { (ds).relwheel = 0; } while (0)

/* resolution in points/m */
#define ISDV4_PEN_RESOLUTION    100000
#define ISDV4_TOUCH_RESOLUTION  10000

/* ISDV4 init process
   This process is the same for other backends (i.e. USB).

   1. isdv4Detect - called to test if device may be serial
   2. isdv4ProbeKeys - called to fake up keybits
   3. isdv4ParseOptions - parse ISDV4-specific options
   4. isdv4Init - init ISDV4-specific stuff and set the tablet model.

   After isdv4Init has been called, common->model points to the ISDV4 model,
   further calls are model-specific (not that it matters for ISDV4, we only
   have one model).

   5. isdv4InitISDV4 - do whatever device-specific init is necessary
   6. isdv4GetRanges - Query axis ranges

   --- end of PreInit ---

   isdv4StartTablet is called in DEVICE_ON
   isdv4Parse is called during ReadInput.

 */

typedef struct {
	/* Counter for dependent devices. We can only send one QUERY command to
	   the tablet and we must not send the SAMPLING command until the last
	   device is enabled.  */
	int initialized_devices;
	/* QUERY can only be run once */
	int tablet_initialized;
	int baudrate;
} wcmISDV4Data;

static Bool isdv4Detect(InputInfoPtr);
static Bool isdv4ParseOptions(InputInfoPtr pInfo);
static Bool isdv4Init(InputInfoPtr, char* id, float *version);
static int isdv4ProbeKeys(InputInfoPtr pInfo);
static void isdv4InitISDV4(WacomCommonPtr, const char* id, float version);
static int isdv4GetRanges(InputInfoPtr);
static int isdv4StartTablet(InputInfoPtr);
static int isdv4StopTablet(InputInfoPtr);
static int isdv4Parse(InputInfoPtr, const unsigned char* data, int len);
static int wcmSerialValidate(InputInfoPtr pInfo, const unsigned char* data);
static int wcmWaitForTablet(InputInfoPtr pInfo, char * data, int size);
static int wcmWriteWait(InputInfoPtr pInfo, const char* request);

	WacomDeviceClass gWacomISDV4Device =
	{
		isdv4Detect,
		isdv4ParseOptions,
		isdv4Init,
		isdv4ProbeKeys,
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

static void memdump(InputInfoPtr pInfo, char *buffer, unsigned int len)
{
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	int i;

	DBG(10, common, "memdump of ISDV4 data (len %d)\n", len);
	/* can't use DBG macro here, need to do it manually. */
	for (i = 0 ; i < len && common->debugLevel >= 10; i++)
	{
		LogMessageVerbSigSafe(X_NONE, 0, "%#hhx ", buffer[i]);
		if (i % 8 == 7)
			LogMessageVerbSigSafe(X_NONE, 0, "\n");
	}

	LogMessageVerbSigSafe(X_NONE, 0, "\n");
#endif
}


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

static int wcmSerialValidate(InputInfoPtr pInfo, const unsigned char* data)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;

	int n;

	/* First byte must have header bit set, if not, skip until next
	 * header byte */
	if (!(data[0] & HEADER_BIT))
	{
		n = wcmSkipInvalidBytes(data, common->wcmPktLength);
		LogMessageVerbSigSafe(X_WARNING, 0,
			"%s: missing header bit. skipping %d bytes.\n",
			pInfo->name, n);
		return n;
	}

	/* Remainder must _not_ have header bit set, if not, skip to first
	 * header byte. wcmSkipInvalidBytes gives us the number of bytes
	 * without the header bit set, so use the next one.
	 */
	n = wcmSkipInvalidBytes(&data[1], common->wcmPktLength - 1);
	n += 1; /* the header byte we already checked */
	if (n != common->wcmPktLength) {
		LogMessageVerbSigSafe(X_WARNING, 0, "%s: bad data at %d v=%x l=%d\n", pInfo->name,
			n, data[n], common->wcmPktLength);
		return n;
	}

	return 0;
}

/*****************************************************************************
 * isdv4Detect -- Test if the attached device is ISDV4.
 ****************************************************************************/

static Bool isdv4Detect(InputInfoPtr pInfo)
{
	struct serial_struct ser;
	int rc;

	rc = ioctl(pInfo->fd, TIOCGSERIAL, &ser);
	if (rc == -1)
		return FALSE;

	return TRUE;
}

/*****************************************************************************
 * isdv4ParseOptions -- parse ISDV4-specific options
 ****************************************************************************/
static Bool isdv4ParseOptions(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	wcmISDV4Data *isdv4data;
	int baud;

	/* Determine default baud rate */
	baud = (common->tablet_id == 0x90)? 19200 : 38400;

	baud = xf86SetIntOption(pInfo->options, "BaudRate", baud);

	switch (baud)
	{
		case 38400:
		case 19200:
			/* xf86OpenSerial() takes the baud rate from the options */
			xf86ReplaceIntOption(pInfo->options, "BaudRate", baud);
			break;
		default:
			xf86Msg(X_ERROR, "%s: Illegal speed value "
					"(must be 19200 or 38400).",
					pInfo->name);
			return FALSE;
	}

	if (!common->private)
	{
		if (!(common->private = calloc(1, sizeof(wcmISDV4Data))))
		{
			xf86Msg(X_ERROR, "%s: failed to alloc backend-specific data.\n",
				pInfo->name);
			return FALSE;
		}
		isdv4data = common->private;
		isdv4data->baudrate = baud;
		isdv4data->tablet_initialized = 0;
		isdv4data->initialized_devices = 0;
	}

	return TRUE;
}

/*****************************************************************************
 * isdv4Init --
 ****************************************************************************/

static Bool isdv4Init(InputInfoPtr pInfo, char* id, float *version)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	wcmISDV4Data *isdv4data = common->private;

	DBG(1, priv, "initializing ISDV4 tablet\n");

	/* Set baudrate */
	if (xf86SetSerialSpeed(pInfo->fd, isdv4data->baudrate) < 0)
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

static int isdv4Query(InputInfoPtr pInfo, const char* query, char* data)
{
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
#endif

	DBG(1, priv, "Querying ISDV4 tablet\n");

	if (isdv4StopTablet(pInfo) != Success)
		return !Success;

	/* Send query command to the tablet */
	if (!wcmWriteWait(pInfo, query))
		return !Success;

	/* Read the control data */
	if (!wcmWaitForTablet(pInfo, data, ISDV4_PKGLEN_TPCCTL))
		return !Success;

	/* Control data bit check */
	if ( !(data[0] & 0x40) )
	{
		/* Reread the control data since it may fail the first time */
		wcmWaitForTablet(pInfo, data, ISDV4_PKGLEN_TPCCTL);
		if ( !(data[0] & 0x40) )
			return !Success;
	}

	return Success;
}

/*****************************************************************************
 * isdv4InitISDV4 -- Setup the device
 ****************************************************************************/

static void isdv4InitISDV4(WacomCommonPtr common, const char* id, float version)
{
	/* length of a packet */
	common->wcmPktLength = ISDV4_PKGLEN_TPCPEN;

	/* digitizer X resolution in points/m */
	common->wcmResolX = ISDV4_PEN_RESOLUTION;
	/* digitizer Y resolution in points/m */
	common->wcmResolY = ISDV4_PEN_RESOLUTION;

	/* tilt disabled */
	common->wcmFlags &= ~TILT_ENABLED_FLAG;
}

/*****************************************************************************
 * isdv4GetRanges -- get ranges of the device
 ****************************************************************************/

static int isdv4GetRanges(InputInfoPtr pInfo)
{
	char data[BUFFER_SIZE];
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common =	priv->common;
	wcmISDV4Data *isdv4data = common->private;
	int ret = Success;

	DBG(2, priv, "getting ISDV4 Ranges\n");

	if (isdv4data->tablet_initialized)
		goto out;

	/* Set baudrate to configured value */
	if (xf86SetSerialSpeed(pInfo->fd, isdv4data->baudrate) < 0)
	{
		ret = !Success;
		goto out;
	}

	/* Send query command to the tablet */
	ret = isdv4Query(pInfo, ISDV4_QUERY, data);
	if (ret != Success)
	{
		int baud;

		/* Try with the other baudrate */
		baud = (isdv4data->baudrate == 38400)? 19200 : 38400;

		xf86Msg(X_WARNING, "%s: Query failed with %d baud. Trying %d.\n",
				   pInfo->name, isdv4data->baudrate, baud);

		if (xf86SetSerialSpeed(pInfo->fd, baud) < 0)
		{
			ret = !Success;
			goto out;
		}

		ret = isdv4Query(pInfo, ISDV4_QUERY, data);

		if (ret == Success) {
			isdv4data->baudrate = baud;
			/* xf86OpenSerial() takes the baud rate from the options */
			xf86ReplaceIntOption(pInfo->options, "BaudRate", baud);
		}

	}

	if (ret == Success)
	{
		ISDV4QueryReply reply;
		int rc;

		rc = isdv4ParseQuery((unsigned char*)data, sizeof(data), &reply);
		if (rc <= 0)
		{
			xf86Msg(X_ERROR, "%s: Error while parsing ISDV4 query.\n",
					pInfo->name);
			if (rc == 0)
				DBG(2, common, "reply or len invalid.\n");
			else
				DBG(2, common, "header data corrupt.\n");
			memdump(pInfo, data, sizeof(reply));
			ret = BadAlloc;
			goto out;
		}

		/* transducer data */
		common->wcmMaxZ = reply.pressure_max;
		common->wcmMaxX = reply.x_max;
		common->wcmMaxY = reply.y_max;
		if (reply.tilt_x_max && reply.tilt_y_max)
		{
			common->wcmTiltOffX = 0 - reply.tilt_x_max / 2;
			common->wcmTiltFactX = 1.0;
			common->wcmTiltMinX = 0 + common->wcmTiltOffX;
			common->wcmTiltMaxX = reply.tilt_x_max +
					      common->wcmTiltOffX;

			common->wcmTiltOffY = 0 - reply.tilt_y_max / 2;
			common->wcmTiltFactY = 1.0;
			common->wcmTiltMinY = 0 + common->wcmTiltOffY;
			common->wcmTiltMaxY = reply.tilt_y_max +
					      common->wcmTiltOffY;

			common->wcmFlags |= TILT_ENABLED_FLAG;
		}

		common->wcmVersion = reply.version;

		/* default to no pen 2FGT if size is undefined */
		if (!common->wcmMaxX || !common->wcmMaxY)
			common->tablet_id = 0xE2;

		DBG(2, priv, "Pen speed=%d "
			"maxX=%d maxY=%d maxZ=%d resX=%d resY=%d \n",
			isdv4data->baudrate, common->wcmMaxX, common->wcmMaxY,
			common->wcmMaxZ, common->wcmResolX, common->wcmResolY);
	}

	/* Touch might be supported. Send a touch query command */
	if (isdv4data->baudrate == 38400 &&
	    isdv4Query(pInfo, ISDV4_TOUCH_QUERY, data) == Success)
	{
		ISDV4TouchQueryReply reply;
		int rc;

		rc = isdv4ParseTouchQuery((unsigned char*)data, sizeof(data), &reply);
		if (rc <= 0)
		{
			xf86Msg(X_ERROR, "%s: Error while parsing ISDV4 touch query.\n",
					pInfo->name);
			if (rc == 0)
				DBG(2, common, "reply or len invalid.\n");
			else
				DBG(2, common, "header data corrupt.\n");
			memdump(pInfo, data, sizeof(reply));
			ret = BadAlloc;
			goto out;
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
				    xf86Msg(X_WARNING, "%s: tablet id(%x)"
					    " mismatch with data id (0x01) \n",
					    pInfo->name, common->tablet_id);
				    goto out;
				}
				break;
				/* 2FGT */
			case 0x03:
				if ((common->tablet_id != 0xE2) &&
						(common->tablet_id != 0xE3))
				{
				    xf86Msg(X_WARNING, "%s: tablet id(%x)"
					    " mismatch with data id (0x03) \n",
					    pInfo->name, common->tablet_id);
				    goto out;
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
			common->wcmTouchResolX = common->wcmTouchResolY = ISDV4_TOUCH_RESOLUTION;

		common->wcmVersion = reply.version;
		ret = Success;

		DBG(2, priv, "touch speed=%d "
			"maxTouchX=%d maxTouchY=%d TouchresX=%d TouchresY=%d \n",
			isdv4data->baudrate, common->wcmMaxTouchX,
			common->wcmMaxTouchY, common->wcmTouchResolX,
			common->wcmTouchResolY);
	}

	xf86Msg(X_INFO, "%s: serial tablet id 0x%X.\n", pInfo->name, common->tablet_id);

out:
	if (ret == Success)
	{
		isdv4data->tablet_initialized = 1;
		isdv4data->initialized_devices++;
	}

	return ret;
}

static int isdv4StartTablet(InputInfoPtr pInfo)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common =	priv->common;
	wcmISDV4Data *isdv4data = common->private;

	if (--isdv4data->initialized_devices)
		return Success;

	/* Tell the tablet to start sending coordinates */
	if (!wcmWriteWait(pInfo, ISDV4_SAMPLING))
		return !Success;

	return Success;
}

static int isdv4StopTablet(InputInfoPtr pInfo)
{
#if DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
#endif
	int fd_flags;

	/* Send stop command to the tablet */
	if (!wcmWriteWait(pInfo, ISDV4_STOP))
		return !Success;

	/* Wait 250 mSecs */
	if (wcmWait(250))
		return !Success;

	/* discard potential data on the line */
	fd_flags = fcntl(pInfo->fd, F_GETFL);
	if (fcntl(pInfo->fd, F_SETFL, fd_flags | O_NONBLOCK) == 0)
	{
		char buffer[10];
		while (read(pInfo->fd, buffer, sizeof(buffer)) > 0)
			DBG(10, common, "discarding garbage data.\n");
		fcntl(pInfo->fd, F_SETFL, fd_flags);
	}

	return Success;
}
/**
 * Parse one touch packet.
 *
 * @param pInfo The device to parse the packet for
 * @param data Data read from the device
 * @param len Data length in bytes
 * @param[out] ds The device state, modified in place.
 *
 * @return The channel number of -1 on error.
 */

static int isdv4ParseTouchPacket(InputInfoPtr pInfo, const unsigned char *data,
				 int len, WacomDeviceState *ds)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceState* last = &common->wcmChannel[0].valid.state;
	WacomDeviceState* lastTemp = &common->wcmChannel[1].valid.state;
	ISDV4TouchData touchdata;
	int rc;
	int channel = 0;

	rc = isdv4ParseTouchData(data, len, common->wcmPktLength, &touchdata);
	if (rc == -1)
	{
		LogMessageVerbSigSafe(X_ERROR, 0, "%s: failed to parse touch data.\n",
				      pInfo->name);
		return -1;
	}

	ds->x = touchdata.x;
	ds->y = touchdata.y;
	ds->proximity = touchdata.status;
	ds->device_type = TOUCH_ID;
	ds->device_id = TOUCH_DEVICE_ID;
	ds->serial_num = 1;
	ds->time = (int)GetTimeInMillis();

	if (common->wcmPktLength == ISDV4_PKGLEN_TOUCH2FG)
	{
		if (touchdata.finger2.status ||
		    (!touchdata.finger2.status && lastTemp->proximity))
		{
			/* Got 2FGT. Send the first one if received */
			if (ds->proximity || (!ds->proximity && last->proximity))
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
			ds->serial_num = 2;
			ds->proximity = touchdata.finger2.status;
			ds->time = (int)GetTimeInMillis();
			/* time stamp for 2FGT gesture events */
			if ((ds->proximity && !lastTemp->proximity) ||
			    (!ds->proximity && lastTemp->proximity))
				ds->sample = (int)GetTimeInMillis();
		}
	}

	DBG(8, priv, "MultiTouch %s proximity \n", ds->proximity ? "in" : "out of");

	return channel;
}

/**
 * Parse one pen packet.
 *
 * @param pInfo The device to parse the packet for
 * @param data Data read from the device
 * @param len Data length in bytes
 * @param[out] ds The device state, modified in place.
 *
 * @return The channel number.
 */
static int isdv4ParsePenPacket(InputInfoPtr pInfo, const unsigned char *data,
			       int len, WacomDeviceState *ds)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceState* last = &common->wcmChannel[0].valid.state;
	int rc;
	ISDV4CoordinateData coord;
	int channel = 0;
	int cur_type;

	rc = isdv4ParseCoordinateData(data, ISDV4_PKGLEN_TPCPEN, &coord);

	if (rc == -1)
	{
		LogMessageVerbSigSafe(X_ERROR, 0,
				      "%s: failed to parse coordinate data.\n", pInfo->name);
		return -1;
	}

	ds->time = (int)GetTimeInMillis();
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
			WacomDeviceState out = OUTPROX_STATE;
			wcmEvent(common, 0, &out);
			ds->device_type = cur_type;
		}
	}

	ds->device_id = (ds->device_type == ERASER_ID) ?  ERASER_DEVICE_ID : STYLUS_DEVICE_ID;

	/* don't send button 3 event for eraser
	 * button 1 event will be sent by testing presure level
	 */
	if (ds->device_type == ERASER_ID && ds->buttons & 4)
	{
		ds->buttons = 0;
		ds->device_id = ERASER_DEVICE_ID;
	}

	DBG(8, priv, "%s\n",
			ds->device_type == ERASER_ID ? "ERASER " :
			ds->device_type == STYLUS_ID ? "STYLUS" : "NONE");

	return channel;
}


static int isdv4Parse(InputInfoPtr pInfo, const unsigned char* data, int len)
{
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr common = priv->common;
	WacomDeviceState* last = &common->wcmChannel[0].valid.state;
	WacomDeviceState* ds;
	int n, channel = 0;

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
			WacomDeviceState out = OUTPROX_STATE;
			out.device_type = TOUCH_ID;
			out.serial_num = 1;
			wcmEvent(common, channel, &out);
		}
	}

	/* Coordinate data bit check */
	if (data[0] & CONTROL_BIT) /* control data */
		return common->wcmPktLength;
	else if ((n = wcmSerialValidate(pInfo,data)) > 0)
		return n;

	/* pick up where we left off, minus relative values */
	ds = &common->wcmChannel[channel].work;
	RESET_RELATIVE(*ds);

	if (common->wcmPktLength == ISDV4_PKGLEN_TPCPEN)
		channel = isdv4ParsePenPacket(pInfo, data, len, ds);
	else { /* a touch */
		channel = isdv4ParseTouchPacket(pInfo, data, len, ds);
		ds = &common->wcmChannel[channel].work;
	}

	if (channel < 0)
		return 0;

	wcmEvent(common, channel, ds);
	return common->wcmPktLength;
}

/*****************************************************************************
 * wcmWriteWait --
 *   send a request
 ****************************************************************************/

static int wcmWriteWait(InputInfoPtr pInfo, const char* request)
{
	int len, maxtry = MAXTRY;

	/* send request string */
	do
	{
		len = xf86WriteSerial(pInfo->fd, request, strlen(request));
		if ((len == -1) && (errno != EAGAIN))
		{
			xf86Msg(X_ERROR, "%s: wcmWriteWait error : %s\n",
					pInfo->name, strerror(errno));
			return 0;
		}

		maxtry--;

	} while ((len <= 0) && maxtry);

	if (!maxtry)
		xf86Msg(X_WARNING, "%s: Failed to issue command '%s' "
				   "after %d tries.\n", pInfo->name, request, MAXTRY);

	return maxtry;
}

/*****************************************************************************
 * wcmWaitForTablet --
 *   wait for tablet data
 ****************************************************************************/

static int wcmWaitForTablet(InputInfoPtr pInfo, char* answer, int size)
{
	int len, maxtry = MAXTRY;

	/* Read size bytes of the answer */
	do
	{
		if ((len = xf86WaitForInput(pInfo->fd, 1000000)) > 0)
		{
			len = xf86ReadSerial(pInfo->fd, answer, size);
			if ((len == -1) && (errno != EAGAIN))
			{
				xf86Msg(X_ERROR, "%s: xf86ReadSerial error : %s\n",
						pInfo->name, strerror(errno));
				return 0;
			}
		}
		maxtry--;
	} while ((len <= 0) && maxtry);

	if (!maxtry)
		xf86Msg(X_WARNING, "%s: Waited too long for answer "
				   "(failed after %d tries).\n",
				   pInfo->name, MAXTRY);

	return maxtry;
}

static int set_keybits_wacom(int id, unsigned long *keys)
{
	int tablet_id = 0;

	/* id < 0x008 are only penabled */
	if (id > 0x007)
		SETBIT(keys, BTN_TOOL_FINGER);
	if (id > 0x0a)
		SETBIT(keys, BTN_TOOL_DOUBLETAP);

	/* no pen 2FGT */
	if (id == 0x010)
	{
		CLEARBIT(keys, BTN_TOOL_PEN);
		CLEARBIT(keys, BTN_TOOL_RUBBER);
	}

	/* 0x9a and 0x9f are only detected by communicating
	 * with device.  This means tablet_id will be updated/refined
	 * at later stage and true knowledge of capacitive
	 * support will be delayed until that point.
	 */
	switch(id)
	{
		case 0x0 ... 0x7: tablet_id = 0x90; break;
		case 0x8 ... 0xa: tablet_id = 0x93; break;
		case 0xb ... 0xe: tablet_id = 0xe3; break;
		case 0x10:	  tablet_id = 0xe2; break;
	}

	return tablet_id;
}

static int set_keybits_fujitsu(int id, unsigned long *keys)
{
	int tablet_id = 0x90; /* default to penabled */

	if (id == 0x2e7) {
		SETBIT(keys, BTN_TOOL_DOUBLETAP);
		tablet_id = 0xe3;
	}

	if (id == 0x2e9) {
		SETBIT(keys, BTN_TOOL_FINGER);
		tablet_id = 0x93;
	}

	return tablet_id;
}

/**
 * Match the device id to a vendor, return the vendor ID, key bits and
 * tablet ID.
 *
 * @param name device id string
 * @param common set key bits, vendor_id and tablet_id
 */
static Bool get_keys_vendor_tablet_id(char *name, WacomCommonPtr common)
{
	int id;

	if (sscanf(name, "WACf%x", &id) == 1) {
		common->vendor_id = WACOM_VENDOR_ID;
		common->tablet_id = set_keybits_wacom(id, common->wcmKeys);
	} else if (sscanf(name, "FUJ%x", &id) == 1) {
		common->vendor_id = 0;
		common->tablet_id = set_keybits_fujitsu(id, common->wcmKeys);
	} else
		return FALSE;

	return TRUE;
}

/**
 * Return the content of id file from sysfs:  /sys/.../device/id
 *
 * @param pInfo for fd
 * @param buf[out] preallocated buffer to return the result in.
 * @param buf_size: size of preallocated buffer
 */
static Bool get_sysfs_id(InputInfoPtr pInfo, char *buf, int buf_size)
{
	WacomDevicePtr  priv = (WacomDevicePtr)pInfo->private;
	struct udev *udev = NULL;
	struct udev_device *device = NULL;
	struct stat st;
	char *sysfs_path = NULL;
	FILE *file = NULL;
	Bool ret = FALSE;

	fstat(pInfo->fd, &st);

	udev = udev_new();
	device = udev_device_new_from_devnum(udev, 'c', st.st_rdev);

	if (!device)
		goto out;
	if (asprintf(&sysfs_path, "%s/device/id",
		     udev_device_get_syspath(device)) == -1)
		goto out;

	DBG(8, priv, "sysfs path: %s\n", sysfs_path);

	file = fopen(sysfs_path, "r");
	if (!file)
		goto out;
	if (!fread(buf, 1, buf_size, file))
		goto out;
	ret = TRUE;
out:
	udev_device_unref(device);
	udev_unref(udev);
	if (file)
		fclose(file);
	free(sysfs_path);

	return ret;
}

/**
 * Query the device's fd for the key bits and the tablet ID. Returns the ID
 * on success. If the model vendor is unknown, we assume a penabled device
 * (0x90). If the model vendor is known but the model itself is unknown, the
 * return value depends on the model-specific matching code (0 for Wacom,
 * 0x90 for Fujitsu).
 *
 * For serial devices, we set the BTN_TOOL_DOUBLETAP etc. bits based on the
 * device ID. This matching only works for known devices (see the
 * isdv4_model list), all others are simply assumed to be pen + erasor.
 */
static int isdv4ProbeKeys(InputInfoPtr pInfo)
{
	struct serial_struct tmp;
	WacomDevicePtr  priv = (WacomDevicePtr)pInfo->private;
	WacomCommonPtr  common = priv->common;

	if (ioctl(pInfo->fd, TIOCGSERIAL, &tmp) < 0)
		return 0;

	common->tablet_id = 0x90;

	/* default to penabled */
	memset(common->wcmKeys, 0, sizeof(common->wcmKeys));

	SETBIT(common->wcmKeys, BTN_TOOL_PEN);
	SETBIT(common->wcmKeys, BTN_TOOL_RUBBER);

	/* Change to generic protocol to match USB MT format */
	common->wcmProtocolLevel = WCM_PROTOCOL_GENERIC;

	if (!get_keys_vendor_tablet_id(pInfo->name, common)) {
		char buf[15] = {0};
		if (get_sysfs_id(pInfo, buf, sizeof(buf)))
			get_keys_vendor_tablet_id(buf, common);
	}

	return common->tablet_id;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
