# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64, dbus, json, logging, os
from subprocess import Popen, PIPE
from threading import Thread

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import debugd_util

class PipeReader():
    """
    The class to read from a pipe. Intended for running off the main thread.
    """
    def __init__(self, pipe_r):
        self.pipe_r = pipe_r

    def read(self):
        """
        Drain from self.pipe_r and store the result in self.result. This method
        runs in a new thread.
        """
        # Read feedback logs content (JSON) from pipe_r.
        self.result = os.fdopen(self.pipe_r, 'r').read()

class platform_DebugDaemonPerfDataInFeedbackLogs(test.test):
    """
    This autotest tests perf profile in feedback logs. It calls the debugd
    method GetBigFeedbackLogs and checks whether 'perf-data' is present in the
    returned logs. The perf data is base64-encoded lzma-compressed quipper
    output.
    """

    version = 1

    def xz_decompress_string(self, compressed_input):
        """
        xz-decompresses a string.

        @param compressed_input: The input string to be decompressed.

        Returns:
          The decompressed string.
        """
        process = Popen('/usr/bin/xz -d', stdout=PIPE, stderr=PIPE, stdin=PIPE,
                        shell=True)
        out, err = process.communicate(input=compressed_input)

        if len(err) > 0:
            raise error.TestFail('decompress() failed with %s' % err)

        logging.info('decompress() %d -> %d bytes', len(compressed_input),
                     len(out))
        return out

    def validate_perf_data_in_feedback_logs(self):
        """
        Validate that feedback logs contain valid perf data.
        """
        pipe_r, pipe_w = os.pipe()

        # GetBigFeedbackReport transfers large content through the pipe. We
        # need to read from the pipe off-thread to prevent a deadlock.
        pipe_reader = PipeReader(pipe_r)
        thread = Thread(target = pipe_reader.read)
        thread.start()

        # Use 180-sec timeout because GetBigFeedbackLogs runs arc-bugreport,
        # which takes a while to finish.
        debugd_util.iface().GetBigFeedbackLogs(dbus.types.UnixFd(pipe_w),
                                               signature='h', timeout=180)

        # pipe_w is dup()'d in calling dbus. Close in this process.
        os.close(pipe_w)
        thread.join()

        # Decode into a dictionary.
        logs = json.loads(pipe_reader.result)

        if len(logs) == 0:
            raise error.TestFail('GetBigFeedbackLogs() returned no data')
        logging.info('GetBigFeedbackLogs() returned %d elements.', len(logs))

        perf_data = logs['perf-data']

        if perf_data is None:
            raise error.TestFail('perf-data not found in feedback logs')

        BLOB_START_TOKEN = '<base64>: '
        try:
            blob_start = perf_data.index(BLOB_START_TOKEN)
        except:
            raise error.TestFail(("perf-data doesn't include base64 encoded"
                                  "data"))

        # Skip description text and BLOB_START_TOKEN
        perf_data = perf_data[blob_start + len(BLOB_START_TOKEN):]

        logging.info('base64 perf data: %d bytes', len(perf_data))

        # This raises TypeError if input is invalid base64-encoded data.
        compressed_data = base64.b64decode(perf_data)

        protobuff = self.xz_decompress_string(compressed_data)
        if len(protobuff) < 10:
            raise error.TestFail('Perf output too small (%d bytes)' %
                                 len(protobuff))

        if protobuff.startswith('<process exited with status: '):
            raise error.TestFail('Failed to capture a profile: %s' %
                                 protobuff)

    def run_once(self, *args, **kwargs):
        """
        Primary autotest function.
        """
        self.validate_perf_data_in_feedback_logs()

