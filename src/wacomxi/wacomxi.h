/*
 * wacomxi.h -- Add X11 extended input handling capability for wacomcpl.
 *
 * Author		: Ping Cheng
 * Creation date	: 04/05/2003
 *
 */

/*
 *  Based on xi.h 1998-99 Patrick Lecoanet --
 *
 * This code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this code; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _WacomXi_H
#define _WacomXi_H

#include <X11/extensions/XInput.h>
/*
 * Included to obtain the number of events used by the xinput
 * extension.
 */
#include <X11/extensions/XIproto.h>
#include <X11/Xlib.h>
#include <tk.h>

/*
 * These constants are used to represent fixed
 * event types. These types are computed by
 * mapping event types send by each server. The
 * mapping is done by a table maintained for
 * each server.
 */
#define KEY_EVENT		1
#define BUTTON_EVENT		2
#define MOTION_EVENT		3
#define FOCUS_EVENT		4
#define PROXIMITY_EVENT		5
#define CHANGE_DEVICE_EVENT	6
#define DEVICE_MAPPING_EVENT	7
#define DEVICE_STATE_EVENT	8

/*
 * These constants must be kept in sync with the
 * xi_event_names array.
 */
#define KEY_PRESS		0
#define KEY_RELEASE		1
#define BUTTON_PRESS		2
#define BUTTON_PRESS_GRAB	3
#define BUTTON_PRESS_OWNER_GRAB	4
#define BUTTON_RELEASE		5
#define MOTION			6
#define MOTION_HINT		7
#define BUTTON_MOTION		8
#define B1_MOTION		9
#define B2_MOTION		10
#define B3_MOTION		11
#define B4_MOTION		12
#define B5_MOTION		13
#define FOCUS_IN		14
#define FOCUS_OUT		15
#define PROXIMITY_IN		16
#define PROXIMITY_OUT		17
#define DEVICE_STATE		18
#define DEVICE_MAPPING		19
#define CHANGE_DEVICE		20

#define	NUM_XI_EVENTS		21
#define NUM_EVENTS		256	/* Core limit of the protocol */

#define NUM_SIZE 50

static Tk_Uid xi_event_names[NUM_XI_EVENTS] = {
  "KeyPress",
  "KeyRelease",
  "ButtonPress",
  "ButtonPressGrab",
  "ButtonPressOwnerGrab",
  "ButtonRelease",
  "Motion",
  "MotionHint",
  "ButtonMotion",
  "B1Motion",
  "B2Motion",
  "B3Motion",
  "B4Motion",
  "B5Motion",
  "FocusIn",
  "FocusOut",
  "ProximityIn",
  "ProximityOut",
  "DeviceState",
  "DeviceMapping",
  "ChangeDevice",
};

typedef struct _EventHandlerStruct {
  Tk_EventProc			*proc;
  ClientData			client_data;
  int				type;
  XID				device_id;
  int				num_classes;
  int				classes[3];
  Tk_Window			tkwin;
  struct _EventHandlerStruct	*next;
} EventHandlerStruct;

typedef struct _EventScriptRecord {
  struct _DeviceInfoStruct	*device;
  Tk_Uid			event_spec;
  Tk_Window			tkwin;
  Tcl_Interp			*interp;
  char				*script;
  struct _EventScriptRecord	*next;
} EventScriptRecord;

typedef struct _AxeInfo {
  int	min_value;
  int	max_value;
  int	resolution;
  int	value;
} AxeInfo;

typedef struct _WindowInfoStruct {
  EventHandlerStruct	*handlers;
  EventScriptRecord	*scripts;
} WindowInfoStruct;

typedef struct _InProgress {
  ClientData		next_handler;	/* Next handler in search. */
  struct _InProgress	*next;		/* Next higher nested search. */
} InProgress;

typedef struct _DeviceInfoStruct {
  struct _DisplayInfoStruct *dpy_info;
  XDevice	*xdev;
  Tk_Uid	name;
  XID		id;
  char		core;		/* If the device is the core keyboard or pointer */
  unsigned char	x_index;	/* index of valuator corresponding to core X */
  unsigned char	y_index;	/* index of valuator corresponding to core Y */
  int		num_axes;
  int		num_keys;
  int		num_buttons;
  char		focusable;	/* True if the device can be explicitly focused */
  char		proximity;	/* True if the device can emit proximity events */
  char		feedback;	/* True if the device has some feedback control */
  int		mode;		/* ABSOLUTE or RELATIVE */
  int		history_size;
  Time		last_motion_time;
  
  AxeInfo	*axe_info;
  int		*valuator_cache; /* Used to store valuators if the device reports
				  * more than six axes. The array is allocated
				  * dynamically to match the number of axes. */
  int		event_classes[NUM_XI_EVENTS];
  int		no_event_class;
} DeviceInfoStruct;

typedef struct _DisplayInfoStruct {
  char		has_xdevices;		/* Non zero if some devices are available
					 * (other than core devices). */
  Display	*display;		/* The display that is described. */
  DeviceInfoStruct *devices;		/* The device list */
  int		num_dev;
  char		event_types[NUM_XI_EVENTS];
  char		event_atypes[NUM_EVENTS]; /* Mapping from server local event types
					   * to absolutes types independant from
					   * servers. */
  int		event_base;		/* The first event type reported by the
					 * xinput extension on this server. */
  Tcl_HashTable	per_wins;		/* Records for all handlers per windows on
					 * this display. */
  EventHandlerStruct *other_handlers;	/* The handlers associated with events not
					 * reported relative to a window. */
  EventHandlerStruct *frozen_handlers;	/* The handlers on hold because theirs
					 * devices are currently core devices. */
  struct _DisplayInfoStruct *next;
} DisplayInfoStruct;

int Tk_CreateXiEventHandler(Tk_Window, Tk_Uid, Tk_Uid, DeviceInfoStruct *, Tk_EventProc *,
			    ClientData);
void Tk_DeleteXiEventHandler(Tk_Window, Tk_Uid, Tk_Uid, DeviceInfoStruct *, Tk_EventProc *,
			     ClientData);
int Tk_DispatchXiEvent(XEvent *);
int LibwacomXi_Init(Tcl_Interp *interp);

#endif
