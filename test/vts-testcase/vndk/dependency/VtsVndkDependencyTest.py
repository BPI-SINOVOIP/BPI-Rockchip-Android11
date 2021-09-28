#!/usr/bin/env python
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

# TODO(b/147454897): Keep the test logic in sync with vts_vndk_dependency.py
#                    until this file is removed.
import collections
import logging
import os
import re
import shutil
import tempfile

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import keys
from vts.runners.host import test_runner
from vts.runners.host import utils
from vts.testcases.vndk.golden import vndk_data
from vts.utils.python.controllers import android_device
from vts.utils.python.file import target_file_utils
from vts.utils.python.library import elf_parser
from vts.utils.python.os import path_utils
from vts.utils.python.vndk import vndk_utils


class VtsVndkDependencyTest(base_test.BaseTestClass):
    """A test case to verify vendor library dependency.

    Attributes:
        data_file_path: The path to VTS data directory.
        _dut: The AndroidDevice under test.
        _temp_dir: The temporary directory to which the odm and vendor
                   partitions are copied.
        _ll_ndk: Set of strings. The names of low-level NDK libraries in
                 /system/lib[64].
        _sp_hal: List of patterns. The names of the same-process HAL libraries
                 expected to be in /vendor/lib[64].
        _vndk: Set of strings. The names of VNDK-core libraries.
        _vndk_sp: Set of strings. The names of VNDK-SP libraries.
        _SP_HAL_LINK_PATHS: Format strings of same-process HAL's link paths.
        _VENDOR_LINK_PATHS: Format strings of vendor processes' link paths.
    """
    _TARGET_DIR_SEP = "/"
    _TARGET_ROOT_DIR = "/"
    _TARGET_ODM_DIR = "/odm"
    _TARGET_VENDOR_DIR = "/vendor"

    _SP_HAL_LINK_PATHS = [
        "/odm/{LIB}/egl", "/odm/{LIB}/hw", "/odm/{LIB}",
        "/vendor/{LIB}/egl", "/vendor/{LIB}/hw", "/vendor/{LIB}"
    ]
    _VENDOR_LINK_PATHS = [
        "/odm/{LIB}/hw", "/odm/{LIB}/egl", "/odm/{LIB}",
        "/vendor/{LIB}/hw", "/vendor/{LIB}/egl", "/vendor/{LIB}"
    ]
    _DEFAULT_PROGRAM_INTERPRETERS = [
        "/system/bin/linker", "/system/bin/linker64"
    ]

    class ElfObject(object):
        """Contains dependencies of an ELF file on target device.

        Attributes:
            target_path: String. The path to the ELF file on target.
            name: String. File name of the ELF.
            target_dir: String. The directory containing the ELF file on target.
            bitness: Integer. Bitness of the ELF.
            deps: List of strings. The names of the depended libraries.
            runpaths: List of strings. The library search paths.
        """

        def __init__(self, target_path, bitness, deps, runpaths):
            self.target_path = target_path
            self.name = path_utils.TargetBaseName(target_path)
            self.target_dir = path_utils.TargetDirName(target_path)
            self.bitness = bitness
            self.deps = deps
            # Format runpaths
            self.runpaths = []
            lib_dir_name = "lib64" if bitness == 64 else "lib"
            for runpath in runpaths:
                path = runpath.replace("${LIB}", lib_dir_name)
                path = path.replace("$LIB", lib_dir_name)
                path = path.replace("${ORIGIN}", self.target_dir)
                path = path.replace("$ORIGIN", self.target_dir)
                self.runpaths.append(path)

    def setUpClass(self):
        """Initializes device, temporary directory, and VNDK lists."""
        required_params = [keys.ConfigKeys.IKEY_DATA_FILE_PATH]
        self.getUserParams(required_params)
        self._dut = self.android_devices[0]
        self._temp_dir = tempfile.mkdtemp()
        for target_dir in (self._TARGET_ODM_DIR, self._TARGET_VENDOR_DIR):
            if target_file_utils.IsDirectory(target_dir, self._dut.shell):
                logging.info("adb pull %s %s", target_dir, self._temp_dir)
                self._dut.adb.pull(target_dir, self._temp_dir)
            else:
                logging.info("Skip adb pull %s", target_dir)

        vndk_lists = vndk_data.LoadVndkLibraryLists(
            self.data_file_path,
            self._dut.vndk_version,
            vndk_data.SP_HAL,
            vndk_data.LL_NDK,
            vndk_data.VNDK,
            vndk_data.VNDK_SP)
        asserts.assertTrue(vndk_lists, "Cannot load VNDK library lists.")

        sp_hal_strings = vndk_lists[0]
        self._sp_hal = [re.compile(x) for x in sp_hal_strings]
        (self._ll_ndk, self._vndk, self._vndk_sp) = vndk_lists[1:]

        logging.debug("LL_NDK: %s", self._ll_ndk)
        logging.debug("SP_HAL: %s", sp_hal_strings)
        logging.debug("VNDK: %s", self._vndk)
        logging.debug("VNDK_SP: %s", self._vndk_sp)

    def tearDownClass(self):
        """Deletes the temporary directory."""
        logging.info("Delete %s", self._temp_dir)
        shutil.rmtree(self._temp_dir)

    def _IsElfObjectForAp(self, elf, target_path, abi_list):
        """Checks whether an ELF object is for application processor.

        Args:
            elf: The object of elf_parser.ElfParser.
            target_path: The path to the ELF file on target.
            abi_list: A list of strings, the ABIs of the application processor.

        Returns:
            A boolean, whether the ELF object is for application processor.
        """
        if not any(elf.MatchCpuAbi(x) for x in abi_list):
            logging.debug("%s does not match the ABI", target_path)
            return False

        # b/115567177 Skip an ELF file if it meets the following 3 conditions:
        # The ELF type is executable.
        if not elf.IsExecutable():
            return True

        # It requires special program interpreter.
        interp = elf.GetProgramInterpreter()
        if not interp or interp in self._DEFAULT_PROGRAM_INTERPRETERS:
            return True

        # It does not have execute permission in the file system.
        permissions = target_file_utils.GetPermission(target_path,
                                                      self._dut.shell)
        if target_file_utils.IsExecutable(permissions):
            return True

        return False

    def _IsElfObjectBuiltForAndroid(self, elf, target_path):
        """Checks whether an ELF object is built for Android.

        Some ELF objects in vendor partition require special program
        interpreters. Such executable files have .interp sections, but shared
        libraries don't. As there is no reliable way to identify those
        libraries. This method checks .note.android.ident section which is
        created by Android build system.

        Args:
            elf: The object of elf_parser.ElfParser.
            target_path: The path to the ELF file on target.

        Returns:
            A boolean, whether the ELF object is built for Android.
        """
        # b/133399940 Skip an ELF file if it does not have .note.android.ident
        # section and meets one of the following conditions:
        if elf.HasAndroidIdent():
            return True

        # It's in the specific directory and is a shared library.
        if (target_path.startswith("/vendor/arib/lib/") and
                ".so" in target_path and
                elf.IsSharedObject()):
            return False

        # It's in the specific directory, requires special program interpreter,
        # and is executable.
        if target_path.startswith("/vendor/arib/bin/"):
            interp = elf.GetProgramInterpreter()
            if interp and interp not in self._DEFAULT_PROGRAM_INTERPRETERS:
                permissions = target_file_utils.GetPermission(target_path,
                                                              self._dut.shell)
                if (elf.IsExecutable() or
                        target_file_utils.IsExecutable(permissions)):
                    return False

        return True

    def _LoadElfObjects(self, host_dir, target_dir, abi_list,
                        elf_error_handler):
        """Scans a host directory recursively and loads all ELF files in it.

        Args:
            host_dir: The host directory to scan.
            target_dir: The path from which host_dir is copied.
            abi_list: A list of strings, the ABIs of the ELF files to load.
            elf_error_handler: A function that takes 2 arguments
                               (target_path, exception). It is called when
                               the parser fails to read an ELF file.

        Returns:
            List of ElfObject.
        """
        objs = []
        for root_dir, file_name in utils.iterate_files(host_dir):
            full_path = os.path.join(root_dir, file_name)
            rel_path = os.path.relpath(full_path, host_dir)
            target_path = path_utils.JoinTargetPath(
                target_dir, *rel_path.split(os.path.sep))
            try:
                elf = elf_parser.ElfParser(full_path)
            except elf_parser.ElfError:
                logging.debug("%s is not an ELF file", target_path)
                continue
            try:
                if not self._IsElfObjectForAp(elf, target_path, abi_list):
                    logging.info("%s is not for application processor",
                                 target_path)
                    continue
                if not self._IsElfObjectBuiltForAndroid(elf, target_path):
                    logging.info("%s is not built for Android", target_path)
                    continue

                deps, runpaths = elf.ListDependencies()
            except elf_parser.ElfError as e:
                elf_error_handler(target_path, e)
                continue
            finally:
                elf.Close()

            logging.info("%s depends on: %s", target_path, ", ".join(deps))
            if runpaths:
                logging.info("%s has runpaths: %s",
                             target_path, ":".join(runpaths))
            objs.append(self.ElfObject(target_path, elf.bitness, deps,
                                       runpaths))
        return objs

    def _FindLibsInLinkPaths(self, bitness, link_paths, objs):
        """Finds libraries in link paths.

        Args:
            bitness: 32 or 64, the bitness of the returned libraries.
            link_paths: List of strings, the default link paths.
            objs: List of ElfObject, the libraries/executables to be filtered
                  by bitness and path.

        Returns:
            A defaultdict, {dir: {name: obj}} where obj is an ElfObject, dir
            is obj.target_dir, and name is obj.name.
        """
        namespace = collections.defaultdict(dict)
        for obj in objs:
            if (obj.bitness == bitness and
                    any(obj.target_path.startswith(link_path +
                                                   self._TARGET_DIR_SEP)
                        for link_path in link_paths)):
                namespace[obj.target_dir][obj.name] = obj
        return namespace

    def _DfsDependencies(self, lib, searched, namespace, link_paths):
        """Depth-first-search for library dependencies.

        Args:
            lib: ElfObject, the library to search for dependencies.
            searched: The set of searched libraries.
            namespace: Defaultdict, {dir: {name: obj}} containing all
                       searchable libraries.
            link_paths: List of strings, the default link paths.
        """
        if lib in searched:
            return
        searched.add(lib)
        for dep_name in lib.deps:
            for link_path in lib.runpaths + link_paths:
                if dep_name in namespace[link_path]:
                    self._DfsDependencies(namespace[link_path][dep_name],
                                          searched, namespace, link_paths)
                    break

    def _FindDisallowedDependencies(self, objs, namespace, link_paths,
                                    *vndk_lists):
        """Tests if libraries/executables have disallowed dependencies.

        Args:
            objs: Collection of ElfObject, the libraries/executables under
                  test.
            namespace: Defaultdict, {dir: {name: obj}} containing all libraries
                       in the linker namespace.
            link_paths: List of strings, the default link paths.
            vndk_lists: Collections of library names in VNDK, VNDK-SP, etc.

        Returns:
            List of tuples (path, disallowed_dependencies).
        """
        dep_errors = []
        for obj in objs:
            disallowed_libs = []
            for dep_name in obj.deps:
                if any((dep_name in vndk_list) for vndk_list in vndk_lists):
                    continue
                if any((dep_name in namespace[link_path]) for link_path in
                       obj.runpaths + link_paths):
                    continue
                disallowed_libs.append(dep_name)

            if disallowed_libs:
                dep_errors.append((obj.target_path, disallowed_libs))
        return dep_errors

    def _TestElfDependency(self, bitness, objs):
        """Tests vendor libraries/executables and SP-HAL dependencies.

        Args:
            bitness: 32 or 64, the bitness of the vendor libraries.
            objs: List of ElfObject. The libraries/executables in odm and
                  vendor partitions.

        Returns:
            List of tuples (path, disallowed_dependencies).
        """
        vndk_sp_ext_dirs = vndk_utils.GetVndkSpExtDirectories(bitness)

        vendor_link_paths = [vndk_utils.FormatVndkPath(x, bitness) for
                             x in self._VENDOR_LINK_PATHS]
        vendor_namespace = self._FindLibsInLinkPaths(bitness,
                                                     vendor_link_paths, objs)
        # Exclude VNDK and VNDK-SP extensions from vendor libraries.
        for vndk_ext_dir in (vndk_utils.GetVndkExtDirectories(bitness) +
                             vndk_utils.GetVndkSpExtDirectories(bitness)):
            vendor_namespace.pop(vndk_ext_dir, None)
        logging.info("%d-bit odm, vendor, and SP-HAL libraries:", bitness)
        for dir_path, libs in vendor_namespace.iteritems():
            logging.info("%s: %s", dir_path, ",".join(libs.iterkeys()))

        sp_hal_link_paths = [vndk_utils.FormatVndkPath(x, bitness) for
                             x in self._SP_HAL_LINK_PATHS]
        sp_hal_namespace = self._FindLibsInLinkPaths(bitness,
                                                     sp_hal_link_paths, objs)

        # Find same-process HAL and dependencies
        sp_hal_libs = set()
        for link_path in sp_hal_link_paths:
            for obj in sp_hal_namespace[link_path].itervalues():
                if any(x.match(obj.target_path) for x in self._sp_hal):
                    self._DfsDependencies(obj, sp_hal_libs, sp_hal_namespace,
                                          sp_hal_link_paths)
        logging.info("%d-bit SP-HAL libraries: %s",
                     bitness, ", ".join(x.name for x in sp_hal_libs))

        # Find VNDK-SP extension libraries and their dependencies.
        vndk_sp_ext_libs = set(obj for obj in objs if
                               obj.bitness == bitness and
                               obj.target_dir in vndk_sp_ext_dirs)
        vndk_sp_ext_deps = set()
        for lib in vndk_sp_ext_libs:
            self._DfsDependencies(lib, vndk_sp_ext_deps, sp_hal_namespace,
                                  sp_hal_link_paths)
        logging.info("%d-bit VNDK-SP extension libraries and dependencies: %s",
                     bitness, ", ".join(x.name for x in vndk_sp_ext_deps))

        # A vendor library/executable is allowed to depend on
        # LL-NDK
        # VNDK
        # VNDK-SP
        # Other libraries in vendor link paths
        vendor_objs = {obj for obj in objs if
                       obj.bitness == bitness and
                       obj not in sp_hal_libs and
                       obj not in vndk_sp_ext_deps}
        dep_errors = self._FindDisallowedDependencies(
            vendor_objs, vendor_namespace, vendor_link_paths,
            self._ll_ndk, self._vndk, self._vndk_sp)

        # A VNDK-SP extension library/dependency is allowed to depend on
        # LL-NDK
        # VNDK-SP
        # Libraries in vendor link paths
        # Other VNDK-SP extension libraries, which is a subset of VNDK-SP
        #
        # However, it is not allowed to indirectly depend on VNDK. i.e., the
        # depended vendor libraries must not depend on VNDK.
        #
        # vndk_sp_ext_deps and sp_hal_libs may overlap. Their dependency
        # restrictions are the same.
        dep_errors.extend(self._FindDisallowedDependencies(
            vndk_sp_ext_deps - sp_hal_libs, vendor_namespace,
            vendor_link_paths, self._ll_ndk, self._vndk_sp))

        if not vndk_utils.IsVndkRuntimeEnforced(self._dut):
            logging.warning("Ignore dependency errors: %s", dep_errors)
            dep_errors = []

        # A same-process HAL library is allowed to depend on
        # LL-NDK
        # VNDK-SP
        # Other same-process HAL libraries and dependencies
        dep_errors.extend(self._FindDisallowedDependencies(
            sp_hal_libs, sp_hal_namespace, sp_hal_link_paths,
            self._ll_ndk, self._vndk_sp))
        return dep_errors

    def testElfDependency(self):
        """Tests vendor libraries/executables and SP-HAL dependencies."""
        read_errors = []
        abi_list = self._dut.getCpuAbiList()
        objs = self._LoadElfObjects(
            self._temp_dir, self._TARGET_ROOT_DIR, abi_list,
            lambda p, e: read_errors.append((p, str(e))))

        dep_errors = self._TestElfDependency(32, objs)
        if self._dut.is64Bit:
            dep_errors.extend(self._TestElfDependency(64, objs))

        assert_lines = []
        if read_errors:
            error_lines = ["%s: %s" % (x[0], x[1]) for x in read_errors]
            logging.error("%d read errors:\n%s",
                          len(read_errors), "\n".join(error_lines))
            assert_lines.extend(error_lines[:20])

        if dep_errors:
            error_lines = ["%s: %s" % (x[0], ", ".join(x[1]))
                           for x in dep_errors]
            logging.error("%d disallowed dependencies:\n%s",
                          len(dep_errors), "\n".join(error_lines))
            assert_lines.extend(error_lines[:20])

        error_count = len(read_errors) + len(dep_errors)
        if error_count:
            if error_count > len(assert_lines):
                assert_lines.append("...")
            assert_lines.append("Total number of errors: " + str(error_count))
            asserts.fail("\n".join(assert_lines))


if __name__ == "__main__":
    test_runner.main()
