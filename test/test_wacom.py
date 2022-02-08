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
