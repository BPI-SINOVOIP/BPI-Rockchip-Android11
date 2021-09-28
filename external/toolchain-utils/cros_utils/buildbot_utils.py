# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for launching and accessing ChromeOS buildbots."""

from __future__ import division
from __future__ import print_function

import ast
import json
import os
import re
import time

from cros_utils import command_executer
from cros_utils import logger

INITIAL_SLEEP_TIME = 7200  # 2 hours; wait time before polling buildbot.
SLEEP_TIME = 600  # 10 minutes; time between polling of buildbot.

# Some of our slower builders (llvm-next) are taking more
# than 11 hours. So, increase this TIME_OUT to 12 hours.
TIME_OUT = 43200  # Decide the build is dead or will never finish


class BuildbotTimeout(Exception):
  """Exception to throw when a buildbot operation timesout."""
  pass


def RunCommandInPath(path, cmd):
  ce = command_executer.GetCommandExecuter()
  cwd = os.getcwd()
  os.chdir(path)
  status, stdout, stderr = ce.RunCommandWOutput(cmd, print_to_console=False)
  os.chdir(cwd)
  return status, stdout, stderr


def PeekTrybotImage(chromeos_root, buildbucket_id):
  """Get the artifact URL of a given tryjob.

  Args:
    buildbucket_id: buildbucket-id
    chromeos_root: root dir of chrome os checkout

  Returns:
    (status, url) where status can be 'pass', 'fail', 'running',
                  and url looks like:
    gs://chromeos-image-archive/trybot-elm-release-tryjob/R67-10468.0.0-b20789
  """
  command = (
      'cros buildresult --report json --buildbucket-id %s' % buildbucket_id)
  rc, out, _ = RunCommandInPath(chromeos_root, command)

  # Current implementation of cros buildresult returns fail when a job is still
  # running.
  if rc != 0:
    return ('running', None)

  results = json.loads(out)[buildbucket_id]

  # Handle the case where the tryjob failed to launch correctly.
  if results['artifacts_url'] is None:
    return (results['status'], '')

  return (results['status'], results['artifacts_url'].rstrip('/'))


def ParseTryjobBuildbucketId(msg):
  """Find the buildbucket-id in the messages from `cros tryjob`.

  Args:
    msg: messages from `cros tryjob`

  Returns:
    buildbucket-id, which will be passed to `cros buildresult`
  """
  output_list = ast.literal_eval(msg)
  output_dict = output_list[0]
  if 'buildbucket_id' in output_dict:
    return output_dict['buildbucket_id']
  return None


def SubmitTryjob(chromeos_root,
                 buildbot_name,
                 patch_list,
                 tryjob_flags=None,
                 build_toolchain=False):
  """Calls `cros tryjob ...`

  Args:
    chromeos_root: the path to the ChromeOS root, needed for finding chromite
                   and launching the buildbot.
    buildbot_name: the name of the buildbot queue, such as lumpy-release or
                   daisy-paladin.
    patch_list: a python list of the patches, if any, for the buildbot to use.
    tryjob_flags: See cros tryjob --help for available options.
    build_toolchain: builds and uses the latest toolchain, rather than the
                     prebuilt one in SDK.

  Returns:
    buildbucket id
  """
  patch_arg = ''
  if patch_list:
    for p in patch_list:
      patch_arg = patch_arg + ' -g ' + repr(p)
  if not tryjob_flags:
    tryjob_flags = []
  if build_toolchain:
    tryjob_flags.append('--latest-toolchain')
  tryjob_flags = ' '.join(tryjob_flags)

  # Launch buildbot with appropriate flags.
  build = buildbot_name
  command = ('cros_sdk -- cros tryjob --yes --json --nochromesdk  %s %s %s' %
             (tryjob_flags, patch_arg, build))
  print('CMD: %s' % command)
  _, out, _ = RunCommandInPath(chromeos_root, command)
  buildbucket_id = ParseTryjobBuildbucketId(out)
  print('buildbucket_id: %s' % repr(buildbucket_id))
  if not buildbucket_id:
    logger.GetLogger().LogFatal('Error occurred while launching trybot job: '
                                '%s' % command)
  return buildbucket_id


def GetTrybotImage(chromeos_root,
                   buildbot_name,
                   patch_list,
                   tryjob_flags=None,
                   build_toolchain=False,
                   asynchronous=False):
  """Launch buildbot and get resulting trybot artifact name.

  This function launches a buildbot with the appropriate flags to
  build the test ChromeOS image, with the current ToT mobile compiler.  It
  checks every 10 minutes to see if the trybot has finished.  When the trybot
  has finished, it parses the resulting report logs to find the trybot
  artifact (if one was created), and returns that artifact name.

  Args:
    chromeos_root: the path to the ChromeOS root, needed for finding chromite
                   and launching the buildbot.
    buildbot_name: the name of the buildbot queue, such as lumpy-release or
                   daisy-paladin.
    patch_list: a python list of the patches, if any, for the buildbot to use.
    tryjob_flags: See cros tryjob --help for available options.
    build_toolchain: builds and uses the latest toolchain, rather than the
                     prebuilt one in SDK.
    asynchronous: don't wait for artifacts; just return the buildbucket id

  Returns:
    (buildbucket id, partial image url) e.g.
    (8952271933586980528, trybot-elm-release-tryjob/R67-10480.0.0-b2373596)
  """
  buildbucket_id = SubmitTryjob(chromeos_root, buildbot_name, patch_list,
                                tryjob_flags, build_toolchain)
  if asynchronous:
    return buildbucket_id, ' '

  # The trybot generally takes more than 2 hours to finish.
  # Wait two hours before polling the status.
  time.sleep(INITIAL_SLEEP_TIME)
  elapsed = INITIAL_SLEEP_TIME
  status = 'running'
  image = ''
  while True:
    status, image = PeekTrybotImage(chromeos_root, buildbucket_id)
    if status == 'running':
      if elapsed > TIME_OUT:
        logger.GetLogger().LogFatal(
            'Unable to get build result for target %s.' % buildbot_name)
      else:
        wait_msg = 'Unable to find build result; job may be running.'
        logger.GetLogger().LogOutput(wait_msg)
      logger.GetLogger().LogOutput('{0} minutes elapsed.'.format(elapsed / 60))
      logger.GetLogger().LogOutput('Sleeping {0} seconds.'.format(SLEEP_TIME))
      time.sleep(SLEEP_TIME)
      elapsed += SLEEP_TIME
    else:
      break

  if not buildbot_name.endswith('-toolchain') and status == 'fail':
    # For rotating testers, we don't care about their status
    # result, because if any HWTest failed it will be non-zero.
    #
    # The nightly performance tests do not run HWTests, so if
    # their status is non-zero, we do care.  In this case
    # non-zero means the image itself probably did not build.
    image = ''

  if not image:
    logger.GetLogger().LogError(
        'Trybot job (buildbucket id: %s) failed with'
        'status %s; no trybot image generated. ' % (buildbucket_id, status))
  else:
    # Convert full gs path to what crosperf expects. For example, convert
    # gs://chromeos-image-archive/trybot-elm-release-tryjob/R67-10468.0.0-b20789
    # to
    # trybot-elm-release-tryjob/R67-10468.0.0-b20789
    image = '/'.join(image.split('/')[-2:])

  logger.GetLogger().LogOutput("image is '%s'" % image)
  logger.GetLogger().LogOutput('status is %s' % status)
  return buildbucket_id, image


def DoesImageExist(chromeos_root, build):
  """Check if the image for the given build exists."""

  ce = command_executer.GetCommandExecuter()
  command = ('gsutil ls gs://chromeos-image-archive/%s'
             '/chromiumos_test_image.tar.xz' % (build))
  ret = ce.ChrootRunCommand(chromeos_root, command, print_to_console=False)
  return not ret


def WaitForImage(chromeos_root, build):
  """Wait for an image to be ready."""

  elapsed_time = 0
  while elapsed_time < TIME_OUT:
    if DoesImageExist(chromeos_root, build):
      return
    logger.GetLogger().LogOutput(
        'Image %s not ready, waiting for 10 minutes' % build)
    time.sleep(SLEEP_TIME)
    elapsed_time += SLEEP_TIME

  logger.GetLogger().LogOutput(
      'Image %s not found, waited for %d hours' % (build, (TIME_OUT / 3600)))
  raise BuildbotTimeout('Timeout while waiting for image %s' % build)


def GetLatestImage(chromeos_root, path):
  """Get latest image"""

  fmt = re.compile(r'R([0-9]+)-([0-9]+).([0-9]+).([0-9]+)')

  ce = command_executer.GetCommandExecuter()
  command = ('gsutil ls gs://chromeos-image-archive/%s' % path)
  _, out, _ = ce.ChrootRunCommandWOutput(
      chromeos_root, command, print_to_console=False)
  candidates = [l.split('/')[-2] for l in out.split()]
  candidates = [fmt.match(c) for c in candidates]
  candidates = [[int(r) for r in m.group(1, 2, 3, 4)] for m in candidates if m]
  candidates.sort(reverse=True)
  for c in candidates:
    build = '%s/R%d-%d.%d.%d' % (path, c[0], c[1], c[2], c[3])
    # Blacklist "R79-12384.0.0" image released by mistake.
    # TODO(crbug.com/992242): Remove the filter by 2019-09-05.
    if c == [79, 12384, 0, 0]:
      continue

    if DoesImageExist(chromeos_root, build):
      return build
