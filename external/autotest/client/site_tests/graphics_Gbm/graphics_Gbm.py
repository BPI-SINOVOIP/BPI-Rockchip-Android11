# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os, re
import logging
from autotest_lib.client.common_lib import error, utils
from autotest_lib.client.cros.graphics import graphics_utils


class graphics_Gbm(graphics_utils.GraphicsTest):
    """Test the gbm implementation.
    """
    version = 1
    preserve_srcdir = True

    def setup(self):
        """Setup the test code."""
        os.chdir(self.srcdir)
        utils.make('clean')
        utils.make('all')

    def initialize(self):
        super(graphics_Gbm, self).initialize()

    def cleanup(self):
        super(graphics_Gbm, self).cleanup()

    @graphics_utils.GraphicsTest.failure_report_decorator('graphics_Gbm')
    def run_once(self):
        """Main body to exercise the test code."""
        board = utils.get_current_board()
        if board in ['monroe', 'fizz-moblab', 'guado_moblab']:
            # Monroe has a hardware problem requiring hard cycling. Moblab
            # images behave odd compared to non-moblab images.
            logging.info('Skipping test.')
            return
        cmd = os.path.join(self.srcdir, 'gbmtest')
        result = utils.run(
            cmd,
            stderr_is_expected=False,
            stdout_tee=utils.TEE_TO_LOGS,
            stderr_tee=utils.TEE_TO_LOGS,
            ignore_status=True)
        report = re.findall(r'\[  PASSED  \]', result.stdout)
        if not report:
            raise error.TestFail('Failed: Gbm test failed (' + result.stdout +
                                 ')')
