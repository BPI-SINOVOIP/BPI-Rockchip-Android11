#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Given a tryjob and perf profile, generates an AFDO profile."""

from __future__ import print_function

import argparse
import distutils.spawn
import os
import os.path
import shutil
import subprocess
import sys
import tempfile

_CREATE_LLVM_PROF = 'create_llvm_prof'
_GS_PREFIX = 'gs://'


def _fetch_gs_artifact(remote_name, local_name):
  assert remote_name.startswith(_GS_PREFIX)
  subprocess.check_call(['gsutil', 'cp', remote_name, local_name])


def _fetch_and_maybe_unpack(remote_name, local_name):
  unpackers = [
      ('.tar.bz2', ['tar', 'xaf']),
      ('.bz2', ['bunzip2']),
      ('.tar.xz', ['tar', 'xaf']),
      ('.xz', ['xz', '-d']),
  ]

  unpack_ext = None
  unpack_cmd = None
  for ext, unpack in unpackers:
    if remote_name.endswith(ext):
      unpack_ext, unpack_cmd = ext, unpack
      break

  download_to = local_name + unpack_ext if unpack_ext else local_name
  _fetch_gs_artifact(remote_name, download_to)
  if unpack_cmd is not None:
    print('Unpacking', download_to)
    subprocess.check_output(unpack_cmd + [download_to])
    assert os.path.exists(local_name)


def _generate_afdo(perf_profile_loc, tryjob_loc, output_name):
  if perf_profile_loc.startswith(_GS_PREFIX):
    local_loc = 'perf.data'
    _fetch_and_maybe_unpack(perf_profile_loc, local_loc)
    perf_profile_loc = local_loc

  chrome_in_debug_loc = 'debug/opt/google/chrome/chrome.debug'
  debug_out = 'debug.tgz'
  _fetch_gs_artifact(os.path.join(tryjob_loc, 'debug.tgz'), debug_out)

  print('Extracting chrome.debug.')
  # This has tons of artifacts, and we only want Chrome; don't waste time
  # extracting the rest in _fetch_and_maybe_unpack.
  subprocess.check_call(['tar', 'xaf', 'debug.tgz', chrome_in_debug_loc])

  # Note that the AFDO tool *requires* a binary named `chrome` to be present if
  # we're generating a profile for chrome. It's OK for this to be split debug
  # information.
  os.rename(chrome_in_debug_loc, 'chrome')

  print('Generating AFDO profile.')
  subprocess.check_call([
      _CREATE_LLVM_PROF, '--out=' + output_name, '--binary=chrome',
      '--profile=' + perf_profile_loc
  ])


def _abspath_or_gs_link(path):
  if path.startswith(_GS_PREFIX):
    return path
  return os.path.abspath(path)


def _tryjob_arg(tryjob_arg):
  # Forward gs args through
  if tryjob_arg.startswith(_GS_PREFIX):
    return tryjob_arg

  # Clicking on the 'Artifacts' link gives us a pantheon link that's basically
  # a preamble and gs path.
  pantheon = 'https://pantheon.corp.google.com/storage/browser/'
  if tryjob_arg.startswith(pantheon):
    return _GS_PREFIX + tryjob_arg[len(pantheon):]

  # Otherwise, only do things with a tryjob ID (e.g. R75-11965.0.0-b3648595)
  if not tryjob_arg.startswith('R'):
    raise ValueError('Unparseable tryjob arg; give a tryjob ID, pantheon '
                     'link, or gs:// link. Please see source for more.')

  chell_path = 'chromeos-image-archive/chell-chrome-pfq-tryjob/'
  # ...And assume it's from chell, since that's the only thing we generate
  # profiles with today.
  return _GS_PREFIX + chell_path + tryjob_arg


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--perf_profile',
      required=True,
      help='Path to our perf profile. Accepts either a gs:// path or local '
      'filepath.')
  parser.add_argument(
      '--tryjob',
      required=True,
      type=_tryjob_arg,
      help="Path to our tryjob's artifacts. Accepts a gs:// path, pantheon "
      'link, or tryjob ID, e.g. R75-11965.0.0-b3648595. In the last case, '
      'the assumption is that you ran a chell-chrome-pfq-tryjob.')
  parser.add_argument(
      '-o',
      '--output',
      default='afdo.prof',
      help='Where to put the AFDO profile. Default is afdo.prof.')
  parser.add_argument(
      '-k',
      '--keep_artifacts_on_failure',
      action='store_true',
      help="Don't remove the tempdir on failure")
  args = parser.parse_args()

  if not distutils.spawn.find_executable(_CREATE_LLVM_PROF):
    sys.exit(_CREATE_LLVM_PROF + ' not found; are you in the chroot?')

  profile = _abspath_or_gs_link(args.perf_profile)
  afdo_output = os.path.abspath(args.output)

  initial_dir = os.getcwd()
  temp_dir = tempfile.mkdtemp(prefix='generate_afdo')
  success = True
  try:
    os.chdir(temp_dir)
    _generate_afdo(profile, args.tryjob, afdo_output)

    # The AFDO tooling is happy to generate essentially empty profiles for us.
    # Chrome's profiles are often 8+ MB; if we only see a small fraction of
    # that, something's off. 512KB was arbitrarily selected.
    if os.path.getsize(afdo_output) < 512 * 1024:
      raise ValueError('The AFDO profile is suspiciously small for Chrome. '
                       'Something might have gone wrong.')
  except:
    success = False
    raise
  finally:
    os.chdir(initial_dir)

    if success or not args.keep_artifacts_on_failure:
      shutil.rmtree(temp_dir, ignore_errors=True)
    else:
      print('Artifacts are available at', temp_dir)


if __name__ == '__main__':
  sys.exit(main())
