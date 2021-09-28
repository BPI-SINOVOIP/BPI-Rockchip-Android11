#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
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
"""This file contains unit tests for vtable_dumper."""

import os
import unittest

from vts.utils.python.library.vtable import vtable_dumper

_VTABLES = [
    ('_ZTV11VirtualBase',
     [(0x8, ['_ZTI11VirtualBase']),
      (0x10, ['__cxa_pure_virtual']),
      (0x18, ['__cxa_pure_virtual']),
      (0x20, ['__cxa_pure_virtual']),
      (0x28, ['_ZN11VirtualBase3barEv']),
      (0x30, ['_ZN11VirtualBase3bazEv'])]),
    ('_ZTV14VirtualDerived',
     [(0x8, ['_ZTI14VirtualDerived']),
      (0x10, ['_ZN14VirtualDerivedD2Ev']),
      (0x18, ['_ZN14VirtualDerivedD0Ev']),
      (0x20, ['_ZN14VirtualDerived3fooEv']),
      (0x28, ['_ZN11VirtualBase3barEv']),
      (0x30, ['__cxa_pure_virtual'])]),
    ('_ZTV15ConcreteDerived',
     [(0x8, ['_ZTI15ConcreteDerived']),
      (0x10, ['_ZN15ConcreteDerivedD2Ev']),
      (0x18, ['_ZN15ConcreteDerivedD0Ev']),
      (0x20, ['_ZN14VirtualDerived3fooEv']),
      (0x28, ['_ZN15ConcreteDerived3barEv']),
      (0x30, ['_ZN15ConcreteDerived3bazEv'])]),
]


class VtableDumperTest(unittest.TestCase):
    """Unit tests for VtableDumper from vtable_dumper."""

    def setUp(self):
        """Creates a VtableDumper."""
        dir_path = os.path.dirname(os.path.realpath(__file__))
        self.elf_file_path = os.path.join(dir_path, 'testing', 'libtest.so')
        self.dumper = vtable_dumper.VtableDumper(self.elf_file_path)

    def tearDown(self):
        """Closes the VtableDumper."""
        self.dumper.Close()

    def testDumpVtables(self):
        """Tests that DumpVtables dumps vtable structure correctly."""
        vtables_dump = []
        for vtable in self.dumper.DumpVtables():
            entries = [(entry.offset, entry.names) for entry in vtable.entries]
            vtables_dump.append((vtable.name, entries))
        vtables_dump.sort()
        self.assertEqual(vtables_dump, _VTABLES)


if __name__ == '__main__':
    unittest.main()
