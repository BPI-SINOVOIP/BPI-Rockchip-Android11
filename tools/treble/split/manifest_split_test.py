# Copyright (C) 2020 The Android Open Source Project
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
"""Test manifest split."""

import hashlib
import mock
import os
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

import manifest_split


class ManifestSplitTest(unittest.TestCase):

  def test_read_config(self):
    with tempfile.NamedTemporaryFile('w+t') as test_config:
      test_config.write("""
        <config>
          <add_project name="add1" />
          <add_project name="add2" />
          <remove_project name="remove1" />
          <remove_project name="remove2" />
        </config>""")
      test_config.flush()
      remove_projects, add_projects = manifest_split.read_config(
          test_config.name)
      self.assertEqual(remove_projects, set(['remove1', 'remove2']))
      self.assertEqual(add_projects, set(['add1', 'add2']))

  def test_get_repo_projects(self):
    with tempfile.NamedTemporaryFile('w+t') as repo_list_file:
      repo_list_file.write("""
        system/project1 : platform/project1
        system/project2 : platform/project2""")
      repo_list_file.flush()
      repo_projects = manifest_split.get_repo_projects(repo_list_file.name)
      self.assertEqual(
          repo_projects, {
              'system/project1': 'platform/project1',
              'system/project2': 'platform/project2',
          })

  def test_get_module_info(self):
    with tempfile.NamedTemporaryFile('w+t') as module_info_file:
      module_info_file.write("""{
        "target1a": { "path": ["system/project1"] },
        "target1b": { "path": ["system/project1"] },
        "target2": { "path": ["out/project2"] },
        "target3": { "path": ["vendor/google/project3"] }
      }""")
      module_info_file.flush()
      repo_projects = {
          'system/project1': 'platform/project1',
          'vendor/google/project3': 'vendor/project3',
      }
      module_info = manifest_split.get_module_info(module_info_file.name,
                                                   repo_projects)
      self.assertEqual(
          module_info, {
              'platform/project1': set(['target1a', 'target1b']),
              'vendor/project3': set(['target3']),
          })

  def test_get_module_info_raises_on_unknown_module_path(self):
    with tempfile.NamedTemporaryFile('w+t') as module_info_file:
      module_info_file.write("""{
        "target1": { "path": ["system/unknown/project1"] }
      }""")
      module_info_file.flush()
      repo_projects = {}
      with self.assertRaisesRegex(ValueError,
                                  'Unknown module path for module target1'):
        manifest_split.get_module_info(module_info_file.name, repo_projects)

  @mock.patch.object(subprocess, 'check_output', autospec=True)
  def test_get_kati_makefiles(self, mock_check_output):
    with tempfile.TemporaryDirectory() as temp_dir:
      os.chdir(temp_dir)

      makefiles = [
          'device/oem1/product1.mk',
          'device/oem2/product2.mk',
          'device/google/google_product.mk',
          'overlays/oem_overlay/device/oem3/product3.mk',
          'packages/apps/Camera/Android.mk',
      ]
      for makefile in makefiles:
        os.makedirs(os.path.dirname(makefile))
        os.mknod(makefile)

      symlink_src = os.path.join(temp_dir, 'vendor/oem4/symlink_src.mk')
      os.makedirs(os.path.dirname(symlink_src))
      os.mknod(symlink_src)
      symlink_dest = 'device/oem4/symlink_dest.mk'
      os.makedirs(os.path.dirname(symlink_dest))
      os.symlink(symlink_src, symlink_dest)
      # Only append the symlink destination, not where the symlink points to.
      # (The Kati stamp file does not resolve symlink sources.)
      makefiles.append(symlink_dest)

      # Mock the output of ckati_stamp_dump:
      mock_check_output.side_effect = [
          '\n'.join(makefiles).encode(),
      ]

      kati_makefiles = manifest_split.get_kati_makefiles(
          'stamp-file', ['overlays/oem_overlay/'])
      self.assertEqual(
          kati_makefiles,
          set([
              # Regular product makefiles
              'device/oem1/product1.mk',
              'device/oem2/product2.mk',
              # Product makefile remapped from an overlay
              'device/oem3/product3.mk',
              # Product makefile symlink and its source
              'device/oem4/symlink_dest.mk',
              'vendor/oem4/symlink_src.mk',
          ]))

  def test_scan_repo_projects(self):
    repo_projects = {
        'system/project1': 'platform/project1',
        'system/project2': 'platform/project2',
    }
    self.assertEqual(
        manifest_split.scan_repo_projects(repo_projects,
                                          'system/project1/path/to/file.h'),
        'system/project1')
    self.assertEqual(
        manifest_split.scan_repo_projects(
            repo_projects, 'system/project2/path/to/another_file.cc'),
        'system/project2')
    self.assertIsNone(
        manifest_split.scan_repo_projects(
            repo_projects, 'system/project3/path/to/unknown_file.h'))

  def test_get_input_projects(self):
    repo_projects = {
        'system/project1': 'platform/project1',
        'system/project2': 'platform/project2',
        'system/project4': 'platform/project4',
    }
    inputs = [
        'system/project1/path/to/file.h',
        'out/path/to/out/file.h',
        'system/project2/path/to/another_file.cc',
        'system/project3/path/to/unknown_file.h',
        '/tmp/absolute/path/file.java',
    ]
    self.assertEqual(
        manifest_split.get_input_projects(repo_projects, inputs),
        set(['platform/project1', 'platform/project2']))

  def test_update_manifest(self):
    manifest_contents = """
      <manifest>
        <project name="platform/project1" path="system/project1" />
        <project name="platform/project2" path="system/project2" />
        <project name="platform/project3" path="system/project3" />
      </manifest>"""
    input_projects = set(['platform/project1', 'platform/project3'])
    remove_projects = set(['platform/project3'])
    manifest = manifest_split.update_manifest(
        ET.ElementTree(ET.fromstring(manifest_contents)), input_projects,
        remove_projects)

    projects = manifest.getroot().findall('project')
    self.assertEqual(len(projects), 1)
    self.assertEqual(
        ET.tostring(projects[0]).strip().decode(),
        '<project name="platform/project1" path="system/project1" />')

  def test_create_manifest_sha1_element(self):
    manifest = ET.ElementTree(ET.fromstring('<manifest></manifest>'))
    manifest_sha1 = hashlib.sha1(ET.tostring(manifest.getroot())).hexdigest()
    self.assertEqual(
        ET.tostring(
            manifest_split.create_manifest_sha1_element(
                manifest, 'test_manifest')).decode(),
        '<hash name="test_manifest" type="sha1" value="%s" />' % manifest_sha1)

  @mock.patch.object(subprocess, 'check_output', autospec=True)
  def test_create_split_manifest(self, mock_check_output):
    with tempfile.NamedTemporaryFile('w+t') as repo_list_file, \
      tempfile.NamedTemporaryFile('w+t') as manifest_file, \
      tempfile.NamedTemporaryFile('w+t') as module_info_file, \
      tempfile.NamedTemporaryFile('w+t') as config_file, \
      tempfile.NamedTemporaryFile('w+t') as split_manifest_file:

      repo_list_file.write("""
        system/project1 : platform/project1
        system/project2 : platform/project2
        system/project3 : platform/project3
        system/project4 : platform/project4
        system/project5 : platform/project5
        system/project6 : platform/project6""")
      repo_list_file.flush()

      manifest_file.write("""
        <manifest>
          <project name="platform/project1" path="system/project1" />
          <project name="platform/project2" path="system/project2" />
          <project name="platform/project3" path="system/project3" />
          <project name="platform/project4" path="system/project4" />
          <project name="platform/project5" path="system/project5" />
          <project name="platform/project6" path="system/project6" />
        </manifest>""")
      manifest_file.flush()

      module_info_file.write("""{
        "droid": { "path": ["system/project1"] },
        "target_a": { "path": ["out/project2"] },
        "target_b": { "path": ["system/project3"] },
        "target_c": { "path": ["system/project4"] },
        "target_d": { "path": ["system/project5"] },
        "target_e": { "path": ["system/project6"] }
      }""")
      module_info_file.flush()

      # droid needs inputs from project1 and project3
      ninja_inputs_droid = b"""
      system/project1/file1
      system/project1/file2
      system/project3/file1
      """

      # target_b (indirectly included due to being in project3) needs inputs
      # from project3 and project4
      ninja_inputs_target_b = b"""
      system/project3/file2
      system/project4/file1
      """

      # target_c (indirectly included due to being in project4) needs inputs
      # from only project4
      ninja_inputs_target_c = b"""
      system/project4/file2
      system/project4/file3
      """

      mock_check_output.side_effect = [
          ninja_inputs_droid,
          b'',  # Unused kati makefiles. This is tested in its own method.
          ninja_inputs_target_b,
          ninja_inputs_target_c,
      ]

      # The config file says to manually include project6
      config_file.write("""
        <config>
          <add_project name="platform/project6" />
        </config>""")
      config_file.flush()

      manifest_split.create_split_manifest(
          ['droid'], manifest_file.name, split_manifest_file.name,
          [config_file.name], repo_list_file.name, 'build-target.ninja',
          'ninja', module_info_file.name, 'unused kati stamp',
          ['unused overlay'])
      split_manifest = ET.parse(split_manifest_file.name)
      split_manifest_projects = [
          child.attrib['name']
          for child in split_manifest.getroot().findall('project')
      ]
      self.assertEqual(
          split_manifest_projects,
          [
              # From droid
              'platform/project1',
              # From droid
              'platform/project3',
              # From target_b (module within project3, indirect dependency)
              'platform/project4',
              # Manual inclusion from config file
              'platform/project6',
          ])


if __name__ == '__main__':
  unittest.main()
