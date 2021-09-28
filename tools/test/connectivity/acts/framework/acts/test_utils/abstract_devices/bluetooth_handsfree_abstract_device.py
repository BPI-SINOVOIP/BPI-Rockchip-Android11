#!/usr/bin/env python3
#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
import inspect
import time
from acts import asserts
from acts.controllers.buds_lib.dev_utils import apollo_sink_events
from acts.test_utils.bt.bt_constants import bt_default_timeout



def validate_controller(controller, abstract_device_class):
    """Ensure controller has all methods in abstract_device_class.
    Also checks method signatures to ensure parameters are satisfied.

    Args:
        controller: instance of a device controller.
        abstract_device_class: class definition of an abstract_device interface.
    Raises:
         NotImplementedError: if controller is missing one or more methods.
    """
    ctlr_methods = inspect.getmembers(controller, predicate=callable)
    reqd_methods = inspect.getmembers(
        abstract_device_class, predicate=inspect.ismethod)
    expected_func_names = {method[0] for method in reqd_methods}
    controller_func_names = {method[0] for method in ctlr_methods}

    if not controller_func_names.issuperset(expected_func_names):
        raise NotImplementedError(
            'Controller {} is missing the following functions: {}'.format(
                controller.__class__.__name__,
                repr(expected_func_names - controller_func_names)))

    for func_name in expected_func_names:
        controller_func = getattr(controller, func_name)
        required_func = getattr(abstract_device_class, func_name)
        required_signature = inspect.signature(required_func)
        if inspect.signature(controller_func) != required_signature:
            raise NotImplementedError(
                'Method {} must have the signature {}{}.'.format(
                    controller_func.__qualname__, controller_func.__name__,
                    required_signature))


class BluetoothHandsfreeAbstractDevice:
    """Base class for all Bluetooth handsfree abstract devices.

    Desired controller classes should have a corresponding Bluetooth handsfree
    abstract device class defined in this module.
    """

    @property
    def mac_address(self):
        raise NotImplementedError

    def accept_call(self):
        raise NotImplementedError()

    def end_call(self):
        raise NotImplementedError()

    def enter_pairing_mode(self):
        raise NotImplementedError()

    def next_track(self):
        raise NotImplementedError()

    def pause(self):
        raise NotImplementedError()

    def play(self):
        raise NotImplementedError()

    def power_off(self):
        raise NotImplementedError()

    def power_on(self):
        raise NotImplementedError()

    def previous_track(self):
        raise NotImplementedError()

    def reject_call(self):
        raise NotImplementedError()

    def volume_down(self):
        raise NotImplementedError()

    def volume_up(self):
        raise NotImplementedError()


class PixelBudsBluetoothHandsfreeAbstractDevice(
        BluetoothHandsfreeAbstractDevice):

    CMD_EVENT = 'EvtHex'

    def __init__(self, pixel_buds_controller):
        self.pixel_buds_controller = pixel_buds_controller

    def format_cmd(self, cmd_name):
        return self.CMD_EVENT + ' ' + apollo_sink_events.SINK_EVENTS[cmd_name]

    @property
    def mac_address(self):
        return self.pixel_buds_controller.bluetooth_address

    def accept_call(self):
        return self.pixel_buds_controller.cmd(
            self.format_cmd('EventUsrAnswer'))

    def end_call(self):
        return self.pixel_buds_controller.cmd(
            self.format_cmd('EventUsrCancelEnd'))

    def enter_pairing_mode(self):
        return self.pixel_buds_controller.set_pairing_mode()

    def next_track(self):
        return self.pixel_buds_controller.cmd(
            self.format_cmd('EventUsrAvrcpSkipForward'))

    def pause(self):
        return self.pixel_buds_controller.cmd(
            self.format_cmd('EventUsrAvrcpPause'))

    def play(self):
        return self.pixel_buds_controller.cmd(
            self.format_cmd('EventUsrAvrcpPlay'))

    def power_off(self):
        return self.pixel_buds_controller.power('Off')

    def power_on(self):
        return self.pixel_buds_controller.power('On')

    def previous_track(self):
        return self.pixel_buds_controller.cmd(
            self.format_cmd('EventUsrAvrcpSkipBackward'))

    def reject_call(self):
        return self.pixel_buds_controller.cmd(
            self.format_cmd('EventUsrReject'))

    def volume_down(self):
        return self.pixel_buds_controller.volume('Down')

    def volume_up(self):
        return self.pixel_buds_controller.volume('Up')


class EarstudioReceiverBluetoothHandsfreeAbstractDevice(
        BluetoothHandsfreeAbstractDevice):
    def __init__(self, earstudio_controller):
        self.earstudio_controller = earstudio_controller

    @property
    def mac_address(self):
        return self.earstudio_controller.mac_address

    def accept_call(self):
        return self.earstudio_controller.press_accept_call()

    def end_call(self):
        return self.earstudio_controller.press_end_call()

    def enter_pairing_mode(self):
        return self.earstudio_controller.enter_pairing_mode()

    def next_track(self):
        return self.earstudio_controller.press_next()

    def pause(self):
        return self.earstudio_controller.press_play_pause()

    def play(self):
        return self.earstudio_controller.press_play_pause()

    def power_off(self):
        return self.earstudio_controller.power_off()

    def power_on(self):
        return self.earstudio_controller.power_on()

    def previous_track(self):
        return self.earstudio_controller.press_previous()

    def reject_call(self):
        return self.earstudio_controller.press_reject_call()

    def volume_down(self):
        return self.earstudio_controller.press_volume_down()

    def volume_up(self):
        return self.earstudio_controller.press_volume_up()


class JaybirdX3EarbudsBluetoothHandsfreeAbstractDevice(
        BluetoothHandsfreeAbstractDevice):
    def __init__(self, jaybird_controller):
        self.jaybird_controller = jaybird_controller

    @property
    def mac_address(self):
        return self.jaybird_controller.mac_address

    def accept_call(self):
        return self.jaybird_controller.press_accept_call()

    def end_call(self):
        return self.jaybird_controller.press_reject_call()

    def enter_pairing_mode(self):
        return self.jaybird_controller.enter_pairing_mode()

    def next_track(self):
        return self.jaybird_controller.press_next()

    def pause(self):
        return self.jaybird_controller.press_play_pause()

    def play(self):
        return self.jaybird_controller.press_play_pause()

    def power_off(self):
        return self.jaybird_controller.power_off()

    def power_on(self):
        return self.jaybird_controller.power_on()

    def previous_track(self):
        return self.jaybird_controller.press_previous()

    def reject_call(self):
        return self.jaybird_controller.press_reject_call()

    def volume_down(self):
        return self.jaybird_controller.press_volume_down()

    def volume_up(self):
        return self.jaybird_controller.press_volume_up()


class AndroidHeadsetBluetoothHandsfreeAbstractDevice(
        BluetoothHandsfreeAbstractDevice):
    def __init__(self, ad_controller):
        self.ad_controller = ad_controller

    @property
    def mac_address(self):
        """Getting device mac with more stability ensurance.

        Sometime, getting mac address is flaky that it returns None. Adding a
        loop to add more ensurance of getting correct mac address.
        """
        device_mac = None
        start_time = time.time()
        end_time = start_time + bt_default_timeout
        while not device_mac and time.time() < end_time:
            device_mac = self.ad_controller.droid.bluetoothGetLocalAddress()
        asserts.assert_true(device_mac, 'Can not get the MAC address')
        return device_mac

    def accept_call(self):
        return self.ad_controller.droid.telecomAcceptRingingCall(None)

    def end_call(self):
        return self.ad_controller.droid.telecomEndCall()

    def enter_pairing_mode(self):
        self.ad_controller.droid.bluetoothStartPairingHelper(True)
        return self.ad_controller.droid.bluetoothMakeDiscoverable()

    def next_track(self):
        return (self.ad_controller.droid.bluetoothMediaPassthrough("skipNext"))

    def pause(self):
        return self.ad_controller.droid.bluetoothMediaPassthrough("pause")

    def play(self):
        return self.ad_controller.droid.bluetoothMediaPassthrough("play")

    def power_off(self):
        return self.ad_controller.droid.bluetoothToggleState(False)

    def power_on(self):
        return self.ad_controller.droid.bluetoothToggleState(True)

    def previous_track(self):
        return (self.ad_controller.droid.bluetoothMediaPassthrough("skipPrev"))

    def reject_call(self):
        return self.ad_controller.droid.telecomCallDisconnect(
            self.ad_controller.droid.telecomCallGetCallIds()[0])

    def reset(self):
        return self.ad_controller.droid.bluetoothFactoryReset()

    def volume_down(self):
        target_step = self.ad_controller.droid.getMediaVolume() - 1
        target_step = max(target_step, 0)
        return self.ad_controller.droid.setMediaVolume(target_step)

    def volume_up(self):
        target_step = self.ad_controller.droid.getMediaVolume() + 1
        max_step = self.ad_controller.droid.getMaxMediaVolume()
        target_step = min(target_step, max_step)
        return self.ad_controller.droid.setMediaVolume(target_step)


class BluetoothHandsfreeAbstractDeviceFactory:
    """Generates a BluetoothHandsfreeAbstractDevice for any device controller.
    """

    _controller_abstract_devices = {
        'EarstudioReceiver': EarstudioReceiverBluetoothHandsfreeAbstractDevice,
        'JaybirdX3Earbuds': JaybirdX3EarbudsBluetoothHandsfreeAbstractDevice,
        'ParentDevice': PixelBudsBluetoothHandsfreeAbstractDevice,
        'AndroidDevice': AndroidHeadsetBluetoothHandsfreeAbstractDevice
    }

    def generate(self, controller):
        class_name = controller.__class__.__name__
        if class_name in self._controller_abstract_devices:
            return self._controller_abstract_devices[class_name](controller)
        else:
            validate_controller(controller, BluetoothHandsfreeAbstractDevice)
            return controller
