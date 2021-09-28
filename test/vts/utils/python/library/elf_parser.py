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
r"""This file contains an ELF parser and ELF header structures.

Example usage:
    import elf_parser
    with elf_parser.ElfParser(file) as e:
        print('\n'.join(e.ListGlobalDynamicSymbols()))
        print('\n'.join(e.ListDependencies()[0]))
"""

import ctypes
import os
import struct

from vts.utils.python.library.elf import consts
from vts.utils.python.library.elf import structs
from vts.utils.python.library.elf import utils


class ElfError(Exception):
    """The exception raised by ElfParser."""
    pass


class ElfParser(object):
    """The class reads information from an ELF file.

    Attributes:
        _file: The ELF file object.
        _begin_offset: The offset of the ELF object in the file. The value is
                       non-zero if the ELF is in an archive, such as .a file.
        _file_size: Size of the file.
        bitness: Bitness of the ELF.
        Ehdr: An Elf_Endr, the ELF header structure of the file.
        Shdr: A list of Elf_Shdr, the section headers of the file.
        Elf_Addr: ELF unsigned program address type.
        Elf_Off: ELF unsigned file offset type.
        Elf_Half: ELF unsigned medium integer type.
        Elf_Word: ELF unsigned integer type.
        Elf_Sword: ELF signed integer type.
        Elf_Ehdr: ELF header class.
        Elf_Shdr: ELF section header class.
        Elf_Dyn: ELF dynamic entry class.
        Elf_Sym: ELF symbol entry class.
        Elf_Rel: ELF relocation entry class.
        Elf_Rela: ELF relocation entry class with explicit addend.
        Elf_Phdr: ELF program header class.
        Elf_Nhdr: ELF note header class.
    """

    def __init__(self, file_path, begin_offset=0):
        """Creates a parser to open and read an ELF file.

        Args:
            file_path: The path to the file.
            begin_offset: The offset of the ELF object in the file.

        Raises:
            ElfError: File is not a valid ELF.
        """
        self._begin_offset = begin_offset
        try:
            self._file = open(file_path, 'rb')
        except IOError as e:
            raise ElfError(e)
        try:
            self._file_size = os.fstat(self._file.fileno()).st_size
        except OSError as e:
            self.Close()
            raise ElfError(e)

        try:
            e_ident = self._SeekRead(0, consts.EI_NIDENT)

            if e_ident[:4] != consts.ELF_MAGIC_NUMBER:
                raise ElfError('Unexpected magic bytes: {}'.format(e_ident[:4]))

            if utils.ByteToInt(e_ident[consts.EI_CLASS]) not in (
                    consts.ELFCLASS32, consts.ELFCLASS64):
                raise ElfError('Unexpected file class: {}'
                               .format(e_ident[consts.EI_CLASS]))

            if utils.ByteToInt(e_ident[consts.EI_DATA]) != consts.ELFDATA2LSB:
                raise ElfError('Unexpected data encoding: {}'
                               .format(e_ident[consts.EI_DATA]))
        except ElfError:
            self.Close()
            raise

        if utils.ByteToInt(e_ident[consts.EI_CLASS]) == consts.ELFCLASS32:
            self.bitness = 32
            self.Elf_Addr = structs.Elf32_Addr
            self.Elf_Off = structs.Elf32_Off
            self.Elf_Half = structs.Elf32_Half
            self.Elf_Word = structs.Elf32_Word
            self.Elf_Sword = structs.Elf32_Sword
            self.Elf_Ehdr = structs.Elf32_Ehdr
            self.Elf_Shdr = structs.Elf32_Shdr
            self.Elf_Dyn = structs.Elf32_Dyn
            self.Elf_Sym = structs.Elf32_Sym
            self.Elf_Rel = structs.Elf32_Rel
            self.Elf_Rela = structs.Elf32_Rela
            self.Elf_Phdr = structs.Elf32_Phdr
            self.Elf_Nhdr = structs.Elf32_Nhdr
        else:
            self.bitness = 64
            self.Elf_Addr = structs.Elf64_Addr
            self.Elf_Off = structs.Elf64_Off
            self.Elf_Half = structs.Elf64_Half
            self.Elf_Word = structs.Elf64_Word
            self.Elf_Sword = structs.Elf64_Sword
            self.Elf_Ehdr = structs.Elf64_Ehdr
            self.Elf_Shdr = structs.Elf64_Shdr
            self.Elf_Dyn = structs.Elf64_Dyn
            self.Elf_Sym = structs.Elf64_Sym
            self.Elf_Rel = structs.Elf64_Rel
            self.Elf_Rela = structs.Elf64_Rela
            self.Elf_Phdr = structs.Elf64_Phdr
            self.Elf_Nhdr = structs.Elf64_Nhdr

        try:
            self.Ehdr = self._SeekReadStruct(0, self.Elf_Ehdr)
            shoff = self.Ehdr.e_shoff
            shentsize = self.Ehdr.e_shentsize
            self.Shdr = [self._SeekReadStruct(shoff + i * shentsize,
                                              self.Elf_Shdr)
                         for i in range(self.Ehdr.e_shnum)]
        except ElfError:
            self.Close()
            raise

    def __del__(self):
        """Closes the ELF file."""
        self.Close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        """Closes the ELF file."""
        self.Close()

    def Close(self):
        """Closes the ELF file."""
        if hasattr(self, "_file"):
            self._file.close()

    def _SeekRead(self, offset, read_size):
        """Reads a byte string at specific offset in the file.

        Args:
            offset: An integer, the offset from the beginning of the ELF.
            read_size: An integer, number of bytes to read.

        Returns:
            A bytes object which is the file content.

        Raises:
            ElfError: Fails to seek and read.
        """
        if offset + read_size > self._file_size:
            raise ElfError("Read beyond end of file.")
        try:
            self._file.seek(self._begin_offset + offset)
            return self._file.read(read_size)
        except IOError as e:
            raise ElfError(e)

    def _SeekRead8(self, offset):
        """Reads an 1-byte integer from file."""
        return struct.unpack("B", self._SeekRead(offset, 1))[0]

    def _SeekRead16(self, offset):
        """Reads a 2-byte integer from file."""
        return struct.unpack("H", self._SeekRead(offset, 2))[0]

    def _SeekRead32(self, offset):
        """Reads a 4-byte integer from file."""
        return struct.unpack("I", self._SeekRead(offset, 4))[0]

    def _SeekRead64(self, offset):
        """Reads an 8-byte integer from file."""
        return struct.unpack("Q", self._SeekRead(offset, 8))[0]

    def _SeekReadString(self, offset):
        """Reads a null-terminated string starting from specific offset.

        Args:
            offset: The offset from the beginning of the ELF object.

        Returns:
            A string, excluding the null character.

        Raises:
            ElfError: String reaches end of file without null terminator.
        """
        ret = b""
        buf_size = 16
        self._file.seek(self._begin_offset + offset)
        while True:
            try:
                buf = self._file.read(buf_size)
            except IOError as e:
                raise ElfError(e)
            end_index = buf.find(b"\0")
            if end_index < 0:
                ret += buf
            else:
                ret += buf[:end_index]
                return utils.BytesToString(ret)
            if len(buf) != buf_size:
                raise ElfError("Null-terminated string reaches end of file.")

    def _SeekReadStruct(self, offset, struct_type):
        """Reads a ctypes.Structure / ctypes.Union from file.

        Args:
            offset: An integer, the offset from the beginning of the ELF.
            struct_type: A class, the structure type to read.

        Returns:
            An object of struct_type.

        Raises:
            ElfError: Fails to seek and read.
                      Fails to create struct_type instance.
        """
        raw_bytes = self._SeekRead(offset, ctypes.sizeof(struct_type))
        try:
            return struct_type.from_buffer_copy(raw_bytes)
        except ValueError as e:
            raise ElfError(e)

    def GetString(self, strtab, offset):
        """Retrieves a null-terminated string from string table.

        Args:
            strtab: Section header of the string table.
            offset: Section offset (string index) to start reading from.

        Returns:
            A string without the null terminator.

        Raises:
            ElfError: Fails to seek and read.
        """
        return self._SeekReadString(strtab.sh_offset + offset)

    def GetSectionName(self, sh):
        """Returns a section name.

        Args:
            sh: A section header.

        Returns:
            A String.

        Raises:
            ElfError: Fails to seek and read.
        """
        strtab = self.Shdr[self.Ehdr.e_shstrndx]
        return self.GetString(strtab, sh.sh_name)

    def GetSectionsByName(self, name):
        """Returns a generator of section headers from a given name.

        If multiple sections have the same name, yield them all.

        Args:
            name: The section name to search for.

        Returns:
            A generator of Elf_Shdr.

        Raises:
            ElfError: Fails to seek and read.
        """
        return (sh for sh in self.Shdr if name == self.GetSectionName(sh))

    def GetSectionByName(self, name):
        """Returns a section header whose name equals a given name.

        Returns only the first match, assuming the section name is unique.

        Args:
            name: The section name to search for.

        Returns:
            An Elf_Shdr if found.
            None if no sections have the given name.

        Raises:
            ElfError: Fails to seek and read.
        """
        for sh in self.GetSectionsByName(name):
            return sh
        return None

    def GetDynamic(self, dynamic):
        """Yields the _DYNAMIC array.

        Args:
            dynamic: Section header of the dynamic section.

        Yields:
            Elf_Dyn.

        Raises:
            ElfError: Fails to seek and read.
        """
        off = dynamic.sh_offset
        num = int(dynamic.sh_size // dynamic.sh_entsize)
        for _ in range(num):
            dyn = self._SeekReadStruct(off, self.Elf_Dyn)
            yield dyn
            if dyn.d_tag == consts.DT_NULL:
                break
            off += dynamic.sh_entsize

    def GetSymbol(self, symtab, idx):
        """Retrieves a Elf_Sym entry from symbol table.

        Args:
            symtab: A symbol table.
            idx: An integer, symbol table index.

        Returns:
            An Elf_Sym.

        Raises:
            ElfError: Fails to seek and read.
        """
        off = symtab.sh_offset + idx * symtab.sh_entsize
        return self._SeekReadStruct(off, self.Elf_Sym)

    def GetSymbols(self, symtab):
        """Returns a generator of Elf_Sym in symbol table.

        Args:
            symtab: A symbol table.

        Returns:
            A generator of Elf_Sym.

        Raises:
            ElfError: Fails to seek and read.
        """
        num = int(symtab.sh_size // symtab.sh_entsize)
        return (self.GetSymbol(symtab, i) for i in range(num))

    def GetRelocationSymbol(self, symtab, rel):
        """Retrieves the Elf_Sym with respect to an Elf_Rel / Elf_Rela.

        Args:
            symtab: A symbol table.
            rel: A Elf_Rel or Elf_Rela.

        Returns:
            An Elf_Sym.

        Raises:
            ElfError: Fails to seek and read.
        """
        return self.GetSymbol(symtab, rel.GetSymbol())

    def _CreateElfRel(self, offset, info):
        """Creates an instance of Elf_Rel.

        Args:
            offset: The initial value of r_offset.
            info: The initial value of r_info.

        Returns:
            An Elf_Rel.
        """
        elf_rel = self.Elf_Rel()
        elf_rel.r_offset = offset
        elf_rel.r_info = info
        return elf_rel

    def _DecodeAndroidRelr(self, rel):
        """Decodes a SHT_RELR / SHT_ANDROID_RELR section.

        Args:
            rel: A relocation table.

        Yields:
            Elf_Rel.

        Raises:
            ElfError: Fails to seek and read.
        """
        if self.bitness == 32:
            addr_size = 4
            seek_read_entry = self._SeekRead32
        else:
            addr_size = 8
            seek_read_entry = self._SeekRead64

        rel_offset = 0
        for ent_offset in range(rel.sh_offset, rel.sh_offset + rel.sh_size,
                                rel.sh_entsize):
            relr_entry = seek_read_entry(ent_offset)
            if (relr_entry & 1) == 0:
                # The entry is an address.
                yield self._CreateElfRel(relr_entry, 0)
                rel_offset = relr_entry + addr_size
            else:
                # The entry is a bitmap.
                for bit_idx in range(1, rel.sh_entsize * 8):
                    if (relr_entry >> bit_idx) & 1:
                        yield self._CreateElfRel(rel_offset, 0)
                    rel_offset += addr_size

    def GetRelocation(self, rel, idx):
        """Retrieves a Elf_Rel / Elf_Rela entry from relocation table.

        Args:
            rel: A relocation table.
            idx: An integer, relocation table index.

        Returns:
            An Elf_Rel or Elf_Rela.

        Raises:
            ElfError: Fails to seek and read.
        """
        off = rel.sh_offset + idx * rel.sh_entsize
        if rel.sh_type == consts.SHT_RELA:
            return self._SeekReadStruct(off, self.Elf_Rela)
        return self._SeekReadStruct(off, self.Elf_Rel)

    def GetRelocations(self, rel):
        """Returns a generator of Elf_Rel / Elf_Rela in relocation table.

        Args:
            rel: A relocation table.

        Returns:
            A generator of Elf_Rel or Elf_Rela.

        Raises:
            ElfError: Fails to seek and read.
        """
        if rel.sh_type in (consts.SHT_ANDROID_REL, consts.SHT_ANDROID_RELA):
            relocations = self._UnpackAndroidRela(rel)
            if rel.sh_type == consts.SHT_ANDROID_REL:
                return (self.Elf_Rel(r_offset=rela.r_offset, r_info=rela.r_info)
                        for rela in relocations)
            return relocations
        elif rel.sh_type in (consts.SHT_RELR, consts.SHT_ANDROID_RELR):
            return self._DecodeAndroidRelr(rel)
        else:
            num = int(rel.sh_size // rel.sh_entsize)
            return (self.GetRelocation(rel, i) for i in range(num))

    def _UnpackAndroidRela(self, android_rela):
        """Unpacks a SHT_ANDROID_REL / SHT_ANDROID_RELA section.

        Args:
            android_rela: The packed section's section header.

        Yields:
            Elf_Rela.

        Raises:
            ElfError: Fails to decode android rela section.
        """
        data = self._SeekRead(android_rela.sh_offset, android_rela.sh_size)
        # Check packed section header.
        if len(data) < 4 or data[:4] != b'APS2':
            raise ElfError('Unexpected SHT_ANDROID_RELA header: {}'
                           .format(data[:4]))
        # Decode SLEB128 word stream.
        def _PackedWordsGen():
            cur = 4
            while cur < len(data):
                try:
                    value, num = utils.DecodeSLEB128(data, cur)
                except IndexError:
                    raise ElfError('Decoding pass end of section.')
                yield value
                cur += num
            raise ElfError('Decoding pass end of section.')

        _packed_words_gen = _PackedWordsGen()
        _PopWord = lambda: next(_packed_words_gen)
        # Decode delta encoded relocation data.
        current_count = 0
        total_count = _PopWord()
        offset = _PopWord()
        addend = 0
        while current_count < total_count:
            # Read relocaiton group info.
            group_size = _PopWord()
            group_flags = _PopWord()
            group_offset_delta = 0
            # Read group flag and prepare delta values.
            grouped_by_info = (
                group_flags & consts.RELOCATION_GROUPED_BY_INFO_FLAG)
            grouped_by_offset_delta = (
                group_flags & consts.RELOCATION_GROUPED_BY_OFFSET_DELTA_FLAG)
            grouped_by_addend = (
                group_flags & consts.RELOCATION_GROUPED_BY_ADDEND_FLAG)
            group_has_addend = (
                group_flags & consts.RELOCATION_GROUP_HAS_ADDEND_FLAG)
            if grouped_by_offset_delta:
                group_offset_delta = _PopWord()
            if grouped_by_info:
                info = _PopWord()
            if group_has_addend and grouped_by_addend:
                addend += _PopWord()
            if not group_has_addend:
                addend = 0
            # Handle each relocation entry in group.
            for _ in range(group_size):
                if grouped_by_offset_delta:
                    offset += group_offset_delta
                else:
                    offset += _PopWord()
                if not grouped_by_info:
                    info = _PopWord()
                if group_has_addend and not grouped_by_addend:
                    addend += _PopWord()

                relocation = self.Elf_Rela(r_offset=offset,
                                           r_info=info,
                                           r_addend=addend)
                yield relocation
            current_count += group_size

    def _LoadDynamicSection(self, dynamic):
        """Reads entries from dynamic section.

        Args:
            dynamic: Section header of the dynamic section.

        Returns:
            A dict of {DT_NEEDED: [libraries names], DT_RUNPATH: [paths]}
            where the library names and the paths are strings.

        Raises:
            ElfError: Fails to find dynamic string table.
        """
        strtab_addr = None
        dt_needed_offsets = []
        dt_runpath_offsets = []
        for dyn in self.GetDynamic(dynamic):
            if dyn.d_tag == consts.DT_NEEDED:
                dt_needed_offsets.append(dyn.d_un.d_val)
            elif dyn.d_tag == consts.DT_RUNPATH:
                dt_runpath_offsets.append(dyn.d_un.d_val)
            elif dyn.d_tag == consts.DT_STRTAB:
                strtab_addr = dyn.d_un.d_ptr

        if strtab_addr is None:
            raise ElfError("Cannot find string table address in dynamic "
                           "section.")
        try:
            strtab = next(sh for sh in self.Shdr if sh.sh_addr == strtab_addr)
        except StopIteration:
            raise ElfError("Cannot find dynamic string table.")
        dt_needed = [self.GetString(strtab, off) for off in dt_needed_offsets]
        dt_runpath = []
        for off in dt_runpath_offsets:
            dt_runpath.extend(self.GetString(strtab, off).split(":"))
        return {consts.DT_NEEDED: dt_needed, consts.DT_RUNPATH: dt_runpath}

    def IsExecutable(self):
        """Returns whether the ELF is executable."""
        return self.Ehdr.e_type == consts.ET_EXEC

    def IsSharedObject(self):
        """Returns whether the ELF is a shared object."""
        return self.Ehdr.e_type == consts.ET_DYN

    def HasAndroidIdent(self):
        """Returns whether the ELF has a .note.android.ident section."""
        for sh in self.GetSectionsByName(".note.android.ident"):
            nh = self._SeekReadStruct(sh.sh_offset, self.Elf_Nhdr)
            name = self._SeekRead(sh.sh_offset + ctypes.sizeof(self.Elf_Nhdr),
                                  nh.n_namesz)
            if name == b"Android\0":
                return True
        return False

    def MatchCpuAbi(self, abi):
        """Returns whether the ELF matches the ABI.

        Args:
            abi: A string, the name of the ABI.

        Returns:
            A boolean, whether the ELF matches the ABI.
        """
        for abi_prefix, machine in (("arm64", consts.EM_AARCH64),
                                    ("arm", consts.EM_ARM),
                                    ("mips64", consts.EM_MIPS),
                                    ("mips", consts.EM_MIPS),
                                    ("x86_64", consts.EM_X86_64),
                                    ("x86", consts.EM_386)):
            if abi.startswith(abi_prefix):
                return self.Ehdr.e_machine == machine
        return False

    def ListDependencies(self):
        """Lists the shared libraries that the ELF depends on.

        Returns:
            2 lists of strings, the names of the depended libraries and the
            search paths.
        """
        deps = []
        runpaths = []
        for sh in self.Shdr:
            if sh.sh_type == consts.SHT_DYNAMIC:
                dynamic = self._LoadDynamicSection(sh)
                deps.extend(dynamic[consts.DT_NEEDED])
                runpaths.extend(dynamic[consts.DT_RUNPATH])
        return deps, runpaths

    def ListGlobalSymbols(self, include_weak=False,
                          symtab_name=consts.SYMTAB,
                          strtab_name=consts.STRTAB):
        """Lists the global symbols defined in the ELF.

        Args:
            include_weak: A boolean, whether to include weak symbols.
            symtab_name: A string, the name of the symbol table.
            strtab_name: A string, the name of the string table.

        Returns:
            A list of strings, the names of the symbols.

        Raises:
            ElfError: Fails to find symbol table.
        """
        symtab = self.GetSectionByName(symtab_name)
        strtab = self.GetSectionByName(strtab_name)
        if not symtab or not strtab or symtab.sh_size == 0:
            raise ElfError("Cannot find symbol table.")

        include_bindings = [consts.STB_GLOBAL]
        if include_weak:
            include_bindings.append(consts.STB_WEAK)

        sym_names = []
        for sym in self.GetSymbols(symtab):
            # Global symbols can be defined at most once at link time,
            # while weak symbols may have multiple definitions.
            if sym.GetType() == consts.STT_NOTYPE:
                continue
            if sym.GetBinding() not in include_bindings:
                continue
            if sym.st_shndx == consts.SHN_UNDEF:
                continue
            sym_names.append(self.GetString(strtab, sym.st_name))
        return sym_names

    def ListGlobalDynamicSymbols(self, include_weak=False):
        """Lists the global dynamic symbols defined in the ELF.

        Args:
            include_weak: A boolean, whether to include weak symbols.

        Returns:
            A list of strings, the names of the symbols.

        Raises:
            ElfError: Fails to find symbol table.
        """
        return self.ListGlobalSymbols(include_weak,
                                      consts.DYNSYM, consts.DYNSTR)

    def GetProgramInterpreter(self):
        """Gets the path to the program interpreter of the ELF.

        Returns:
            A string, the contents of .interp section.
            None if the section is not found.
        """
        for ph_index in range(self.Ehdr.e_phnum):
            ph = self._SeekReadStruct(
                self.Ehdr.e_phoff + ph_index * self.Ehdr.e_phentsize,
                self.Elf_Phdr)
            if ph.p_type == consts.PT_INTERP:
                return self._SeekReadString(ph.p_offset)
