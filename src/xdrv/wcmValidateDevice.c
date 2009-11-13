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
 * xf86WcmCheckSource - Check if there is another source defined this device
 * before or not: don't add the tool by hal/udev if user has defined at least 
 * one tool for the device in xorg.conf. One device can have multiple tools
 * with the same type to individualize tools with serial number or areas
 ****************************************************************************/
static Bool xf86WcmCheckSource(LocalDevicePtr local, dev_t min_maj)
{
	int match = 0;
	char* device;
	char* lSource = xf86CheckStrOption(local->options, "_source", "");
	LocalDevicePtr pDevices = xf86FirstLocalDevice();
	WacomCommonPtr pCommon = NULL;
	char* pSource;

	for (; pDevices != NULL; pDevices = pDevices->next)
	{
		device = xf86CheckStrOption(pDevices->options, "Device", NULL);

		/* device can be NULL on some distros */
		if (!device || !strstr(pDevices->drv->driverName, "wacom"))
			continue;

		if (local != pDevices)
		{
			pSource = xf86CheckStrOption(pDevices->options, "_source", "");
			pCommon = ((WacomDevicePtr)pDevices->private)->common;
			if ( pCommon->min_maj && pCommon->min_maj == min_maj)
			{
				/* only add the new tool if the matching major/minor
				 * was from the same source */
				if (strcmp(lSource, pSource))
				{
					match = 1;
					break;
				}
			}
 		}
	}

	if (match)
		xf86Msg(X_WARNING, "%s: device file already in use by %s."
			" Ignoring.\n", local->name, pDevices->name);

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
	char* lSource = xf86CheckStrOption(local->options, "_source", "");
	local->fd = -1;

	/* always allow xorg.conf defined tools to be added */
	if (!strlen(lSource)) goto ret;

	/* open the port */
	do {
        	SYSCALL(local->fd = open(device, O_RDONLY, 0));
	} while (local->fd < 0 && errno == EINTR);

	if (local->fd < 0)
	{
		/* can not open the device */
        	xf86Msg(X_ERROR, "Unable to open Wacom device \"%s\".\n", device);
		isInUse = 1;
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
		/* device matches with another added port */
		if (xf86WcmCheckSource(local, st.st_rdev))
		{
			isInUse = 3;
			goto ret;
		}
	}
	else
	{
		/* major/minor can never be 0, right? */
		xf86Msg(X_ERROR, "%s: device opened with a major/minor of 0. "
			"Something was wrong.\n", local->name);
		isInUse = 4;
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
	const char* type;
	__u16 tool;
} wcmType [] =
{
	{ "stylus", BTN_TOOL_PEN       },
	{ "eraser", BTN_TOOL_RUBBER    },
	{ "cursor", BTN_TOOL_MOUSE     },
	{ "touch",  BTN_TOOL_DOUBLETAP },
	{ "pad",    BTN_TOOL_FINGER    }
};

static Bool checkValidType(char* type, unsigned long* keys)
{
	int j, ret = FALSE;

	/* walkthrough all types */
	for (j = 0; j < sizeof (wcmType) / sizeof (wcmType [0]); j++)
	{
		if (!strcmp(wcmType[j].type, type))
			if (ISBITSET (keys, wcmType[j].tool))
				ret = TRUE;
	}
	return ret;
}

/* validate tool type for device/product */
Bool wcmIsAValidType(char* device, LocalDevicePtr local)
{
	Bool ret = FALSE;
	int fd = -1;
	unsigned long keys[NBITS(KEY_MAX)];
	char* type = xf86FindOptionValue(local->options, "Type");

	SYSCALL(fd = open(device, O_RDONLY));
	if (fd < 0)
		return FALSE;

	/* test if the tool is defined in the kernel */
	if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0)
	{
		xf86Msg(X_ERROR, "WACOM: wcmIsAValidType unable to ioctl key bits.\n");
		return FALSE;
	}
	close(fd);

	ret = checkValidType(type, keys);

	return ret;
}
#endif   /* WCM_XORG_XSERVER_1_4 */

