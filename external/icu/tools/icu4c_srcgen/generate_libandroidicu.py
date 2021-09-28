#!/usr/bin/env python
#
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
"""Generate ICU stable C API wrapper source.

This script parses all the header files specified by the ICU module names. For
each function marked as ICU stable, it generates a wrapper function to be
called by NDK, which in turn calls the available function at runtime. The tool
relies on libclang to parse header files, which is a component provided by
Clang.

Reference to ICU4C stable C APIs:
http://icu-project.org/apiref/icu4c/files.html
"""
from __future__ import absolute_import
from __future__ import print_function

import logging
import os
import shutil

import jinja2

from genutil import (
    android_path,
    DeclaredFunctionsParser,
    StableDeclarationFilter,
    WhitelistedDeclarationFilter,
    KNOWN_VA_FUNCTIONS,
    WHITELISTED_FUNCTION_NAMES,
)

THIS_DIR = os.path.dirname(os.path.realpath(__file__))

JINJA_ENV = jinja2.Environment(loader=jinja2.FileSystemLoader(
    os.path.join(THIS_DIR, 'jinja_templates')))
JINJA_ENV.trim_blocks = True
JINJA_ENV.lstrip_blocks = True


def generate_shim(functions, includes):
    """Generates the library source file from the given functions."""
    data = {
        'functions': functions,
        'icu_headers': includes,
    }
    return JINJA_ENV.get_template('shim.cpp.j2').render(data)


def generate_symbol_txt(shim_functions, extra_function_names):
    """Generates the symbol txt file from the given functions."""
    data = {
        # Each shim_function is given an _android suffix.
        'shim_functions' : shim_functions,
        # Each extra function name is included as given.
        'extra_function_names': extra_function_names,
    }
    return JINJA_ENV.get_template('libandroidicu.map.txt.j2').render(data)


def main():
    """Parse the ICU4C headers and generate the shim libandroidicu."""
    logging.basicConfig(level=logging.DEBUG)

    decl_filters = [StableDeclarationFilter()]
    whitelisted_filters = [WhitelistedDeclarationFilter(WHITELISTED_FUNCTION_NAMES)]
    parser = DeclaredFunctionsParser(decl_filters, whitelisted_filters)
    parser.set_va_functions_mapping(KNOWN_VA_FUNCTIONS)

    parser.parse()

    includes = parser.header_includes
    functions = parser.declared_functions

    headers_folder = android_path('external/icu/libandroidicu/include/unicode')
    if os.path.exists(headers_folder):
        shutil.rmtree(headers_folder)
    os.mkdir(headers_folder)

    with open(android_path('external/icu/libandroidicu/static_shim/shim.cpp'),
              'w') as out_file:
        out_file.write(generate_shim(functions, includes).encode('utf8'))

    with open(android_path('external/icu/libandroidicu/aicu/extra_function_names.txt'),
              'r') as in_file:
        extra_function_names = [
                line.strip() for line in in_file.readlines() if not line.startswith('#')
        ]

    with open(android_path('external/icu/libandroidicu/libandroidicu.map.txt'),
              'w') as out_file:
        out_file.write(generate_symbol_txt(functions, extra_function_names).encode('utf8'))

    for path in parser.header_paths_to_copy:
        basename = os.path.basename(path)
        shutil.copyfile(path, os.path.join(headers_folder, basename))


if __name__ == '__main__':
    main()
