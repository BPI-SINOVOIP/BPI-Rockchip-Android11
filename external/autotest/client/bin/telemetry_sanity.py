#!/usr/bin/python2
#
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Sanity tests for Chrome on Chrome OS.

This script runs a number of sanity tests to ensure that Chrome browser on
Chrome OS is functional.
'''

from __future__ import print_function

import argparse
import datetime
import logging
import sys

# This sets up import paths for autotest.
import common
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib.cros import arc, arc_common, chrome
from autotest_lib.client.common_lib.cros import session_manager
from autotest_lib.client.common_lib.error import TestFail
from autotest_lib.client.cros import cryptohome


class TelemetrySanity(object):
  """Class for running sanity tests to verify telemetry."""


  def __init__(self, count=1, run_cryptohome=True, run_incognito=True,
               run_screenlock=True):
    self.count = count
    self.run_cryptohome = run_cryptohome
    self.run_incognito = run_incognito
    self.run_screenlock = run_screenlock


  def Run(self):
    start = datetime.datetime.now()

    for i in range(self.count):
      if self.count > 1:
        logging.info('Starting iteration %d.', i)
      if self.run_cryptohome:
        self.RunCryptohomeTest()
      if self.run_incognito:
        self.RunIncognitoTest()
      if self.run_screenlock:
        self.RunScreenlockTest()

    elapsed = datetime.datetime.now() - start
    logging.info('Tests succeeded in %s seconds.', elapsed.seconds)


  def RunCryptohomeTest(self):
    """Test Cryptohome."""
    logging.info('RunCryptohomeTest: Starting chrome and logging in.')
    is_arc_available = utils.is_arc_available()
    arc_mode = arc_common.ARC_MODE_ENABLED if is_arc_available else None
    with chrome.Chrome(arc_mode=arc_mode, num_tries=1) as cr:
      # Check that the cryptohome is mounted.
      # is_vault_mounted throws an exception if it fails.
      logging.info('Checking mounted cryptohome.')
      cryptohome.is_vault_mounted(user=cr.username, allow_fail=False)
      # Navigate to about:blank.
      tab = cr.browser.tabs[0]
      tab.Navigate('about:blank')

      # Evaluate some javascript.
      logging.info('Evaluating JavaScript.')
      if tab.EvaluateJavaScript('2+2') != 4:
        raise TestFail('EvaluateJavaScript failed')

      # ARC test.
      if is_arc_available:
        arc.wait_for_adb_ready()
        logging.info('Android booted successfully.')
        arc.wait_for_android_process('org.chromium.arc.intent_helper')
        if not arc.is_package_installed('android'):
          raise TestFail('"android" system package was not listed by '
                         'Package Manager.')

    if is_arc_available:
      utils.poll_for_condition(lambda: not arc.is_android_container_alive(),
                               timeout=15,
                               desc='Android container still running '
                               'after Chrome shutdown.')


  def RunIncognitoTest(self):
    """Test Incognito mode."""
    logging.info('RunIncognitoTest')
    with chrome.Chrome(logged_in=False):
      if not cryptohome.is_guest_vault_mounted():
        raise TestFail('Expected to find a guest vault mounted.')
    if cryptohome.is_guest_vault_mounted(allow_fail=True):
      raise TestFail('Expected to NOT find a guest vault mounted.')


  def RunScreenlockTest(self):
    """Run a test that locks the screen."""
    logging.info('RunScreenlockTest')
    with chrome.Chrome(autotest_ext=True) as cr:
      cr.autotest_ext.ExecuteJavaScript('chrome.autotestPrivate.lockScreen();')
      utils.poll_for_condition(
          lambda: cr.login_status['isScreenLocked'],
          timeout=15,
          exception=TestFail('Screen not locked'))


  @staticmethod
  def ParseArgs(argv):
    """Parse command line.

    Args:
      argv: List of command line arguments.

    Returns:
      List of parsed opts.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--count', type=int, default=1,
                        help='Number of iterations of the test to run.')
    parser.add_argument('--run-all', default=False, action='store_true',
                        help='Run all tests.')
    parser.add_argument('--run-cryptohome', default=False, action='store_true',
                        help='Run Cryptohome test.')
    parser.add_argument('--run-incognito', default=False, action='store_true',
                        help='Run Incognito test.')
    parser.add_argument('--run-screenlock', default=False, action='store_true',
                        help='Run Screenlock test.')
    return parser.parse_args(argv)


def main(argv):
    '''The main function.'''
    opts = TelemetrySanity.ParseArgs(argv)

    # Run all tests if none are specified.
    if opts.run_all or not (opts.run_cryptohome or opts.run_incognito or
                            opts.run_screenlock):
      opts.run_cryptohome = opts.run_screenlock = True
      opts.run_incognito = False  # crbug.com/970065

    TelemetrySanity(opts.count, opts.run_cryptohome, opts.run_incognito,
             opts.run_screenlock).Run()


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
