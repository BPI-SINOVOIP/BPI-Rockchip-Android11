# Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server import test
from autotest_lib.server import frontend

class hardware_StorageQualCheckSetup(test.test):
    """
    Verifies the moblab and DUT setup for storage qual
    A correct setup consists of
        At least one pool (the default "no pool" counts as a pool)
        Each of the labels [retention, trim, suspend] is applied to exactly
            one DUT in the pool.
        No duplication of the labels, all labels are applied exactly once
            per pool

    The test will verify this set up, for the pool that is selected on the
    RunSuite page (by getting the pool of the DUT running the test). If this
    test passes, we have confidence that this individual pool is a valid setup
    for storage qual.
    """

    version = 1

    REQUIRED_LABELS = ['retention', 'trim', 'suspend']

    def _group_hosts_into_pools(self, hosts):
        pools = {}
        for host in hosts:
            labels = [label.name for label in host.get_labels()]

            pool_name = 'none'
            for label in labels:
                if 'pool:' in label:
                    pool_name = label.replace('pool:', '')

            if pool_name not in pools:
                pools[pool_name] = []

            pools[pool_name].append({
                'host': host.hostname,
                'labels': labels
            })

        return pools

    def _get_running_host_pool(self):
        host = list(self.job.hosts)[0]
        pools = host.host_info_store.get().pools
        return list(pools)[0] if len(pools) > 0 else 'none'

    def run_once(self):
        """ Tests the moblab's connected DUTs to see if the current
        configuration is valid for storage qual
        """

        # get the pool of the host this test is running on
        pool_name = self._get_running_host_pool()
        logging.info('Test is running on pool %s', pool_name)

        afe = frontend.AFE(server='localhost', user='moblab')

        # get autotest statuses that indicate a live host
        live_statuses = afe.host_statuses(live=True)

        # get the hosts connected to autotest, find the live ones
        hosts = []
        for host in afe.get_hosts():
            if host.status in live_statuses:
                logging.info('Host %s is live, status %s',
                        host.hostname, host.status)
                hosts.append(host)
            else:
                logging.info('Host %s is not live, status %s',
                        host.hostname, host.status)

        pools = self._group_hosts_into_pools(hosts)

        # verify that the pool is set up to run storage qual, with correct
        # number of DUTs and correct labels
        required_set = set(self.REQUIRED_LABELS)
        provided_set = set()
        for host in pools[pool_name]:
            host_provided_labels = set(host['labels']) & required_set
            # check that each DUT has at most 1 storage qual label
            if len(host_provided_labels) > 1:
                raise error.TestFail(
                    ('Host %s is assigned more than '
                        'one storage qual label %s') %
                        (host['host'], str(host_provided_labels)))
            if len(host_provided_labels) == 0:
                continue

            # check that each label is only on one DUT in the pool
            provided_label = host_provided_labels.pop()
            if provided_label in provided_set:
                raise error.TestFail(
                    ('Host %s is assigned label %s, which is already '
                      'assigned to another DUT in pool %s') %
                        (host['host'], provided_label, pool_name)
                )

            provided_set.add(provided_label)
            logging.info(' - %s %s', host['host'], provided_label)

        # check that all storage qual labels are accounted for in the pool
        missing_labels = required_set - provided_set
        if len(missing_labels) > 0:
            raise error.TestFail(
                'Pool %s is missing required labels %s' %
                    (pool_name, str(missing_labels))
                )

        return
