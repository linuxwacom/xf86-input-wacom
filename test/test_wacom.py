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
from . import Device, Monitor, Sev, Proximity

import pytest
import logging
from gi.repository import GLib

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


@pytest.mark.parametrize("axis", ["x", "y", "pressure", "tilt_x", "tilt_y"])
def test_axis_updates(mainloop, opts, axis):
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
    }

    # Send a bunch of events with only one axis changing, the rest remains at
    # the device's logical center
    for i in range(0, 30, 2):
        axes = [0] * len(map)
        axes[map[axis]] = i

        def axval(axis: str) -> int:
            return axes[map[axis]]

        ev = [
            Sev("ABS_X", 50 + axval("x")),
            Sev("ABS_Y", 50 + axval("y")),
            Sev("ABS_Z", 50),  # FIXME: what is this axis??
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

    events = iter(monitor.events)
    # Ignore the proximity event since all axes change there by necessity
    _ = next(events)

    first = {name: getattr(next(events).axes, name) for name in map}

    for e in events:
        current = {name: getattr(e.axes, name) for name in map}
        for name in map:
            if name == axis:
                assert first[name] < current[name]
            else:
                assert first[name] == current[name]
