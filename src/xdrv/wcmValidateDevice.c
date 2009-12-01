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
    #include <linux/serial.h>

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

/* validate tool type for device/product */
Bool wcmIsAValidType(const char* type, unsigned long* keys)
{
	int j, ret = FALSE;
ErrorF("wcmIsAValidType type %s \n", type);
	if (!type)
		return ret;

	/* walkthrough all types */
	for (j = 0; j < sizeof (wcmType) / sizeof (wcmType [0]); j++)
	{
		if (!strcmp(wcmType[j].type, type))
			if (ISBITSET (keys, wcmType[j].tool))
			{
				ret = TRUE;
				break;
			}
	}
ErrorF("wcmIsAValidType found type %s %d \n", type, ret);
	return ret;
}

/* Choose valid types according to device ID */
int wcmDeviceTypeKeys(LocalDevicePtr local, unsigned long* keys)
{
	int ret = 1, i, fd = -1;
	unsigned int id = 0;
	char* device, *stopstring;
	char* str = strstr(local->name, "WACf");
	struct serial_struct tmp;

	device = xf86SetStrOption(local->options, "Device", NULL);

	SYSCALL(fd = open(device, O_RDONLY));
	if (fd < 0)
	{
		xf86Msg(X_WARNING, "%s: failed to open %s in "
			"wcmDeviceTypeKeys.\n", local->name, device);
		return 0;
	}

	/* we have tried memset. it doesn't work */
	for (i=0; i<NBITS(KEY_MAX); i++)
		keys[i] = 0;

	/* serial ISDV4 devices */
	if (ioctl(fd, TIOCGSERIAL, &tmp) == 0)
	{
		if (str) /* id in name */
		{
			str = str + 4;
			if (str)
				id = (int)strtol(str, &stopstring, 16);

		}
		else /* id in file /sys/class/tty/%str/device/id */
		{
			FILE *file;
			char sysfs_id[256];
			str = strstr(device, "ttyS");
			snprintf(sysfs_id, sizeof(sysfs_id),
				"/sys/class/tty/%s/device/id", str);
			file = fopen(sysfs_id, "r");

			if (file)
			{
				/* make sure we fall to default */
				if (fscanf(file, "WACf%x\n", &id) <= 0)
					id = 0;
			}
		}
		/* default to penabled */
		keys[LONG(BTN_TOOL_PEN)] |= BIT(BTN_TOOL_PEN);
		keys[LONG(BTN_TOOL_RUBBER)] |= BIT(BTN_TOOL_RUBBER);

		/* id < 0x008 are only penabled */
		if (id > 0x007)
		{
			keys[LONG(BTN_TOOL_DOUBLETAP)] |= BIT(BTN_TOOL_DOUBLETAP);
		}

		/* no pen 2FGT */
		if (id == 0x010)
		{
			keys[LONG(BTN_TOOL_PEN)] &= ~BIT(BTN_TOOL_PEN);
			keys[LONG(BTN_TOOL_RUBBER)] &= ~BIT(BTN_TOOL_RUBBER);
		}
	}
	else /* USB devices */
	{
		/* test if the tool is defined in the kernel */
		if (ioctl(fd, EVIOCGBIT(EV_KEY, (sizeof(unsigned long)
			 * NBITS(KEY_MAX))), keys) < 0)
		{
			xf86Msg(X_ERROR, "%s: wcmDeviceTypeKeys unable to "
				"ioctl USB key bits.\n", local->name);
			ret = 0;
		}
	}
	close(fd);
	return ret;
}
#endif   /* WCM_XORG_XSERVER_1_4 */

