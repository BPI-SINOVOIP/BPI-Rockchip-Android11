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
"""This file contains ELF C structs and data types."""

import ctypes

from vts.utils.python.library.elf import consts


# ELF data types.


Elf32_Addr = ctypes.c_uint32
Elf32_Off = ctypes.c_uint32
Elf32_Half = ctypes.c_uint16
Elf32_Word = ctypes.c_uint32
Elf32_Sword = ctypes.c_int32

Elf64_Addr = ctypes.c_uint64
Elf64_Off = ctypes.c_uint64
Elf64_Half = ctypes.c_uint16
Elf64_Word = ctypes.c_uint32
Elf64_Sword = ctypes.c_int32
Elf64_Xword = ctypes.c_uint64
Elf64_Sxword = ctypes.c_int64


# ELF C structs.


class CStructure(ctypes.LittleEndianStructure):
    """Little endian C structure base class."""
    pass


class CUnion(ctypes.Union):
    """Native endian C union base class."""
    pass


class _Ehdr(CStructure):
    """ELF header base class."""

    def GetFileClass(self):
        """Returns the file class."""
        return self.e_ident[consts.EI_CLASS]

    def GetDataEncoding(self):
        """Returns the data encoding of the file."""
        return self.e_ident[consts.EI_DATA]


class Elf32_Ehdr(_Ehdr):
    """ELF 32-bit header."""
    _fields_ = [('e_ident', ctypes.c_uint8 * consts.EI_NIDENT),
                ('e_type', Elf32_Half),
                ('e_machine', Elf32_Half),
                ('e_version', Elf32_Word),
                ('e_entry', Elf32_Addr),
                ('e_phoff', Elf32_Off),
                ('e_shoff', Elf32_Off),
                ('e_flags', Elf32_Word),
                ('e_ehsize', Elf32_Half),
                ('e_phentsize', Elf32_Half),
                ('e_phnum', Elf32_Half),
                ('e_shentsize', Elf32_Half),
                ('e_shnum', Elf32_Half),
                ('e_shstrndx', Elf32_Half)]


class Elf64_Ehdr(_Ehdr):
    """ELF 64-bit header."""
    _fields_ = [('e_ident', ctypes.c_uint8 * consts.EI_NIDENT),
                ('e_type', Elf64_Half),
                ('e_machine', Elf64_Half),
                ('e_version', Elf64_Word),
                ('e_entry', Elf64_Addr),
                ('e_phoff', Elf64_Off),
                ('e_shoff', Elf64_Off),
                ('e_flags', Elf64_Word),
                ('e_ehsize', Elf64_Half),
                ('e_phentsize', Elf64_Half),
                ('e_phnum', Elf64_Half),
                ('e_shentsize', Elf64_Half),
                ('e_shnum', Elf64_Half),
                ('e_shstrndx', Elf64_Half)]


class Elf32_Shdr(CStructure):
    """ELF 32-bit section header."""
    _fields_ = [('sh_name', Elf32_Word),
                ('sh_type', Elf32_Word),
                ('sh_flags', Elf32_Word),
                ('sh_addr', Elf32_Addr),
                ('sh_offset', Elf32_Off),
                ('sh_size', Elf32_Word),
                ('sh_link', Elf32_Word),
                ('sh_info', Elf32_Word),
                ('sh_addralign', Elf32_Word),
                ('sh_entsize', Elf32_Word)]


class Elf64_Shdr(CStructure):
    """ELF 64-bit section header."""
    _fields_ = [('sh_name', Elf64_Word),
                ('sh_type', Elf64_Word),
                ('sh_flags', Elf64_Xword),
                ('sh_addr', Elf64_Addr),
                ('sh_offset', Elf64_Off),
                ('sh_size', Elf64_Xword),
                ('sh_link', Elf64_Word),
                ('sh_info', Elf64_Word),
                ('sh_addralign', Elf64_Xword),
                ('sh_entsize', Elf64_Xword)]


class Elf32_Dyn(CStructure):
    """ELF 32-bit dynamic section entry."""
    class _Elf32_Dyn__d_un(CUnion):
        _fields_ = [('d_val', Elf32_Word),
                    ('d_ptr', Elf32_Addr)]
    _fields_ = [('d_tag', Elf32_Sword),
                ('d_un', _Elf32_Dyn__d_un)]


class Elf64_Dyn(CStructure):
    """ELF 64-bit dynamic section entry."""
    class _Elf64_Dyn__d_un(CUnion):
        _fields_ = [('d_val', Elf64_Xword),
                    ('d_ptr', Elf64_Addr)]
    _fields_ = [('d_tag', Elf64_Sxword),
                ('d_un', _Elf64_Dyn__d_un)]


class _Sym(CStructure):
    """ELF symbol table entry base class."""

    def GetBinding(self):
        """Returns the symbol binding."""
        return self.st_info >> 4

    def GetType(self):
        """Returns the symbol type."""
        return self.st_info & 0xf

    def SetBinding(self, binding):
        """Sets the symbol binding.

        Args:
            binding: An integer specifying the new binding.
        """
        self.SetSymbolAndType(binding, self.GetType())

    def SetType(self, type_):
        """Sets the symbol type.

        Args:
            type_: An integer specifying the new type.
        """
        self.SetSymbolAndType(self.GetBinding(), type_)

    def SetBindingAndType(self, binding, type_):
        """Sets the symbol binding and type.

        Args:
            binding: An integer specifying the new binding.
            type_: An integer specifying the new type.
        """
        self.st_info = (binding << 4) | (type_ & 0xf)


class Elf32_Sym(_Sym):
    """ELF 32-bit symbol table entry."""
    _fields_ = [('st_name', Elf32_Word),
                ('st_value', Elf32_Addr),
                ('st_size', Elf32_Word),
                ('st_info', ctypes.c_uint8),
                ('st_other', ctypes.c_uint8),
                ('st_shndx', Elf32_Half)]


class Elf64_Sym(_Sym):
    """ELF 64-bit symbol table entry."""
    _fields_ = [('st_name', Elf64_Word),
                ('st_info', ctypes.c_uint8),
                ('st_other', ctypes.c_uint8),
                ('st_shndx', Elf64_Half),
                ('st_value', Elf64_Addr),
                ('st_size', Elf64_Xword)]


class _32_Rel(CStructure):
    """ELF 32-bit relocation table entry base class."""

    def GetSymbol(self):
        """Returns the symbol table index with respect to the relocation.

        Symbol table index with respect to which the relocation must be made.
        """
        return self.r_info >> 8

    def GetType(self):
        """Returns the relocation type."""
        return self.r_info & 0xff

    def SetSymbol(self, symndx):
        """Sets the relocation's symbol table index.

        Args:
            symndx: An integer specifying the new symbol table index.
        """
        self.SetSymbolAndType(symndx, self.GetType())

    def SetType(self, type_):
        """Sets the relocation type.

        Args:
            type_: An integer specifying the new relocation type.
        """
        self.SetSymbolAndType(self.GetSymbol(), type_)

    def SetSymbolAndType(self, symndx, type_):
        """Sets the relocation's symbol table index and type.

        Args:
            symndx: An integer specifying the new symbol table index.
            type_: An integer specifying the new relocation type.
        """
        self.r_info = (symndx << 8) | (type_ & 0xff)


class Elf32_Rel(_32_Rel):
    """ELF 32-bit relocation table entry."""
    _fields_ = [('r_offset', Elf32_Addr),
                ('r_info', Elf32_Word)]


class Elf32_Rela(_32_Rel):
    """ELF 32-bit relocation table entry with explicit addend."""
    _fields_ = [('r_offset', Elf32_Addr),
                ('r_info', Elf32_Word),
                ('r_addend', Elf32_Sword)]


class _64_Rel(CStructure):
    """ELF 64-bit relocation table entry base class."""

    def GetSymbol(self):
        """Returns the symbol table index with respect to the relocation.

        Symbol table index with respect to which the relocation must be made.
        """
        return self.r_info >> 32

    def GetType(self):
        """Returns the relocation type."""
        return self.r_info & 0xffffffff

    def SetSymbol(self, symndx):
        """Sets the relocation's symbol table index.

        Args:
            symndx: An integer specifying the new symbol table index.
        """
        self.SetSymbolAndType(symndx, self.GetType())

    def SetType(self, type_):
        """Sets the relocation type.

        Args:
            type_: An integer specifying the new relocation type.
        """
        self.SetSymbolAndType(self.GetSymbol(), type_)

    def SetSymbolAndType(self, symndx, type_):
        """Sets the relocation's symbol table index and type.

        Args:
            symndx: An integer specifying the new symbol table index.
            type_: An integer specifying the new relocation type.
        """
        self.r_info = (symndx << 32) | (type_ & 0xffffffff)


class Elf64_Rel(_64_Rel):
    """ELF 64-bit relocation table entry."""
    _fields_ = [('r_offset', Elf64_Addr),
                ('r_info', Elf64_Xword)]


class Elf64_Rela(_64_Rel):
    """ELF 64-bit relocation table entry with explicit addend."""
    _fields_ = [('r_offset', Elf64_Addr),
                ('r_info', Elf64_Xword),
                ('r_addend', Elf64_Sxword)]


class Elf32_Phdr(CStructure):
    """ELF 32-bit program header."""
    _fields_ = [('p_type', Elf32_Word),
                ('p_offset', Elf32_Off),
                ('p_vaddr', Elf32_Addr),
                ('p_paddr', Elf32_Addr),
                ('p_filesz', Elf32_Word),
                ('p_memsz', Elf32_Word),
                ('p_flags', Elf32_Word),
                ('p_align', Elf32_Word)]


class Elf64_Phdr(CStructure):
    """ELF 64-bit program header."""
    _fields_ = [('p_type', Elf64_Word),
                ('p_flags', Elf64_Word),
                ('p_offset', Elf64_Off),
                ('p_vaddr', Elf64_Addr),
                ('p_paddr', Elf64_Addr),
                ('p_filesz', Elf64_Xword),
                ('p_memsz', Elf64_Xword),
                ('p_align', Elf64_Xword)]


class Elf32_Nhdr(CStructure):
    """ELF 32-bit note header."""
    _fields_ = [('n_namesz', Elf32_Word),
                ('n_descsz', Elf32_Word),
                ('n_type', Elf32_Word)]


class Elf64_Nhdr(CStructure):
    """ELF 64-bit note header."""
    _fields_ = [('n_namesz', Elf64_Word),
                ('n_descsz', Elf64_Word),
                ('n_type', Elf64_Word)]
