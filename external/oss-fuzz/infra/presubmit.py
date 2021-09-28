#!/usr/bin/env python3
# Copyright 2020 Google LLC.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
"""Check code for common issues before submitting."""

import argparse
import os
import subprocess
import sys
import yaml

_SRC_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _is_project_file(actual_path, expected_filename):
  """Returns True if actual_path's name is |expected_filename| and is a file
  that exists and is in in projects/."""
  if os.path.basename(actual_path) != expected_filename:
    return False

  if os.path.basename(os.path.dirname(
      os.path.dirname(actual_path))) != 'projects':
    return False

  return os.path.exists(actual_path)


# TODO: Check for -fsanitize=fuzzer in files as well.


def _check_one_lib_fuzzing_engine(build_sh_file):
  """Returns False if |build_sh_file| contains -lFuzzingEngine.
  This is deprecated behavior. $LIB_FUZZING_ENGINE should be used instead
  so that -fsanitize=fuzzer is used."""
  if not _is_project_file(build_sh_file, 'build.sh'):
    return True

  with open(build_sh_file) as build_sh:
    build_sh_lines = build_sh.readlines()
  for line_num, line in enumerate(build_sh_lines):
    uncommented_code = line.split('#')[0]
    if '-lFuzzingEngine' in uncommented_code:
      print(
          'Error: build.sh contains deprecated "-lFuzzingEngine" on line: {0}. '
          'Please use "$LIB_FUZZING_ENGINE" instead.'.format(line_num))
      return False
  return True


def check_lib_fuzzing_engine(paths):
  """Call _check_one_lib_fuzzing_engine on each path in |paths|. Return True if
  the result of every call is True."""
  return all([_check_one_lib_fuzzing_engine(path) for path in paths])


class ProjectYamlChecker:
  """Checks for a project.yaml file."""

  # Sections in a project.yaml and the constant values that they are allowed
  # to have.
  SECTIONS_AND_CONSTANTS = {
      'sanitizers': {'address', 'none', 'memory', 'undefined', 'dataflow'},
      'architectures': {'i386', 'x86_64'},
      'fuzzing_engines': {'afl', 'libfuzzer', 'honggfuzz', 'dataflow'},
  }

  # Note: this list must be updated when we allow new sections.
  VALID_SECTION_NAMES = [
      'architectures',
      'auto_ccs',
      'coverage_extra_args',
      'disabled',
      'fuzzing_engines',
      'homepage',
      'primary_contact',
      'sanitizers',
      'vendor_ccs',
      'view_restrictions',
      'language',
  ]

  LANGUAGES_SUPPORTED = ['c', 'cpp', 'go', 'rust', 'python']

  # Note that some projects like boost only have auto-ccs. However, forgetting
  # primary contact is probably a mistake.
  REQUIRED_SECTIONS = ['primary_contact']

  def __init__(self, filename):
    self.filename = filename
    with open(filename) as file_handle:
      self.data = yaml.safe_load(file_handle)

    self.success = True

  def do_checks(self):
    """Do all project.yaml checks. Return True if they pass."""
    if self.is_disabled():
      return True

    checks = [
        self.check_project_yaml_constants,
        self.check_required_sections,
        self.check_valid_section_names,
        self.check_valid_emails,
        self.check_valid_language,
    ]
    for check_function in checks:
      check_function()
    return self.success

  def is_disabled(self):
    """Is this project disabled."""
    return self.data.get('disabled', False)

  def error(self, message):
    """Print an error message and set self.success to False."""
    self.success = False
    print('Error in {filename}: {message}'.format(filename=self.filename,
                                                  message=message))

  def check_project_yaml_constants(self):
    """Check that certain sections only have certain constant values."""
    for section, allowed_constants in self.SECTIONS_AND_CONSTANTS.items():
      if section not in self.data:
        continue
      actual_constants = self.data[section]
      for constant in actual_constants:
        if isinstance(constant, str):
          if constant not in allowed_constants:
            self.error(('{constant} (in {section} section) is not a valid '
                        'constant ({allowed_constants}).').format(
                            constant=constant,
                            section=section,
                            allowed_constants=', '.join(allowed_constants)))
        elif isinstance(constant, dict):
          # The only alternative value allowed is the experimental flag, i.e.
          # `constant == {'memory': {'experimental': True}}`. Do not check the
          # experimental flag, but assert that the sanitizer is a valid one.
          if (len(constant.keys()) > 1 or
              list(constant.keys())[0] not in allowed_constants):
            self.error('Not allowed value in the project.yaml: ' +
                       str(constant))
        else:
          self.error('Not allowed value in the project.yaml: ' + str(constant))

  def check_valid_section_names(self):
    """Check that only valid sections are included."""
    for name in self.data:
      if name not in self.VALID_SECTION_NAMES:
        self.error('{name} is not a valid section name ({valid_names})'.format(
            name=name, valid_names=self.VALID_SECTION_NAMES))

  def check_required_sections(self):
    """Check that all required sections are present."""
    for section in self.REQUIRED_SECTIONS:
      if section not in self.data:
        self.error(section + ' section is missing.')

  def check_valid_emails(self):
    """Check that emails are valid looking."""
    # Get email addresses.
    email_addresses = []
    primary_contact = self.data.get('primary_contact')
    if primary_contact:
      email_addresses.append(primary_contact)
    auto_ccs = self.data.get('auto_ccs')
    if auto_ccs:
      email_addresses.extend(auto_ccs)

    # Sanity check them.
    for email_address in email_addresses:
      if '@' not in email_address or '.' not in email_address:
        self.error(email_address + ' is an invalid email address.')

  def check_valid_language(self):
    """Check that the language specified is valid."""
    language = self.data.get('language')
    if not language:
      return

    if language not in self.LANGUAGES_SUPPORTED:
      self.error('{language} is not supported ({supported}).'.format(
          language=language, supported=self.LANGUAGES_SUPPORTED))


def _check_one_project_yaml(project_yaml_filename):
  """Do checks on the project.yaml file."""
  if not _is_project_file(project_yaml_filename, 'project.yaml'):
    return True

  checker = ProjectYamlChecker(project_yaml_filename)
  return checker.do_checks()


def check_project_yaml(paths):
  """Call _check_one_project_yaml on each path in |paths|. Return True if
  the result of every call is True."""
  return all([_check_one_project_yaml(path) for path in paths])


def do_checks(changed_files):
  """Run all presubmit checks return False if any fails."""
  checks = [
      check_license, yapf, lint, check_project_yaml, check_lib_fuzzing_engine
  ]
  # Use a list comprehension here and in other cases where we use all() so that
  # we don't quit early on failure. This is more user-friendly since the more
  # errors we spit out at once, the less frequently the less check-fix-check
  # cycles they need to do.
  return all([check(changed_files) for check in checks])


_CHECK_LICENSE_FILENAMES = ['Dockerfile']
_CHECK_LICENSE_EXTENSIONS = [
    '.bash',
    '.c',
    '.cc',
    '.cpp',
    '.css',
    '.h',
    '.htm',
    '.html',
    '.js',
    '.proto',
    '.py',
    '.sh',
]

_LICENSE_STRING = 'http://www.apache.org/licenses/LICENSE-2.0'


def check_license(paths):
  """Validate license header."""
  if not paths:
    return True

  success = True
  for path in paths:
    filename = os.path.basename(path)
    extension = os.path.splitext(path)[1]
    if (filename not in _CHECK_LICENSE_FILENAMES and
        extension not in _CHECK_LICENSE_EXTENSIONS):
      continue

    with open(path) as file_handle:
      if _LICENSE_STRING not in file_handle.read():
        print('Missing license header in file %s.' % str(path))
        success = False

  return success


def bool_to_returncode(success):
  """Return 0 if |success|. Otherwise return 1."""
  if success:
    print('Success.')
    return 0

  print('Failed.')
  return 1


def is_python(path):
  """Returns True if |path| ends in .py."""
  return os.path.splitext(path)[1] == '.py'


def lint(paths):
  """Run python's linter on |paths| if it is a python file. Return False if it
  fails linting."""
  paths = [path for path in paths if is_python(path)]
  if not paths:
    return True

  command = ['python3', '-m', 'pylint', '-j', '0']
  command.extend(paths)

  returncode = subprocess.run(command, check=False).returncode
  return returncode == 0


def yapf(paths, validate=True):
  """Do yapf on |path| if it is Python file. Only validates format if
  |validate| otherwise, formats the file. Returns False if validation
  or formatting fails."""
  paths = [path for path in paths if is_python(path)]
  if not paths:
    return True

  validate_argument = '-d' if validate else '-i'
  command = ['yapf', validate_argument, '-p']
  command.extend(paths)

  returncode = subprocess.run(command, check=False).returncode
  return returncode == 0


def get_changed_files():
  """Return a list of absolute paths of files changed in this git branch."""
  # FIXME: This doesn't work if branch is behind master.
  diff_command = ['git', 'diff', '--name-only', 'FETCH_HEAD']
  return [
      os.path.abspath(path)
      for path in subprocess.check_output(diff_command).decode().splitlines()
      if os.path.isfile(path)
  ]


def main():
  """Check changes on a branch for common issues before submitting."""
  # Get program arguments.
  parser = argparse.ArgumentParser(description='Presubmit script for oss-fuzz.')
  parser.add_argument('command',
                      choices=['format', 'lint', 'license'],
                      nargs='?')
  args = parser.parse_args()

  changed_files = get_changed_files()

  os.chdir(_SRC_ROOT)

  # Do one specific check if the user asked for it.
  if args.command == 'format':
    success = yapf(changed_files, False)
    return bool_to_returncode(success)

  if args.command == 'lint':
    success = lint(changed_files)
    return bool_to_returncode(success)

  if args.command == 'license':
    success = check_license(changed_files)
    return bool_to_returncode(success)

  # Otherwise, do all of them.
  success = do_checks(changed_files)
  return bool_to_returncode(success)


if __name__ == '__main__':
  sys.exit(main())
