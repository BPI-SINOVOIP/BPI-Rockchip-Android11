# Copyright (C) 2018 The Android Open Source Project
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
"""Tool functions to deal with files."""

import datetime
import os

from google.protobuf import text_format    # pylint: disable=import-error

import metadata_pb2    # pylint: disable=import-error

ANDROID_TOP = os.environ.get('ANDROID_BUILD_TOP', os.getcwd())
EXTERNAL_PATH = os.path.join(ANDROID_TOP, 'external/')

METADATA_FILENAME = 'METADATA'


def get_absolute_project_path(project_path):
    """Gets absolute path of a project.

    Path resolution starts from external/.
    """
    return os.path.join(EXTERNAL_PATH, project_path)


def get_metadata_path(project_path):
    """Gets the absolute path of METADATA for a project."""
    return os.path.join(
        get_absolute_project_path(project_path), METADATA_FILENAME)


def get_relative_project_path(project_path):
    """Gets the relative path of a project starting from external/."""
    project_path = get_absolute_project_path(project_path)
    return os.path.relpath(project_path, EXTERNAL_PATH)


def read_metadata(proj_path):
    """Reads and parses METADATA file for a project.

    Args:
      proj_path: Path to the project.

    Returns:
      Parsed MetaData proto.

    Raises:
      text_format.ParseError: Occurred when the METADATA file is invalid.
      FileNotFoundError: Occurred when METADATA file is not found.
    """

    with open(get_metadata_path(proj_path), 'r') as metadata_file:
        metadata = metadata_file.read()
        return text_format.Parse(metadata, metadata_pb2.MetaData())


def write_metadata(proj_path, metadata):
    """Writes updated METADATA file for a project.

    This function updates last_upgrade_date in metadata and write to the project
    directory.

    Args:
      proj_path: Path to the project.
      metadata: The MetaData proto to write.
    """

    date = metadata.third_party.last_upgrade_date
    now = datetime.datetime.now()
    date.year = now.year
    date.month = now.month
    date.day = now.day
    text_metadata = text_format.MessageToString(metadata)
    with open(get_metadata_path(proj_path), 'w') as metadata_file:
        metadata_file.write(text_metadata)
