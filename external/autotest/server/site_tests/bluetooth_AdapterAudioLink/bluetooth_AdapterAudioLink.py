# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Bluetooth audio test to verify that audio connection can be established
and stay connected during audio. In the futuer the test can be expanded
to test audio quality etc.
"""
import logging
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.chameleon import chameleon_audio_helper
from autotest_lib.client.cros.chameleon import chameleon_audio_ids
from autotest_lib.server.cros.audio import audio_test
from autotest_lib.server.cros.bluetooth import bluetooth_adapter_tests
from autotest_lib.server.cros.multimedia import remote_facade_factory

class bluetooth_AdapterAudioLink(
        bluetooth_adapter_tests.BluetoothAdapterTests):
    """
    Bluetooth audio test to verify that audio connection can be established
    and stay connected during audio. In the futuer the test can be expanded
    to test audio quality etc.
    """
    version = 1

    def play_audio(self, binder, factory, widget_factory, num_iterations):
        """Test Body - playing audio and checking connection stability"""
        with chameleon_audio_helper.bind_widgets(binder):
            start_time = time.time()
            dut_device = binder._link._bt_adapter
            cham_bt_addr = binder._link._mac_address
            logging.info('DUT Address: %s', dut_device.get_address())
            logging.info('Playing an audio file. num inter %d', num_iterations)
            audio_facade = factory.create_audio_facade()

            # The raw audio file is short. In order to test long time,
            # Need to run it in iterations
            for x in range(num_iterations):
                is_connected = dut_device.device_is_connected(cham_bt_addr)
                logging.info('Playback iter %d, is chameleon connected %d',
                              x, is_connected)
                #Fail the test if the link was lost
                if not is_connected:
                     raise error.TestFail("Failure: BT link diconnection")
                file_path = '/usr/local/autotest/cros/audio/fix_440_16.raw'
                audio_facade.playback(client_path=file_path,
                                      data_format={'file_type': 'raw',
                                                   'sample_format': 'S16_LE',
                                                   'channel': 2,
                                                   'rate': 48000},
                                      blocking=False)
                # Playback for 5 seconds, which is shorter than the
                # duration for the file
                time.sleep(5)
                audio_facade.stop_playback()
            end_time = time.time()
            logging.info('Running time %0.1f seconds.', end_time - start_time)

    def run_once(self, host, num_iterations=12):
        """Running Bluetooth adapter tests for basic audio link

        @param host: the DUT, usually a chromebook
        @param num_iterations: the number of rounds to execute the test
        """
        self.host = host
        self.check_chameleon();

        # Setup Bluetooth widgets and their binder, but do not yet connect.
        audio_test.audio_test_requirement()
        factory = remote_facade_factory.RemoteFacadeFactory(
                host, results_dir=self.resultsdir, disable_arc=True)
        chameleon_board = self.host.chameleon
        if chameleon_board is None:
            raise error.TestNAError("No chameleon device is present")

        chameleon_board.setup_and_reset(self.outputdir)
        widget_factory = chameleon_audio_helper.AudioWidgetFactory(
                factory, host)
        source = widget_factory.create_widget(
                chameleon_audio_ids.CrosIds.BLUETOOTH_HEADPHONE)
        bluetooth_widget = widget_factory.create_widget(
                chameleon_audio_ids.PeripheralIds.BLUETOOTH_DATA_RX)
        binder = widget_factory.create_binder(
            source, bluetooth_widget)

        # Test body
        self.play_audio(binder, factory, widget_factory, num_iterations);

    def cleanup(self):
        return 0
