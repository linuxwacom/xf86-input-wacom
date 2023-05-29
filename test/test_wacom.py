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


from typing import Dict
from . import Button, Device, Monitor, Ev, Sev, Proximity, PenId

import pytest
import logging
import gi
from gi.repository import GLib

gi.require_version("wacom", "1.0")
from gi.repository import wacom

logger = logging.getLogger(__name__)


@pytest.fixture
def mainloop():
    """Default mainloop fixture, exiting after 500ms"""
    mainloop = GLib.MainLoop()
    GLib.timeout_add(500, mainloop.quit)
    return mainloop


@pytest.fixture
def opts() -> Dict[str, str]:
    """Default driver options (for debugging)"""
    return {
        "CommonDBG": "12",
        "DebugLevel": "12",
    }


def test_proximity(mainloop, opts):
    """
    Simple test to verify proximity in/out events are sent
    """
    dev = Device.from_name("PTH660", "Pen")
    monitor = Monitor.new_from_device(dev, opts)

    prox_in = [
        Sev("ABS_X", 50),
        Sev("ABS_Y", 50),
        Sev("BTN_TOOL_PEN", 1),
        Sev("SYN_REPORT", 0),
    ]
    prox_out = [
        Sev("ABS_X", 50),
        Sev("ABS_Y", 50),
        Sev("BTN_TOOL_PEN", 0),
        Sev("SYN_REPORT", 0),
    ]
    monitor.write_events(prox_in)
    monitor.write_events(prox_out)
    mainloop.run()

    assert isinstance(monitor.events[0], Proximity)
    assert monitor.events[0].is_prox_in

    assert isinstance(monitor.events[-1], Proximity)
    assert not monitor.events[-1].is_prox_in


@pytest.mark.parametrize("rotate", ["NONE", "CW", "CCW", "HALF"])
def test_relative_motion(mainloop, opts, rotate):
    """
    Check relative motion works in the various rotations
    """
    opts["Mode"] = "Relative"
    opts["Rotate"] = rotate
    dev = Device.from_name("PTH660", "Pen")
    monitor = Monitor.new_from_device(dev, opts)

    prox_in = [
        Sev("ABS_X", 50),
        Sev("ABS_Y", 50),
        Sev("BTN_TOOL_PEN", 1),
        Sev("SYN_REPORT", 0),
    ]

    # Physical pen motion is center towards bottom right
    motions = []
    for i in range(20):
        motions.extend(
            [
                Sev("ABS_X", 50 + i),
                Sev("ABS_Y", 50 + i),
                Sev("SYN_REPORT", 0),
            ]
        )

    prox_out = [
        Sev("ABS_X", 50),
        Sev("ABS_Y", 50),
        Sev("BTN_TOOL_PEN", 0),
        Sev("SYN_REPORT", 0),
    ]
    monitor.write_events(prox_in)
    monitor.write_events(motions)
    monitor.write_events(prox_out)
    mainloop.run()

    # Now collect the events
    xs = [e.axes.x for e in monitor.events]
    ys = [e.axes.y for e in monitor.events]

    print(f"Collected events: {list(zip(xs, ys))}")

    # We're in relative mode, so we expect the first and last event (the
    # proximity ones) to be 0/0
    assert xs[0] == 0
    assert ys[0] == 0
    assert xs[-1] == 0
    assert ys[-1] == 0

    # There's some motion adjustment, so we skip the first five events, the
    # rest should be exactly the same
    motions = list(zip(xs[5:-1], ys[5:-1]))

    print(f"Motion events to analyze: {motions}")

    # WARNING: the CW and CCW rotation seems swapped but that's what the
    # driver does atm
    if rotate == "NONE":
        assert all([x > 0 and y > 0 for x, y in motions])
    elif rotate == "HALF":
        assert all([x < 0 and y < 0 for x, y in motions])
    elif rotate == "CCW":
        assert all([x < 0 and y > 0 for x, y in motions])
    elif rotate == "CW":
        assert all([x > 0 and y < 0 for x, y in motions])
    else:
        pytest.fail("Invalid rotation mode")

    assert all([m == motions[0] for m in motions])


@pytest.mark.parametrize("axis", ["x", "y", "pressure", "tilt_x", "tilt_y", "wheel"])
@pytest.mark.parametrize("stylus_type", [PenId.ARTPEN, PenId.CINTIQ_13_PEN])
def test_axis_updates(mainloop, opts, axis, stylus_type):
    """
    Check that the various axes come through correctly
    """
    dev = Device.from_name("PTH660", "Pen")
    monitor = Monitor.new_from_device(dev, opts)

    # This is merely our local mapping into the axes array in the loop,
    # nothing to do with the driver
    map = {
        "x": 0,
        "y": 1,
        "pressure": 2,
        "tilt_x": 3,
        "tilt_y": 4,
        "wheel": 5,
    }

    # Send a bunch of events with only one axis changing, the rest remains at
    # the device's logical center
    for i in range(0, 30, 2):
        axes = [0] * len(map)
        axes[map[axis]] = i

        def axval(axis: str) -> int:
            return axes[map[axis]]

        ev = [
            Ev("ABS_MISC", stylus_type),
            Ev("MSC_SERIAL", 0x123456),
            Sev("ABS_X", 50 + axval("x")),
            Sev("ABS_Y", 50 + axval("y")),
            # ABS_Z sets ds->abswheel in the driver which is used for artpen
            # physical rotation and airbrush wheel - both share the
            # same valuator. This is *not* rotation, that axis is for the
            # cursor rotation only.
            # Here we send this event even for the Cintiq Pen to ensure it
            # gets correctly ignored by the driver
            Sev("ABS_Z", 50 + axval("wheel")),
            Sev("ABS_PRESSURE", 50 + axval("pressure")),
            Sev("ABS_DISTANCE", 0),  # Distance isn't exported
            Sev("ABS_TILT_X", 50 + axval("tilt_x")),
            Sev("ABS_TILT_Y", 50 + axval("tilt_y")),
            Sev("BTN_TOOL_PEN", 1),
            Sev("SYN_REPORT", 0),
        ]

        monitor.write_events(ev)

    mainloop.run()
    logger.debug(f"We have {len(monitor.events)} events")

    # Force a prox-out so we don't get stuck buttons. We don't call
    # mainloop.run() because we don't want to collect the prox out.
    ev = [
        Sev("BTN_TOOL_PEN", 0),
        Sev("SYN_REPORT", 0),
    ]
    monitor.write_events(ev)

    events = iter(monitor.events)
    # Ignore the proximity event since all axes change there by necessity
    _ = next(events)

    # bitflags to indicate which axis is actually present in the event
    mask_map = {
        "x": wacom.EventAxis.AXIS_X,
        "y": wacom.EventAxis.AXIS_Y,
        "pressure": wacom.EventAxis.AXIS_PRESSURE,
        "tilt_x": wacom.EventAxis.AXIS_TILT_X,
        "tilt_y": wacom.EventAxis.AXIS_TILT_X,
        "wheel": wacom.EventAxis.AXIS_WHEEL,
    }

    first = {name: getattr(next(events).axes, name) for name in map}

    for e in events:
        current = {name: getattr(e.axes, name) for name in map}
        for name in map:
            # Artpen has rotation via our 'wheel' axis but the cintiq pen does
            # not send that axis even when the kernel sends events for it
            pen_has_axis = name != "wheel" or stylus_type == PenId.ARTPEN
            event_has_axis = e.axes.mask & mask_map[name] != 0

            assert event_has_axis == pen_has_axis, f"for axis {name}"
            if event_has_axis and name == axis:
                assert first[name] < current[name], f"for axis {name}"
            else:
                assert first[name] == current[name], f"for axis {name}"


@pytest.mark.parametrize(
    "stylus_type", [PenId.ARTPEN, PenId.CINTIQ_13_PEN, PenId.AIRBRUSH]
)
def test_axis_updates_wheel(mainloop, opts, stylus_type):
    """
    Same as the above test but tests the wheel axis only, since that one is
    multiplexed through a single valuator.
    """
    dev = Device.from_name("PTH660", "Pen")
    monitor = Monitor.new_from_device(dev, opts)

    # Send a bunch of events with only one the wheel axis changing, the rest
    # remains at the device's logical center
    for i in range(0, 30, 2):
        extra_events = []

        # FIXME: there's a bug in the driver - because *both* ABS_Z (artpen)
        # and ABS_WHEEL (airbrush) unconditionally update ds->abswheel
        # regardless of the tool currently in prox, we must not send the
        # respective "other" axis for the current tool. Exception is the
        # default pen which is supposed to ignore both ABS_Z and ABS_WHEEL.

        # ABS_WHEEL sets ds->abswheel in the driver which is used for airbrush
        # physical wheel - both artpen and airbrush share the same valuator.
        # This is *not* the wheel for touchring. That axis is for the pad.
        if stylus_type != PenId.ARTPEN:
            extra_events.append(Sev("ABS_WHEEL", 50 + i))

        # ABS_Z sets ds->abswheel in the driver which is used for artpen
        # physical rotation - artpen shares the same valuator with airbrush.
        # This is *not* rotation. That axis is for the cursor rotation only.
        if stylus_type != PenId.AIRBRUSH:
            extra_events.append(Sev("ABS_Z", 50 + i))

        ev = [
            Ev("ABS_MISC", stylus_type),
            Ev("MSC_SERIAL", 0x123456),
            Sev("ABS_X", 50),
            Sev("ABS_Y", 50),
            # expands to ABS_Z, ABS_WHEEL or both
            *extra_events,
            Sev("ABS_PRESSURE", 50),
            Sev("ABS_DISTANCE", 0),  # Distance isn't exported
            Sev("ABS_TILT_X", 50),
            Sev("ABS_TILT_Y", 50),
            Sev("BTN_TOOL_PEN", 1),
            Sev("SYN_REPORT", 0),
        ]

        monitor.write_events(ev)

    mainloop.run()
    logger.debug(f"We have {len(monitor.events)} events")

    # Force a prox-out so we don't get stuck buttons. We don't call
    # mainloop.run() because we don't want to collect the prox out.
    ev = [
        Sev("BTN_TOOL_PEN", 0),
        Sev("SYN_REPORT", 0),
    ]
    monitor.write_events(ev)

    events = iter(monitor.events)
    # Ignore the proximity event since all axes change there by necessity
    _ = next(events)

    first = next(events).axes
    first_wheel = first.wheel

    for e in events:
        current = e.axes
        current_wheel = current.wheel

        # The default pen doesn't have the axis, artpen and airbrush have it
        # and we sent events for it, so expect it to be present
        pen_has_axis = stylus_type in [PenId.ARTPEN, PenId.AIRBRUSH]
        event_has_axis = e.axes.mask & wacom.EventAxis.AXIS_WHEEL != 0
        assert event_has_axis == pen_has_axis
        if pen_has_axis:
            assert first_wheel < current_wheel
        else:
            assert first_wheel == current_wheel


@pytest.mark.parametrize("smooth", (True, False))
@pytest.mark.parametrize("vertical", (True, False))
def test_scroll(mainloop, opts, vertical, smooth):
    """
    Check panscrolling works correctly
    """
    dev = Device.from_name("PTH660", "Pen")
    opts["PanScrollThreshold"] = "150"
    opts["SmoothPanscrollingEnabled"] = "true" if smooth else "false"

    prox_in = [
        Sev("ABS_X", 50),
        Sev("ABS_Y", 50),
        Sev("BTN_TOOL_PEN", 1),
        Sev("SYN_REPORT", 0),
    ]

    prox_out = [
        Sev("BTN_TOOL_PEN", 0),
        Sev("SYN_REPORT", 0),
    ]

    press_button2 = [
        Sev("BTN_STYLUS", 1),
        Sev("SYN_REPORT", 0),
    ]

    touchdown_pen = [
        Sev("BTN_TOUCH", 1),
        Sev("ABS_PRESSURE", 20),
        Sev("SYN_REPORT", 0),
    ]

    if vertical:
        move_pen = [Sev("ABS_Y", 75), Sev("SYN_REPORT", 0)]
    else:
        move_pen = [Sev("ABS_X", 75), Sev("SYN_REPORT", 0)]

    up_pen = [Sev("BTN_TOUCH", 0), Sev("ABS_PRESSURE", 0), Sev("SYN_REPORT", 0)]

    depress_button2 = [Sev("BTN_STYLUS", 0), Sev("SYN_REPORT", 0)]

    monitor = Monitor.new_from_device(dev, opts)
    monitor.wacom_device.set_runtime_option("PanButton", "2")

    monitor.write_events(prox_in)
    monitor.write_events(press_button2)
    monitor.write_events(touchdown_pen)  # Pen touchdown
    monitor.write_events(move_pen)  # Move pen 25% towards positive x or y
    monitor.write_events(up_pen)  # Pen up
    monitor.write_events(depress_button2)  # Depress button2
    monitor.write_events(prox_out)

    mainloop.run()
    if smooth:
        have_we_scrolled = False
        for event in monitor.events:
            if vertical:
                if event.axes.scroll_y != 0:
                    assert event.axes.scroll_y == -808265
                    have_we_scrolled = True
            else:
                if event.axes.scroll_x != 0:
                    assert event.axes.scroll_x == -1223320
                    have_we_scrolled = True
        assert have_we_scrolled
    else:
        have_button_events = False
        for event in monitor.events:
            if isinstance(event, Button):
                print(event.button)
                if vertical:
                    if event.button == 4:  # we're moving bottom-to-top
                        have_button_events = True
                else:
                    if event.button == 6:  # we're moving right-to-left
                        have_button_events = True
        assert have_button_events


# vim: set expandtab tabstop=4 shiftwidth=4:
