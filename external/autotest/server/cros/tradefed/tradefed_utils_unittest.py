# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

import tradefed_utils


def _load_data(filename):
    """Loads the test data of the given file name."""
    with open(os.path.join(os.path.dirname(os.path.realpath(__file__)),
                           'tradefed_utils_unittest_data', filename), 'r') as f:
        return f.read()


class TradefedTestTest(unittest.TestCase):
    """Unittest for tradefed_utils."""

    def test_parse_tradefed_result(self):
        """Test for parse_tradefed_result."""

        waivers = set([
            'android.app.cts.SystemFeaturesTest#testUsbAccessory',
            'android.widget.cts.GridViewTest#testSetNumColumns',
        ])

        # b/35605415 and b/36520623
        # http://pantheon/storage/browser/chromeos-autotest-results/108103986-chromeos-test/
        # CTS: Tradefed may split a module to multiple chunks.
        # Besides, the module name may not end with "TestCases".
        waived, _ = tradefed_utils.parse_tradefed_result(
            _load_data('CtsHostsideNetworkTests.txt'),
            waivers=waivers)
        self.assertEquals(0, len(waived))

        # b/35530394
        # http://pantheon/storage/browser/chromeos-autotest-results/108291418-chromeos-test/
        # Crashed, but the automatic retry by tradefed executed the rest.
        waived, _ = tradefed_utils.parse_tradefed_result(
            _load_data('CtsMediaTestCases.txt'),
            waivers=waivers)
        self.assertEquals(0, len(waived))

        # b/35530394
        # http://pantheon/storage/browser/chromeos-autotest-results/106540705-chromeos-test/
        # Crashed in the middle, and the device didn't came back.
        waived, _ = tradefed_utils.parse_tradefed_result(
            _load_data('CtsSecurityTestCases.txt'),
            waivers=waivers)
        self.assertEquals(0, len(waived))

        # b/36629187
        # http://pantheon/storage/browser/chromeos-autotest-results/108855595-chromeos-test/
        # Crashed in the middle. Tradefed decided not to continue.
        waived, _ = tradefed_utils.parse_tradefed_result(
            _load_data('CtsViewTestCases.txt'),
            waivers=waivers)
        self.assertEquals(0, len(waived))

        # b/36375690
        # http://pantheon/storage/browser/chromeos-autotest-results/109040174-chromeos-test/
        # Mixture of real failures and waivers.
        waived, _ = tradefed_utils.parse_tradefed_result(
            _load_data('CtsAppTestCases.txt'),
            waivers=waivers)
        self.assertEquals(1, len(waived))
        # ... and the retry of the above failing iteration.
        waived, _ = tradefed_utils.parse_tradefed_result(
            _load_data('CtsAppTestCases-retry.txt'),
            waivers=waivers)
        self.assertEquals(1, len(waived))

        # http://pantheon/storage/browser/chromeos-autotest-results/116875512-chromeos-test/
        # When a test case crashed during teardown, tradefed prints the "fail"
        # message twice. Tolerate it and still return an (inconsistent) count.
        waived, _ = tradefed_utils.parse_tradefed_result(
            _load_data('CtsWidgetTestCases.txt'),
            waivers=waivers)
        self.assertEquals(1, len(waived))

        # http://pantheon/storage/browser/chromeos-autotest-results/117914707-chromeos-test/
        # When a test case unrecoverably crashed during teardown, tradefed
        # prints the "fail" and failure summary message twice. Tolerate it.
        waived, _ = tradefed_utils.parse_tradefed_result(
            _load_data('CtsPrintTestCases.txt'),
            waivers=waivers)
        self.assertEquals(0, len(waived))

        gts_waivers = set([
            ('com.google.android.placement.gts.CoreGmsAppsTest#' +
                'testCoreGmsAppsPreloaded'),
            ('com.google.android.placement.gts.CoreGmsAppsTest#' +
                'testGoogleDuoPreloaded'),
            'com.google.android.placement.gts.UiPlacementTest#testPlayStore'
        ])

        # crbug.com/748116
        # http://pantheon/storage/browser/chromeos-autotest-results/130080763-chromeos-test/
        # 3 ABIS: x86, x86_64, and armeabi-v7a
        waived, _ = tradefed_utils.parse_tradefed_result(
            _load_data('GtsPlacementTestCases.txt'),
            waivers=gts_waivers)
        self.assertEquals(9, len(waived))

        # b/64095702
        # http://pantheon/storage/browser/chromeos-autotest-results/130211812-chromeos-test/
        # The result of the last chunk not reported by tradefed.
        # The actual dEQP log is too big, hence the test data here is trimmed.
        waived, _ = tradefed_utils.parse_tradefed_result(
            _load_data('CtsDeqpTestCases-trimmed.txt'),
            waivers=waivers)
        self.assertEquals(0, len(waived))

        # b/80160772
        # http://pantheon/storage/browser/chromeos-autotest-results/201962931-kkanchi/
        # The newer tradefed requires different parsing to count waivers.
        waived, _ = tradefed_utils.parse_tradefed_result(
            _load_data('CtsAppTestCases_P_simplified.txt'),
            waivers=waivers)
        self.assertEquals(1, len(waived))

        # b/66899135, tradefed may reported inaccuratly with `list results`.
        # Check if summary section shows that the result is inacurrate.
        _, accurate = tradefed_utils.parse_tradefed_result(
            _load_data('CtsAppTestCases_P_simplified.txt'),
            waivers=waivers)
        self.assertTrue(accurate)

        _, accurate = tradefed_utils.parse_tradefed_result(
            _load_data('CtsDeqpTestCases-trimmed-inaccurate.txt'),
            waivers=waivers)
        self.assertFalse(accurate)

    def test_get_test_result_xml_path(self):
        path = tradefed_utils.get_test_result_xml_path(os.path.join(
            os.path.dirname(os.path.realpath(__file__)),
            'tradefed_utils_unittest_data', 'results'))
        self.assertEqual(path, os.path.join(
            os.path.dirname(os.path.realpath(__file__)),
            'tradefed_utils_unittest_data', 'results', '2019.11.07_10.14.55',
            'test_result.xml'))

    def test_get_perf_metrics_from_test_result_xml(self):
        perf_result = tradefed_utils.get_perf_metrics_from_test_result_xml(
            os.path.join(os.path.dirname(os.path.realpath(__file__)),
                         'tradefed_utils_unittest_data', 'test_result.xml'),
            os.path.join('/', 'resultsdir'))
        expected_result = [
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioRecordTest'
                            '#testAudioRecordLocalMono16Bit',
             'value': '7.1688596491228065', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioRecordTest'
                            '#testAudioRecordLocalMono16BitShort',
             'value': '2.5416666666666665', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioRecordTest'
                            '#testAudioRecordLocalNonblockingStereoFloat',
             'value': '1.75', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioRecordTest'
                            '#testAudioRecordMonoFloat',
             'value': '12.958881578947368', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioRecordTest'
                            '#testAudioRecordResamplerMono8Bit',
             'value': '0.0', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioRecordTest'
                            '#testAudioRecordResamplerStereo8Bit',
             'value': '3.5', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioRecordTest'
                            '#testAudioRecordStereo16Bit',
             'value': '3.5', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioTrackTest'
                            '#testFastTimestamp',
             'value': '0.1547618955373764', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioTrackTest'
                            '#testGetTimestamp',
             'value': '0.1490119844675064', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioTrack_ListenerTest'
                            '#testAudioTrackCallback',
             'value': '9.347127739984884', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioTrack_ListenerTest'
                            '#testAudioTrackCallbackWithHandler',
             'value': '7.776177955844914', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioTrack_ListenerTest'
                            '#testStaticAudioTrackCallback',
             'value': '7.776177955844914', 'higher_is_better': False},
            {'units': 'ms',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.AudioTrack_ListenerTest'
                            '#testStaticAudioTrackCallbackWithHandler',
             'value': '9.514361300075587', 'higher_is_better': False},
            {'units': 'count',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.DecoderTest'
                            '#testH264ColorAspects',
             'value': '1.0', 'higher_is_better': True},
            {'units': 'count',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.DecoderTest'
                            '#testH265ColorAspects',
             'value': '1.0', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testAvcGoog0Perf0320x0240',
             'value': '580.1607045151507', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testAvcGoog0Perf0720x0480',
             'value': '244.18184010611358', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testAvcGoog0Perf1280x0720',
             'value': '70.96290491279275', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testAvcGoog0Perf1920x1080',
             'value': '31.299613935451564', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testAvcOther0Perf0320x0240',
             'value': '1079.6843075197307', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testAvcOther0Perf0720x0480',
             'value': '873.7785366761784', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testAvcOther0Perf1280x0720',
             'value': '664.6463289568261', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testAvcOther0Perf1920x1080',
             'value': '382.10811352923474', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testH263Goog0Perf0176x0144',
             'value': '1511.3027429644353', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testHevcGoog0Perf0352x0288',
             'value': '768.8737453173384', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testHevcGoog0Perf0640x0360',
             'value': '353.7226028743237', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testHevcGoog0Perf0720x0480',
             'value': '319.3122874170939', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testHevcGoog0Perf1280x0720',
             'value': '120.89218432028369', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testMpeg4Goog0Perf0176x0144',
             'value': '1851.890822618321', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testVp8Goog0Perf0320x0180',
             'value': '1087.946513466716', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testVp8Goog0Perf0640x0360',
             'value': '410.18461316281423', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testVp8Goog0Perf1920x1080',
             'value': '36.26433070651982', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testVp8Other0Perf0320x0180',
             'value': '1066.7819511702078', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testVp8Other0Perf0640x0360',
             'value': '930.261434505189', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testVp8Other0Perf1280x0720',
             'value': '720.4170603577236', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testVp8Other0Perf1920x1080',
             'value': '377.55742437554915', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testVp9Goog0Perf0320x0180',
             'value': '988.6158776121617', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testVp9Goog0Perf0640x0360',
             'value': '409.8162085338674', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testVp9Goog0Perf1280x0720',
             'value': '147.75847359424512', 'higher_is_better': True},
            {'units': 'fps',
             'resultsdir': '/resultsdir/tests/CTS.CtsMediaTestCases',
             'description': 'android.media.cts.VideoDecoderPerfTest'
                            '#testVp9Goog0Perf1920x1080',
             'value': '83.95677136649255', 'higher_is_better': True}
        ]
        self.assertListEqual(list(perf_result), expected_result)

        perf_result = tradefed_utils.get_perf_metrics_from_test_result_xml(
            os.path.join(os.path.dirname(os.path.realpath(__file__)),
                         'tradefed_utils_unittest_data',
                         'malformed_test_result.xml'),
            os.path.join('/', 'resultsdir'))
        self.assertListEqual(list(perf_result), [])


if __name__ == '__main__':
    unittest.main()
