# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import common
from autotest_lib.client.common_lib import hosts
from autotest_lib.server.hosts import repair_utils


class _LabstationUpdateVerifier(hosts.Verifier):
    """
    Verifier to trigger a labstation update, if necessary.

    The operation doesn't wait for the update to complete and is
    considered a success whether or not the servo is currently
    up-to-date.
    """

    def verify(self, host):
        """First, only run this verifier if the host is in the physical lab.
        Secondly, skip if the test is being run by test_that, because subnet
        restrictions can cause the update to fail.
        """
        if host.is_in_lab() and host.job and host.job.in_lab:
            host.update_image(wait_for_update=False)

    @property
    def description(self):
        return 'Labstation image is updated to current stable-version'


class _LabstationRebootVerifier(hosts.Verifier):
    """Check if reboot is need for the labstation and perform a reboot if it's
    not currently using by any tests.
    """
    def verify(self, host):
        if host.is_reboot_requested():
            host.try_reboot()

    @property
    def description(self):
        return 'Reboot labstation if requested and the labstation is not in use'


def create_labstation_repair_strategy():
    """
    Return a `RepairStrategy` for a `LabstationHost`.
    """
    verify_dag = [
        (repair_utils.SshVerifier,   'ssh',     []),
        (_LabstationUpdateVerifier,  'update',  ['ssh']),
        (_LabstationRebootVerifier,  'reboot',  ['ssh']),
    ]

    repair_actions = [
        (repair_utils.RPMCycleRepair, 'rpm', [], ['ssh']),
    ]
    return hosts.RepairStrategy(verify_dag, repair_actions, 'labstation')