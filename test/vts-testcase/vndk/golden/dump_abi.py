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

import argparse
import os
import subprocess
import sys

import extract_lsdump


def _ExecuteCommand(cmd, **kwargs):
    """Executes a command and returns stdout.

    Args:
        cmd: A list of strings, the command to execute.
        **kwargs: The arguments passed to subprocess.Popen.

    Returns:
        A string, the stdout.
    """
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kwargs)
    stdout, stderr = proc.communicate()
    if proc.returncode:
        sys.exit("Command failed: %s\nstdout=%s\nstderr=%s" % (
                 cmd, stdout, stderr))
    if stderr:
        print("Warning: cmd=%s\nstdout=%s\nstderr=%s" % (cmd, stdout, stderr))
    return stdout.strip()


def GetBuildVariables(build_top_dir, abs_path, vars):
    """Gets values of variables from build config.

    Args:
        build_top_dir: The path to root directory of Android source.
        abs_path: A boolean, whether to convert the values to absolute paths.
        vars: A list of strings, the names of the variables.

    Returns:
        A list of strings which are the values of the variables.
    """
    cmd = ["build/soong/soong_ui.bash", "--dumpvars-mode",
           ("--abs-vars" if abs_path else "--vars"), " ".join(vars)]
    stdout = _ExecuteCommand(cmd, cwd=build_top_dir)
    print(stdout)
    return [line.split("=", 1)[1].strip("'") for line in stdout.splitlines()]


def _LoadLibraryNamesFromTxt(vndk_lib_list_file):
    """Loads VNDK and VNDK-SP library names from a VNDK library list.

    Args:
        vndk_lib_list_file: A file object of
                            build/make/target/product/vndk/current.txt

    Returns:
        A list of strings, the VNDK and VNDK-SP library names with vndk/vndk-sp
        directory prefixes.
    """
    tags = (
        ("VNDK-core: ", len("VNDK-core: "), False),
        ("VNDK-SP: ", len("VNDK-SP: "), False),
        ("VNDK-private: ", len("VNDK-private: "), True),
        ("VNDK-SP-private: ", len("VNDK-SP-private: "), True),
    )
    lib_names = set()
    lib_names_exclude = set()
    for line in vndk_lib_list_file:
        for tag, tag_len, is_exclude in tags:
            if line.startswith(tag):
                lib_name = line[tag_len:].strip()
                if is_exclude:
                    lib_names_exclude.add(lib_name)
                else:
                    lib_names.add(lib_name)
    return sorted(lib_names - lib_names_exclude)


def _LoadLibraryNames(file_names):
    """Loads library names from files.

    Each element in the input list can be a .so file or a .txt file. The
    returned list consists of:
    - The .so file names in the input list.
    - The libraries tagged with VNDK-core or VNDK-SP in the .txt file.

    Args:
        file_names: A list of strings, the library or text file names.

    Returns:
        A list of strings, the library names (probably with vndk/vndk-sp
        directory prefixes).
    """
    lib_names = []
    for file_name in file_names:
        if file_name.endswith(".so"):
            lib_names.append(file_name)
        else:
            with open(file_name, "r") as txt_file:
                lib_names.extend(_LoadLibraryNamesFromTxt(txt_file))
    return lib_names


def DumpAbi(output_dir, lib_names, lsdump_path):
    """Generates ABI dumps from library lsdumps.

    Args:
        output_dir: The output directory of dump files.
        lib_names: The names of the libraries to dump.
        lsdump_path: The path to the directory containing lsdumps.

    Returns:
        A list of strings, the libraries whose ABI dump fails to be created.
    """
    missing_dumps = []
    for lib_name in lib_names:
        dump_path = os.path.join(output_dir, lib_name + '.abi.dump')
        lib_lsdump_path = os.path.join(lsdump_path, lib_name + '.lsdump')
        if os.path.isfile(lib_lsdump_path + '.gz'):
            lib_lsdump_path += '.gz'

        print(lib_lsdump_path)
        try:
            extract_lsdump.ParseLsdumpFile(lib_lsdump_path, dump_path)
        except extract_lsdump.LsdumpError as e:
            missing_dumps.append(lib_name)
            print(e)
        else:
            print('Output: ' + dump_path)
        print('')
    return missing_dumps


def _GetTargetArchDir(target_arch, target_arch_variant):
    if target_arch == target_arch_variant:
        return target_arch
    return '{}_{}'.format(target_arch, target_arch_variant)


def _GetAbiBitnessFromArch(target_arch):
    arch_bitness = {
        'arm': '32',
        'arm64': '64',
        'x86': '32',
        'x86_64': '64',
    }
    return arch_bitness[target_arch]


def main():
    # Parse arguments
    description = (
        'Generates VTS VNDK ABI test abidumps from lsdump. '
        'Option values are read from build variables if no value is given. '
        'If none of the options are specified, then abidumps for target second '
        'arch are also generated.'
    )
    arg_parser = argparse.ArgumentParser(description=description)
    arg_parser.add_argument("file", nargs="*",
                            help="the libraries to dump. Each file can be "
                                 ".so or .txt. The text file can be found at "
                                 "build/make/target/product/vndk/current.txt.")
    arg_parser.add_argument("--output", "-o", action="store",
                            help="output directory for ABI reference dump. "
                                 "Default value is PLATFORM_VNDK_VERSION.")
    arg_parser.add_argument('--platform-vndk-version',
                            help='platform VNDK version. '
                                 'Default value is PLATFORM_VNDK_VERSION.')
    arg_parser.add_argument('--binder-bitness',
                            choices=['32', '64'],
                            help='bitness of binder interface. '
                                 'Default value is 32 if BINDER32BIT is set '
                                 'else is 64.')
    arg_parser.add_argument('--target-main-arch',
                            choices=['arm', 'arm64', 'x86', 'x86_64'],
                            help='main CPU arch of the device. '
                                 'Default value is TARGET_ARCH.')
    arg_parser.add_argument('--target-arch',
                            choices=['arm', 'arm64', 'x86', 'x86_64'],
                            help='CPU arch of the libraries to dump. '
                                 'Default value is TARGET_ARCH.')
    arg_parser.add_argument('--target-arch-variant',
                            help='CPU arch variant of the libraries to dump. '
                                 'Default value is TARGET_ARCH_VARIANT.')

    args = arg_parser.parse_args()

    build_top_dir = os.getenv("ANDROID_BUILD_TOP")
    if not build_top_dir:
        sys.exit("env var ANDROID_BUILD_TOP is not set")

    # If some options are not specified, read build variables as default values.
    if not all([args.platform_vndk_version,
                args.binder_bitness,
                args.target_main_arch,
                args.target_arch,
                args.target_arch_variant]):
        [platform_vndk_version,
         binder_32_bit,
         target_arch,
         target_arch_variant,
         target_2nd_arch,
         target_2nd_arch_variant] = GetBuildVariables(
            build_top_dir,
            False,
            ['PLATFORM_VNDK_VERSION',
             'BINDER32BIT',
             'TARGET_ARCH',
             'TARGET_ARCH_VARIANT',
             'TARGET_2ND_ARCH',
             'TARGET_2ND_ARCH_VARIANT']
        )
        target_main_arch = target_arch
        binder_bitness = '32' if binder_32_bit else '64'

    if args.platform_vndk_version:
        platform_vndk_version = args.platform_vndk_version

    if args.binder_bitness:
        binder_bitness = args.binder_bitness

    if args.target_main_arch:
        target_main_arch = args.target_main_arch

    if args.target_arch:
        target_arch = args.target_arch

    if args.target_arch_variant:
        target_arch_variant = args.target_arch_variant

    dump_targets = [(platform_vndk_version,
                     binder_bitness,
                     target_main_arch,
                     target_arch,
                     target_arch_variant)]

    # If all options are not specified, then also create dump for 2nd arch.
    if not any([args.platform_vndk_version,
                args.binder_bitness,
                args.target_main_arch,
                args.target_arch,
                args.target_arch_variant]):
        dump_targets.append((platform_vndk_version,
                             binder_bitness,
                             target_main_arch,
                             target_2nd_arch,
                             target_2nd_arch_variant))

    for target_tuple in dump_targets:
        (platform_vndk_version,
         binder_bitness,
         target_main_arch,
         target_arch,
         target_arch_variant) = target_tuple

        # Determine abi_bitness from target architecture
        abi_bitness = _GetAbiBitnessFromArch(target_arch)

        # Generate ABI dump from lsdump in TOP/prebuilts/abi-dumps
        lsdump_path = os.path.join(
            build_top_dir,
            'prebuilts',
            'abi-dumps',
            'vndk',
            platform_vndk_version,
            binder_bitness,
            _GetTargetArchDir(target_arch, target_arch_variant),
            'source-based')
        if not os.path.exists(lsdump_path):
            print('Warning: lsdump path does not exist: ' + lsdump_path)
            print('No abidump created.')
            continue

        output_dir = os.path.join(
            args.output if args.output else platform_vndk_version,
            'binder' + binder_bitness,
            target_main_arch,
            'lib64' if abi_bitness == '64' else 'lib')
        print("OUTPUT_DIR=" + output_dir)

        lib_names = _LoadLibraryNames(args.file)

        missing_dumps = DumpAbi(output_dir, lib_names, lsdump_path)

        if missing_dumps:
            print('Warning: Fails to create ABI dumps for libraries:')
            for lib_name in missing_dumps:
                print(lib_name)


if __name__ == "__main__":
    main()
