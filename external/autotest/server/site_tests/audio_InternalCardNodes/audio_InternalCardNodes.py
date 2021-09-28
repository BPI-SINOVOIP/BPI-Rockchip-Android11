# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This is a server side test to check nodes created for internal card."""

from autotest_lib.client.cros.chameleon import audio_test_utils
from autotest_lib.server.cros.audio import audio_test


class audio_InternalCardNodes(audio_test.AudioTest):
    """Server side test to check audio nodes for internal card.

    This test talks to a Chameleon board and a Cros device to verify
    audio nodes created for internal cards are correct.

    """
    version = 1
    _jack_plugger = None

    def cleanup(self):
        """Cleanup for this test."""
        if self._jack_plugger is not None:
            self._jack_plugger.plug()
        super(audio_InternalCardNodes, self).cleanup()

    def get_expected_nodes(self, plugged):
        """Gets expected nodes should should be created for internal cards.

        @param plugged: True for plugged state, false otherwise.
        @returns:
            a tuple (output, input) containing lists of expected input and
            output nodes.
        """
        nodes = ([], ['POST_DSP_LOOPBACK', 'POST_MIX_LOOPBACK'])
        if plugged:
            # Checks whether line-out or headphone is detected.
            hp_jack_node_type = audio_test_utils.check_hp_or_lineout_plugged(
                    self.facade)
            nodes[0].append(hp_jack_node_type)
            nodes[1].append('MIC')
        if audio_test_utils.has_internal_speaker(self.host):
            nodes[0].append('INTERNAL_SPEAKER')
        if audio_test_utils.has_internal_microphone(self.host):
            nodes[1].extend(
                    audio_test_utils.get_plugged_internal_mics(self.host))
        if audio_test_utils.has_hotwording(self.host):
            nodes[1].append('HOTWORD')
        if audio_test_utils.has_echo_reference(self.host):
            nodes[1].append('ECHO_REFERENCE')
        return nodes

    def run_once(self, plug=True):
        """Runs InternalCardNodes test."""
        if not audio_test_utils.has_audio_jack(self.host):
            audio_test_utils.check_plugged_nodes(
                    self.facade, self.get_expected_nodes(False))
            return

        if not plug:
            self._jack_plugger = self.host.chameleon.get_audio_board(
            ).get_jack_plugger()
            self._jack_plugger.unplug()

        audio_test_utils.check_plugged_nodes(self.facade,
                                             self.get_expected_nodes(plug))
