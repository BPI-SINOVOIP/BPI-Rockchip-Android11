#!/usr/bin/python2
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for server/cros/host_lock_manager.py."""
import mock
import unittest
import common

from autotest_lib.server.cros import host_lock_manager
from autotest_lib.server.cros.chaos_lib import chaos_datastore_utils


class HostLockManagerTest(unittest.TestCase):
    """Unit tests for host_lock_manager.HostLockManager.

    @attribute HOST1: a string, fake host.
    @attribute HOST2: a string, fake host.
    @attribute HOST3: a string, fake host.
    """

    HOST1 = 'host1'
    HOST2 = 'host2'
    HOST3 = 'host3'


    class MockHostLockManager(host_lock_manager.HostLockManager):
        """Mock out _host_modifier() in HostLockManager class..

        @attribute locked: a boolean, True == host is locked.
        @attribute locked_by: a string, fake user.
        @attribute lock_time: a string, fake timestamp.
        """

        def _host_modifier(self, hosts, operation, lock_reason=''):
            """Overwrites original _host_modifier().

            Add hosts to self.locked_hosts for LOCK and remove hosts from
            self.locked_hosts for UNLOCK.

            @param a set of strings, host names.
            @param operation: a string, LOCK or UNLOCK.
            @param lock_reason: a string, a reason for locking the hosts
            """
            if operation == self.LOCK:
                assert lock_reason
                self.locked_hosts = self.locked_hosts.union(hosts)
            elif operation == self.UNLOCK:
                self.locked_hosts = self.locked_hosts.difference(hosts)


    def setUp(self):
        super(HostLockManagerTest, self).setUp()
        self.manager = host_lock_manager.HostLockManager()


    # Patch mock object to return host as unknown from DataStore
    @mock.patch.object(chaos_datastore_utils.ChaosDataStoreUtils, 'show_device',
        return_value=False)
    def testCheckHost_SkipsUnknownHost(self, get_mock):
        actual = self.manager._check_host('host1', None)
        self.assertEquals(None, actual)


    @mock.patch.object(chaos_datastore_utils.ChaosDataStoreUtils, 'show_device',
        return_value={'lock_status': True, 'locked_by': 'Mock',
        'lock_status_updated': 'fake_time'})
    def testCheckHost_DetectsLockedHost(self, get_mock):
        """Test that a host which is already locked is skipped."""
        actual = self.manager._check_host(self.HOST1, self.manager.LOCK)
        self.assertEquals(None, actual)


    @mock.patch.object(chaos_datastore_utils.ChaosDataStoreUtils, 'show_device',
        return_value={'lock_status': False, 'locked_by': 'Mock',
        'lock_status_updated': 'fake_time'})
    def testCheckHost_DetectsUnlockedHost(self, get_mock):
        """Test that a host which is already unlocked is skipped."""
        actual = self.manager._check_host(self.HOST1, self.manager.UNLOCK)
        self.assertEquals(None, actual)


    @mock.patch.object(chaos_datastore_utils.ChaosDataStoreUtils, 'show_device',
        return_value={'lock_status': False, 'locked_by': 'Mock',
        'lock_status_updated': 'fake_time'})
    def testCheckHost_ReturnsHostToLock(self, get_mock):
        """Test that a host which can be locked is returned."""
        actual = self.manager._check_host(self.HOST1, self.manager.LOCK)
        self.assertEquals(self.HOST1, actual)


    @mock.patch.object(chaos_datastore_utils.ChaosDataStoreUtils, 'show_device',
        return_value={'lock_status': True, 'locked_by': 'Mock',
        'lock_status_updated': 'fake_time'})
    def testCheckHost_ReturnsHostToUnlock(self, get_mock):
        """Test that a host which can be unlocked is returned."""
        actual = self.manager._check_host(self.HOST1, self.manager.UNLOCK)
        self.assertEquals(self.HOST1, actual)


    def testLock_WithNonOverlappingHosts(self):
        """Tests host locking, all hosts not in self.locked_hosts."""
        hosts = [self.HOST2]
        manager = self.MockHostLockManager()
        manager.locked_hosts = set([self.HOST1])
        manager.lock(hosts, lock_reason='Locking for test')
        self.assertEquals(set([self.HOST1, self.HOST2]), manager.locked_hosts)


    def testLock_WithPartialOverlappingHosts(self):
        """Tests host locking, some hosts not in self.locked_hosts."""
        hosts = [self.HOST1, self.HOST2]
        manager = self.MockHostLockManager()
        manager.locked_hosts = set([self.HOST1, self.HOST3])
        manager.lock(hosts, lock_reason='Locking for test')
        self.assertEquals(set([self.HOST1, self.HOST2, self.HOST3]),
                          manager.locked_hosts)


    def testLock_WithFullyOverlappingHosts(self):
        """Tests host locking, all hosts in self.locked_hosts."""
        hosts = [self.HOST1, self.HOST2]
        self.manager.locked_hosts = set(hosts)
        self.manager.lock(hosts)
        self.assertEquals(set(hosts), self.manager.locked_hosts)


    def testUnlock_WithNonOverlappingHosts(self):
        """Tests host unlocking, all hosts not in self.locked_hosts."""
        hosts = [self.HOST2]
        self.manager.locked_hosts = set([self.HOST1])
        self.manager.unlock(hosts)
        self.assertEquals(set([self.HOST1]), self.manager.locked_hosts)


if __name__ == '__main__':
    unittest.main()
