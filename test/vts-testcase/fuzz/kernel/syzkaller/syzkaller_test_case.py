#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import logging
import os
import time

import environment_variables as env


class SyzkallerTestCase(object):
    """Represents syzkaller test case.

    Attributes:
        _env: dict, inverted map of environment varialbes for this test.
        _device_target: string, OS of the target device.
        _device_code: string, serial number of the target device.
        _device_type: string, type of the target device.
        _reproduce: boolean, whether or not to reproduce if a crash is found.
        _http: integer, path to localhost where the test information will be shown.
        _procs: integer, number of processes this test case will use if available.
        _test_name: string, the unique name for this test case.
        _work_dir_path: string, path to the work directory for this test case.
        _config_file_path: string, path to the config file for this test case.
    """

    def __init__(self, env, device_target, device_code, device_type, reproduce,
                 procs):
        self._env = env
        self._device_target = device_target
        self._device_code = device_code
        self._device_type = device_type
        self._reproduce = reproduce
        self._http = 'localhost:50000'
        self._procs = procs
        self._test_name = self.GenerateTestName()
        self._work_dir_path = self.GenerateCorpusDir()
        self._config_file_path = self.GenerateConfigFile()

    def GenerateTestName(self):
        """Uses device code and time to create unique name

        Returns:
            test_name, string, the unique test name for this test case.
        """
        test_name = '%s_%s_%d' % (time.strftime('%Y-%m-%d'), self._device_code,
                                  int(time.time()))
        return str(test_name)

    def GenerateCorpusDir(self):
        """Creates working directory for this test case.

        Returns:
            work_dir_path, string, path to the working directory for this test case.
        """
        work_dir_path = os.path.join(self._env['syzkaller_work_dir'],
                                     self._test_name)
        if not os.path.exists(work_dir_path):
            os.makedirs(work_dir_path)
        return work_dir_path

    def GenerateConfigFile(self):
        """Creates configuration file for this test case.

        Returns:
            config_file_path, string, path to the configuration file for this test case.
        """

        # read template config
        with open(self._env['template_cfg'], 'r') as temp:
            template_cfg = str(temp.read())

        # fill out template
        template_cfg = template_cfg.replace('{device_target}',
                                            self._device_target)
        template_cfg = template_cfg.replace('{reproduce}', self._reproduce)
        template_cfg = template_cfg.replace('{work_dir_path}',
                                            self._work_dir_path)
        template_cfg = template_cfg.replace('{http}', self._http)
        template_cfg = template_cfg.replace('{syzkaller_dir_path}',
                                            self._env['syzkaller_dir'])
        template_cfg = template_cfg.replace('{device_code}', self._device_code)
        template_cfg = template_cfg.replace('{device_type}', self._device_type)
        template_cfg = template_cfg.replace('{procs}', str(self._procs))

        # save config file
        config_file_path = self._work_dir_path + str('.cfg')
        with open(config_file_path, 'w') as config_file:
            config_file.write(template_cfg)
        return config_file_path

    def GetRunCommand(self):
        """Creates test run command for this case.

        Returns:
            test_command, string, test run command for this test case.
        """
        syz_manager_path = os.path.join(self._env['syzkaller_bin_dir'],
                                        'syz-manager')
        test_command = '%s -config=%s' % (syz_manager_path,
                                          self._config_file_path)
        return test_command
