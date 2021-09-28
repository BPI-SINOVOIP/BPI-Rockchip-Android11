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
"""This file contains ELF constants."""

# e_ident[] indices
EI_MAG0 = 0         # File identification
EI_MAG1 = 1         # File identification
EI_MAG2 = 2         # File identification
EI_MAG3 = 3         # File identification
EI_CLASS = 4        # File class
EI_DATA = 5         # Data encoding
EI_VERSION = 6      # File version
EI_OSABI = 7        # Operating system/ABI identification
EI_ABIVERSION = 8   # ABI version
EI_PAD = 9          # Start of padding bytes
EI_NIDENT = 16      # Size of e_ident[]

# EI_MAG0 to EI_MAG3
ELF_MAGIC_NUMBER = b'\x7fELF'

# EI_CLASS
ELFCLASSNONE = 0    # Invalid class
ELFCLASS32 = 1      # 32-bit objects
ELFCLASS64 = 2      # 64-bit objects

# EI_DATA
ELFDATANONE = 0     # Invalid data encoding
ELFDATA2LSB = 1     # Little endian
ELFDATA2MSB = 2     # Big endian

# e_type
ET_NONE = 0         # No file type
ET_REL = 1          # Relocatable file (object file)
ET_EXEC = 2         # Executable file
ET_DYN = 3          # Shared object file
ET_CORE = 4         # Core file

# e_machine
EM_386 = 3
EM_X86_64 = 62
EM_MIPS = 8
EM_ARM = 40
EM_AARCH64 = 183

# Relocation types
R_ARM_ABS32 = 0x02
R_ARM_RELATIVE = 0x17
R_AARCH64_ABS64 = 0x101
R_AARCH64_RELATIVE = 0x403
R_386_32 = 1
R_386_RELATIVE = 8
R_X86_64_64 = 1
R_X86_64_RELATIVE = 8

# Section types
SHT_NULL = 0
SHT_PROGBITS = 1
SHT_SYMTAB = 2
SHT_STRTAB = 3
SHT_RELA = 4
SHT_HASH = 5
SHT_DYNAMIC = 6
SHT_NOTE = 7
SHT_NOBITS = 8
SHT_REL = 9
SHT_SHLIB = 10
SHT_DYNSYM = 11
SHT_INIT_ARRAY = 14
SHT_FINI_ARRAY = 15
SHT_PREINIT_ARRAY = 16
SHT_GROUP = 17
SHT_SYMTAB_SHNDX = 18
SHT_RELR = 19
SHT_LOOS = 0x60000000
SHT_ANDROID_REL = SHT_LOOS + 1
SHT_ANDROID_RELA = SHT_LOOS + 2
SHT_ANDROID_RELR = SHT_LOOS + 0xfffff00

# Android packed relocation flags
RELOCATION_GROUPED_BY_INFO_FLAG = 1
RELOCATION_GROUPED_BY_OFFSET_DELTA_FLAG = 2
RELOCATION_GROUPED_BY_ADDEND_FLAG = 4
RELOCATION_GROUP_HAS_ADDEND_FLAG = 8

# Section names
SYMTAB = '.symtab'
STRTAB = '.strtab'
DYNSYM = '.dynsym'
DYNSTR = '.dynstr'

# Special section indices
SHN_UNDEF = 0
SHN_LORESERVE = 0xff00
SHN_LOPROC = 0xff00
SHN_HIPROC = 0xff1f
SHN_LOOS = 0xff20
SHN_HIOS = 0xff3f
SHN_ABS = 0xfff1
SHN_COMMON = 0xfff2
SHN_XINDEX = 0xffff
SHN_HIRESERVE = 0xffff

# Symbol bindings
STB_LOCAL = 0
STB_GLOBAL = 1
STB_WEAK = 2
STB_LOOS = 10
STB_HIOS = 12
STB_LOPROC = 13
STB_HIPROC = 15

# Symbol types
STT_NOTYPE = 0
STT_OBJECT = 1
STT_FUNC = 2
STT_SECTION = 3
STT_FILE = 4
STT_COMMON = 5
STT_TLS = 6
STT_LOOS = 10
STT_HIOS = 12
STT_LOPROC = 13
STT_HIPROC = 15

# Segment types
PT_NULL = 0
PT_LOAD = 1
PT_DYNAMIC = 2
PT_INTERP = 3
PT_NOTE = 4
PT_SHLIB = 5
PT_PHDR = 6
PT_TLS = 7
PT_LOOS = 0x60000000
PT_HIOS = 0x6fffffff
PT_LOPROC = 0x70000000
PT_HIPROC = 0x7fffffff
PT_GNU_EH_FRAME = 0x6474e550
PT_SUNW_EH_FRAME = 0x6474e550
PT_SUNW_UNWIND = 0x6464e550
PT_GNU_STACK = 0x6474e551
PT_GNU_RELRO = 0x6474e552
PT_ARM_ARCHEXT = 0x70000000
PT_ARM_EXIDX = 0x70000001
PT_ARM_UNWIND = 0x70000001
PT_MIPS_REGINFO = 0x70000000
PT_MIPS_RTPROC = 0x70000001
PT_MIPS_OPTIONS = 0x70000002
PT_MIPS_ABIFLAGS = 0x70000003

# Dynamic array tags
# Name     Value        d_un          Executable  Shared Object
DT_NULL = 0             # ignored     mandatory   mandatory
DT_NEEDED = 1           # d_val       optional    optional
DT_PLTRELSZ = 2         # d_val       optional    optional
DT_PLTGOT = 3           # d_ptr       optional    optional
DT_HASH = 4             # d_ptr       mandatory   mandatory
DT_STRTAB = 5           # d_ptr       mandatory   mandatory
DT_SYMTAB = 6           # d_ptr       mandatory   mandatory
DT_RELA = 7             # d_ptr       mandatory   optional
DT_RELASZ = 8           # d_val       mandatory   optional
DT_RELAENT = 9          # d_val       mandatory   optional
DT_STRSZ = 10           # d_val       mandatory   mandatory
DT_SYMENT = 11          # d_val       mandatory   mandatory
DT_INIT = 12            # d_ptr       optional    optional
DT_FINI = 13            # d_ptr       optional    optional
DT_SONAME = 14          # d_val       ignored     optional
DT_RPATH = 15           # d_val       optional    ignored
DT_SYMBOLIC = 16        # ignored     ignored     optional
DT_REL = 17             # d_ptr       mandatory   optional
DT_RELSZ = 18           # d_val       mandatory   optional
DT_RELENT = 19          # d_val       mandatory   optional
DT_PLTREL = 20          # d_val       optional    optional
DT_DEBUG = 21           # d_ptr       optional    ignored
DT_TEXTREL = 22         # ignored     optional    optional
DT_JMPREL = 23          # d_ptr       optional    optional
DT_BIND_NOW = 24        # ignored     optional    optional
DT_INIT_ARRAY = 25      # d_ptr       optional    optional
DT_FINI_ARRAY = 26      # d_ptr       optional    optional
DT_INIT_ARRAYSZ = 27    # d_val       optional    optional
DT_FINI_ARRAYSZ = 28    # d_val       optional    optional
DT_RUNPATH = 29         # d_val       optional    optional
DT_FLAGS = 30           # d_val       optional    optional
DT_ENCODING = 32        # unspecified unspecified unspecified
DT_LOOS = 0x6000000D    # unspecified unspecified unspecified
DT_ANDROID_REL = DT_LOOS + 2      # d_ptr
DT_ANDROID_RELSZ = DT_LOOS + 3    # d_val
DT_ANRDOID_RELA = DT_LOOS + 4     # d_ptr
DT_ANRDOID_RELASZ = DT_LOOS + 5   # d_val
DT_RELR = 0x6fffe000              # d_ptr
DT_RELRSZ = 0x6fffe001            # d_val
DT_RELRENT = 0x6fffe003           # d_val
