# emugen (cuttlefish)

This directory contains a fork of the emugen tool from the external/qemu project
on the emu-master-dev branch. The tool is unmodified except for the following
changes:

1) The android/base/EnumFlags.h header has been forked, to avoid having to copy
   over all of the host-side android-base port.

2) The unittest infrastructure and all unittests have been removed.

3) This README.md file replaces the upstream README file.

4) The binary builds to `emugen_cuttlefish` so as to avoid conflicts.

Do not contribute change only to this project; contribute them to external/qemu
upstream, then cherry-pick them over.
