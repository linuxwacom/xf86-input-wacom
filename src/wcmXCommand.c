/*
 * Copyright 2007-2010 by Ping Cheng, Wacom. <pingc@wacom.com>
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
#include <wacom-properties.h>

#include "xf86Wacom.h"
#include "wcmFilter.h"
#include <exevents.h>
#include <xf86_OSproc.h>
#include <X11/Xatom.h>

#ifndef XI_PROP_DEVICE_NODE
#define XI_PROP_DEVICE_NODE "Device Node"
#endif
#ifndef XI_PROP_PRODUCT_ID
#define XI_PROP_PRODUCT_ID "Device Product ID"
#endif

static void wcmBindToSerial(WacomDevicePtr priv, unsigned int serial);
static int wcmSetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop, BOOL checkonly);
static int wcmGetProperty(DeviceIntPtr dev, Atom property);
static int wcmDeleteProperty(DeviceIntPtr dev, Atom property);

/*****************************************************************************
* wcmDevSwitchModeCall --
*****************************************************************************/

int wcmDevSwitchModeCall(WacomDevicePtr priv, int mode)
{
	DBG(3, priv, "to mode=%d\n", mode);

	/* Pad is always in absolute mode.*/
	if (IsPad(priv))
		return (mode == Absolute) ? Success : XI_BadMode;

	if ((mode == Absolute) && !is_absolute(priv))
		set_absolute(priv, TRUE);
	else if ((mode == Relative) && is_absolute(priv))
		set_absolute(priv, FALSE);
	else if ( (mode != Absolute) && (mode != Relative))
	{
		DBG(10, priv, "invalid mode=%d\n", mode);
		return XI_BadMode;
	}

	return Success;
}

/*****************************************************************************
* wcmDevSwitchMode --
*****************************************************************************/

int wcmDevSwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
	InputInfoPtr pInfo = (InputInfoPtr)dev->public.devicePrivate;
#ifdef DEBUG
	WacomDevicePtr priv = (WacomDevicePtr)pInfo->private;

	DBG(3, priv, "dev=%p mode=%d\n",
		(void *)dev, mode);
#endif
	/* Share this call with sendAButton in wcmCommon.c */
	return wcmDevSwitchModeCall(priv, mode);
}

static Atom prop_devnode;
static Atom prop_rotation;
static Atom prop_tablet_area;
static Atom prop_pressurecurve;
static Atom prop_serials;
static Atom prop_serial_binding;
static Atom prop_strip_buttons;
static Atom prop_wheel_buttons;
static Atom prop_proxout;
static Atom prop_threshold;
static Atom prop_suppress;
static Atom prop_touch;
static Atom prop_hardware_touch;
static Atom prop_gesture;
static Atom prop_gesture_param;
static Atom prop_hover;
static Atom prop_tooltype;
static Atom prop_btnactions;
static Atom prop_product_id;
static Atom prop_pressure_recal;
static Atom prop_panscroll_threshold;
#ifdef DEBUG
static Atom prop_debuglevels;
#endif

/**
 * Calculate a user-visible pressure level from a driver-internal pressure
 * level. Pressure settings exposed to the user assume a range of 0-2047
 * while the driver scales everything to a range of 0-maxCurve.
 */
static inline int wcmInternalToUserPressure(WacomDevicePtr priv, int pressure)
{
	return pressure / (priv->maxCurve / 2048);
}

/**
 * Calculate a driver-internal pressure level from a user-visible pressure
 * level. Pressure settings exposed to the user assume a range of 0-2047
 * while the driver scales everything to a range of 0-maxCurve.
 */
static inline int wcmUserToInternalPressure(WacomDevicePtr priv, int pressure)
{
	return pressure * (priv->maxCurve / 2048);
}

/**
 * Resets an arbitrary Action property, given a pointer to the old
 * handler and information about the new Action.
 */
static void wcmInitActionProp(WacomDevicePtr priv, const char *name,
			      Atom *handler, WacomAction *action)
{
	InputInfoPtr pInfo = priv->pInfo;
	Atom prop = MakeAtom(name, strlen(name), TRUE);
	size_t sz = wcmActionSize(action);

	XIChangeDeviceProperty(pInfo->dev, prop, XA_INTEGER, 32,
			       PropModeReplace, sz, (char*)wcmActionData(action), FALSE);
	*handler = prop;
}

static void wcmInitButtonActionProp(WacomDevicePtr priv, int button)
{
	char name[64];

	sprintf(name, "Wacom button action %d", button);
	wcmInitActionProp(priv, name, &priv->btn_action_props[button], &priv->key_actions[button]);
}

static void wcmInitStripActionProp(WacomDevicePtr priv, int index)
{
	char name[64];

	sprintf(name, "Wacom strip action %d", index);
	wcmInitActionProp(priv, name, &priv->strip_action_props[index], &priv->strip_actions[index]);
}

static void wcmInitWheelActionProp(WacomDevicePtr priv, int index)
{
	char name[64];

	sprintf(name, "Wacom wheel action %d", index);
	wcmInitActionProp(priv, name, &priv->wheel_action_props[index], &priv->wheel_actions[index]);
}

/**
 * Registers a property for the input device. This function registers
 * the property name atom, as well as creates the property itself.
 * At creation, the property values are initialized from the 'values'
 * array. The device property is marked as non-deletable.
 * Initialization values are always to be provided by means of an
 * array of 32 bit integers, regardless of 'format'
 *
 * @param dev Pointer to device structure
 * @param name Name of device property
 * @param type Type of the property
 * @param format Format of the property (8/16/32)
 * @param nvalues Number of values in the property
 * @param values Pointer to 32 bit integer array of initial property values
 * @return Atom handle of property name
 */
static Atom InitWcmAtom(DeviceIntPtr dev, const char *name, Atom type, int format, int nvalues, int *values)
{
	int i;
	Atom atom;
	uint8_t val_8[WCM_MAX_BUTTONS];
	uint16_t val_16[WCM_MAX_BUTTONS];
	uint32_t val_32[WCM_MAX_BUTTONS];
	pointer converted = val_32;

	for (i = 0; i < nvalues; i++)
	{
		switch(format)
		{
			case 8:  val_8[i]  = values[i]; break;
			case 16: val_16[i] = values[i]; break;
			case 32: val_32[i] = values[i]; break;
		}
	}

	switch(format)
	{
		case 8: converted = val_8; break;
		case 16: converted = val_16; break;
		case 32: converted = val_32; break;
	}

	atom = MakeAtom(name, strlen(name), TRUE);
	XIChangeDeviceProperty(dev, atom, type, format,
			PropModeReplace, nvalues,
			converted, FALSE);
	XISetDevicePropertyDeletable(dev, atom, FALSE);
	return atom;
}

void InitWcmDeviceProperties(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->pInfo;
	WacomCommonPtr common = priv->common;
	int values[WCM_MAX_BUTTONS];
	int i;

	DBG(10, priv, "\n");

	prop_devnode = MakeAtom(XI_PROP_DEVICE_NODE, strlen(XI_PROP_DEVICE_NODE), TRUE);
	XIChangeDeviceProperty(pInfo->dev, prop_devnode, XA_STRING, 8,
				PropModeReplace, strlen(common->device_path),
				common->device_path, FALSE);
	XISetDevicePropertyDeletable(pInfo->dev, prop_devnode, FALSE);

	if (!IsPad(priv)) {
		values[0] = priv->topX;
		values[1] = priv->topY;
		values[2] = priv->bottomX;
		values[3] = priv->bottomY;
		prop_tablet_area = InitWcmAtom(pInfo->dev, WACOM_PROP_TABLET_AREA, XA_INTEGER, 32, 4, values);
	}

	values[0] = common->wcmRotate;
	if (!IsPad(priv)) {
		prop_rotation = InitWcmAtom(pInfo->dev, WACOM_PROP_ROTATION, XA_INTEGER, 8, 1, values);
	}

	if (IsPen(priv) || IsTouch(priv)) {
		values[0] = priv->nPressCtrl[0];
		values[1] = priv->nPressCtrl[1];
		values[2] = priv->nPressCtrl[2];
		values[3] = priv->nPressCtrl[3];
		prop_pressurecurve = InitWcmAtom(pInfo->dev, WACOM_PROP_PRESSURECURVE, XA_INTEGER, 32, 4, values);
	}

	values[0] = common->tablet_id;
	values[1] = priv->oldState.serial_num;
	values[2] = priv->oldState.device_id;
	values[3] = priv->cur_serial;
	values[4] = priv->cur_device_id;
	prop_serials = InitWcmAtom(pInfo->dev, WACOM_PROP_SERIALIDS, XA_INTEGER, 32, 5, values);

	values[0] = priv->serial;
	prop_serial_binding = InitWcmAtom(pInfo->dev, WACOM_PROP_SERIAL_BIND, XA_INTEGER, 32, 1, values);

	if (IsTablet(priv)) {
		values[0] = priv->wcmProxoutDist;
		prop_proxout = InitWcmAtom(pInfo->dev, WACOM_PROP_PROXIMITY_THRESHOLD, XA_INTEGER, 32, 1, values);
	}

	values[0] = (!common->wcmMaxZ) ? 0 : common->wcmThreshold;
	values[0] = wcmInternalToUserPressure(priv, values[0]);
	prop_threshold = InitWcmAtom(pInfo->dev, WACOM_PROP_PRESSURE_THRESHOLD, XA_INTEGER, 32, 1, values);

	values[0] = common->wcmSuppress;
	values[1] = common->wcmRawSample;
	prop_suppress = InitWcmAtom(pInfo->dev, WACOM_PROP_SAMPLE, XA_INTEGER, 32, 2, values);

	values[0] = common->wcmTouch;
	prop_touch = InitWcmAtom(pInfo->dev, WACOM_PROP_TOUCH, XA_INTEGER, 8, 1, values);

	if (common->wcmHasHWTouchSwitch && IsTouch(priv)) {
		values[0] = common->wcmHWTouchSwitchState;
		prop_hardware_touch = InitWcmAtom(pInfo->dev, WACOM_PROP_HARDWARE_TOUCH, XA_INTEGER, 8, 1, values);
	}

	if (IsStylus(priv)) {
		values[0] = !common->wcmTPCButton;
		prop_hover = InitWcmAtom(pInfo->dev, WACOM_PROP_HOVER, XA_INTEGER, 8, 1, values);
	}

	values[0] = common->wcmGesture;
	prop_gesture = InitWcmAtom(pInfo->dev, WACOM_PROP_ENABLE_GESTURE, XA_INTEGER, 8, 1, values);

	values[0] = common->wcmGestureParameters.wcmZoomDistance;
	values[1] = common->wcmGestureParameters.wcmScrollDistance;
	values[2] = common->wcmGestureParameters.wcmTapTime;
	prop_gesture_param = InitWcmAtom(pInfo->dev, WACOM_PROP_GESTURE_PARAMETERS, XA_INTEGER, 32, 3, values);

	values[0] = MakeAtom(pInfo->type_name, strlen(pInfo->type_name), TRUE);
	prop_tooltype = InitWcmAtom(pInfo->dev, WACOM_PROP_TOOL_TYPE, XA_ATOM, 32, 1, values);

	for (i = 0; i < priv->nbuttons; i++)
		wcmInitButtonActionProp(priv, i);
	prop_btnactions = InitWcmAtom(pInfo->dev, WACOM_PROP_BUTTON_ACTIONS, XA_ATOM, 32,
				      priv->nbuttons, (int*)priv->btn_action_props);

	if (IsPad(priv)) {
		for (i = 0; i < 4; i++)
			wcmInitStripActionProp(priv, i);
		prop_strip_buttons = InitWcmAtom(pInfo->dev, WACOM_PROP_STRIPBUTTONS, XA_ATOM, 32,
						 4, (int*)priv->strip_action_props);
	}

	if (IsPad(priv) || IsCursor(priv))
	{
		for (i = 0; i < 6; i++)
			wcmInitWheelActionProp(priv, i);
		prop_wheel_buttons = InitWcmAtom(pInfo->dev, WACOM_PROP_WHEELBUTTONS, XA_ATOM, 32,
						 6, (int*)priv->wheel_action_props);
	}

	if (IsStylus(priv) || IsEraser(priv)) {
		values[0] = common->wcmPressureRecalibration;
		prop_pressure_recal = InitWcmAtom(pInfo->dev,
						  WACOM_PROP_PRESSURE_RECAL,
						  XA_INTEGER, 8, 1, values);
	}

	values[0] = common->wcmPanscrollThreshold;
	prop_panscroll_threshold = InitWcmAtom(pInfo->dev, WACOM_PROP_PANSCROLL_THRESHOLD, XA_INTEGER, 32, 1, values);

	values[0] = common->vendor_id;
	values[1] = common->tablet_id;
	prop_product_id = InitWcmAtom(pInfo->dev, XI_PROP_PRODUCT_ID, XA_INTEGER, 32, 2, values);

#ifdef DEBUG
	values[0] = priv->debugLevel;
	values[1] = common->debugLevel;
	prop_debuglevels = InitWcmAtom(pInfo->dev, WACOM_PROP_DEBUGLEVELS, XA_INTEGER, 8, 2, values);
#endif

	XIRegisterPropertyHandler(pInfo->dev, wcmSetProperty, wcmGetProperty, wcmDeleteProperty);
}

/* Returns the offset of the property in the list given. If the property is
 * not found, a negative error code is returned. */
static int wcmFindProp(Atom property, Atom *prop_list, int nprops)
{
	int i;

	/* check all properties used for button actions */
	for (i = 0; i < nprops; i++)
		if (prop_list[i] == property)
			return i;

	return -BadAtom;
}

/**
 * Obtain a pointer to the the handler and action list for a given Action
 * property. This function searches the button, wheel, and strip property
 * handler lists.
 *
 * @param priv          The device whose handler lists should be searched
 * @param property      The Action property that should be searched for
 * @param[out] handler  Returns a pointer to the property's handler
 * @param[out] action   Returns a pointer to the property's action list
 * @return              'true' if the property was found. Neither out parameter
 *                      will be null if this is the case.
 */
static BOOL wcmFindActionHandler(WacomDevicePtr priv, Atom property, Atom **handler,
				 WacomAction **action)
{
	int offset;

	offset = wcmFindProp(property, priv->btn_action_props, ARRAY_SIZE(priv->btn_action_props));
	if (offset >=0)
	{
		*handler = &priv->btn_action_props[offset];
		*action  = &priv->key_actions[offset];
		return TRUE;
	}

	offset = wcmFindProp(property, priv->wheel_action_props, ARRAY_SIZE(priv->wheel_action_props));
	if (offset >= 0)
	{
		*handler = &priv->wheel_action_props[offset];
		*action  = &priv->wheel_actions[offset];
		return TRUE;
	}

	offset = wcmFindProp(property, priv->strip_action_props, ARRAY_SIZE(priv->strip_action_props));
	if (offset >= 0)
	{
		*handler = &priv->strip_action_props[offset];
		*action  = &priv->strip_actions[offset];
		return TRUE;
	}

	return FALSE;
}

static int wcmCheckActionProperty(WacomDevicePtr priv, Atom property, XIPropertyValuePtr prop)
{
	CARD32 *data;
	int j;

	if (!property) {
		DBG(3, priv, "ERROR: Atom is NONE\n");
		return BadMatch;
	}

	if (prop == NULL) {
		DBG(3, priv, "ERROR: Value is NULL\n");
		return BadMatch;
	}

	if (prop->size >= 255) {
		DBG(3, priv, "ERROR: Too many values (%ld > 255)\n", prop->size);
		return BadMatch;
	}

	if (prop->format != 32) {
		DBG(3, priv, "ERROR: Incorrect value format (%d != 32)\n", prop->format);
		return BadMatch;
	}

	if (prop->type != XA_INTEGER) {
		DBG(3, priv, "ERROR: Incorrect value type (%d != XA_INTEGER)\n", prop->type);
		return BadMatch;
	}

	data = (CARD32*)prop->data;

	for (j = 0; j < prop->size; j++)
	{
		int code = data[j] & AC_CODE;
		int type = data[j] & AC_TYPE;

		DBG(10, priv, "Index %d == %d (type: %d, code: %d)\n", j, data[j], type, code);

		switch(type)
		{
			case AC_KEY:
				break;
			case AC_BUTTON:
				if (code > WCM_MAX_X11BUTTON) {
					DBG(3, priv, "ERROR: AC_BUTTON code too high (%d > %d)\n", code, WCM_MAX_X11BUTTON);
					return BadValue;
				}
				break;
			case AC_MODETOGGLE:
				break;
			case AC_PANSCROLL:
				break;
			default:
				DBG(3, priv, "ERROR: Unknown command\n");
				return BadValue;
		}
	}

	return Success;
}

/**
 * An 'Action' property (such as an element of "Wacom Button Actions")
 * defines an action to be performed for some event. The property is
 * validated, and then saved for later use. Both the property itself
 * (as 'handler') and the data it references (as 'action') are saved.
 *
 * @param dev        The device being modified
 * @param property   The Action property being set
 * @param prop       The data contained in 'property'
 * @param checkonly  'true' if the property should only be checked for validity
 * @param handler    Pointer to the handler that must be updated
 * @param action     Pointer to the action list that must be updated
 */
static int wcmSetActionProperty(DeviceIntPtr dev, Atom property,
				XIPropertyValuePtr prop, BOOL checkonly,
				Atom *handler, WacomAction *action)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int rc, i;

	DBG(5, priv, "%s new actions for Atom %d\n", checkonly ? "Checking" : "Setting", property);

	rc = wcmCheckActionProperty(priv, property, prop);
	if (rc != Success) {
		const char *msg = NULL;
		switch (rc) {
			case BadMatch: msg = "BadMatch"; break;
			case BadValue: msg = "BadValue"; break;
			default: msg = "UNKNOWN"; break;
		}
		DBG(3, priv, "Action validation failed with code %d (%s)\n", rc, msg);
		return rc;
	}

	if (!checkonly)
	{
		WacomAction act = {};

		for (i = 0; i < prop->size; i++)
			wcmActionSet(&act, i, ((unsigned int*)prop->data)[i]);

		wcmActionCopy(action, &act);
		*handler = property;
	}

	return Success;
}

static int wcmCheckActionsProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	XIPropertyValuePtr val;
	Atom *values = (Atom*)prop->data;
	int i;

	if (prop->format != 32 || prop->type != XA_ATOM)
		return BadMatch;

	for (i = 0; i < prop->size; i++)
	{
		if (!values[i])
			continue;

		if (values[i] == property || !ValidAtom(values[i]))
			return BadValue;

		if (XIGetDeviceProperty(pInfo->dev, values[i], &val) != Success)
			return BadValue;
	}

	return Success;
}

/**
 * An 'Actions' property (such as "Wacom Button Actions") stores a list of
 * 'Action' properties that define an action to be performed. This function
 * goes through the list of actions and saves each one.
 *
 * @param dev        The device being modified
 * @param property   The Actions property being set
 * @param prop       The data contained in 'property'
 * @param checkonly  'true' if the property should only be checked for validity
 * @param size       Expected number of elements in 'prop'
 * @param handlers   List of handlers that must be updated
 * @param actions    List of actions that must be updated
 */
static int wcmSetActionsProperty(DeviceIntPtr dev, Atom property,
                                 XIPropertyValuePtr prop, BOOL checkonly,
                                 int size, Atom* handlers, WacomAction *actions)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int rc, i;

	DBG(10, priv, "\n");

	if (prop->size != size)
		return BadValue;

	rc = wcmCheckActionsProperty(dev, property, prop);
	if (rc != Success)
		return rc;

	for (i = 0; i < prop->size; i++)
	{
		int index = i;
		Atom subproperty = ((Atom*)prop->data)[i];
		XIPropertyValuePtr subprop;

		if (property == prop_btnactions)
		{ /* Driver uses physical -- not X11 -- button numbering internally */
			if (i < 3)
				index = i;
			else if (i < 7)
				continue;
			else
				index = i - 4;
		}

		if (subproperty == 0)
		{ /* Interpret 'None' as meaning 'reset' */
			if (!checkonly)
			{
				if (property == prop_btnactions)
				{
					wcmResetButtonAction(priv, index);
					wcmInitButtonActionProp(priv, index);
				} else if (property == prop_strip_buttons)
				{
					wcmResetStripAction(priv, index);
					wcmInitStripActionProp(priv, index);
				} else if (property == prop_wheel_buttons) {
					wcmResetWheelAction(priv, index);
					wcmInitWheelActionProp(priv, index);
				}

				if (subproperty != handlers[index])
					subproperty = handlers[index];
			}
		}
		else
		{
			XIGetDeviceProperty(dev, subproperty, &subprop);
			rc = wcmSetActionProperty(dev, subproperty, subprop, checkonly, &handlers[index], &actions[index]);
			if (rc != Success)
				return rc;
		}
	}

	return Success;
}

/**
 * Update the rotation property for all tools on the same physical tablet as
 * pInfo.
 */
void wcmUpdateRotationProperty(WacomDevicePtr priv)
{
	WacomCommonPtr common = priv->common;
	WacomDevicePtr other;
	char rotation = common->wcmRotate;

	for (other = common->wcmDevices; other; other = other->next)
	{
		InputInfoPtr pInfo;
		DeviceIntPtr dev;

		if (other == priv)
			continue;

		pInfo = other->pInfo;
		dev = pInfo->dev;

		XIChangeDeviceProperty(dev, prop_rotation, XA_INTEGER, 8,
				       PropModeReplace, 1, &rotation,
				       TRUE);
	}
}

static void
wcmSetHWTouchProperty(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->pInfo;
	WacomCommonPtr common = priv->common;
	XIPropertyValuePtr prop;
	CARD8 prop_value;
	int rc;

	rc = XIGetDeviceProperty(pInfo->dev, prop_hardware_touch, &prop);
	if (rc != Success || prop->format != 8 || prop->size != 1)
	{
		xf86IDrvMsg(pInfo, X_ERROR, "Failed to update hardware touch state.\n");
		return;
	}

	prop_value = common->wcmHWTouchSwitchState;
	XIChangeDeviceProperty(pInfo->dev, prop_hardware_touch, XA_INTEGER,
			       prop->format, PropModeReplace,
			       prop->size, &prop_value, TRUE);
}

static CARD32
touchTimerFunc(OsTimerPtr timer, CARD32 now, pointer arg)
{
	WacomDevicePtr priv = arg;
#if !HAVE_THREADED_INPUT
	int sigstate = xf86BlockSIGIO();
#endif

	wcmSetHWTouchProperty(priv);

#if !HAVE_THREADED_INPUT
	xf86UnblockSIGIO(sigstate);
#endif

	return 0;
}

/**
 * Update HW touch property when its state is changed by touch switch
 */
void
wcmUpdateHWTouchProperty(WacomDevicePtr priv)
{
	/* This function is called during SIGIO/InputThread. Schedule timer
	 * for property event delivery by the main thread. */
	priv->touch_timer = TimerSet(priv->touch_timer, 0 /* reltime */,
				      1, touchTimerFunc, priv);
}

/**
 * Only allow deletion of a property if it is not being used by any of the
 * button actions.
 */
static int wcmDeleteProperty(DeviceIntPtr dev, Atom property)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	int i;

	i = wcmFindProp(property, priv->btn_action_props, ARRAY_SIZE(priv->btn_action_props));
	if (i < 0)
		i = wcmFindProp(property, priv->wheel_action_props,
				ARRAY_SIZE(priv->wheel_action_props));
	if (i < 0)
		i = wcmFindProp(property, priv->strip_action_props,
				ARRAY_SIZE(priv->strip_action_props));

	return (i >= 0) ? BadAccess : Success;
}

static int wcmSetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop,
			  BOOL checkonly)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomCommonPtr common = priv->common;

	DBG(10, priv, "\n");

	if (property == prop_devnode || property == prop_product_id)
		return BadValue; /* Read-only */
	else if (property == prop_tablet_area)
	{
		INT32 *values = (INT32*)prop->data;

		if (prop->size != 4 || prop->format != 32)
			return BadValue;

		if (!checkonly)
		{
			if ((values[0] == -1) && (values[1] == -1) &&
					(values[2] == -1) && (values[3] == -1))
			{
				values[0] = priv->minX;
				values[1] = priv->minX;
				values[2] = priv->maxX;
				values[3] = priv->maxY;
			}

			priv->topX = values[0];
			priv->topY = values[1];
			priv->bottomX = values[2];
			priv->bottomY = values[3];
		}
	} else if (property == prop_pressurecurve)
	{
		INT32 *pcurve;

		if (prop->size != 4 || prop->format != 32)
			return BadValue;

		pcurve = (INT32*)prop->data;

		if (!wcmCheckPressureCurveValues(pcurve[0], pcurve[1],
						 pcurve[2], pcurve[3]))
			return BadValue;

		if (IsCursor(priv) || IsPad (priv))
			return BadValue;

		if (!checkonly)
			wcmSetPressureCurve (priv, pcurve[0], pcurve[1],
					pcurve[2], pcurve[3]);
	} else if (property == prop_suppress)
	{
		CARD32 *values;

		if (prop->size != 2 || prop->format != 32)
			return BadValue;

		values = (CARD32*)prop->data;

		if (values[0] > 100)
			return BadValue;

		if ((values[1] < 1) || (values[1] > MAX_SAMPLES))
			return BadValue;

		if (!checkonly)
		{
			common->wcmSuppress = values[0];
			common->wcmRawSample = values[1];
		}
	} else if (property == prop_rotation)
	{
		CARD8 value;
		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		value = *(CARD8*)prop->data;

		if (value > 3)
			return BadValue;

		if (!checkonly && common->wcmRotate != value)
			wcmRotateTablet(priv, value);

	} else if (property == prop_serials)
	{
		/* This property is read-only but we need to
		 * set it at runtime. If we get here from wcmUpdateSerial,
		 * we know the serial has ben set internally already, so we
		 * can reply with success. */
		if (prop->size == 5 && prop->format == 32)
			if (((CARD32*)prop->data)[3] == priv->cur_serial)
				return Success;

		return BadValue; /* Read-only */
	} else if (property == prop_serial_binding)
	{
		unsigned int serial;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		if (!checkonly)
		{
			serial = *(CARD32*)prop->data;
			wcmBindToSerial(priv, serial);
		}
	} else if (property == prop_strip_buttons)
		return wcmSetActionsProperty(dev, property, prop, checkonly, ARRAY_SIZE(priv->strip_action_props), priv->strip_action_props, priv->strip_actions);
	else if (property == prop_wheel_buttons)
		return wcmSetActionsProperty(dev, property, prop, checkonly, ARRAY_SIZE(priv->wheel_action_props), priv->wheel_action_props, priv->wheel_actions);
	else if (property == prop_proxout)
	{
		CARD32 value;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		if (!IsTablet (priv))
			return BadValue;

		value = *(CARD32*)prop->data;

		if (value > common->wcmMaxDist)
			return BadValue;

		if (!checkonly)
			priv->wcmProxoutDist = value;
	} else if (property == prop_threshold)
	{
		const INT32 MAXIMUM = wcmInternalToUserPressure(priv, priv->maxCurve);
		INT32 value;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		value = *(INT32*)prop->data;

		if (value == -1)
			value = priv->maxCurve * DEFAULT_THRESHOLD;
		else if ((value < 1) || (value > MAXIMUM))
			return BadValue;
		else
			value = wcmUserToInternalPressure(priv, value);

		if (!checkonly)
			common->wcmThreshold = value;
	} else if (property == prop_touch)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!checkonly && common->wcmTouch != values[0])
			common->wcmTouch = values[0];
	} else if (property == prop_hardware_touch)
	{
		if (common->wcmHasHWTouchSwitch)
		{
			/* If we get here from wcmUpdateHWTouchProperty, we know
			 * the wcmHWTouchSwitchState has been set internally
			 * already, so we can reply with success. */
			if (prop->size == 1 && prop->format == 8)
				if (((CARD8*)prop->data)[0] == common->wcmHWTouchSwitchState)
					return Success;
		}

		return BadValue; /* read-only */
	} else if (property == prop_gesture)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!checkonly && common->wcmGesture != values[0])
			common->wcmGesture = values[0];
	} else if (property == prop_gesture_param)
	{
		CARD32 *values;

		if (prop->size != 3 || prop->format != 32)
			return BadValue;

		values = (CARD32*)prop->data;

		if (!checkonly)
		{
			if (common->wcmGestureParameters.wcmZoomDistance != values[0])
				common->wcmGestureParameters.wcmZoomDistance = values[0];
			if (common->wcmGestureParameters.wcmScrollDistance != values[1])
				common->wcmGestureParameters.wcmScrollDistance = values[1];
			if (common->wcmGestureParameters.wcmTapTime != values[2])
				common->wcmGestureParameters.wcmTapTime = values[2];
		}
	} else if (property == prop_hover)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!IsStylus(priv))
			return BadMatch;

		if (!checkonly)
			common->wcmTPCButton = !values[0];
#ifdef DEBUG
	} else if (property == prop_debuglevels)
	{
		CARD8 *values;

		if (prop->size != 2 || prop->format != 8)
			return BadMatch;

		values = (CARD8*)prop->data;
		if (values[0] > 12 || values[1] > 12)
			return BadValue;

		if (!checkonly)
		{
			priv->debugLevel = values[0];
			common->debugLevel = values[1];
		}
#endif
	} else if (property == prop_btnactions)
	{
		int nbuttons = priv->nbuttons < 4 ? priv->nbuttons : priv->nbuttons + 4;
		return wcmSetActionsProperty(dev, property, prop, checkonly, nbuttons, priv->btn_action_props, priv->key_actions);
	} else if (property == prop_pressure_recal)
	{
		CARD8 *values = (CARD8*)prop->data;

		if (prop->size != 1 || prop->format != 8)
			return BadValue;

		if ((values[0] != 0) && (values[0] != 1))
			return BadValue;

		if (!IsStylus(priv) && !IsEraser(priv))
			return BadMatch;

		if (!checkonly)
			common->wcmPressureRecalibration = values[0];
	} else if (property == prop_panscroll_threshold)
	{
		CARD32 *values = (CARD32*)prop->data;

		if (prop->size != 1 || prop->format != 32)
			return BadValue;

		if (values[0] <= 0)
			return BadValue;

		if (IsTouch(priv))
			return BadMatch;

		if (!checkonly)
			common->wcmPanscrollThreshold = values[0];
	} else
	{
		Atom *handler = NULL;
		WacomAction *action = NULL;
		if (wcmFindActionHandler(priv, property, &handler, &action))
			return wcmSetActionProperty(dev, property, prop, checkonly, handler, action);
		/* backwards-compatible behavior silently ignores the not-found case */
	}

	return Success;
}

static int wcmGetProperty (DeviceIntPtr dev, Atom property)
{
	InputInfoPtr pInfo = (InputInfoPtr) dev->public.devicePrivate;
	WacomDevicePtr priv = (WacomDevicePtr) pInfo->private;
	WacomCommonPtr common = priv->common;

	DBG(10, priv, "\n");

	if (property == prop_serials)
	{
		uint32_t values[5];

		values[0] = common->tablet_id;
		values[1] = priv->oldState.serial_num;
		values[2] = priv->oldState.device_id;
		values[3] = priv->cur_serial;
		values[4] = priv->cur_device_id;

		DBG(10, priv, "Update to serial: %d\n", priv->oldState.serial_num);

		return XIChangeDeviceProperty(dev, property, XA_INTEGER, 32,
					      PropModeReplace, 5,
					      values, FALSE);
	}
	else if (property == prop_btnactions)
	{
		/* Convert the physical button representation used internally
		 * to the X11 button representation we've historically used.
		 * To do this, we need to skip X11 buttons 4-7 which would be
		 * used by a scroll wheel rather than an actual button.
		 */
		int nbuttons = priv->nbuttons < 4 ? priv->nbuttons : priv->nbuttons + 4;
		Atom x11_btn_action_props[nbuttons];
		int i;

		for (i = 0; i < nbuttons; i++)
		{
			if (i < 3)
				x11_btn_action_props[i] = priv->btn_action_props[i];
			else if (i < 7)
				x11_btn_action_props[i] = 0;
			else
				x11_btn_action_props[i] = priv->btn_action_props[i-4];
		}

		return XIChangeDeviceProperty(dev, property, XA_ATOM, 32,
		                              PropModeReplace, nbuttons,
		                              x11_btn_action_props, FALSE);
	}
	else if (property == prop_strip_buttons)
	{
		return XIChangeDeviceProperty(dev, property, XA_ATOM, 32,
					      PropModeReplace, ARRAY_SIZE(priv->strip_action_props),
					      priv->strip_action_props, FALSE);
	}
	else if (property == prop_wheel_buttons)
	{
		return XIChangeDeviceProperty(dev, property, XA_ATOM, 32,
		                              PropModeReplace, ARRAY_SIZE(priv->wheel_action_props),
		                              priv->wheel_action_props, FALSE);
	}

	return Success;
}

static void
wcmSetSerialProperty(WacomDevicePtr priv)
{
	InputInfoPtr pInfo = priv->pInfo;
	XIPropertyValuePtr prop;
	CARD32 prop_value[5];
	int rc;

	rc = XIGetDeviceProperty(pInfo->dev, prop_serials, &prop);
	if (rc != Success || prop->format != 32 || prop->size != 5)
	{
		xf86IDrvMsg(pInfo, X_ERROR, "Failed to update serial number.\n");
		return;
	}

	memcpy(prop_value, prop->data, sizeof(prop_value));
	prop_value[3] = priv->cur_serial;
	prop_value[4] = priv->cur_device_id;

	XIChangeDeviceProperty(pInfo->dev, prop_serials, XA_INTEGER,
			       prop->format, PropModeReplace,
			       prop->size, prop_value, TRUE);
}

static CARD32
serialTimerFunc(OsTimerPtr timer, CARD32 now, pointer arg)
{
	WacomDevicePtr priv = arg;

#if !HAVE_THREADED_INPUT
	int sigstate = xf86BlockSIGIO();
#endif

	wcmSetSerialProperty(priv);

#if !HAVE_THREADED_INPUT
	xf86UnblockSIGIO(sigstate);
#endif

	return 0;
}

void
wcmUpdateSerialProperty(WacomDevicePtr priv)
{
	/* This function is called during SIGIO/InputThread. Schedule timer
	 * for property event delivery by the main thread. */
	priv->serial_timer = TimerSet(priv->serial_timer, 0 /* reltime */,
				      1, serialTimerFunc, priv);
}

static void
wcmBindToSerial(WacomDevicePtr priv, unsigned int serial)
{
	priv->serial = serial;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
