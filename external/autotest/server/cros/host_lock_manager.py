# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import signal
import common

from autotest_lib.server import site_utils
from autotest_lib.server.cros.chaos_lib import chaos_datastore_utils
"""HostLockManager class, for the dynamic_suite module.

A HostLockManager instance manages locking and unlocking a set of autotest DUTs.
A caller can lock or unlock one or more DUTs. If the caller fails to unlock()
locked hosts before the instance is destroyed, it will attempt to unlock() the
hosts automatically, but this is to be avoided.

Sample usage:
  manager = host_lock_manager.HostLockManager()
  try:
      manager.lock(['host1'])
      # do things
  finally:
      manager.unlock()
"""

class HostLockManager(object):
    """
    @attribute _afe: an instance of AFE as defined in server/frontend.py.
    @attribute _locked_hosts: a set of DUT hostnames.
    @attribute LOCK: a string.
    @attribute UNLOCK: a string.
    """

    LOCK = 'lock'
    UNLOCK = 'unlock'


    @property
    def locked_hosts(self):
        """@returns set of locked hosts."""
        return self._locked_hosts


    @locked_hosts.setter
    def locked_hosts(self, hosts):
        """Sets value of locked_hosts.

        @param hosts: a set of strings.
        """
        self._locked_hosts = hosts


    def __init__(self, afe=None):
        """
        Constructor
        """
        self.dutils = chaos_datastore_utils.ChaosDataStoreUtils()
        # Keep track of hosts locked by this instance.
        self._locked_hosts = set()


    def __del__(self):
        if self._locked_hosts:
            logging.warning('Caller failed to unlock %r! Forcing unlock now.',
                            self._locked_hosts)
            self.unlock()


    def _check_host(self, host, operation):
        """Checks host for desired operation.

        @param host: a string, hostname.
        @param operation: a string, LOCK or UNLOCK.
        @returns a string: host name, if desired operation can be performed on
                           host or None otherwise.
        """
        host_checked = host
        # Get host details from DataStore
        host_info = self.dutils.show_device(host)

        if not host_info:
            logging.warning('Host (AP) details not found in DataStore')
            return None

        if operation == self.LOCK and host_info['lock_status']:
            err = ('Contention detected: %s is locked by %s at %s.' %
                   (host, host_info['locked_by'],
                    host_info['lock_status_updated']))
            logging.error(err)
            return None

        elif operation == self.UNLOCK and not host_info['lock_status']:
            logging.info('%s not locked.', host)
            return None

        return host_checked


    def lock(self, hosts, lock_reason='Locked by HostLockManager'):
        """Lock hosts in datastore.

        @param hosts: a list of strings, host names.
        @param lock_reason: a string, a reason for locking the hosts.

        @returns a boolean, True == at least one host from hosts is locked.
        """
        # Filter out hosts that we may have already locked
        new_hosts = set(hosts).difference(self._locked_hosts)
        logging.info('Attempt to lock %s', new_hosts)
        if not new_hosts:
            return False

        return self._host_modifier(new_hosts, self.LOCK, lock_reason=lock_reason)


    def unlock(self, hosts=None):
        """Unlock hosts in datastore after use.

        @param hosts: a list of strings, host names.
        @returns a boolean, True == at least one host from self._locked_hosts is
                 unlocked.
        """
        # Filter out hosts that we did not lock
        updated_hosts = self._locked_hosts
        if hosts:
            unknown_hosts = set(hosts).difference(self._locked_hosts)
            logging.warning('Skip unknown hosts: %s', unknown_hosts)
            updated_hosts = set(hosts) - unknown_hosts
            logging.info('Valid hosts: %s', updated_hosts)
            updated_hosts = updated_hosts.intersection(self._locked_hosts)

        if not updated_hosts:
            return False

        logging.info('Unlocking hosts (APs / PCAPs): %s', updated_hosts)
        return self._host_modifier(updated_hosts, self.UNLOCK)


    def _host_modifier(self, hosts, operation, lock_reason=None):
        """Helper that locks hosts in DataStore.

        @param: hosts, a set of strings, host names.
        @param operation: a string, LOCK or UNLOCK.
        @param lock_reason: a string, a reason must be provided when locking.

        @returns a boolean, if operation succeeded on at least one host in
                 hosts.
        """
        updated_hosts = set()
        for host in hosts:
            verified_host = self._check_host(host, operation)
            if verified_host is not None:
                updated_hosts.add(verified_host)

        logging.info('host_modifier: updated_hosts = %s', updated_hosts)
        if not updated_hosts:
            logging.info('host_modifier: no host to update')
            return False

        for host in updated_hosts:
            if operation == self.LOCK:
                if self.dutils.lock_device(host, lock_reason):
                    logging.info('Locked host in datastore: %s', host)
                    self._locked_hosts = self._locked_hosts.union([host])
                else:
                    logging.error('Unable to lock host: ', host)

            if operation == self.UNLOCK:
                if self.dutils.unlock_device(host):
                    logging.info('Unlocked host in datastore: %s', host)
                    self._locked_hosts = self._locked_hosts.difference([host])
                else:
                    logging.error('Unable to un-lock host: %s', host)

        return True


class HostsLockedBy(object):
    """Context manager to make sure that a HostLockManager will always unlock
    its machines. This protects against both exceptions and SIGTERM."""

    def _make_handler(self):
        def _chaining_signal_handler(signal_number, frame):
            self._manager.unlock()
            # self._old_handler can also be signal.SIG_{IGN,DFL} which are ints.
            if callable(self._old_handler):
                self._old_handler(signal_number, frame)
        return _chaining_signal_handler


    def __init__(self, manager):
        """
        @param manager: The HostLockManager used to lock the hosts.
        """
        self._manager = manager
        self._old_handler = signal.SIG_DFL


    def __enter__(self):
        self._old_handler = signal.signal(signal.SIGTERM, self._make_handler())


    def __exit__(self, exntype, exnvalue, backtrace):
        signal.signal(signal.SIGTERM, self._old_handler)
        self._manager.unlock()
