# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.chameleon import chameleon_audio_helper
from autotest_lib.server import site_utils
from autotest_lib.server import test
from autotest_lib.server.cros.multimedia import remote_facade_factory
from autotest_lib.site_utils import lxc


class AudioTest(test.test):
    """Base class for audio tests.

    AudioTest provides basic initialization, setup and warmup check for the
    collection of server side audio tests. It is assumed to be run with a
    Chameleon audio boards. It is recommended to use this base class for server
    side Chameleon audio tests to take advantage of the initialize, setup and
    sanity check.
    """

    def initialize(self, host):
        """Initialize audio test needed components and do some sanity checks"""
        if host.chameleon is None:
            raise error.TestError("host.chameleon is None."
                                  "Please check the chameleon of this DUT.")

        self.host = host
        self.factory = remote_facade_factory.RemoteFacadeFactory(
            host, results_dir=self.resultsdir)
        self.widget_factory = chameleon_audio_helper.AudioWidgetFactory(
            self.factory, self.host)
        self.facade = self.factory.create_audio_facade()
        host.chameleon.setup_and_reset(self.resultsdir)

    def setup(self):
        """Setup needed audio test requirement before executing main logic."""
        super(AudioTest, self).setup()
        audio_test_requirement()

    def warmup(self):
        """Warmup for the test before executing main logic of the test."""
        # test.test is an old-style class.
        test.test.warmup(self)


def audio_test_requirement():
    """Installs sox and checks it is installed correctly."""
    install_sox()
    check_sox_installed()


def install_sox():
    """Install sox command on autotest drone."""
    try:
        lxc.install_package('sox')
    except error.ContainerError:
        logging.info('Can not install sox outside of container.')


def check_sox_installed():
    """Checks if sox is installed.

    @raises: error.TestError if sox is not installed.

    """
    try:
        utils.run('sox --help')
        logging.info('Found sox executable.')
    except error.CmdError:
        error_message = 'sox command is not installed.'
        if site_utils.is_inside_chroot():
            error_message += ' sudo emerge sox to install sox in chroot'
        raise error.TestError(error_message)
