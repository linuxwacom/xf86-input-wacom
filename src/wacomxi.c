/*
 * wacomxi.c -- Add X11 extended input handling capability for wacomcpl.
 *
 * Author		: Ping Cheng
 * Creation date	: 04/05/2003
 *
 * Based on xi.c 1998-99 Patrick Lecoanet --
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <X11/extensions/XInput.h>
/*
 * Included to obtain the number of events used by the xinput
 * extension.
 */
#include <X11/extensions/XIproto.h>

#include "wacomxi.h"

/*
 *----------------------------------------------------------------------
 *
 * This code implements tcl commands that give access
 * to additional input devices manipulation at the tcl level.
 *
 * The command 'bindevent' binds tcl scripts to xinput events
 * occuring in a widget.
 *
 *----------------------------------------------------------------------
 */

#define CORE_KEYBOARD	1
#define CORE_POINTER	2

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

typedef struct _WindowInfoStruct {
  EventHandlerStruct	*handlers;
  EventScriptRecord	*scripts;
} WindowInfoStruct;

typedef struct _InProgress {
  ClientData		next_handler;	/* Next handler in search. */
  struct _InProgress	*next;		/* Next higher nested search. */
} InProgress;


static DisplayInfoStruct *display_infos = NULL;
static InProgress *pending_handlers = NULL;

static void InvokeEventScript(ClientData client_data, XEvent *e);
static WindowInfoStruct *GetWindowInfo(Tk_Window w, int create);


/*
 *----------------------------------------------------------------------
 *
 * GetDisplayInfo --
 *	Return all known information known about the display.
 *	This gets all the needed informations on the available devices
 *	including their name.
 *	This has the side effect (on the first call) of gathering from
 *	the server, all devices that are hooked on the display.
 *
 *----------------------------------------------------------------------
 */
static DisplayInfoStruct *
GetDisplayInfo(Display	*dpy)
{
    XDeviceInfoPtr	device_list;
    DisplayInfoStruct	*info;
    DeviceInfoStruct	*device;
    int			i, axe, num_classes;
    XAnyClassPtr	any;
    XKeyInfoPtr		k;
    XButtonInfoPtr	b;
    XValuatorInfoPtr	v;
    int			dummy = 0;

/*printf("Begin of GetDisplayInfo\n");*/
  
    /*
     * Lookup the display in the already known list.
     */
    info = display_infos;
    while (info) 
    {
	if (dpy == info->display) 
	{
	    /*printf("End of GetDisplayInfo\n");*/
	    return info;
	}
	else 
	{
	    info = info->next;
	}
    }
    /*
     * Nothing found, make a new entry and fill it with all the available info.
     */
    /*
     * First ask if the server has made the xinput extension available.
     */
    info = (DisplayInfoStruct *) ckalloc(sizeof(DisplayInfoStruct));
    info->next = display_infos;
    display_infos = info;
    info->has_xdevices = XQueryExtension(dpy, INAME, &dummy,
				       &info->event_base, &dummy);
    info->display = dpy;
    Tcl_InitHashTable(&info->per_wins, TCL_ONE_WORD_KEYS);
    info->other_handlers = NULL;
    info->frozen_handlers = NULL;
    /*
     * Then ask the device list.
     */
    if (info->has_xdevices) 
    {
	device_list = (XDeviceInfoPtr) XListInputDevices(dpy, &info->num_dev);

	if (info->num_dev) 
	{
	    info->devices = (DeviceInfoStruct *) 
			ckalloc(info->num_dev * sizeof(DeviceInfoStruct));

	    dummy = 0;
	    for (device = info->devices, i = 0; i < info->num_dev; i++, device++) 
	    {
		device->dpy_info = info;
		device->xdev = NULL;
		device->id = device_list[i].id;
		device->name = Tk_GetUid(device_list[i].name);
		device->core = ((device_list[i].use == IsXExtensionDevice) ? 0 :
		((device_list[i].use == IsXPointer) ? CORE_POINTER : CORE_KEYBOARD));
		device->x_index = 0;
		device->y_index = 1;
		device->num_axes = 0;
		device->num_keys = 0;
		device->num_buttons = 0;
		device->focusable = 0;
		device->proximity = 0;
		device->feedback = 0;
		/*
		 * Setup each input class declared by the device.
		 */
		if (device_list[i].num_classes > 0) 
		{
		    num_classes = device_list[i].num_classes;
		    any = (XAnyClassPtr) device_list[i].inputclassinfo;
		    while (num_classes--) 
		    {
			switch (any->class) 
			{
			    case KeyClass:
			  	k = (XKeyInfoPtr) any;
				device->num_keys = k->num_keys;
				break;
			    case ButtonClass:
				b = (XButtonInfoPtr) any;
				device->num_buttons = b->num_buttons;
				break;
			    case ValuatorClass:
				v = (XValuatorInfoPtr) any;
				device->num_axes = v->num_axes;
				device->axe_info = (AxeInfo*) 
					ckalloc(sizeof(AxeInfo)*v->num_axes);
				device->history_size = v->motion_buffer;
				for (axe = 0; axe < v->num_axes; axe++) 
				{
				    device->axe_info[axe].min_value = 
						v->axes[axe].min_value;
				    device->axe_info[axe].max_value = 
						v->axes[axe].max_value;
				    device->axe_info[axe].resolution = 
						v->axes[axe].resolution;
				    device->axe_info[axe].value = 0;
				}
				break;
	        	    default:
		  		break;
			}
			any = (XAnyClassPtr) ((char *) any + any->length);
		    }
		}
	    }
	    XFreeDeviceList(device_list);
	}
	else 
	{
	    info->has_xdevices = 0;
	}
    }
    return info;
}

/*
 *----------------------------------------------------------------------
 *
 * LookupDeviceById --
 *	Return a device record from the display and the id of
 *	the device on the display. This is typically used to
 *	retrieve a device record from an event.
 *
 *----------------------------------------------------------------------
 */
static DeviceInfoStruct *
LookupDeviceById(Display	*dpy,
		 XID		device_id)
{
    DisplayInfoStruct	*info;
    int			i;
  
    info = GetDisplayInfo(dpy);
    for (i = 0; i < info->num_dev; i++) 
    {
	if (info->devices[i].id == device_id) 
	{
	    return &info->devices[i];
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateCoreMark --
 *	Extended event handler bound to ChangeDevice events.
 *
 *	It's purpose is to maintain the core bits (CORE_POINTER,
 *	CORE_KEYBOARD) in the device structs when the core devices
 *	change.
 *	This also implies that all handlers registered for the new
 *	core device are put on hold and all handlers put on hold for
 *	the previous core device are revived.
 *
 *----------------------------------------------------------------------
 */
static void
UpdateCoreMark(ClientData	client_data,
	       XEvent		*e)
{
    XChangeDeviceNotifyEvent	*cdne = (XChangeDeviceNotifyEvent *) e;
    DeviceInfoStruct		*device, *old_device = NULL;
    DisplayInfoStruct		*info;
    int				i, event_atype;
    EventHandlerStruct		*handlers, *h_prev, *h_next;
    Tcl_HashEntry		*entry;
    Tcl_HashSearch		search;
    WindowInfoStruct		*pw;
  
/*printf("UpdateCoreMark: type=%d, win=0x%X\n", e->type, (int)e->xany.window);*/

    device = LookupDeviceById(cdne->display, cdne->deviceid);
    info = device->dpy_info;
    for (i = 0; i < info->num_dev; i++) 
    {
	if ((info->devices[i].core == CORE_POINTER) &&
		(cdne->request == NewPointer)) 
	{
	    old_device = &info->devices[i];
	    old_device->core = 0;
/*printf("old pointer is %s\n", old_device->name);*/
	    break;
	}
	else if ((info->devices[i].core == CORE_KEYBOARD) &&
		(cdne->request == NewKeyboard)) 
	{
	    old_device = &info->devices[i];
	    old_device->core = 0;
	    break;
	}
    }
    if (cdne->request == NewKeyboard) 
    {
	device->core = CORE_KEYBOARD;
    }
    else if (cdne->request == NewPointer) 
    {
	device->core = CORE_POINTER;
/*printf("new pointer is %s\n", device->name);*/
    }
  
    /*
     * Try to unfreeze all handlers associated with the previous
     * core device.
     */
    for (handlers = info->frozen_handlers, h_prev = NULL; handlers != NULL;
       handlers = h_next) 
    {
	h_next = handlers->next;
	if (handlers->device_id == old_device->id) 
	{
	    if (handlers == info->frozen_handlers) 
	    {
		info->frozen_handlers = handlers->next;
	    }
	    else 
	    {
		h_prev->next = handlers->next;
	    }
	    event_atype = info->event_atypes[handlers->type];
	    if ((event_atype == CHANGE_DEVICE_EVENT) ||
			(event_atype == DEVICE_MAPPING_EVENT) ||
			(event_atype == DEVICE_STATE_EVENT)) 
	    {
		handlers->next = info->other_handlers;
		info->other_handlers = handlers;
	    }
	    else 
	    {
		WindowInfoStruct *pw = GetWindowInfo(handlers->tkwin, 0);	
		handlers->next = pw->handlers;
		pw->handlers = handlers;
	    }
	}
	else 
	{
	    h_prev = handlers;
	}
    }
    /*
     * Then try to freeze all handlers associated with the new
     * core device. First traverse the other_handlers list and
     * then all the per window handlers.
     */
    for (handlers = info->other_handlers, h_prev = NULL; 
		handlers != NULL; handlers = h_next) 
    {
	h_next = handlers->next;
	if (handlers->device_id == device->id) 
	{
	    if (handlers == info->other_handlers) 
	    {
		info->other_handlers = handlers->next;
	    }
	    else 
	    {
		h_prev->next = handlers->next;
	    }
	    handlers->next = info->frozen_handlers;
	    info->frozen_handlers = handlers;
	}
	else 
	{
	    h_prev = handlers;
	}
    }
    entry = Tcl_FirstHashEntry(&info->per_wins, &search);
    while (entry) 
    {
	pw = (WindowInfoStruct *) Tcl_GetHashValue(entry);
	for (handlers = pw->handlers, h_prev = NULL; 
			handlers != NULL; handlers = h_next) 
	{
	    h_next = handlers->next;
	    if (handlers->device_id == device->id) 
	    {
		if (handlers == pw->handlers) 
		{
		    pw->handlers = handlers->next;
		}
		else 
		{
		    h_prev->next = handlers->next;
		}
		handlers->next = info->frozen_handlers;
		info->frozen_handlers = handlers;
	    }
	    else 
	    {
		h_prev = handlers;
	    }
	}
	entry = Tcl_NextHashEntry(&search);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetDeviceInfo --
 *	Get the device descriptor for the device named 'name' on
 *	the given window's display. The device is opened as needed.
 *	If no such device exists on the display the function returns
 *	NULL.
 *
 *----------------------------------------------------------------------
 */
static DeviceInfoStruct *
GetDeviceInfo(Tk_Window	tkwin,	/* Used to get the display */
	      Tk_Uid	name)
{
    Display		*dpy = Tk_Display(tkwin);
    DisplayInfoStruct	*info;
    DeviceInfoStruct	*device = NULL;
    XInputClassInfo	*classes;
    int			i, dummy;

    info = GetDisplayInfo(dpy);
/*printf("display_info=0x%X, display=0x%X\n", info, dpy);*/
    for (i = 0; i < info->num_dev; i++) 
    {
	if (info->devices[i].name == name) 
	{
	    device = &info->devices[i];
	}
    }
    if (device) 
    {
	/*
	 * Open the xdevice if not already done and not currently
	 * a core device.
	 */
/*printf("device=%s(0x%X), id=%d\n", device->name, device, (int)device->id);*/
	if (!device->xdev && !device->core) 
	{
	    dummy = 0;
	    device->xdev = XOpenDevice(dpy, device->id);
	    if (device->xdev) 
	    {
		/*
		 * Process all declared input classes. This code finalizes
		 * the work done when getting the device list for the display.
		 * Some input classes are not reported until the device is open.
		 */
		for (i = 0, classes = device->xdev->classes; 
			i < device->xdev->num_classes; i++, classes++) 
		{
		    switch (classes->input_class) 
		    {
			case ButtonClass:
			    DeviceButtonRelease(device->xdev, 
				info->event_types[BUTTON_RELEASE],
				device->event_classes[BUTTON_RELEASE]);
			    DeviceButtonPress(device->xdev, 
				info->event_types[BUTTON_PRESS],
				device->event_classes[BUTTON_PRESS]);
	      		    info->event_atypes[(int)info->
				event_types[BUTTON_RELEASE]] = BUTTON_EVENT;
			    info->event_atypes[(int)info->
				event_types[BUTTON_PRESS]] = BUTTON_EVENT;
			    break;
			case KeyClass:
			    DeviceKeyRelease(device->xdev, 
				info->event_types[KEY_RELEASE],
				device->event_classes[KEY_RELEASE]);
			    DeviceKeyPress(device->xdev, 
				info->event_types[KEY_PRESS],
				device->event_classes[KEY_PRESS]);
			    info->event_atypes[(int)info->
				event_types[KEY_RELEASE]] = KEY_EVENT;
			    info->event_atypes[(int)info->
				event_types[KEY_PRESS]] = KEY_EVENT;
			    break;
			case ValuatorClass:
			    DeviceMotionNotify(device->xdev, 
				info->event_types[MOTION],
				device->event_classes[MOTION]);
			    DevicePointerMotionHint(device->xdev, dummy,
				device->event_classes[MOTION_HINT]);
			    info->event_types[MOTION_HINT] = 
				info->event_types[MOTION];
			    info->event_atypes[(int)info->
				event_types[MOTION]] = MOTION_EVENT;
			    break;
			case FocusClass:
			    device->focusable = 1;
			    DeviceFocusIn(device->xdev, 
				info->event_types[FOCUS_IN],
				device->event_classes[FOCUS_IN]);
			    DeviceFocusOut(device->xdev, 
				info->event_types[FOCUS_OUT],
				device->event_classes[FOCUS_OUT]);
			    info->event_atypes[(int)info->
				event_types[FOCUS_IN]] = FOCUS_EVENT;
			    info->event_atypes[(int)info->
				event_types[FOCUS_OUT]] = FOCUS_EVENT;
			    break;
			case ProximityClass:
			    device->proximity = 1;
			    ProximityIn(device->xdev, 
				info->event_types[PROXIMITY_IN],
				device->event_classes[PROXIMITY_IN]);
			    ProximityOut(device->xdev, 
				info->event_types[PROXIMITY_OUT],
				device->event_classes[PROXIMITY_OUT]);
			    info->event_atypes[(int)info->
				event_types[PROXIMITY_OUT]] = PROXIMITY_EVENT;
			    info->event_atypes[(int)info->
				event_types[PROXIMITY_IN]] = PROXIMITY_EVENT;
			    break;
			case FeedbackClass:
			    device->feedback = 1;
			    break;
		    }
		}
		/*
		 * If the device can report both valuators and buttons, it is
		 * possible to ask for these events. The event type is Motion.
		 */
		if (device->num_buttons && device->num_axes) 
		{
		    DeviceButtonMotion(device->xdev, dummy,
			device->event_classes[BUTTON_MOTION]);
		    info->event_types[BUTTON_MOTION] = 
			info->event_types[MOTION];
		    DeviceButton1Motion(device->xdev, dummy,
			device->event_classes[B1_MOTION]);
		    info->event_types[B1_MOTION] = 
			info->event_types[MOTION];
		    DeviceButton2Motion(device->xdev, dummy,
			device->event_classes[B2_MOTION]);
		    info->event_types[B2_MOTION] = 
			info->event_types[MOTION];
		    DeviceButton3Motion(device->xdev, dummy,
			device->event_classes[B3_MOTION]);
		    info->event_types[B3_MOTION] = 
			info->event_types[MOTION];
		    DeviceButton4Motion(device->xdev, dummy,
			device->event_classes[B4_MOTION]);
		    info->event_types[B4_MOTION] = 
			info->event_types[MOTION];
		    DeviceButton5Motion(device->xdev, dummy,
			device->event_classes[B5_MOTION]);
		    info->event_types[B5_MOTION] = 
			info->event_types[MOTION];
		}
		/*
		 * Setup the no-event class for this device.
		 */
		NoExtensionEvent(device->xdev, dummy, device->no_event_class);
		/*
		 * Setup the ChangeDevice, DeviceMapping and DeviceState
		 * classes and types.
		 */
		ChangeDeviceNotify(device->xdev, 
			info->event_types[CHANGE_DEVICE],
			device->event_classes[CHANGE_DEVICE]);
		info->event_atypes[(int)info->
			event_types[CHANGE_DEVICE]] = CHANGE_DEVICE_EVENT;
		DeviceMappingNotify(device->xdev, 
			info->event_types[DEVICE_MAPPING],
			device->event_classes[DEVICE_MAPPING]);
		info->event_atypes[(int)info->
			event_types[DEVICE_MAPPING]] = DEVICE_MAPPING_EVENT;
		DeviceStateNotify(device->xdev, 
			info->event_types[DEVICE_STATE],
			device->event_classes[DEVICE_STATE]);
		info->event_atypes[(int)info->
			event_types[DEVICE_STATE]] = DEVICE_STATE_EVENT;
		/*
		 * If the device has more than six valuators, allocate an
		 * array big enough to hold all the valuators of the device.
		 */
		if (device->num_axes) 
		{
		    device->valuator_cache = (int *) 
			ckalloc(device->num_axes*sizeof(int));
		}
		else 
		{
		    device->valuator_cache = NULL;
		}
		/*
		 * Create an event handler on ChangeDevice 
		 * to update the core field.
		 */
		Tk_CreateXiEventHandler(tkwin, 
			xi_event_names[CHANGE_DEVICE], 
			device->name,
			UpdateCoreMark, NULL);
	    }
	}
	if (!device->xdev || device->core) 
	{
	    /*
	     * Open failed or access denied, return NULL to inform the caller.
	     */
	    device = NULL;
	}
    }
/*printf("End of GetDeviceInfo\n");*/

  return device;
}

/*
 *----------------------------------------------------------------------
 *
 * GetEventIndex --
 *	Given an event spec uid, the function return the corresponding
 *	event index, needed to retrieve the type and class values.
 *	If the string does not describe a valid event, the function
 *	return -1.
 *
 *----------------------------------------------------------------------
 */
static int
GetEventIndex(Tk_Uid	event_spec)
{
    int		i;

    for (i = 0; i < NUM_XI_EVENTS; i++) 
    {
	if (xi_event_names[i] == event_spec) 
	{
	    return i;
	}
    }
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyPerWindow --
 *	Called when a window is destroyed to cleanup the event handlers
 *	that remain registered.
 *	No care is taken to prevent a race with Tk_DispatchXiEvent
 *	since DestroyPerWindow is also triggered as an event handler
 *	and thus should not be active at the same time. This is unfor-
 *	tunately a _FALSE_ guess, DestroyNotify is generated internally
 *	and processed as soon as the destroy command is emitted.
 *
 *----------------------------------------------------------------------
 */
static void
DestroyPerWindow(ClientData	client_data,
		 XEvent		*e)
{
    Tk_Window		w = (Tk_Window) client_data;
    DisplayInfoStruct	*info;
    EventScriptRecord	*escripts, *es_next;
    EventHandlerStruct	*handlers, *h_next, *h_prev;
    WindowInfoStruct	*pw;
    Tcl_HashEntry	*he;
    InProgress		*ip;
  
    if (e->type == DestroyNotify) 
    {
/*printf("Begin of DestroyPerWindow\n");*/
	info = GetDisplayInfo(e->xany.display);
	he = Tcl_FindHashEntry(&info->per_wins, (char *) w);
	if (he) 
	{
	    pw = (WindowInfoStruct *) Tcl_GetHashValue(he);
	    for (handlers = pw->handlers; handlers != NULL; handlers = h_next)
	    {
		h_next = handlers->next;
		/*
		 * Need to see if Tk_DispatchXiEvent is not about to
		 * fire this handler. If it is, just abort the whole
		 * dispatch thread, the window is gone.
		 */
		for (ip = pending_handlers; ip != NULL; ip = ip->next) 
		{
		    if (ip->next_handler == (ClientData) handlers) 
		    {
			ip->next_handler = NULL;
		    }
		}
		ckfree((char *) handlers);
	    }
	    for (escripts = pw->scripts; escripts != NULL; escripts = es_next) 
	    {
		es_next = escripts->next;
		ckfree(escripts->script);
		ckfree((char *) escripts);
	    }
	    ckfree((char *) pw);
	    Tcl_DeleteHashEntry(he);
	}
	/*
	 * Scan through the 'special events' handlers.
	 */
	for (handlers = info->other_handlers, h_prev = NULL; 
		handlers != NULL; handlers = h_next) 
	{
	    h_next = handlers->next;
	    if (handlers->tkwin == w) 
	    {
		if (handlers == info->other_handlers) 
		{
		    info->other_handlers = h_next;
		}
		else 
		{
		    h_prev->next = h_next;
		}
		/*
		 * Need to see if Tk_DispatchXiEvent is not about to
		 * fire this handler. If it is, point it to the next.
		 */
		for (ip = pending_handlers; ip != NULL; ip = ip->next) 
		{
		    if (ip->next_handler == (ClientData) handlers) 
		    {
			ip->next_handler = h_next;
		    }
		}	
		ckfree((char *) handlers);
	    }
	    else 
	    {
		h_prev = handlers;
	    }
	}
	/*
	 * Scan through the frozen handlers.
	 */
	for (handlers = info->frozen_handlers, h_prev = NULL; 
		handlers != NULL; handlers = h_next) 
	{
	    h_next = handlers->next;
	    if (handlers->tkwin == w) 
	    {
		if (handlers == info->frozen_handlers) 
		{
		    info->frozen_handlers = handlers->next;
		}
		else 
		{
		    h_prev->next = handlers->next;
		}
		ckfree((char *) handlers);
	    }
	    else 
	    {
		h_prev = handlers;
	    }
	}
/*printf("End of DestroyPerWindow\n");*/
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetWindowInfo --
 *	Return the handlers registered for the window.
 *
 *----------------------------------------------------------------------
 */
static WindowInfoStruct *
GetWindowInfo(Tk_Window	w,
	      int	create)
{
    DisplayInfoStruct	*info = GetDisplayInfo(Tk_Display(w));
    Tcl_HashEntry	*he;
    WindowInfoStruct	*pw;
    int			new;
  
/*printf("Begin of GetWindowInfo\n");*/
  
    he = Tcl_FindHashEntry(&info->per_wins, (char *) w);
    if (he == NULL) 
    {
	if (create) 
	{
	    pw = (WindowInfoStruct *) ckalloc(sizeof(WindowInfoStruct));
	    pw->handlers = NULL;
	    pw->scripts = NULL;
	    he = Tcl_CreateHashEntry(&info->per_wins, (char *) w, &new);
	    Tcl_SetHashValue(he, pw);
	    Tk_CreateEventHandler(w, StructureNotifyMask, DestroyPerWindow, w);
	}
	else 
	{
	    pw = NULL;
	}
/*printf("End of GetWindowInfo\n");*/
	return pw;
    }

/*printf("End of GetWindowInfo\n");*/
    return (WindowInfoStruct *) Tcl_GetHashValue(he);
}

/*
 *----------------------------------------------------------------------
 *
 * SelectEvents --
 *	Traverse all event handlers for a given window and collect
 *	all classes needed for correct event selection. With this,
 *	select the requested events on the window. This may also
 *	unselect as needed all events from a device.
 *
 *----------------------------------------------------------------------
 */
static void
SelectEvents(Tk_Window	w,
	     int	no_event_class)
{
    DisplayInfoStruct	*info = GetDisplayInfo(Tk_Display(w));
    WindowInfoStruct	*pw = GetWindowInfo(w, 0);
    EventHandlerStruct	*handlers;
    int			i, count = 0;
    int			*classes;
  
/*printf("Begin SelectEvents\n");*/
    if (no_event_class >= 0) 
    {
	count++;
    }
    if (pw) 
    {
	for (handlers = pw->handlers; handlers; handlers = handlers->next) 
	{
	    count += handlers->num_classes;
	}
    }
    for (handlers = info->other_handlers; handlers; handlers = handlers->next) 
    {
	if (handlers->tkwin == w) 
	{
	    count += handlers->num_classes;
	}
    }

    if (count == 0) return;

    classes = (int *) alloca(count * sizeof(int));
    count = 0;
    if (no_event_class >= 0) 
    {
	classes[count] = no_event_class;
	count++;
    }
    if (pw) 
    {
	for (handlers = pw->handlers; handlers; handlers = handlers->next) 
	{
	    for (i = 0; i < handlers->num_classes; i++, count++) 
	    {
		classes[count] = handlers->classes[i];
	    }
	}
    }
    for (handlers = info->other_handlers; handlers; handlers = handlers->next) 
    {
	if (handlers->tkwin == w) 
	{
	    for (i = 0; i < handlers->num_classes; i++, count++) 
	    {
		classes[count] = handlers->classes[i];
	    }
	}
    }

    XSelectExtensionEvent(Tk_Display(w), Tk_WindowId(w),
			(XEventClass *) classes, count);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_CreateXiEventHandler --
 *	Public function provided to declare an event handler for
 *	an event from a device on a window. The client can pass a
 *	procedure to be called and a data pointer that will be passed
 *	back to the procedure.
 *
 *	If it is not possible to bind the event the function return
 *	0 else it returns 1.
 *
 *----------------------------------------------------------------------
 */
int
Tk_CreateXiEventHandler(Tk_Window	w,
			Tk_Uid		event_spec,
			Tk_Uid		device_spec,
			Tk_EventProc	*proc,
			ClientData	client_data)
{
    int			found, no_window;
    int			event_index, event_type, event_atype;
    WindowInfoStruct	*pw;
    EventHandlerStruct	*handlers, **handlers_head;
    DeviceInfoStruct	*device;
    DisplayInfoStruct	*info;
  
/*printf("Begin Tk_CreateXiEventHandler\n");*/
  
    device = GetDeviceInfo(w, device_spec);
    info = device->dpy_info;
    event_index = GetEventIndex(event_spec);

    if (event_index < 0) return 0;

    event_type = info->event_types[event_index];
    event_atype = info->event_atypes[event_type];
    if (((event_atype == KEY_EVENT) && !device->num_keys) ||
      ((event_atype == BUTTON_EVENT) && !device->num_buttons) ||
      ((event_atype == MOTION_EVENT) && !device->num_axes) ||
      ((event_atype == FOCUS_EVENT) && !device->focusable) ||
      ((event_atype == PROXIMITY_EVENT) && !device->proximity)) 
    {
	return 0;
    }

    /*
     * Special case for events that do not report a window.
     */
    no_window = ((event_atype == CHANGE_DEVICE_EVENT) ||
	       (event_atype == DEVICE_MAPPING_EVENT) ||
	       (event_atype == DEVICE_STATE_EVENT));
  
    if (no_window) 
    {
	handlers_head = &info->other_handlers;
    }
    else 
    {
	pw = GetWindowInfo(w, 1);
	handlers_head = &pw->handlers;
    }
  
    /*
     * Lookup if the proc is already installed with the same
     * client_data for the same event and device.
     */
    found = 0;
    for (handlers = *handlers_head ; handlers; handlers = handlers->next) 
    {
	if ((handlers->proc == proc) && 
		(handlers->client_data == client_data) &&
		(handlers->type == event_type) && 
		(handlers->device_id == device->id)) 
	{
	    found = 1;
	    break;
	}
    }
  
    if (!found) 
    {
	handlers = (EventHandlerStruct *) ckalloc(sizeof(EventHandlerStruct));
	handlers->next = *handlers_head;
	*handlers_head = handlers;
	handlers->proc = proc;
	handlers->client_data = client_data;
	handlers->type = event_type;
	handlers->device_id = device->id;
	handlers->tkwin = w;
    
	if ((event_index == BUTTON_PRESS_GRAB) ||
		(event_index == BUTTON_PRESS_OWNER_GRAB)) 
	{
	    handlers->num_classes = 2;
	    handlers->classes[0] = device->event_classes[BUTTON_PRESS];
	    handlers->classes[1] = device->event_classes[BUTTON_PRESS_GRAB];
	    if (event_index == BUTTON_PRESS_OWNER_GRAB) 
	    {
		handlers->num_classes++;
		handlers->classes[2] = 
			device->event_classes[BUTTON_PRESS_OWNER_GRAB];	
	    }
	}
	else if (event_index == MOTION_HINT) 
	{
	    handlers->num_classes = 2;
	    handlers->classes[0] = device->event_classes[MOTION];
	    handlers->classes[1] = device->event_classes[MOTION_HINT];
	}
	else 
	{
	    handlers->num_classes = 1;
	    handlers->classes[0] = device->event_classes[event_index];
	}
    }

    /*
     * Now we need to check if the widget window is created and
     * if it is, select the right events.
     */
    if (Tk_WindowId(w)) 
    {
	SelectEvents(w, -1);
    }

/*printf("End Tk_CreateXiEventHandler\n");*/
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_DeleteXiEventHandler --
 *	Public function provided to suppress an event handler for
 *	an event from a device on a window. The client pass a
 *	procedure and a data pointer to be matched. Only the handler
 *	matching all criteria will be removed.
 *
 *----------------------------------------------------------------------
 */
void
Tk_DeleteXiEventHandler(Tk_Window	w,
			Tk_Uid		event_spec,
			Tk_Uid		device_spec,
			Tk_EventProc	*proc,
			ClientData	client_data)
{
    WindowInfoStruct	*pw;
    int			event_index, event_type, event_atype;
    DeviceInfoStruct	*device;
    EventHandlerStruct	*handlers, *prev, *next;
    EventHandlerStruct	**handlers_head;
    InProgress		*ip;
    int			no_window;
    int			device_in_use = 0;
  
/*printf("Begin Tk_DeleteXiEventHandler\n");*/
    device = GetDeviceInfo(w, device_spec);
    event_index = GetEventIndex(event_spec);
    event_type = device->dpy_info->event_types[event_index];
    event_atype = device->dpy_info->event_atypes[event_type];

    /*
     * Special case for events that do not report a window.
     */
    no_window = ((event_atype == CHANGE_DEVICE_EVENT) ||
	       (event_atype == DEVICE_MAPPING_EVENT) ||
	       (event_atype == DEVICE_STATE_EVENT));

    if (no_window) 
    {
	handlers_head = &device->dpy_info->other_handlers;
    }
    else 
    {
	pw = GetWindowInfo(w, 0);

	if ( !pw ) return;

	handlers_head = &pw->handlers;
    }
  
    for (handlers = *handlers_head, prev = NULL;
		 handlers != NULL; handlers = next) 
    {
	if ((handlers->proc == proc) && 
		(handlers->client_data == client_data) &&
		(handlers->type == event_type) && 
		(handlers->device_id == device->id)) 
	{
	    /* Ok, found the handler we are looking for, but we need
	     * to check if Tk_DispatchXiEvent is not about to
	     * process the handler. If it is, skip to the next.
	     */
	    next = handlers->next;

	    for (ip = pending_handlers; ip != NULL; ip = ip->next) 
	    {
		if (ip->next_handler == (ClientData) handlers) 
		{
		    ip->next_handler = (ClientData) next;
		}
	    }
	    /*
	     * Unlink and free the handler.
	     */
	    if (handlers == *handlers_head) 
	    {
		*handlers_head = next;
	    }
	    else 
	    {
		prev->next = next;
	    }
	    ckfree((char *) handlers);
	}
	else 
	{
	    prev = handlers;
	    /*
	     * Record if the device will be still in use after the
	     * removal of this handler.
	     */
	    next = handlers->next;
	    device_in_use = device_in_use || (handlers->device_id == device->id);
	}
    }
    /*
     * Now we need to check if the widget window is created and
     * if it is, select the right events.
     */
/*printf("device in use %s, %d, no_event=%d\n", device->name, device_in_use,
	 device->no_event_class);*/
    if (Tk_WindowId(w)) 
    {
	SelectEvents(w, device_in_use ? -1 : device->no_event_class);
    }
/*printf("End Tk_DeleteXiEventHandler\n");*/
}

/*
 *----------------------------------------------------------------------
 *
 * WacomxiGenericEventHandler --
 *	The generic event handler that is used to hook this extension
 *	in Tk event machinery.
 *	If checks if the event is from the xinput extension and
 *	dispatchs it using Tk_DispatchXiEvent on the appropriate
 *	window.
 *
 *----------------------------------------------------------------------
 */
static int
WacomxiGenericEventHandler(ClientData	client_data,
		      XEvent		*event)
{
    DisplayInfoStruct	*info;
  
    info = GetDisplayInfo(event->xany.display);
    if ((event->type >= info->event_base) &&
		(event->type < (info->event_base+IEVENTS))) 
    {
	return Tk_DispatchXiEvent(event);
    }

    /* Not able to dispatch return and let the standard system have a try.
     */
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_DispatchXiEvent --
 *	Public function provided to dispatch an xinput event to a
 *	window. The event must be an xinput event.
 *
 *----------------------------------------------------------------------
 */
int
Tk_DispatchXiEvent(XEvent	*e)
{
    WindowInfoStruct	*pw;
    Tk_Window		tkwin;
    InProgress		ip;
    EventHandlerStruct	*handlers;
    DisplayInfoStruct	*info;
    int			hit = 0;
  
/*printf("Begin Tk_DispatchXiEvent : %d\n", e->type);*/

    /*
     * Special case needed for ChangeDeviceEvent, DeviceMappingEvent
     * and DeviceStateEvent because the window is not reported.
     */
    if (e->xany.window) 
    {
	tkwin = Tk_IdToWindow(e->xany.display, e->xany.window);

	if ( !tkwin ) return hit;

	pw = GetWindowInfo(tkwin, 0);

	if ( !pw ) return hit;

	handlers = pw->handlers;
    }
    else 
    {
	info = GetDisplayInfo(e->xany.display);
	handlers = info->other_handlers;
    }
  
    ip.next_handler = NULL;
    ip.next = pending_handlers;
    pending_handlers = &ip;
    for ( ; handlers != NULL; ) 
    {
	if ((handlers->type == e->type) &&
		(handlers->device_id == ((XDeviceKeyEvent *) e)->deviceid)) 
	{
	    hit = 1;
	    ip.next_handler = (ClientData) handlers->next;
	    (*(handlers->proc))(handlers->client_data, e);
	    handlers = (EventHandlerStruct *) ip.next_handler;
	}
	else 
	{
	    handlers = handlers->next; 
	}
    }
    pending_handlers = pending_handlers->next;

/*printf("End Tk_DispatchXiEvent\n");*/
    return hit;
}


/*
 *----------------------------------------------------------------------
 *
 * RemoveEventScript --
 *	Delete a script associated with and event from a device. The
 *	server is asked to stop reporting this event in the window.
 *
 *----------------------------------------------------------------------
 */
static void
RemoveEventScript(Tcl_Interp	*interp,
		  Tk_Window	tkwin,
		  DeviceInfoStruct *device,
		  Tk_Uid	event_spec)
{
    WindowInfoStruct	*pw = GetWindowInfo(tkwin, 0);
    EventScriptRecord	*escripts, *prev_escript;

/*printf("Begin RemoveEventScript\n");
printf("for %s\n", event_spec);*/
    /*
     * No window info. The window has probably been destroyed.
     */
    if ( !pw ) return;

    for (escripts = pw->scripts, prev_escript = NULL; 
		escripts != NULL; 
		prev_escript = escripts, escripts = escripts->next) 
    {
	if ((escripts->device == device) &&
		(escripts->event_spec == event_spec) &&
		(escripts->interp == interp)) 
	{
	    /*
	     * Ok, found the script we are looking for.
	     * Unlink the script from the list.
	     */
	    if (escripts == pw->scripts) 
	    {
		pw->scripts = escripts->next;
	    }
	    else 
	    {
		prev_escript->next = escripts->next;
	    }
	    Tk_DeleteXiEventHandler(tkwin, event_spec, device->name,
			InvokeEventScript, (ClientData) escripts);
	    ckfree(escripts->script);
	    ckfree((char *) escripts);
	    break;
	}
    }
/*printf("End RemoveEventScript\n");*/
}

/*
 *----------------------------------------------------------------------
 *
 * ExpandPercents --
 *	Do the actual work of expanding the percents in a script. The
 *	code is an adaptation of the function of the same name in
 *	tkBind.c		To be completed
 *
 *----------------------------------------------------------------------
 */

static void
ExpandPercents(Tk_Window	tkwin,
	       DeviceInfoStruct	*device,
	       XEvent		*e,
	       char		event_atype,
	       char		*script,
	       Tcl_DString	*exp_script)
{
    char		*scan;
    char		num_buffer[NUM_SIZE];
    int			space_needed, cvt_flags;
    unsigned long	number;
    int   		length, i, x, y, width, height;
    int			k_b_v_p_flag = 0, k_b_flag = 0;
    unsigned int	device_state = 0;
    int			axes_count = 0, *axis_data = NULL;
  
/*printf("Begin ExpandPercents\n");*/
    if ((event_atype == KEY_EVENT) || (event_atype == BUTTON_EVENT)) 
    {
	k_b_flag = k_b_v_p_flag = 1;
	device_state = ((XDeviceKeyEvent *) e)->device_state;
	axes_count = ((XDeviceKeyEvent *) e)->axes_count;
	axis_data = ((XDeviceKeyEvent *) e)->axis_data;
    }
    else if (event_atype == MOTION_EVENT) 
    {
	k_b_v_p_flag = 1;
	device_state = ((XDeviceMotionEvent *) e)->device_state;
	axes_count = ((XDeviceMotionEvent *) e)->axes_count;
	axis_data = ((XDeviceMotionEvent *) e)->axis_data;
    }
    else if (event_atype == PROXIMITY_EVENT) 
    {
	k_b_v_p_flag = 1;
	device_state = ((XProximityNotifyEvent *) e)->device_state;
	axes_count = ((XProximityNotifyEvent *) e)->axes_count;
	axis_data = ((XProximityNotifyEvent *) e)->axis_data;
    }

    while (1) 
    {
	/*
	 * Find everything up to the next % character and append it
	 * to the result string.
	 */
	for (scan = script; (*scan != 0) && (*scan != '%'); scan++) 
	{
	    /* Empty loop body. */
	}
	if (scan != script) 
	{
	    Tcl_DStringAppend(exp_script, script, scan-script);
	    script = scan;
	}
	if (*script == 0) 
	{
	    break;
	}
	/*
	 * Script is on a percent sequence. Process it.
	 */
	number = 0;
	scan = "??";
	switch (script[1]) 
	{
	    case '#':
		number = e->xany.serial;
		goto do_unsigned_number;
	    case '0': 
	    case '1': 
	    case '2': 
	    case '3': 
	    case '4':
	    case '5': 
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		if (k_b_v_p_flag) 
		{
		    int	index = script[1]-'0';

		    if (index < axes_count) 
		    {
			if (axes_count > 6) 
			{
			    number = device->valuator_cache[index];
			}
			else 
			{
			    number = axis_data[index];
			}
		    }
		}
		goto do_number;
	    case  '*':
		if (event_atype == MOTION_EVENT) 
		{
		    DeviceInfoStruct	*devices = device->dpy_info->devices;
		    int			num_devices = device->dpy_info->num_dev;
	
		    /*
		     * Go through all known devices and look for a device
		     * id equal to the valuator value. If not found return
		     * the valuator value as an integer.
		     */
		    for (i = 0; i < num_devices; i++) 
		    {
			if (devices[i].id == axis_data[0]) 
			{
			    scan = (char *)devices[i].name;
			    goto do_string;
			}
		    }
		    number = axis_data[0];
		}
		goto do_number;
	    case 'b':
		if (event_atype == BUTTON_EVENT) 
		{
		    number = ((XDeviceButtonEvent *) e)->button;
		}
		goto do_unsigned_number;
	    case 'h':
		if (event_atype == MOTION_EVENT) 
		{
		    number = ((XDeviceMotionEvent *) e)->is_hint;
		}
		goto do_unsigned_number;
	    case 'k':
		if (event_atype == KEY_EVENT) 
		{
		    number = ((XDeviceKeyEvent *) e)->keycode;
		}
		goto do_unsigned_number;
	    case 's':
		if (k_b_v_p_flag) 
		{
		    number = ((XDeviceButtonEvent *) e)->state;
		}
		goto do_unsigned_number;
	    case 't':
		if (k_b_v_p_flag) 
		{
		    number = ((XDeviceButtonEvent *) e)->time;
		}
		else if (event_atype == FOCUS_EVENT) 
		{
		    number = ((XDeviceFocusChangeEvent *) e)->time;
		}
		else 
		{
		    /* The other 3 event types: 
		     * Mapping, State & Change are compatible. 
		     */
		    number = ((XChangeDeviceNotifyEvent *) e)->time;
		}
		goto do_unsigned_number;
	    case 'x':
		if (k_b_v_p_flag) 
		{
		    number = ((XDeviceButtonEvent *) e)->x;
		}
		goto do_unsigned_number;
	    case 'y':
		if (k_b_v_p_flag) 
		{
		    number = ((XDeviceButtonEvent *) e)->y;
	 	}
		goto do_unsigned_number;
	    case 'C':
		if (event_atype == CHANGE_DEVICE_EVENT) 
		{
		    XChangeDeviceNotifyEvent *cdne = (XChangeDeviceNotifyEvent *) e;
		    if (cdne->request == NewKeyboard) 
		    {
			scan = "NewKeyboard";
		    }
		    else if (cdne->request == NewPointer) 
		    {
			scan = "NewPointer";
		    }
		}
		goto do_string;
	    case 'D':
		scan = (char *)device->name;
		goto do_string;
	    case 'E':
		number = e->xany.send_event;
		goto do_unsigned_number;
	    case 'S':
		if (k_b_v_p_flag) 
		{
		    number = device_state;
		}
		goto do_unsigned_number;
	    case 'T':
		for (i = 0; i < NUM_XI_EVENTS; i++) 
		{
		    if (device->dpy_info->event_types[i] == e->type) 
		    {
			scan = (char *)xi_event_names[i];
			break;
		    }
		}
		goto do_string;
	    case 'W':
		if (tkwin != NULL) 
		{
		    scan = Tk_PathName(tkwin);
		}
		goto do_string;
	    case 'X':
		if (k_b_v_p_flag) 
		{
		    number = ((XDeviceButtonEvent *) e)->x_root;
		    if (tkwin != NULL)
		    {
			Tk_GetVRootGeometry(tkwin, &x, &y, &width, &height);
			number -= x;
		    }
		}
		goto do_unsigned_number;
	    case 'Y':
		if (k_b_v_p_flag) 
		{
		    number = ((XDeviceButtonEvent *) e)->y_root;
		    if (tkwin != NULL) 
		    {
			Tk_GetVRootGeometry(tkwin, &x, &y, &width, &height);
			number -= y;
		    }
		}
		goto do_unsigned_number;
	    default:
		num_buffer[0] = script[1];
		num_buffer[1] = '\0';
		scan = num_buffer;
		goto do_string; 
	}

	do_unsigned_number:
	    sprintf(num_buffer, "%lu", number);
	    scan = num_buffer;
	    goto do_string;
    
	do_number:
	    sprintf(num_buffer, "%ld", number);
	    scan = num_buffer;
    
	do_string:
	    space_needed = Tcl_ScanElement(scan, &cvt_flags);
	    length = Tcl_DStringLength(exp_script);
	    Tcl_DStringSetLength(exp_script, length + space_needed);
	    space_needed = Tcl_ConvertElement(scan,
			Tcl_DStringValue(exp_script) + length,
			cvt_flags | TCL_DONT_USE_BRACES);
	    Tcl_DStringSetLength(exp_script, length + space_needed);
	    script += 2;
    }
/*printf("End ExpandPercents\n");*/
}

/*
 *----------------------------------------------------------------------
 *
 * InvokeEventScript --
 *	Invoke a script that has been registered for an event form an
 *	input device. Any % occuring in the script will be replaced by
 *	a string depending on the characters following the percent.
 *	The recognized % sequences are:
 *
 *		%#	The number of the last request processed by the
 *			server.
 *		%0-9	The value of valuator n (0-9).
 *		%*	The name of the device (as encoded in a valuator
 *			by the switch virtual device).
 *		%b	The button number for events related to buttons.
 *		%h	The motion hint field from the event.
 *		%k	The keycode for events related to keys.
 *		%s	The state field from the event (the core that is).
 *		%t	The time field from the event.
 *		%x	The x coord of the core pointer.
 *		%y	The y coord of the core pointer.
 *		%C	The name of the core device that changed (only
 *			for DeviceChange events).
 *		%D	The device name that has emitted the event.
 *		%E	The send_event field from the event.
 *		%S	The device state field from the event.
 *		%T	The type field from the event, as an event name.
 *		%W	The window receiving the event.
 *		%X	The root window x coord of the core pointer.
 *		%Y	The root window y coord of the core pointer.
 *
 *	An unrecognized sequence is replaced by the sequence with
 *	the leading percent removed.
 *
 *----------------------------------------------------------------------
 */
static void
InvokeEventScript(ClientData	client_data,
		  XEvent	*e)
{
    EventScriptRecord	*escripts = (EventScriptRecord *) client_data;
    DeviceInfoStruct	*device;
    Tcl_Interp		*interp = escripts->interp;
    int			result;
    char		event_atype;
    Tcl_DString		exp_script;
    int			i, try_to_merge = 0;
    int			first_axis = 0, axes_count = 0;
    int			*axis_data = NULL;
  
    device = LookupDeviceById(((XDeviceKeyEvent *) e)->display,
			    ((XDeviceKeyEvent *) e)->deviceid);
    event_atype = device->dpy_info->event_atypes[e->type];
    /*
     * If the event is a key, button, motion or proximity event,
     * try to merge events emitted to report more than 6 valuators.
     * It is assumed that an event train can't be mixed with another
     * from the same device.
     */
    if ((event_atype == BUTTON_EVENT) || (event_atype == KEY_EVENT)) 
    {
	first_axis = ((XDeviceKeyEvent *) e)->first_axis;
	axes_count = ((XDeviceKeyEvent *) e)->axes_count;
	axis_data = ((XDeviceKeyEvent *) e)->axis_data;
	try_to_merge = 1;
    }
    else if (event_atype == MOTION_EVENT) 
    {
	first_axis = ((XDeviceMotionEvent *) e)->first_axis;
	axes_count = ((XDeviceMotionEvent *) e)->axes_count;
	axis_data = ((XDeviceMotionEvent *) e)->axis_data;
	try_to_merge = 1;
    }
    else if (event_atype == PROXIMITY_EVENT) 
    {
	first_axis = ((XProximityNotifyEvent *) e)->first_axis;
	axes_count = ((XProximityNotifyEvent *) e)->axes_count;
	axis_data = ((XProximityNotifyEvent *) e)->axis_data;
	try_to_merge = 1;
    }
  
    if (try_to_merge) 
    {
	if (axes_count > 6) 
	{
	    /*
	     * Cache the axes reported in this event.
	     */
	    for (i = first_axis; i < axes_count; i++) 
	    {
		device->valuator_cache[i] = axis_data[i-first_axis];
	    }
	    /*
	     * If this isn't the last event, wait for the others.
	     */
	    if (axes_count > (first_axis+6)) return;
	}
    }
  
    Tcl_Preserve((ClientData) interp);
    Tcl_DStringInit(&exp_script);
    ExpandPercents(escripts->tkwin, device, e, event_atype,
		 escripts->script, &exp_script);
    /*
     * Get the content of the escript before triggering
     * the script. It may destroy the binding before returning
     * and we might still need the info in case of error.
     */
    result = Tcl_GlobalEval(interp, Tcl_DStringValue(&exp_script));
    Tcl_DStringFree(&exp_script);
    if (result != TCL_OK) 
    {
	Tcl_BackgroundError(interp);
    }
    Tcl_Release((ClientData) interp);
/*printf("End InvokeEventScript\n");*/
}

/*
 *----------------------------------------------------------------------
 *
 * AddEventScript --
 *	Register a script to be invoked when an event is received in a
 *	window from a device. The server is asked to report the event
 *	on the window.
 *
 *	If it is not possible to bind the event the function return
 *	0 else it returns 1.
 *
 *----------------------------------------------------------------------
 */
static int
AddEventScript(Tcl_Interp	*interp,
	       Tk_Window	tkwin,
	       DeviceInfoStruct	*device,
	       Tk_Uid		event_spec,
	       char		*script)
{
    WindowInfoStruct  *pw = GetWindowInfo(tkwin, 1);
    EventScriptRecord *escripts = NULL;

/*printf("Begin of AddEventScript\n");*/
  
    /*
     * Try to see if a script is already registered with the
     * same combination of device, event and interp.
     */
    for (escripts = pw->scripts; escripts != NULL; escripts = escripts->next) 
    {
	if ((escripts->device == device) &&
		(escripts->event_spec == event_spec) &&
		(escripts->interp == interp)) 
	{
	    ckfree(escripts->script);
	    escripts->script = NULL;
	    break;
	}
    }
    /*
     * The event/device combo has no script already registered. Create
     * a new record and register with the server. The InvokeEventScript
     * function will be called with the script record as parameter when
     * an event of this type/device will be received on the window.
     */
    if (escripts == NULL) 
    {
	escripts = (EventScriptRecord *) ckalloc(sizeof(EventScriptRecord));
	escripts->device = device;
	escripts->tkwin = tkwin;
	escripts->event_spec = event_spec;
	escripts->interp = interp;
	if (!Tk_CreateXiEventHandler(tkwin, event_spec, device->name,
			InvokeEventScript, (ClientData) escripts)) 
	{
	    ckfree((char *) escripts);
	    return 0;
	}
	escripts->next = pw->scripts;
	pw->scripts = escripts;
    }
    /*
     * Setup the script in the record.
     */
    escripts->script = ckalloc(strlen(script) + 1);
    strcpy(escripts->script, script);

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * WacomxiBindEventCmd --
 *	Implements the 'bindevent' tcl command.
 *
 *----------------------------------------------------------------------
 */
static int
WacomxiBindEventCmd(ClientData	clientData,
	       Tcl_Interp	*interp,
	       int		argc,
	       char		**argv)
{
    Tk_Window		mainwin = (Tk_Window) clientData;
    Tk_Window		tkwin;
    DeviceInfoStruct 	*device;
    int			len;
    Tk_Uid		event_spec;
  
    if ((argc != 4) && (argc != 5)) 
    {
	Tcl_AppendResult(interp, "wrong # of arguments, should be \"",
		     argv[0], "win device event ?script?\"", (char *) NULL);
	return TCL_ERROR;
    }
    tkwin = Tk_NameToWindow(interp, argv[1], mainwin);

    if ( !tkwin ) return TCL_ERROR;

    device = GetDeviceInfo(tkwin, Tk_GetUid(argv[2]));
    if (!device) 
    {
	Tcl_AppendResult(interp, "unknown device \"", argv[2],
		     "\" or it is currently a core device", (char *) NULL);
	return TCL_ERROR;    
    }

    len = strlen(argv[3]);
    if ((argv[3][0] != '<') || (argv[3][len-1] != '>')) 
    {
	Tcl_AppendResult(interp,
		     "invalid event specification, should perhaps be <",
		     argv[3], ">", (char *) NULL);
	return TCL_ERROR;
    }
    argv[3][len-1] = 0;
    event_spec = Tk_GetUid(&argv[3][1]);
    argv[3][len-1] = '>';
  
    /*
     * Asked to return the script associated with the widget/event.
     * Lookup the scripts associated with window and in this list
     * the script associated with the given event.
     */
    if (argc == 4) 
    {
	WindowInfoStruct  *pw = GetWindowInfo(tkwin, 1);
	EventScriptRecord *escripts;
    
	for (escripts = pw->scripts; escripts != NULL; escripts = escripts->next) 
	{
	    if ((escripts->device == device) &&
			(escripts->event_spec == event_spec) &&
			(escripts->interp == interp)) 
	    {
		Tcl_SetResult(interp, (char *)escripts->script, TCL_STATIC);
		break;
	    }
	}
	return TCL_OK;
    }
    /*
     * The script is the empty string, remove the event binding.
     */
    if (!argv[4][0]) 
    {
	RemoveEventScript(interp, tkwin, device, event_spec);
	return TCL_OK;
    }
    /*
     * Add or override the event binding if the event can be
     * reported by the device. Else report an error.
     */
    if (!AddEventScript(interp, tkwin, device, event_spec, argv[4])) 
    {
	Tcl_AppendResult(interp, "Event \"", argv[3],
		     "\" can't be reported by device \"", argv[2],
		     "\"", (char *) NULL);
	return TCL_ERROR;
    }
/*printf("Added event %s \n", argv[3] );*/
  
    return TCL_OK;
}


int
Wacomxi_Init(Tcl_Interp	*interp)
{
    static int	setup_done = 0;
    int		i;
  
    if (!Tk_MainWindow(interp)) {
	Tcl_AppendResult(interp, 
			"... Xinput package need Tk to run.",
			(char *) NULL);
	return TCL_ERROR;
    }

    if (!setup_done) {
	setup_done = 1;
	Tk_CreateGenericHandler(WacomxiGenericEventHandler, NULL);
    }

    Tcl_CreateCommand(interp, 
			"wacomxi::bindevent", 
			(Tcl_CmdProc *)WacomxiBindEventCmd,
			(ClientData) Tk_MainWindow(interp),
			(Tcl_CmdDeleteProc *) NULL); 

    /*
     * Convert events names into uids for speed and ease of use.
     */
    for (i = 0; i < NUM_XI_EVENTS; i++) 
    {
	xi_event_names[i] = Tk_GetUid(xi_event_names[i]);
    }

    return Tcl_PkgProvide(interp, "LIBWACOMXI", "1.0");
}
