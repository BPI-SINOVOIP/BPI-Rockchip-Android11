# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Facade to access the audio-related functionality."""

import functools
import glob
import logging
import numpy as np
import os
import tempfile

from autotest_lib.client.cros import constants
from autotest_lib.client.cros.audio import audio_helper
from autotest_lib.client.cros.audio import cmd_utils
from autotest_lib.client.cros.audio import cras_dbus_utils
from autotest_lib.client.cros.audio import cras_utils
from autotest_lib.client.cros.audio import alsa_utils
from autotest_lib.client.cros.multimedia import audio_extension_handler


class AudioFacadeNativeError(Exception):
    """Error in AudioFacadeNative."""
    pass


def check_arc_resource(func):
    """Decorator function for ARC related functions in AudioFacadeNative."""
    @functools.wraps(func)
    def wrapper(instance, *args, **kwargs):
        """Wrapper for the methods to check _arc_resource.

        @param instance: Object instance.

        @raises: AudioFacadeNativeError if there is no ARC resource.

        """
        if not instance._arc_resource:
            raise AudioFacadeNativeError('There is no ARC resource.')
        return func(instance, *args, **kwargs)
    return wrapper


def file_contains_all_zeros(path):
    """Reads a file and checks whether the file contains all zeros."""
    with open(path) as f:
        binary = f.read()
        # Assume data is in 16 bit signed int format. The real format
        # does not matter though since we only care if there is nonzero data.
        np_array = np.fromstring(binary, dtype='<i2')
        return not np.any(np_array)


class AudioFacadeNative(object):
    """Facede to access the audio-related functionality.

    The methods inside this class only accept Python native types.

    """
    _CAPTURE_DATA_FORMATS = [
            dict(file_type='raw', sample_format='S16_LE',
                 channel=1, rate=48000),
            dict(file_type='raw', sample_format='S16_LE',
                 channel=2, rate=48000)]

    _PLAYBACK_DATA_FORMAT = dict(
            file_type='raw', sample_format='S16_LE', channel=2, rate=48000)

    _LISTEN_DATA_FORMATS = [
            dict(file_type='raw', sample_format='S16_LE',
                 channel=1, rate=16000)]

    def __init__(self, resource, arc_resource=None):
        """Initializes an audio facade.

        @param resource: A FacadeResource object.
        @param arc_resource: An ArcResource object.

        """
        self._resource = resource
        self._listener = None
        self._recorders = {}
        self._player = None
        self._counter = None
        self._loaded_extension_handler = None
        self._arc_resource = arc_resource


    @property
    def _extension_handler(self):
        """Multimedia test extension handler."""
        if not self._loaded_extension_handler:
            extension = self._resource.get_extension(
                    constants.AUDIO_TEST_EXTENSION)
            logging.debug('Loaded extension: %s', extension)
            self._loaded_extension_handler = (
                    audio_extension_handler.AudioExtensionHandler(extension))
        return self._loaded_extension_handler


    def get_audio_availability(self):
        """Returns the availability of chrome.audio API.

        @returns: True if chrome.audio exists
        """
        return self._extension_handler.get_audio_api_availability()


    def get_audio_devices(self):
        """Returns the audio devices from chrome.audio API.

        @returns: Checks docstring of get_audio_devices of AudioExtensionHandler.

        """
        return self._extension_handler.get_audio_devices()


    def set_chrome_active_volume(self, volume):
        """Sets the active audio output volume using chrome.audio API.

        @param volume: Volume to set (0~100).

        """
        self._extension_handler.set_active_volume(volume)


    def set_chrome_mute(self, mute):
        """Mutes the active audio output using chrome.audio API.

        @param mute: True to mute. False otherwise.

        """
        self._extension_handler.set_mute(mute)


    def get_chrome_active_volume_mute(self):
        """Gets the volume state of active audio output using chrome.audio API.

        @param returns: A tuple (volume, mute), where volume is 0~100, and mute
                        is True if node is muted, False otherwise.

        """
        return self._extension_handler.get_active_volume_mute()


    def set_chrome_active_node_type(self, output_node_type, input_node_type):
        """Sets active node type through chrome.audio API.

        The node types are defined in cras_utils.CRAS_NODE_TYPES.
        The current active node will be disabled first if the new active node
        is different from the current one.

        @param output_node_type: A node type defined in
                                 cras_utils.CRAS_NODE_TYPES. None to skip.
        @param input_node_type: A node type defined in
                                 cras_utils.CRAS_NODE_TYPES. None to skip

        """
        if output_node_type:
            node_id = cras_utils.get_node_id_from_node_type(
                    output_node_type, False)
            self._extension_handler.set_active_node_id(node_id)
        if input_node_type:
            node_id = cras_utils.get_node_id_from_node_type(
                    input_node_type, True)
            self._extension_handler.set_active_node_id(node_id)


    def check_audio_stream_at_selected_device(self):
        """Checks the audio output is at expected node"""
        output_device_name = cras_utils.get_selected_output_device_name()
        output_device_type = cras_utils.get_selected_output_device_type()
        logging.info("Output device name is %s", output_device_name)
        logging.info("Output device type is %s", output_device_type)
        alsa_utils.check_audio_stream_at_selected_device(output_device_name,
                                                         output_device_type)


    def cleanup(self):
        """Clean up the temporary files."""
        for path in glob.glob('/tmp/playback_*'):
            os.unlink(path)

        for path in glob.glob('/tmp/capture_*'):
            os.unlink(path)

        for path in glob.glob('/tmp/listen_*'):
            os.unlink(path)

        if self._recorders:
            for _, recorder in self._recorders:
                recorder.cleanup()
        self._recorders.clear()

        if self._player:
            self._player.cleanup()
        if self._listener:
            self._listener.cleanup()

        if self._arc_resource:
            self._arc_resource.cleanup()


    def playback(self, file_path, data_format, blocking=False, node_type=None,
                 block_size=None):
        """Playback a file.

        @param file_path: The path to the file.
        @param data_format: A dict containing data format including
                            file_type, sample_format, channel, and rate.
                            file_type: file type e.g. 'raw' or 'wav'.
                            sample_format: One of the keys in
                                           audio_data.SAMPLE_FORMAT.
                            channel: number of channels.
                            rate: sampling rate.
        @param blocking: Blocks this call until playback finishes.
        @param node_type: A Cras node type defined in cras_utils.CRAS_NODE_TYPES
                          that we like to pin at. None to have the playback on
                          active selected device.
        @param block_size: The number for frames per callback.

        @returns: True.

        @raises: AudioFacadeNativeError if data format is not supported.

        """
        logging.info('AudioFacadeNative playback file: %r. format: %r',
                     file_path, data_format)

        if data_format != self._PLAYBACK_DATA_FORMAT:
            raise AudioFacadeNativeError(
                    'data format %r is not supported' % data_format)

        device_id = None
        if node_type:
            device_id = int(cras_utils.get_device_id_from_node_type(
                    node_type, False))

        self._player = Player()
        self._player.start(file_path, blocking, device_id, block_size)

        return True


    def stop_playback(self):
        """Stops playback process."""
        self._player.stop()


    def start_recording(self, data_format, node_type=None, block_size=None):
        """Starts recording an audio file.

        Currently the format specified in _CAPTURE_DATA_FORMATS is the only
        formats.

        @param data_format: A dict containing:
                            file_type: 'raw'.
                            sample_format: 'S16_LE' for 16-bit signed integer in
                                           little-endian.
                            channel: channel number.
                            rate: sampling rate.
        @param node_type: A Cras node type defined in cras_utils.CRAS_NODE_TYPES
                          that we like to pin at. None to have the recording
                          from active selected device.
        @param block_size: The number for frames per callback.

        @returns: True

        @raises: AudioFacadeNativeError if data format is not supported, no
                 active selected node or the specified node is occupied.

        """
        logging.info('AudioFacadeNative record format: %r', data_format)

        if data_format not in self._CAPTURE_DATA_FORMATS:
            raise AudioFacadeNativeError(
                    'data format %r is not supported' % data_format)

        if node_type is None:
            device_id = None
            node_type = cras_utils.get_selected_input_device_type()
            if node_type is None:
                raise AudioFacadeNativeError('No active selected input node.')
        else:
            device_id = int(cras_utils.get_device_id_from_node_type(
                    node_type, True))

        if node_type in self._recorders:
            raise AudioFacadeNativeError(
                    'Node %s is already ocuppied' % node_type)

        self._recorders[node_type] = Recorder()
        self._recorders[node_type].start(data_format, device_id, block_size)

        return True


    def stop_recording(self, node_type=None):
        """Stops recording an audio file.
        @param node_type: A Cras node type defined in cras_utils.CRAS_NODE_TYPES
                          that we like to pin at. None to have the recording
                          from active selected device.

        @returns: The path to the recorded file.
                  None if capture device is not functional.

        @raises: AudioFacadeNativeError if no recording is started on
                 corresponding node.
        """
        if node_type is None:
            device_id = None
            node_type = cras_utils.get_selected_input_device_type()
            if node_type is None:
                raise AudioFacadeNativeError('No active selected input node.')
        else:
            device_id = int(cras_utils.get_device_id_from_node_type(
                    node_type, True))


        if node_type not in self._recorders:
            raise AudioFacadeNativeError(
                    'No recording is started on node %s' % node_type)

        recorder = self._recorders[node_type]
        recorder.stop()
        del self._recorders[node_type]

        file_path = recorder.file_path
        if file_contains_all_zeros(recorder.file_path):
            logging.error('Recorded file contains all zeros. '
                          'Capture device is not functional')
            return None

        return file_path


    def start_listening(self, data_format):
        """Starts listening to hotword for a given format.

        Currently the format specified in _CAPTURE_DATA_FORMATS is the only
        formats.

        @param data_format: A dict containing:
                            file_type: 'raw'.
                            sample_format: 'S16_LE' for 16-bit signed integer in
                                           little-endian.
                            channel: channel number.
                            rate: sampling rate.


        @returns: True

        @raises: AudioFacadeNativeError if data format is not supported.

        """
        logging.info('AudioFacadeNative record format: %r', data_format)

        if data_format not in self._LISTEN_DATA_FORMATS:
            raise AudioFacadeNativeError(
                    'data format %r is not supported' % data_format)

        self._listener = Listener()
        self._listener.start(data_format)

        return True


    def stop_listening(self):
        """Stops listening to hotword.

        @returns: The path to the recorded file.
                  None if hotwording is not functional.

        """
        self._listener.stop()
        if file_contains_all_zeros(self._listener.file_path):
            logging.error('Recorded file contains all zeros. '
                          'Hotwording device is not functional')
            return None
        return self._listener.file_path


    def set_selected_output_volume(self, volume):
        """Sets the selected output volume.

        @param volume: the volume to be set(0-100).

        """
        cras_utils.set_selected_output_node_volume(volume)


    def set_selected_node_types(self, output_node_types, input_node_types):
        """Set selected node types.

        The node types are defined in cras_utils.CRAS_NODE_TYPES.

        @param output_node_types: A list of output node types.
                                  None to skip setting.
        @param input_node_types: A list of input node types.
                                 None to skip setting.

        """
        cras_utils.set_selected_node_types(output_node_types, input_node_types)


    def get_selected_node_types(self):
        """Gets the selected output and input node types.

        @returns: A tuple (output_node_types, input_node_types) where each
                  field is a list of selected node types defined in
                  cras_utils.CRAS_NODE_TYPES.

        """
        return cras_utils.get_selected_node_types()


    def get_plugged_node_types(self):
        """Gets the plugged output and input node types.

        @returns: A tuple (output_node_types, input_node_types) where each
                  field is a list of plugged node types defined in
                  cras_utils.CRAS_NODE_TYPES.

        """
        return cras_utils.get_plugged_node_types()


    def dump_diagnostics(self, file_path):
        """Dumps audio diagnostics results to a file.

        @param file_path: The path to dump results.

        """
        audio_helper.dump_audio_diagnostics(file_path)


    def start_counting_signal(self, signal_name):
        """Starts counting DBus signal from Cras.

        @param signal_name: Signal of interest.

        """
        if self._counter:
            raise AudioFacadeNativeError('There is an ongoing counting.')
        self._counter = cras_dbus_utils.CrasDBusBackgroundSignalCounter()
        self._counter.start(signal_name)


    def stop_counting_signal(self):
        """Stops counting DBus signal from Cras.

        @returns: Number of signals starting from last start_counting_signal
                  call.

        """
        if not self._counter:
            raise AudioFacadeNativeError('Should start counting signal first')
        result = self._counter.stop()
        self._counter = None
        return result


    def wait_for_unexpected_nodes_changed(self, timeout_secs):
        """Waits for unexpected nodes changed signal.

        @param timeout_secs: Timeout in seconds for waiting.

        """
        cras_dbus_utils.wait_for_unexpected_nodes_changed(timeout_secs)


    @check_arc_resource
    def start_arc_recording(self):
        """Starts recording using microphone app in container."""
        self._arc_resource.microphone.start_microphone_app()


    @check_arc_resource
    def stop_arc_recording(self):
        """Checks the recording is stopped and gets the recorded path.

        The recording duration of microphone app is fixed, so this method just
        copies the recorded result from container to a path on Cros device.

        """
        _, file_path = tempfile.mkstemp(prefix='capture_', suffix='.amr-nb')
        self._arc_resource.microphone.stop_microphone_app(file_path)
        return file_path


    @check_arc_resource
    def set_arc_playback_file(self, file_path):
        """Copies the audio file to be played into container.

        User should call this method to put the file into container before
        calling start_arc_playback.

        @param file_path: Path to the file to be played on Cros host.

        @returns: Path to the file in container.

        """
        return self._arc_resource.play_music.set_playback_file(file_path)


    @check_arc_resource
    def start_arc_playback(self, path):
        """Start playback through Play Music app.

        Before calling this method, user should call set_arc_playback_file to
        put the file into container.

        @param path: Path to the file in container.

        """
        self._arc_resource.play_music.start_playback(path)


    @check_arc_resource
    def stop_arc_playback(self):
        """Stop playback through Play Music app."""
        self._arc_resource.play_music.stop_playback()


class RecorderError(Exception):
    """Error in Recorder."""
    pass


class Recorder(object):
    """The class to control recording subprocess.

    Properties:
        file_path: The path to recorded file. It should be accessed after
                   stop() is called.

    """
    def __init__(self):
        """Initializes a Recorder."""
        _, self.file_path = tempfile.mkstemp(prefix='capture_', suffix='.raw')
        self._capture_subprocess = None


    def start(self, data_format, pin_device, block_size):
        """Starts recording.

        Starts recording subprocess. It can be stopped by calling stop().

        @param data_format: A dict containing:
                            file_type: 'raw'.
                            sample_format: 'S16_LE' for 16-bit signed integer in
                                           little-endian.
                            channel: channel number.
                            rate: sampling rate.
        @param pin_device: A integer of device id to record from.
        @param block_size: The number for frames per callback.
        """
        self._capture_subprocess = cmd_utils.popen(
                cras_utils.capture_cmd(
                        capture_file=self.file_path, duration=None,
                        channels=data_format['channel'],
                        rate=data_format['rate'],
                        pin_device=pin_device, block_size=block_size))


    def stop(self):
        """Stops recording subprocess."""
        if self._capture_subprocess.poll() is None:
            self._capture_subprocess.terminate()
        else:
            raise RecorderError(
                    'Recording process was terminated unexpectedly.')


    def cleanup(self):
        """Cleanup the resources.

        Terminates the recording process if needed.

        """
        if self._capture_subprocess and self._capture_subprocess.poll() is None:
            self._capture_subprocess.terminate()


class PlayerError(Exception):
    """Error in Player."""
    pass


class Player(object):
    """The class to control audio playback subprocess.

    Properties:
        file_path: The path to the file to play.

    """
    def __init__(self):
        """Initializes a Player."""
        self._playback_subprocess = None


    def start(self, file_path, blocking, pin_device, block_size):
        """Starts playing.

        Starts playing subprocess. It can be stopped by calling stop().

        @param file_path: The path to the file.
        @param blocking: Blocks this call until playback finishes.
        @param pin_device: A integer of device id to play on.
        @param block_size: The number for frames per callback.

        """
        self._playback_subprocess = cras_utils.playback(
                blocking, playback_file=file_path, pin_device=pin_device,
                block_size=block_size)


    def stop(self):
        """Stops playback subprocess."""
        cmd_utils.kill_or_log_returncode(self._playback_subprocess)


    def cleanup(self):
        """Cleanup the resources.

        Terminates the playback process if needed.

        """
        self.stop()


class ListenerError(Exception):
    """Error in Listener."""
    pass


class Listener(object):
    """The class to control listening subprocess.

    Properties:
        file_path: The path to recorded file. It should be accessed after
                   stop() is called.

    """
    def __init__(self):
        """Initializes a Listener."""
        _, self.file_path = tempfile.mkstemp(prefix='listen_', suffix='.raw')
        self._capture_subprocess = None


    def start(self, data_format):
        """Starts listening.

        Starts listening subprocess. It can be stopped by calling stop().

        @param data_format: A dict containing:
                            file_type: 'raw'.
                            sample_format: 'S16_LE' for 16-bit signed integer in
                                           little-endian.
                            channel: channel number.
                            rate: sampling rate.

        @raises: ListenerError: If listening subprocess is terminated
                 unexpectedly.

        """
        self._capture_subprocess = cmd_utils.popen(
                cras_utils.listen_cmd(
                        capture_file=self.file_path, duration=None,
                        channels=data_format['channel'],
                        rate=data_format['rate']))


    def stop(self):
        """Stops listening subprocess."""
        if self._capture_subprocess.poll() is None:
            self._capture_subprocess.terminate()
        else:
            raise ListenerError(
                    'Listening process was terminated unexpectedly.')


    def cleanup(self):
        """Cleanup the resources.

        Terminates the listening process if needed.

        """
        if self._capture_subprocess and self._capture_subprocess.poll() is None:
            self._capture_subprocess.terminate()
