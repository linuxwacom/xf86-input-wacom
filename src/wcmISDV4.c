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

static Bool xf86WcmISDV4Detect(LocalDevicePtr);
static Bool xf86WcmISDV4Init(LocalDevicePtr);
static void xf86WcmISDV4Read(LocalDevicePtr);

	WacomDeviceClass gWacomISDV4Device =
	{
		xf86WcmISDV4Detect,
		xf86WcmISDV4Init,
		xf86WcmISDV4Read,
	};

/*****************************************************************************
 * xf86WcmISDV4Detect -- Test if the attached device is ISDV4.
 ****************************************************************************/

static Bool xf86WcmISDV4Detect(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	return (common->wcmForceDevice == DEVICE_ISDV4) ? 1 : 0;
}

/*****************************************************************************
 * xf86WcmISDV4Init --
 ****************************************************************************/

static Bool xf86WcmISDV4Init(LocalDevicePtr local)
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

	/* Default threshold value if not set */
	if (common->wcmThreshold <= 0)
	{
		/* Threshold for counting pressure as a button */
		common->wcmThreshold = common->wcmMaxZ / 32;
		ErrorF("%s Wacom using pressure threshold of %d for button 1\n",
			XCONFIG_PROBED, common->wcmThreshold);
	}


	DBG(2, ErrorF("setup is max X=%d max Y=%d resol X=%d resol Y=%d\n",
		  common->wcmMaxX, common->wcmMaxY, common->wcmResolX,
		  common->wcmResolY));

	/* suppress is not currently implemented for this device --
	if (common->wcmSuppress < 0)
	{
		int xratio = common->wcmMaxX/screenInfo.screens[0]->width;
		int yratio = common->wcmMaxY/screenInfo.screens[0]->height;
		common->wcmSuppress = (xratio > yratio) ? yratio : xratio;
	}
    
	if (common->wcmSuppress > 100)
		common->wcmSuppress = 99;
	*/

	if (xf86Verbose)
		ErrorF("%s Wacom %s tablet maximum X=%d maximum Y=%d "
			"X resolution=%d Y resolution=%d suppress=%d%s\n",
			XCONFIG_PROBED, "ISDV4",
			common->wcmMaxX, common->wcmMaxY,
			common->wcmResolX, common->wcmResolY,
			common->wcmSuppress,
			HANDLE_TILT(common) ? " Tilt" : "");
  
	if (err == -1)
	{
		SYSCALL(xf86WcmClose(local->fd));
		local->fd = -1;
		return !Success;
	}

	return Success;
}

/*****************************************************************************
 * xf86WcmISDV4Read -- Read the new events from the device, and enqueue them.
 ****************************************************************************/

static void xf86WcmISDV4Read(LocalDevicePtr local)
{
	WacomDevicePtr priv = (WacomDevicePtr) local->private;
	WacomCommonPtr common = priv->common;
	int len, loop, idx;
	int is_stylus = 1, is_button, is_proximity;
	int x, y, z, tmp_coord, buttons, tx = 0, ty = 0;
	unsigned char buffer[BUFFER_SIZE];
  
	DBG(7, ErrorF("xf86WcmISDV4Read BEGIN device=%s fd=%d\n",
			common->wcmDevice, local->fd));

	SYSCALL(len = xf86WcmRead(local->fd, buffer, sizeof(buffer)));

	if (len <= 0)
	{
		ErrorF("Error reading wacom device : %s\n", strerror(errno));
		return;
	}
	DBG(10, ErrorF("xf86WcmISDV4Read read %d bytes\n", len));

	/* check high bits */
	for(loop=0; loop<len; loop++)
	{
		/* check high bit in leading byte */
		if ((common->wcmIndex == 0) && !(buffer[loop] & HEADER_BIT))
		{
			DBG(6, ErrorF("xf86WcmISDV4Read bad magic "
					"number 0x%x (pktlength=%d) %d\n",
					buffer[loop], common->wcmPktLength,
					loop));
			continue;

		}

		/* check high bit in trailing bytes */
		else if ((common->wcmIndex != 0) && (buffer[loop] & HEADER_BIT))
		{
			DBG(6, ErrorF("xf86WcmISDV4Read magic "
					"number 0x%x detetected at index %d "
					"loop=%d\n", (unsigned int)buffer[loop],
					common->wcmIndex, loop));
			common->wcmIndex = 0;
		}
	
	
		common->wcmData[common->wcmIndex++] = buffer[loop];

		if (common->wcmIndex == common->wcmPktLength)
		{
			/* we have a full packet */

			is_proximity = (common->wcmData[0] & 0x20);

			/* reset char count for next read */
			common->wcmIndex = 0;

			/* x and y in "normal" orientetion (wide length is X) */
			x = (((int)common->wcmData[6] & 0x60) >> 5) |
				((int)common->wcmData[2] << 2) |
				((int)common->wcmData[1] << 9);
			y = (((int)common->wcmData[6] & 0x18) >> 3) |
				((int)common->wcmData[4] << 2) |
				((int)common->wcmData[3] << 9);

			/* rotation mixes x and y up a bit */
			if (common->wcmRotate == ROTATE_CW)
			{
				tmp_coord = x;
				x = y;
				y = common->wcmMaxY - tmp_coord;
			}
			else if (common->wcmRotate == ROTATE_CCW)
			{
				tmp_coord = y;
				y = x;
				x = common->wcmMaxX - tmp_coord;
			}


			/* check which device we have */
			is_stylus = (common->wcmData[0] & 0x04) ? 0 : 1;

			/* pressure */
			z = ((common->wcmData[6] & 0x01) << 7) |
				(common->wcmData[5] & 0x7F);

			/* report touch as button 1 */
			buttons = (common->wcmData[0] & 0x01) ? 1 : 0 ;
			/* report side switch as button 3 */
			buttons |= (common->wcmData[0] & 0x02) ? 0x04 : 0 ;

			is_button = (buttons != 0);
	    
			/* save if we have stylus or eraser for
			 * future proximity out */
			common->wcmStylusSide = is_stylus ? 1 : 0;

			DBG(8, ErrorF("xf86WcmISDV4Read %s side\n",
					common->wcmStylusSide ?
					"stylus" : "eraser"));
			common->wcmStylusProximity = is_proximity;

			/* figure out which device(s) to impersonate and send */
			for(idx=0; idx<common->wcmNumDevices; idx++)
			{
				LocalDevicePtr local_dev =
					common->wcmDevices[idx];
				WacomDevicePtr priv = (WacomDevicePtr)
					local_dev->private;
				int curDevice;
			
				DBG(7, ErrorF("xf86WcmISDV4Read trying to "
					"send to %s\n",local_dev->name));

				if (is_proximity)
				{
					/* stylus and eraser are
					 * distingushable in proximity report
					 * as eraser if we have one,
					 * otherwise as stylus */

					if (!is_stylus && common->wcmHasEraser)
						curDevice = ERASER_ID;
					else
						curDevice = STYLUS_ID;
				}
				else
				{
					/* eraser bit not set when out
					 * of proximity, check */
					curDevice = (common->wcmHasEraser &&
						(!common->wcmStylusSide)) ?
						ERASER_ID : STYLUS_ID;
				}

				if (DEVICE_ID(priv->flags) != curDevice)
					continue;
				
				DBG(10, ErrorF((DEVICE_ID(priv->flags) ==
					ERASER_ID) ?  "Eraser\n" : "Stylus\n"));
				
		/* HACKED */
		{
			/* hack, we'll get this whole function working later */
			WacomDeviceState ds = { 0 };
			ds.device_type = curDevice;
			ds.buttons = buttons;
			ds.proximity = is_proximity;
			ds.x = x;
			ds.y = y;
			ds.pressure = z;
			ds.tiltx = tx;
			ds.tilty = ty;

			xf86WcmSendEvents(common->wcmDevices[idx],&ds);
		}

			}
		} /* full packet */
	} /* next data */
	DBG(7, ErrorF("xf86WcmISDV4Read END   local=0x%x priv=0x%x index=%d\n",
			local, priv, common->wcmIndex));
}

