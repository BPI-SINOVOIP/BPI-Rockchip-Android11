This directory contains ELF parsing utilities for VTS ABI test.

* elf_parser.py: Contains ElfParser that reads metadata from an ELF file.
* elf/consts.py: Contains ELF constants.
* elf/structs.py: Contains ELF C structs and data types.
* vtable/vtable_dumper.py: Contains VtableDumper that dumps vtable structures
                           from an ELF file.

For more info about the meanings and definitions of each ELF constant and
structs, please consult the System V ABI:
https://refspecs.linuxfoundation.org/elf/gabi4+/contents.html
