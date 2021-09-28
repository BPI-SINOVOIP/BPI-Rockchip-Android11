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

import collections
import json
import logging
import os
import re
import zipfile

try:
    from importlib import resources
except ImportError:
    resources = None

# The tags in VNDK list:
# Low-level NDK libraries that can be used by framework and vendor modules.
LL_NDK = "LLNDK"

# Same-process HAL implementation in vendor partition.
SP_HAL = "SP-HAL"

# Framework libraries that can be used by vendor modules except same-process HAL
# and its dependencies in vendor partition.
VNDK = "VNDK-core"

# VNDK dependencies that vendor modules cannot directly access.
VNDK_PRIVATE = "VNDK-core-private"

# Same-process HAL dependencies in framework.
VNDK_SP = "VNDK-SP"

# VNDK-SP dependencies that vendor modules cannot directly access.
VNDK_SP_PRIVATE = "VNDK-SP-private"

# The tuples of (ABI name, bitness, arch name). 64-bit comes before 32-bit in
# order to sequentially search for longest prefix.
_ABI_LIST = (
    ("arm64", 64, "arm64_armv8-a"),
    ("arm64", 32, "arm_armv8-a"),
    ("arm", 32, "arm_armv7-a-neon"),
    ("x86_64", 64, "x86_x86_64"),
    ("x86_64", 32, "x86_64"),
    ("x86", 32, "x86"),
)

# The data directory.
_GOLDEN_DIR = os.path.join("vts", "testcases", "vndk", "golden")

# The data package.
_RESOURCE_PACKAGE = "vts.testcases.vndk";

# The name of the zip file containing ABI dumps.
_ABI_DUMP_ZIP_NAME = "abi_dump.zip"

# Regular expression prefix for library name patterns.
_REGEX_PREFIX = "[regex]"

def LoadDefaultVndkVersion(data_file_path):
    """Loads the name of the data directory for devices with no VNDK version.

    Args:
        data_file_path: The path to VTS data directory.

    Returns:
        A string, the directory name.
        None if fails to load the name.
    """
    try:
        with open(os.path.join(data_file_path, _GOLDEN_DIR,
                               "platform_vndk_version.txt"), "r") as f:
            return f.read().strip()
    except IOError:
        logging.error("Cannot load default VNDK version.")
        return None


def GetAbiDumpDirectory(data_file_path, version, binder_bitness, abi_name,
                        abi_bitness):
    """Returns the VNDK dump directory on host.

    Args:
        data_file_path: The path to VTS data directory.
        version: A string, the VNDK version.
        binder_bitness: A string or an integer, 32 or 64.
        abi_name: A string, the ABI of the library dump.
        abi_bitness: A string or an integer, 32 or 64.

    Returns:
        A string, the path to the dump directory.
        None if there is no directory for the version and ABI.
    """
    try:
        abi_dir = next(x[0] for x in _ABI_LIST if abi_name.startswith(x[0]))
    except StopIteration:
        logging.warning("Unknown ABI %s.", abi_name)
        return None

    version_dir = (version if version else
                   LoadDefaultVndkVersion(data_file_path))
    if not version_dir:
        return None

    dump_dir = os.path.join(
        data_file_path, _GOLDEN_DIR, version_dir,
        "binder64" if str(binder_bitness) == "64" else "binder32",
        abi_dir, "lib64" if str(abi_bitness) == "64" else "lib")

    if not os.path.isdir(dump_dir):
        logging.warning("%s is not a directory.", dump_dir)
        return None

    return dump_dir


class AbiDumpResource:
    """The class for loading ABI dumps from the zip in resources."""

    def __init__(self):
        self._resource = None
        self.zip_file = None

    def __enter__(self):
        self._resource = resources.open_binary(_RESOURCE_PACKAGE,
                                               _ABI_DUMP_ZIP_NAME)
        self.zip_file = zipfile.ZipFile(self._resource, "r")
        return self

    def __exit__(self, exc_type, exc_val, traceback):
        if self._resource:
            self._resource.close()
        if self.zip_file:
            self.zip_file.close()


def GetAbiDumpPathsFromResources(version, binder_bitness, abi_name, abi_bitness):
    """Returns the VNDK dump paths in resources.

    Args:
        version: A string, the VNDK version.
        binder_bitness: A string or an integer, 32 or 64.
        abi_name: A string, the ABI of the library dump.
        abi_bitness: A string or an integer, 32 or 64.

    Returns:
        A dict of {library name: dump resource path}. For example,
        {"libbase.so": "R/64/arm64_armv8-a/source-based/libbase.so.lsdump"}.
        If there is no dump for the version and ABI, this function returns an
        empty dict.
    """
    if not resources:
        logging.error("Could not import resources module.")
        return dict()

    abi_bitness = int(abi_bitness)
    try:
        arch_name = next(x[2] for x in _ABI_LIST if
                         abi_name.startswith(x[0]) and x[1] == abi_bitness)
    except StopIteration:
        logging.warning("Unknown %d-bit ABI %s.", abi_bitness, abi_name)
        return dict()

    # The separator in zipped path is always "/".
    dump_dir = "/".join((version, str(binder_bitness), arch_name,
                         "source-based")) + "/"

    dump_paths = dict()

    with AbiDumpResource() as dump_resource:
        for path in dump_resource.zip_file.namelist():
            if path.startswith(dump_dir) and path.endswith(".lsdump"):
                lib_name = path[len(dump_dir):-len(".lsdump")]
                dump_paths[lib_name] = path

    return dump_paths


def _LoadVndkLibraryListsFile(vndk_lists, tags, vndk_lib_list_file):
    """Load VNDK libraries from the file to the specified tuple.

    Args:
        vndk_lists: The output tuple of lists containing library names.
        tags: Strings, the tags of the libraries to find.
        vndk_lib_list_file: The file object containing the VNDK library list.
    """

    lib_sets = collections.defaultdict(set)

    # Load VNDK tags from the list.
    for line in vndk_lib_list_file:
        # Ignore comments.
        if line.startswith('#'):
            continue

        # Split columns.
        cells = line.split(': ', 1)
        if len(cells) < 2:
            continue
        tag = cells[0]
        lib_name = cells[1].strip()

        lib_sets[tag].add(lib_name)

    # Compute VNDK-core-private and VNDK-SP-private.
    private = lib_sets.get('VNDK-private', set())

    lib_sets[VNDK_PRIVATE].update(lib_sets[VNDK] & private)
    lib_sets[VNDK_SP_PRIVATE].update(lib_sets[VNDK_SP] & private)

    lib_sets[LL_NDK].difference_update(private)
    lib_sets[VNDK].difference_update(private)
    lib_sets[VNDK_SP].difference_update(private)

    # Update the output entries.
    for index, tag in enumerate(tags):
        for lib_name in lib_sets.get(tag, tuple()):
            if lib_name.startswith(_REGEX_PREFIX):
                lib_name = lib_name[len(_REGEX_PREFIX):]
            vndk_lists[index].append(lib_name)


def LoadVndkLibraryLists(data_file_path, version, *tags):
    """Find the VNDK libraries with specific tags.

    Args:
        data_file_path: The path to VTS data directory.
        version: A string, the VNDK version.
        *tags: Strings, the tags of the libraries to find.

    Returns:
        A tuple of lists containing library names. Each list corresponds to
        one tag in the argument. For SP-HAL, the returned names are regular
        expressions.
        None if the spreadsheet for the version is not found.
    """
    version_dir = (version if version else
                   LoadDefaultVndkVersion(data_file_path))
    if not version_dir:
        return None

    vndk_lib_list_path = os.path.join(
        data_file_path, _GOLDEN_DIR, version_dir, "vndk-lib-list.txt")
    if not os.path.isfile(vndk_lib_list_path):
        logging.warning("Cannot load %s.", vndk_lib_list_path)
        return None

    vndk_lib_extra_list_path = os.path.join(
        data_file_path, _GOLDEN_DIR, version_dir, "vndk-lib-extra-list.txt")
    if not os.path.isfile(vndk_lib_extra_list_path):
        logging.warning("Cannot load %s.", vndk_lib_extra_list_path)
        return None

    vndk_lists = tuple([] for x in tags)

    with open(vndk_lib_list_path, "r") as f:
        _LoadVndkLibraryListsFile(vndk_lists, tags, f)
    with open(vndk_lib_extra_list_path, "r") as f:
        _LoadVndkLibraryListsFile(vndk_lists, tags, f)
    return vndk_lists


def LoadVndkLibraryListsFromResources(version, *tags):
    """Find the VNDK libraries with specific tags in resources.

    Args:
        version: A string, the VNDK version.
        *tags: Strings, the tags of the libraries to find.

    Returns:
        A tuple of lists containing library names. Each list corresponds to
        one tag in the argument. For SP-HAL, the returned names are regular
        expressions.
        None if the VNDK list for the version is not found.
    """
    if not resources:
        logging.error("Could not import resources module.")
        return None

    version_str = (version if version and re.match("\\d+", version) else
                   "current")
    vndk_lib_list_name = version_str + ".txt"
    vndk_lib_extra_list_name = "vndk-lib-extra-list-" + version_str + ".txt"

    if not resources.is_resource(_RESOURCE_PACKAGE, vndk_lib_list_name):
        logging.warning("Cannot load %s.", vndk_lib_list_name)
        return None

    if not resources.is_resource(_RESOURCE_PACKAGE, vndk_lib_extra_list_name):
        logging.warning("Cannot load %s.", vndk_lib_extra_list_name)
        return None

    vndk_lists = tuple([] for x in tags)

    with resources.open_text(_RESOURCE_PACKAGE, vndk_lib_list_name) as f:
        _LoadVndkLibraryListsFile(vndk_lists, tags, f)
    with resources.open_text(_RESOURCE_PACKAGE, vndk_lib_extra_list_name) as f:
        _LoadVndkLibraryListsFile(vndk_lists, tags, f)
    return vndk_lists
