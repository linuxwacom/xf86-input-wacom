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
#include "wcmFilter.h"

static Bool isdv4Detect(LocalDevicePtr);
static Bool isdv4Init(LocalDevicePtr);
static void isdv4InitISDV4(WacomCommonPtr common, int fd, const char* id,
	float version);
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
		NULL,                 /* ranges not queried */
		NULL,                 /* reset not supported */
		NULL,                 /* tilt automatically enabled */
		NULL,                 /* suppress implemented in software */
		NULL,                 /* link speed unsupported */
		NULL,                 /* start not supported */
		isdv4Parse,
		xf86WcmFilterCoord,   /* input filtering */
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

	DBG(1, ErrorF("resetting tablet\n"));    

	/* Set the speed of the serial link to 38400 */
	if (xf86WcmSetSerialSpeed(local->fd, 38400) < 0)
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

	xf86WcmFlushTablet(local->fd);

	DBG(2, ErrorF("not reading model -- Wacom TabletPC ISD V4\n"));
	return xf86WcmInitTablet(common,&isdv4General,local->fd,"unknown",0.0);
}

/*****************************************************************************
 * isdv4InitISDV4 -- Setup the device
 ****************************************************************************/

static void isdv4InitISDV4(WacomCommonPtr common, int fd,
	const char* id, float version)
{
	DBG(2, ErrorF("initializing as ISDV4 model\n"));
  
	/* set parameters */
	common->wcmProtocolLevel = 0;
	common->wcmMaxZ = 255;          /* max Z value (pressure)*/
	common->wcmResolX = 2570;       /* X resolution in points/inch */
	common->wcmResolY = 2570;       /* Y resolution in points/inch */
	common->wcmPktLength = 9;       /* length of a packet */

	if (common->wcmRotate==ROTATE_NONE)
	{
		common->wcmMaxX = 21136;
		common->wcmMaxY = 15900;
	}
	else if (common->wcmRotate==ROTATE_CW || common->wcmRotate==ROTATE_CCW)
	{
		common->wcmMaxX = 15900;
		common->wcmMaxY = 21136;
	}
}

static int isdv4Parse(WacomCommonPtr common, const unsigned char* data)
{
	WacomDeviceState* last = &common->wcmChannel[0].valid.state;
	WacomDeviceState* ds;
	int n, is_stylus, cur_type, tmp_coord;

	if ((n = xf86WcmSerialValidate(common,data)) > 0)
		return n;

	/* pick up where we left off, minus relative values */
	ds = &common->wcmChannel[0].work;
	RESET_RELATIVE(*ds);

	ds->proximity = (data[0] & 0x20);

	/* x and y in "normal" orientetion (wide length is X) */
	ds->x = (((int)data[6] & 0x60) >> 5) | ((int)data[2] << 2) |
		((int)data[1] << 9);
	ds->y = (((int)data[6] & 0x18) >> 3) | ((int)data[4] << 2) |
		((int)data[3] << 9);

	/* rotation mixes x and y up a bit */
	if (common->wcmRotate == ROTATE_CW)
	{
		tmp_coord = ds->x;
		ds->x = ds->y;
		ds->y = common->wcmMaxY - tmp_coord;
	}
	else if (common->wcmRotate == ROTATE_CCW)
	{
		tmp_coord = ds->y;
		ds->y = ds->x;
		ds->x = common->wcmMaxX - tmp_coord;
	}

	/* pressure */
	ds->pressure = ((data[6] & 0x01) << 7) | (data[5] & 0x7F);

	/* report touch as button 1 */
	ds->buttons = (data[0] & 0x01) ? 1 : 0 ;

	/* report side switch as button 3 */
	ds->buttons |= (data[0] & 0x02) ? 0x04 : 0 ;

	/* check which device we have */
	is_stylus = (data[0] & 0x04) ? 0 : 1;
	cur_type = is_stylus ? STYLUS_ID : ERASER_ID;

	/* first time into prox */
	if (!last->proximity && ds->proximity) 
		ds->device_type = cur_type;

	/* out of prox */
	else if (!ds->proximity)
		memset(ds,0,sizeof(*ds));

	DBG(8, ErrorF("isdv4Parse %s\n",
		ds->device_type == ERASER_ID ? "ERASER " :
		ds->device_type == STYLUS_ID ? "STYLUS" : "NONE"));

	xf86WcmEvent(common,0,ds);

	return common->wcmPktLength;
}

