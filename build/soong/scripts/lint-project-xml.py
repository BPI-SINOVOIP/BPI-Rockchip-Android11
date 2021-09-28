#!/usr/bin/env python3
#
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
#

"""This file generates project.xml and lint.xml files used to drive the Android Lint CLI tool."""

import argparse


def check_action(check_type):
  """
  Returns an action that appends a tuple of check_type and the argument to the dest.
  """
  class CheckAction(argparse.Action):
    def __init__(self, option_strings, dest, nargs=None, **kwargs):
      if nargs is not None:
        raise ValueError("nargs must be None, was %s" % nargs)
      super(CheckAction, self).__init__(option_strings, dest, **kwargs)
    def __call__(self, parser, namespace, values, option_string=None):
      checks = getattr(namespace, self.dest, [])
      checks.append((check_type, values))
      setattr(namespace, self.dest, checks)
  return CheckAction


def parse_args():
  """Parse commandline arguments."""

  def convert_arg_line_to_args(arg_line):
    for arg in arg_line.split():
      if arg.startswith('#'):
        return
      if not arg.strip():
        continue
      yield arg

  parser = argparse.ArgumentParser(fromfile_prefix_chars='@')
  parser.convert_arg_line_to_args = convert_arg_line_to_args
  parser.add_argument('--project_out', dest='project_out',
                      help='file to which the project.xml contents will be written.')
  parser.add_argument('--config_out', dest='config_out',
                      help='file to which the lint.xml contents will be written.')
  parser.add_argument('--name', dest='name',
                      help='name of the module.')
  parser.add_argument('--srcs', dest='srcs', action='append', default=[],
                      help='file containing whitespace separated list of source files.')
  parser.add_argument('--generated_srcs', dest='generated_srcs', action='append', default=[],
                      help='file containing whitespace separated list of generated source files.')
  parser.add_argument('--resources', dest='resources', action='append', default=[],
                      help='file containing whitespace separated list of resource files.')
  parser.add_argument('--classes', dest='classes', action='append', default=[],
                      help='file containing the module\'s classes.')
  parser.add_argument('--classpath', dest='classpath', action='append', default=[],
                      help='file containing classes from dependencies.')
  parser.add_argument('--extra_checks_jar', dest='extra_checks_jars', action='append', default=[],
                      help='file containing extra lint checks.')
  parser.add_argument('--manifest', dest='manifest',
                      help='file containing the module\'s manifest.')
  parser.add_argument('--merged_manifest', dest='merged_manifest',
                      help='file containing merged manifest for the module and its dependencies.')
  parser.add_argument('--library', dest='library', action='store_true',
                      help='mark the module as a library.')
  parser.add_argument('--test', dest='test', action='store_true',
                      help='mark the module as a test.')
  parser.add_argument('--cache_dir', dest='cache_dir',
                      help='directory to use for cached file.')
  parser.add_argument('--root_dir', dest='root_dir',
                      help='directory to use for root dir.')
  group = parser.add_argument_group('check arguments', 'later arguments override earlier ones.')
  group.add_argument('--fatal_check', dest='checks', action=check_action('fatal'), default=[],
                     help='treat a lint issue as a fatal error.')
  group.add_argument('--error_check', dest='checks', action=check_action('error'), default=[],
                     help='treat a lint issue as an error.')
  group.add_argument('--warning_check', dest='checks', action=check_action('warning'), default=[],
                     help='treat a lint issue as a warning.')
  group.add_argument('--disable_check', dest='checks', action=check_action('ignore'), default=[],
                     help='disable a lint issue.')
  return parser.parse_args()


class NinjaRspFileReader:
  """
  Reads entries from a Ninja rsp file.  Ninja escapes any entries in the file that contain a
  non-standard character by surrounding the whole entry with single quotes, and then replacing
  any single quotes in the entry with the escape sequence '\''.
  """

  def __init__(self, filename):
    self.f = open(filename, 'r')
    self.r = self.character_reader(self.f)

  def __iter__(self):
    return self

  def character_reader(self, f):
    """Turns a file into a generator that returns one character at a time."""
    while True:
      c = f.read(1)
      if c:
        yield c
      else:
        return

  def __next__(self):
    entry = self.read_entry()
    if entry:
      return entry
    else:
      raise StopIteration

  def read_entry(self):
    c = next(self.r, "")
    if not c:
      return ""
    elif c == "'":
      return self.read_quoted_entry()
    else:
      entry = c
      for c in self.r:
        if c == " " or c == "\n":
          break
        entry += c
      return entry

  def read_quoted_entry(self):
    entry = ""
    for c in self.r:
      if c == "'":
        # Either the end of the quoted entry, or the beginning of an escape sequence, read the next
        # character to find out.
        c = next(self.r)
        if not c or c == " " or c == "\n":
          # End of the item
          return entry
        elif c == "\\":
          # Escape sequence, expect a '
          c = next(self.r)
          if c != "'":
            # Malformed escape sequence
            raise "malformed escape sequence %s'\\%s" % (entry, c)
          entry += "'"
        else:
          raise "malformed escape sequence %s'%s" % (entry, c)
      else:
        entry += c
    raise "unterminated quoted entry %s" % entry


def write_project_xml(f, args):
  test_attr = "test='true' " if args.test else ""

  f.write("<?xml version='1.0' encoding='utf-8'?>\n")
  f.write("<project>\n")
  if args.root_dir:
    f.write("  <root dir='%s' />\n" % args.root_dir)
  f.write("  <module name='%s' android='true' %sdesugar='full' >\n" % (args.name, "library='true' " if args.library else ""))
  if args.manifest:
    f.write("    <manifest file='%s' %s/>\n" % (args.manifest, test_attr))
  if args.merged_manifest:
    f.write("    <merged-manifest file='%s' %s/>\n" % (args.merged_manifest, test_attr))
  for src_file in args.srcs:
    for src in NinjaRspFileReader(src_file):
      f.write("    <src file='%s' %s/>\n" % (src, test_attr))
  for src_file in args.generated_srcs:
    for src in NinjaRspFileReader(src_file):
      f.write("    <src file='%s' generated='true' %s/>\n" % (src, test_attr))
  for res_file in args.resources:
    for res in NinjaRspFileReader(res_file):
      f.write("    <resource file='%s' %s/>\n" % (res, test_attr))
  for classes in args.classes:
    f.write("    <classes jar='%s' />\n" % classes)
  for classpath in args.classpath:
    f.write("    <classpath jar='%s' />\n" % classpath)
  for extra in args.extra_checks_jars:
    f.write("    <lint-checks jar='%s' />\n" % extra)
  f.write("  </module>\n")
  if args.cache_dir:
    f.write("  <cache dir='%s'/>\n" % args.cache_dir)
  f.write("</project>\n")


def write_config_xml(f, args):
  f.write("<?xml version='1.0' encoding='utf-8'?>\n")
  f.write("<lint>\n")
  for check in args.checks:
    f.write("  <issue id='%s' severity='%s' />\n" % (check[1], check[0]))
  f.write("</lint>\n")


def main():
  """Program entry point."""
  args = parse_args()

  if args.project_out:
    with open(args.project_out, 'w') as f:
      write_project_xml(f, args)

  if args.config_out:
    with open(args.config_out, 'w') as f:
      write_config_xml(f, args)


if __name__ == '__main__':
  main()
