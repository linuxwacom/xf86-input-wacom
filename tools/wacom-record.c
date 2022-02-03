/*
 * Copyright 2021 Red Hat, Inc.
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

#include <config.h>
#include "config-ver.h"

#include <stdbool.h>
#include <stdio.h>
#include <glib.h>
#include <glib-unix.h>
#include <libudev.h>
#include <libevdev/libevdev.h>
#include "wacom-driver.h"
#include "wacom-device.h"

#define strbool(x_)  (x_) ? "true" : "false"

static guint debug_level = 0;
static gboolean print_version = false;
static gboolean grab_device = false;
static gboolean log_evdev = false;
static const char *driver_options = NULL;

static GOptionEntry opts[] =
{
	{ "version", 0, 0, G_OPTION_ARG_NONE, &print_version, "Print version and exit", NULL },
	{ "debug-level", 'v', 0, G_OPTION_ARG_INT, &debug_level, "Set the debug level", NULL },
	{ "options", 0, 0, G_OPTION_ARG_STRING, &driver_options, "Driver options in the form \"Foo=bar,Baz=bat\"", NULL },
	{ "grab", 0, 0, G_OPTION_ARG_NONE, &grab_device, "Grab the device while recording", NULL },
	{ "evdev", 0, 0, G_OPTION_ARG_NONE, &log_evdev, "Log evdev events", NULL },
	{ NULL },
};

static void log_message(WacomDevice *device, const char *type, const char *message)
{
	printf("# [%s] %s: %s", type, wacom_device_get_name(device), message);
}

static void debug_message(WacomDevice *device, int debug_level, const char *func, const char *message)
{
	printf("# DBG%02d %-35s| %s: %s", debug_level, func, wacom_device_get_name(device), message);
}

static inline void print_axes(const WacomEventData *data)
{
	char buf[1024] = {0};
	const char *prefix = "";
	uint32_t mask = data->mask;

	for (uint32_t flag = 0x1; flag <= _WACOM_EVENT_AXIS_LAST; flag <<= 1) {
		const char *name = "unknown axis";
		if ((mask & flag) == 0)
			continue;

		switch (flag) {
		case WACOM_X: name = "x"; break;
		case WACOM_Y: name = "y"; break;
		case WACOM_PRESSURE: name = "pressure"; break;
		case WACOM_TILT_X: name = "tilt-x"; break;
		case WACOM_TILT_Y: name = "tilt-y"; break;
		case WACOM_ROTATION: name = "rotation"; break;
		case WACOM_THROTTLE: name = "throttle"; break;
		case WACOM_WHEEL: name = "wheel"; break;
		case WACOM_RING: name = "ring"; break;
		case WACOM_RING2: name = "ring"; break;
		}

		g_assert_cmpint(strlen(buf) + strlen(prefix) + strlen(name), <, sizeof(buf));

		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s\"%s\"", prefix, name);
		prefix = ", ";
	}

	printf("      mask: [ %s ]\n", buf);
	printf("      axes: { x: %5d, y: %5d, pressure: %4d, tilt: [%3d,%3d], rotation: %3d, throttle: %3d, wheel: %3d, rings: [%3d, %3d] }\n",
	       data->x, data->y,
	       (data->mask & WACOM_PRESSURE) ? data->pressure : 0,
	       (data->mask & WACOM_TILT_X) ? data->tilt_x : 0,
	       (data->mask & WACOM_TILT_Y) ? data->tilt_y : 0,
	       (data->mask & WACOM_ROTATION) ? data->rotation : 0,
	       (data->mask & WACOM_THROTTLE) ? data->throttle : 0,
	       (data->mask & WACOM_WHEEL) ? data->wheel : 0,
	       (data->mask & WACOM_RING) ? data->ring : 0,
	       (data->mask & WACOM_RING2) ? data->ring2 : 0);
}

static void proximity(WacomDevice *device, gboolean is_prox_in, WacomEventData *data)
{
	printf("    - source: %d\n"
	       "      event: proximity\n"
	       "      proximity-in: %s\n",
	       wacom_device_get_id(device), strbool(is_prox_in));
	print_axes(data);
}

static void motion(WacomDevice *device, gboolean is_absolute, WacomEventData *data)
{
	printf("    - source: %d\n"
	       "      mode: %s\n"
	       "      event: motion\n",
	       wacom_device_get_id(device),
	       is_absolute ? "absolute" : "relative");
	print_axes(data);
}

static void button(WacomDevice *device, gboolean is_absolute, int button,
		   gboolean is_press, WacomEventData *data)
{
	printf("    - source: %d\n"
	       "      event: button\n"
	       "      button: %d\n"
	       "      is-press: %s\n",
	       wacom_device_get_id(device), button, strbool(is_press));
	print_axes(data);
}

static void key(WacomDevice *device, gboolean keycode, gboolean is_press)
{
	printf("    - source: %d\n"
	       "      event: key\n"
	       "      key: %d\n"
	       "      is-press: %s\n", wacom_device_get_id(device),
	       keycode, strbool(is_press));
}

static void evdev(WacomDevice *device, const struct input_event *evdev)
{
	printf("    - { source: %d, event: evdev, data: [%6ld, %6ld, %3d, %3d, %10d] } # %s / %-20s %5d\n",
	       wacom_device_get_id(device),
	       evdev->input_event_sec,
	       evdev->input_event_usec,
	       evdev->type,
	       evdev->code,
	       evdev->value,
	       libevdev_event_type_get_name(evdev->type),
	       libevdev_event_code_get_name(evdev->type, evdev->code),
	       evdev->value
	);
}

static void device_added(WacomDriver *driver, WacomDevice *device)
{
	printf("    - source: %d\n"
	       "      event: new-device\n"
	       "      name: \"%s\"\n",
	       wacom_device_get_id(device), wacom_device_get_name(device));

	g_signal_connect(device, "log-message", G_CALLBACK(log_message), NULL);
	g_signal_connect(device, "debug-message", G_CALLBACK(debug_message), NULL);
	g_signal_connect(device, "motion", G_CALLBACK(motion), NULL);
	g_signal_connect(device, "proximity", G_CALLBACK(proximity), NULL);
	g_signal_connect(device, "button", G_CALLBACK(button), NULL);
	g_signal_connect(device, "keycode", G_CALLBACK(key), NULL);
	if (log_evdev)
		g_signal_connect(device, "evdev-event", G_CALLBACK(evdev), NULL);

	if (!wacom_device_preinit(device))
		fprintf(stderr, "Failed to preinit device %s\n", wacom_device_get_name(device));
	else if (!wacom_device_setup(device))
		fprintf(stderr, "Failed to setup device %s\n", wacom_device_get_name(device));
	else if (!wacom_device_enable(device))
		fprintf(stderr, "Failed to enable device %s\n", wacom_device_get_name(device));
	else {
		const char *typestr = NULL;

		switch(wacom_device_get_tool_type(device)) {
			case WACOM_TOOL_INVALID: typestr = "invalid"; break;
			case WACOM_TOOL_STYLUS: typestr = "stylus"; break;
			case WACOM_TOOL_ERASER: typestr = "eraser"; break;
			case WACOM_TOOL_CURSOR: typestr = "cursor"; break;
			case WACOM_TOOL_PAD: typestr = "pad"; break;
			case WACOM_TOOL_TOUCH: typestr = "touch"; break;

		}

		printf("      type: %s\n", typestr);
		printf("      capabilities:\n"
		       "        keys: %s\n"
		       "        is-absolute: %s\n"
		       "        is-direct-touch: %s\n"
		       "        ntouches: %d\n"
		       "        naxes: %d\n",
		       strbool(wacom_device_has_keys(device)),
		       strbool(wacom_device_is_absolute(device)),
		       strbool(wacom_device_is_direct_touch(device)),
		       wacom_device_get_num_touches(device),
		       wacom_device_get_num_axes(device));
		printf("        axes:\n");
		for (enum WacomEventAxis which = WACOM_X; which <= WACOM_RING2; which <<= 1) {
			const WacomAxis *axis = wacom_device_get_axis(device, which);
			const char *typestr = NULL;

			if (!axis)
				continue;

			switch (axis->type) {
				case WACOM_X: typestr = "x"; break;
				case WACOM_Y: typestr = "y"; break;
				case WACOM_PRESSURE: typestr = "pressure"; break;
				case WACOM_TILT_X: typestr = "tilt_x"; break;
				case WACOM_TILT_Y: typestr = "tilt_y"; break;
				case WACOM_STRIP_X: typestr = "strip_x"; break;
				case WACOM_STRIP_Y: typestr = "strip_y"; break;
				case WACOM_ROTATION: typestr = "rotation"; break;
				case WACOM_THROTTLE: typestr = "throttle"; break;
				case WACOM_WHEEL: typestr = "wheel"; break;
				case WACOM_RING: typestr = "ring"; break;
				case WACOM_RING2: typestr = "ring2"; break;
			}

			printf("          - {type: %-12s, range: [%5d, %5d], resolution: %5d}\n",
			       typestr, axis->min, axis->max, axis->res);

		}
		return;
	}

	wacom_device_remove(device);
}


static void device_removed(WacomDriver *driver, WacomDevice *device)
{
	printf("    - source: %d\n"
	       "      event: removed-device\n"
	       "      name: \"%s\"\n",
	       wacom_device_get_id(device), wacom_device_get_name(device));
}

static char *find_device(void)
{
	const char *entry;
	GDir *dir = g_dir_open("/dev/input/", 0, NULL);
	struct udev *udev;
	const char *prefix = isatty(STDERR_FILENO) ? "" : "#";
	int selected_device;

	if (!dir)
		return NULL;

	udev = udev_new();
	fprintf(stderr, "%sAvailable devices:\n", prefix);

	while ((entry = g_dir_read_name(dir))) {
		struct udev_device *dev, *parent;
		const char *name;
		g_autofree char *syspath = NULL;

		if (!g_str_has_prefix(entry, "event"))
			continue;

		syspath = g_strdup_printf("/sys/class/input/%s", entry);
		dev = udev_device_new_from_syspath(udev, syspath);
		if (!dev)
			continue;

		parent = udev_device_get_parent(dev);
		name = udev_device_get_sysattr_value(parent, "name");
		fprintf(stderr, "%s/dev/input/%s: %s\n", prefix, entry, name);

		udev_device_unref(dev);
	}
	fprintf(stderr, "%sSelect the device event number: ", prefix);
	if (scanf("%d", &selected_device) != 1 || selected_device < 0)
		return NULL;

	udev_unref(udev);
	g_dir_close(dir);

	return g_strdup_printf("/dev/input/event%d", selected_device);
}

static void print_device_info(const char *path, char *name)
{
	printf("  device:\n");
	printf("    path: %s\n", path);
	printf("    name: \"%s\"\n", name);
}

static char *get_device_name(const char *path)
{
	struct udev *udev;
	struct udev_device *dev, *parent;
	g_autofree char *basename = g_path_get_basename(path);
	g_autofree char *syspath;
	char *name = NULL;

	udev = udev_new();

	/* This won't work for symlinks. Oh well */
	syspath = g_strdup_printf("/sys/class/input/%s", basename);
	dev = udev_device_new_from_syspath(udev, syspath);
	if (dev) {
		const char *udev_name;
		parent = udev_device_get_parent(dev);
		udev_name = udev_device_get_sysattr_value(parent, "name");
		name  = g_strdup(udev_name);
	}

	udev_device_unref(dev);
	udev_unref(udev);
	return name;
}

static gboolean cb_sigint(gpointer loop)
{
	fprintf(stderr, "Exiting\n");
	g_main_loop_quit(loop);
	return FALSE;
}


int main(int argc, char **argv)
{
	g_autoptr(WacomDriver) driver = NULL;
	g_autoptr(WacomDevice) device = NULL;
	g_autoptr(WacomOptions) options = NULL;
	g_autoptr(GMainLoop) loop = NULL;
	g_autoptr(GOptionContext) context = g_option_context_new("- record events from a Wacom device");
	GError *error = NULL;
	g_autofree char *path = NULL;
	g_autofree char *name = NULL;

	g_option_context_add_main_entries(context, opts, NULL);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		return 2;
	}

	if (print_version) {
		printf("%s\n", PACKAGE_VERSION);
		return 0;
	}

	if (argc <= 1)
		path = find_device();
	else
		path = strdup(argv[1]);

	if (!path) {
		fprintf(stderr, "Invalid device path or unable to find device");
		return 1;
	}

	printf("wacom-record:\n");
	printf("  version: %s\n", PACKAGE_VERSION);
	printf("  git: %s\n", BUILD_VERSION);

	name = get_device_name(path);
	print_device_info(path, name);

	driver = wacom_driver_new();
	options	= wacom_options_new("device", path, NULL);
	if (grab_device)
		wacom_options_set(options, "Grab", "true");
	if (debug_level) {
		g_autofree char *lvl = g_strdup_printf("%d", debug_level);
		wacom_options_set(options, "DebugLevel", lvl);
		wacom_options_set(options, "CommonDBG", lvl);
	}
	if (driver_options) {
		g_auto(GStrv) strv = g_strsplit(driver_options, ",", -1);
		char **opt = strv;

		printf("  options:\n");

		while (*opt) {
			g_auto(GStrv) kv = g_strsplit(*opt, "=", 2);
			g_return_val_if_fail(kv[0] != NULL, 1);
			g_return_val_if_fail(kv[1] != NULL, 1);
			printf("    - %s: \"%s\"\n", kv[0], kv[1]);
			wacom_options_set(options, kv[0], kv[1]);
			opt++;
		}
	}

	printf("  events:\n");

	g_signal_connect(driver, "device-added", G_CALLBACK(device_added), NULL);
	g_signal_connect(driver, "device-removed", G_CALLBACK(device_removed), NULL);

	++argv; --argc; /* first arg is already in path */
	while (true) {
		g_autofree char *path = NULL;

		device = wacom_device_new(driver, name, options);
		if (!device) {
			fprintf(stderr, "Unable to record device %s - is this a Wacom tablet?\n", path);
			return 1;
		}

		if (--argc <= 0)
			break;

		path = strdup(*(++argv));
		wacom_options_set(options, "device", path);
	}


	loop = g_main_loop_new(NULL, FALSE);
	g_unix_signal_add(SIGINT, cb_sigint, loop);
	g_main_loop_run(loop);

	return 0;
}
