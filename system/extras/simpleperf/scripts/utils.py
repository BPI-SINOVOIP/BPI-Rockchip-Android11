#!/usr/bin/env python
#
# Copyright (C) 2016 The Android Open Source Project
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

"""utils.py: export utility functions.
"""

from __future__ import print_function
import argparse
import logging
import os
import os.path
import re
import shutil
import subprocess
import sys
import time

def get_script_dir():
    return os.path.dirname(os.path.realpath(__file__))

def is_windows():
    return sys.platform == 'win32' or sys.platform == 'cygwin'

def is_darwin():
    return sys.platform == 'darwin'

def get_platform():
    if is_windows():
        return 'windows'
    if is_darwin():
        return 'darwin'
    return 'linux'

def is_python3():
    return sys.version_info >= (3, 0)


def log_debug(msg):
    logging.debug(msg)


def log_info(msg):
    logging.info(msg)


def log_warning(msg):
    logging.warning(msg)


def log_fatal(msg):
    raise Exception(msg)

def log_exit(msg):
    sys.exit(msg)

def disable_debug_log():
    logging.getLogger().setLevel(logging.WARN)

def set_log_level(level_name):
    if level_name == 'debug':
        level = logging.DEBUG
    elif level_name == 'info':
        level = logging.INFO
    elif level_name == 'warning':
        level = logging.WARNING
    else:
        log_fatal('unknown log level: %s' % level_name)
    logging.getLogger().setLevel(level)

def str_to_bytes(str_value):
    if not is_python3():
        return str_value
    # In python 3, str are wide strings whereas the C api expects 8 bit strings,
    # hence we have to convert. For now using utf-8 as the encoding.
    return str_value.encode('utf-8')

def bytes_to_str(bytes_value):
    if not bytes_value:
        return ''
    if not is_python3():
        return bytes_value
    return bytes_value.decode('utf-8')

def get_target_binary_path(arch, binary_name):
    if arch == 'aarch64':
        arch = 'arm64'
    arch_dir = os.path.join(get_script_dir(), "bin", "android", arch)
    if not os.path.isdir(arch_dir):
        log_fatal("can't find arch directory: %s" % arch_dir)
    binary_path = os.path.join(arch_dir, binary_name)
    if not os.path.isfile(binary_path):
        log_fatal("can't find binary: %s" % binary_path)
    return binary_path


def get_host_binary_path(binary_name):
    dirname = os.path.join(get_script_dir(), 'bin')
    if is_windows():
        if binary_name.endswith('.so'):
            binary_name = binary_name[0:-3] + '.dll'
        elif '.' not in binary_name:
            binary_name += '.exe'
        dirname = os.path.join(dirname, 'windows')
    elif sys.platform == 'darwin': # OSX
        if binary_name.endswith('.so'):
            binary_name = binary_name[0:-3] + '.dylib'
        dirname = os.path.join(dirname, 'darwin')
    else:
        dirname = os.path.join(dirname, 'linux')
    dirname = os.path.join(dirname, 'x86_64' if sys.maxsize > 2 ** 32 else 'x86')
    binary_path = os.path.join(dirname, binary_name)
    if not os.path.isfile(binary_path):
        log_fatal("can't find binary: %s" % binary_path)
    return binary_path


def is_executable_available(executable, option='--help'):
    """ Run an executable to see if it exists. """
    try:
        subproc = subprocess.Popen([executable, option], stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
        subproc.communicate()
        return subproc.returncode == 0
    except OSError:
        return False

DEFAULT_NDK_PATH = {
    'darwin': 'Library/Android/sdk/ndk-bundle',
    'linux': 'Android/Sdk/ndk-bundle',
    'windows': 'AppData/Local/Android/sdk/ndk-bundle',
}

EXPECTED_TOOLS = {
    'adb': {
        'is_binutils': False,
        'test_option': 'version',
        'path_in_ndk': lambda _: '../platform-tools/adb',
    },
    'readelf': {
        'is_binutils': True,
        'accept_tool_without_arch': True,
    },
    'llvm-symbolizer': {
        'is_binutils': False,
        'path_in_ndk':
            lambda platform: 'toolchains/llvm/prebuilt/%s-x86_64/bin/llvm-symbolizer' % platform,
    },
    'objdump': {
        'is_binutils': True,
    },
    'strip': {
        'is_binutils': True,
    },
}

def _get_binutils_path_in_ndk(toolname, arch, platform):
    if not arch:
        arch = 'arm64'
    if arch == 'arm64':
        name = 'aarch64-linux-android-' + toolname
        path = 'toolchains/llvm/prebuilt/%s-x86_64/bin/%s' % (platform, name)
    elif arch == 'arm':
        name = 'arm-linux-androideabi-' + toolname
        path = 'toolchains/llvm/prebuilt/%s-x86_64/bin/%s' % (platform, name)
    elif arch == 'x86_64':
        name = 'x86_64-linux-android-' + toolname
        path = 'toolchains/llvm/prebuilt/%s-x86_64/bin/%s' % (platform, name)
    elif arch == 'x86':
        name = 'i686-linux-android-' + toolname
        path = 'toolchains/llvm/prebuilt/%s-x86_64/bin/%s' % (platform, name)
    else:
        log_fatal('unexpected arch %s' % arch)
    return (name, path)

def find_tool_path(toolname, ndk_path=None, arch=None):
    if toolname not in EXPECTED_TOOLS:
        return None
    tool_info = EXPECTED_TOOLS[toolname]
    is_binutils = tool_info['is_binutils']
    test_option = tool_info.get('test_option', '--help')
    platform = get_platform()
    if is_binutils:
        toolname_with_arch, path_in_ndk = _get_binutils_path_in_ndk(toolname, arch, platform)
    else:
        toolname_with_arch = toolname
        path_in_ndk = tool_info['path_in_ndk'](platform)
    path_in_ndk = path_in_ndk.replace('/', os.sep)

    # 1. Find tool in the given ndk path.
    if ndk_path:
        path = os.path.join(ndk_path, path_in_ndk)
        if is_executable_available(path, test_option):
            return path

    # 2. Find tool in the ndk directory containing simpleperf scripts.
    path = os.path.join('..', path_in_ndk)
    if is_executable_available(path, test_option):
        return path

    # 3. Find tool in the default ndk installation path.
    home = os.environ.get('HOMEPATH') if is_windows() else os.environ.get('HOME')
    if home:
        default_ndk_path = os.path.join(home, DEFAULT_NDK_PATH[platform].replace('/', os.sep))
        path = os.path.join(default_ndk_path, path_in_ndk)
        if is_executable_available(path, test_option):
            return path

    # 4. Find tool in $PATH.
    if is_executable_available(toolname_with_arch, test_option):
        return toolname_with_arch

    # 5. Find tool without arch in $PATH.
    if is_binutils and tool_info.get('accept_tool_without_arch'):
        if is_executable_available(toolname, test_option):
            return toolname
    return None


class AdbHelper(object):
    def __init__(self, enable_switch_to_root=True):
        adb_path = find_tool_path('adb')
        if not adb_path:
            log_exit("Can't find adb in PATH environment.")
        self.adb_path = adb_path
        self.enable_switch_to_root = enable_switch_to_root


    def run(self, adb_args):
        return self.run_and_return_output(adb_args)[0]


    def run_and_return_output(self, adb_args, log_output=True, log_stderr=True):
        adb_args = [self.adb_path] + adb_args
        log_debug('run adb cmd: %s' % adb_args)
        subproc = subprocess.Popen(adb_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        stdout_data, stderr_data = subproc.communicate()
        stdout_data = bytes_to_str(stdout_data)
        stderr_data = bytes_to_str(stderr_data)
        returncode = subproc.returncode
        result = (returncode == 0)
        if log_output and stdout_data and adb_args[1] != 'push' and adb_args[1] != 'pull':
            log_debug(stdout_data)
        if log_stderr and stderr_data:
            log_warning(stderr_data)
        log_debug('run adb cmd: %s  [result %s]' % (adb_args, result))
        return (result, stdout_data)

    def check_run(self, adb_args):
        self.check_run_and_return_output(adb_args)


    def check_run_and_return_output(self, adb_args, stdout_file=None, log_output=True):
        result, stdoutdata = self.run_and_return_output(adb_args, stdout_file, log_output)
        if not result:
            log_exit('run "adb %s" failed' % adb_args)
        return stdoutdata


    def _unroot(self):
        result, stdoutdata = self.run_and_return_output(['shell', 'whoami'])
        if not result:
            return
        if 'root' not in stdoutdata:
            return
        log_info('unroot adb')
        self.run(['unroot'])
        self.run(['wait-for-device'])
        time.sleep(1)


    def switch_to_root(self):
        if not self.enable_switch_to_root:
            self._unroot()
            return False
        result, stdoutdata = self.run_and_return_output(['shell', 'whoami'])
        if not result:
            return False
        if 'root' in stdoutdata:
            return True
        build_type = self.get_property('ro.build.type')
        if build_type == 'user':
            return False
        self.run(['root'])
        time.sleep(1)
        self.run(['wait-for-device'])
        result, stdoutdata = self.run_and_return_output(['shell', 'whoami'])
        return result and 'root' in stdoutdata

    def get_property(self, name):
        result, stdoutdata = self.run_and_return_output(['shell', 'getprop', name])
        return stdoutdata if result else None

    def set_property(self, name, value):
        return self.run(['shell', 'setprop', name, value])


    def get_device_arch(self):
        output = self.check_run_and_return_output(['shell', 'uname', '-m'])
        if 'aarch64' in output:
            return 'arm64'
        if 'arm' in output:
            return 'arm'
        if 'x86_64' in output:
            return 'x86_64'
        if '86' in output:
            return 'x86'
        log_fatal('unsupported architecture: %s' % output.strip())
        return ''


    def get_android_version(self):
        """ Get Android version on device, like 7 is for Android N, 8 is for Android O."""
        build_version = self.get_property('ro.build.version.release')
        android_version = 0
        if build_version:
            if not build_version[0].isdigit():
                c = build_version[0].upper()
                if c.isupper() and c >= 'L':
                    android_version = ord(c) - ord('L') + 5
            else:
                strs = build_version.split('.')
                if strs:
                    android_version = int(strs[0])
        return android_version


def flatten_arg_list(arg_list):
    res = []
    if arg_list:
        for items in arg_list:
            res += items
    return res


def remove(dir_or_file):
    if os.path.isfile(dir_or_file):
        os.remove(dir_or_file)
    elif os.path.isdir(dir_or_file):
        shutil.rmtree(dir_or_file, ignore_errors=True)


def open_report_in_browser(report_path):
    if is_darwin():
        # On darwin 10.12.6, webbrowser can't open browser, so try `open` cmd first.
        try:
            subprocess.check_call(['open', report_path])
            return
        except subprocess.CalledProcessError:
            pass
    import webbrowser
    try:
        # Try to open the report with Chrome
        browser = webbrowser.get('google-chrome')
        browser.open(report_path, new=0, autoraise=True)
    except webbrowser.Error:
        # webbrowser.get() doesn't work well on darwin/windows.
        webbrowser.open_new_tab(report_path)

def is_elf_file(path):
    if os.path.isfile(path):
        with open(path, 'rb') as fh:
            return fh.read(4) == b'\x7fELF'
    return False

def find_real_dso_path(dso_path_in_record_file, binary_cache_path):
    """ Given the path of a shared library in perf.data, find its real path in the file system. """
    if binary_cache_path:
        tmp_path = os.path.join(binary_cache_path, dso_path_in_record_file[1:])
        if is_elf_file(tmp_path):
            return tmp_path
    if is_elf_file(dso_path_in_record_file):
        return dso_path_in_record_file
    return None


class Addr2Nearestline(object):
    """ Use llvm-symbolizer to convert (dso_path, func_addr, addr) to (source_file, line).
        For instructions generated by C++ compilers without a matching statement in source code
        (like stack corruption check, switch optimization, etc.), addr2line can't generate
        line information. However, we want to assign the instruction to the nearest line before
        the instruction (just like objdump -dl). So we use below strategy:
        Instead of finding the exact line of the instruction in an address, we find the nearest
        line to the instruction in an address. If an address doesn't have a line info, we find
        the line info of address - 1. If still no line info, then use address - 2, address - 3,
        etc.

        The implementation steps are as below:
        1. Collect all (dso_path, func_addr, addr) requests before converting. This saves the
        times to call addr2line.
        2. Convert addrs to (source_file, line) pairs for each dso_path as below:
          2.1 Check if the dso_path has .debug_line. If not, omit its conversion.
          2.2 Get arch of the dso_path, and decide the addr_step for it. addr_step is the step we
          change addr each time. For example, since instructions of arm64 are all 4 bytes long,
          addr_step for arm64 can be 4.
          2.3 Use addr2line to find line info for each addr in the dso_path.
          2.4 For each addr without line info, use addr2line to find line info for
              range(addr - addr_step, addr - addr_step * 4 - 1, -addr_step).
          2.5 For each addr without line info, use addr2line to find line info for
              range(addr - addr_step * 5, addr - addr_step * 128 - 1, -addr_step).
              (128 is a guess number. A nested switch statement in
               system/core/demangle/Demangler.cpp has >300 bytes without line info in arm64.)
    """
    class Dso(object):
        """ Info of a dynamic shared library.
            addrs: a map from address to Addr object in this dso.
        """
        def __init__(self):
            self.addrs = {}

    class Addr(object):
        """ Info of an addr request.
            func_addr: start_addr of the function containing addr.
            source_lines: a list of [file_id, line_number] for addr.
                          source_lines[:-1] are all for inlined functions.
        """
        def __init__(self, func_addr):
            self.func_addr = func_addr
            self.source_lines = None

    def __init__(self, ndk_path, binary_cache_path, with_function_name):
        self.symbolizer_path = find_tool_path('llvm-symbolizer', ndk_path)
        if not self.symbolizer_path:
            log_exit("Can't find llvm-symbolizer. Please set ndk path with --ndk_path option.")
        self.readelf = ReadElf(ndk_path)
        self.dso_map = {}  # map from dso_path to Dso.
        self.binary_cache_path = binary_cache_path
        self.with_function_name = with_function_name
        # Saving file names for each addr takes a lot of memory. So we store file ids in Addr,
        # and provide data structures connecting file id and file name here.
        self.file_name_to_id = {}
        self.file_id_to_name = []
        self.func_name_to_id = {}
        self.func_id_to_name = []

    def add_addr(self, dso_path, func_addr, addr):
        dso = self.dso_map.get(dso_path)
        if dso is None:
            dso = self.dso_map[dso_path] = self.Dso()
        if addr not in dso.addrs:
            dso.addrs[addr] = self.Addr(func_addr)

    def convert_addrs_to_lines(self):
        for dso_path in self.dso_map:
            self._convert_addrs_in_one_dso(dso_path, self.dso_map[dso_path])

    def _convert_addrs_in_one_dso(self, dso_path, dso):
        real_path = find_real_dso_path(dso_path, self.binary_cache_path)
        if not real_path:
            if dso_path not in ['//anon', 'unknown', '[kernel.kallsyms]']:
                log_debug("Can't find dso %s" % dso_path)
            return

        if not self._check_debug_line_section(real_path):
            log_debug("file %s doesn't contain .debug_line section." % real_path)
            return

        addr_step = self._get_addr_step(real_path)
        self._collect_line_info(dso, real_path, [0])
        self._collect_line_info(dso, real_path, range(-addr_step, -addr_step * 4 - 1, -addr_step))
        self._collect_line_info(dso, real_path,
                                range(-addr_step * 5, -addr_step * 128 - 1, -addr_step))

    def _check_debug_line_section(self, real_path):
        return '.debug_line' in self.readelf.get_sections(real_path)

    def _get_addr_step(self, real_path):
        arch = self.readelf.get_arch(real_path)
        if arch == 'arm64':
            return 4
        if arch == 'arm':
            return 2
        return 1

    def _collect_line_info(self, dso, real_path, addr_shifts):
        """ Use addr2line to get line info in a dso, with given addr shifts. """
        # 1. Collect addrs to send to addr2line.
        addr_set = set()
        for addr in dso.addrs:
            addr_obj = dso.addrs[addr]
            if addr_obj.source_lines:  # already has source line, no need to search.
                continue
            for shift in addr_shifts:
                # The addr after shift shouldn't change to another function.
                shifted_addr = max(addr + shift, addr_obj.func_addr)
                addr_set.add(shifted_addr)
                if shifted_addr == addr_obj.func_addr:
                    break
        if not addr_set:
            return
        addr_request = '\n'.join(['0x%x' % addr for addr in sorted(addr_set)])

        # 2. Use addr2line to collect line info.
        try:
            subproc = subprocess.Popen(self._build_symbolizer_args(real_path),
                                       stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            (stdoutdata, _) = subproc.communicate(str_to_bytes(addr_request))
            stdoutdata = bytes_to_str(stdoutdata)
        except OSError:
            return
        addr_map = {}
        cur_line_list = None
        need_function_name = self.with_function_name
        cur_function_name = None
        for line in stdoutdata.strip().split('\n'):
            line = line.strip()
            if not line:
                continue
            if line[:2] == '0x':
                # a new address
                cur_line_list = addr_map[int(line, 16)] = []
            elif need_function_name:
                cur_function_name = line.strip()
                need_function_name = False
            else:
                need_function_name = self.with_function_name
                if cur_line_list is None:
                    continue
                file_path, line_number = self._parse_source_location(line)
                if not file_path or not line_number:
                    # An addr can have a list of (file, line), when the addr belongs to an inlined
                    # function. Sometimes only part of the list has ? mark. In this case, we think
                    # the line info is valid if the first line doesn't have ? mark.
                    if not cur_line_list:
                        cur_line_list = None
                    continue
                file_id = self._get_file_id(file_path)
                if self.with_function_name:
                    func_id = self._get_func_id(cur_function_name)
                    cur_line_list.append((file_id, line_number, func_id))
                else:
                    cur_line_list.append((file_id, line_number))

        # 3. Fill line info in dso.addrs.
        for addr in dso.addrs:
            addr_obj = dso.addrs[addr]
            if addr_obj.source_lines:
                continue
            for shift in addr_shifts:
                shifted_addr = max(addr + shift, addr_obj.func_addr)
                lines = addr_map.get(shifted_addr)
                if lines:
                    addr_obj.source_lines = lines
                    break
                if shifted_addr == addr_obj.func_addr:
                    break

    def _build_symbolizer_args(self, binary_path):
        args = [self.symbolizer_path, '-print-address', '-inlining', '-obj=%s' % binary_path]
        if self.with_function_name:
            args += ['-functions=linkage', '-demangle']
        else:
            args.append('-functions=none')
        return args

    def _parse_source_location(self, line):
        file_path, line_number = None, None
        # Handle lines in format filename:line:column, like "runtest/two_functions.cpp:14:25".
        # Filename may contain ':' like "C:\Users\...\file".
        items = line.rsplit(':', 2)
        if len(items) == 3:
            file_path, line_number = items[:2]
        if not file_path or ('?' in file_path) or not line_number or ('?' in line_number):
            return None, None
        try:
            line_number = int(line_number)
        except ValueError:
            return None, None
        return file_path, line_number

    def _get_file_id(self, file_path):
        file_id = self.file_name_to_id.get(file_path)
        if file_id is None:
            file_id = self.file_name_to_id[file_path] = len(self.file_id_to_name)
            self.file_id_to_name.append(file_path)
        return file_id

    def _get_func_id(self, func_name):
        func_id = self.func_name_to_id.get(func_name)
        if func_id is None:
            func_id = self.func_name_to_id[func_name] = len(self.func_id_to_name)
            self.func_id_to_name.append(func_name)
        return func_id

    def get_dso(self, dso_path):
        return self.dso_map.get(dso_path)

    def get_addr_source(self, dso, addr):
        source = dso.addrs[addr].source_lines
        if source is None:
            return None
        if self.with_function_name:
            return [(self.file_id_to_name[file_id], line, self.func_id_to_name[func_id])
                    for (file_id, line, func_id) in source]
        return [(self.file_id_to_name[file_id], line) for (file_id, line) in source]


class SourceFileSearcher(object):
    """ Find source file paths in the file system.
        The file paths reported by addr2line are the paths stored in debug sections
        of shared libraries. And we need to convert them to file paths in the file
        system. It is done in below steps:
        1. Collect all file paths under the provided source_dirs. The suffix of a
           source file should contain one of below:
            h: for C/C++ header files.
            c: for C/C++ source files.
            java: for Java source files.
            kt: for Kotlin source files.
        2. Given an abstract_path reported by addr2line, select the best real path
           as below:
           2.1 Find all real paths with the same file name as the abstract path.
           2.2 Select the real path having the longest common suffix with the abstract path.
    """

    SOURCE_FILE_EXTS = {'.h', '.hh', '.H', '.hxx', '.hpp', '.h++',
                        '.c', '.cc', '.C', '.cxx', '.cpp', '.c++',
                        '.java', '.kt'}

    @classmethod
    def is_source_filename(cls, filename):
        ext = os.path.splitext(filename)[1]
        return ext in cls.SOURCE_FILE_EXTS

    def __init__(self, source_dirs):
        # Map from filename to a list of reversed directory path containing filename.
        self.filename_to_rparents = {}
        self._collect_paths(source_dirs)

    def _collect_paths(self, source_dirs):
        for source_dir in source_dirs:
            for parent, _, file_names in os.walk(source_dir):
                rparent = None
                for file_name in file_names:
                    if self.is_source_filename(file_name):
                        rparents = self.filename_to_rparents.get(file_name)
                        if rparents is None:
                            rparents = self.filename_to_rparents[file_name] = []
                        if rparent is None:
                            rparent = parent[::-1]
                        rparents.append(rparent)

    def get_real_path(self, abstract_path):
        abstract_path = abstract_path.replace('/', os.sep)
        abstract_parent, file_name = os.path.split(abstract_path)
        abstract_rparent = abstract_parent[::-1]
        real_rparents = self.filename_to_rparents.get(file_name)
        if real_rparents is None:
            return None
        best_matched_rparent = None
        best_common_length = -1
        for real_rparent in real_rparents:
            length = len(os.path.commonprefix((real_rparent, abstract_rparent)))
            if length > best_common_length:
                best_common_length = length
                best_matched_rparent = real_rparent
        if best_matched_rparent is None:
            return None
        return os.path.join(best_matched_rparent[::-1], file_name)


class Objdump(object):
    """ A wrapper of objdump to disassemble code. """
    def __init__(self, ndk_path, binary_cache_path):
        self.ndk_path = ndk_path
        self.binary_cache_path = binary_cache_path
        self.readelf = ReadElf(ndk_path)
        self.objdump_paths = {}

    def get_dso_info(self, dso_path):
        real_path = find_real_dso_path(dso_path, self.binary_cache_path)
        if not real_path:
            return None
        arch = self.readelf.get_arch(real_path)
        if arch == 'unknown':
            return None
        return (real_path, arch)

    def disassemble_code(self, dso_info, start_addr, addr_len):
        """ Disassemble [start_addr, start_addr + addr_len] of dso_path.
            Return a list of pair (disassemble_code_line, addr).
        """
        real_path, arch = dso_info
        objdump_path = self.objdump_paths.get(arch)
        if not objdump_path:
            objdump_path = find_tool_path('objdump', self.ndk_path, arch)
            if not objdump_path:
                log_exit("Can't find objdump. Please set ndk path with --ndk_path option.")
            self.objdump_paths[arch] = objdump_path

        # 3. Run objdump.
        args = [objdump_path, '-dlC', '--no-show-raw-insn',
                '--start-address=0x%x' % start_addr,
                '--stop-address=0x%x' % (start_addr + addr_len),
                real_path]
        try:
            subproc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            (stdoutdata, _) = subproc.communicate()
            stdoutdata = bytes_to_str(stdoutdata)
        except OSError:
            return None

        if not stdoutdata:
            return None
        result = []
        for line in stdoutdata.split('\n'):
            line = line.rstrip()  # Remove '\r' on Windows.
            items = line.split(':', 1)
            try:
                addr = int(items[0], 16)
            except ValueError:
                addr = 0
            result.append((line, addr))
        return result


class ReadElf(object):
    """ A wrapper of readelf. """
    def __init__(self, ndk_path):
        self.readelf_path = find_tool_path('readelf', ndk_path)
        if not self.readelf_path:
            log_exit("Can't find readelf. Please set ndk path with --ndk_path option.")

    def get_arch(self, elf_file_path):
        """ Get arch of an elf file. """
        if is_elf_file(elf_file_path):
            try:
                output = subprocess.check_output([self.readelf_path, '-h', elf_file_path])
                output = bytes_to_str(output)
                if output.find('AArch64') != -1:
                    return 'arm64'
                if output.find('ARM') != -1:
                    return 'arm'
                if output.find('X86-64') != -1:
                    return 'x86_64'
                if output.find('80386') != -1:
                    return 'x86'
            except subprocess.CalledProcessError:
                pass
        return 'unknown'

    def get_build_id(self, elf_file_path, with_padding=True):
        """ Get build id of an elf file. """
        if is_elf_file(elf_file_path):
            try:
                output = subprocess.check_output([self.readelf_path, '-n', elf_file_path])
                output = bytes_to_str(output)
                result = re.search(r'Build ID:\s*(\S+)', output)
                if result:
                    build_id = result.group(1)
                    if with_padding:
                        build_id = self.pad_build_id(build_id)
                    return build_id
            except subprocess.CalledProcessError:
                pass
        return ""

    @staticmethod
    def pad_build_id(build_id):
        """ Pad build id to 40 hex numbers (20 bytes). """
        if len(build_id) < 40:
            build_id += '0' * (40 - len(build_id))
        else:
            build_id = build_id[:40]
        return '0x' + build_id

    def get_sections(self, elf_file_path):
        """ Get sections of an elf file. """
        section_names = []
        if is_elf_file(elf_file_path):
            try:
                output = subprocess.check_output([self.readelf_path, '-SW', elf_file_path])
                output = bytes_to_str(output)
                for line in output.split('\n'):
                    # Parse line like:" [ 1] .note.android.ident NOTE  0000000000400190 ...".
                    result = re.search(r'^\s+\[\s*\d+\]\s(.+?)\s', line)
                    if result:
                        section_name = result.group(1).strip()
                        if section_name:
                            section_names.append(section_name)
            except subprocess.CalledProcessError:
                pass
        return section_names

def extant_dir(arg):
    """ArgumentParser type that only accepts extant directories.

    Args:
        arg: The string argument given on the command line.
    Returns: The argument as a realpath.
    Raises:
        argparse.ArgumentTypeError: The given path isn't a directory.
    """
    path = os.path.realpath(arg)
    if not os.path.isdir(path):
        raise argparse.ArgumentTypeError('{} is not a directory.'.format(path))
    return path

def extant_file(arg):
    """ArgumentParser type that only accepts extant files.

    Args:
        arg: The string argument given on the command line.
    Returns: The argument as a realpath.
    Raises:
        argparse.ArgumentTypeError: The given path isn't a file.
    """
    path = os.path.realpath(arg)
    if not os.path.isfile(path):
        raise argparse.ArgumentTypeError('{} is not a file.'.format(path))
    return path

logging.getLogger().setLevel(logging.DEBUG)
