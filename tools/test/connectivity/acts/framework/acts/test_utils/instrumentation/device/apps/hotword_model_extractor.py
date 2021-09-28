#!/usr/bin/env python3
#
#   Copyright 2020 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the 'License');
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an 'AS IS' BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import os
import tempfile
import zipfile

from acts.test_utils.instrumentation.device.apps.app_installer \
    import AppInstaller

DEFAULT_MODEL_NAME = 'en_us.mmap'
MODEL_DIR = 'res/raw'


class HotwordModelExtractor(object):
    """
    Extracts a voice model data file from the Hotword APK and pushes it
    onto the device.
    """
    def __init__(self, dut):
        self._dut = dut

    def extract_to_dut(self, hotword_pkg, model_name=DEFAULT_MODEL_NAME):
        with tempfile.TemporaryDirectory() as tmp_dir:
            extracted_model = self._extract(hotword_pkg, model_name, tmp_dir)
            if not extracted_model:
                return
            device_dir = self._dut.adb.shell('echo $EXTERNAL_STORAGE')
            self._dut.adb.push(
                extracted_model, os.path.join(device_dir, model_name))

    def _extract(self, hotword_pkg, model_name, dest):
        """Extracts the model file from the given Hotword APK.

        Args:
            hotword_pkg: Package name of the Hotword APK
            model_name: File name of the model file.
            dest: Destination directory

        Returns: Full path to the extracted model file.
        """
        self._dut.log.info('Extracting voice model from Hotword APK.')
        hotword_apk = AppInstaller.pull_from_device(
            self._dut, hotword_pkg, dest)
        if not hotword_apk:
            self._dut.log.warning('Cannot extract Hotword voice model: '
                                  'Hotword APK not installed.')
            return None

        model_rel_path = os.path.join(MODEL_DIR, model_name)
        with zipfile.ZipFile(hotword_apk.apk_path) as hotword_zip:
            try:
                return hotword_zip.extract(model_rel_path, dest)
            except KeyError:
                self._dut.log.warning(
                    'Cannot extract Hotword voice model: Model file %s not '
                    'found.' % model_rel_path)
                return None
