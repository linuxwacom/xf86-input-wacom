/*****************************************************************************
** wactablet.h
**
** Copyright (C) 2002,2003 - John E. Joganic
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
**
****************************************************************************/

#ifndef __LINUXWACOM_WACTABLET_H
#define __LINUXWACOM_WACTABLET_H

#define WACOMVENDOR_UNKNOWN     0x0000
#define WACOMVENDOR_WACOM       0x056A
#define WACOMVENDOR_ACER        0xFFFFFF01

#define WACOMCLASS_SERIAL       0x0001
#define WACOMCLASS_USB          0x0002

#define WACOMDEVICE_UNKNOWN     0x0000
#define WACOMDEVICE_ARTPAD      0x0001
#define WACOMDEVICE_ARTPADII    0x0002
#define WACOMDEVICE_DIGITIZER   0x0003
#define WACOMDEVICE_DIGITIZERII 0x0004
#define WACOMDEVICE_PENPARTNER  0x0005
#define WACOMDEVICE_GRAPHIRE    0x0006
#define WACOMDEVICE_GRAPHIRE2   0x0007
#define WACOMDEVICE_INTUOS      0x0008
#define WACOMDEVICE_INTUOS2     0x0009
#define WACOMDEVICE_CINTIQ      0x000A
#define WACOMDEVICE_VOLITO      0x000B
#define WACOMDEVICE_ACERC100    0x000C

typedef struct _WACOMMODEL WACOMMODEL;
struct _WACOMMODEL
{
	unsigned int uClass;
	unsigned int uVendor;
	unsigned int uDevice;
	unsigned int uSubType;
};

#define WACOMTOOLTYPE_NONE      0x00
#define WACOMTOOLTYPE_PEN       0x01
#define WACOMTOOLTYPE_PENCIL    0x02
#define WACOMTOOLTYPE_BRUSH     0x03
#define WACOMTOOLTYPE_ERASER    0x04
#define WACOMTOOLTYPE_AIRBRUSH  0x05
#define WACOMTOOLTYPE_MOUSE     0x06
#define WACOMTOOLTYPE_LENS      0x07
#define WACOMTOOLTYPE_MAX       0x08

#define WACOMBUTTON_LEFT        0
#define WACOMBUTTON_MIDDLE      1
#define WACOMBUTTON_RIGHT       2
#define WACOMBUTTON_EXTRA       3
#define WACOMBUTTON_SIDE        4
#define WACOMBUTTON_TOUCH       5
#define WACOMBUTTON_STYLUS      6
#define WACOMBUTTON_STYLUS2     7
#define WACOMBUTTON_MAX         8

#define WACOMFIELD_TOOLTYPE     0
#define WACOMFIELD_SERIAL       1
#define WACOMFIELD_PROXIMITY    2
#define WACOMFIELD_BUTTONS      3
#define WACOMFIELD_POSITION_X   4
#define WACOMFIELD_POSITION_Y   5
#define WACOMFIELD_ROTATION_Z   6
#define WACOMFIELD_DISTANCE	    7
#define WACOMFIELD_PRESSURE	    8
#define WACOMFIELD_TILT_X       9
#define WACOMFIELD_TILT_Y       10
#define WACOMFIELD_ABSWHEEL     11
#define WACOMFIELD_RELWHEEL     12
#define WACOMFIELD_THROTTLE     13
#define WACOMFIELD_MAX          14

typedef struct
{
	int nValue;
	int nMin;
	int nMax;
	int nReserved;
} WACOMVALUE;

typedef struct
{
	unsigned int uValueCnt;     /* This MUST be set to WACOMFIELD_MAX. */
	unsigned int uValid;        /* Bit mask of WACOMFIELD_xxx bits. */
	WACOMVALUE values[WACOMFIELD_MAX];
} WACOMSTATE;

#define WACOMSTATE_INIT { WACOMFIELD_MAX }

typedef struct
{
	const char* pszName;
	const char* pszDesc;
	unsigned int uDeviceClass;
} WACOMCLASSREC;

typedef struct
{
	const char* pszName;
	const char* pszDesc;
	const char* pszVendorName;
	const char* pszVendorDesc;
	const char* pszClass;
	WACOMMODEL model;
} WACOMDEVICEREC;

/*****************************************************************************
** Public structures
*****************************************************************************/

typedef struct { int __unused; } *WACOMTABLET;

/*****************************************************************************
** Public API
*****************************************************************************/

	int WacomGetSupportedClassList(WACOMCLASSREC** ppList, int* pnSize);
	/* Returns 0 on success.  Pointer to class record list is returned to
	 * ppList, and size of list to pnSize.  Use WacomFreeList to release. */

	int WacomGetSupportedDeviceList(unsigned int uDeviceClass,
			WACOMDEVICEREC** ppList, int* pnSize);
	/* Returns 0 on success.  If device class is specified, only devices
	 * of the request type are returned.  A value of 0 will return all
	 * devices for all classes.  Pointer to device record list is returned to
	 * ppList, and size of list to pnSize.  Use WacomFreeList to release. */

	void WacomFreeList(void* pvList);
	/* Releases list memory. */

	int WacomCopyState(WACOMSTATE* pDest, WACOMSTATE* pSrc);
	/* Returns 0 on success.  Copies tablet state structures.
	 * Source and destination structures must be properly initialized,
	 * particularly the uValueCnt field must be set WACOMFIELD_MAX.
	 * Returns 0 on success. */

	unsigned int WacomGetClassFromName(const char* pszName);
	/* Returns the device class for a given name. Returns 0, if unknown. */

	unsigned int WacomGetDeviceFromName(const char* pszName,
			unsigned int uDeviceClass);
	/* Returns the device type for a given device name.  If the
	 * device class is specified, only that class will be searched.
	 * Returns 0, if unknown. */

	WACOMTABLET WacomOpenTablet(const char* pszDevice, WACOMMODEL* pModel);
	/* Returns tablet handle on success, NULL otherwise.
	 * pszDevice is pathname to device.  Model may be NULL; if any model
	 * parameters are 0, detection is automatic. */

	void WacomCloseTablet(WACOMTABLET hTablet);
	/* Releases all resource associated with tablet and closes device. */

	WACOMMODEL WacomGetModel(WACOMTABLET hTablet);
	/* Returns model (vendor, class, and device) of specified tablet. */

	const char* WacomGetVendorName(WACOMTABLET hTablet);
	/* Returns vendor name as human-readable string.  String is valid as
	 * long as tablet handle is valid and does not need to be freed. */

	const char* WacomGetClassName(WACOMTABLET hTablet);
	/* Returns class name as human-readable string.  String is valid as
	 * long as tablet handle is valid and does not need to be freed. */

	const char* WacomGetDeviceName(WACOMTABLET hTablet);
	/* Returns device name as human-readable string.  String is valid as
	 * long as tablet handle is valid and does not need to be freed. */

	const char* WacomGetSubTypeName(WACOMTABLET hTablet);
	/* Returns subtype name as human-readable string.  This is typically
	 * the model number (eg. ABC-1234).  String is valid as long as tablet
	 * handle is valid and does not need to be freed. */

	const char* WacomGetModelName(WACOMTABLET hTablet);
	/* Returns model name as human-readable string.  This is typically
	 * the full model name (eg. FooCo FooModel 9x12).  String is valid as
	 * long as tablet handle is valid and does not need to be freed. */

	int WacomGetROMVersion(WACOMTABLET hTablet, int* pnMajor, int* pnMinor,
			int* pnRelease);
	/* Returns 0 on success.  ROM version of the tablet firmware is returned
	 * in major, minor, and release values.  If the caller needs to
	 * distinguish between ROM versions to work around a bug, please consider
	 * submitting a patch to this library.  All tablets should appear
	 * equivalent through this interface. This call is provided for
	 * information purposes. */

	int WacomGetCapabilities(WACOMTABLET hTablet);
	/* Returns bitmask of valid fields.  Use (1 << WACOMFIELD_xxx) to
	 * translate field identifiers to bit positions.  This API will only
	 * support 32 capabilities. */

	int WacomGetState(WACOMTABLET hTablet, WACOMSTATE* pState);
	/* Returns 0 on success.  Tablet state is copied to specified structure.
	 * Data is only a snapshot of the device and may not accurately reflect
	 * the position of any specific tool. This is particularly true of
	 * multi-tool mode tablets.  NOTE: pState must point to an initialized
	 * structure; the uValueCnt field must be correctly set to
	 * WACOMFIELD_MAX. */

	int WacomGetFileDescriptor(WACOMTABLET hTablet);
	/* Returns the file descriptor of the tablet or -1 on error. */

	int WacomReadRaw(WACOMTABLET hTablet, unsigned char* puchData,
			unsigned int uSize);
	/* Returns number of bytes read, typically a single packet.  Call will
	 * block.  uSize should be the maximum size of the given data buffer. */

	int WacomParseData(WACOMTABLET hTablet, const unsigned char* puchData,
			unsigned int uLength, WACOMSTATE* pState);
	/* Updates the tablet state from a given packet.  If pState is specified,
	 * the tablet state is copied to the given structure.  This structure
	 * must be correctly initialized; the uValueCnt member must be set to
	 * WACOMFIELD_MAX. Returns 0 on success. */

/*****************************************************************************
** Private structures
*****************************************************************************/

typedef struct _WACOMTABLET_PRIV WACOMTABLET_PRIV;

struct _WACOMTABLET_PRIV
{
	void (*Close)(WACOMTABLET_PRIV* pTablet);
	WACOMMODEL (*GetModel)(WACOMTABLET_PRIV* pTablet);
	const char* (*GetVendorName)(WACOMTABLET_PRIV* pTablet);
	const char* (*GetClassName)(WACOMTABLET_PRIV* pTablet);
	const char* (*GetDeviceName)(WACOMTABLET_PRIV* pTablet);
	const char* (*GetSubTypeName)(WACOMTABLET_PRIV* pTablet);
	const char* (*GetModelName)(WACOMTABLET_PRIV* pTablet);
	int (*GetROMVer)(WACOMTABLET_PRIV* pTablet, int* pnMajor, int* pnMinor,
		int* pnRelease);
	int (*GetCaps)(WACOMTABLET_PRIV* pTablet);
	int (*GetState)(WACOMTABLET_PRIV* pTablet, WACOMSTATE* pState);
	int (*GetFD)(WACOMTABLET_PRIV* pTablet);
	int (*ReadRaw)(WACOMTABLET_PRIV* pTablet, unsigned char* puchData,
			unsigned int uSize);
	int (*ParseData)(WACOMTABLET_PRIV* pTablet, const unsigned char* puchData,
			unsigned int uLength, WACOMSTATE* pState);
};

#endif /* __LINUXWACOM_WACTABLET_H */
