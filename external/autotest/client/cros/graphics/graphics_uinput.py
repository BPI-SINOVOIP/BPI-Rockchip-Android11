# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Provides uinput utils for generating user input events.
"""

# Please limit the use of the uinput library to this file. Try not to spread
# dependencies and abstract as much as possible to make switching to a different
# input library in the future easier.
import uinput


# Don't create a device during build_packages or for tests that don't need it.
uinput_device_keyboard = None
uinput_device_touch = None
uinput_device_mouse_rel = None

# Don't add more events to this list than are used. For a complete list of
# available events check python2.7/site-packages/uinput/ev.py.
UINPUT_DEVICE_EVENTS_KEYBOARD = [
    uinput.KEY_F4,
    uinput.KEY_F11,
    uinput.KEY_KPPLUS,
    uinput.KEY_KPMINUS,
    uinput.KEY_LEFTCTRL,
    uinput.KEY_TAB,
    uinput.KEY_UP,
    uinput.KEY_DOWN,
    uinput.KEY_LEFT,
    uinput.KEY_RIGHT,
    uinput.KEY_RIGHTSHIFT,
    uinput.KEY_LEFTALT,
    uinput.KEY_A,
    uinput.KEY_M,
    uinput.KEY_Q,
    uinput.KEY_V
]
# TODO(ihf): Find an ABS sequence that actually works.
UINPUT_DEVICE_EVENTS_TOUCH = [
    uinput.BTN_TOUCH,
    uinput.ABS_MT_SLOT,
    uinput.ABS_MT_POSITION_X + (0, 2560, 0, 0),
    uinput.ABS_MT_POSITION_Y + (0, 1700, 0, 0),
    uinput.ABS_MT_TRACKING_ID + (0, 10, 0, 0),
    uinput.BTN_TOUCH
]
UINPUT_DEVICE_EVENTS_MOUSE_REL = [
    uinput.REL_X,
    uinput.REL_Y,
    uinput.BTN_MOUSE,
    uinput.BTN_LEFT,
    uinput.BTN_RIGHT
]


def get_device_keyboard():
    """
    Lazy initialize device and return it. We don't want to create a device
    during build_packages or for tests that don't need it, hence init with None.
    """
    global uinput_device_keyboard
    if uinput_device_keyboard is None:
        uinput_device_keyboard = uinput.Device(UINPUT_DEVICE_EVENTS_KEYBOARD)
    return uinput_device_keyboard


def get_device_mouse_rel():
    """
    Lazy initialize device and return it. We don't want to create a device
    during build_packages or for tests that don't need it, hence init with None.
    """
    global uinput_device_mouse_rel
    if uinput_device_mouse_rel is None:
        uinput_device_mouse_rel = uinput.Device(UINPUT_DEVICE_EVENTS_MOUSE_REL)
    return uinput_device_mouse_rel


def get_device_touch():
    """
    Lazy initialize device and return it. We don't want to create a device
    during build_packages or for tests that don't need it, hence init with None.
    """
    global uinput_device_touch
    if uinput_device_touch is None:
        uinput_device_touch = uinput.Device(UINPUT_DEVICE_EVENTS_TOUCH)
    return uinput_device_touch


def translate_name(event_name):
    """
    Translates string |event_name| to uinput event.
    """
    return getattr(uinput, event_name)


def emit(device, event_name, value, syn=True):
    """
    Wrapper for uinput.emit. Emits event with value.
    Example: ('REL_X', 20), ('BTN_RIGHT', 1)
    """
    event = translate_name(event_name)
    device.emit(event, value, syn)


def emit_click(device, event_name, syn=True):
    """
    Wrapper for uinput.emit_click. Emits click event. Only KEY and BTN events
    are accepted, otherwise ValueError is raised. Example: 'KEY_A'
    """
    event = translate_name(event_name)
    device.emit_click(event, syn)


def emit_combo(device, event_names, syn=True):
    """
    Wrapper for uinput.emit_combo. Emits sequence of events.
    Example: ['KEY_LEFTCTRL', 'KEY_LEFTALT', 'KEY_F5']
    """
    events = [translate_name(en) for en in event_names]
    device.emit_combo(events, syn)
