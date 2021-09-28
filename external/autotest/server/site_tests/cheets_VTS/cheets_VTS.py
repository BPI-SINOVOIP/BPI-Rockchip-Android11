# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# repohooks/pre-upload.py currently does not run pylint. But for developers who
# want to check their code manually we disable several harmless pylint warnings
# which just distract from more serious remaining issues.
#
# The instance variable _android_vts is not defined in __init__().
# pylint: disable=attribute-defined-outside-init
#
# Many short variable names don't follow the naming convention.
# pylint: disable=invalid-name

import logging
import os

from autotest_lib.server import utils
from autotest_lib.server.cros.tradefed import tradefed_test

# Maximum default time allowed for each individual CTS module.
_CTS_TIMEOUT_SECONDS = 3600

# Internal download locations for android vtas bundles.
_INTERNAL_VTS = 'gs://chromeos-arc-images/vts'
_VTS_URI = {
    'arm': _INTERNAL_VTS + 'VTS-9_r11_arm.zip',
    'x86': _INTERNAL_VTS + 'VTS-9_r11_x86.zip',
}


class cheets_VTS(tradefed_test.TradefedTest):
    """Sets up tradefed to run VTS tests."""
    version = 1

    _SHARD_CMD = '--shard-count'

    def _tradefed_retry_command(self, template, session_id):
        """Build tradefed 'retry' command from template."""
        cmd = []
        for arg in template:
            cmd.append(arg.format(session_id=session_id))
        return cmd

    def _tradefed_run_command(self, template):
        """Build tradefed 'run' command from template."""
        cmd = template[:]
        # If we are running outside of the lab we can collect more data.
        if not utils.is_in_container():
            logging.info('Running outside of lab, adding extra debug options.')
            cmd.append('--log-level-display=DEBUG')
        return cmd

    def _get_default_bundle_url(self, bundle):
        return _VTS_URI[bundle]

    def _get_tradefed_base_dir(self):
        return 'android-vts'

    def _tradefed_cmd_path(self):
        return os.path.join(self._repository, 'tools', 'vts-tradefed')

    def _should_skip_test(self, bundle):
        """Some tests are expected to fail and are skipped."""
        # novato* are x86 VMs without binary translation. Skip the ARM tests.
        no_ARM_ABI_test_boards = ('novato', 'novato-arc64', 'novato-arcnext')
        if self._get_board_name() in no_ARM_ABI_test_boards and bundle == 'arm':
            return True
        return False

    def run_once(self,
                 test_name,
                 run_template,
                 retry_template=None,
                 target_module=None,
                 target_plan=None,
                 target_class=None,
                 target_method=None,
                 needs_push_media=False,
                 bundle=None,
                 precondition_commands=[],
                 login_precondition_commands=[],
                 timeout=_CTS_TIMEOUT_SECONDS):
        """Runs the specified CTS once, but with several retries.

        Run an arbitrary tradefed command.

        @param test_name: the name of test. Used for logging.
        @param run_template: the template to construct the run command.
                             Example: ['run', 'commandAndExit', 'cts',
                                       '--skip-media-download']
        @param retry_template: the template to construct the retry command.
                               Example: ['run', 'commandAndExit', 'retry',
                                         '--skip-media-download', '--retry',
                                         '{session_id}']
        @param target_module: the name of test module to run.
        @param target_plan: the name of the test plan to run.
        @param target_class: the name of the class to be tested.
        @param target_method: the name of the method to be tested.
        @param needs_push_media: need to push test media streams.
        @param bundle: the type of the CTS bundle: 'arm' or 'x86'
        @param precondition_commands: a list of scripts to be run on the
        dut before the test is run, the scripts must already be installed.
        @param login_precondition_commands: a list of scripts to be run on the
        dut before the log-in for the test is performed.
        @param timeout: time after which tradefed can be interrupted.
        """
        self._run_tradefed_with_retries(
            test_name=test_name,
            run_template=run_template,
            retry_template=retry_template,
            timeout=timeout,
            target_module=target_module,
            target_plan=target_plan,
            bundle=bundle,
            cts_uri=_VTS_URI,
            login_precondition_commands=login_precondition_commands,
            precondition_commands=precondition_commands)
