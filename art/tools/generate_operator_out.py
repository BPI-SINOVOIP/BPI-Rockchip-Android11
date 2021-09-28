#!/usr/bin/env python
#
# Copyright (C) 2012 The Android Open Source Project
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

"""Generates default implementations of operator<< for enum types."""

import codecs
import re
import sys


_ENUM_START_RE = re.compile(
    r'\benum\b\s+(class\s+)?(\S+)\s+:?.*\{(\s+// private)?')
_ENUM_VALUE_RE = re.compile(r'([A-Za-z0-9_]+)(.*)')
_ENUM_END_RE = re.compile(r'^\s*\};$')
_ENUMS = {}
_NAMESPACES = {}
_ENUM_CLASSES = {}


def Confused(filename, line_number, line):
    sys.stderr.write('%s:%d: confused by:\n%s\n' %
                     (filename, line_number, line))
    raise Exception("giving up!")
    sys.exit(1)


def ProcessFile(filename):
    lines = codecs.open(filename, 'r', 'utf8', 'replace').read().split('\n')

    class EnumLines:
        def __init__(self, ns, ec):
            self.namespaces = ns
            self.enclosing_classes = ec
            self.lines = []

    def generate_enum_lines(l):
        line_number = 0
        enum_lines = None
        namespaces = []
        enclosing_classes = []

        for raw_line in l:
            line_number += 1

            if enum_lines is None:
                # Is this the start of a new enum?
                m = _ENUM_START_RE.search(raw_line)
                if m:
                    # Yes, so create new line list.
                    enum_lines = EnumLines(namespaces[:], enclosing_classes[:])
                    enum_lines.lines.append((raw_line, line_number))
                    continue

                # Is this the start or end of a namespace?
                m = re.search(r'^namespace (\S+) \{', raw_line)
                if m:
                    namespaces.append(m.group(1))
                    continue
                m = re.search(r'^\}\s+// namespace', raw_line)
                if m:
                    namespaces = namespaces[0:len(namespaces) - 1]
                    continue

                # Is this the start or end of an enclosing class or struct?
                m = re.search(
                    r'^\s*(?:class|struct)(?: MANAGED)?(?: PACKED\([0-9]\))? (\S+).* \{', raw_line)
                if m:
                    enclosing_classes.append(m.group(1))
                    continue

                # End of class/struct -- be careful not to match "do { ... } while" constructs by accident
                m = re.search(r'^\s*\}(\s+)?(while)?(.+)?;', raw_line)
                if m and not m.group(2):
                    enclosing_classes = enclosing_classes[0:len(enclosing_classes) - 1]
                    continue

                continue

            # Is this the end of the current enum?
            m = _ENUM_END_RE.search(raw_line)
            if m:
                if enum_lines is None:
                    Confused(filename, line_number, raw_line)
                yield enum_lines
                enum_lines = None
                continue

            # Append the line
            enum_lines.lines.append((raw_line, line_number))

    for enum_lines in generate_enum_lines(lines):
        m = _ENUM_START_RE.search(enum_lines.lines[0][0])
        if m.group(3) is not None:
            # Skip private enums.
            continue

        # Add an empty entry to _ENUMS for this enum.
        is_enum_class = m.group(1) is not None
        enum_name = m.group(2)
        if len(enum_lines.enclosing_classes) > 0:
            enum_name = '::'.join(enum_lines.enclosing_classes) + '::' + enum_name
        _ENUMS[enum_name] = []
        _NAMESPACES[enum_name] = '::'.join(enum_lines.namespaces)
        _ENUM_CLASSES[enum_name] = is_enum_class

        def generate_non_empty_line(lines):
            for raw_line, line_number in lines:
                # Strip // comments.
                line = re.sub(r'//.*', '', raw_line)
                # Strip whitespace.
                line = line.strip()
                # Skip blank lines.
                if len(line) == 0:
                    continue

                # The only useful thing in comments is the <<alternate text>> syntax for
                # overriding the default enum value names. Pull that out...
                enum_text = None
                m_comment = re.search(r'// <<(.*?)>>', raw_line)
                if m_comment:
                    enum_text = m_comment.group(1)

                yield (line, enum_text, raw_line, line_number)

        for line, enum_text, raw_line, line_number in generate_non_empty_line(enum_lines.lines[1:]):
            # Since we know we're in an enum type, and we're not looking at a comment
            # or a blank line, this line should be the next enum value...
            m = _ENUM_VALUE_RE.search(line)
            if not m:
                Confused(filename, line_number, raw_line)
            enum_value = m.group(1)

            # By default, we turn "kSomeValue" into "SomeValue".
            if enum_text is None:
                enum_text = enum_value
                if enum_text.startswith('k'):
                    enum_text = enum_text[1:]

            # Check that we understand the line (and hopefully do not parse incorrectly), or should
            # filter.
            rest = m.group(2).strip()

            # With "kSomeValue = kOtherValue," we take the original and skip later synonyms.
            # TODO: check that the rhs is actually an existing value.
            if rest.startswith('= k'):
                continue

            # Remove trailing comma.
            if rest.endswith(','):
                rest = rest[:-1]

            # We now expect rest to be empty, or an assignment to an "expression."
            if len(rest):
                # We want to lose the expression "= [exp]". As we do not have a real C parser, just
                # assume anything without a comma is valid.
                m_exp = re.match('= [^,]+$', rest)
                if m_exp is None:
                    sys.stderr.write('%s\n' % (rest))
                    Confused(filename, line_number, raw_line)

            # If the enum is scoped, we must prefix enum value with enum name (which is already prefixed
            # by enclosing classes).
            if is_enum_class:
                enum_value = enum_name + '::' + enum_value
            else:
                if len(enum_lines.enclosing_classes) > 0:
                    enum_value = '::'.join(enum_lines.enclosing_classes) + '::' + enum_value

            _ENUMS[enum_name].append((enum_value, enum_text))


def main():
    local_path = sys.argv[1]
    header_files = []
    for header_file in sys.argv[2:]:
        header_files.append(header_file)
        ProcessFile(header_file)

    print('#include <iostream>')
    print('')

    for header_file in header_files:
        header_file = header_file.replace(local_path + '/', '')
        print('#include "%s"' % header_file)

    print('')

    for enum_name in _ENUMS:
        print('// This was automatically generated by art/tools/generate_operator_out.py --- do not edit!')

        namespaces = _NAMESPACES[enum_name].split('::')
        for namespace in namespaces:
            print('namespace %s {' % namespace)

        print(
            'std::ostream& operator<<(std::ostream& os, const %s& rhs) {' % enum_name)
        print('  switch (rhs) {')
        for (enum_value, enum_text) in _ENUMS[enum_name]:
            print('    case %s: os << "%s"; break;' % (enum_value, enum_text))
        if not _ENUM_CLASSES[enum_name]:
            print(
                '    default: os << "%s[" << static_cast<int>(rhs) << "]"; break;' % enum_name)
        print('  }')
        print('  return os;')
        print('}')

        for namespace in reversed(namespaces):
            print('}  // namespace %s' % namespace)
        print('')

    sys.exit(0)


if __name__ == '__main__':
    main()
