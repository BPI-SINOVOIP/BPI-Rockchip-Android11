# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import kernel_config
from autotest_lib.client.cros.graphics.graphics_utils import GraphicsTest

class graphics_KernelConfig(GraphicsTest):
    """Examine a kernel build CONFIG list to verify related flags.
    """
    version = 1
    arch = None
    userspace_arch = None

    IS_BUILTIN = [
        # Sanity checks; should be present in builds as builtins.
    ]
    IS_MODULE = [
        # Sanity checks; should be present in builds as modules.
    ]
    IS_ENABLED = [
        # Sanity checks; should be enabled.
    ]
    IS_MISSING = [
        # Sanity checks; should be disabled.
        'DRM_KMS_FB_HELPER'
        'FB',
        'FB_CFB_COPYAREA',
        'FB_CFB_FILLRECT',
        'FB_CFB_IMAGEBLIT',
        'FB_CFB_REV_PIXELS_IN_BYTE',
        'FB_SIMPLE',
        'FB_SYS_COPYAREA',
        'FB_SYS_FOPS',
        'FB_SYS_FILLRECT',
        'FB_SYS_IMAGEBLIT',
        'FB_VIRTUAL'
    ]

    def setup(self):
        """ Test setup. """
        self.arch = utils.get_arch()
        self.userspace_arch = utils.get_arch_userspace()
        # Report the full uname for anyone reading logs.
        logging.info('Running %s kernel, %s userspace: %s',
                     self.arch, self.userspace_arch,
                     utils.system_output('uname -a'))

    @GraphicsTest.failure_report_decorator('graphics_KernelConfig')
    def run_once(self):
        """
        The actual test that read config and check.
        """
        # Load the list of kernel config variables.
        config = kernel_config.KernelConfig()
        config.initialize()
        logging.debug(config._config)

        # Run the static checks.
        map(config.has_builtin, self.IS_BUILTIN)
        map(config.has_module, self.IS_MODULE)
        map(config.is_enabled, self.IS_ENABLED)
        map(config.is_missing, self.IS_MISSING)

        # Raise a failure if anything unexpected was seen.
        if len(config.failures()):
            raise error.TestFail((", ".join(config.failures())))
