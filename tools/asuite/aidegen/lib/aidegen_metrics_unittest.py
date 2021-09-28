#!/usr/bin/env python3
#
# Copyright 2019, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Unittests for aidegen_metrics."""

import unittest
from unittest import mock

from aidegen import constant
from aidegen.lib import aidegen_metrics
from atest import atest_utils


try:
    from asuite.metrics import metrics
    from asuite.metrics import metrics_utils
except ImportError:
    metrics = None
    metrics_utils = None


class AidegenMetricsUnittests(unittest.TestCase):
    """Unit tests for aidegen_metrics.py."""

    @mock.patch.object(atest_utils, 'print_data_collection_notice')
    def test_starts_asuite_metrics(self, mock_print_data):
        """Test starts_asuite_metrics."""
        references = ['nothing']
        if not metrics:
            aidegen_metrics.starts_asuite_metrics(references)
            self.assertFalse(mock_print_data.called)
        else:
            with mock.patch.object(metrics_utils, 'get_start_time') as mk_get:
                with mock.patch.object(metrics, 'AtestStartEvent') as mk_start:
                    aidegen_metrics.starts_asuite_metrics(references)
                    self.assertTrue(mock_print_data.called)
                    self.assertTrue(mk_get.called)
                    self.assertTrue(mk_start.called)

    def test_ends_asuite_metrics(self):
        """Test ends_asuite_metrics."""
        exit_code = constant.EXIT_CODE_NORMAL
        if metrics_utils:
            with mock.patch.object(metrics_utils, 'send_exit_event') as mk_send:
                aidegen_metrics.ends_asuite_metrics(exit_code)
                self.assertTrue(mk_send.called)

    @mock.patch.object(aidegen_metrics, 'ends_asuite_metrics')
    def test_send_exception_metrics(self, mock_ends_metrics):
        """Test send_exception_metrics."""
        mock_ends_metrics.return_value = True
        aidegen_metrics.send_exception_metrics(1, '', '', 'err_test')
        self.assertTrue(mock_ends_metrics.called)

if __name__ == '__main__':
    unittest.main()
