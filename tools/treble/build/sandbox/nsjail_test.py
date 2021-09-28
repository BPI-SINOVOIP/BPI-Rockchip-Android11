# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Test nsjail."""

import os
import subprocess
import tempfile
import unittest
from . import nsjail


class NsjailTest(unittest.TestCase):

  def setUp(self):
    nsjail.__file__ = '/'

  def testMinimalParameters(self):
    commands = nsjail.run(
        nsjail_bin='/bin/true',
        chroot='/chroot',
        source_dir='/source_dir',
        command=['/bin/bash'],
        android_target='target_name',
        dry_run=True)
    self.assertEqual(
        commands,
        [
            '/bin/true',
            '--env', 'USER=nobody',
            '--config', '/nsjail.cfg',
            '--bindmount', '/source_dir:/src',
            '--', '/bin/bash'
        ]
    )

  def testSetBadMetaAndroidDir(self):
    os.chdir('/')
    with self.assertRaises(ValueError):
      commands = nsjail.run(
        nsjail_bin='/bin/true',
        chroot='/chroot',
        source_dir='/source_dir',
        command=['/bin/bash'],
        android_target='target_name',
        dry_run=True,
        meta_root_dir='/meta/dir',
        meta_android_dir='/android/dir')

  def testRedirectStdout(self):
    with tempfile.TemporaryFile('w+t') as out:
      nsjail.run(
          nsjail_bin='/bin/echo',
          chroot='/chroot',
          source_dir='/source_dir',
          command=['/bin/bash'],
          android_target='target_name',
          stdout=out)
      out.seek(0)
      stdout = out.read()
      args = ('--env USER=nobody --config /nsjail.cfg '
              '--bindmount /source_dir:/src -- /bin/bash')
      expected = '\n'.join([args, 'NsJail command:', '/bin/echo '+args])+'\n'
      self.assertEqual(stdout, expected)

  def testFailingJailedCommand(self):
    with self.assertRaises(subprocess.CalledProcessError):
      nsjail.run(
          nsjail_bin='/bin/false',
          chroot='/chroot',
          source_dir='/source_dir',
          command=['/bin/bash'],
          android_target='target_name')

  def testDist(self):
    commands = nsjail.run(
        nsjail_bin='/bin/true',
        chroot='/chroot',
        source_dir='/source_dir',
        command=['/bin/bash'],
        android_target='target_name',
        dist_dir='/dist_dir',
        dry_run=True)
    self.assertEqual(
        commands,
        [
            '/bin/true',
            '--env', 'USER=nobody',
            '--config', '/nsjail.cfg',
            '--env', 'DIST_DIR=/dist',
            '--bindmount', '/source_dir:/src',
            '--bindmount', '/dist_dir:/dist',
            '--', '/bin/bash'
        ]
    )

  def testBuildID(self):
    commands = nsjail.run(
        nsjail_bin='/bin/true',
        chroot='/chroot',
        source_dir='/source_dir',
        command=['/bin/bash'],
        android_target='target_name',
        build_id='0',
        dry_run=True)
    self.assertEqual(
        commands,
        [
            '/bin/true',
            '--env', 'USER=nobody',
            '--config', '/nsjail.cfg',
            '--env', 'BUILD_NUMBER=0',
            '--bindmount', '/source_dir:/src',
            '--', '/bin/bash'
        ]
    )

  def testMaxCPU(self):
    commands = nsjail.run(
        nsjail_bin='/bin/true',
        chroot='/chroot',
        source_dir='/source_dir',
        command=['/bin/bash'],
        android_target='target_name',
        max_cpus=1,
        dry_run=True)
    self.assertEqual(
        commands,
        [
            '/bin/true',
            '--env', 'USER=nobody',
            '--config', '/nsjail.cfg',
            '--max_cpus=1',
            '--bindmount', '/source_dir:/src',
            '--', '/bin/bash'
        ]
    )

  def testEnv(self):
    commands = nsjail.run(
        nsjail_bin='/bin/true',
        chroot='/chroot',
        source_dir='/source_dir',
        command=['/bin/bash'],
        android_target='target_name',
        max_cpus=1,
        dry_run=True,
        env=['foo=bar', 'spam=eggs'])
    self.assertEqual(
        commands,
        [
            '/bin/true',
            '--env', 'USER=nobody',
            '--config', '/nsjail.cfg',
            '--max_cpus=1',
            '--bindmount', '/source_dir:/src',
            '--env', 'foo=bar', '--env', 'spam=eggs',
            '--', '/bin/bash'
        ]
    )


if __name__ == '__main__':
  unittest.main()
