/*
 * Licensed under the GNU General Public License Version 2
 *
 * Copyright (C) 2009 Red Hat <mjg@redhat.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <hal/libhal.h>

static LibHalContext *ctx = NULL;
static char* udi;

int
main (int argc, char **argv)
{
	char *device;
	char *newudi;
	char *forcedev;
	char *name;
	char *subname;
	char **types;
	int i;
	DBusError error;

	udi = getenv ("UDI");
	if (udi == NULL) {
		fprintf (stderr, "hal-setup-wacom: Failed to get UDI\n");
		return 1;
	}

	asprintf (&newudi, "%s_subdev", udi);

	dbus_error_init (&error);
	if ((ctx = libhal_ctx_init_direct (&error)) == NULL) {
		fprintf (stderr, "hal-setup-wacom: Unable to initialise libhal context: %s\n", error.message);
		return 1;
	}

	dbus_error_init (&error);
	if (!libhal_device_addon_is_ready (ctx, udi, &error)) {
		return 1;
	}

	dbus_error_init (&error);

	/* get the device */
	device = libhal_device_get_property_string (ctx, udi, "input.device",
						    &error);
	if (dbus_error_is_set (&error) == TRUE) {
		fprintf (stderr,
			 "hal-setup-wacom: Failed to get input device: '%s'\n",
			 error.message);
		return 1;
	}

	/* Is there a forcedevice? */
	dbus_error_init (&error);
	forcedev = libhal_device_get_property_string
		(ctx, udi, "input.x11_options.ForceDevice", &error);

	dbus_error_init (&error);
	name = libhal_device_get_property_string (ctx, udi, "info.product",
						  &error);

	dbus_error_init (&error);
	types = libhal_device_get_property_strlist (ctx, udi, "wacom.types",
						    &error);

	if (dbus_error_is_set (&error) == TRUE) {
		fprintf (stderr,
			 "hal-setup-wacom: Failed to get wacom types: '%s'\n",
			 error.message);
		return 1;
	}

	/* Set up the extra devices */
	for (i=0; types[i] != NULL; i++) {
		char *tmpdev;

		dbus_error_init (&error);
		tmpdev = libhal_new_device(ctx, &error);
		if (dbus_error_is_set (&error) == TRUE) {
			fprintf (stderr,
				 "hal-setup-wacom: Failed to create input device: '%s'\n",
				 error.message);
			return 1;
		}
		dbus_error_init (&error);
		libhal_device_set_property_string (ctx, tmpdev, "input.device",
						   device, &error);
		dbus_error_init (&error);
		libhal_device_set_property_string (ctx, tmpdev,
						   "input.x11_driver", "wacom",
						   &error);
		dbus_error_init (&error);
		libhal_device_set_property_string (ctx, tmpdev,
						   "input.x11_options.Type",
						   types[i], &error);
		dbus_error_init (&error);
		libhal_device_set_property_string (ctx, tmpdev, "info.parent",
						   udi, &error);
		dbus_error_init (&error);
		libhal_device_property_strlist_append (ctx, tmpdev,
						       "info.capabilities",
						       "input", &error);
		if (forcedev) {
			dbus_error_init (&error);
			libhal_device_set_property_string (ctx, tmpdev,
							   "input.x11_options.ForceDevice",
							   forcedev, &error);
		}
		if (name) {
			dbus_error_init (&error);
			asprintf (&subname, "%s %s", name, types[i]);
			libhal_device_set_property_string (ctx, tmpdev,
							   "info.product",
							   subname, &error);
			free (subname);
		}
		dbus_error_init (&error);
		libhal_device_commit_to_gdl (ctx, tmpdev, newudi, &error);

		if (dbus_error_is_set (&error) == TRUE) {
			fprintf (stderr,
				 "hal-setup-wacom: Failed to add input device: '%s'\n",
				 error.message);
			return 1;
		}
	}

	return 0;
}

