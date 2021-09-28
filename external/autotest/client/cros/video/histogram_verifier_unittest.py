#!/usr/bin/python2
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import unittest

import common

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.video import histogram_verifier
import mock


class HistogramVerifierTest(unittest.TestCase):
    """
    Tests histogram_verifier's module methods.
    """

    HISTOGRAM_TEXT_1 = '\n'.join([
        'Histogram: Media.Engagement.ScoreAtPlayback recorded 29 samples, '
        'mean = 44.8 (flags = 0x41)',
        '0   ------------------------------------O                             '
        '        (6 = 20.7%)',
        '1   ...',
        '5   ------O                                                           '
        '       (1 = 3.4%) {20.7%}',
        '6   ...',
        '10  ------O                                                           '
        '        (1 = 3.4%) {24.1%}',
        '11  ...',
        '15  ------------------O                                               '
        '        (3 = 10.3%) {27.6%}',
        '16  ------------------------------O                                   '
        '        (5 = 17.2%) {37.9%}',
        '17  ...',
        '89  ------------------------------------------------------------------'
        '------O (12 = 41.4%) {55.2%}',
        '90  ------O                                                           '
        '        (1 = 3.4%) {96.6%}',
        '91  ... '])


    def test_parse_histogram(self):
        """
        Tests parse_histogram().
        """
        self.assertDictEqual(
            {0: 6, 5: 1, 10: 1, 15: 3, 16: 5, 89: 12, 90: 1},
            histogram_verifier.parse_histogram(self.HISTOGRAM_TEXT_1))
        self.assertDictEqual({}, histogram_verifier.parse_histogram(''))

    def test_subtract_histogram(self):
        """
        Tests subtract_histogram().
        """
        self.assertDictEqual({}, histogram_verifier.subtract_histogram({}, {}))
        self.assertDictEqual(
            {0: 10},
            histogram_verifier.subtract_histogram({0: 10}, {}))
        self.assertDictEqual(
            {0: -10},
            histogram_verifier.subtract_histogram({}, {0: 10}))
        self.assertDictEqual(
            {0: 10},
            histogram_verifier.subtract_histogram({0: 10}, {}))
        self.assertDictEqual(
            {0: 1},
            histogram_verifier.subtract_histogram({0: 1, 15:4}, {0:0, 15:4}))


class PollHistogramDifferBase(unittest.TestCase):
    """
    Base class to test methods that polls HistogramDiffer.

    It mocks HistogramDiffer.end() to facilitate its callers' unittest.
    It mocks utils.Timer so that it times out on the third
    sleep() call.
    """
    def setUp(self):
        self.histogram_name = 'mock_histogram'
        self.bucket_name = 'mock_bucket'
        self.differ = histogram_verifier.HistogramDiffer(
            None, self.histogram_name, begin=False)
        self.differ.end = mock.Mock()

        self.time_patcher = mock.patch.object(histogram_verifier.utils.Timer,
                                              'sleep')
        # Timeout at the third call of sleep().
        self.sleep_mock = self.time_patcher.start()
        self.sleep_mock.side_effect = [True, True, False]

    def tearDown(self):
        self.time_patcher.stop()


class ExpectSoleBucketTest(PollHistogramDifferBase):
    """
    Tests histogram_verifier.expect_sole_bucket().
    """
    def test_diff_sole_bucket(self):
        """
        Tests expect_sole_bucket() with sole bucket.
        """
        self.differ.end.side_effect = [{self.bucket_name:1}]
        self.assertTrue(histogram_verifier.expect_sole_bucket(
            self.differ, self.bucket_name, self.bucket_name))

    def test_diff_second_time(self):
        """
        Tests expect_sole_bucket() with sole bucket on the second poll.
        """
        # First time return empty.
        self.differ.end.side_effect = [{}, {self.bucket_name:1}]
        self.assertTrue(histogram_verifier.expect_sole_bucket(
            self.differ, self.bucket_name, self.bucket_name))

    def test_diff_more_than_one_bucket(self):
        """
        Tests expect_sole_bucket() with more than one bucket.
        """
        self.differ.end.side_effect = [{self.bucket_name:1, 'unexpected':1}]
        with self.assertRaisesRegexp(
                error.TestError,
                '%s has bucket other than %s' % (self.histogram_name,
                                                 self.bucket_name)):
            histogram_verifier.expect_sole_bucket(self.differ, self.bucket_name,
                                                  self.bucket_name)

    def test_diff_nothing(self):
        """
        Tests expect_sole_bucket() with no bucket.
        """
        self.differ.end.side_effect = [{}, {}]
        with self.assertRaisesRegexp(
                error.TestError,
                'Expect %s has %s' % (self.histogram_name, self.bucket_name)):
            histogram_verifier.expect_sole_bucket(self.differ, self.bucket_name,
                                                  self.bucket_name)

    def test_diff_too_late(self):
        """
        Tests expect_sole_bucket() with bucket arrives too late.
        """
        # differ polls histogram diff twice. But the bucket comes at the third
        # polling call.
        self.differ.end.side_effect = [{}, {}, {self.bucket_name: 1}]
        with self.assertRaisesRegexp(
                error.TestError,
                'Expect %s has %s' % (self.histogram_name, self.bucket_name)):
            histogram_verifier.expect_sole_bucket(self.differ, self.bucket_name,
                                                  self.bucket_name)

class PollHistogramGrowTest(PollHistogramDifferBase):
    """
    Tests histogram_verifier.poll_histogram_grow().
    """
    def test_diff_sole_bucket(self):
        """
        Tests poll_histogram_grow() with sole bucket.
        """
        self.differ.end.side_effect = [{self.bucket_name:1}]
        is_grow, histogram = histogram_verifier.poll_histogram_grow(
            self.differ, self.bucket_name, self.bucket_name)
        self.assertTrue(is_grow)
        self.assertDictEqual({self.bucket_name:1}, histogram)

    def test_diff_nochange(self):
        """
        Tests expect_sole_bucket() with sole bucket on the second poll.
        """
        self.differ.end.side_effect = [{}, {}, {}]
        is_grow, histogram = histogram_verifier.poll_histogram_grow(
            self.differ, self.bucket_name, self.bucket_name)
        self.assertFalse(is_grow)
        self.assertDictEqual({}, histogram)


class HistogramDifferTest(unittest.TestCase):
    """
    Tests histogram_verifier.HistogramDiffer class.
    """

    HISTOGRAM_BEGIN = '\n'.join([
        'Histogram: Media.GpuVideoDecoderInitializeStatus recorded 3521 samples'
        ', mean = 2.7 (flags = 0x41)',
        '0   ------------------------------------------------------------------'
        '------O (2895 = 82.2%)',
        '1   ...',
        '15  ----------------O                                                 '
        '        (626 = 17.8%) {82.2%}',
        '16  ... '])

    HISTOGRAM_END = '\n'.join([
        'Histogram: Media.GpuVideoDecoderInitializeStatus recorded 3522 samples'
        ', mean = 2.7 (flags = 0x41)',
        '0   ------------------------------------------------------------------'
        '------O (2896 = 82.2%)',
        '1   ...',
        '15  ----------------O                                                 '
        '        (626 = 17.8%) {82.2%}',
        '16  ... '])

    HISTOGRAM_NAME = 'Media.GpuVideoDecoderInitializeStatus'

    def test_init(self):
        """
        Tests __init__().
        """
        differ = histogram_verifier.HistogramDiffer(None, self.HISTOGRAM_NAME,
                                                    begin=False)
        self.assertEqual(self.HISTOGRAM_NAME, differ.histogram_name)
        self.assertDictEqual({}, differ.begin_histogram)
        self.assertDictEqual({}, differ.end_histogram)

    def test_begin_end(self):
        """
        Tests HistogramDiffer's begin() and end().

        Mocks out HistogramDiffer.get_histogram() to simplify test.
        """

        differ = histogram_verifier.HistogramDiffer(None, self.HISTOGRAM_NAME,
                                                    begin=False)
        differ._get_histogram = mock.Mock(
            side_effect = [
                (histogram_verifier.parse_histogram(self.HISTOGRAM_BEGIN),
                 self.HISTOGRAM_BEGIN),
                (histogram_verifier.parse_histogram(self.HISTOGRAM_END),
                 self.HISTOGRAM_END)])
        differ.begin()
        self.assertDictEqual({0: 1}, differ.end())

    def test_histogram_unchange(self):
        """
        Tests HistogramDiffer with histogram unchanged.

        Expects no difference.
        """
        differ = histogram_verifier.HistogramDiffer(None, self.HISTOGRAM_NAME,
                                                    begin=False)
        differ._get_histogram = mock.Mock(
            side_effect = [
                (histogram_verifier.parse_histogram(self.HISTOGRAM_BEGIN),
                 self.HISTOGRAM_BEGIN),
                (histogram_verifier.parse_histogram(self.HISTOGRAM_BEGIN),
                 self.HISTOGRAM_BEGIN)])
        differ.begin()
        self.assertDictEqual({}, differ.end())


if __name__ == '__main__':
    logging.basicConfig(
        level=logging.DEBUG,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    unittest.main()
