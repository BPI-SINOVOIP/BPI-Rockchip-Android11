#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Download profdata from different arches, merge them and upload to gs

The script is used for updating the PGO profiles for LLVM. The workflow
is that the script will download profdata from different PGO builds, merge
them and then upload it to a gs location that LLVM can access.

The simplest way of using this script, is to run:
    ./merge_profdata_and_upload.py --all_latest_profiles
which will automatically grab profdata from latest PGO generate builders
for three different architectures and merge them. LLVM hash is also
detected automatically from the artifacts.

If you want to specify certain llvm hash, run it with:
    ./merge_profdata_and_upload.py --all_latest_profiles --llvm_hash LLVM_HASH
Note that hash checking will fail if the llvm hash you provided is not the
same as those in artifacts, or llvm hash in different artifacts are not the
same.

To only use profiles from PGO generate tryjob, run it with:
    ./merge_profdata_and_upload.py --nolatest -t TRYJOB1 -t TRYJOB2 ...
Using of --nolatest will tell the script not to use any results from builders,
and merge only the profdata from the tryjobs you specified.

There is a chance that builders only succeeded partially, in this case, you
can run this script to merge both profdata from builder and tryjob:
    ./merge_profdata_and_upload.py -l arm -l amd64 -t TRYJOB_FOR_ARM64
In this example, the script will merge profdata from arm and amd64 builder, and
profdata from an arm64 tryjob.
"""

from __future__ import print_function

import argparse
import collections
import distutils.spawn
import json
import os
import os.path
import shutil
import subprocess
import sys
import tempfile

_LLVM_PROFDATA = '/usr/bin/llvm-profdata'
_GS_PREFIX = 'gs://'

_LLVMMetadata = collections.namedtuple('_LLVMMetadata', ['head_sha'])


def _get_gs_latest(remote_lastest):
  assert remote_lastest.startswith(_GS_PREFIX)
  try:
    return subprocess.check_output(['gsutil', 'cat', remote_lastest],
                                   encoding='utf-8')
  except subprocess.CalledProcessError:
    raise RuntimeError('Lastest artifacts not found: %s' % remote_lastest)


def _fetch_gs_artifact(remote_name, local_name):
  assert remote_name.startswith(_GS_PREFIX)

  print('Fetching %r to %r' % (remote_name, local_name))
  subprocess.check_call(['gsutil', 'cp', remote_name, local_name])


def _find_latest_artifacts(arch):
  remote_latest = (
      '%schromeos-image-archive/'
      '%s-pgo-generate-llvm-next-toolchain/LATEST-master' % (_GS_PREFIX, arch))
  version = _get_gs_latest(remote_latest)
  return '%s-pgo-generate-llvm-next-toolchain/%s' % (arch, version)


def _get_gs_profdata(remote_base, base_dir):
  remote_profdata_basename = 'llvm_profdata.tar.xz'

  remote_profdata = os.path.join(remote_base, remote_profdata_basename)
  tar = 'llvm_profdata.tar.xz'
  _fetch_gs_artifact(remote_profdata, tar)
  extract_cmd = ['tar', '-xf', tar]

  print('Extracting profdata tarball.\nCMD: %s\n' % extract_cmd)
  subprocess.check_call(extract_cmd)
  # Return directory to the llvm.profdata extracted.
  if '-tryjob/' in base_dir:
    prefix = 'b/s/w/ir/cache/cbuild/repository/trybot_archive/'
  else:
    prefix = 'b/s/w/ir/cache/cbuild/repository/buildbot_archive/'
  return os.path.join(prefix, base_dir, 'llvm.profdata')


def _get_gs_metadata(remote_base):
  metadata_basename = 'llvm_metadata.json'
  _fetch_gs_artifact(
      os.path.join(remote_base, metadata_basename), metadata_basename)

  with open(metadata_basename) as f:
    result = json.load(f)

  return _LLVMMetadata(head_sha=result['head_sha'])


def _get_gs_artifacts(base_dir):
  remote_base = '%schromeos-image-archive/%s' % (_GS_PREFIX, base_dir)
  profile_path = _get_gs_profdata(remote_base, base_dir)
  metadata = _get_gs_metadata(remote_base)
  return metadata, profile_path


def _merge_profdata(profdata_list, output_name):
  merge_cmd = [_LLVM_PROFDATA, 'merge', '-output', output_name] + profdata_list
  print('Merging PGO profiles.\nCMD: %s\n' % merge_cmd)
  subprocess.check_call(merge_cmd)


def _tar_and_upload_profdata(profdata, name_suffix):
  tarball = 'llvm-profdata-%s.tar.xz' % name_suffix
  print('Making profdata tarball: %s' % tarball)
  subprocess.check_call(
      ['tar', '--sparse', '-I', 'xz', '-cf', tarball, profdata])

  upload_location = '%schromeos-localmirror/distfiles/%s' % (_GS_PREFIX,
                                                             tarball)

  # TODO: it's better to create a subdir: distfiles/llvm_pgo_profile, but
  # now llvm could only recognize distfiles.
  upload_cmd = [
      'gsutil',
      '-m',
      'cp',
      '-n',
      '-a',
      'public-read',
      tarball,
      upload_location,
  ]
  print('Uploading tarball to gs.\nCMD: %s\n' % upload_cmd)

  # gsutil prints all status to stderr, oddly enough.
  gs_output = subprocess.check_output(
      upload_cmd, stderr=subprocess.STDOUT, encoding='utf-8')
  print(gs_output)

  # gsutil exits successfully even if it uploaded nothing. It prints a summary
  # of what all it did, though. Successful uploads are just a progress bar,
  # unsuccessful ones note that items were skipped.
  if 'Skipping existing item' in gs_output:
    raise ValueError('Profile upload failed: would overwrite an existing '
                     'profile at %s' % upload_location)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      '-a',
      '--all_latest_profiles',
      action='store_true',
      help='Merge and upload profiles from the latest builders.')
  parser.add_argument(
      '-l',
      '--latest',
      default=[],
      action='append',
      help='User can specify the profdata from which builder with specific '
      'architecture to download. By default, we merge profdata from arm, '
      'arm64, amd64.')
  parser.add_argument(
      '-t',
      '--tryjob',
      default=[],
      action='append',
      help='Extra pgo-generate-llvm-next-toolchain/tryjob results to be used. '
      'Format should be '
      '{arch}-pgo-generate-llvm-next-toolchain(-tryjob)/{VERSION}.')
  parser.add_argument(
      '-o',
      '--output',
      default='llvm.profdata',
      help='Where to put merged PGO profile. The default is to not save it '
      'anywhere.')
  parser.add_argument(
      '--llvm_hash',
      help='The LLVM hash to select for the profiles. Generally autodetected.')
  args = parser.parse_args()

  if not args.all_latest_profiles and not (args.latest or args.tryjob):
    sys.exit('Please specify whether to use latest profiles or profiles from '
             'tryjobs')

  if args.all_latest_profiles and (args.latest or args.tryjob):
    sys.exit('--all_latest_profiles cannot be specified together with '
             '--latest or --tryjob.')

  latest = ['arm', 'arm64', 'amd64'] \
    if args.all_latest_profiles else args.latest

  if not distutils.spawn.find_executable(_LLVM_PROFDATA):
    sys.exit(_LLVM_PROFDATA + ' not found; are you in the chroot?')

  initial_dir = os.getcwd()
  temp_dir = tempfile.mkdtemp(prefix='merge_pgo')
  success = True
  try:
    os.chdir(temp_dir)
    profdata_list = []
    heads = set()

    def fetch_and_append_artifacts(gs_url):
      llvm_metadata, profdata_loc = _get_gs_artifacts(gs_url)
      if os.path.getsize(profdata_loc) < 512 * 1024:
        raise RuntimeError('The PGO profile in %s (local path: %s) is '
                           'suspiciously small. Something might have gone '
                           'wrong.' % (gs_url, profdata_loc))

      heads.add(llvm_metadata.head_sha)
      profdata_list.append(profdata_loc)

    for arch in latest:
      fetch_and_append_artifacts(_find_latest_artifacts(arch))

    if args.tryjob:
      for tryjob in args.tryjob:
        fetch_and_append_artifacts(tryjob)

    assert heads, "Didn't fetch anything?"

    def die_with_head_complaint(complaint):
      extra = ' (HEADs found: %s)' % sorted(heads)
      raise RuntimeError(complaint.rstrip() + extra)

    llvm_hash = args.llvm_hash
    if not llvm_hash:
      if len(heads) != 1:
        die_with_head_complaint(
            '%d LLVM HEADs were found, which is more than one. You probably '
            'want a consistent set of HEADs for a profile. If you know you '
            "don't, please specify --llvm_hash, and note that *all* profiles "
            'will be merged into this final profile, regardless of their '
            'reported HEAD.' % len(heads))
      llvm_hash, = heads

    if llvm_hash not in heads:
      assert llvm_hash == args.llvm_hash
      die_with_head_complaint(
          "HEAD %s wasn't found in any fetched artifacts." % llvm_hash)

    print('Using LLVM hash: %s' % llvm_hash)

    _merge_profdata(profdata_list, args.output)
    print('Merged profdata locates at %s\n' % os.path.abspath(args.output))
    _tar_and_upload_profdata(args.output, name_suffix=llvm_hash)
    print('Merged profdata uploaded successfully.')
  except:
    success = False
    raise
  finally:
    os.chdir(initial_dir)
    if success:
      print('Clearing temp directory.')
      shutil.rmtree(temp_dir, ignore_errors=True)
    else:
      print('Script fails, temp directory is at: %s' % temp_dir)


if __name__ == '__main__':
  sys.exit(main())
