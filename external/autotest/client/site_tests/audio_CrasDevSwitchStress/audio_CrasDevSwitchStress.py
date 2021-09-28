# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dbus
import logging
import os
import re
import subprocess
import time

from autotest_lib.client.bin import test
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.audio import audio_helper
from autotest_lib.client.cros.audio import cras_utils

_STREAM_TYPE_INPUT_APM = 0
_STREAM_TYPE_OUTPUT = 1

class audio_CrasDevSwitchStress(test.test):
    """
    Checks if output buffer will drift to super high level when
    audio devices switch repeatedly.
    """
    version = 1
    _LOOP_COUNT = 150
    _INPUT_BUFFER_LEVEL = '.*?READ_AUDIO.*?dev:(\d+).*hw_level.*?(\d+).*?'
    _OUTPUT_BUFFER_LEVEL = '.*?FILL_AUDIO.*?dev:(\d+).*hw_level.*?(\d+).*?'
    _CHECK_PERIOD_TIME_SECS = 0.5
    _SILENT_OUTPUT_DEV_ID = 2
    _STREAM_BLOCK_SIZE = 480

    """
    Buffer level of input device should stay between 0 and block size.
    Buffer level of output device should between 1 to 2 times of block size.
    Device switching sometimes cause the buffer level to grow more then
    the ideal range, so we use tripple block size here.
    """
    _INPUT_BUFFER_DRIFT_CRITERIA = 3 * _STREAM_BLOCK_SIZE
    _OUTPUT_BUFFER_DRIFT_CRITERIA = 3 * _STREAM_BLOCK_SIZE

    def cleanup(self):
        """Remove all streams for testing."""
        if self._streams:
            # Clean up all streams.
            while len(self._streams) > 0:
                self._streams[0].kill()
                self._streams.remove(self._streams[0])

        super(audio_CrasDevSwitchStress, self).cleanup()

    def _new_stream(self, stream_type, node_pinned):
        """
        Runs new stream by cras_test_client.
        Args:
            stream_type: _STREAM_TYPE_INPUT_APM or _STREAM_TYPE_OUTPUT
            node_pinned: if not None then create the new stream that
                pinned to this node.
        Returns:
            New process that runs a CRAS client stream.
        """
        if stream_type == _STREAM_TYPE_INPUT_APM:
            cmd = ['cras_test_client', '--capture_file', '/dev/null',
                   '--effects', 'aec']
        else:
            cmd = ['cras_test_client', '--playback_file', '/dev/zero']

        cmd += ['--block_size', str(self._STREAM_BLOCK_SIZE)]

        if node_pinned and node_pinned['Id']:
            dev_id = node_pinned['Id'] >> 32
            if stream_type == _STREAM_TYPE_INPUT_APM:
                if node_pinned['IsInput']:
                    cmd += ['--pin_device', str(dev_id)]
            elif not node_pinned['IsInput']:
                cmd += ['--pin_device', str(dev_id)]

        return subprocess.Popen(cmd)

    def _get_buffer_level(self, match_str, dev_id):
        """
        Gets a rough number about current buffer level.

        Args:
            match_str: regular expression to match device buffer level.
            dev_id: audio device id in CRAS.

        Returns:
            The current buffer level.
        """
        proc = subprocess.Popen(['cras_test_client', '--dump_a'],
                                stdout=subprocess.PIPE)
        output, err = proc.communicate()
        buffer_level = 0
        for line in output.split('\n'):
            search = re.match(match_str, line)
            if search:
                if dev_id != int(search.group(1)):
                    continue
                tmp = int(search.group(2))
                if tmp > buffer_level:
                    buffer_level = tmp
        return buffer_level

    def _check_buffer_level(self, node):
        """Checks if current buffer level of node stay in criteria."""
        if node['IsInput']:
            match_str = self._INPUT_BUFFER_LEVEL
            criteria = self._INPUT_BUFFER_DRIFT_CRITERIA
        else:
            match_str = self._OUTPUT_BUFFER_LEVEL
            criteria = self._OUTPUT_BUFFER_DRIFT_CRITERIA

        dev_id = node['Id'] >> 32
        buffer_level = self._get_buffer_level(match_str, dev_id)

        logging.debug("Max buffer level: %d on dev %d", buffer_level, dev_id)
        if buffer_level > criteria:
            audio_helper.dump_audio_diagnostics(
                    os.path.join(self.resultsdir, "audio_diagnostics.txt"))
            raise error.TestFail('Buffer level %d drift too high on %s node'
                                 ' with dev id %d' %
                                 (buffer_level, node['Type'], dev_id))

    def _get_cras_pid(self):
        """Gets the pid of CRAS, used to check if CRAS crashes and respawn."""
        pid = utils.system_output('pgrep cras$ 2>&1', retain_output=True,
                                  ignore_status=True).strip()
        try:
            pid = int(pid)
        except ValueError, e:  # empty or invalid string
            raise error.TestFail('CRAS not running')

    def _switch_to_node(self, node):
        """
        Switches CRAS input or output active node to target node.
        Args:
            node: if not None switch CRAS input/output active node to it
        """
        if node == None:
            return
        if node['IsInput']:
            cras_utils.set_active_input_node(node['Id'])
        else:
            cras_utils.set_active_output_node(node['Id'])

    def run_once(self, type_a=None, type_b=None, pinned_type=None):
        """
        Setup the typical streams used for hangout, one input stream with APM
        plus an output stream. If pinned_type is not None, set up an additional
        stream that pinned to the first node of pinned_type.
        The test repeatedly switch active nodes between the first nodes of
        type_a and type_b to verify there's no crash and buffer level stay
        in reasonable range.

        Args:
            type_a: node type to match a CRAS node
            type_b: node type to match a CRAS node
            pinned_type: node type to match a CRAS node
        """
        node_a = None
        node_b = None
        node_pinned = None
        self._streams = []

        cras_pid = self._get_cras_pid()

        try:
            nodes = cras_utils.get_cras_nodes()
        except dbus.DBusException as e:
            logging.error("Get CRAS nodes failure.");
            raise error.TestFail("No reply from CRAS for get nodes request.")

        for node in nodes:
            if type_a and node['Type'] == type_a:
                node_a = node
            elif type_b and node['Type'] == type_b:
                node_b = node

            if pinned_type and node['Type'] == pinned_type:
                node_pinned = node

        if not (node_a and node_b):
            raise error.TestNAError("No output nodes pair to switch.")

        if node_pinned:
            if node_pinned['IsInput']:
                self._streams.append(
                        self._new_stream(_STREAM_TYPE_INPUT_APM, node_pinned))
            else:
                self._streams.append(
                        self._new_stream(_STREAM_TYPE_OUTPUT, node_pinned))

        self._streams.append(self._new_stream(_STREAM_TYPE_OUTPUT, None))
        self._streams.append(self._new_stream(_STREAM_TYPE_INPUT_APM, None))

        loop_count = 0
        past_time = time.time()

        try:
            while loop_count < self._LOOP_COUNT:
                # Switch active input/output node in each loop.
                if loop_count % 2 == 0:
                    self._switch_to_node(node_a)
                else:
                    self._switch_to_node(node_b)

                time.sleep(0.2)
                now = time.time()

                # Check buffer level.
                if now - past_time > self._CHECK_PERIOD_TIME_SECS:
                    past_time = now
                    self._check_buffer_level(node_a)
                    self._check_buffer_level(node_b)

                loop_count += 1
                if cras_pid != self._get_cras_pid():
                    raise error.TestFail("CRAS crashed and respawned.")
        except dbus.DBusException as e:
            logging.exception(e)
            raise error.TestFail("CRAS may have crashed.")
