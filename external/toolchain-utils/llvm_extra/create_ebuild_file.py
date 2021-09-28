#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create llvm ebuild file.

This script takes an existing host llvm compiler ebuild and
creates another build that should be installable in a prefixed location.
The script patches a few lines in the llvm ebuild to make that happen.

Since the script is based on the current llvm ebuild patterns,
it may need to be updated if those patterns change.

This script should normally be invoked by the shell script
create_llvm_extra.sh .

Below is an example of the expected diff of the newly generated ebuild with
some explanation of the diffs.

diff -Nuar llvm-pre7.0_pre335547_p20180529.ebuild newly-created-file.ebuild
--- llvm-7.0_pre331547_p20180529-r8.ebuild
+++ newly-created-file.ebuild

@@ -60,9 +60,9 @@ EGIT_REPO_URIS=(
 fi

 LICENSE="UoI-NCSA"
-SLOT="0/${PV%%_*}"
+SLOT="${PV%%_p[[:digit:]]*}" # Creates a unique slot so that multiple copies
                                of the new build can be installed.

 KEYWORDS="-* amd64"

 # Change USE flags to match llvm ebuild installtion. To see the set of flags
 enabled in llvm compiler ebuild, run $ sudo emerge -pv llvm

-IUSE="debug +default-compiler-rt +default-libcxx doc libedit +libffi"
+IUSE="debug +default-compiler-rt +default-libcxx doc libedit +libffi
        ncurses ocaml python llvm-next llvm-tot test xml video_cards_radeon"

 COMMON_DEPEND="
@@ -145,6 +145,7 @@ pkg_pretend() {
 }

 pkg_setup() {
 # This Change is to install the files in $PREFIX.
+       export PREFIX="/usr/${PN}/${SLOT}"
        pkg_pretend
 }

@@ -272,13 +273,13 @@
        sed -e "/RUN/s/-warn-error A//" -i test/Bindings/OCaml/*ml  || die

        # Allow custom cmake build types (like 'Gentoo')
 # Convert use of PN to llvm in epatch commands.
-       epatch "${FILESDIR}"/cmake/${PN}-3.8-allow_custom_cmake_build.patch
+       epatch "${FILESDIR}"/cmake/llvm-3.8-allow_custom_cmake_build.patch

        # crbug/591436
        epatch "${FILESDIR}"/clang-executable-detection.patch

        # crbug/606391
-       epatch "${FILESDIR}"/${PN}-3.8-invocation.patch
+       epatch "${FILESDIR}"/llvm-3.8-invocation.patch

@@ -411,11 +412,14 @@ src_install() {
                /usr/include/llvm/Config/llvm-config.h
        )

+       MULTILIB_CHOST_TOOLS=() # No need to install any multilib tools/headers.
+       MULTILIB_WRAPPED_HEADERS=()
        multilib-minimal_src_install
 }

 multilib_src_install() {
        cmake-utils_src_install
+       return # No need to install any wrappers.

        local wrapper_script=clang_host_wrapper
        cat "${FILESDIR}/clang_host_wrapper.header" \
@@ -434,6 +438,7 @@ multilib_src_install() {
 }

 multilib_src_install_all() {
+       return # No need to install common multilib files.
        insinto /usr/share/vim/vimfiles
        doins -r utils/vim/*/.
        # some users may find it useful
"""

from __future__ import print_function

import os
import sys


def process_line(line, text):
  # Process the line and append to the text we want to generate.
  # Check if line has any patterns that we want to handle.
  newline = line.strip()
  if newline.startswith('#'):
    # Do not process comment lines.
    text.append(line)
  elif line.startswith('SLOT='):
    # Change SLOT to "${PV%%_p[[:digit:]]*}"
    SLOT_STRING = 'SLOT="${PV%%_p[[:digit:]]*}"\n'
    text.append(SLOT_STRING)
  elif line.startswith('IUSE') and 'multitarget' in line:
    # Enable multitarget USE flag.
    newline = line.replace('multitarget', '+multitarget')
    text.append(newline)
  elif line.startswith('pkg_setup()'):
    # Setup PREFIX.
    text.append(line)
    text.append('\texport PREFIX="/usr/${PN}/${SLOT}"\n')
  elif line.startswith('multilib_src_install_all()'):
    text.append(line)
    # Do not install any common files.
    text.append('\treturn\n')
  elif 'epatch ' in line:
    # Convert any $PN or ${PN} in epatch files to llvm.
    newline = line.replace('$PN', 'llvm')
    newline = newline.replace('${PN}', 'llvm')
    text.append(newline)
  elif 'multilib-minimal_src_install' in line:
    # Disable MULTILIB_CHOST_TOOLS and MULTILIB_WRAPPED_HEADERS
    text.append('\tMULTILIB_CHOST_TOOLS=()\n')
    text.append('\tMULTILIB_WRAPPED_HEADERS=()\n')
    text.append(line)
  elif 'cmake-utils_src_install' in line:
    text.append(line)
    # Do not install any wrappers.
    text.append('\treturn\n')
  else:
    text.append(line)


def main():
  if len(sys.argv) != 3:
    filename = os.path.basename(__file__)
    print('Usage: ', filename, ' <input.ebuild> <output.ebuild>')
    return 1

  text = []
  with open(sys.argv[1], 'r') as infile:
    for line in infile:
      process_line(line, text)

  with open(sys.argv[2], 'w') as outfile:
    outfile.write(''.join(text))

  return 0


if __name__ == '__main__':
  sys.exit(main())
