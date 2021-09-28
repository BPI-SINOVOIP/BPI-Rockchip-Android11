# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import subprocess

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.server import test
from autotest_lib.server import frontend

class moblab_Setup(test.test):
    """ Moblab server test that checks for a specified number of
    connected DUTs and that those DUTs have specified labels. Used to
    verify the setup before kicking off a long running test suite.
    """
    version = 1

    def run_once(self, required_duts=1, required_labels=[]):
        """ Tests the moblab's connected DUTs to see if the current
        configuration is valid for a specific test.

        @param required_duts [int] number of _live_ DUTs required to run
            the test in question. A DUT is not live if it is in a failed
            repair state
        @param required_labels [list<string>] list of labels that are
            required to be on at least one _live_ DUT for this test.
        """
        logging.info('required_duts=%d required_labels=%s' %
                (required_duts, str(required_labels)))

        # creating a client to connect to autotest rpc interface
        # all available rpc calls are defined in
        # src/third_party/autotest/files/server/frontend.py
        afe = frontend.AFE(server='localhost', user='moblab')

        # get autotest statuses that indicate a live host
        live_statuses = afe.host_statuses(live=True)
        hosts = []
        # get the hosts connected to autotest, find the live ones
        for host in afe.get_hosts():
            if host.status in live_statuses:
                logging.info('Host %s is live, status %s' %
                        (host.hostname, host.status))
                hosts.append(host)
            else:
                logging.info('Host %s is not live, status %s' %
                        (host.hostname, host.status))

        # check that we have the required number of live duts
        if len(hosts) < required_duts:
            raise error.TestFail(('Suite requires %d DUTs, only %d connected' %
                    (required_duts, len(hosts))))

        required_labels_found = {}
        for label in required_labels:
            required_labels_found[label] = False

        # check that at least one DUT has each required label
        for host in hosts:
            for label in host.get_labels():
                if label.name in required_labels_found:
                    required_labels_found[label.name] = True
        # note: pools are stored as specially formatted labels
        # to find if a DUT is in a pool,
        # check if it has the label pool:mypoolname
        for key in required_labels_found:
            if not required_labels_found[key]:
                raise error.TestFail('No DUT with required label %s' % key)

        return

        # to have autotest reverify that hosts are live, use the reverify_hosts
        # rpc call
        # note: this schedules a background asynchronous job, and
        # logic to check back in on hosts would need to be built
        # reverify_hostnames = [host.hostname for host in hosts]
        # afe.reverify_hosts(hostnames=reverify_hostnames)

        # example of running a command on the dut and getting the output back
        # def run_ssh_command_on_dut(hostname, cmd):
        #     """ Run a command on a DUT via ssh
        #
        #     @return output of the command
        #     @raises subprocess.CalledProcessError if the ssh command fails,
        #         such as a connection couldn't be established
        #     """
        #     ssh_cmd = ('ssh -o ConnectTimeout=2 -o StrictHostKeyChecking=no '
        #             "root@%s '%s'") % (hostname, cmd)
        #     return subprocess.check_output(ssh_cmd, shell=True)
        # for host in hosts:
        #     logging.info(run_ssh_command_on_dut(
        #             host.hostname, 'cat /etc/lsb-release'))
