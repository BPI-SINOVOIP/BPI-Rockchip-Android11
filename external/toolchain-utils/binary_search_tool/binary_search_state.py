#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The binary search wrapper."""

from __future__ import division
from __future__ import print_function

import argparse
import contextlib
import errno
import math
import os
import pickle
import re
import shutil
import sys
import tempfile
import time

# Adds cros_utils to PYTHONPATH
from binary_search_tool import binary_search_perforce
from binary_search_tool import common
from binary_search_tool import pass_mapping

# Now we do import from cros_utils
from cros_utils import command_executer
from cros_utils import logger

GOOD_SET_VAR = 'BISECT_GOOD_SET'
BAD_SET_VAR = 'BISECT_BAD_SET'

STATE_FILE = '%s.state' % sys.argv[0]
HIDDEN_STATE_FILE = os.path.join(
    os.path.dirname(STATE_FILE), '.%s' % os.path.basename(STATE_FILE))


@contextlib.contextmanager
def SetFile(env_var, items):
  """Generate set files that can be used by switch/test scripts.

  Generate temporary set file (good/bad) holding contents of good/bad items for
  the current binary search iteration. Store the name of each file as an
  environment variable so all child processes can access it.

  This function is a contextmanager, meaning it's meant to be used with the
  "with" statement in Python. This is so cleanup and setup happens automatically
  and cleanly. Execution of the outer "with" statement happens at the "yield"
  statement.

  Args:
    env_var: What environment variable to store the file name in.
    items: What items are in this set.
  """
  with tempfile.NamedTemporaryFile('w', encoding='utf-8') as f:
    os.environ[env_var] = f.name
    f.write('\n'.join(items))
    f.flush()
    yield


class BinarySearchState(object):
  """The binary search state class."""

  def __init__(self, get_initial_items, switch_to_good, switch_to_bad,
               test_setup_script, test_script, incremental, prune, pass_bisect,
               ir_diff, iterations, prune_iterations, verify, file_args,
               verbose):
    """BinarySearchState constructor, see Run for full args documentation."""
    self.get_initial_items = get_initial_items
    self.switch_to_good = switch_to_good
    self.switch_to_bad = switch_to_bad
    self.test_setup_script = test_setup_script
    self.test_script = test_script
    self.incremental = incremental
    self.prune = prune
    self.pass_bisect = pass_bisect
    self.ir_diff = ir_diff
    self.iterations = iterations
    self.prune_iterations = prune_iterations
    self.verify = verify
    self.file_args = file_args
    self.verbose = verbose

    self.l = logger.GetLogger()
    self.ce = command_executer.GetCommandExecuter()

    self.resumed = False
    self.prune_cycles = 0
    self.search_cycles = 0
    self.binary_search = None
    self.all_items = None
    self.cmd_script = None
    self.mode = None
    self.PopulateItemsUsingCommand(self.get_initial_items)
    self.currently_good_items = set()
    self.currently_bad_items = set()
    self.found_items = set()
    self.known_good = set()

    self.start_time = time.time()

  def SwitchToGood(self, item_list):
    """Switch given items to "good" set."""
    if self.incremental:
      self.l.LogOutput(
          'Incremental set. Wanted to switch %s to good' % str(item_list),
          print_to_console=self.verbose)
      incremental_items = [
          item for item in item_list if item not in self.currently_good_items
      ]
      item_list = incremental_items
      self.l.LogOutput(
          'Incremental set. Actually switching %s to good' % str(item_list),
          print_to_console=self.verbose)

    if not item_list:
      return

    self.l.LogOutput(
        'Switching %s to good' % str(item_list), print_to_console=self.verbose)
    self.RunSwitchScript(self.switch_to_good, item_list)
    self.currently_good_items = self.currently_good_items.union(set(item_list))
    self.currently_bad_items.difference_update(set(item_list))

  def SwitchToBad(self, item_list):
    """Switch given items to "bad" set."""
    if self.incremental:
      self.l.LogOutput(
          'Incremental set. Wanted to switch %s to bad' % str(item_list),
          print_to_console=self.verbose)
      incremental_items = [
          item for item in item_list if item not in self.currently_bad_items
      ]
      item_list = incremental_items
      self.l.LogOutput(
          'Incremental set. Actually switching %s to bad' % str(item_list),
          print_to_console=self.verbose)

    if not item_list:
      return

    self.l.LogOutput(
        'Switching %s to bad' % str(item_list), print_to_console=self.verbose)
    self.RunSwitchScript(self.switch_to_bad, item_list)
    self.currently_bad_items = self.currently_bad_items.union(set(item_list))
    self.currently_good_items.difference_update(set(item_list))

  def RunSwitchScript(self, switch_script, item_list):
    """Pass given items to switch script.

    Args:
      switch_script: path to switch script
      item_list: list of all items to be switched
    """
    if self.file_args:
      with tempfile.NamedTemporaryFile('w', encoding='utf-8') as f:
        f.write('\n'.join(item_list))
        f.flush()
        command = '%s %s' % (switch_script, f.name)
        ret, _, _ = self.ce.RunCommandWExceptionCleanup(
            command, print_to_console=self.verbose)
    else:
      command = '%s %s' % (switch_script, ' '.join(item_list))
      try:
        ret, _, _ = self.ce.RunCommandWExceptionCleanup(
            command, print_to_console=self.verbose)
      except OSError as e:
        if e.errno == errno.E2BIG:
          raise RuntimeError('Too many arguments for switch script! Use '
                             '--file_args')
    assert ret == 0, 'Switch script %s returned %d' % (switch_script, ret)

  def TestScript(self):
    """Run test script and return exit code from script."""
    command = self.test_script
    ret, _, _ = self.ce.RunCommandWExceptionCleanup(command)
    return ret

  def TestSetupScript(self):
    """Run test setup script and return exit code from script."""
    if not self.test_setup_script:
      return 0

    command = self.test_setup_script
    ret, _, _ = self.ce.RunCommandWExceptionCleanup(command)
    return ret

  def GenerateBadCommandScript(self, bad_items):
    """Generate command line script for building bad item."""
    assert not self.prune, 'Prune must be false if pass_bisect is set.'
    assert len(bad_items) == 1, 'Pruning is off, but number of bad ' \
                                       'items found was not 1.'
    item = list(bad_items)[0]
    command = '%s %s' % (self.pass_bisect, item)
    ret, _, _ = self.ce.RunCommandWExceptionCleanup(
        command, print_to_console=self.verbose)
    return ret

  def DoVerify(self):
    """Verify correctness of test environment.

    Verify that a "good" set of items produces a "good" result and that a "bad"
    set of items produces a "bad" result. To be run directly before running
    DoSearch. If verify is False this step is skipped.
    """
    if not self.verify:
      return

    self.l.LogOutput('VERIFICATION')
    self.l.LogOutput('Beginning tests to verify good/bad sets\n')

    self._OutputProgress('Verifying items from GOOD set\n')
    with SetFile(GOOD_SET_VAR, self.all_items), SetFile(BAD_SET_VAR, []):
      self.l.LogOutput('Resetting all items to good to verify.')
      self.SwitchToGood(self.all_items)
      status = self.TestSetupScript()
      assert status == 0, 'When reset_to_good, test setup should succeed.'
      status = self.TestScript()
      assert status == 0, 'When reset_to_good, status should be 0.'

    self._OutputProgress('Verifying items from BAD set\n')
    with SetFile(GOOD_SET_VAR, []), SetFile(BAD_SET_VAR, self.all_items):
      self.l.LogOutput('Resetting all items to bad to verify.')
      self.SwitchToBad(self.all_items)
      status = self.TestSetupScript()
      # The following assumption is not true; a bad image might not
      # successfully push onto a device.
      # assert status == 0, 'When reset_to_bad, test setup should succeed.'
      if status == 0:
        status = self.TestScript()
      assert status == 1, 'When reset_to_bad, status should be 1.'

  def DoSearchBadItems(self):
    """Perform full search for bad items.

    Perform full search until prune_iterations number of bad items are found.
    """
    while (True and len(self.all_items) > 1 and
           self.prune_cycles < self.prune_iterations):
      terminated = self.DoBinarySearchBadItems()
      self.prune_cycles += 1
      if not terminated:
        break
      # Prune is set.
      prune_index = self.binary_search.current

      # If found item is last item, no new items can be found
      if prune_index == len(self.all_items) - 1:
        self.l.LogOutput('First bad item is the last item. Breaking.')
        self.l.LogOutput('Bad items are: %s' % self.all_items[-1])
        self.found_items.add(self.all_items[-1])
        break

      # If already seen item we have no new bad items to find, finish up
      if self.all_items[prune_index] in self.found_items:
        self.l.LogOutput(
            'Found item already found before: %s.' %
            self.all_items[prune_index],
            print_to_console=self.verbose)
        self.l.LogOutput('No more bad items remaining. Done searching.')
        self.l.LogOutput('Bad items are: %s' % ' '.join(self.found_items))
        break

      new_all_items = list(self.all_items)
      # Move prune item to the end of the list.
      new_all_items.append(new_all_items.pop(prune_index))
      self.found_items.add(new_all_items[-1])

      # Everything below newly found bad item is now known to be a good item.
      # Take these good items out of the equation to save time on the next
      # search. We save these known good items so they are still sent to the
      # switch_to_good script.
      if prune_index:
        self.known_good.update(new_all_items[:prune_index])
        new_all_items = new_all_items[prune_index:]

      self.l.LogOutput(
          'Old list: %s. New list: %s' % (str(self.all_items),
                                          str(new_all_items)),
          print_to_console=self.verbose)

      if not self.prune:
        self.l.LogOutput('Not continuning further, --prune is not set')
        break
      # FIXME: Do we need to Convert the currently good items to bad
      self.PopulateItemsUsingList(new_all_items)

    # If pass level bisecting is set, generate a script which contains command
    # line options to rebuild bad item.
    if self.pass_bisect:
      status = self.GenerateBadCommandScript(self.found_items)
      if status == 0:
        self.cmd_script = os.path.join(
            os.path.dirname(self.pass_bisect), 'cmd_script.sh')
        self.l.LogOutput('Command script generated at %s.' % self.cmd_script)
      else:
        raise RuntimeError('Error while generating command script.')

  def DoBinarySearchBadItems(self):
    """Perform single iteration of binary search."""
    # If in resume mode don't reset search_cycles
    if not self.resumed:
      self.search_cycles = 0
    else:
      self.resumed = False

    terminated = False
    while self.search_cycles < self.iterations and not terminated:
      self.SaveState()
      self.OutputIterationProgressBadItem()

      self.search_cycles += 1
      [bad_items, good_items] = self.GetNextItems()

      with SetFile(GOOD_SET_VAR, good_items), SetFile(BAD_SET_VAR, bad_items):
        # TODO: bad_items should come first.
        self.SwitchToGood(good_items)
        self.SwitchToBad(bad_items)
        status = self.TestSetupScript()
        if status == 0:
          status = self.TestScript()
        terminated = self.binary_search.SetStatus(status)

      if terminated:
        self.l.LogOutput('Terminated!', print_to_console=self.verbose)
    if not terminated:
      self.l.LogOutput('Ran out of iterations searching...')
    self.l.LogOutput(str(self), print_to_console=self.verbose)
    return terminated

  def CollectPassName(self, pass_info):
    """Mapping opt-bisect output of pass info to debugcounter name."""
    self.l.LogOutput('Pass info: %s' % pass_info, print_to_console=self.verbose)

    for desc in pass_mapping.pass_name:
      if desc in pass_info:
        return pass_mapping.pass_name[desc]

    # If pass not found, return None
    return None

  def BuildWithPassLimit(self, limit, generate_ir=False):
    """Rebuild bad item with pass level bisect limit

    Run command line script generated by GenerateBadCommandScript(), with
    pass level limit flags.

    Returns:
      pass_num: current number of the pass, or total number of passes if
                limit set to -1.
      pass_name: The debugcounter name of current limit pass.
    """
    os.environ['LIMIT_FLAGS'] = '-mllvm -opt-bisect-limit=' + str(limit)
    if generate_ir:
      os.environ['LIMIT_FLAGS'] += ' -S -emit-llvm'
    self.l.LogOutput(
        'Limit flags: %s' % os.environ['LIMIT_FLAGS'],
        print_to_console=self.verbose)
    command = self.cmd_script
    _, _, msg = self.ce.RunCommandWOutput(command, print_to_console=False)

    # Massages we get will be like this:
    #   BISECT: running pass (9) <Pass Description> on <function> (<file>)
    #   BISECT: running pass (10) <Pass Description> on <module> (<file>)
    #   BISECT: NOT running pass (11) <Pass Description> on <SCG> (<file>)
    #   BISECT: NOT running pass (12) <Pass Description> on <SCG> (<file>)
    # We want to get the pass description of last running pass, to have
    # transformation level bisect on it.
    if 'BISECT: ' not in msg:
      raise RuntimeError('No bisect info printed, OptBisect may not be '
                         'supported by the compiler.')

    lines = msg.split('\n')
    pass_num = 0
    last_pass = ''
    for l in lines:
      if 'running pass' in l:
        # For situation of limit==-1, we want the total number of passes
        if limit != -1 and 'BISECT: NOT ' in l:
          break
        pass_num += 1
        last_pass = l
    if limit not in (-1, pass_num):
      raise ValueError('[Error] While building, limit number does not match.')
    return pass_num, self.CollectPassName(last_pass)

  def BuildWithTransformLimit(self,
                              limit,
                              pass_name=None,
                              pass_limit=-1,
                              generate_ir=False):
    """Rebuild bad item with transformation level bisect limit

    Run command line script generated by GenerateBadCommandScript(), with
    pass level limit flags and transformation level limit flags.

    Args:
      limit: transformation level limit for bad item.
      pass_name: name of bad pass debugcounter from pass level bisect result.
      pass_limit: pass level limit from pass level bisect result.
      generate_ir: Whether to generate IR comparison.

    Returns:
      Total number of transformations if limit set to -1, else return 0.
    """
    counter_name = pass_name

    os.environ['LIMIT_FLAGS'] = '-mllvm -opt-bisect-limit=' + \
                                str(pass_limit) + \
                                ' -mllvm -debug-counter=' + counter_name + \
                                '-count=' + str(limit) + \
                                ' -mllvm -print-debug-counter'
    if generate_ir:
      os.environ['LIMIT_FLAGS'] += ' -S -emit-llvm'
    self.l.LogOutput(
        'Limit flags: %s' % os.environ['LIMIT_FLAGS'],
        print_to_console=self.verbose)
    command = self.cmd_script
    _, _, msg = self.ce.RunCommandWOutput(command, print_to_console=False)

    if 'Counters and values:' not in msg:
      # Print pass level IR diff only if transformation level bisection does
      # not work.
      if self.ir_diff:
        self.PrintIRDiff(pass_limit)
      raise RuntimeError('No bisect info printed, DebugCounter may not be '
                         'supported by the compiler.')

    # With debugcounter enabled, there will be DebugCounter counting info in
    # the output.
    lines = msg.split('\n')
    for l in lines:
      if pass_name in l:
        # Output of debugcounter will be like:
        #   instcombine-visit: {10, 0, 20}
        #   dce-transform: {1, 0, -1}
        # which indicates {Count, Skip, StopAfter}.
        # The last number should be the limit we set.
        # We want the first number as the total transformation count.
        # Split each line by ,|{|} and we can get l_list as:
        #   ['instcombine: ', '10', '0', '20', '']
        # and we will need the second item in it.
        l_list = re.split(',|{|}', l)
        count = int(l_list[1])
        if limit == -1:
          return count
    # The returned value is only useful when limit == -1, which shows total
    # transformation count.
    return 0

  def PrintIRDiff(self, pass_index, pass_name=None, trans_index=-1):
    bad_item = list(self.found_items)[0]
    self.l.LogOutput(
        'IR difference before and after bad pass/transformation:',
        print_to_console=self.verbose)

    if trans_index == -1:
      # Pass level IR diff
      self.BuildWithPassLimit(pass_index, self.ir_diff)
      good_ir = os.path.join(tempfile.tempdir, 'good.s')
      shutil.copyfile(bad_item, good_ir)
      pass_index += 1
      self.BuildWithPassLimit(pass_index, self.ir_diff)
    else:
      # Transformation level IR diff
      self.BuildWithTransformLimit(trans_index, pass_name, pass_index,
                                   self.ir_diff)
      good_ir = os.path.join(tempfile.tempdir, 'good.s')
      shutil.copyfile(bad_item, good_ir)
      trans_index += 1
      self.BuildWithTransformLimit(trans_index, pass_name, pass_index,
                                   self.ir_diff)

    bad_ir = os.path.join(tempfile.tempdir, 'bad.s')
    shutil.copyfile(bad_item, bad_ir)

    command = 'diff %s %s' % (good_ir, bad_ir)
    _, _, _ = self.ce.RunCommandWOutput(command, print_to_console=self.verbose)

  def DoSearchBadPass(self):
    """Perform full search for bad pass of bad item."""
    logger.GetLogger().LogOutput('Starting to bisect bad pass for bad item.')

    # Pass level bisection
    self.mode = 'pass'
    self.binary_search = binary_search_perforce.BinarySearcherForPass(
        logger_to_set=self.l)
    self.binary_search.total, _ = self.BuildWithPassLimit(-1)
    logger.GetLogger().LogOutput(
        'Total %s number: %d' % (self.mode, self.binary_search.total))

    pass_index, pass_name = self.DoBinarySearchBadPass()

    if (not pass_name and pass_index == 0):
      raise ValueError('Bisecting passes cannot reproduce good result.')
    logger.GetLogger().LogOutput('Bad pass found: %s.' % pass_name)

    # Transformation level bisection.
    logger.GetLogger().LogOutput('Starting to bisect at transformation level.')

    self.mode = 'transform'
    self.binary_search = binary_search_perforce.BinarySearcherForPass(
        logger_to_set=self.l)
    self.binary_search.total = self.BuildWithTransformLimit(
        -1, pass_name, pass_index)
    logger.GetLogger().LogOutput(
        'Total %s number: %d' % (self.mode, self.binary_search.total))

    trans_index, _ = self.DoBinarySearchBadPass(pass_index, pass_name)
    if trans_index == 0:
      raise ValueError('Bisecting %s cannot reproduce good result.' % pass_name)

    if self.ir_diff:
      self.PrintIRDiff(pass_index, pass_name, trans_index)

    logger.GetLogger().LogOutput(
        'Bisection result for bad item %s:\n'
        'Bad pass: %s at number %d\n'
        'Bad transformation number: %d' % (self.found_items, pass_name,
                                           pass_index, trans_index))

  def DoBinarySearchBadPass(self, pass_index=-1, pass_name=None):
    """Perform single iteration of binary search at pass level

    Args:
      pass_index: Works for transformation level bisection, indicates the limit
                  number of pass from pass level bisecting result.
      pass_name: Works for transformation level bisection, indicates
                 DebugCounter name of the bad pass from pass level bisecting
                 result.

    Returns:
      index: Index of problematic pass/transformation.
      pass_name: Works for pass level bisection, returns DebugCounter name for
                 bad pass.
    """
    # If in resume mode don't reset search_cycles
    if not self.resumed:
      self.search_cycles = 0
    else:
      self.resumed = False

    terminated = False
    index = 0
    while self.search_cycles < self.iterations and not terminated:
      self.SaveState()
      self.OutputIterationProgressBadPass()

      self.search_cycles += 1
      current = self.binary_search.GetNext()

      if self.mode == 'pass':
        index, pass_name = self.BuildWithPassLimit(current)
      else:
        self.BuildWithTransformLimit(current, pass_name, pass_index)
        index = current

      # TODO: Newly generated object should not directly replace original
      # one, need to put it somewhere and symbol link original one to it.
      # Will update cmd_script to do it.

      status = self.TestSetupScript()
      assert status == 0, 'Test setup should succeed.'
      status = self.TestScript()
      terminated = self.binary_search.SetStatus(status)

      if terminated:
        self.l.LogOutput('Terminated!', print_to_console=self.verbose)
    if not terminated:
      self.l.LogOutput('Ran out of iterations searching...')
    self.l.LogOutput(str(self), print_to_console=self.verbose)
    return index, pass_name

  def PopulateItemsUsingCommand(self, command):
    """Update all_items and binary search logic from executable.

    This method is mainly required for enumerating the initial list of items
    from the get_initial_items script.

    Args:
      command: path to executable that will enumerate items.
    """
    ce = command_executer.GetCommandExecuter()
    _, out, _ = ce.RunCommandWExceptionCleanup(
        command, return_output=True, print_to_console=self.verbose)
    all_items = out.split()
    self.PopulateItemsUsingList(all_items)

  def PopulateItemsUsingList(self, all_items):
    """Update all_items and binary searching logic from list.

    Args:
      all_items: new list of all_items
    """
    self.all_items = all_items
    self.binary_search = binary_search_perforce.BinarySearcher(
        logger_to_set=self.l)
    self.binary_search.SetSortedList(self.all_items)

  def SaveState(self):
    """Save state to STATE_FILE.

    SaveState will create a new unique, hidden state file to hold data from
    object. Then atomically overwrite the STATE_FILE symlink to point to the
    new data.

    Raises:
      OSError if STATE_FILE already exists but is not a symlink.
    """
    ce, l = self.ce, self.l
    self.ce, self.l, self.binary_search.logger = None, None, None
    old_state = None

    _, path = tempfile.mkstemp(prefix=HIDDEN_STATE_FILE, dir='.')
    with open(path, 'wb') as f:
      pickle.dump(self, f)

    if os.path.exists(STATE_FILE):
      if os.path.islink(STATE_FILE):
        old_state = os.readlink(STATE_FILE)
      else:
        raise OSError(('%s already exists and is not a symlink!\n'
                       'State file saved to %s' % (STATE_FILE, path)))

    # Create new link and atomically overwrite old link
    temp_link = '%s.link' % HIDDEN_STATE_FILE
    os.symlink(path, temp_link)
    os.rename(temp_link, STATE_FILE)

    if old_state:
      os.remove(old_state)

    self.ce, self.l, self.binary_search.logger = ce, l, l

  @classmethod
  def LoadState(cls):
    """Create BinarySearchState object from STATE_FILE."""
    if not os.path.isfile(STATE_FILE):
      return None
    try:
      with open(STATE_FILE, 'rb') as f:
        bss = pickle.load(f)
        bss.l = logger.GetLogger()
        bss.ce = command_executer.GetCommandExecuter()
        bss.binary_search.logger = bss.l
        bss.start_time = time.time()

        # Set resumed to be True so we can enter DoBinarySearch without the
        # method resetting our current search_cycles to 0.
        bss.resumed = True

        # Set currently_good_items and currently_bad_items to empty so that the
        # first iteration after resuming will always be non-incremental. This
        # is just in case the environment changes, the user makes manual
        # changes, or a previous switch_script corrupted the environment.
        bss.currently_good_items = set()
        bss.currently_bad_items = set()

        binary_search_perforce.verbose = bss.verbose
        return bss
    except Exception:
      return None

  def RemoveState(self):
    """Remove STATE_FILE and its symlinked data from file system."""
    if os.path.exists(STATE_FILE):
      if os.path.islink(STATE_FILE):
        real_file = os.readlink(STATE_FILE)
        os.remove(real_file)
        os.remove(STATE_FILE)

  def GetNextItems(self):
    """Get next items for binary search based on result of the last test run."""
    border_item = self.binary_search.GetNext()
    index = self.all_items.index(border_item)

    next_bad_items = self.all_items[:index + 1]
    next_good_items = self.all_items[index + 1:] + list(self.known_good)

    return [next_bad_items, next_good_items]

  def ElapsedTimeString(self):
    """Return h m s format of elapsed time since execution has started."""
    diff = int(time.time() - self.start_time)
    seconds = diff % 60
    minutes = (diff // 60) % 60
    hours = diff // (60 * 60)

    seconds = str(seconds).rjust(2)
    minutes = str(minutes).rjust(2)
    hours = str(hours).rjust(2)

    return '%sh %sm %ss' % (hours, minutes, seconds)

  def _OutputProgress(self, progress_text):
    """Output current progress of binary search to console and logs.

    Args:
      progress_text: The progress to display to the user.
    """
    progress = ('\n***** PROGRESS (elapsed time: %s) *****\n'
                '%s'
                '************************************************')
    progress = progress % (self.ElapsedTimeString(), progress_text)
    self.l.LogOutput(progress)

  def OutputIterationProgressBadItem(self):
    out = ('Search %d of estimated %d.\n'
           'Prune %d of max %d.\n'
           'Current bad items found:\n'
           '%s\n')
    out = out % (self.search_cycles + 1,
                 math.ceil(math.log(len(self.all_items), 2)), self.prune_cycles
                 + 1, self.prune_iterations, ', '.join(self.found_items))
    self._OutputProgress(out)

  def OutputIterationProgressBadPass(self):
    out = ('Search %d of estimated %d.\n' 'Current limit: %s\n')
    out = out % (self.search_cycles + 1,
                 math.ceil(math.log(self.binary_search.total, 2)),
                 self.binary_search.current)
    self._OutputProgress(out)

  def __str__(self):
    ret = ''
    ret += 'all: %s\n' % str(self.all_items)
    ret += 'currently_good: %s\n' % str(self.currently_good_items)
    ret += 'currently_bad: %s\n' % str(self.currently_bad_items)
    ret += str(self.binary_search)
    return ret


class MockBinarySearchState(BinarySearchState):
  """Mock class for BinarySearchState."""

  def __init__(self, **kwargs):
    # Initialize all arguments to None
    default_kwargs = {
        'get_initial_items': 'echo "1"',
        'switch_to_good': None,
        'switch_to_bad': None,
        'test_setup_script': None,
        'test_script': None,
        'incremental': True,
        'prune': False,
        'pass_bisect': None,
        'ir_diff': False,
        'iterations': 50,
        'prune_iterations': 100,
        'verify': True,
        'file_args': False,
        'verbose': False
    }
    default_kwargs.update(kwargs)
    super(MockBinarySearchState, self).__init__(**default_kwargs)


def _CanonicalizeScript(script_name):
  """Return canonical path to script.

  Args:
    script_name: Relative or absolute path to script

  Returns:
    Canonicalized script path
  """
  script_name = os.path.expanduser(script_name)
  if not script_name.startswith('/'):
    return os.path.join('.', script_name)


def Run(get_initial_items,
        switch_to_good,
        switch_to_bad,
        test_script,
        test_setup_script=None,
        iterations=50,
        prune=False,
        pass_bisect=None,
        ir_diff=False,
        noincremental=False,
        file_args=False,
        verify=True,
        prune_iterations=100,
        verbose=False,
        resume=False):
  """Run binary search tool.

  Equivalent to running through terminal.

  Args:
    get_initial_items: Script to enumerate all items being binary searched
    switch_to_good: Script that will take items as input and switch them to good
      set
    switch_to_bad: Script that will take items as input and switch them to bad
      set
    test_script: Script that will determine if the current combination of good
      and bad items make a "good" or "bad" result.
    test_setup_script: Script to do necessary setup (building, compilation,
      etc.) for test_script.
    iterations: How many binary search iterations to run before exiting.
    prune: If False the binary search tool will stop when the first bad item is
      found. Otherwise then binary search tool will continue searching until all
      bad items are found (or prune_iterations is reached).
    pass_bisect: Script that takes single bad item from POPULATE_BAD and returns
      the compiler command used to generate the bad item. This will turn on
      pass/ transformation level bisection for the bad item. Requires that
      'prune' be set to False, and needs support of `-opt-bisect-limit`(pass)
      and `-print-debug-counter`(transformation) from LLVM.
    ir_diff: Whether to print IR differences before and after bad
      pass/transformation to verbose output. Defaults to False, only works when
      pass_bisect is enabled.
    noincremental: Whether to send "diffs" of good/bad items to switch scripts.
    file_args: If True then arguments to switch scripts will be a file name
      containing a newline separated list of the items to switch.
    verify: If True, run tests to ensure initial good/bad sets actually produce
      a good/bad result.
    prune_iterations: Max number of bad items to search for.
    verbose: If True will print extra debug information to user.
    resume: If True will resume using STATE_FILE.

  Returns:
    0 for success, error otherwise
  """
  # Notice that all the argument checks are in the Run() function rather than
  # in the Main() function. It is not common to do so but some wrappers are
  # going to call Run() directly and bypass checks in Main() function.
  if resume:
    logger.GetLogger().LogOutput('Resuming from %s' % STATE_FILE)
    bss = BinarySearchState.LoadState()
    if not bss:
      logger.GetLogger().LogOutput(
          '%s is not a valid binary_search_tool state file, cannot resume!' %
          STATE_FILE)
      return 1
    logger.GetLogger().LogOutput('Note: resuming from previous state, '
                                 'ignoring given options and loading saved '
                                 'options instead.')
  else:
    if not (get_initial_items and switch_to_good and switch_to_bad and
            test_script):
      logger.GetLogger().LogOutput('The following options are required: '
                                   '[-i, -g, -b, -t] | [-r]')
      return 1
    if pass_bisect and prune:
      logger.GetLogger().LogOutput('"--pass_bisect" only works when '
                                   '"--prune" is set to be False.')
      return 1
    if not pass_bisect and ir_diff:
      logger.GetLogger().LogOutput('"--ir_diff" only works when '
                                   '"--pass_bisect" is enabled.')

    switch_to_good = _CanonicalizeScript(switch_to_good)
    switch_to_bad = _CanonicalizeScript(switch_to_bad)
    if test_setup_script:
      test_setup_script = _CanonicalizeScript(test_setup_script)
    if pass_bisect:
      pass_bisect = _CanonicalizeScript(pass_bisect)
    test_script = _CanonicalizeScript(test_script)
    get_initial_items = _CanonicalizeScript(get_initial_items)
    incremental = not noincremental

    binary_search_perforce.verbose = verbose

    bss = BinarySearchState(get_initial_items, switch_to_good, switch_to_bad,
                            test_setup_script, test_script, incremental, prune,
                            pass_bisect, ir_diff, iterations, prune_iterations,
                            verify, file_args, verbose)
    bss.DoVerify()

  bss.DoSearchBadItems()
  if pass_bisect:
    bss.DoSearchBadPass()
  bss.RemoveState()
  logger.GetLogger().LogOutput(
      'Total execution time: %s' % bss.ElapsedTimeString())

  return 0


def Main(argv):
  """The main function."""
  # Common initializations

  parser = argparse.ArgumentParser()
  common.BuildArgParser(parser)
  logger.GetLogger().LogOutput(' '.join(argv))
  options = parser.parse_args(argv)

  # Get dictionary of all options
  args = vars(options)
  return Run(**args)


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
