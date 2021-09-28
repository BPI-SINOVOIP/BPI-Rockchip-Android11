#!/usr/bin/env python3
#
# Copyright (C) 2019 The Android Open Source Project
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
"""Run sine wave audio quality test from Android to headset over 5 codecs."""
import time

from acts import asserts
from acts.signals import TestPass
from acts.test_utils.bt.A2dpBaseTest import A2dpBaseTest
from acts.test_utils.bt import bt_constants
from acts.test_utils.bt.loggers.bluetooth_metric_logger import BluetoothMetricLogger

DEFAULT_THDN_THRESHOLD = .1
DEFAULT_ANOMALIES_THRESHOLD = 0


class BtCodecSweepTest(A2dpBaseTest):

    def setup_class(self):
        super().setup_class()
        self.bt_logger = BluetoothMetricLogger.for_test_case()
        self.start_time = time.time()

    def setup_test(self):
        super().setup_test()
        req_params = ['dut',
                      'phone_music_file_dir',
                      'host_music_file_dir',
                      'music_file_name',
                      'audio_params']
        opt_params = ['RelayDevice', 'codecs']
        self.unpack_userparams(req_params, opt_params)
        for codec in self.user_params.get('codecs', []):
            self.generate_test_case(codec)
        self.log.info('Sleep to ensure connection...')
        time.sleep(30)

    def teardown_test(self):
        # TODO(aidanhb): Modify abstract device classes to make this generic.
        self.bt_device.earstudio_controller.clean_up()

    def print_results_summary(self, thdn_results, anomaly_results):
        channnel_results = zip(thdn_results, anomaly_results)
        for ch_no, result in enumerate(channnel_results):
            self.log.info('======CHANNEL %s RESULTS======' % ch_no)
            self.log.info('\tTHD+N: %s%%' % (result[0] * 100))
            self.log.info('\tANOMALIES: %s' % len(result[1]))
            for anom in result[1]:
                self.log.info('\t\tAnomaly from %s to %s of duration %s' % (
                    anom[0], anom[1], anom[1] - anom[0]
                ))

    def base_codec_test(self, codec_type, sample_rate, bits_per_sample,
                        channel_mode):
        """Base test flow that all test cases in this class will follow.
        Args:
            codec_type (str): the desired codec type. For reference, see
                test_utils.bt.bt_constants.codec_types
            sample_rate (int|str): the desired sample rate. For reference, see
                test_utils.bt.bt_constants.sample_rates
            bits_per_sample (int|str): the desired bits per sample. For
                reference, see test_utils.bt.bt_constants.bits_per_samples
            channel_mode (str): the desired channel mode. For reference, see
                test_utils.bt.bt_constants.channel_modes
        Raises:
            TestPass, TestFail, or TestError test signal.
        """
        self.stream_music_on_codec(codec_type=codec_type,
                                   sample_rate=sample_rate,
                                   bits_per_sample=bits_per_sample,
                                   channel_mode=channel_mode)
        proto = self.run_analysis_and_generate_proto(
                codec_type=codec_type,
                sample_rate=sample_rate,
                bits_per_sample=bits_per_sample,
                channel_mode=channel_mode)
        self.raise_pass_fail(proto)

    def generate_test_case(self, codec_config):
        def test_case_fn(inst):
            inst.stream_music_on_codec(**codec_config)
            proto = inst.run_analysis_and_generate_proto(**codec_config)
            inst.raise_pass_fail(proto)
        test_case_name = 'test_{}'.format(
            '_'.join([str(codec_config[key]) for key in [
                'codec_type',
                'sample_rate',
                'bits_per_sample',
                'channel_mode',
                'codec_specific_1'
            ] if key in codec_config])
        )
        if hasattr(self, test_case_name):
            self.log.warning('Test case %s already defined. Skipping '
                             'assignment...')
        else:
            bound_test_case = test_case_fn.__get__(self, BtCodecSweepTest)
            setattr(self, test_case_name, bound_test_case)

    def run_analysis_and_generate_proto(self, codec_type, sample_rate,
                                        bits_per_sample, channel_mode):
        """Analyze audio and generate a results protobuf.

        Args:
            codec_type: The codec type config to store in the proto.
            sample_rate: The sample rate config to store in the proto.
            bits_per_sample: The bits per sample config to store in the proto.
            channel_mode: The channel mode config to store in the proto.
        Returns:
             dict: Dictionary with key 'proto' mapping to serialized protobuf,
               'proto_ascii' mapping to human readable protobuf info, and 'test'
               mapping to the test class name that generated the results.
        """
        # Analyze audio and log results.
        thdn_results = self.run_thdn_analysis()
        anomaly_results = self.run_anomaly_detection()
        self.print_results_summary(thdn_results, anomaly_results)

        # Populate protobuf
        test_case_proto = self.bt_logger.proto_module.BluetoothAudioTestResult()
        audio_data_proto = test_case_proto.data_points.add()

        audio_data_proto.timestamp_since_beginning_of_test_millis = int(
                (time.time() - self.start_time) * 1000)
        audio_data_proto.audio_streaming_duration_millis = (
                int(self.mic.get_last_record_duration_millis()))
        audio_data_proto.attenuation_db = 0
        audio_data_proto.total_harmonic_distortion_plus_noise_percent = float(
            thdn_results[0])
        audio_data_proto.audio_glitches_count = len(anomaly_results[0])

        codec_proto = test_case_proto.a2dp_codec_config
        codec_proto.codec_type = bt_constants.codec_types[codec_type]
        codec_proto.sample_rate = int(sample_rate)
        codec_proto.bits_per_sample = int(bits_per_sample)
        codec_proto.channel_mode = bt_constants.channel_modes[channel_mode]

        self.bt_logger.add_config_data_to_proto(test_case_proto,
                                                self.android,
                                                self.bt_device)

        self.bt_logger.add_proto_to_results(test_case_proto,
                                            self.__class__.__name__)

        return self.bt_logger.get_proto_dict(self.__class__.__name__,
                                             test_case_proto)

    def raise_pass_fail(self, extras=None):
        """Raise pass or fail test signal based on analysis results."""
        try:
            anomalies_threshold = self.user_params.get(
                'anomalies_threshold', DEFAULT_ANOMALIES_THRESHOLD)
            asserts.assert_true(len(self.metrics['anomalies'][0]) <=
                                anomalies_threshold,
                                'Number of glitches exceeds threshold.',
                                extras=extras)
            thdn_threshold = self.user_params.get('thdn_threshold',
                                                  DEFAULT_THDN_THRESHOLD)
            asserts.assert_true(self.metrics['thdn'][0] <= thdn_threshold,
                                'THD+N exceeds threshold.',
                                extras=extras)
        except IndexError as e:
            self.log.error('self.raise_pass_fail called before self.analyze. '
                           'Anomaly and THD+N results not populated.')
            raise e
        raise TestPass('Test passed.', extras=extras)

    def test_SBC_44100_16_STEREO(self):
        self.base_codec_test(codec_type='SBC',
                             sample_rate=44100,
                             bits_per_sample=16,
                             channel_mode='STEREO')

    def test_AAC_44100_16_STEREO(self):
        self.base_codec_test(codec_type='AAC',
                             sample_rate=44100,
                             bits_per_sample=16,
                             channel_mode='STEREO')

    def test_APTX_44100_16_STEREO(self):
        self.base_codec_test(codec_type='APTX',
                             sample_rate=44100,
                             bits_per_sample=16,
                             channel_mode='STEREO')

    def test_APTX_HD_48000_24_STEREO(self):
        self.base_codec_test(codec_type='APTX-HD',
                             sample_rate=48000,
                             bits_per_sample=24,
                             channel_mode='STEREO')

    def test_LDAC_44100_16_STEREO(self):
        self.base_codec_test(codec_type='LDAC',
                             sample_rate=44100,
                             bits_per_sample=16,
                             channel_mode='STEREO')
