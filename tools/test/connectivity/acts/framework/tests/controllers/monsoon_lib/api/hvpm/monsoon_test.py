#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import unittest

import mock

from acts.controllers.monsoon_lib.api.hvpm.monsoon import Monsoon

ASSEMBLY_LINE_IMPORT = ('acts.controllers.monsoon_lib.api.hvpm.monsoon'
                        '.AssemblyLineBuilder')
DOWNSAMPLER_IMPORT = ('acts.controllers.monsoon_lib.api.hvpm.monsoon'
                      '.DownSampler')
TEE_IMPORT = 'acts.controllers.monsoon_lib.api.hvpm.monsoon.Tee'

# The position in the call tuple that represents the args array.
ARGS = 0


class BaseMonsoonTest(unittest.TestCase):
    """Tests acts.controllers.monsoon_lib.api.monsoon.Monsoon."""

    SERIAL = 534147

    def setUp(self):
        self.sleep_patch = mock.patch('time.sleep')
        self.sleep_patch.start()

        self.mp_manager_patch = mock.patch('multiprocessing.Manager')
        self.mp_manager_patch.start()

        proxy_mock = mock.MagicMock()
        proxy_mock.Protocol.getValue.return_value = 1048576 * 4
        self.monsoon_proxy = mock.patch(
            'Monsoon.HVPM.Monsoon', return_value=proxy_mock)
        self.monsoon_proxy.start()

    def tearDown(self):
        self.sleep_patch.stop()
        self.monsoon_proxy.stop()
        self.mp_manager_patch.stop()

    def test_status_fills_status_packet_first(self):
        """Tests fillStatusPacket() is called before returning the status.

        If this is not done, the status packet returned is stale.
        """

        def verify_call_order():
            if not self.monsoon_proxy().fillStatusPacket.called:
                self.fail('fillStatusPacket must be called first.')

        monsoon = Monsoon(self.SERIAL)
        monsoon._mon.statusPacket.side_effect = verify_call_order

        status_packet = monsoon.status

        self.assertEqual(
            status_packet, monsoon._mon.statusPacket,
            'monsoon.status MUST return '
            'MonsoonProxy.statusPacket.')

    @mock.patch(DOWNSAMPLER_IMPORT)
    @mock.patch(ASSEMBLY_LINE_IMPORT)
    def test_measure_power_downsample_skipped_if_hz_unset(
            self, _, downsampler):
        """Tests the DownSampler transformer is skipped if it is not needed."""
        monsoon = Monsoon(self.SERIAL)
        unimportant_kwargs = {'output_path': None, 'transformers': None}

        monsoon.measure_power(1, hz=5000, **unimportant_kwargs)

        self.assertFalse(
            downsampler.called,
            'A Downsampler should not have been created for a the default '
            'sampling frequency.')

    @mock.patch(DOWNSAMPLER_IMPORT)
    @mock.patch(ASSEMBLY_LINE_IMPORT)
    def test_measure_power_downsamples_immediately_after_sampling(
            self, assembly_line, downsampler):
        """Tests """
        monsoon = Monsoon(self.SERIAL)
        unimportant_kwargs = {'output_path': None, 'transformers': None}

        monsoon.measure_power(1, hz=500, **unimportant_kwargs)

        downsampler.assert_called_once_with(int(round(5000 / 500)))
        # Assert Downsampler() is the first element within the list.
        self.assertEqual(assembly_line().into.call_args_list[0][ARGS][0],
                         downsampler())

    @mock.patch(TEE_IMPORT)
    @mock.patch(ASSEMBLY_LINE_IMPORT)
    def test_measure_power_tee_skipped_if_ouput_path_not_set(self, _, tee):
        """Tests the Tee Transformer is not added when not needed."""
        monsoon = Monsoon(self.SERIAL)
        unimportant_kwargs = {'hz': 5000, 'transformers': None}

        monsoon.measure_power(1, output_path=None, **unimportant_kwargs)

        self.assertFalse(
            tee.called,
            'A Tee Transformer should not have been created for measure_power '
            'without an output_path.')

    @mock.patch(TEE_IMPORT)
    @mock.patch(ASSEMBLY_LINE_IMPORT)
    def test_measure_power_tee_is_added_to_assembly_line(
            self, assembly_line, tee):
        """Tests Tee is added to the assembly line with the correct path."""
        monsoon = Monsoon(self.SERIAL)
        unimportant_kwargs = {'hz': 5000, 'transformers': None}

        monsoon.measure_power(1, output_path='foo', **unimportant_kwargs)

        tee.assert_called_once_with('foo', 0)
        # Assert Tee() is the first element within the assembly into calls.
        self.assertEqual(assembly_line().into.call_args_list[0][ARGS][0],
                         tee())

    @mock.patch(ASSEMBLY_LINE_IMPORT)
    def test_measure_power_transformers_are_added(self, assembly_line):
        """Tests additional transformers are added to the assembly line."""
        monsoon = Monsoon(self.SERIAL)
        unimportant_kwargs = {'hz': 5000, 'output_path': None}
        expected_transformers = [mock.Mock(), mock.Mock()]

        monsoon.measure_power(
            1, transformers=expected_transformers, **unimportant_kwargs)

        self.assertEqual(expected_transformers[0],
                         assembly_line().into.call_args_list[-2][ARGS][0])
        self.assertEqual(expected_transformers[1],
                         assembly_line().into.call_args_list[-1][ARGS][0])


if __name__ == '__main__':
    unittest.main()
