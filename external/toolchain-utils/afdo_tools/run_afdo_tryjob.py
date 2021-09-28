#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Spawns off an AFDO tryjob.

This tryjob will cause perf profiles to be collected as though we were running
our benchmark AFDO pipeline. Depending on the set of flags that you use,
different things will happen. Any artifacts will land in
gs://chromeos-localmirror/distfiles/afdo/experimental/approximation

This tryjob will generate *either* a full (AFDO profile, perf.data,
chrome.debug) combo, or just a perf.data, depending on the arguments you feed
it.

The thing to be careful of is that our localmirror bucket is shared between
everyone, so it's super easy for two AFDO profile runs to 'collide'. Hence, if
you provide the --tag_profiles_with_current_time flag, the script will generate
*only* a perf.data, but that perf.data will have a timestamp (with second
resolution) on it. This makes collisions super unlikely.

If you'd like to know which perf profile was yours:
  - Go to the tryjob output page
  - Look for 'HWTest [AFDO_Record]'
  - Click on its stdout
  - Find "Links to test logs:" in the stdout
  - Follow the link by telemetry_AFDOGenerate
  - Find and click the link to debug/autoserv.DEBUG
  - Look for a gs:// link ending in `.perf.data` with a compression suffix
    (currently `.bz2`; maybe `.xz` eventually). That's the gs:// path to your
    perf profile.

The downside to this option is that there's no (reliable + trivial to
implement) way for the bot that converts AFDO profiles into perf profiles to
know the profile to choose. So, you're stuck generating a profile on your own.
We have a tool for just that. Please see `generate_afdo_from_tryjob.py`.

If you don't like that tool, generating your own profile isn't super difficult.
Just grab the perf profile that your logs note from gs://, grab a copy of
chrome.debug from your tryjob, and use `create_llvm_prof` to create a profile.

On the other hand, if you're 100% sure that your profile won't collide, you can
make your life easier by providing --use_afdo_generation_stage.

If you provide neither --use_afdo_generation_stage nor
--tag_profiles_with_current_time, --tag_profiles_with_current_time is implied,
since it's safer.
"""

from __future__ import print_function

import argparse
import collections
import pipes
import subprocess
import sys
import time


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      '--force_no_patches',
      action='store_true',
      help='Run even if no patches are provided')
  parser.add_argument(
      '--tag_profiles_with_current_time',
      action='store_true',
      help='Perf profile names will have the current time added to them.')
  parser.add_argument(
      '--use_afdo_generation_stage',
      action='store_true',
      help='Perf profiles will be automatically converted to AFDO profiles.')
  parser.add_argument(
      '-g',
      '--patch',
      action='append',
      default=[],
      help='A patch to add to the AFDO run')
  parser.add_argument(
      '-n',
      '--dry_run',
      action='store_true',
      help='Just print the command that would be run')
  args = parser.parse_args()

  dry_run = args.dry_run
  force_no_patches = args.force_no_patches
  tag_profiles_with_current_time = args.tag_profiles_with_current_time
  use_afdo_generation_stage = args.use_afdo_generation_stage
  user_patches = args.patch

  if tag_profiles_with_current_time and use_afdo_generation_stage:
    raise ValueError("You can't tag profiles with the time + have "
                     'afdo-generate')

  if not tag_profiles_with_current_time and not use_afdo_generation_stage:
    print('Neither current_time nor afdo_generate asked for. Assuming you '
          'prefer current time tagging.')
    print('You have 5 seconds to cancel and try again.')
    print()
    if not dry_run:
      time.sleep(5)
    tag_profiles_with_current_time = True

  patches = [
      # Send profiles to localmirror instead of chromeos-prebuilt. This should
      # always be done, since sending profiles into production is bad. :)
      # https://chromium-review.googlesource.com/c/chromiumos/third_party/autotest/+/1436158
      1436158,
      # Force profile generation. Otherwise, we'll decide to not spawn off the
      # perf hwtests.
      # https://chromium-review.googlesource.com/c/chromiumos/chromite/+/1313291
      1313291,
  ]

  if tag_profiles_with_current_time:
    # Tags the profiles with the current time of day. As detailed in the
    # docstring, this is desirable unless you're sure that this is the only
    # experimental profile that will be generated today.
    # https://chromium-review.googlesource.com/c/chromiumos/third_party/autotest/+/1436157
    patches.append(1436157)

  if use_afdo_generation_stage:
    # Make the profile generation stage look in localmirror, instead of having
    # it look in chromeos-prebuilt. Without this, we'll never upload
    # chrome.debug or try to generate an AFDO profile.
    # https://chromium-review.googlesource.com/c/chromiumos/chromite/+/1436583
    patches.append(1436583)

  if not user_patches and not force_no_patches:
    raise ValueError('No patches given; pass --force_no_patches to force a '
                     'tryjob')

  for patch in user_patches:
    # We accept two formats. Either a URL that ends with a number, or a number.
    if patch.startswith('http'):
      patch = patch.split('/')[-1]
    patches.append(int(patch))

  count = collections.Counter(patches)
  too_many = [k for k, v in count.items() if v > 1]
  if too_many:
    too_many.sort()
    raise ValueError(
        'Patch(es) asked for application more than once: %s' % too_many)

  args = [
      'cros',
      'tryjob',
  ]

  for patch in patches:
    args += ['-g', str(patch)]

  args += [
      '--nochromesdk',
      '--hwtest',
      'chell-chrome-pfq-tryjob',
  ]

  print(' '.join(pipes.quote(a) for a in args))
  if not dry_run:
    sys.exit(subprocess.call(args))


if __name__ == '__main__':
  main()
