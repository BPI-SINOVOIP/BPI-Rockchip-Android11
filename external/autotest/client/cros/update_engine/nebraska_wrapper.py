# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import requests
import subprocess

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error


# JSON attributes used in payload properties. Look at nebraska.py for more
# information.
KEY_PUBLIC_KEY='public_key'
KEY_METADATA_SIZE='metadata_size'
KEY_SHA256='sha256_hex'


class NebraskaWrapper(object):
    """
    A wrapper around nebraska.py

    This wrapper is used to start a nebraska.py service and allow the
    update_engine to interact with it.

    """

    def __init__(self, log_dir=None, update_metadata_dir=None,
                 update_payloads_address=None):
        """
        Initializes the NebraskaWrapper module.

        @param log_dir: The directory to write nebraska.log into.
        @param update_metadata_dir: The directory containing payload properties
                files. Look at nebraska.py
        @param update_payloads_address: The base URL for the update payload.

        """
        self._nebraska_server = None
        self._port = None
        self._log_dir = log_dir
        self._update_metadata_dir = update_metadata_dir
        self._update_payloads_address = update_payloads_address

    def __enter__(self):
        """So that NebraskaWrapper can be used as a Context Manager."""
        self.start()
        return self

    def __exit__(self, *exception_details):
        """
        So that NebraskaWrapper can be used as a Context Manager.

        @param exception_details: Details of exceptions happened in the
                ContextManager.

        """
        self.stop()

    def start(self):
        """
        Starts the Nebraska server.

        @raise error.TestError: If fails to start the Nebraska server.

        """
        # Any previously-existing files (port, pid and log files) will be
        # overriden by Nebraska during bring up.
        runtime_root = '/tmp/nebraska'
        cmd = ['nebraska.py', '--runtime-root', runtime_root]
        if self._log_dir:
            cmd += ['--log-file', os.path.join(self._log_dir, 'nebraska.log')]
        if self._update_metadata_dir:
            cmd += ['--update-metadata', self._update_metadata_dir]
        if self._update_payloads_address:
            cmd += ['--update-payloads-address', self._update_payloads_address]

        logging.info('Starting nebraska.py with command: %s', cmd)

        try:
            self._nebraska_server = subprocess.Popen(cmd,
                                                     stdout=subprocess.PIPE,
                                                     stderr=subprocess.STDOUT)

            # Wait for port file to appear.
            port_file = os.path.join(runtime_root, 'port')
            utils.poll_for_condition(lambda: os.path.exists(port_file),
                                     timeout=5)

            with open(port_file, 'r') as f:
                self._port = int(f.read())

            # Send a health_check request to it to make sure its working.
            requests.get('http://127.0.0.1:%d/health_check' % self._port)

        except Exception as e:
            raise error.TestError('Failed to start Nebraska %s' % e)

    def stop(self):
        """Stops the Nebraska server."""
        if not self._nebraska_server:
            return
        try:
            self._nebraska_server.terminate()
            stdout, _ = self._nebraska_server.communicate()
            logging.info('Stopping nebraska.py with stdout %s', stdout)
            self._nebraska_server.wait()
        except subprocess.TimeoutExpired:
            logging.error('Failed to stop Nebraska. Ignoring...')
        finally:
            self._nebraska_server = None

    def get_port(self):
        """Returns the port which Nebraska is running."""
        return self._port

    def get_update_url(self, **kwargs):
        """
        Returns a URL for getting updates from this Nebraska instance.

        @param kwargs: A set of key/values to form a search query to instruct
                Nebraska to do a set of activities. See
                nebraska.py::ResponseProperties for examples key/values.
        """
        query = '&'.join('%s=%s' % (k, v) for k, v in kwargs.items())
        return 'http://127.0.0.1:%d/update?%s' % (self._port, query)
