/*
 * Copyright 2009 by Ping Cheng, Wacom. <pingc@wacom.com>
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
#include "wcmFilter.h"
#ifdef WCM_XORG_XSERVER_1_4
    #ifndef _XF86_ANSIC_H
	#include <fcntl.h>
	#include <sys/stat.h>
    #endif

/*****************************************************************************
 * xf86WcmCheckTypeAndSource - Check if both devices have the same type OR
 * the device has been used in xorg.conf: don't add the tool by hal/udev
 * if user has defined at least one tool for the device in xorg.conf
 ****************************************************************************/
static Bool xf86WcmCheckTypeAndSource(LocalDevicePtr fakeLocal, LocalDevicePtr pLocal)
{
	int match = 1;
	char* fsource = xf86CheckStrOption(fakeLocal->options, "_source", "");
	char* psource = xf86CheckStrOption(pLocal->options, "_source", "");
	char* type = xf86FindOptionValue(fakeLocal->options, "Type");
	WacomDevicePtr priv = (WacomDevicePtr) pLocal->private;

	/* only add the new tool if the matching major/minor
	 * was from the same source */
	if (!strcmp(fsource, psource))
	{
		/* and the tools have different types */
		if (strcmp(type, xf86FindOptionValue(pLocal->options, "Type")))
			match = 0;
	}
	DBG(2, priv->debugLevel, xf86Msg(X_INFO, "xf86WcmCheckTypeAndSource "
		"device %s from %s %s \n", fakeLocal->name, fsource,
		match ? "will be added" : "will be ignored"));
	return match;
}

/* check if the device has been added */
int wcmIsDuplicate(char* device, LocalDevicePtr local)
{
#ifdef _XF86_ANSIC_H
	struct xf86stat st;
#else
	struct stat st;
#endif
	int isInUse = 0;
	LocalDevicePtr localDevices = NULL;
	WacomCommonPtr common = NULL;

	/* open the port */
	do {
        	SYSCALL(local->fd = open(device, O_RDONLY, 0));
	} while (local->fd < 0 && errno == EINTR);

	if (local->fd < 0)
	{
		/* can not open the device */
        	xf86Msg(X_ERROR, "Unable to open Wacom device \"%s\".\n", device);
		isInUse = 2;
		goto ret;
	}

#ifdef _XF86_ANSIC_H
	if (xf86fstat(local->fd, &st) == -1)
#else
	if (fstat(local->fd, &st) == -1)
#endif
	{
		/* can not access major/minor to check device duplication */
		xf86Msg(X_ERROR, "%s: stat failed (%s). cannot check for duplicates.\n",
                		local->name, strerror(errno));

		/* older systems don't support the required ioctl.  let it pass */
		goto ret;
	}

	if ((int)st.st_rdev)
	{
		localDevices = xf86FirstLocalDevice();

		for (; localDevices != NULL; localDevices = localDevices->next)
		{
			device = xf86CheckStrOption(localDevices->options, "Device", NULL);

			/* device can be NULL on some distros */
			if (!device || !strstr(localDevices->drv->driverName, "wacom"))
				continue;

			common = ((WacomDevicePtr)localDevices->private)->common;
			if (local != localDevices &&
				common->min_maj &&
				common->min_maj == st.st_rdev)
			{
				/* device matches with another added port */
				if (xf86WcmCheckTypeAndSource(local, localDevices))
				{
					xf86Msg(X_WARNING, "%s: device file already in use by %s. "
						"Ignoring.\n", local->name, localDevices->name);
					isInUse = 4;
					goto ret;
				}
			}
 		}
	}
	else
	{
		/* major/minor can never be 0, right? */
		xf86Msg(X_ERROR, "%s: device opened with a major/minor of 0. "
			"Something was wrong.\n", local->name);
		isInUse = 5;
	}
ret:
	if (local->fd >= 0)
	{ 
		close(local->fd);
		local->fd = -1;
	}
	return isInUse;
}

static struct
{
	__u16 productID;
	__u16 flags;
} validType [] =
{
	{ 0x00, STYLUS_ID }, /* PenPartner */
	{ 0x10, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Graphire */
	{ 0x11, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Graphire2 4x5 */
	{ 0x12, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Graphire2 5x7 */

	{ 0x13, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Graphire3 4x5 */
	{ 0x14, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Graphire3 6x8 */

	{ 0x15, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* Graphire4 4x5 */
	{ 0x16, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* Graphire4 6x8 */ 
	{ 0x17, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* BambooFun 4x5 */
	{ 0x18, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* BambooFun 6x8 */
	{ 0x19, STYLUS_ID | ERASER_ID                      }, /* Bamboo1 Medium*/ 
	{ 0x81, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* Graphire4 6x8 BlueTooth */

	{ 0x20, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos 4x5 */
	{ 0x21, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos 6x8 */
	{ 0x22, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos 9x12 */
	{ 0x23, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos 12x12 */
	{ 0x24, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos 12x18 */

	{ 0x03, STYLUS_ID | ERASER_ID }, /* PTU600 */
	{ 0x30, STYLUS_ID | ERASER_ID }, /* PL400 */
	{ 0x31, STYLUS_ID | ERASER_ID }, /* PL500 */
	{ 0x32, STYLUS_ID | ERASER_ID }, /* PL600 */
	{ 0x33, STYLUS_ID | ERASER_ID }, /* PL600SX */
	{ 0x34, STYLUS_ID | ERASER_ID }, /* PL550 */
	{ 0x35, STYLUS_ID | ERASER_ID }, /* PL800 */
	{ 0x37, STYLUS_ID | ERASER_ID }, /* PL700 */
	{ 0x38, STYLUS_ID | ERASER_ID }, /* PL510 */
	{ 0x39, STYLUS_ID | ERASER_ID }, /* PL710 */ 
	{ 0xC0, STYLUS_ID }, /* DTF720 */
	{ 0xC2, STYLUS_ID }, /* DTF720a */
	{ 0xC4, STYLUS_ID | ERASER_ID }, /* DTF521 */ 
	{ 0xC7, STYLUS_ID | ERASER_ID }, /* DTU1931 */

	{ 0x41, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos2 4x5 */
	{ 0x42, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos2 6x8 */
	{ 0x43, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos2 9x12 */
	{ 0x44, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos2 12x12 */
	{ 0x45, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos2 12x18 */
	{ 0x47, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos2 6x8  */

	{ 0x60, STYLUS_ID }, /* Volito */ 
	{ 0x61, STYLUS_ID }, /* PenStation */
	{ 0x62, STYLUS_ID }, /* Volito2 4x5 */
	{ 0x63, STYLUS_ID }, /* Volito2 2x3 */
	{ 0x64, STYLUS_ID }, /* PenPartner2 */

	{ 0x65, STYLUS_ID | ERASER_ID | CURSOR_ID |  PAD_ID }, /* Bamboo */
	{ 0x69, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Bamboo1 */ 

	{ 0xB0, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos3 4x5 */
	{ 0xB1, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos3 6x8 */
	{ 0xB2, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos3 9x12 */
	{ 0xB3, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos3 12x12 */
	{ 0xB4, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos3 12x19 */
	{ 0xB5, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos3 6x11 */
	{ 0xB7, STYLUS_ID | ERASER_ID | CURSOR_ID }, /* Intuos3 4x6 */

	{ 0xB8, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* Intuos4 4x6 */
	{ 0xB9, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* Intuos4 6x9 */
	{ 0xBA, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* Intuos4 8x13 */
	{ 0xBB, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* Intuos4 12x19*/

	{ 0x3F, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* Cintiq 21UX */ 
	{ 0xC5, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* Cintiq 20WSX */ 
	{ 0xC6, STYLUS_ID | ERASER_ID | CURSOR_ID | PAD_ID }, /* Cintiq 12WX */ 

	{ 0x90, STYLUS_ID | ERASER_ID }, /* TabletPC 0x90 */ 
	{ 0x93, STYLUS_ID | ERASER_ID  | TOUCH_ID }, /* TabletPC 0x93 */
	{ 0x9A, STYLUS_ID | ERASER_ID  | TOUCH_ID }, /* TabletPC 0x9A */
	{ 0x9F, TOUCH_ID }, /* CapPlus  0x9F */
	{ 0xE2, TOUCH_ID }, /* TabletPC 0xE2 */ 
	{ 0xE3, STYLUS_ID | ERASER_ID | TOUCH_ID }  /* TabletPC 0xE3 */
};

static struct
{
	const char* type;
	__u16 id;
} wcmTypeAndID [] =
{
	{ "stylus", STYLUS_ID },
	{ "eraser", ERASER_ID },
	{ "cursor", CURSOR_ID },
	{ "touch",  TOUCH_ID  },
	{ "pad",    PAD_ID    }
};

static Bool checkValidType(char* type, unsigned short id)
{
	int i, j, ret = FALSE;

	/* walkthrough all supported models */
	for (i = 0; i < sizeof (validType) / sizeof (validType [0]); i++)
	{
		if (validType[i].productID == id)
		{
			/* walkthrough all types */
			for (j = 0; j < sizeof (wcmTypeAndID) / sizeof (wcmTypeAndID [0]); j++)
			    if (!strcmp(wcmTypeAndID[j].type, type))
				if (wcmTypeAndID[j].id & validType[i].flags)
					ret = TRUE;
		}		
	}
	return ret;
}

static Bool aTouchPort(char* device)
{
	int fd = -1;
	unsigned long keys[NBITS(KEY_MAX)];

	SYSCALL(fd = open(device, O_RDONLY));
	if (fd < 0)
		return FALSE;

	/* test if BTN_TOOL_DOUBLETAP set or not for touch device */
	if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0)
	{
		xf86Msg(X_ERROR, "WACOM: aTouchPort unable to ioctl key bits.\n");
		return FALSE;
	}
	close(fd);

	/* BTN_TOOL_DOUBLETAP is used to define touch tools */
	if (ISBITSET (keys, BTN_TOOL_DOUBLETAP))
		return TRUE;
	else
		return FALSE;
}

/* validate tool type for device/product */
Bool wcmIsAValidType(char* device, LocalDevicePtr local, unsigned short id)
{
	Bool ret = FALSE;
	char* type = xf86FindOptionValue(local->options, "Type");

	/* touch tool has its own port. 
	 * we need to distinguish it from the others first */
	if (checkValidType("touch", id))
	{
		if (aTouchPort(device))
		{
			if (strstr(type, "touch"))
				return TRUE;
			else
				return FALSE;
		}
		else
		{
			if (strstr(type, "touch"))
				return FALSE;
		}
	}

	/* not a touch tool or touch is not support forthe id */
	ret = checkValidType(type, id);

	return ret;
}
#endif   /* WCM_XORG_XSERVER_1_4 */

