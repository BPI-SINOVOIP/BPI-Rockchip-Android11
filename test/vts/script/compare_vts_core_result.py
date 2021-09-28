#
# Copyright (C) 2020 The Android Open Source Project
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
import sys

import xml.etree.ElementTree as ET

VTS_TO_VTS_CORE_MAPPING = {
    "VtsHalCameraProviderV2_5Target": ["VtsHalCameraProviderV2_4TargetTest"],
    "VtsKernelLiblpTest": ["vts_core_liblp_test"],
    "VtsFastbootVerification": ["FastbootVerifyUserspaceTest"],
    "VtsKernelMemInfoTest": ["vts_core_meminfo_test"],
    "VtsKernelQtaguidTest": ["vts_test_binary_qtaguid_module"],
    "VtsTrebleFrameworkVintfTest": ["vts_treble_vintf_framework_test"],
    "VtsKernelLibdmTest": ["vts_libdm_test"],
    "VtsGsiBootTest": ["vts_gsi_boot_test"],
    "VtsHalBaseV1_0TargetTest": ["vts_ibase_test"],
    "VtsKernelLibcutilsTest": ["KernelLibcutilsTest"],
    "VtsKernelNetBpfTest": ["bpf_module_test"],
    "VtsSecurityAvb": ["vts_security_avb_test"],
    "VtsKernelBinderTest": ["binderSafeInterfaceTest",
                            "binderLibTest",
                            "binderDriverInterfaceTest",
                            "memunreachable_binder_test"],
    "VtsKernelTunTest": ["vts_kernel_tun_test"],
    "VtsTrebleVendorVintfTest": ["vts_treble_vintf_vendor_test"],
}

IGNORED_MODULES = ["VtsKernelHwBinder"]

def usage():
    """Prints usage and exits."""
    print("Arguments Error: Please specify the test_result file of "
          "vts and vts-core.")
    print "Usage: <this script> <test_result_of_vts> <test_result_of_vts_core>"
    print "       e.g. python compare_vts_core_result.py vts_test_result.xml vts_core_test_result.xml"
    exit(-1)

def _get_all_modules_name_from_xml(result_file):
    """Get all modules name from a test_result file.

    Args:
        restul_file: The path of a test result file.

    Returns:
        A set of all test module names.
    """
    xml_tree = ET.parse(result_file)
    xml_root = xml_tree.getroot()
    all_modules = set()
    for elem in xml_root.iter(tag="Module"):
        all_modules.add(elem.attrib['name'])
    return all_modules

def _compare_vts_and_vts_core_module(vts_modules, vts_core_modules):
    """Compare vts and vts-core module names.

    Args:
        vts_modules: All modules names of vts.
        vts_core_modules: All modules name of vts-core.

    Returns:
        A set of the modules that in the vts but not in the vts-core.
    """
    module_name_convert_keys = VTS_TO_VTS_CORE_MAPPING.keys()
    not_converted_modules = []
    for vts_module_name in vts_modules:
        vts_core_module_names = [vts_module_name]
        if vts_module_name in module_name_convert_keys:
            vts_core_module_names = VTS_TO_VTS_CORE_MAPPING[vts_module_name]
        for necessary_module_name in vts_core_module_names:
            if necessary_module_name not in vts_core_modules:
                Hal_test_name = necessary_module_name + "Test"
                if Hal_test_name not in vts_core_modules:
                    if vts_module_name not in IGNORED_MODULES:
                        not_converted_modules.append(vts_module_name)
                        break
    return sorted(not_converted_modules)

def compare_vts_and_vts_core_result_file(vts_result_file, vts_core_result_file):
    """Compare the test result file of vts and vts-core.

    Args:
        vts_result_file: The path of the test result file of vts.
        vts_core_result_file: The path of the test result file of vts-core.

    Returns:
        A set of the modules that in the vts but not in the vts-core.
    """
    vts_modules = _get_all_modules_name_from_xml(vts_result_file)
    vts_core_modules = _get_all_modules_name_from_xml(vts_core_result_file)
    not_converted_modules = _compare_vts_and_vts_core_module(vts_modules, vts_core_modules)
    for module in not_converted_modules:
        print module

def main(argv):
    """Entry point of atest script.

    Args:
        argv: A list of arguments.
    """
    if len(argv) != 3:
        usage()
    compare_vts_and_vts_core_result_file(argv[1], argv[2])

if __name__ == '__main__':
    main(sys.argv)
