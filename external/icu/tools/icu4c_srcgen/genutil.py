# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#            http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""Utility for ICU4C code generation"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import logging
import os
import site
import sys
import textwrap
from collections import deque

THIS_DIR = os.path.dirname(os.path.realpath(__file__))
ANDROID_TOP = os.path.realpath(os.path.join(THIS_DIR, '../../../..'))


def android_path(*args):
    """Returns the absolute path to a directory within the Android tree."""
    return os.path.join(ANDROID_TOP, *args)


# TODO: Include clang bindings in prebuilt package. http://b/119270767
site.addsitedir(android_path('external/clang/bindings/python'))
import clang.cindex  # pylint: disable=import-error,wrong-import-position

# TODO: Do not hardcode clang version. http://b/119270767
CLANG_REVISION = 'r365631c'
CLANG_LIB_VERSION = '9svn'
CLANG_HEADER_VERSION = '9.0.8'
CLANG_PATH = android_path('prebuilts/clang/host/linux-x86/clang-%s' % CLANG_REVISION)


class Function(object):
    """A visible function found in an ICU header."""

    def __init__(self, name, result_type, params, is_variadic, module):
        self.name = name
        self.result_type = result_type
        self.params = params
        self.is_variadic = is_variadic
        self.va_list_insert_position = -1

        # callee will be used in dlsym and may be identical to others for
        # functions with variable argument lists.
        self.callee = self.name
        if self.is_variadic:
            self.last_param = self.params[-1][1]
        self.handle = 'handle_' + module
        self.return_void = self.result_type == 'void'

    @property
    def param_str(self):
        """Returns a string usable as a parameter list in a function decl."""
        params = []
        for param_type, param_name in self.params:
            if '[' in param_type:
                # `int foo[42]` will be a param_type of `int [42]` and a
                # param_name of `foo`. We need to put these back in the right
                # order.
                param_name += param_type[param_type.find('['):]
                param_type = param_type[:param_type.find('[')]
            params.append('{} {}'.format(param_type, param_name))
        if self.is_variadic:
            params.append('...')
        return ', '.join(params)

    @property
    def arg_str(self):
        """Returns a string usable as an argument list in a function call."""
        args = []
        for _, param_name in self.params:
            args.append(param_name)
        if self.is_variadic:
            if self.va_list_insert_position >= 0:
                args.insert(self.va_list_insert_position, 'args')
            else:
                raise ValueError(textwrap.dedent("""\
                {}({}) is variadic, but has no valid \
                inserted position""".format(
                    self.name,
                    self.param_str)))
        return ', '.join(args)

    def set_variadic_callee(self, callee, inserted_position):
        """Set variadic callee with callee name and inserted position"""
        if self.is_variadic:
            self.callee = callee
            self.va_list_insert_position = inserted_position


def logger():
    """Returns the module level logger."""
    return logging.getLogger(__name__)


class DeclaredFunctionsParser(object):
    """Parser to get declared functions from ICU4C headers. """

    def __init__(self, decl_filters, whitelisted_decl_filter):
        """
        Args:
            decl_filters: A list of filters for declared functions.
            whitelisted_decl_filter: A list of whitelisting filters for declared functions.
            If the function is whitelisted here, the function will not filtered by the filter added
            in decl_filters
        """
        self.decl_filters = decl_filters
        self.whitelisted_decl_filters = whitelisted_decl_filter
        self.va_functions_mapping = {}

        # properties to store the parsing result
        self.all_headers = []
        self.all_header_paths_to_copy = set()
        self.all_declared_functions = []
        self.seen_functions = set()

        # Configures libclang to load in our environment
        # Set up LD_LIBRARY_PATH to include libclang.so, libLLVM.so, etc.  Note
        # that setting LD_LIBRARY_PATH with os.putenv() sometimes doesn't help.
        # clang.cindex.Config.set_library_path(os.path.join(CLANG_PATH, 'lib64'))
        clang.cindex.Config.set_library_file(
            os.path.join(CLANG_PATH, 'lib64', 'libclang.so.%s' % CLANG_LIB_VERSION))

    def set_va_functions_mapping(self, mapping):
        """Set mapping from a variable argument function to an implementation.

        Functions w/ variable argument lists (...) need special care to call
        their corresponding v- versions that accept a va_list argument. Note that
        although '...' will always appear as the last parameter, its v- version
        may put the va_list arg in a different place. Hence we provide an index
        to indicate the position.
        e.g. 'umsg_format': ('umsg_vformat', 3) means in the wrapper function of
        'umsg_format', it will call 'umsg_vformat' instead, with the va_list arg
        inserted as the 3rd argument."""
        self.va_functions_mapping = mapping

    @property
    def header_includes(self):
        """Return all headers declaring the functions returned in get_all_declared_functions.

        If all functions in the header are filtered, the header is not included in here."""
        return [self.short_header_path(header) for header in self.all_headers]

    @property
    def header_paths_to_copy(self):
        """Return all headers needed to be copied"""
        return self.all_header_paths_to_copy

    @property
    def declared_functions(self):
        """Return all declared functions after filtering"""
        return self.all_declared_functions

    def get_cflags(self):
        """Returns the cflags that should be used for parsing."""
        clang_flags = [
            '-x',
            'c',
            '-std=c99',
            '-DU_DISABLE_RENAMING=1',
            '-DU_SHOW_CPLUSPLUS_API=0',
            '-DU_HIDE_DRAFT_API',
            '-DU_HIDE_DEPRECATED_API',
            '-DU_HIDE_INTERNAL_API',
            '-DANDROID_LINK_SHARED_ICU4C',
        ]

        include_dirs = [
            # TODO: Do not hardcode clang version. http://b/119270767
            os.path.join(CLANG_PATH, 'lib64/clang/', CLANG_HEADER_VERSION, 'include/'),
            android_path('bionic/libc/include'),
            android_path('external/icu/android_icu4c/include'),
            android_path('external/icu/icu4c/source/common'),
            android_path('external/icu/icu4c/source/i18n'),
        ]

        for include_dir in include_dirs:
            clang_flags.append('-I' + include_dir)
        return clang_flags

    def get_all_cpp_headers(self):
        """Return all C++ header names in icu4c/source/test/hdrtst/cxxfiles.txt"""
        cpp_headers = []
        with open(android_path('external/icu/tools/icu4c_srcgen/cxxfiles.txt'),
                  'r') as f:
            for line in f:
                line = line.strip()
                if not line.startswith("#"):
                    cpp_headers.append(line)
        return cpp_headers

    def parse(self):
        """Parse the headers and collect the declared functions after filtering
        and the headers containing the functions."""
        index = clang.cindex.Index.create()

        icu_modules = (
            'common',
            'i18n',
        )
        header_dependencies = {}
        for module in icu_modules:
            path = android_path(android_path('external/icu/icu4c/source', module, 'unicode'))
            files = [os.path.join(path, f)
                     for f in os.listdir(path) if f.endswith('.h')]

            for file_path in files:
                # Ignore C++ headers.
                if os.path.basename(file_path) in self.get_all_cpp_headers():
                    continue

                tunit = index.parse(file_path, self.get_cflags())
                self.handle_diagnostics(tunit)
                header_dependencies[file_path] = [file_inclusion.include.name for file_inclusion
                                                  in tunit.get_includes()]
                visible_functions = self.get_visible_functions(
                    tunit.cursor, module, file_path)
                for function in visible_functions:
                    self.seen_functions.add(function.name)
                    self.all_declared_functions.append(function)
                if visible_functions:
                    self.all_headers.append(file_path)

        # Sort to produce an deterministic output
        self.all_declared_functions = sorted(self.all_declared_functions, key=lambda f: f.name)
        self.all_headers = sorted(self.all_headers)

        # Build the headers required for using your restricted API set, and put the set into
        # all_header_files_to_copy.
        # header_dependencies is a map from icu4c header file path to a list of included headers.
        # The key must be a ICU4C header, but the value could contain non-ICU4C headers, e.g.
        # {
        #   ".../icu4c/source/common/unicode/utype.h": [
        #      ".../icu4c/source/common/unicode/uversion.h",
        #      ".../bionic/libc/include/ctype.h",
        #    ],
        #    ...
        # }
        file_queue = deque()
        file_processed = set()
        for header in self.all_headers:
            file_queue.appendleft(header)
            self.all_header_paths_to_copy.add(header)
        while file_queue:
            f = file_queue.pop()
            if f in file_processed:
                continue
            file_processed.add(f)
            for header in header_dependencies[f]:
                if header in header_dependencies:  # Do not care non-icu4c headers
                    self.all_header_paths_to_copy.add(header)
                    file_queue.appendleft(header)

    def handle_diagnostics(self, tunit):
        """Prints compiler diagnostics to stdout. Exits if errors occurred."""
        errors = 0
        for diag in tunit.diagnostics:
            if diag.severity == clang.cindex.Diagnostic.Fatal:
                level = logging.CRITICAL
                errors += 1
            elif diag.severity == clang.cindex.Diagnostic.Error:
                level = logging.ERROR
                errors += 1
            elif diag.severity == clang.cindex.Diagnostic.Warning:
                level = logging.WARNING
            elif diag.severity == clang.cindex.Diagnostic.Note:
                level = logging.INFO
            logger().log(
                level, '%s:%s:%s %s', diag.location.file, diag.location.line,
                diag.location.column, diag.spelling)
        if errors:
            sys.exit('Errors occurred during parsing. Exiting.')

    def get_visible_functions(self, cursor, module, file_name):
        """Returns a list of all visible functions in a header file."""
        functions = []
        for child in cursor.get_children():
            if self.should_process_decl(child, file_name):
                functions.append(self.from_cursor(child, module))
        return functions

    def should_process_decl(self, decl, file_name):
        """Returns True if this function needs to be processed."""
        if decl.kind != clang.cindex.CursorKind.FUNCTION_DECL:
            return False
        if decl.location.file.name != file_name:
            return False
        if decl.spelling in self.seen_functions:
            return False
        if not self.is_function_visible(decl):
            return False
        for whitlisted_decl_filter in self.whitelisted_decl_filters:
            if whitlisted_decl_filter(decl):
                return True
        for decl_filter in self.decl_filters:
            if not decl_filter(decl):
                return False
        return True

    def is_function_visible(self, decl):
        """Returns True if the function has default visibility."""
        visible = False
        vis_attrs = self.get_children_by_kind(
            decl, clang.cindex.CursorKind.VISIBILITY_ATTR)
        for child in vis_attrs:
            visible = child.spelling == 'default'
        return visible

    def get_children_by_kind(self, cursor, kind):
        """Returns a generator of cursor's children of a specific kind."""
        for child in cursor.get_children():
            if child.kind == kind:
                yield child

    def short_header_path(self, name):
        """Trim the given file name to 'unicode/xyz.h'."""
        return name[name.rfind('unicode/'):]

    def from_cursor(self, cursor, module):
        """Creates a Function object from the decl at the cursor."""
        if cursor.type.kind != clang.cindex.TypeKind.FUNCTIONPROTO:
            raise ValueError(textwrap.dedent("""\
                {}'s type kind is {}, expected TypeKind.FUNCTIONPROTO.
                {} Line {} Column {}""".format(
                    cursor.spelling,
                    cursor.type.kind,
                    cursor.location.file,
                    cursor.location.line,
                    cursor.location.column)))

        name = cursor.spelling
        result_type = cursor.result_type.spelling
        is_variadic = cursor.type.is_function_variadic()
        params = []
        for arg in cursor.get_arguments():
            params.append((arg.type.spelling, arg.spelling))
        function = Function(name, result_type, params, is_variadic, module)
        # For variadic function, set the callee and va_list position
        if function.is_variadic and function.name in self.va_functions_mapping:
            m = self.va_functions_mapping[function.name]
            function.set_variadic_callee(m[0], m[1])
        return function


class StableDeclarationFilter(object):
    """Return true if it's @stable API"""
    def __call__(self, decl):
        """Returns True if the given decl has a doxygen stable tag."""
        if not decl.raw_comment:
            return False
        if '@stable' in decl.raw_comment:
            return True
        return False


class WhitelistedDeclarationFilter(object):
    """A filter for whitelisting function declarations."""
    def __init__(self, whitelisted_function_names):
        self.whitelisted_function_names = whitelisted_function_names

    def __call__(self, decl):
        """Returns True if the given decl is whitelisted"""
        return decl.spelling in self.whitelisted_function_names


class BlacklistedlistedDeclarationFilter(object):
    """A filter for blacklisting function declarations."""
    def __init__(self, blacklisted_function_names):
        self.blacklisted_function_names = blacklisted_function_names

    def __call__(self, decl):
        """Returns True if the given decl is nor blacklisted"""
        return decl.spelling not in self.blacklisted_function_names


# Functions w/ variable argument lists (...) need special care to call
# their corresponding v- versions that accept a va_list argument. Note that
# although '...' will always appear as the last parameter, its v- version
# may put the va_list arg in a different place. Hence we provide an index
# to indicate the position.
#
# e.g. 'umsg_format': ('umsg_vformat', 3) means in the wrapper function of
# 'umsg_format', it will call 'umsg_vformat' instead, with the va_list arg
# inserted as the 3rd argument.

# We need to insert the va_list (named args) at the position
# indicated by the KNOWN_VA_FUNCTIONS map.
KNOWN_VA_FUNCTIONS = {
    'u_formatMessage': ('u_vformatMessage', 5),
    'u_parseMessage': ('u_vparseMessage', 5),
    'u_formatMessageWithError': ('u_vformatMessageWithError', 6),
    'u_parseMessageWithError': ('u_vparseMessageWithError', 5),
    'umsg_format': ('umsg_vformat', 3),
    'umsg_parse': ('umsg_vparse', 4),
    'utrace_format': ('utrace_vformat', 4),
}

# The following functions are not @stable
WHITELISTED_FUNCTION_NAMES = (
    # Not intended to be called directly, but are used by @stable macros.
    'utf8_nextCharSafeBody',
    'utf8_appendCharSafeBody',
    'utf8_prevCharSafeBody',
    'utf8_back1SafeBody',
)
