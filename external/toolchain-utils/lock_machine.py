#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module controls locking and unlocking of test machines."""

from __future__ import print_function

import argparse
import enum
import getpass
import os
import sys

import file_lock_machine

from cros_utils import command_executer
from cros_utils import logger
from cros_utils import machines


class LockException(Exception):
  """Base class for exceptions in this module."""


class MachineNotPingable(LockException):
  """Raised when machine does not respond to ping."""


class LockingError(LockException):
  """Raised when server fails to lock/unlock machine as requested."""


class DontOwnLock(LockException):
  """Raised when user attmepts to unlock machine locked by someone else."""
  # This should not be raised if the user specified '--force'


class MachineType(enum.Enum):
  """Enum class to hold machine type."""
  LOCAL = 'local'
  SKYLAB = 'skylab'


class LockManager(object):
  """Class for locking/unlocking machines vie three different modes.

  This class contains methods for checking the locked status of machines,
  and for changing the locked status.  It handles HW lab machines and local
  machines, using appropriate locking mechanisms for each.
  """

  SKYLAB_PATH = '/usr/local/bin/skylab'

  # TODO(zhizhouy): lease time may needs to be dynamically adjusted. For now we
  # set it long enough to cover the period to finish nightly rotation tests.
  LEASE_MINS = 1440

  SKYLAB_CREDENTIAL = ('/usr/local/google/home/mobiletc-prebuild'
                       '/sheriff_utils/credentials/skylab'
                       '/chromeos-swarming-credential.json')
  SWARMING = 'chromite/third_party/swarming.client/swarming.py'
  SUCCESS = 0

  def __init__(self,
               remotes,
               force_option,
               chromeos_root,
               locks_dir='',
               log=None):
    """Initializes an LockManager object.

    Args:
      remotes: A list of machine names or ip addresses to be managed.  Names
        and ip addresses should be represented as strings.  If the list is
        empty, the lock manager will get all known machines.
      force_option: A Boolean indicating whether or not to force an unlock of
        a machine that was locked by someone else.
      chromeos_root: The ChromeOS chroot to use for the autotest scripts.
      locks_dir: A directory used for file locking local devices.
      log: If not None, this is the logger object to be used for writing out
        informational output messages.  It is expected to be an instance of
        Logger class from cros_utils/logger.py.
    """
    self.chromeos_root = chromeos_root
    self.user = getpass.getuser()
    self.logger = log or logger.GetLogger()
    self.ce = command_executer.GetCommandExecuter(self.logger)

    sys.path.append(chromeos_root)

    self.locks_dir = locks_dir

    self.machines = list(set(remotes)) or []
    self.toolchain_lab_machines = self.GetAllToolchainLabMachines()

    if not self.machines:
      self.machines = self.toolchain_lab_machines
    self.force = force_option

    self.local_machines = []
    self.skylab_machines = []

  def CheckMachine(self, machine, error_msg):
    """Verifies that machine is responding to ping.

    Args:
      machine: String containing the name or ip address of machine to check.
      error_msg: Message to print if ping fails.

    Raises:
      MachineNotPingable:  If machine is not responding to 'ping'
    """
    if not machines.MachineIsPingable(machine, logging_level='none'):
      cros_machine = machine + '.cros'
      if not machines.MachineIsPingable(cros_machine, logging_level='none'):
        raise MachineNotPingable(error_msg)

  def GetAllToolchainLabMachines(self):
    """Gets a list of all the toolchain machines in the ChromeOS HW lab.

    Returns:
      A list of names of the toolchain machines in the ChromeOS HW lab.
    """
    machines_file = os.path.join(
        os.path.dirname(__file__), 'crosperf', 'default_remotes')
    machine_list = []
    with open(machines_file, 'r') as input_file:
      lines = input_file.readlines()
      for line in lines:
        _, remotes = line.split(':')
        remotes = remotes.strip()
        for r in remotes.split():
          machine_list.append(r.strip())
    return machine_list

  def GetMachineType(self, m):
    """Get where the machine is located.

    Args:
      m: String containing the name or ip address of machine.

    Returns:
      Value of the type in MachineType Enum.
    """
    if m in self.local_machines:
      return MachineType.LOCAL
    if m in self.skylab_machines:
      return MachineType.SKYLAB

  def PrintStatusHeader(self):
    """Prints the status header lines for machines."""
    print('\nMachine (Board)\t\t\t\t\tStatus')
    print('---------------\t\t\t\t\t------')

  def PrintStatus(self, m, state, machine_type):
    """Prints status for a single machine.

    Args:
      m: String containing the name or ip address of machine.
      state: A dictionary of the current state of the machine.
      machine_type: MachineType to determine where the machine is located.
    """
    if state['locked']:
      print('%s (%s)\t\t%slocked by %s since %s' %
            (m, state['board'], '\t\t' if machine_type == MachineType.LOCAL else
             '', state['locked_by'], state['lock_time']))
    else:
      print(
          '%s (%s)\t\t%sunlocked' % (m, state['board'], '\t\t' if
                                     machine_type == MachineType.LOCAL else ''))

  def AddMachineToLocal(self, machine):
    """Adds a machine to local machine list.

    Args:
      machine: The machine to be added.
    """
    if machine not in self.local_machines:
      self.local_machines.append(machine)

  def AddMachineToSkylab(self, machine):
    """Adds a machine to skylab machine list.

    Args:
      machine: The machine to be added.
    """
    if machine not in self.skylab_machines:
      self.skylab_machines.append(machine)

  def ListMachineStates(self, machine_states):
    """Gets and prints the current status for a list of machines.

    Prints out the current status for all of the machines in the current
    LockManager's list of machines (set when the object is initialized).

    Args:
      machine_states: A dictionary of the current state of every machine in
        the current LockManager's list of machines.  Normally obtained by
        calling LockManager::GetMachineStates.
    """
    self.PrintStatusHeader()
    for m in machine_states:
      machine_type = self.GetMachineType(m)
      state = machine_states[m]
      self.PrintStatus(m, state, machine_type)

  def UpdateLockInSkylab(self, should_lock_machine, machine):
    """Ask skylab to lease/release a machine.

    Args:
      should_lock_machine: Boolean indicating whether to lock the machine (True)
        or unlock the machine (False).
      machine: The machine to update.

    Returns:
      True if requested action succeeded, else False.
    """
    try:
      if should_lock_machine:
        ret = self.LeaseSkylabMachine(machine)
      else:
        ret = self.ReleaseSkylabMachine(machine)
    except Exception:
      return False
    return ret

  def UpdateFileLock(self, should_lock_machine, machine):
    """Use file lock for local machines,

    Args:
      should_lock_machine: Boolean indicating whether to lock the machine (True)
        or unlock the machine (False).
      machine: The machine to update.

    Returns:
      True if requested action succeeded, else False.
    """
    try:
      if should_lock_machine:
        ret = file_lock_machine.Machine(machine, self.locks_dir).Lock(
            True, sys.argv[0])
      else:
        ret = file_lock_machine.Machine(machine, self.locks_dir).Unlock(True)
    except Exception:
      return False
    return ret

  def UpdateMachines(self, lock_machines):
    """Sets the locked state of the machines to the requested value.

    The machines updated are the ones in self.machines (specified when the
    class object was intialized).

    Args:
      lock_machines: Boolean indicating whether to lock the machines (True) or
        unlock the machines (False).

    Returns:
      A list of the machines whose state was successfully updated.
    """
    updated_machines = []
    action = 'Locking' if lock_machines else 'Unlocking'
    for m in self.machines:
      # TODO(zhizhouy): Handling exceptions with more details when locking
      # doesn't succeed.
      machine_type = self.GetMachineType(m)
      if machine_type == MachineType.SKYLAB:
        ret = self.UpdateLockInSkylab(lock_machines, m)
      elif machine_type == MachineType.LOCAL:
        ret = self.UpdateFileLock(lock_machines, m)

      if ret:
        self.logger.LogOutput(
            '%s %s machine succeeded: %s.' % (action, machine_type.value, m))
        updated_machines.append(m)
      else:
        self.logger.LogOutput(
            '%s %s machine failed: %s.' % (action, machine_type.value, m))

    self.machines = updated_machines
    return updated_machines

  def _InternalRemoveMachine(self, machine):
    """Remove machine from internal list of machines.

    Args:
      machine: Name of machine to be removed from internal list.
    """
    # Check to see if machine is lab machine and if so, make sure it has
    # ".cros" on the end.
    cros_machine = machine
    if machine.find('rack') > 0 and machine.find('row') > 0:
      if machine.find('.cros') == -1:
        cros_machine = cros_machine + '.cros'

    self.machines = [
        m for m in self.machines if m not in (cros_machine, machine)
    ]

  def CheckMachineLocks(self, machine_states, cmd):
    """Check that every machine in requested list is in the proper state.

    If the cmd is 'unlock' verify that every machine is locked by requestor.
    If the cmd is 'lock' verify that every machine is currently unlocked.

    Args:
      machine_states: A dictionary of the current state of every machine in
        the current LockManager's list of machines.  Normally obtained by
        calling LockManager::GetMachineStates.
      cmd: The user-requested action for the machines: 'lock' or 'unlock'.

    Raises:
      DontOwnLock: The lock on a requested machine is owned by someone else.
    """
    for k, state in machine_states.items():
      if cmd == 'unlock':
        if not state['locked']:
          self.logger.LogWarning('Attempt to unlock already unlocked machine '
                                 '(%s).' % k)
          self._InternalRemoveMachine(k)

        # TODO(zhizhouy): Skylab doesn't support host info such as locked_by.
        # Need to update this when skylab supports it.
        if (state['locked'] and state['locked_by'] and
            state['locked_by'] != self.user):
          raise DontOwnLock('Attempt to unlock machine (%s) locked by someone '
                            'else (%s).' % (k, state['locked_by']))
      elif cmd == 'lock':
        if state['locked']:
          self.logger.LogWarning(
              'Attempt to lock already locked machine (%s)' % k)
          self._InternalRemoveMachine(k)

  def GetMachineStates(self, cmd=''):
    """Gets the current state of all the requested machines.

    Gets the current state of all the requested machines. Stores the data in a
    dictionary keyed by machine name.

    Args:
      cmd: The command for which we are getting the machine states. This is
        important because if one of the requested machines is missing we raise
        an exception, unless the requested command is 'add'.

    Returns:
      A dictionary of machine states for all the machines in the LockManager
      object.
    """
    machine_list = {}
    for m in self.machines:
      # For local or skylab machines, we simply set {'locked': status} for them
      # TODO(zhizhouy): This is a quick fix since skylab cannot return host info
      # as afe does. We need to get more info such as locked_by when skylab
      # supports that.
      values = {
          'locked': 0 if cmd == 'lock' else 1,
          'board': '??',
          'locked_by': '',
          'lock_time': ''
      }
      machine_list[m] = values

    self.ListMachineStates(machine_list)

    return machine_list

  def CheckMachineInSkylab(self, machine):
    """Run command to check if machine is in Skylab or not.

    Returns:
      True if machine in skylab, else False
    """
    credential = ''
    if os.path.exists(self.SKYLAB_CREDENTIAL):
      credential = '--auth-service-account-json %s' % self.SKYLAB_CREDENTIAL
    swarming = os.path.join(self.chromeos_root, self.SWARMING)
    cmd = (('%s query --swarming https://chromeos-swarming.appspot.com ' \
            "%s 'bots/list?is_dead=FALSE&dimensions=dut_name:%s'") % \
           (swarming,
            credential,
            machine.rstrip('.cros')))
    ret_tup = self.ce.RunCommandWOutput(cmd)
    # The command will return a json output as stdout. If machine not in skylab
    # stdout will look like this:
    #  {
    #    "death_timeout": "600",
    #    "now": "TIMESTAMP"
    #  }
    # Otherwise there will be a tuple starting with 'items', we simply detect
    # this keyword for result.
    if 'items' not in ret_tup[1]:
      return False
    else:
      return True

  def LeaseSkylabMachine(self, machine):
    """Run command to lease dut from skylab.

    Returns:
      True if succeeded, False if failed.
    """
    credential = ''
    if os.path.exists(self.SKYLAB_CREDENTIAL):
      credential = '-service-account-json %s' % self.SKYLAB_CREDENTIAL
    cmd = (('%s lease-dut -minutes %s %s %s') % \
           (self.SKYLAB_PATH,
            self.LEASE_MINS,
            credential,
            machine.rstrip('.cros')))
    # Wait 120 seconds for server to start the lease task, if not started,
    # we will treat it as unavailable.
    check_interval_time = 120
    retval = self.ce.RunCommand(cmd, command_timeout=check_interval_time)
    return retval == self.SUCCESS

  def ReleaseSkylabMachine(self, machine):
    """Run command to release dut from skylab.

    Returns:
      True if succeeded, False if failed.
    """
    credential = ''
    if os.path.exists(self.SKYLAB_CREDENTIAL):
      credential = '-service-account-json %s' % self.SKYLAB_CREDENTIAL
    cmd = (('%s release-dut %s %s') % \
           (self.SKYLAB_PATH,
            credential,
            machine.rstrip('.cros')))
    retval = self.ce.RunCommand(cmd)
    return retval == self.SUCCESS


def Main(argv):
  """Parse the options, initialize lock manager and dispatch proper method.

  Args:
    argv: The options with which this script was invoked.

  Returns:
    0 unless an exception is raised.
  """
  parser = argparse.ArgumentParser()

  parser.add_argument(
      '--list',
      dest='cmd',
      action='store_const',
      const='status',
      help='List current status of all known machines.')
  parser.add_argument(
      '--lock',
      dest='cmd',
      action='store_const',
      const='lock',
      help='Lock given machine(s).')
  parser.add_argument(
      '--unlock',
      dest='cmd',
      action='store_const',
      const='unlock',
      help='Unlock given machine(s).')
  parser.add_argument(
      '--status',
      dest='cmd',
      action='store_const',
      const='status',
      help='List current status of given machine(s).')
  parser.add_argument(
      '--remote', dest='remote', help='machines on which to operate')
  parser.add_argument(
      '--chromeos_root',
      dest='chromeos_root',
      required=True,
      help='ChromeOS root to use for autotest scripts.')
  parser.add_argument(
      '--force',
      dest='force',
      action='store_true',
      default=False,
      help='Force lock/unlock of machines, even if not'
      ' current lock owner.')

  options = parser.parse_args(argv)

  if not options.remote and options.cmd != 'status':
    parser.error('No machines specified for operation.')

  if not os.path.isdir(options.chromeos_root):
    parser.error('Cannot find chromeos_root: %s.' % options.chromeos_root)

  if not options.cmd:
    parser.error('No operation selected (--list, --status, --lock, --unlock,'
                 ' --add_machine, --remove_machine).')

  machine_list = []
  if options.remote:
    machine_list = options.remote.split()

  lock_manager = LockManager(machine_list, options.force, options.chromeos_root)

  machine_states = lock_manager.GetMachineStates(cmd=options.cmd)
  cmd = options.cmd

  if cmd == 'status':
    lock_manager.ListMachineStates(machine_states)

  elif cmd == 'lock':
    if not lock_manager.force:
      lock_manager.CheckMachineLocks(machine_states, cmd)
      lock_manager.UpdateMachines(True)

  elif cmd == 'unlock':
    if not lock_manager.force:
      lock_manager.CheckMachineLocks(machine_states, cmd)
      lock_manager.UpdateMachines(False)

  elif cmd == 'add':
    lock_manager.AddMachinesToLocalServer()

  elif cmd == 'remove':
    lock_manager.RemoveMachinesFromLocalServer()

  return 0


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
