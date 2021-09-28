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

"""Mounts all the projects required by a selected Android target.

For details on how filesystem overlays work see the filesystem overlays
section of the README.md.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import collections
import os
import subprocess
import tempfile
import xml.etree.ElementTree as ET

BindMount = collections.namedtuple('BindMount', ['source_dir', 'readonly'])


class BindOverlay(object):
  """Manages filesystem overlays of Android source tree using bind mounts.
  """

  MAX_BIND_MOUNTS = 10000

  def _HideDir(self, target_dir):
    """Temporarily replace the target directory for an empty directory.

    Args:
      target_dir: A string path to the target directory.

    Returns:
      A string path to the empty directory that replaced the target directory.
    """
    empty_dir = tempfile.mkdtemp(prefix='empty_dir_')
    self._AddBindMount(empty_dir, target_dir)
    return empty_dir

  def _FindBindMountConflict(self, path):
    """Finds any path in the bind mounts that conflicts with the provided path.

    Args:
      path: A string path to be checked.

    Returns:
      A string of the conflicting path in the bind mounts.
      None if there was no conflict found.
    """
    conflict_path = None
    for bind_destination, bind_mount in self._bind_mounts.items():
      # Check if the path is a subdir or the bind destination
      if path == bind_destination:
        conflict_path = bind_mount.source_dir
        break
      elif path.startswith(bind_destination + os.sep):
        relative_path = os.path.relpath(path, bind_destination)
        path_in_source = os.path.join(bind_mount.source_dir, relative_path)
        if os.path.exists(path_in_source) and os.listdir(path_in_source):
          # A conflicting path exists within this bind mount
          # and it's not empty
          conflict_path = path_in_source
          break

    return conflict_path

  def _AddOverlay(self, overlay_dir, intermediate_work_dir, skip_subdirs,
                  destination_dir, rw_whitelist):
    """Adds a single overlay directory.

    Args:
      overlay_dir: A string path to the overlay directory to apply.
      intermediate_work_dir: A string path to the intermediate work directory used as the
        base for constructing the overlay filesystem.
      skip_subdirs: A set of string paths to skip from overlaying.
      destination_dir: A string with the path to the source with the overlays
        applied to it.
      rw_whitelist: An optional set of source paths to bind mount with
        read/write access.
    """
    # Traverse the overlay directory twice
    # The first pass only process git projects
    # The second time process all other files that are not in git projects

    # We need to process all git projects first because
    # the way we process a non-git directory will depend on if
    # it contains a git project in a subdirectory or not.

    dirs_with_git_projects = set('/')
    for current_dir_origin, subdirs, files in os.walk(overlay_dir):

      if current_dir_origin in skip_subdirs:
        del subdirs[:]
        continue

      current_dir_relative = os.path.relpath(current_dir_origin, overlay_dir)
      current_dir_destination = os.path.normpath(
        os.path.join(destination_dir, current_dir_relative))

      if '.git' in subdirs:
        # The current dir is a git project
        # so just bind mount it
        del subdirs[:]

        if rw_whitelist is None or current_dir_origin in rw_whitelist:
          self._AddBindMount(current_dir_origin, current_dir_destination, False)
        else:
          self._AddBindMount(current_dir_origin, current_dir_destination, True)

        current_dir_ancestor = current_dir_origin
        while current_dir_ancestor and current_dir_ancestor not in dirs_with_git_projects:
          dirs_with_git_projects.add(current_dir_ancestor)
          current_dir_ancestor = os.path.dirname(current_dir_ancestor)

    # Process all other files that are not in git projects
    for current_dir_origin, subdirs, files in os.walk(overlay_dir):

      if current_dir_origin in skip_subdirs:
        del subdirs[:]
        continue

      if '.git' in subdirs:
        del subdirs[:]
        continue

      current_dir_relative = os.path.relpath(current_dir_origin, overlay_dir)
      current_dir_destination = os.path.normpath(
        os.path.join(destination_dir, current_dir_relative))

      if current_dir_origin in dirs_with_git_projects:
        # Symbolic links to subdirectories
        # have to be copied to the intermediate work directory.
        # We can't bind mount them because bind mounts deference
        # symbolic links, and the build system filters out any
        # directory symbolic links.
        for subdir in subdirs:
          subdir_origin = os.path.join(current_dir_origin, subdir)
          if os.path.islink(subdir_origin):
            if subdir_origin not in skip_subdirs:
              subdir_destination = os.path.join(intermediate_work_dir,
                  current_dir_relative, subdir)
              self._CopyFile(subdir_origin, subdir_destination)

        # bind each file individually then keep travesting
        for file in files:
          file_origin = os.path.join(current_dir_origin, file)
          file_destination = os.path.join(current_dir_destination, file)
          if rw_whitelist is None or file_origin in rw_whitelist:
            self._AddBindMount(file_origin, file_destination, False)
          else:
            self._AddBindMount(file_origin, file_destination, True)

      else:
        # The current dir does not have any git projects to it can be bind
        # mounted wholesale
        del subdirs[:]
        if rw_whitelist is None or current_dir_origin in rw_whitelist:
          self._AddBindMount(current_dir_origin, current_dir_destination, False)
        else:
          self._AddBindMount(current_dir_origin, current_dir_destination, True)

  def _AddArtifactDirectories(self, source_dir, destination_dir, skip_subdirs):
    """Add directories that were not synced as workspace source.

    Args:
      source_dir: A string with the path to the Android platform source.
      destination_dir: A string with the path to the source where the overlays
        will be applied.
      skip_subdirs: A set of string paths to be skipped from overlays.

    Returns:
      A list of string paths to be skipped from overlaying.
    """

    # Ensure the main out directory exists
    main_out_dir = os.path.join(source_dir, 'out')
    if not os.path.exists(main_out_dir):
      os.makedirs(main_out_dir)

    for subdir in os.listdir(source_dir):
      if subdir.startswith('out'):
        out_origin = os.path.join(source_dir, subdir)
        if out_origin in skip_subdirs:
          continue
        out_destination = os.path.join(destination_dir, subdir)
        self._AddBindMount(out_origin, out_destination, False)
        skip_subdirs.add(out_origin)

    repo_origin = os.path.join(source_dir, '.repo')
    if os.path.exists(repo_origin):
      repo_destination = os.path.normpath(
        os.path.join(destination_dir, '.repo'))
      self._AddBindMount(repo_origin, repo_destination, False)
      skip_subdirs.add(repo_origin)

    return skip_subdirs

  def _AddOverlays(self, source_dir, overlay_dirs, destination_dir,
                   skip_subdirs, rw_whitelist):
    """Add the selected overlay directories.

    Args:
      source_dir: A string with the path to the Android platform source.
      overlay_dirs: A list of strings with the paths to the overlay
        directory to apply.
      destination_dir: A string with the path to the source where the overlays
        will be applied.
      skip_subdirs: A set of string paths to be skipped from overlays.
      rw_whitelist: An optional set of source paths to bind mount with
        read/write access.
    """

    # Create empty intermediate workdir
    intermediate_work_dir = self._HideDir(destination_dir)
    overlay_dirs.append(source_dir)

    skip_subdirs = self._AddArtifactDirectories(source_dir, destination_dir,
        skip_subdirs)


    # Bind mount each overlay directory using a
    # depth first traversal algorithm.
    #
    # The algorithm described works under the condition that the overlaid file
    # systems do not have conflicting projects.
    #
    # The results of attempting to overlay two git projects on top
    # of each other are unpredictable and may push the limits of bind mounts.

    skip_subdirs.add(os.path.join(source_dir, 'overlays'))

    for overlay_dir in overlay_dirs:
      self._AddOverlay(overlay_dir, intermediate_work_dir,
                       skip_subdirs, destination_dir, rw_whitelist)


  def _AddBindMount(self, source_dir, destination_dir, readonly=False):
    """Adds a bind mount for the specified directory.

    Args:
      source_dir: A string with the path of a source directory to bind.
        It must already exist.
      destination_dir: A string with the path ofa destination
        directory to bind the source into. If it does not exist,
        it will be created.
      readonly: A flag to indicate whether this path should be bind mounted
        with read-only access.
    """
    conflict_path = self._FindBindMountConflict(destination_dir)
    if conflict_path:
      raise ValueError("Project %s could not be overlaid at %s "
        "because it conflicts with %s"
        % (source_dir, destination_dir, conflict_path))

    if len(self._bind_mounts) >= self.MAX_BIND_MOUNTS:
      raise ValueError("Bind mount limit of %s reached" % self.MAX_BIND_MOUNTS)

    self._bind_mounts[destination_dir] = BindMount(
        source_dir=source_dir, readonly=readonly)

  def _CopyFile(self, source_path, dest_path):
    """Copies a file to the specified destination.

    Args:
      source_path: A string with the path of a source file to copy. It must
        exist.
      dest_path: A string with the path to copy the file to. It should not
        exist.
    """
    dest_dir = os.path.dirname(dest_path)
    if not os.path.exists(dest_dir):
      os.makedirs(dest_dir)
    subprocess.check_call(['cp', '--no-dereference', source_path, dest_path])

  def GetBindMounts(self):
    """Enumerates all bind mounts required by this Overlay.

    Returns:
      An ordered dict of BindMount objects keyed by destination path string.
      The order of the bind mounts does matter, this is why it's an ordered
      dict instead of a standard dict.
    """
    return self._bind_mounts

  def __init__(self,
               target,
               source_dir,
               config_file,
               whiteout_list = [],
               destination_dir=None,
               rw_whitelist=None):
    """Inits Overlay with the details of what is going to be overlaid.

    Args:
      target: A string with the name of the target to be prepared.
      source_dir: A string with the path to the Android platform source.
      config_file: A string path to the XML config file.
      whiteout_list: A list of directories to hide from the build system.
      destination_dir: A string with the path where the overlay filesystem
        will be created. If none is provided, the overlay filesystem
        will be applied directly on top of source_dir.
      rw_whitelist: An optional set of source paths to bind mount with
        read/write access. If none is provided, all paths will be mounted with
        read/write access. If the set is empty, all paths will be mounted
        read-only.
    """

    if not destination_dir:
      destination_dir = source_dir

    self._overlay_dirs = None
    # The order of the bind mounts does matter, this is why it's an ordered
    # dict instead of a standard dict.
    self._bind_mounts = collections.OrderedDict()

    # We will be repeateadly searching for items to skip so a set
    # seems appropriate
    skip_subdirs = set(whiteout_list)

    # The read/write whitelist provids paths relative to the source dir. It
    # needs to be updated with absolute paths to make lookup possible.
    if rw_whitelist:
      rw_whitelist = {os.path.join(source_dir, p) for p in rw_whitelist}

    overlay_dirs = []
    overlay_map = get_overlay_map(config_file)
    for overlay_dir in overlay_map[target]:
      overlay_dir = os.path.join(source_dir, 'overlays', overlay_dir)
      overlay_dirs.append(overlay_dir)

    self._AddOverlays(
        source_dir, overlay_dirs, destination_dir, skip_subdirs, rw_whitelist)

    # If specified for this target, create a custom filesystem view
    fs_view_map = get_fs_view_map(config_file)
    if target in fs_view_map:
      for path_relative_from, path_relative_to in fs_view_map[target]:
        path_from = os.path.join(source_dir, path_relative_from)
        if os.path.isfile(path_from) or os.path.isdir(path_from):
          path_to = os.path.join(destination_dir, path_relative_to)
          if rw_whitelist is None or path_from in rw_whitelist:
            self._AddBindMount(path_from, path_to, False)
          else:
            self._AddBindMount(path_from, path_to, True)
        else:
          raise ValueError("Path '%s' must be a file or directory" % path_from)

    self._overlay_dirs = overlay_dirs
    print('Applied overlays ' + ' '.join(self._overlay_dirs))

  def __del__(self):
    """Cleans up Overlay.
    """
    if self._overlay_dirs:
      print('Stripped out overlay ' + ' '.join(self._overlay_dirs))

def get_config(config_file):
  """Parses the overlay configuration file.

  Args:
    config_file: A string path to the XML config file.

  Returns:
    A root config XML Element.
    None if there is no config file.
  """
  config = None
  if os.path.exists(config_file):
    tree = ET.parse(config_file)
    config = tree.getroot()
  return config

def get_overlay_map(config_file):
  """Retrieves the map of overlays for each target.

  Args:
    config_file: A string path to the XML config file.

  Returns:
    A dict of keyed by target name. Each value in the
    dict is a list of overlay names corresponding to
    the target.
  """
  overlay_map = {}
  config = get_config(config_file)
  # The presence of the config file is optional
  if config:
    for target in config.findall('target'):
      name = target.get('name')
      overlay_list = [o.get('name') for o in target.findall('overlay')]
      overlay_map[name] = overlay_list
    # A valid configuration file is required
    # to have at least one overlay target
    if not overlay_map:
      raise ValueError('Error: the overlay configuration file '
          'is missing at least one overlay target')

  return overlay_map

def get_fs_view_map(config_file):
  """Retrieves the map of filesystem views for each target.

  Args:
    config_file: A string path to the XML config file.

  Returns:
    A dict of filesystem views keyed by target name.
    A filesystem view is a list of (source, destination)
    string path tuples.
  """
  fs_view_map = {}
  config = get_config(config_file)

  # The presence of the config file is optional
  if config:
    # A valid config file is not required to
    # include FS Views, only overlay targets
    views = {}
    for view in config.findall('view'):
      name = view.get('name')
      paths = []
      for path in view.findall('path'):
        paths.append((
              path.get('source'),
              path.get('destination')))
      views[name] = paths

    for target in config.findall('target'):
      target_name = target.get('name')
      view_paths = []
      for view in target.findall('view'):
        view_paths.extend(views[view.get('name')])

      if view_paths:
        fs_view_map[target_name] = view_paths

  return fs_view_map
