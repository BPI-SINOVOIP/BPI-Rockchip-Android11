# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import shutil
import traceback
import uuid
from autotest_lib.client.common_lib import error
from autotest_lib.client.bin import utils
from autotest_lib.server import test, autotest


LOWER_IS_BETTER_METRICS = set(['rdbytes', 'seconds'])


class platform_BootPerfServer(test.test):
    """A test that reboots the client and collect boot perf data."""
    version = 1

    def _is_rootfs_verification_enabled(self, host):
        """Helper function to check whether rootfs verification is enabled"""
        kernel_cmdline = host.run_output('cat /proc/cmdline')
        return 'dm_verity.dev_wait=1' in kernel_cmdline

    def _get_root_partition(self, host):
        """Helper function for getting the partition index from the system. """
        # Determine root partition
        rootdev = host.run_output('rootdev -s')
        # Sample value of rootdev: "/dev/mmcblk0p3" or "/dev/nvme0n1p3." For
        # "/dev/mmcblk0p3", the target is partition 2. Extract the last digit
        # to get the partition index.
        logging.info('rootdev: %s', rootdev)
        match = re.match(r'^/dev/.*\dp(\d+)$', rootdev)
        if match:
            return int(match.group(1)) - 1

        return None

    def _edit_kernel_args(self, host, gen_sed_command):
        """Helper function for editing kernel args."""
        partition = self._get_root_partition(host)
        if partition is None:
            logging.warn('Unable to get root partition index')
            return

        tmp_name = str(uuid.uuid4())

        # Save the current boot config.
        host.run(('/usr/share/vboot/bin/make_dev_ssd.sh --save_config /tmp/%s '
                  '--partitions %d') % (tmp_name, partition))
        # Add "cros_bootchart" to the boot config and then make it effective.
        tmp_file = '/tmp/%s.%d' % (tmp_name, partition)
        host.run(gen_sed_command(tmp_file))
        host.run(('/usr/share/vboot/bin/make_dev_ssd.sh --set_config /tmp/%s '
                '--partitions %d') % (tmp_name, partition))

    def initialize(self, host):
        """Initialization steps before running the test"""
        # Some tests might disable rootfs verification and mount rootfs as rw.
        # If we run after those tests, re-enable rootfs verification to get
        # consistent boot perf metrics.
        if not self._is_rootfs_verification_enabled(host):
            logging.info('Reimage to enable rootfs verification.')
            version = host.get_release_builder_path()
            # Force reimage to the current version to enable rootfs
            # verification.
            self.job.run_test('provision_AutoUpdate', host=host, value=version,
                              force_update_engine=True)

        # Bootchart is shipped but disabled by default in the image. Before
        # the test, enable by adding 'cros_bootchart' to the kernel arg list.
        kernel_cmdline = host.run_output('cat /proc/cmdline')
        if 'cros_bootchart' in kernel_cmdline:
            logging.warn('cros_bootchart is enabled before the test.')
            return

        logging.info('Enable bootchart.')
        self._edit_kernel_args(
            host,
            lambda tmp_file: 'sed -i "s/$/ cros_bootchart/g" %s' % tmp_file)

    def cleanup(self, host):
        """After running the test, disable cros_bootchart by removing
        "cros_bootchart" from the kernel arg list.
        """
        kernel_cmdline = host.run_output('cat /proc/cmdline')
        if 'cros_bootchart' not in kernel_cmdline:
            logging.warn('Bootchart not enabled in the test.')
            return

        logging.info('Disable cros_bootchart and reboot.')
        self._edit_kernel_args(
            host,
            lambda tmp_file: 'sed -i "s/ cros_bootchart//g" %s' % tmp_file)
        host.reboot()

    def upload_perf_keyvals(self, keyvals):
        """Upload perf keyvals in dictionary |keyvals| to Chrome perf dashboard.

        This method assumes that the key of a perf keyval is in the format
        of "units_description". The text before the first underscore represents
        the units and the rest of the text represents
        a description of the measured perf value. For instance,
        'seconds_kernel_to_login', 'rdbytes_kernel_to_startup'.

        @param keyvals: A dictionary that maps a perf metric to its value.

        """
        for key, val in keyvals.items():
            match = re.match(r'^(.+?)_.+$', key)
            if match:
                units = match.group(1)
                higher_is_better = units not in LOWER_IS_BETTER_METRICS
                self.output_perf_value(
                        description=key, value=val,
                        units=units, higher_is_better=higher_is_better)


    def run_once(self, host=None, upload_perf=False):
        """Runs the test once: reboot and collect boot metrics from DUT."""
        self.client = host
        self.client_test = 'platform_BootPerf'

        # Run a login test to complete the OOBE flow, if we haven't already.
        # This is so that we measure boot times for the stable state.
        try:
            self.client.run('ls /home/chronos/.oobe_completed')
        except error.AutoservRunError:
            logging.info('Taking client through OOBE.')
            client_at = autotest.Autotest(self.client)
            client_at.run_test('login_LoginSuccess', disable_sysinfo=True)

        # Reboot the client
        logging.info('BootPerfServer: reboot %s', self.client.hostname)
        try:
            self.client.reboot(reboot_timeout=90)
        except error.AutoservRebootError as e:
            raise error.TestFail('%s.\nTest failed with error %s' % (
                    traceback.format_exc(), str(e)))

        # Collect the performance metrics by running a client side test
        logging.info('BootPerfServer: start client test')
        client_at = autotest.Autotest(self.client)
        client_at.run_test(
            self.client_test, last_boot_was_reboot=True, disable_sysinfo=True)

        # In the client results directory are a 'keyval' file, and
        # various raw bootstat data files.  First promote the client
        # test 'keyval' as our own.
        logging.info('BootPerfServer: gather client results')
        client_results_dir = os.path.join(
            self.outputdir, self.client_test, "results")
        src = os.path.join(client_results_dir, "keyval")
        dst = os.path.join(self.resultsdir, "keyval")
        if os.path.exists(src):
            client_results = open(src, "r")
            server_results = open(dst, "a")
            shutil.copyfileobj(client_results, server_results)
            server_results.close()
            client_results.close()
        else:
            logging.warning('Unable to locate %s', src)

        # Upload perf keyvals in the client keyval file to perf dashboard.
        if upload_perf:
            logging.info('Output perf data for iteration %03d', self.iteration)
            perf_keyvals = utils.read_keyval(src, type_tag='perf')
            self.upload_perf_keyvals(perf_keyvals)

        # Everything that isn't the client 'keyval' file is raw data
        # from the client test:  move it to a per-iteration
        # subdirectory.  We move instead of copying so we can be sure
        # we don't have any stale results in the next iteration
        if self.iteration is not None:
            rawdata_dir = "rawdata.%03d" % self.iteration
        else:
            rawdata_dir = "rawdata"
        rawdata_dir = os.path.join(self.resultsdir, rawdata_dir)
        shutil.move(client_results_dir, rawdata_dir)
        try:
            os.remove(os.path.join(rawdata_dir, "keyval"))
        except Exception:
            pass
