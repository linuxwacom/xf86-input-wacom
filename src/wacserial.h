/*****************************************************************************
** wacserial.h
**
** Copyright (C) 2002 - John E. Joganic
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

#ifndef __WACPACK_WACSERIAL_H
#define __WACPACK_WACSERIAL_H

#define WACOMDEVICE_MASK    0x000F
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
#define WACOMDEVICE(x) ((x)&WACOMDEVICE_MASK)

#define WACOMCLASS_MASK     0x0300
#define WACOMCLASS_SERIAL   0x0100
#define WACOMCLASS_USB      0x0200
#define WACOMCLASS(x) ((x)&WACOMCLASS_MASK)

#define WACOMMODEL_SERIAL_INTUOS2 (WACOMCLASS_SERIAL|WACOMDEVICE_INTUOS2)
typedef unsigned int WACOMMODEL;

#define WACOMTOOLTYPE_NONE      0x00
#define WACOMTOOLTYPE_PEN       0x01
#define WACOMTOOLTYPE_PENCIL    0x02
#define WACOMTOOLTYPE_BRUSH     0x03
#define WACOMTOOLTYPE_ERASER    0x04
#define WACOMTOOLTYPE_AIRBRUSH  0x05
#define WACOMTOOLTYPE_MOUSE     0x06
#define WACOMTOOLTYPE_LENS      0x07

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
	int nValid;
	int nToolType;
	int nSerial;
	int nProximity;
	int nButtons;
	int nPosX;
	int nPosY;
	int nRotZ;
	int nDistance;
	int nPressure;
	int nTiltX;
	int nTiltY;
	int nAbsWheel;
	int nRelWheel;
	int nThrottle;
} WACOMSTATE;

typedef struct { int __unused; } *WACOMTABLET;

WACOMTABLET WacomOpenSerial(const char* pszDevice);
void WacomCloseSerial(WACOMTABLET hTablet);
WACOMMODEL WacomGetModel(WACOMTABLET hTablet);
const char* WacomGetModelName(WACOMTABLET hTablet);
int WacomReadRaw(WACOMTABLET hTablet, unsigned char* puchData,
		unsigned int uSize);
int WacomGetRomVersion(WACOMTABLET hTablet, int* pnMajor, int* pnMinor,
		int* pnRelease);
int WacomGetCapabilities(WACOMTABLET hTablet);
int WacomGetRanges(WACOMTABLET hTablet, WACOMSTATE* pMin, WACOMSTATE* pMax);
int WacomParseData(WACOMTABLET hTablet, const unsigned char* puchData,
		unsigned int uLength, WACOMSTATE* pState);

#endif /* __WACPACK_WACSERIAL_H */
