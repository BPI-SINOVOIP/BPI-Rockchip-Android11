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

import json
import logging
import os
import shutil
import tempfile

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import keys
from vts.runners.host import test_runner
from vts.runners.host import utils
from vts.testcases.vndk.golden import vndk_data
from vts.utils.python.file import target_file_utils
from vts.utils.python.library import elf_parser
from vts.utils.python.library.vtable import vtable_dumper
from vts.utils.python.vndk import vndk_utils


class VtsVndkAbiTest(base_test.BaseTestClass):
    """A test module to verify ABI compliance of vendor libraries.

    Attributes:
        _dut: the AndroidDevice under test.
        _temp_dir: The temporary directory for libraries copied from device.
        _vndk_version: String, the VNDK version supported by the device.
        data_file_path: The path to VTS data directory.
    """

    def setUpClass(self):
        """Initializes data file path, device, and temporary directory."""
        required_params = [keys.ConfigKeys.IKEY_DATA_FILE_PATH]
        self.getUserParams(required_params)
        self._dut = self.android_devices[0]
        self._temp_dir = tempfile.mkdtemp()
        self._vndk_version = self._dut.vndk_version

    def tearDownClass(self):
        """Deletes the temporary directory."""
        logging.info("Delete %s", self._temp_dir)
        shutil.rmtree(self._temp_dir)

    def _PullOrCreateDir(self, target_dir, host_dir):
        """Copies a directory from device. Creates an empty one if not exist.

        Args:
            target_dir: The directory to copy from device.
            host_dir: The directory to copy to host.
        """
        if not target_file_utils.IsDirectory(target_dir, self._dut.shell):
            logging.info("%s doesn't exist. Create %s.", target_dir, host_dir)
            os.makedirs(host_dir)
            return
        parent_dir = os.path.dirname(host_dir)
        if parent_dir and not os.path.isdir(parent_dir):
            os.makedirs(parent_dir)
        logging.info("adb pull %s %s", target_dir, host_dir)
        self._dut.adb.pull(target_dir, host_dir)

    def _ToHostPath(self, target_path):
        """Maps target path to host path in self._temp_dir."""
        return os.path.join(self._temp_dir, *target_path.strip("/").split("/"))

    @staticmethod
    def _LoadGlobalSymbolsFromDump(dump_obj):
        """Loads global symbols from a dump object.

        Args:
            dump_obj: A dict, the dump in JSON format.

        Returns:
            A set of strings, the symbol names.
        """
        symbols = set()
        for key in ("elf_functions", "elf_objects"):
            symbols.update(
                symbol.get("name", "") for symbol in dump_obj.get(key, []) if
                symbol.get("binding", "global") == "global")
        return symbols

    def _DiffElfSymbols(self, dump_obj, parser):
        """Checks if a library includes all symbols in a dump.

        Args:
            dump_obj: A dict, the dump in JSON format.
            parser: An elf_parser.ElfParser that loads the library.

        Returns:
            A list of strings, the global symbols that are in the dump but not
            in the library.

        Raises:
            elf_parser.ElfError if fails to load the library.
        """
        dump_symbols = self._LoadGlobalSymbolsFromDump(dump_obj)
        lib_symbols = parser.ListGlobalDynamicSymbols(include_weak=True)
        return sorted(dump_symbols.difference(lib_symbols))

    @staticmethod
    def _DiffVtableComponent(offset, expected_symbol, vtable):
        """Checks if a symbol is in a vtable entry.

        Args:
            offset: An integer, the offset of the expected symbol.
            exepcted_symbol: A string, the name of the expected symbol.
            vtable: A dict of {offset: [entry]} where offset is an integer and
                    entry is an instance of vtable_dumper.VtableEntry.

        Returns:
            A list of strings, the actual possible symbols if expected_symbol
            does not match the vtable entry.
            None if expected_symbol matches the entry.
        """
        if offset not in vtable:
            return []

        entry = vtable[offset]
        if not entry.names:
            return [hex(entry.value).rstrip('L')]

        if expected_symbol not in entry.names:
            return entry.names

    def _DiffVtableComponents(self, dump_obj, dumper):
        """Checks if a library includes all vtable entries in a dump.

        Args:
            dump_obj: A dict, the dump in JSON format.
            dumper: An vtable_dumper.VtableDumper that loads the library.

        Returns:
            A list of tuples (VTABLE, OFFSET, EXPECTED_SYMBOL, ACTUAL).
            ACTUAL can be "missing", a list of symbol names, or an ELF virtual
            address.

        Raises:
            vtable_dumper.VtableError if fails to dump vtable from the library.
        """
        function_kinds = [
            "function_pointer",
            "complete_dtor_pointer",
            "deleting_dtor_pointer"
        ]
        non_function_kinds = [
            "vcall_offset",
            "vbase_offset",
            "offset_to_top",
            "rtti",
            "unused_function_pointer"
        ]
        default_vtable_component_kind = "function_pointer"

        global_symbols = self._LoadGlobalSymbolsFromDump(dump_obj)

        lib_vtables = {vtable.name: vtable
                       for vtable in dumper.DumpVtables()}
        logging.debug("\n\n".join(str(vtable)
                                  for _, vtable in lib_vtables.iteritems()))

        vtables_diff = []
        for record_type in dump_obj.get("record_types", []):
            # Since Android R, unique_id has been replaced with linker_set_key.
            # unique_id starts with "_ZTI"; linker_set_key starts with "_ZTS".
            type_name_symbol = record_type.get("unique_id", "")
            if type_name_symbol:
                vtable_symbol = type_name_symbol.replace("_ZTS", "_ZTV", 1)
            else:
                type_name_symbol = record_type.get("linker_set_key", "")
                vtable_symbol = type_name_symbol.replace("_ZTI", "_ZTV", 1)

            # Skip if the vtable symbol isn't global.
            if vtable_symbol not in global_symbols:
                continue

            # Collect vtable entries from library dump.
            if vtable_symbol in lib_vtables:
                lib_vtable = {entry.offset: entry
                              for entry in lib_vtables[vtable_symbol].entries}
            else:
                lib_vtable = dict()

            for index, entry in enumerate(record_type.get("vtable_components",
                                                          [])):
                entry_offset = index * int(self.abi_bitness) // 8
                entry_kind = entry.get("kind", default_vtable_component_kind)
                entry_symbol = entry.get("mangled_component_name", "")
                entry_is_pure = entry.get("is_pure", False)

                if entry_kind in non_function_kinds:
                    continue

                if entry_kind not in function_kinds:
                    logging.warning("%s: Unexpected vtable entry kind %s",
                                    vtable_symbol, entry_kind)

                if entry_symbol not in global_symbols:
                    # Itanium cxx abi doesn't specify pure virtual vtable
                    # entry's behaviour. However we can still do some checks
                    # based on compiler behaviour.
                    # Even though we don't check weak symbols, we can still
                    # issue a warning when a pure virtual function pointer
                    # is missing.
                    if entry_is_pure and entry_offset not in lib_vtable:
                        logging.warning("%s: Expected pure virtual function"
                                        "in %s offset %s",
                                        vtable_symbol, vtable_symbol,
                                        entry_offset)
                    continue

                diff_symbols = self._DiffVtableComponent(
                    entry_offset, entry_symbol, lib_vtable)
                if diff_symbols is None:
                    continue

                vtables_diff.append(
                    (vtable_symbol, str(entry_offset), entry_symbol,
                     (",".join(diff_symbols) if diff_symbols else "missing")))

        return vtables_diff

    def _ScanLibDirs(self, dump_dir, lib_dirs, dump_version):
        """Compares dump files with libraries copied from device.

        Args:
            dump_dir: The directory containing dump files.
            lib_dirs: The list of directories containing libraries.
            dump_version: The VNDK version of the dump files. If the device has
                          no VNDK version or has extension in vendor partition,
                          this method compares the unversioned VNDK directories
                          with the dump directories of the given version.

        Returns:
            A list of strings, the incompatible libraries.
        """
        error_list = []
        dump_paths = dict()
        lib_paths = dict()
        for parent_dir, dump_name in utils.iterate_files(dump_dir):
            dump_path = os.path.join(parent_dir, dump_name)
            if dump_path.endswith(".dump"):
                lib_name = dump_name.rpartition(".dump")[0]
                dump_paths[lib_name] = dump_path
            else:
                logging.warning("Unknown dump: %s", dump_path)

        for lib_dir in lib_dirs:
            for parent_dir, lib_name in utils.iterate_files(lib_dir):
                if lib_name not in lib_paths:
                    lib_paths[lib_name] = os.path.join(parent_dir, lib_name)

        for lib_name, dump_path in dump_paths.iteritems():
            if lib_name not in lib_paths:
                logging.info("%s: Not found on target", lib_name)
                continue
            lib_path = lib_paths[lib_name]
            rel_path = os.path.relpath(lib_path, self._temp_dir)

            has_exception = False
            missing_symbols = []
            vtable_diff = []

            try:
                with open(dump_path, "r") as dump_file:
                    dump_obj = json.load(dump_file)
                with vtable_dumper.VtableDumper(lib_path) as dumper:
                    missing_symbols = self._DiffElfSymbols(
                        dump_obj, dumper)
                    vtable_diff = self._DiffVtableComponents(
                        dump_obj, dumper)
            except (IOError,
                    elf_parser.ElfError,
                    vtable_dumper.VtableError) as e:
                logging.exception("%s: Cannot diff ABI", rel_path)
                has_exception = True

            if missing_symbols:
                logging.error("%s: Missing Symbols:\n%s",
                              rel_path, "\n".join(missing_symbols))
            if vtable_diff:
                logging.error("%s: Vtable Difference:\n"
                              "vtable offset expected actual\n%s",
                              rel_path,
                              "\n".join(" ".join(e) for e in vtable_diff))
            if (has_exception or missing_symbols or vtable_diff):
                error_list.append(rel_path)
            else:
                logging.info("%s: Pass", rel_path)
        return error_list

    @staticmethod
    def _GetLinkerSearchIndex(target_path):
        """Returns the key for sorting linker search paths."""
        index = 0
        for prefix in ("/odm", "/vendor", "/apex"):
            if target_path.startswith(prefix):
                return index
            index += 1
        return index

    def testAbiCompatibility(self):
        """Checks ABI compliance of VNDK libraries."""
        primary_abi = self._dut.getCpuAbiList()[0]
        binder_bitness = self._dut.getBinderBitness()
        asserts.assertTrue(binder_bitness,
                           "Cannot determine binder bitness.")
        dump_version = (self._vndk_version if self._vndk_version else
                        vndk_data.LoadDefaultVndkVersion(self.data_file_path))
        asserts.assertTrue(dump_version,
                           "Cannot load default VNDK version.")

        dump_dir = vndk_data.GetAbiDumpDirectory(
            self.data_file_path,
            dump_version,
            binder_bitness,
            primary_abi,
            self.abi_bitness)
        asserts.assertTrue(
            dump_dir,
            "No dump files. version: %s ABI: %s bitness: %s" % (
                self._vndk_version, primary_abi, self.abi_bitness))
        logging.info("dump dir: %s", dump_dir)

        target_dirs = vndk_utils.GetVndkExtDirectories(self.abi_bitness)
        target_dirs += vndk_utils.GetVndkSpExtDirectories(self.abi_bitness)
        target_dirs += [vndk_utils.GetVndkDirectory(self.abi_bitness,
                                                    self._vndk_version)]
        target_dirs.sort(key=self._GetLinkerSearchIndex)

        host_dirs = [self._ToHostPath(x) for x in target_dirs]
        for target_dir, host_dir in zip(target_dirs, host_dirs):
            self._PullOrCreateDir(target_dir, host_dir)

        assert_lines = self._ScanLibDirs(dump_dir, host_dirs, dump_version)
        if assert_lines:
            error_count = len(assert_lines)
            if error_count > 20:
                assert_lines = assert_lines[:20] + ["..."]
            assert_lines.append("Total number of errors: " + str(error_count))
            asserts.fail("\n".join(assert_lines))


if __name__ == "__main__":
    test_runner.main()
