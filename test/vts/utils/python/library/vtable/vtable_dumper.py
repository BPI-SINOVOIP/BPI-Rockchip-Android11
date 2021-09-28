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
r"""This file contains an ELF vtable abi dumper.

Example usage:
    from vts.utils.python.library import vtable_dumper
    with vtable_dumper.VtableDumper(file) as dumper:
        print('\n\n'.join(str(vtable) for vtable in dumper.DumpVtables()))
"""

import bisect

from vts.utils.python.library import elf_parser
from vts.utils.python.library.elf import consts


class VtableError(Exception):
    """The exception raised by VtableDumper."""
    pass


class VtableEntry(object):
    """This class contains an entry in Vtable.

    The names attribute constains all the possible symbol names for this entry
    due to symbol aliasing.

    Attributes:
        offset: Offset with respect to vtable.
        names: A list of possible symbol names of the entry.
        value: Value of the entry.
        is_undefined: If entry has a symbol, whether symbol is undefined or not.
    """

    def __init__(self, offset, names, value, is_undefined):
        self.offset = offset
        self.names = names
        self.value = value
        self.is_undefined = is_undefined

    def __lt__(self, other):
        return self.offset < other.offset


class Vtable(object):
    """This class contains a vtable and its entries.

    Attributes:
        name: Symbol name of vtable.
        begin_addr: Begin address of vtable.
        end_addr: End Address of vtable.
        entries: A list of VtableEntry.
    """

    def __init__(self, name, begin_addr, end_addr):
        self.name = name
        self.begin_addr = begin_addr
        self.end_addr = end_addr
        self.entries = []

    def __lt__(self, other):
        if isinstance(other, Vtable):
            key = other.begin_addr
        else:
            key = other
        return self.begin_addr < key

    def __str__(self):
        msg = ('vtable {} {} entries begin_addr={:#x} size={:#x}'
               .format(self.name,
                       len(self.entries),
                       self.begin_addr,
                       self.end_addr - self.begin_addr))
        for entry in self.entries:
            msg += ('\n{:#x} {} {:#x} {}'
                    .format(entry.offset,
                            entry.is_undefined,
                            entry.value,
                            entry.names))
        return msg


class VtableDumper(elf_parser.ElfParser):
    """This class wraps around a ElfParser and dumps vtables from an ELF file.
    """

    def __init__(self, file_path, begin_offset=0):
        """Creates a VtableDumper to open and dump an ELF file's vtable.

        Args:
            file_path: The path to the file.
            begin_offset: The offset of the ELF object in the file.

        Raises:
            ElfError: File is not a valid ELF.
        """
        super(VtableDumper, self).__init__(file_path, begin_offset)

    def DumpVtables(self):
        """Scans the relocation section and dump exported vtables.

        Returns:
            A list of Vtable.

        Raises:
            VtableError: Fails to dump vtable.
            ElfError: ELF decoding fails.
        """
        # Determine absolute and relative relocation type from e_machine.
        machine = self.Ehdr.e_machine
        rel_type = {
            consts.EM_ARM: (consts.R_ARM_ABS32, consts.R_ARM_RELATIVE),
            consts.EM_AARCH64: (consts.R_AARCH64_ABS64, consts.R_AARCH64_RELATIVE),
            consts.EM_386: (consts.R_386_32, consts.R_386_RELATIVE),
            consts.EM_X86_64: (consts.R_X86_64_64, consts.R_X86_64_RELATIVE),
        }
        if machine in rel_type:
            rel_abs_type, rel_relative_type = rel_type[machine]
        else:
            raise VtableError('Unexpected machine type: {}'.format(machine))
        # Initialize vtable ranges.
        vtables = self._PrepareVtables()
        inv_table = self._FunctionSymbolInverseTable()
        # Scan relocation sections.
        for rel_sh in self._RelocationSections():
            is_rela = rel_sh.sh_type in (consts.SHT_RELA,
                                         consts.SHT_ANDROID_RELA)
            is_relr = rel_sh.sh_type in (consts.SHT_RELR,
                                         consts.SHT_ANDROID_RELR)
            symtab = self.Shdr[rel_sh.sh_link]
            strtab = self.Shdr[symtab.sh_link]
            for reloc in self.GetRelocations(rel_sh):
                # RELR is relative and has no type.
                is_absolute_type = (not is_relr and
                                    reloc.GetType() == rel_abs_type)
                is_relative_type = (is_relr or
                                    reloc.GetType() == rel_relative_type)
                if not is_absolute_type and not is_relative_type:
                    continue
                # If relocation target is a vtable entry, find the vtable.
                vtable = self._LocateVtable(vtables, reloc.r_offset)
                if not vtable:
                    continue
                # *_RELA sections have explicit addend.
                # *_REL and *_RELR sections have implicit addend.
                if is_rela:
                    addend = reloc.r_addend
                else:
                    addend = self._ReadRelocationAddend(reloc)
                if is_absolute_type:
                    # Absolute relocations uses symbol value + addend.
                    sym = self.GetRelocationSymbol(symtab, reloc)
                    reloc_value = sym.st_value + addend
                    sym_is_undefined = (sym.st_shndx == consts.SHN_UNDEF)
                    if reloc_value in inv_table:
                        entry_names = inv_table[reloc_value]
                    else:
                        sym_name = self.GetString(strtab, sym.st_name)
                        entry_names = [sym_name]
                elif is_relative_type:
                    # Relative relocations don't have symbol table entry,
                    # instead it uses a vaddr offset which is stored
                    # in the addend value.
                    reloc_value = addend
                    sym_is_undefined = False
                    if reloc_value in inv_table:
                        entry_names = inv_table[reloc_value]
                    else:
                        entry_names = []
                vtable.entries.append(VtableEntry(
                    reloc.r_offset - vtable.begin_addr,
                    entry_names, reloc_value, sym_is_undefined))
        # Sort the vtable entries.
        for vtable in vtables:
            vtable.entries.sort()
        return vtables

    def _PrepareVtables(self):
        """Collects vtable symbols from symbol table / dynamic symbol table.

        Returns:
            A list of Vtable.

        Raises:
            ElfError: ELF decoding fails.
        """
        vtables = []
        vtable_names = set()
        symtab_names = ('.symtab', '.dynsym')
        for symtab_name in symtab_names:
            # Object files may have one section of each type
            symtab = self.GetSectionByName(symtab_name)
            if not symtab:
                continue
            strtab = self.Shdr[symtab.sh_link]
            for sym in self.GetSymbols(symtab):
                if sym.st_shndx == consts.SHN_UNDEF:
                    continue
                sym_name = self.GetString(strtab, sym.st_name)
                if sym_name.startswith('_ZTV') and sym_name not in vtable_names:
                    vtable_begin = sym.st_value
                    vtable_end = sym.st_value + sym.st_size
                    vtable = Vtable(sym_name, vtable_begin, vtable_end)
                    vtables.append(vtable)
                    vtable_names.add(sym_name)
        # Sort the vtables with Vtable.begin_addr so that we can use binary
        # search to speed up _LocateVtable()'s query.
        vtables.sort()
        return vtables

    def _FunctionSymbolInverseTable(self):
        """Returns an address to symbol name inverse lookup table.

        For symbols in .symtab and .dynsym that are not undefined,
        construct an address to symbol name lookup table.

        Returns:
            A dictionary of {address: [symbol names]}.

        Raises:
            ElfError: ELF decoding fails.
        """
        inv_table = dict()
        symtab_names = ('.symtab', '.dynsym')
        for symtab_name in symtab_names:
            # Object files may have one section of each type
            symtab = self.GetSectionByName(symtab_name)
            if not symtab:
                continue
            strtab = self.Shdr[symtab.sh_link]
            for sym in self.GetSymbols(symtab):
                if (sym.GetType() in (consts.STT_OBJECT, consts.STT_FUNC)
                        and sym.st_shndx != consts.SHN_UNDEF):
                    sym_name = self.GetString(strtab, sym.st_name)
                    if sym.st_value in inv_table:
                        inv_table[sym.st_value].append(sym_name)
                    else:
                        inv_table[sym.st_value] = [sym_name]
        for key in inv_table:
            inv_table[key] = sorted(set(inv_table[key]))
        return inv_table

    def _LocateVtable(self, vtables, offset):
        """Searches for the vtable that contains the offset.

        Args:
            vtables: A list of Vtable to search from.
            offset: The offset value to search for.

        Returns:
            The vtable whose begin_addr <= offset and offset < end_addr.
            None if no such vtable cound be found.
        """
        search_key = Vtable("", offset, offset)
        idx = bisect.bisect(vtables, search_key)
        if idx <= 0:
            return None
        vtable = vtables[idx-1]
        if vtable.begin_addr <= offset and offset < vtable.end_addr:
            return vtable
        return None

    def _ReadRelocationAddend(self, reloc):
        """Reads the addend value from the location to be modified.

        Args:
            reloc: A Elf_Rel containing the relocation.

        Returns:
            An integer, the addend value.

        Raises:
            VtableError: reloc is not a valid relocation.
            ElfError: ELF decoding fails.
        """
        for sh in self.Shdr:
            sh_begin = sh.sh_addr
            sh_end = sh.sh_addr + sh.sh_size
            if sh_begin <= reloc.r_offset and reloc.r_offset < sh_end:
                if sh.sh_type == consts.SHT_NOBITS:
                    return 0
                offset = reloc.r_offset - sh.sh_addr + sh.sh_offset
                addend = self._SeekReadStruct(offset, self.Elf_Addr)
                return addend.value
        raise VtableError('Invalid relocation: '
                          'Cannot find relocation target section '
                          'r_offset = {:#x}, r_info = {:#x}'
                          .format(reloc.r_offset, reloc.r_info))

    def _RelocationSections(self):
        """Yields section headers that contain relocation data."""
        sh_rel_types = (consts.SHT_REL, consts.SHT_RELA, consts.SHT_RELR,
                        consts.SHT_ANDROID_REL, consts.SHT_ANDROID_RELA,
                        consts.SHT_ANDROID_RELR)
        for sh in self.Shdr:
            if sh.sh_type in sh_rel_types:
                yield sh
