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

typedef unsigned int WACOMMODEL;

#define WACOMVENDOR_MASK        0xFFFF
#define WACOMVENDOR_WACOM       0x056A
#define WACOMVENDOR(x) (((x)>>16)&WACOMVENDOR_MASK)

#define WACOMCLASS_MASK         0x0003
#define WACOMCLASS_SERIAL       0x0001
#define WACOMCLASS_USB          0x0002
#define WACOMCLASS(x) (((x)>>8)&WACOMCLASS_MASK)

#define WACOMDEVICE_MASK        0x00FF
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
#define WACOMDEVICE(x) ((x)&WACOMDEVICE_MASK)

#define WACOM_MAKEMODEL(v,c,d) \
	(((((WACOMMODEL)(v)) & WACOMVENDOR_MASK) << 16) | \
	 ((((WACOMMODEL)(c)) & WACOMCLASS_MASK) << 8) | \
	 ((((WACOMMODEL)(d)) & WACOMDEVICE_MASK)))

#define WACOMMODEL_SERIAL_INTUOS2 \
	WACOM_MAKEMODEL(WACOMVENDOR_WACOM,WACOMCLASS_SERIAL,WACOMDEVICE_INTUOS2)
#define WACOMMODEL_USB_INTUOS2 \
	WACOM_MAKEMODEL(WACOMVENDOR_WACOM,WACOMCLASS_USB,WACOMDEVICE_INTUOS2)

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
	unsigned int uValueCnt;
	unsigned int uValid;
	WACOMVALUE values[WACOMFIELD_MAX];
} WACOMSTATE;

#define WACOMSTATE_INIT { WACOMFIELD_MAX }

/*****************************************************************************
** Public structures
*****************************************************************************/

typedef struct { int __unused; } *WACOMTABLET;

WACOMTABLET WacomOpenTablet(const char* pszDevice);
void WacomCloseTablet(WACOMTABLET hTablet);
WACOMMODEL WacomGetModel(WACOMTABLET hTablet);
const char* WacomGetVendorName(WACOMTABLET hTablet);
const char* WacomGetModelName(WACOMTABLET hTablet);
int WacomGetROMVersion(WACOMTABLET hTablet, int* pnMajor, int* pnMinor,
		int* pnRelease);
int WacomGetCapabilities(WACOMTABLET hTablet);
int WacomGetState(WACOMTABLET hTablet, WACOMSTATE* pState);
int WacomCopyState(WACOMSTATE* pDest, WACOMSTATE* pSrc);
int WacomReadRaw(WACOMTABLET hTablet, unsigned char* puchData,
		unsigned int uSize);
int WacomParseData(WACOMTABLET hTablet, const unsigned char* puchData,
		unsigned int uLength, WACOMSTATE* pState);

/*****************************************************************************
** Private structures
*****************************************************************************/

typedef struct _WACOMTABLET_PRIV WACOMTABLET_PRIV;

struct _WACOMTABLET_PRIV
{
	void (*Close)(WACOMTABLET_PRIV* pTablet);
	WACOMMODEL (*GetModel)(WACOMTABLET_PRIV* pTablet);
	const char* (*GetVendorName)(WACOMTABLET_PRIV* pTablet);
	const char* (*GetModelName)(WACOMTABLET_PRIV* pTablet);
	int (*GetROMVer)(WACOMTABLET_PRIV* pTablet, int* pnMajor, int* pnMinor,
		int* pnRelease);
	int (*GetCaps)(WACOMTABLET_PRIV* pTablet);
	int (*GetState)(WACOMTABLET_PRIV* pTablet, WACOMSTATE* pState);
	int (*ReadRaw)(WACOMTABLET_PRIV* pTablet, unsigned char* puchData,
			unsigned int uSize);
	int (*ParseData)(WACOMTABLET_PRIV* pTablet, const unsigned char* puchData,
			unsigned int uLength, WACOMSTATE* pState);
};

#endif /* __LINUXWACOM_WACTABLET_H */
