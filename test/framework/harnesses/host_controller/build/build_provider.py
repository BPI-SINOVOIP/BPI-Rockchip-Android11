#
# Copyright (C) 2017 The Android Open Source Project
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
import re
import shutil
import tempfile
import zipfile

from host_controller import common
from vts.runners.host import utils


class BuildProvider(object):
    """The base class for build provider.

    Attributes:
        _IMAGE_FILE_EXTENSIONS: a list of strings which are common image file
                                extensions.
        _BASIC_IMAGE_FILE_NAMES: a list of strings which are the image names in
                                 an artifact zip.
        _CONFIG_FILE_EXTENSION: string, the config file extension.
        _additional_files: a dict containing additionally fetched files that
                           custom features may need. The key is the path
                           relative to temporary directory and the value is the
                           full path.
        _configs: dict where the key is config type and value is the config file
                  path.
        _device_images: dict where the key is image file name and value is the
                        path.
        _test_suites: dict where the key is test suite type and value is the
                      test suite package file path.
        _host_controller_package: dict where the key is a host controller
                                  package and the value is the path
                                  to a package file.
        _tmp_dirpath: string, the temp dir path created to keep artifacts.
        _last_fetched_artifact_type: string, stores the type of the last
                                     artifact fetched.
    """
    _CONFIG_FILE_EXTENSION = ".zip"
    _IMAGE_FILE_EXTENSIONS = [".img", ".bin"]
    _BASIC_IMAGE_FILE_NAMES = ["boot.img", "system.img", "vendor.img"]

    def __init__(self):
        self._additional_files = {}
        self._device_images = {}
        self._test_suites = {}
        self._host_controller_package = {}
        self._configs = {}
        self._last_fetched_artifact_type = None
        tempdir_base = os.path.join(os.getcwd(), "tmp")
        if not os.path.exists(tempdir_base):
            os.mkdir(tempdir_base)
        self._tmp_dirpath = tempfile.mkdtemp(dir=tempdir_base)

    def __del__(self):
        """Deletes the temp dir if still set."""
        if self._tmp_dirpath:
            shutil.rmtree(self._tmp_dirpath)
            self._tmp_dirpath = None

    @property
    def tmp_dirpath(self):
        return self._tmp_dirpath

    def CreateNewTmpDir(self):
        return tempfile.mkdtemp(dir=self._tmp_dirpath)

    def SetDeviceImage(self, name, path):
        """Sets device image `path` for the specified `name`."""
        self._device_images[name] = path
        self._last_fetched_artifact_type = common._ARTIFACT_TYPE_DEVICE

    def _IsFullDeviceImage(self, namelist):
        """Returns true if given namelist list has all common device images."""
        for image_file in self._BASIC_IMAGE_FILE_NAMES:
            if image_file not in namelist:
                return False
        return True

    def _IsImageFile(self, file_path):
        """Returns whether a file is an image.

        Args:
            file_path: string, the file path.

        Returns:
            boolean, whether the file is an image.
        """
        return any(file_path.endswith(ext)
                   for ext in self._IMAGE_FILE_EXTENSIONS)

    def SetDeviceImageZip(self, path, full_device_images=False):
        """Sets device image(s) using files in a given zip file.

        It extracts image files inside the given zip file and selects
        known Android image files.

        Args:
            path: string, the path to a zip file.
        """
        dest_path = path + ".dir"
        fetch_type = None
        with zipfile.ZipFile(path, 'r') as zip_ref:
            if full_device_images or self._IsFullDeviceImage(zip_ref.namelist()):
                self.SetDeviceImage(common.FULL_ZIPFILE, path)
                dir_key = common.FULL_ZIPFILE_DIR
                fetch_type = common._ARTIFACT_TYPE_DEVICE
            else:
                self.SetDeviceImage("gsi-zipfile", path)
                dir_key = common.GSI_ZIPFILE_DIR  # "gsi-zipfile-dir"
                fetch_type = common._ARTIFACT_TYPE_GSI
            if os.path.exists(dest_path):
                shutil.rmtree(dest_path)
                logging.info("%s %s deleted", dir_key, dest_path)
            zip_ref.extractall(dest_path)
            self.SetFetchedDirectory(dest_path)
            self.SetDeviceImage(dir_key, dest_path)

        self._last_fetched_artifact_type = fetch_type

    def GetDeviceImage(self, name=None):
        """Returns device image info."""
        if name is None:
            return self._device_images
        return self._device_images[name]

    def RemoveDeviceImage(self, name):
        """Removes certain device image info.

        Args:
            name: string, the name of the device image file
                  that needs to be removed.
        """
        if name in self._device_images:
            self._device_images.pop(name)

    def SetTestSuitePackage(self, test_suite, path):
        """Sets test suite package `path` for the specified `type`.

        Args:
            test_suite: string, test suite type such as 'vts' or 'cts', etc.
            path: string, the path of a file. if a file is a zip file,
                  it's unziped and its main binary is set.
        """
        if re.match("[vcgs]ts", test_suite):
            suite_name = "android-%s" % test_suite
            tradefed_name = "%s-tradefed" % test_suite
            dest_path = os.path.join(self.tmp_dirpath, suite_name)
            if os.path.exists(dest_path):
                shutil.rmtree(dest_path)
                logging.info("test suite %s deleted", dest_path)
            with zipfile.ZipFile(path, 'r') as zip_ref:
                zip_ref.extractall(dest_path)
                bin_path = os.path.join(dest_path, suite_name,
                                        "tools", tradefed_name)
                os.chmod(bin_path, 0766)
                path = bin_path
        else:
            logging.info("unsupported zip file %s", path)
        self._test_suites[test_suite] = path
        self._last_fetched_artifact_type = common._ARTIFACT_TYPE_TEST_SUITE

    def GetTestSuitePackage(self, type=None):
        """Returns test suite package info."""
        if type is None:
            return self._test_suites
        return self._test_suites[type]

    def SetHostControllerPackage(self, package_type, path):
        """Sets host controller package `path` for the specified `type`.

        Args:
            package_type: string, host controller type such as 'vtslab'.
            path: string, the path of a package file.
        """
        self._host_controller_package[package_type] = path
        self._last_fetched_artifact_type = common._ARTIFACT_TYPE_INFRA

    def GetHostControllerPackage(self, package_type=None):
        """Returns host controller package info.

        Args:
            package_type: string, key value to self._host_controller_package
                          dict.

        Returns:
            the whole dict if package_type is None, otherwise a string which is
            the path to the fetched host controller package.
        """
        if package_type is None:
            return self._host_controller_package
        return self._host_controller_package[package_type]

    def SetConfigPackage(self, config_type, path):
        """Sets test suite package `path` for the specified `type`.

        All valid config files have .zip extension.

        Args:
            config_type: string, config type such as 'prod' or 'test'.
            path: string, the path of a config file.
        """
        if path.endswith(self._CONFIG_FILE_EXTENSION):
            dest_path = os.path.join(
                self.tmp_dirpath, os.path.basename(path) + ".dir")
            with zipfile.ZipFile(path, 'r') as zip_ref:
                zip_ref.extractall(dest_path)
                path = dest_path
        else:
            logging.info("unsupported config package file %s", path)
        self._configs[config_type] = path
        self._last_fetched_artifact_type = common._ARTIFACT_TYPE_INFRA

    def GetConfigPackage(self, config_type=None):
        """Returns config package info."""
        if config_type is None:
            return self._configs
        return self._configs[config_type]

    def SetAdditionalFile(self, rel_path, full_path):
        """Sets the key and value of additionally fetched files.

        Args:
            rel_path: the file path relative to temporary directory.
            abs_path: the file path that this process can access.
        """
        self._additional_files[rel_path] = full_path
        self._last_fetched_artifact_type = common._ARTIFACT_TYPE_INFRA

    def GetAdditionalFile(self, rel_path=None):
        """Returns the paths to fetched files."""
        if rel_path is None:
            return self._additional_files
        return self._additional_files[rel_path]

    def SetFetchedDirectory(self,
                            dir_path,
                            root_path=None,
                            full_device_images=False):
        """Adds every file in a directory to one of the dictionaries.

        This method follows symlink to file, but skips symlink to directory.

        Args:
            dir_path: string, the directory to find files in.
            root_path: string, the temporary directory that dir_path is in.
                       The default value is dir_path.
        """
        for dir_name, file_name in utils.iterate_files(dir_path):
            full_path = os.path.join(dir_name, file_name)
            self.SetFetchedFile(full_path, (root_path
                                            if root_path else dir_path),
                                full_device_images)

    def SetFetchedFile(self,
                       file_path,
                       root_dir=None,
                       full_device_images=False,
                       set_suite_as=None):
        """Adds a file to one of the dictionaries.

        Args:
            file_path: string, the path to the file.
            root_dir: string, the temporary directory that file_path is in.
                      The default value is file_path if file_path is a
                      directory. Otherwise, the default value is file_path's
                      parent directory.
            set_suite_as: string, the test suite name to use for the given
                          artifact. Used when the file name does not follow
                          the standard "android-*ts.zip" file name pattern.
        """
        file_name = os.path.basename(file_path)
        if os.path.isdir(file_path):
            self.SetFetchedDirectory(file_path, root_dir, full_device_images)
        elif self._IsImageFile(file_path):
            self.SetDeviceImage(file_name, file_path)
        elif re.match("android-[vcgs]ts.zip", file_name):
            test_suite = (file_name.split("-")[-1]).split(".")[0]
            self.SetTestSuitePackage(test_suite, file_path)
        elif file_name == "android-vtslab.zip":
            self.SetHostControllerPackage("vtslab", file_path)
        elif file_name.startswith("vti-global-config"):
            self.SetConfigPackage(
                "prod" if "prod" in file_name else "test", file_path)
        elif set_suite_as:
            self.SetTestSuitePackage(set_suite_as, file_path)
        elif file_path.endswith(".zip"):
            self.SetDeviceImageZip(file_path, full_device_images)
        else:
            rel_path = (os.path.relpath(file_path, root_dir) if root_dir else
                        os.path.basename(file_path))
            self.SetAdditionalFile(rel_path, file_path)

    def PrintDeviceImageInfo(self):
        """Prints device image info."""
        logging.info(self.GetDeviceImage())

    def PrintGetTestSuitePackageInfo(self):
        """Prints test suite package info."""
        logging.info(self.GetTestSuitePackage())

    def GetFetchedArtifactType(self):
        """Gets the most recently fetched artifact type.

        Returns:
            string, type of the artifact.
        """
        return self._last_fetched_artifact_type
