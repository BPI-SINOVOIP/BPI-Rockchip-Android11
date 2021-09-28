#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""End-to-end test for afdo_prof_analysis."""

from __future__ import absolute_import, division, print_function

import json
import os
import shutil
import tempfile
import unittest
from datetime import date

from afdo_tools.bisection import afdo_prof_analysis as analysis


class ObjectWithFields(object):
  """Turns kwargs given to the constructor into fields on an object.

  Examples:
    x = ObjectWithFields(a=1, b=2)
    assert x.a == 1
    assert x.b == 2
  """

  def __init__(self, **kwargs):
    for key, val in kwargs.items():
      setattr(self, key, val)


class AfdoProfAnalysisE2ETest(unittest.TestCase):
  """Class for end-to-end testing of AFDO Profile Analysis"""

  # nothing significant about the values, just easier to remember even vs odd
  good_prof = {
      'func_a': ':1\n 1: 3\n 3: 5\n 5: 7\n',
      'func_b': ':3\n 3: 5\n 5: 7\n 7: 9\n',
      'func_c': ':5\n 5: 7\n 7: 9\n 9: 11\n',
      'func_d': ':7\n 7: 9\n 9: 11\n 11: 13\n',
      'good_func_a': ':11\n',
      'good_func_b': ':13\n'
  }

  bad_prof = {
      'func_a': ':2\n 2: 4\n 4: 6\n 6: 8\n',
      'func_b': ':4\n 4: 6\n 6: 8\n 8: 10\n',
      'func_c': ':6\n 6: 8\n 8: 10\n 10: 12\n',
      'func_d': ':8\n 8: 10\n 10: 12\n 12: 14\n',
      'bad_func_a': ':12\n',
      'bad_func_b': ':14\n'
  }

  expected = {
      'good_only_functions': False,
      'bad_only_functions': True,
      'bisect_results': {
          'ranges': [],
          'individuals': ['func_a']
      }
  }

  def test_afdo_prof_analysis(self):
    # Individual issues take precedence by nature of our algos
    # so first, that should be caught
    good = self.good_prof.copy()
    bad = self.bad_prof.copy()
    self.run_check(good, bad, self.expected)

    # Now remove individuals and exclusively BAD, and check that range is caught
    bad['func_a'] = good['func_a']
    bad.pop('bad_func_a')
    bad.pop('bad_func_b')

    expected_cp = self.expected.copy()
    expected_cp['bad_only_functions'] = False
    expected_cp['bisect_results'] = {
        'individuals': [],
        'ranges': [['func_b', 'func_c', 'func_d']]
    }

    self.run_check(good, bad, expected_cp)

  def test_afdo_prof_state(self):
    """Verifies that saved state is correct replication."""
    temp_dir = tempfile.mkdtemp()
    self.addCleanup(shutil.rmtree, temp_dir, ignore_errors=True)

    good = self.good_prof.copy()
    bad = self.bad_prof.copy()
    # add more functions to data
    for x in range(400):
      good['func_%d' % x] = ''
      bad['func_%d' % x] = ''

    fd_first, first_result = tempfile.mkstemp(dir=temp_dir)
    os.close(fd_first)
    fd_state, state_file = tempfile.mkstemp(dir=temp_dir)
    os.close(fd_state)
    self.run_check(
        self.good_prof,
        self.bad_prof,
        self.expected,
        state_file=state_file,
        out_file=first_result)

    fd_second, second_result = tempfile.mkstemp(dir=temp_dir)
    os.close(fd_second)
    completed_state_file = '%s.completed.%s' % (state_file, str(date.today()))
    self.run_check(
        self.good_prof,
        self.bad_prof,
        self.expected,
        state_file=completed_state_file,
        no_resume=False,
        out_file=second_result)

    with open(first_result) as f:
      initial_run = json.load(f)
    with open(second_result) as f:
      loaded_run = json.load(f)
    self.assertEqual(initial_run, loaded_run)

  def test_exit_on_problem_status(self):
    temp_dir = tempfile.mkdtemp()
    self.addCleanup(shutil.rmtree, temp_dir, ignore_errors=True)

    fd_state, state_file = tempfile.mkstemp(dir=temp_dir)
    os.close(fd_state)
    with self.assertRaises(RuntimeError):
      self.run_check(
          self.good_prof,
          self.bad_prof,
          self.expected,
          state_file=state_file,
          extern_decider='problemstatus_external.sh')

  def test_state_assumption(self):

    def compare_runs(tmp_dir, first_ctr, second_ctr):
      """Compares given prof versions between first and second run in test."""
      first_prof = '%s/.first_run_%d' % (tmp_dir, first_ctr)
      second_prof = '%s/.second_run_%d' % (tmp_dir, second_ctr)
      with open(first_prof) as f:
        first_prof_text = f.read()
      with open(second_prof) as f:
        second_prof_text = f.read()
      self.assertEqual(first_prof_text, second_prof_text)

    good_prof = {'func_a': ':1\n3: 3\n5: 7\n'}
    bad_prof = {'func_a': ':2\n4: 4\n6: 8\n'}
    # add some noise to the profiles; 15 is an arbitrary choice
    for x in range(15):
      func = 'func_%d' % x
      good_prof[func] = ':%d\n' % (x)
      bad_prof[func] = ':%d\n' % (x + 1)
    expected = {
        'bisect_results': {
            'ranges': [],
            'individuals': ['func_a']
        },
        'good_only_functions': False,
        'bad_only_functions': False
    }

    # using a static temp dir rather than a dynamic one because these files are
    # shared between the bash scripts and this Python test, and the arguments
    # to the bash scripts are fixed by afdo_prof_analysis.py so it would be
    # difficult to communicate dynamically generated directory to bash scripts
    scripts_tmp_dir = '%s/afdo_test_tmp' % os.getcwd()
    os.mkdir(scripts_tmp_dir)
    self.addCleanup(shutil.rmtree, scripts_tmp_dir, ignore_errors=True)

    # files used in the bash scripts used as external deciders below
    # - count_file tracks the current number of calls to the script in total
    # - local_count_file tracks the number of calls to the script without
    # interruption
    count_file = '%s/.count' % scripts_tmp_dir
    local_count_file = '%s/.local_count' % scripts_tmp_dir

    # runs through whole thing at once
    initial_seed = self.run_check(
        good_prof,
        bad_prof,
        expected,
        extern_decider='state_assumption_external.sh')
    with open(count_file) as f:
      num_calls = int(f.read())
    os.remove(count_file)  # reset counts for second run
    finished_state_file = 'afdo_analysis_state.json.completed.%s' % str(
        date.today())
    self.addCleanup(os.remove, finished_state_file)

    # runs the same analysis but interrupted each iteration
    for i in range(2 * num_calls + 1):
      no_resume_run = (i == 0)
      seed = initial_seed if no_resume_run else None
      try:
        self.run_check(
            good_prof,
            bad_prof,
            expected,
            no_resume=no_resume_run,
            extern_decider='state_assumption_interrupt.sh',
            seed=seed)
        break
      except RuntimeError:
        # script was interrupted, so we restart local count
        os.remove(local_count_file)
    else:
      raise RuntimeError('Test failed -- took too many iterations')

    for initial_ctr in range(3):  # initial runs unaffected by interruption
      compare_runs(scripts_tmp_dir, initial_ctr, initial_ctr)

    start = 3
    for ctr in range(start, num_calls):
      # second run counter incremented by 4 for each one first run is because
      # +2 for performing initial checks on good and bad profs each time
      # +1 for PROBLEM_STATUS run which causes error and restart
      compare_runs(scripts_tmp_dir, ctr, 6 + (ctr - start) * 4)

  def run_check(self,
                good_prof,
                bad_prof,
                expected,
                state_file=None,
                no_resume=True,
                out_file=None,
                extern_decider=None,
                seed=None):

    temp_dir = tempfile.mkdtemp()
    self.addCleanup(shutil.rmtree, temp_dir, ignore_errors=True)

    good_prof_file = '%s/%s' % (temp_dir, 'good_prof.txt')
    bad_prof_file = '%s/%s' % (temp_dir, 'bad_prof.txt')
    good_prof_text = analysis.json_to_text(good_prof)
    bad_prof_text = analysis.json_to_text(bad_prof)
    with open(good_prof_file, 'w') as f:
      f.write(good_prof_text)
    with open(bad_prof_file, 'w') as f:
      f.write(bad_prof_text)

    dir_path = os.path.dirname(os.path.realpath(__file__))  # dir of this file
    external_script = '%s/%s' % (dir_path, extern_decider or 'e2e_external.sh')

    # FIXME: This test ideally shouldn't be writing to $PWD
    if state_file is None:
      state_file = '%s/afdo_analysis_state.json' % os.getcwd()

      def rm_state():
        try:
          os.unlink(state_file)
        except OSError:
          # Probably because the file DNE. That's fine.
          pass

      self.addCleanup(rm_state)

    actual = analysis.main(
        ObjectWithFields(
            good_prof=good_prof_file,
            bad_prof=bad_prof_file,
            external_decider=external_script,
            analysis_output_file=out_file or '/dev/null',
            state_file=state_file,
            no_resume=no_resume,
            remove_state_on_completion=False,
            seed=seed,
        ))
    actual_seed = actual.pop('seed')  # nothing to check
    self.assertEqual(actual, expected)
    return actual_seed


if __name__ == '__main__':
  unittest.main()
