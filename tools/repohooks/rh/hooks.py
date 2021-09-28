# Copyright 2016 The Android Open Source Project
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

"""Functions that implement the actual checks."""

import collections
import fnmatch
import json
import os
import platform
import re
import sys

_path = os.path.realpath(__file__ + '/../..')
if sys.path[0] != _path:
    sys.path.insert(0, _path)
del _path

# pylint: disable=wrong-import-position
import rh.git
import rh.results
import rh.utils


class Placeholders(object):
    """Holder class for replacing ${vars} in arg lists.

    To add a new variable to replace in config files, just add it as a @property
    to this class using the form.  So to add support for BIRD:
      @property
      def var_BIRD(self):
        return <whatever this is>

    You can return either a string or an iterable (e.g. a list or tuple).
    """

    def __init__(self, diff=()):
        """Initialize.

        Args:
          diff: The list of files that changed.
        """
        self.diff = diff

    def expand_vars(self, args):
        """Perform place holder expansion on all of |args|.

        Args:
          args: The args to perform expansion on.

        Returns:
          The updated |args| list.
        """
        all_vars = set(self.vars())
        replacements = dict((var, self.get(var)) for var in all_vars)

        ret = []
        for arg in args:
            if arg.endswith('${PREUPLOAD_FILES_PREFIXED}'):
                if arg == '${PREUPLOAD_FILES_PREFIXED}':
                    assert len(ret) > 1, ('PREUPLOAD_FILES_PREFIXED cannot be '
                                          'the 1st or 2nd argument')
                    prev_arg = ret[-1]
                    ret = ret[0:-1]
                    for file in self.get('PREUPLOAD_FILES'):
                        ret.append(prev_arg)
                        ret.append(file)
                else:
                    prefix = arg[0:-len('${PREUPLOAD_FILES_PREFIXED}')]
                    ret.extend(
                        prefix + file for file in self.get('PREUPLOAD_FILES'))
            else:
                # First scan for exact matches
                for key, val in replacements.items():
                    var = '${%s}' % (key,)
                    if arg == var:
                        if isinstance(val, str):
                            ret.append(val)
                        else:
                            ret.extend(val)
                        # We break on first hit to avoid double expansion.
                        break
                else:
                    # If no exact matches, do an inline replacement.
                    def replace(m):
                        val = self.get(m.group(1))
                        if isinstance(val, str):
                            return val
                        return ' '.join(val)
                    ret.append(re.sub(r'\$\{(%s)\}' % ('|'.join(all_vars),),
                                      replace, arg))
        return ret

    @classmethod
    def vars(cls):
        """Yield all replacement variable names."""
        for key in dir(cls):
            if key.startswith('var_'):
                yield key[4:]

    def get(self, var):
        """Helper function to get the replacement |var| value."""
        return getattr(self, 'var_%s' % (var,))

    @property
    def var_PREUPLOAD_COMMIT_MESSAGE(self):
        """The git commit message."""
        return os.environ.get('PREUPLOAD_COMMIT_MESSAGE', '')

    @property
    def var_PREUPLOAD_COMMIT(self):
        """The git commit sha1."""
        return os.environ.get('PREUPLOAD_COMMIT', '')

    @property
    def var_PREUPLOAD_FILES(self):
        """List of files modified in this git commit."""
        return [x.file for x in self.diff if x.status != 'D']

    @property
    def var_REPO_ROOT(self):
        """The root of the repo checkout."""
        return rh.git.find_repo_root()

    @property
    def var_BUILD_OS(self):
        """The build OS (see _get_build_os_name for details)."""
        return _get_build_os_name()


class ExclusionScope(object):
    """Exclusion scope for a hook.

    An exclusion scope can be used to determine if a hook has been disabled for
    a specific project.
    """

    def __init__(self, scope):
        """Initialize.

        Args:
          scope: A list of shell-style wildcards (fnmatch) or regular
              expression. Regular expressions must start with the ^ character.
        """
        self._scope = []
        for path in scope:
            if path.startswith('^'):
                self._scope.append(re.compile(path))
            else:
                self._scope.append(path)

    def __contains__(self, proj_dir):
        """Checks if |proj_dir| matches the excluded paths.

        Args:
          proj_dir: The relative path of the project.
        """
        for exclusion_path in self._scope:
            if hasattr(exclusion_path, 'match'):
                if exclusion_path.match(proj_dir):
                    return True
            elif fnmatch.fnmatch(proj_dir, exclusion_path):
                return True
        return False


class HookOptions(object):
    """Holder class for hook options."""

    def __init__(self, name, args, tool_paths):
        """Initialize.

        Args:
          name: The name of the hook.
          args: The override commandline arguments for the hook.
          tool_paths: A dictionary with tool names to paths.
        """
        self.name = name
        self._args = args
        self._tool_paths = tool_paths

    @staticmethod
    def expand_vars(args, diff=()):
        """Perform place holder expansion on all of |args|."""
        replacer = Placeholders(diff=diff)
        return replacer.expand_vars(args)

    def args(self, default_args=(), diff=()):
        """Gets the hook arguments, after performing place holder expansion.

        Args:
          default_args: The list to return if |self._args| is empty.
          diff: The list of files that changed in the current commit.

        Returns:
          A list with arguments.
        """
        args = self._args
        if not args:
            args = default_args

        return self.expand_vars(args, diff=diff)

    def tool_path(self, tool_name):
        """Gets the path in which the |tool_name| executable can be found.

        This function performs expansion for some place holders.  If the tool
        does not exist in the overridden |self._tool_paths| dictionary, the tool
        name will be returned and will be run from the user's $PATH.

        Args:
          tool_name: The name of the executable.

        Returns:
          The path of the tool with all optional place holders expanded.
        """
        assert tool_name in TOOL_PATHS
        if tool_name not in self._tool_paths:
            return TOOL_PATHS[tool_name]

        tool_path = os.path.normpath(self._tool_paths[tool_name])
        return self.expand_vars([tool_path])[0]


# A callable hook.
CallableHook = collections.namedtuple('CallableHook', ('name', 'hook', 'scope'))


def _run(cmd, **kwargs):
    """Helper command for checks that tend to gather output."""
    kwargs.setdefault('combine_stdout_stderr', True)
    kwargs.setdefault('capture_output', True)
    kwargs.setdefault('check', False)
    # Make sure hooks run with stdin disconnected to avoid accidentally
    # interactive tools causing pauses.
    kwargs.setdefault('input', '')
    return rh.utils.run(cmd, **kwargs)


def _match_regex_list(subject, expressions):
    """Try to match a list of regular expressions to a string.

    Args:
      subject: The string to match regexes on.
      expressions: An iterable of regular expressions to check for matches with.

    Returns:
      Whether the passed in subject matches any of the passed in regexes.
    """
    for expr in expressions:
        if re.search(expr, subject):
            return True
    return False


def _filter_diff(diff, include_list, exclude_list=()):
    """Filter out files based on the conditions passed in.

    Args:
      diff: list of diff objects to filter.
      include_list: list of regex that when matched with a file path will cause
          it to be added to the output list unless the file is also matched with
          a regex in the exclude_list.
      exclude_list: list of regex that when matched with a file will prevent it
          from being added to the output list, even if it is also matched with a
          regex in the include_list.

    Returns:
      A list of filepaths that contain files matched in the include_list and not
      in the exclude_list.
    """
    filtered = []
    for d in diff:
        if (d.status != 'D' and
                _match_regex_list(d.file, include_list) and
                not _match_regex_list(d.file, exclude_list)):
            # We've got a match!
            filtered.append(d)
    return filtered


def _get_build_os_name():
    """Gets the build OS name.

    Returns:
      A string in a format usable to get prebuilt tool paths.
    """
    system = platform.system()
    if 'Darwin' in system or 'Macintosh' in system:
        return 'darwin-x86'

    # TODO: Add more values if needed.
    return 'linux-x86'


def _fixup_func_caller(cmd, **kwargs):
    """Wraps |cmd| around a callable automated fixup.

    For hooks that support automatically fixing errors after running (e.g. code
    formatters), this function provides a way to run |cmd| as the |fixup_func|
    parameter in HookCommandResult.
    """
    def wrapper():
        result = _run(cmd, **kwargs)
        if result.returncode not in (None, 0):
            return result.stdout
        return None
    return wrapper


def _check_cmd(hook_name, project, commit, cmd, fixup_func=None, **kwargs):
    """Runs |cmd| and returns its result as a HookCommandResult."""
    return [rh.results.HookCommandResult(hook_name, project, commit,
                                         _run(cmd, **kwargs),
                                         fixup_func=fixup_func)]


# Where helper programs exist.
TOOLS_DIR = os.path.realpath(__file__ + '/../../tools')

def get_helper_path(tool):
    """Return the full path to the helper |tool|."""
    return os.path.join(TOOLS_DIR, tool)


def check_custom(project, commit, _desc, diff, options=None, **kwargs):
    """Run a custom hook."""
    return _check_cmd(options.name, project, commit, options.args((), diff),
                      **kwargs)


def check_bpfmt(project, commit, _desc, diff, options=None):
    """Checks that Blueprint files are formatted with bpfmt."""
    filtered = _filter_diff(diff, [r'\.bp$'])
    if not filtered:
        return None

    bpfmt = options.tool_path('bpfmt')
    cmd = [bpfmt, '-l'] + options.args((), filtered)
    ret = []
    for d in filtered:
        data = rh.git.get_file_content(commit, d.file)
        result = _run(cmd, input=data)
        if result.stdout:
            ret.append(rh.results.HookResult(
                'bpfmt', project, commit, error=result.stdout,
                files=(d.file,)))
    return ret


def check_checkpatch(project, commit, _desc, diff, options=None):
    """Run |diff| through the kernel's checkpatch.pl tool."""
    tool = get_helper_path('checkpatch.pl')
    cmd = ([tool, '-', '--root', project.dir] +
           options.args(('--ignore=GERRIT_CHANGE_ID',), diff))
    return _check_cmd('checkpatch.pl', project, commit, cmd,
                      input=rh.git.get_patch(commit))


def check_clang_format(project, commit, _desc, diff, options=None):
    """Run git clang-format on the commit."""
    tool = get_helper_path('clang-format.py')
    clang_format = options.tool_path('clang-format')
    git_clang_format = options.tool_path('git-clang-format')
    tool_args = (['--clang-format', clang_format, '--git-clang-format',
                  git_clang_format] +
                 options.args(('--style', 'file', '--commit', commit), diff))
    cmd = [tool] + tool_args
    fixup_func = _fixup_func_caller([tool, '--fix'] + tool_args)
    return _check_cmd('clang-format', project, commit, cmd,
                      fixup_func=fixup_func)


def check_google_java_format(project, commit, _desc, _diff, options=None):
    """Run google-java-format on the commit."""

    tool = get_helper_path('google-java-format.py')
    google_java_format = options.tool_path('google-java-format')
    google_java_format_diff = options.tool_path('google-java-format-diff')
    tool_args = ['--google-java-format', google_java_format,
                 '--google-java-format-diff', google_java_format_diff,
                 '--commit', commit] + options.args()
    cmd = [tool] + tool_args
    fixup_func = _fixup_func_caller([tool, '--fix'] + tool_args)
    return _check_cmd('google-java-format', project, commit, cmd,
                      fixup_func=fixup_func)


def check_commit_msg_bug_field(project, commit, desc, _diff, options=None):
    """Check the commit message for a 'Bug:' line."""
    field = 'Bug'
    regex = r'^%s: (None|[0-9]+(, [0-9]+)*)$' % (field,)
    check_re = re.compile(regex)

    if options.args():
        raise ValueError('commit msg %s check takes no options' % (field,))

    found = []
    for line in desc.splitlines():
        if check_re.match(line):
            found.append(line)

    if not found:
        error = ('Commit message is missing a "%s:" line.  It must match the\n'
                 'following case-sensitive regex:\n\n    %s') % (field, regex)
    else:
        return None

    return [rh.results.HookResult('commit msg: "%s:" check' % (field,),
                                  project, commit, error=error)]


def check_commit_msg_changeid_field(project, commit, desc, _diff, options=None):
    """Check the commit message for a 'Change-Id:' line."""
    field = 'Change-Id'
    regex = r'^%s: I[a-f0-9]+$' % (field,)
    check_re = re.compile(regex)

    if options.args():
        raise ValueError('commit msg %s check takes no options' % (field,))

    found = []
    for line in desc.splitlines():
        if check_re.match(line):
            found.append(line)

    if not found:
        error = ('Commit message is missing a "%s:" line.  It must match the\n'
                 'following case-sensitive regex:\n\n    %s') % (field, regex)
    elif len(found) > 1:
        error = ('Commit message has too many "%s:" lines.  There can be only '
                 'one.') % (field,)
    else:
        return None

    return [rh.results.HookResult('commit msg: "%s:" check' % (field,),
                                  project, commit, error=error)]


PREBUILT_APK_MSG = """Commit message is missing required prebuilt APK
information.  To generate the information, use the aapt tool to dump badging
information of the APKs being uploaded, specify where the APK was built, and
specify whether the APKs are suitable for release:

    for apk in $(find . -name '*.apk' | sort); do
        echo "${apk}"
        ${AAPT} dump badging "${apk}" |
            grep -iE "(package: |sdkVersion:|targetSdkVersion:)" |
            sed -e "s/' /'\\n/g"
        echo
    done

It must match the following case-sensitive multiline regex searches:

    %s

For more information, see go/platform-prebuilt and go/android-prebuilt.

"""


def check_commit_msg_prebuilt_apk_fields(project, commit, desc, diff,
                                         options=None):
    """Check that prebuilt APK commits contain the required lines."""

    if options.args():
        raise ValueError('prebuilt apk check takes no options')

    filtered = _filter_diff(diff, [r'\.apk$'])
    if not filtered:
        return None

    regexes = [
        r'^package: .*$',
        r'^sdkVersion:.*$',
        r'^targetSdkVersion:.*$',
        r'^Built here:.*$',
        (r'^This build IS( NOT)? suitable for'
         r'( preview|( preview or)? public) release'
         r'( but IS NOT suitable for public release)?\.$')
    ]

    missing = []
    for regex in regexes:
        if not re.search(regex, desc, re.MULTILINE):
            missing.append(regex)

    if missing:
        error = PREBUILT_APK_MSG % '\n    '.join(missing)
    else:
        return None

    return [rh.results.HookResult('commit msg: "prebuilt apk:" check',
                                  project, commit, error=error)]


TEST_MSG = """Commit message is missing a "Test:" line.  It must match the
following case-sensitive regex:

    %s

The Test: stanza is free-form and should describe how you tested your change.
As a CL author, you'll have a consistent place to describe the testing strategy
you use for your work. As a CL reviewer, you'll be reminded to discuss testing
as part of your code review, and you'll more easily replicate testing when you
patch in CLs locally.

Some examples below:

Test: make WITH_TIDY=1 mmma art
Test: make test-art
Test: manual - took a photo
Test: refactoring CL. Existing unit tests still pass.

Check the git history for more examples. It's a free-form field, so we urge
you to develop conventions that make sense for your project. Note that many
projects use exact test commands, which are perfectly fine.

Adding good automated tests with new code is critical to our goals of keeping
the system stable and constantly improving quality. Please use Test: to
highlight this area of your development. And reviewers, please insist on
high-quality Test: descriptions.
"""


def check_commit_msg_test_field(project, commit, desc, _diff, options=None):
    """Check the commit message for a 'Test:' line."""
    field = 'Test'
    regex = r'^%s: .*$' % (field,)
    check_re = re.compile(regex)

    if options.args():
        raise ValueError('commit msg %s check takes no options' % (field,))

    found = []
    for line in desc.splitlines():
        if check_re.match(line):
            found.append(line)

    if not found:
        error = TEST_MSG % (regex)
    else:
        return None

    return [rh.results.HookResult('commit msg: "%s:" check' % (field,),
                                  project, commit, error=error)]


RELNOTE_MISSPELL_MSG = """Commit message contains something that looks
similar to the "Relnote:" tag.  It must match the regex:

    %s

The Relnote: stanza is free-form and should describe what developers need to
know about your change.

Some examples below:

Relnote: "Added a new API `Class#isBetter` to determine whether or not the
class is better"
Relnote: Fixed an issue where the UI would hang on a double tap.

Check the git history for more examples. It's a free-form field, so we urge
you to develop conventions that make sense for your project.
"""

RELNOTE_MISSING_QUOTES_MSG = """Commit message contains something that looks
similar to the "Relnote:" tag but might be malformatted.  For multiline
release notes, you need to include a starting and closing quote.

Multi-line Relnote example:

Relnote: "Added a new API `Class#getSize` to get the size of the class.
This is useful if you need to know the size of the class."

Single-line Relnote example:

Relnote: Added a new API `Class#containsData`
"""

RELNOTE_INVALID_QUOTES_MSG = """Commit message contains something that looks
similar to the "Relnote:" tag but might be malformatted.  If you are using
quotes that do not mark the start or end of a Relnote, you need to escape them
with a backslash.

Non-starting/non-ending quote Relnote examples:

Relnote: "Fixed an error with `Class#getBar()` where \"foo\" would be returned
in edge cases."
Relnote: Added a new API to handle strings like \"foo\"
"""

def check_commit_msg_relnote_field_format(project, commit, desc, _diff,
                                          options=None):
    """Check the commit for one correctly formatted 'Relnote:' line.

    Checks the commit message for two things:
    (1) Checks for possible misspellings of the 'Relnote:' tag.
    (2) Ensures that multiline release notes are properly formatted with a
    starting quote and an endling quote.
    (3) Checks that release notes that contain non-starting or non-ending
    quotes are escaped with a backslash.
    """
    field = 'Relnote'
    regex_relnote = r'^%s:.*$' % (field,)
    check_re_relnote = re.compile(regex_relnote, re.IGNORECASE)

    if options.args():
        raise ValueError('commit msg %s check takes no options' % (field,))

    # Check 1: Check for possible misspellings of the `Relnote:` field.

    # Regex for misspelled fields.
    possible_field_misspells = {'Relnotes', 'ReleaseNote',
                                'Rel-note', 'Rel note',
                                'rel-notes', 'releasenotes',
                                'release-note', 'release-notes'}
    regex_field_misspells = r'^(%s): .*$' % (
        '|'.join(possible_field_misspells),
    )
    check_re_field_misspells = re.compile(regex_field_misspells, re.IGNORECASE)

    ret = []
    for line in desc.splitlines():
        if check_re_field_misspells.match(line):
            error = RELNOTE_MISSPELL_MSG % (regex_relnote, )
            ret.append(
                rh.results.HookResult(('commit msg: "%s:" '
                                       'tag spelling error') % (field,),
                                      project, commit, error=error))

    # Check 2: Check that multiline Relnotes are quoted.

    check_re_empty_string = re.compile(r'^$')

    # Regex to find other fields that could be used.
    regex_other_fields = r'^[a-zA-Z0-9-]+:'
    check_re_other_fields = re.compile(regex_other_fields)

    desc_lines = desc.splitlines()
    for i, cur_line in enumerate(desc_lines):
        # Look for a Relnote tag that is before the last line and
        # lacking any quotes.
        if (check_re_relnote.match(cur_line) and
                i < len(desc_lines) - 1 and
                '"' not in cur_line):
            next_line = desc_lines[i + 1]
            # Check that the next line does not contain any other field
            # and it's not an empty string.
            if (not check_re_other_fields.findall(next_line) and
                    not check_re_empty_string.match(next_line)):
                ret.append(
                    rh.results.HookResult(('commit msg: "%s:" '
                                           'tag missing quotes') % (field,),
                                          project, commit,
                                          error=RELNOTE_MISSING_QUOTES_MSG))
                break

    # Check 3: Check that multiline Relnotes contain matching quotes.
    first_quote_found = False
    second_quote_found = False
    for cur_line in desc_lines:
        contains_quote = '"' in cur_line
        contains_field = check_re_other_fields.findall(cur_line)
        # If we have found the first quote and another field, break and fail.
        if first_quote_found and contains_field:
            break
        # If we have found the first quote, this line contains a quote,
        # and this line is not another field, break and succeed.
        if first_quote_found and contains_quote:
            second_quote_found = True
            break
        # Check that the `Relnote:` tag exists and it contains a starting quote.
        if check_re_relnote.match(cur_line) and contains_quote:
            first_quote_found = True
            # A single-line Relnote containing a start and ending triple quote
            # is valid.
            if cur_line.count('"""') == 2:
                second_quote_found = True
                break
            # A single-line Relnote containing a start and ending quote
            # is valid.
            if cur_line.count('"') - cur_line.count('\\"') == 2:
                second_quote_found = True
                break
    if first_quote_found != second_quote_found:
        ret.append(
            rh.results.HookResult(('commit msg: "%s:" '
                                   'tag missing closing quote') % (field,),
                                  project, commit,
                                  error=RELNOTE_MISSING_QUOTES_MSG))

    # Check 4: Check that non-starting or non-ending quotes are escaped with a
    # backslash.
    line_needs_checking = False
    uses_invalid_quotes = False
    for cur_line in desc_lines:
        if check_re_other_fields.findall(cur_line):
            line_needs_checking = False
        on_relnote_line = check_re_relnote.match(cur_line)
        # Determine if we are parsing the base `Relnote:` line.
        if on_relnote_line and '"' in cur_line:
            line_needs_checking = True
            # We don't think anyone will type '"""' and then forget to
            # escape it, so we're not checking for this.
            if '"""' in cur_line:
                break
        if line_needs_checking:
            stripped_line = re.sub('^%s:' % field, '', cur_line,
                                   flags=re.IGNORECASE).strip()
            for i, character in enumerate(stripped_line):
                if i == 0:
                    # Case 1: Valid quote at the beginning of the
                    # base `Relnote:` line.
                    if on_relnote_line:
                        continue
                    # Case 2: Invalid quote at the beginning of following
                    # lines, where we are not terminating the release note.
                    if character == '"' and stripped_line != '"':
                        uses_invalid_quotes = True
                        break
                # Case 3: Check all other cases.
                if (character == '"'
                        and 0 < i < len(stripped_line) - 1
                        and stripped_line[i-1] != '"'
                        and stripped_line[i-1] != "\\"):
                    uses_invalid_quotes = True
                    break

    if uses_invalid_quotes:
        ret.append(rh.results.HookResult(('commit msg: "%s:" '
                                          'tag using unescaped '
                                          'quotes') % (field,),
                                         project, commit,
                                         error=RELNOTE_INVALID_QUOTES_MSG))
    return ret


RELNOTE_REQUIRED_CURRENT_TXT_MSG = """\
Commit contains a change to current.txt or public_plus_experimental_current.txt,
but the commit message does not contain the required `Relnote:` tag.  It must
match the regex:

    %s

The Relnote: stanza is free-form and should describe what developers need to
know about your change.  If you are making infrastructure changes, you
can set the Relnote: stanza to be "N/A" for the commit to not be included
in release notes.

Some examples:

Relnote: "Added a new API `Class#isBetter` to determine whether or not the
class is better"
Relnote: Fixed an issue where the UI would hang on a double tap.
Relnote: N/A

Check the git history for more examples.
"""

def check_commit_msg_relnote_for_current_txt(project, commit, desc, diff,
                                             options=None):
    """Check changes to current.txt contain the 'Relnote:' stanza."""
    field = 'Relnote'
    regex = r'^%s: .+$' % (field,)
    check_re = re.compile(regex, re.IGNORECASE)

    if options.args():
        raise ValueError('commit msg %s check takes no options' % (field,))

    filtered = _filter_diff(
        diff,
        [r'(^|/)(public_plus_experimental_current|current)\.txt$']
    )
    # If the commit does not contain a change to *current.txt, then this repo
    # hook check no longer applies.
    if not filtered:
        return None

    found = []
    for line in desc.splitlines():
        if check_re.match(line):
            found.append(line)

    if not found:
        error = RELNOTE_REQUIRED_CURRENT_TXT_MSG % (regex)
    else:
        return None

    return [rh.results.HookResult('commit msg: "%s:" check' % (field,),
                                  project, commit, error=error)]


def check_cpplint(project, commit, _desc, diff, options=None):
    """Run cpplint."""
    # This list matches what cpplint expects.  We could run on more (like .cxx),
    # but cpplint would just ignore them.
    filtered = _filter_diff(diff, [r'\.(cc|h|cpp|cu|cuh)$'])
    if not filtered:
        return None

    cpplint = options.tool_path('cpplint')
    cmd = [cpplint] + options.args(('${PREUPLOAD_FILES}',), filtered)
    return _check_cmd('cpplint', project, commit, cmd)


def check_gofmt(project, commit, _desc, diff, options=None):
    """Checks that Go files are formatted with gofmt."""
    filtered = _filter_diff(diff, [r'\.go$'])
    if not filtered:
        return None

    gofmt = options.tool_path('gofmt')
    cmd = [gofmt, '-l'] + options.args((), filtered)
    ret = []
    for d in filtered:
        data = rh.git.get_file_content(commit, d.file)
        result = _run(cmd, input=data)
        if result.stdout:
            fixup_func = _fixup_func_caller([gofmt, '-w', d.file])
            ret.append(rh.results.HookResult(
                'gofmt', project, commit, error=result.stdout,
                files=(d.file,), fixup_func=fixup_func))
    return ret


def check_json(project, commit, _desc, diff, options=None):
    """Verify json files are valid."""
    if options.args():
        raise ValueError('json check takes no options')

    filtered = _filter_diff(diff, [r'\.json$'])
    if not filtered:
        return None

    ret = []
    for d in filtered:
        data = rh.git.get_file_content(commit, d.file)
        try:
            json.loads(data)
        except ValueError as e:
            ret.append(rh.results.HookResult(
                'json', project, commit, error=str(e),
                files=(d.file,)))
    return ret


def _check_pylint(project, commit, _desc, diff, extra_args=None, options=None):
    """Run pylint."""
    filtered = _filter_diff(diff, [r'\.py$'])
    if not filtered:
        return None

    if extra_args is None:
        extra_args = []

    pylint = options.tool_path('pylint')
    cmd = [
        get_helper_path('pylint.py'),
        '--executable-path', pylint,
    ] + extra_args + options.args(('${PREUPLOAD_FILES}',), filtered)
    return _check_cmd('pylint', project, commit, cmd)


def check_pylint2(project, commit, desc, diff, options=None):
    """Run pylint through Python 2."""
    return _check_pylint(project, commit, desc, diff, options=options)


def check_pylint3(project, commit, desc, diff, options=None):
    """Run pylint through Python 3."""
    return _check_pylint(project, commit, desc, diff,
                         extra_args=['--py3'],
                         options=options)


def check_rustfmt(project, commit, _desc, diff, options=None):
    """Run "rustfmt --check" on diffed rust files"""
    filtered = _filter_diff(diff, [r'\.rs$'])
    if not filtered:
        return None

    rustfmt = options.tool_path('rustfmt')
    cmd = [rustfmt] + options.args((), filtered)
    ret = []
    for d in filtered:
        data = rh.git.get_file_content(commit, d.file)
        result = _run(cmd, input=data)
        # If the parsing failed, stdout will contain enough details on the
        # location of the error.
        if result.returncode:
            ret.append(rh.results.HookResult(
                'rustfmt', project, commit, error=result.stdout,
                files=(d.file,)))
            continue
        # TODO(b/164111102): rustfmt stable does not support --check on stdin.
        # If no error is reported, compare stdin with stdout.
        if data != result.stdout:
            msg = ('To fix, please run: %s' %
                   rh.shell.cmd_to_str(cmd + [d.file]))
            ret.append(rh.results.HookResult(
                'rustfmt', project, commit, error=msg,
                files=(d.file,)))
    return ret


def check_xmllint(project, commit, _desc, diff, options=None):
    """Run xmllint."""
    # XXX: Should we drop most of these and probe for <?xml> tags?
    extensions = frozenset((
        'dbus-xml',  # Generated DBUS interface.
        'dia',       # File format for Dia.
        'dtd',       # Document Type Definition.
        'fml',       # Fuzzy markup language.
        'form',      # Forms created by IntelliJ GUI Designer.
        'fxml',      # JavaFX user interfaces.
        'glade',     # Glade user interface design.
        'grd',       # GRIT translation files.
        'iml',       # Android build modules?
        'kml',       # Keyhole Markup Language.
        'mxml',      # Macromedia user interface markup language.
        'nib',       # OS X Cocoa Interface Builder.
        'plist',     # Property list (for OS X).
        'pom',       # Project Object Model (for Apache Maven).
        'rng',       # RELAX NG schemas.
        'sgml',      # Standard Generalized Markup Language.
        'svg',       # Scalable Vector Graphics.
        'uml',       # Unified Modeling Language.
        'vcproj',    # Microsoft Visual Studio project.
        'vcxproj',   # Microsoft Visual Studio project.
        'wxs',       # WiX Transform File.
        'xhtml',     # XML HTML.
        'xib',       # OS X Cocoa Interface Builder.
        'xlb',       # Android locale bundle.
        'xml',       # Extensible Markup Language.
        'xsd',       # XML Schema Definition.
        'xsl',       # Extensible Stylesheet Language.
    ))

    filtered = _filter_diff(diff, [r'\.(%s)$' % '|'.join(extensions)])
    if not filtered:
        return None

    # TODO: Figure out how to integrate schema validation.
    # XXX: Should we use python's XML libs instead?
    cmd = ['xmllint'] + options.args(('${PREUPLOAD_FILES}',), filtered)

    return _check_cmd('xmllint', project, commit, cmd)


def check_android_test_mapping(project, commit, _desc, diff, options=None):
    """Verify Android TEST_MAPPING files are valid."""
    if options.args():
        raise ValueError('Android TEST_MAPPING check takes no options')
    filtered = _filter_diff(diff, [r'(^|.*/)TEST_MAPPING$'])
    if not filtered:
        return None

    testmapping_format = options.tool_path('android-test-mapping-format')
    testmapping_args = ['--commit', commit]
    cmd = [testmapping_format] + options.args(
        (project.dir, '${PREUPLOAD_FILES}'), filtered) + testmapping_args
    return _check_cmd('android-test-mapping-format', project, commit, cmd)


def check_aidl_format(project, commit, _desc, diff, options=None):
    """Checks that AIDL files are formatted with aidl-format."""
    # All *.aidl files except for those under aidl_api directory.
    filtered = _filter_diff(diff, [r'\.aidl$'], [r'/aidl_api/'])
    if not filtered:
        return None
    aidl_format = options.tool_path('aidl-format')
    cmd = [aidl_format, '-d'] + options.args((), filtered)
    ret = []
    for d in filtered:
        data = rh.git.get_file_content(commit, d.file)
        result = _run(cmd, input=data)
        if result.stdout:
            fixup_func = _fixup_func_caller([aidl_format, '-w', d.file])
            ret.append(rh.results.HookResult(
                'aidl-format', project, commit, error=result.stdout,
                files=(d.file,), fixup_func=fixup_func))
    return ret


# Hooks that projects can opt into.
# Note: Make sure to keep the top level README.md up to date when adding more!
BUILTIN_HOOKS = {
    'aidl_format': check_aidl_format,
    'android_test_mapping_format': check_android_test_mapping,
    'bpfmt': check_bpfmt,
    'checkpatch': check_checkpatch,
    'clang_format': check_clang_format,
    'commit_msg_bug_field': check_commit_msg_bug_field,
    'commit_msg_changeid_field': check_commit_msg_changeid_field,
    'commit_msg_prebuilt_apk_fields': check_commit_msg_prebuilt_apk_fields,
    'commit_msg_test_field': check_commit_msg_test_field,
    'commit_msg_relnote_field_format': check_commit_msg_relnote_field_format,
    'commit_msg_relnote_for_current_txt':
        check_commit_msg_relnote_for_current_txt,
    'cpplint': check_cpplint,
    'gofmt': check_gofmt,
    'google_java_format': check_google_java_format,
    'jsonlint': check_json,
    'pylint': check_pylint2,
    'pylint2': check_pylint2,
    'pylint3': check_pylint3,
    'rustfmt': check_rustfmt,
    'xmllint': check_xmllint,
}

# Additional tools that the hooks can call with their default values.
# Note: Make sure to keep the top level README.md up to date when adding more!
TOOL_PATHS = {
    'aidl-format': 'aidl-format',
    'android-test-mapping-format':
        os.path.join(TOOLS_DIR, 'android_test_mapping_format.py'),
    'bpfmt': 'bpfmt',
    'clang-format': 'clang-format',
    'cpplint': os.path.join(TOOLS_DIR, 'cpplint.py'),
    'git-clang-format': 'git-clang-format',
    'gofmt': 'gofmt',
    'google-java-format': 'google-java-format',
    'google-java-format-diff': 'google-java-format-diff.py',
    'pylint': 'pylint',
    'rustfmt': 'rustfmt',
}
