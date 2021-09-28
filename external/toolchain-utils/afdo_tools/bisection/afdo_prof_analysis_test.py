#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for afdo_prof_analysis."""

from __future__ import print_function

import random
import io
import unittest

from afdo_tools.bisection import afdo_prof_analysis as analysis


class AfdoProfAnalysisTest(unittest.TestCase):
  """Class for testing AFDO Profile Analysis"""
  bad_items = {'func_a': '1', 'func_b': '3', 'func_c': '5'}
  good_items = {'func_a': '2', 'func_b': '4', 'func_d': '5'}
  random.seed(13)  # 13 is an arbitrary choice. just for consistency
  # add some extra info to make tests more reflective of real scenario
  for num in range(128):
    func_name = 'func_extra_%d' % num
    # 1/3 to both, 1/3 only to good, 1/3 only to bad
    rand_val = random.randint(1, 101)
    if rand_val < 67:
      bad_items[func_name] = 'test_data'
    if rand_val < 34 or rand_val >= 67:
      good_items[func_name] = 'test_data'

  analysis.random.seed(5)  # 5 is an arbitrary choice. For consistent testing

  def test_text_to_json(self):
    test_data = io.StringIO('deflate_slow:87460059:3\n'
                            ' 3: 24\n'
                            ' 14: 54767\n'
                            ' 15: 664 fill_window:22\n'
                            ' 16: 661\n'
                            ' 19: 637\n'
                            ' 41: 36692 longest_match:36863\n'
                            ' 44: 36692\n'
                            ' 44.2: 5861\n'
                            ' 46: 13942\n'
                            ' 46.1: 14003\n')
    expected = {
        'deflate_slow': ':87460059:3\n'
                        ' 3: 24\n'
                        ' 14: 54767\n'
                        ' 15: 664 fill_window:22\n'
                        ' 16: 661\n'
                        ' 19: 637\n'
                        ' 41: 36692 longest_match:36863\n'
                        ' 44: 36692\n'
                        ' 44.2: 5861\n'
                        ' 46: 13942\n'
                        ' 46.1: 14003\n'
    }
    actual = analysis.text_to_json(test_data)
    self.assertEqual(actual, expected)
    test_data.close()

  def test_text_to_json_empty_afdo(self):
    expected = {}
    actual = analysis.text_to_json('')
    self.assertEqual(actual, expected)

  def test_json_to_text(self):
    example_prof = {'func_a': ':1\ndata\n', 'func_b': ':2\nmore data\n'}
    expected_text = 'func_a:1\ndata\nfunc_b:2\nmore data\n'
    self.assertEqual(analysis.json_to_text(example_prof), expected_text)

  def test_bisect_profiles(self):

    # mock run of external script with arbitrarily-chosen bad profile vals
    # save_run specified and unused b/c afdo_prof_analysis.py
    # will call with argument explicitly specified
    # pylint: disable=unused-argument
    class DeciderClass(object):
      """Class for this tests's decider."""

      def run(self, prof, save_run=False):
        if '1' in prof['func_a'] or '3' in prof['func_b']:
          return analysis.StatusEnum.BAD_STATUS
        return analysis.StatusEnum.GOOD_STATUS

    results = analysis.bisect_profiles_wrapper(DeciderClass(), self.good_items,
                                               self.bad_items)
    self.assertEqual(results['individuals'], sorted(['func_a', 'func_b']))
    self.assertEqual(results['ranges'], [])

  def test_range_search(self):

    # arbitrarily chosen functions whose values in the bad profile constitute
    # a problematic pair
    # pylint: disable=unused-argument
    class DeciderClass(object):
      """Class for this tests's decider."""

      def run(self, prof, save_run=False):
        if '1' in prof['func_a'] and '3' in prof['func_b']:
          return analysis.StatusEnum.BAD_STATUS
        return analysis.StatusEnum.GOOD_STATUS

    # put the problematic combination in separate halves of the common funcs
    # so that non-bisecting search is invoked for its actual use case
    common_funcs = [func for func in self.good_items if func in self.bad_items]
    common_funcs.remove('func_a')
    common_funcs.insert(0, 'func_a')
    common_funcs.remove('func_b')
    common_funcs.append('func_b')

    problem_range = analysis.range_search(DeciderClass(), self.good_items,
                                          self.bad_items, common_funcs, 0,
                                          len(common_funcs))

    self.assertEqual(['func_a', 'func_b'], problem_range)

  def test_check_good_not_bad(self):
    func_in_good = 'func_c'

    # pylint: disable=unused-argument
    class DeciderClass(object):
      """Class for this tests's decider."""

      def run(self, prof, save_run=False):
        if func_in_good in prof:
          return analysis.StatusEnum.GOOD_STATUS
        return analysis.StatusEnum.BAD_STATUS

    self.assertTrue(
        analysis.check_good_not_bad(DeciderClass(), self.good_items,
                                    self.bad_items))

  def test_check_bad_not_good(self):
    func_in_bad = 'func_d'

    # pylint: disable=unused-argument
    class DeciderClass(object):
      """Class for this tests's decider."""

      def run(self, prof, save_run=False):
        if func_in_bad in prof:
          return analysis.StatusEnum.BAD_STATUS
        return analysis.StatusEnum.GOOD_STATUS

    self.assertTrue(
        analysis.check_bad_not_good(DeciderClass(), self.good_items,
                                    self.bad_items))


if __name__ == '__main__':
  unittest.main()
