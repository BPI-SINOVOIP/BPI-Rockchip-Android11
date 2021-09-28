#!/usr/bin/env python
#
#  Copyright (C) 2018 Google, Inc.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at:
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
import os
import re

# Matches a JavaDoc comment followed by an @Rpc annotation.
import subprocess

"""A regex that captures the JavaDoc comment and function signature."""
JAVADOC_RPC_REGEX = re.compile(
    # Capture the entire comment string.
    r'(?P<comment>/\*\*(?:(?!/\*\*).)*?\*/)(?:(?:(?!\*/).)*?)\s*'
    # Find at least one @Rpc Annotation
    r'(?:@\w+\s*)*?(?:@Rpc.*?\s*)(?:@\w+\s*)*?'
    # Capture the function signature, ignoring the throws statement
    # (the throws information will be pulled from the comment).
    r'(?P<function_signature>.*?)(?:throws.*?)?{',
    flags=re.MULTILINE | re.DOTALL)

"""
Captures javadoc "frills" like the ones found below:

/**
  *
  */

/** */

"""
CAPTURE_JAVADOC_FRILLS = re.compile(
    r'(^\s*(/\*\*$|/\*\* |\*($| ))|\s*\*/\s*$)',
    re.MULTILINE)

"""A regex to capture the individual pieces of the function signature."""
CAPTURE_FUNCTION_SIGNATURE = re.compile(
    # Capture any non-static function
    r'(public|private)'
    # Allow synchronized and @Annotations()
    r'[( synchronized)(@\w+\(.*?\)?)]*?'
    # Return Type (Allow n-number of generics and arrays)
    r'(?P<return_type>\w+(?:[\[\]<>\w ,]*?)?)\s+'
    # Capture functionName
    r'(?P<function_name>\w*)\s*'
    # Capture anything enclosed in parens
    r'\((?P<parameters>.*)\)',
    re.MULTILINE | re.DOTALL)

"""Matches a parameter and its RPC annotations."""
CAPTURE_PARAMETER = re.compile(
    r'(?:'
    r'(?P<optional>@RpcOptional\s+)?'
    r'(?P<rpc_param>@RpcParameter\(.*?\)\s*)?'
    r'(?P<default>@RpcDefault\((?P<default_value>.*)\)\s*)?'
    r')*'
    r'(?P<param_type>\w+)\s+(?P<param_name>\w+)',
    flags=re.MULTILINE | re.DOTALL)


class Facade(object):
    """A class representing a Facade.

    Attributes:
        path: the path the facade is located at.
        directory: the
    """

    def __init__(self, path):
        self.path = path
        self.directory = os.path.dirname(self.path)
        # -5 removes the '.java' file extension
        self.name = path[path.rfind('/') + 1:-5]
        self.rpcs = list()


def main():
    basepath = os.path.abspath(os.path.join(os.path.dirname(
        os.path.realpath(__file__)), '..'))

    facades = list()

    for path, dirs, files in os.walk(basepath):
        for file_name in files:
            if file_name.endswith('Facade.java'):
                facades.append(parse_facade_file(os.path.join(path, file_name)))

    basepath = os.path.abspath(os.path.join(os.path.dirname(
        os.path.realpath(__file__)), '..'))
    write_output(facades, os.path.join(basepath, 'Docs/ApiReference.md'))


def write_output(facades, output_path):
    facades = sorted(facades, key=lambda x: x.directory)

    git_rev = None
    try:
        git_rev = subprocess.check_output('git rev-parse HEAD',
                                          shell=True).decode('utf-8').strip()
    except subprocess.CalledProcessError as e:
        # Getting the commit ID is optional; we continue if we cannot get it
        pass

    with open(output_path, 'w') as fd:
        if git_rev:
            fd.write('Generated at commit `%s`\n\n' % git_rev)
        fd.write('# Facade Groups')
        prev_directory = ''
        for facade in facades:
            if facade.directory != prev_directory:
                fd.write('\n\n## %s\n\n' % facade.directory[
                                           facade.directory.rfind('/') + 1:])
                prev_directory = facade.directory
            fd.write('  * [%s](#%s)\n' % (facade.name, facade.name.lower()))

        fd.write('\n# Facades\n\n')
        for facade in facades:
            fd.write('\n## %s' % facade.name)
            for rpc in facade.rpcs:
                fd.write('\n\n### %s\n\n' % rpc.name)
                fd.write('%s\n' % rpc)


def parse_facade_file(file_path):
    """Parses a .*Facade.java file and represents it as a Facade object"""
    facade = Facade(file_path)
    with open(file_path, 'r') as content_file:
        content = content_file.read()
        matches = re.findall(JAVADOC_RPC_REGEX, content)
        for match in matches:
            rpc_function = DocumentedFunction(
                match[0].replace('\\n', '\n'),  # match[0]: JavaDoc comment
                match[1].replace('\\n', '\n'))  # match[1]: function signature
            facade.rpcs.append(rpc_function)
    facade.rpcs.sort(key=lambda rpc: rpc.name)
    return facade


class DefaultValue(object):
    """An object representation of a default value.

    Functions as Optional in Java, or a pointer in C++.

    Attributes:
        value: the default value
    """
    def __init__(self, default_value=None):
        self.value = default_value


class DocumentedValue(object):
    def __init__(self):
        """Creates an empty DocumentedValue object."""
        self.type = 'void'
        self.name = None
        self.description = None
        self.default_value = None

    def set_type(self, param_type):
        self.type = param_type
        return self

    def set_name(self, name):
        self.name = name
        return self

    def set_description(self, description):
        self.description = description
        return self

    def set_default_value(self, default_value):
        self.default_value = default_value
        return self

    def __str__(self):
        if self.name is None:
            return self.description
        if self.default_value is None:
            return '%s: %s' % (self.name, self.description)
        else:
            return '%s: %s (default: %s)' % (self.name, self.description,
                                             self.default_value.value)


class DocumentedFunction(object):
    """A combination of all function documentation into a single object.

    Attributes:
        _description: A string that describes the function.
        _parameters: A dictionary of {parameter name: DocumentedValue object}
        _return: a DocumentedValue with information on the returned value.
        _throws: A dictionary of {throw type (str): DocumentedValue object}

    """
    def __init__(self, comment, function_signature):
        self._name = None
        self._description = None
        self._parameters = {}
        self._return = DocumentedValue()
        self._throws = {}

        self._parse_comment(comment)
        self._parse_function_signature(function_signature)

    @property
    def name(self):
        return self._name

    def _parse_comment(self, comment):
        """Parses a JavaDoc comment into DocumentedFunction attributes."""
        comment = str(re.sub(CAPTURE_JAVADOC_FRILLS, '', comment))
        tag = 'description'
        tag_data = ''
        for line in comment.split('\n'):
            line.strip()
            if line.startswith('@'):
                self._finalize_tag(tag, tag_data)
                tag_end_index = line.find(' ')
                tag = line[1:tag_end_index]
                tag_data = line[tag_end_index + 1:]
            else:
                if not tag_data:
                    whitespace_char = ''
                elif (line.startswith(' ')
                      or tag_data.endswith('\n')
                      or line == ''):
                    whitespace_char = '\n'
                else:
                    whitespace_char = ' '
                tag_data = '%s%s%s' % (tag_data, whitespace_char, line)
        self._finalize_tag(tag, tag_data.strip())

    def __str__(self):
        params_signature = ', '.join(['%s %s' % (param.type, param.name)
                                      for param in self._parameters.values()])
        params_description = '\n    '.join(['%s: %s' % (param.name,
                                                        param.description)
                                            for param in
                                            self._parameters.values()])
        if params_description:
            params_description = ('\n**Parameters:**\n\n    %s\n' %
                                  params_description)
        return_description = '\n' if self._return else ''
        if self._return:
            return_description += ('**Returns:**\n\n    %s' %
                                   self._return.description)
        return (
            # ReturnType functionName(Parameters)
            '%s %s(%s)\n\n'
            # Description
            '%s\n'
            # Params & Return
            '%s%s' % (self._return.type, self._name,
                      params_signature, self._description,
                      params_description, return_description)).strip()

    def _parse_function_signature(self, function_signature):
        """Parses the function signature into DocumentedFunction attributes."""
        header_match = re.search(CAPTURE_FUNCTION_SIGNATURE, function_signature)
        self._name = header_match.group('function_name')
        self._return.set_type(header_match.group('return_type'))

        for match in re.finditer(CAPTURE_PARAMETER,
                                 header_match.group('parameters')):
            param_name = match.group('param_name')
            param_type = match.group('param_type')
            if match.group('default_value'):
                default = DefaultValue(match.group('default_value'))
            elif match.group('optional'):
                default = DefaultValue(None)
            else:
                default = None

            if param_name in self._parameters:
                param = self._parameters[param_name]
            else:
                param = DocumentedValue()
            param.set_type(param_type)
            param.set_name(param_name)
            param.set_default_value(default)

    def _finalize_tag(self, tag, tag_data):
        """Finalize the JavaDoc @tag by adding it to the correct field."""
        name = tag_data[:tag_data.find(' ')]
        description = tag_data[tag_data.find(' ') + 1:].strip()
        if tag == 'description':
            self._description = tag_data
        elif tag == 'param':
            if name in self._parameters:
                param = self._parameters[name]
            else:
                param = DocumentedValue().set_name(name)
                self._parameters[name] = param
            param.set_description(description)
        elif tag == 'return':
            self._return.set_description(tag_data)
        elif tag == 'throws':
            new_throws = DocumentedValue().set_name(name)
            new_throws.set_description(description)
            self._throws[name] = new_throws


if __name__ == '__main__':
    main()
