# Copyright 2022 Red Hat, Inc
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

from typing import Dict, List, Union
from pathlib import Path

import attr
import enum
import pytest
import libevdev
import logging
import yaml

import gi

try:
    gi.require_version("wacom", "1.0")
    from gi.repository import wacom
except ValueError as e:
    print(e)
    print(
        "Export the following variables to fix this error (note the build directory name)"
    )
    print('$ export GI_TYPELIB_PATH="$PWD/builddir:$GI_TYPELIB_PATH"')
    print('$ export LD_LIBRARY_PATH="$PWD/builddir:$LD_LIBRARY_PATH"')
    raise ImportError("Unable to load GIR bindings")


logger = logging.getLogger(__name__)


class PenId(enum.IntEnum):
    ARTPEN = 0x100804


@attr.s
class InputId:
    product: int = attr.ib()
    bustype: int = attr.ib(default=0x3)
    vendor: int = attr.ib(default=0x56A)
    version: int = attr.ib(default=0)

    @classmethod
    def from_list(cls, ids: List[int]) -> "InputId":
        bus, vid, pid, version = ids
        return cls(bustype=bus, vendor=vid, product=pid, version=vid)


@attr.s
class Device:
    """
    The class to set up a device. The best way to use this class is to define
    a device in a yaml file, then use :meth:`Device.from_name` to load that
    file.
    """

    name: str = attr.ib()
    id: InputId = attr.ib()
    bits: List[libevdev.EventCode] = attr.ib()
    absinfo: Dict[libevdev.EventCode, libevdev.InputAbsInfo] = attr.ib(
        default=attr.Factory(dict)
    )
    props: List[libevdev.InputProperty] = attr.ib(default=attr.Factory(list))

    def create_uinput(self) -> "UinputDevice":
        """
        Convert this device into a uinput device and return it.
        """
        d = libevdev.Device()
        d.name = self.name
        d.id = {
            "bustype": self.id.bustype,
            "vendor": self.id.vendor,
            "product": self.id.product,
        }

        for b in self.bits:
            d.enable(b)

        for code, absinfo in self.absinfo.items():
            d.enable(code, absinfo)

        for prop in self.props:
            d.enable(prop)

        try:
            return UinputDevice(d.create_uinput_device())
        except PermissionError:
            pytest.skip("Insufficient permissions to create uinput device")
        except FileNotFoundError:
            pytest.skip("/dev/uinput not available")

    @classmethod
    def from_name(cls, name: str, type: str) -> "Device":
        """
        Create a Device from the given name with the given type (pen, pad,
        finger). This method iterates through the test/devices/*.yml files and
        finds the file for the device with the given name, then loads the
        matching event node for that type.
        """
        type = type.lower()
        assert type.lower() in ("pen", "pad", "finger")

        for ymlfile in Path("test/devices").glob("*.yml"):
            with open(ymlfile) as fd:
                yml = yaml.safe_load(fd)
                logger.debug(f"Found device: {yml['name']}")
                if yml["name"].upper() != name.upper():
                    continue

                for d in yml["devices"]:
                    if d["type"] != type:
                        continue

                    name = d["name"]
                    id = InputId.from_list([int(i, 16) for i in d["id"]])
                    bits = [libevdev.evbit(b) for b in d["bits"]]
                    abs = {
                        libevdev.evbit(n): libevdev.InputAbsInfo(*v)
                        for n, v in d["abs"].items()
                    }
                    props = [libevdev.propbit(p) for p in d["props"]]

                    return Device(name=name, id=id, bits=bits, absinfo=abs, props=props)
                raise ValueError(f"Device '{name}' does not have type '{type}'")

        raise ValueError(f"Device '{name}' does not exist")


@attr.s
class UinputDevice:
    """
    A wrapper around a uinput device.
    """

    uidev: libevdev.Device = attr.ib()

    @property
    def devnode(self):
        return self.uidev.devnode

    def write_events(self, events: List["Ev"]):
        """
        Write the list of events to the uinput device. If a SYN_REPORT is not
        the last element in the event list, it is automatically appended.
        """
        last = events[-1].libevdev_event
        if last.code != libevdev.EV_SYN.SYN_REPORT:
            events += [libevdev.InputEvent(libevdev.EV_SYN.SYN_REPORT, 0)]
        self.uidev.send_events([e.libevdev_event for e in events])


@attr.s
class Monitor:
    """
    An event monitor for a Wacom driver device. This monitor logs any messages
    from the driver and accumulates all events emitted in a list.
    """

    uidev: UinputDevice = attr.ib()
    device: Device = attr.ib()
    wacom_device: wacom.Device = attr.ib()
    events = attr.ib(default=attr.Factory(list))

    @classmethod
    def new(
        cls,
        device: Device,
        uidev: UinputDevice,
        wacom_device: wacom.Device,
    ) -> "Monitor":
        m = cls(
            device=device,
            uidev=uidev,
            wacom_device=wacom_device,
        )

        def cb_log(wacom_device, prefix, msg):
            logger.info(f"{prefix}: {msg.strip()}")

        def cb_debug_log(wacom_device, level, func, msg):
            logger.debug(f"DEBUG: {level:2d}: {func:32s}| {msg.strip()}")

        def cb_proximity(wacom_device, is_prox_in, axes):
            m.events.append(Proximity(is_prox_in, axes))

        def cb_button(wacom_device, is_absolute, button, is_press, axes):
            m.events.append(
                Button(
                    is_absolute=is_absolute, button=button, is_press=is_press, axes=axes
                )
            )

        def cb_motion(wacom_device, is_absolute, axes):
            m.events.append(Motion(is_absolute=is_absolute, axes=axes))

        def cb_key(wacom_device, key, is_press):
            m.events.append(Key(is_press=is_press, key=key))

        def cb_touch(wacom_device, type, touchid, x, y):
            m.events.append(Touch(type=Touch.Type(type), id=touchid, x=x, y=y))

        wacom_device.connect("log-message", cb_log)
        wacom_device.connect("debug-message", cb_debug_log)
        wacom_device.connect("proximity", cb_proximity)
        wacom_device.connect("button", cb_button)
        wacom_device.connect("motion", cb_motion)
        wacom_device.connect("keycode", cb_key)
        wacom_device.connect("button", cb_button)

        return m

    @classmethod
    def new_from_device(cls, device: Device, opts: Dict[str, str]) -> "Monitor":
        uidev = device.create_uinput()
        try:
            with open(uidev.devnode, "rb"):
                pass
        except PermissionError:
            pytest.skip("Insufficient permissions to open event node")

        opts["Device"] = uidev.devnode
        wacom_options = wacom.Options()
        for name, value in opts.items():
            wacom_options.set(name, value)

        wacom_driver = wacom.Driver()
        wacom_device = wacom.Device.new(wacom_driver, device.name, wacom_options)
        logger.debug(f"PreInit for '{device.name}' with options {opts}")

        monitor = cls.new(device, uidev, wacom_device)

        assert wacom_device.preinit()
        assert wacom_device.setup()
        assert wacom_device.enable()

        return monitor

    def write_events(self, events: List[Union["Ev", "Sev"]]) -> None:
        evs = [e.scale(self.device) if isinstance(e, Sev) else e for e in events]
        self.uidev.write_events(evs)


@attr.s
class Ev:
    """
    A class to simplify writing event sequences.

    >>> Ev("BTN_TOUCH", 1)
    >>> Ev("ABS_X", 1234)

    Note that the value in an Ev must be in device coordinates, see
    :class:`Sev` for the scaled version.
    """

    name: str = attr.ib()
    value: int = attr.ib()

    @property
    def libevdev_event(self):
        return libevdev.InputEvent(libevdev.evbit(self.name.upper()), self.value)


@attr.s
class Sev:
    """
    A class to simplify writing event sequences in a generic manner. The value
    range for any ``ABS_FOO`` axis is interpreted as percent of the axis range
    on the device to replay. For example, to put the cursor in the middle of a
    tablet, use:

    >>> Sev("ABS_X", 50.0)
    >>> Sev("ABS_Y", 50.0)

    The value is a real number, converted to an int when scaled into the axis
    range.
    """

    name: str = attr.ib()
    value: float = attr.ib()

    @value.validator
    def _check_value(self, attribute, value):
        if -20 <= value <= 120:  # Allow for 20% outside range for niche test cases
            return
        raise ValueError("value must be in percent")

    def scale(self, device: Device) -> Ev:
        value = self.value
        if self.name.startswith("ABS_") and self.name not in [
            "ABS_MT_SLOT",
            "ABS_MT_TRACKING_ID",
        ]:
            evbit = libevdev.evbit(self.name)
            absinfo = device.absinfo[evbit]
            value = (
                absinfo.minimum
                + (absinfo.maximum - absinfo.minimum + 1) * self.value / 100.0
            )

        return Ev(self.name, int(value))


@attr.s
class Proximity:
    """A proximity event"""

    is_prox_in: bool = attr.ib()
    axes: wacom.EventData = attr.ib()


@attr.s
class Button:
    """A button event"""

    is_absolute: bool = attr.ib()
    button: int = attr.ib()
    is_press: bool = attr.ib()
    axes: wacom.EventData = attr.ib()


@attr.s
class Key:
    """A key event"""

    button: int = attr.ib()
    is_press: bool = attr.ib()


@attr.s
class Motion:
    """A motion event"""

    is_absolute: bool = attr.ib()
    axes: wacom.EventData = attr.ib()


@attr.s
class Touch:
    """A touch event"""

    class Type(enum.IntEnum):
        BEGIN = 0
        UPDATE = 1
        END = 2

    type: Type = attr.ib()
    id: int = attr.ib()
    x: int = attr.ib()
    y: int = attr.ib()
