#!/usr/bin/env python
#
# Copyright 2020 - The Android Open Source Project
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
"""Tests for cvd_runtime_config class."""

import os
import mock
import six

from acloud.internal.lib import cvd_runtime_config as cf_cfg
from acloud.internal.lib import driver_test_lib


class CvdRuntimeconfigTest(driver_test_lib.BaseDriverTest):
    """Test CvdRuntimeConfig."""

    CF_RUNTIME_CONFIG = """
{"x_display" : ":20",
 "x_res" : 720,
 "y_res" : 1280,
 "instances": {
   "1":{
       "adb_ip_and_port": "127.0.0.1:6520",
       "host_port": 6520,
       "instance_dir": "/path-to-instance-dir",
       "vnc_server_port": 6444
   }
 }
}
"""

    # pylint: disable=protected-access
    def testGetCuttlefishRuntimeConfig(self):
        """Test GetCuttlefishRuntimeConfig."""
        # Should raise error when file does not exist.
        self.Patch(os.path, "exists", return_value=False)
        # Verify return data.
        self.Patch(os.path, "exists", return_value=True)
        expected_dict = {u'y_res': 1280,
                         u'x_res': 720,
                         u'x_display': u':20',
                         u'instances':
                             {u'1':
                                  {u'adb_ip_and_port': u'127.0.0.1:6520',
                                   u'host_port': 6520,
                                   u'instance_dir': u'/path-to-instance-dir',
                                   u'vnc_server_port': 6444}
                             },
                        }
        mock_open = mock.mock_open(read_data=self.CF_RUNTIME_CONFIG)
        cf_cfg_path = "/fake-path/local-instance-1/fake.config"
        with mock.patch.object(six.moves.builtins, "open", mock_open):
            self.assertEqual(expected_dict,
                             cf_cfg.CvdRuntimeConfig(cf_cfg_path)._config_dict)
