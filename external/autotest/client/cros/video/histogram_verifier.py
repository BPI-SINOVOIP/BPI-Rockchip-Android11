# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import re

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error


def get_histogram_text(tab, histogram_name):
     """
     This returns contents of the given histogram.

     @param tab: object, Chrome tab instance
     @param histogram_name: string, name of the histogram
     @returns string: contents of the histogram
     """
     docEle = 'document.documentElement'
     tab.Navigate('chrome://histograms/%s' % histogram_name)
     tab.WaitForDocumentReadyStateToBeComplete()
     raw_text = tab.EvaluateJavaScript(
          '{0} && {0}.innerText'.format(docEle))
     # extract the contents of the histogram
     histogram = raw_text[raw_text.find('Histogram:'):].strip()
     if histogram:
          logging.debug('chrome://histograms/%s:\n%s', histogram_name,
                        histogram)
     else:
          logging.debug('No histogram is shown in chrome://histograms/%s',
                        histogram_name)
     return histogram


def loaded(tab, histogram_name, pattern):
     """
     Checks if the histogram page has been fully loaded.

     @param tab: object, Chrome tab instance
     @param histogram_name: string, name of the histogram
     @param pattern: string, required text to look for
     @returns re.MatchObject if the given pattern is found in the text
              None otherwise

     """
     return re.search(pattern, get_histogram_text(tab, histogram_name))


def  verify(cr, histogram_name, histogram_bucket_value):
     """
     Verifies histogram string and success rate in a parsed histogram bucket.
     The histogram buckets are outputted in debug log regardless of the
     verification result.

     Full histogram URL is used to load histogram. Example Histogram URL is :
     chrome://histograms/Media.GpuVideoDecoderInitializeStatus

     @param cr: object, the Chrome instance
     @param histogram_name: string, name of the histogram
     @param histogram_bucket_value: int, required bucket number to look for
     @raises error.TestError if histogram is not successful

     """
     bucket_pattern = '\n'+ str(histogram_bucket_value) +'.*100\.0%.*'
     error_msg_format = ('{} not loaded or histogram bucket not found '
                         'or histogram bucket found at < 100%')
     tab = cr.browser.tabs.New()
     msg = error_msg_format.format(histogram_name)
     utils.poll_for_condition(lambda : loaded(tab, histogram_name,
                                              bucket_pattern),
                              exception=error.TestError(msg),
                              sleep_interval=1)


def is_bucket_present(cr,histogram_name, histogram_bucket_value):
     """
     This returns histogram succes or fail to called function

     @param cr: object, the Chrome instance
     @param histogram_name: string, name of the histogram
     @param histogram_bucket_value: int, required bucket number to look for
     @returns True if histogram page was loaded and the bucket was found.
              False otherwise

     """
     try:
          verify(cr,histogram_name, histogram_bucket_value)
     except error.TestError:
          return False
     else:
          return True


def is_histogram_present(cr, histogram_name):
     """
     This checks if the given histogram is present and non-zero.

     @param cr: object, the Chrome instance
     @param histogram_name: string, name of the histogram
     @returns True if histogram page was loaded and the histogram is present
              False otherwise

     """
     histogram_pattern = 'Histogram: '+ histogram_name + ' recorded ' + \
                         r'[1-9][0-9]*' + ' samples'
     tab = cr.browser.tabs.New()
     try:
          utils.poll_for_condition(lambda : loaded(tab, histogram_name,
                                                   histogram_pattern),
                                   timeout=2,
                                   sleep_interval=0.1)
          return True
     except utils.TimeoutError:
          # the histogram is not present, and then returns false
          return False


def get_histogram(cr, histogram_name):
     """
     This returns contents of the given histogram.

     @param cr: object, the Chrome instance
     @param histogram_name: string, name of the histogram
     @returns string: contents of the histogram

     """
     tab = cr.browser.tabs.New()
     return get_histogram_text(tab, histogram_name)


def parse_histogram(histogram_text):
     """
     Parses histogram text into bucket structure.

     @param histogram_text: histogram raw text.
     @returns dict(bucket_value, bucket_count)
     """
     # Match separator line, e.g. "1   ..."
     RE_SEPEARTOR = re.compile(r'\d+\s+\.\.\.')
     # Match bucket line, e.g. "2  --O  (46 = 1.5%) {46.1%}"
     RE_BUCKET = re.compile(
          r'(\d+)\s+\-*O\s+\((\d+) = (\d+\.\d+)%\).*')
     result = {}
     for line in histogram_text.splitlines():
          if RE_SEPEARTOR.match(line):
               continue
          m = RE_BUCKET.match(line)
          if m:
               result[int(m.group(1))] = int(m.group(2))
     return result


def subtract_histogram(minuend, subtrahend):
     """
     Subtracts histogram: minuend - subtrahend

     @param minuend: histogram bucket dict from which another is to be
                     subtracted.
     @param subtrahend: histogram bucket dict to be subtracted from another.
     @result difference of the two histograms in bucket dict. Note that
             zero-counted buckets are removed.
     """
     result = collections.defaultdict(int, minuend)
     for k, v in subtrahend.iteritems():
          result[k] -= v

     # Remove zero counted buckets.
     return {k: v for k, v in result.iteritems() if v}


def expect_sole_bucket(histogram_differ, bucket, bucket_name, timeout=10,
                       sleep_interval=1):
     """
     Returns true if the given bucket solely exists in histogram differ.

     @param histogram_differ: a HistogramDiffer instance used to get histogram
            name and histogram diff multiple times.
     @param bucket: bucket value.
     @param bucket_name: bucket name to be shown on error message.
     @param timeout: timeout in seconds.
     @param sleep_interval: interval in seconds between getting diff.
     @returns True if the given bucket solely exists in histogram.
     @raises TestError if bucket doesn't exist or other buckets exist.
     """
     timer = utils.Timer(timeout)
     histogram = {}
     histogram_name = histogram_differ.histogram_name
     while timer.sleep(sleep_interval):
          histogram = histogram_differ.end()
          if histogram:
               break

     if bucket not in histogram:
          raise error.TestError('Expect %s has %s. Histogram: %r' %
                                (histogram_name, bucket_name, histogram))
     if len(histogram) > 1:
          raise error.TestError('%s has bucket other than %s. Histogram: %r' %
                                (histogram_name, bucket_name, histogram))
     return True


def poll_histogram_grow(histogram_differ, timeout=2, sleep_interval=0.1):
     """
     Polls histogram to see if it grows within |timeout| seconds.

     @param histogram_differ: a HistogramDiffer instance used to get histogram
            name and histogram diff multiple times.
     @param timeout: observation timeout in seconds.
     @param sleep_interval: interval in seconds between getting diff.
     @returns (True, histogram_diff) if the histogram grows.
              (False, {}) if it does not grow in |timeout| seconds.
     """
     timer = utils.Timer(timeout)
     while timer.sleep(sleep_interval):
          histogram_diff = histogram_differ.end()
          if histogram_diff:
               return (True, histogram_diff)
     return (False, {})


class HistogramDiffer(object):
     """
     Calculates a histogram's progress between begin() and end().

     Usage:
       differ = HistogramDiffer(cr, 'Media.GpuVideoDecoderError')
       ....
       diff_gvd_error = differ.end()
     """
     def __init__(self, cr, histogram_name, begin=True):
          """
          Constructor.

          @param: cr: object, the Chrome instance
          @param: histogram_name: string, name of the histogram
          @param: begin: if set, calls begin().
          """
          self.cr = cr
          self.histogram_name = histogram_name
          self.begin_histogram_text = ''
          self.end_histogram_text = ''
          self.begin_histogram = {}
          self.end_histogram = {}
          if begin:
               self.begin()

     def _get_histogram(self):
          """
          Gets current histogram bucket.

          @returns (dict(bucket_value, bucket_count), histogram_text)
          """
          tab = self.cr.browser.tabs.New()
          text = get_histogram_text(tab, self.histogram_name)
          tab.Close()
          return (parse_histogram(text), text)

     def begin(self):
          """
          Takes a histogram snapshot as begin_histogram.
          """
          (self.begin_histogram,
           self.begin_histogram_text) = self._get_histogram()
          logging.debug('begin histograms/%s: %r\nraw_text: %s',
                        self.histogram_name, self.begin_histogram,
                        self.begin_histogram_text)

     def end(self):
          """
          Takes a histogram snapshot as end_histogram.

          @returns self.diff()
          """
          self.end_histogram, self.end_histogram_text = self._get_histogram()
          logging.debug('end histograms/%s: %r\nraw_text: %s',
                        self.histogram_name, self.end_histogram,
                        self.end_histogram_text)
          diff = subtract_histogram(self.end_histogram, self.begin_histogram)
          logging.debug('histogram diff: %r', diff)
          return diff
