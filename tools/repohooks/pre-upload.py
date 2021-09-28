#!/usr/bin/env python3
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

"""Repo pre-upload hook.

Normally this is loaded indirectly by repo itself, but it can be run directly
when developing.
"""

import argparse
import datetime
import os
import sys


# Assert some minimum Python versions as we don't test or support any others.
if sys.version_info < (3, 6):
    print('repohooks: error: Python-3.6+ is required', file=sys.stderr)
    sys.exit(1)


_path = os.path.dirname(os.path.realpath(__file__))
if sys.path[0] != _path:
    sys.path.insert(0, _path)
del _path

# We have to import our local modules after the sys.path tweak.  We can't use
# relative imports because this is an executable program, not a module.
# pylint: disable=wrong-import-position
import rh
import rh.results
import rh.config
import rh.git
import rh.hooks
import rh.terminal
import rh.utils


# Repohooks homepage.
REPOHOOKS_URL = 'https://android.googlesource.com/platform/tools/repohooks/'


class Output(object):
    """Class for reporting hook status."""

    COLOR = rh.terminal.Color()
    COMMIT = COLOR.color(COLOR.CYAN, 'COMMIT')
    RUNNING = COLOR.color(COLOR.YELLOW, 'RUNNING')
    PASSED = COLOR.color(COLOR.GREEN, 'PASSED')
    FAILED = COLOR.color(COLOR.RED, 'FAILED')
    WARNING = COLOR.color(COLOR.YELLOW, 'WARNING')

    # How long a hook is allowed to run before we warn that it is "too slow".
    _SLOW_HOOK_DURATION = datetime.timedelta(seconds=30)

    def __init__(self, project_name):
        """Create a new Output object for a specified project.

        Args:
          project_name: name of project.
        """
        self.project_name = project_name
        self.num_hooks = None
        self.hook_index = 0
        self.success = True
        self.start_time = datetime.datetime.now()
        self.hook_start_time = None
        self._curr_hook_name = None

    def set_num_hooks(self, num_hooks):
        """Keep track of how many hooks we'll be running.

        Args:
          num_hooks: number of hooks to be run.
        """
        self.num_hooks = num_hooks

    def commit_start(self, commit, commit_summary):
        """Emit status for new commit.

        Args:
          commit: commit hash.
          commit_summary: commit summary.
        """
        status_line = '[%s %s] %s' % (self.COMMIT, commit[0:12], commit_summary)
        rh.terminal.print_status_line(status_line, print_newline=True)
        self.hook_index = 1

    def hook_start(self, hook_name):
        """Emit status before the start of a hook.

        Args:
          hook_name: name of the hook.
        """
        self._curr_hook_name = hook_name
        self.hook_start_time = datetime.datetime.now()
        status_line = '[%s %d/%d] %s' % (self.RUNNING, self.hook_index,
                                         self.num_hooks, hook_name)
        self.hook_index += 1
        rh.terminal.print_status_line(status_line)

    def hook_finish(self):
        """Finish processing any per-hook state."""
        duration = datetime.datetime.now() - self.hook_start_time
        if duration >= self._SLOW_HOOK_DURATION:
            self.hook_warning(
                'This hook took %s to finish which is fairly slow for '
                'developers.\nPlease consider moving the check to the '
                'server/CI system instead.' %
                (rh.utils.timedelta_str(duration),))

    def hook_error(self, error):
        """Print an error for a single hook.

        Args:
          error: error string.
        """
        self.error(self._curr_hook_name, error)

    def hook_warning(self, warning):
        """Print a warning for a single hook.

        Args:
          warning: warning string.
        """
        status_line = '[%s] %s' % (self.WARNING, self._curr_hook_name)
        rh.terminal.print_status_line(status_line, print_newline=True)
        print(warning, file=sys.stderr)

    def error(self, header, error):
        """Print a general error.

        Args:
          header: A unique identifier for the source of this error.
          error: error string.
        """
        status_line = '[%s] %s' % (self.FAILED, header)
        rh.terminal.print_status_line(status_line, print_newline=True)
        print(error, file=sys.stderr)
        self.success = False

    def finish(self):
        """Print summary for all the hooks."""
        status_line = '[%s] repohooks for %s %s in %s' % (
            self.PASSED if self.success else self.FAILED,
            self.project_name,
            'passed' if self.success else 'failed',
            rh.utils.timedelta_str(datetime.datetime.now() - self.start_time))
        rh.terminal.print_status_line(status_line, print_newline=True)


def _process_hook_results(results):
    """Returns an error string if an error occurred.

    Args:
      results: A list of HookResult objects, or None.

    Returns:
      error output if an error occurred, otherwise None
      warning output if an error occurred, otherwise None
    """
    if not results:
        return (None, None)

    # We track these as dedicated fields in case a hook doesn't output anything.
    # We want to treat silent non-zero exits as failures too.
    has_error = False
    has_warning = False

    error_ret = ''
    warning_ret = ''
    for result in results:
        if result:
            ret = ''
            if result.files:
                ret += '  FILES: %s' % (result.files,)
            lines = result.error.splitlines()
            ret += '\n'.join('    %s' % (x,) for x in lines)
            if result.is_warning():
                has_warning = True
                warning_ret += ret
            else:
                has_error = True
                error_ret += ret

    return (error_ret if has_error else None,
            warning_ret if has_warning else None)


def _get_project_config():
    """Returns the configuration for a project.

    Expects to be called from within the project root.
    """
    global_paths = (
        # Load the global config found in the manifest repo.
        os.path.join(rh.git.find_repo_root(), '.repo', 'manifests'),
        # Load the global config found in the root of the repo checkout.
        rh.git.find_repo_root(),
    )
    paths = (
        # Load the config for this git repo.
        '.',
    )
    return rh.config.PreUploadSettings(paths=paths, global_paths=global_paths)


def _attempt_fixes(fixup_func_list, commit_list):
    """Attempts to run |fixup_func_list| given |commit_list|."""
    if len(fixup_func_list) != 1:
        # Only single fixes will be attempted, since various fixes might
        # interact with each other.
        return

    hook_name, commit, fixup_func = fixup_func_list[0]

    if commit != commit_list[0]:
        # If the commit is not at the top of the stack, git operations might be
        # needed and might leave the working directory in a tricky state if the
        # fix is attempted to run automatically (e.g. it might require manual
        # merge conflict resolution). Refuse to run the fix in those cases.
        return

    prompt = ('An automatic fix can be attempted for the "%s" hook. '
              'Do you want to run it?' % hook_name)
    if not rh.terminal.boolean_prompt(prompt):
        return

    result = fixup_func()
    if result:
        print('Attempt to fix "%s" for commit "%s" failed: %s' %
              (hook_name, commit, result),
              file=sys.stderr)
    else:
        print('Fix successfully applied. Amend the current commit before '
              'attempting to upload again.\n', file=sys.stderr)


def _run_project_hooks_in_cwd(project_name, proj_dir, output, commit_list=None):
    """Run the project-specific hooks in the cwd.

    Args:
      project_name: The name of this project.
      proj_dir: The directory for this project (for passing on in metadata).
      output: Helper for summarizing output/errors to the user.
      commit_list: A list of commits to run hooks against.  If None or empty
          list then we'll automatically get the list of commits that would be
          uploaded.

    Returns:
      False if any errors were found, else True.
    """
    try:
        config = _get_project_config()
    except rh.config.ValidationError as e:
        output.error('Loading config files', str(e))
        return False

    # If the repo has no pre-upload hooks enabled, then just return.
    hooks = list(config.callable_hooks())
    if not hooks:
        return True

    output.set_num_hooks(len(hooks))

    # Set up the environment like repo would with the forall command.
    try:
        remote = rh.git.get_upstream_remote()
        upstream_branch = rh.git.get_upstream_branch()
    except rh.utils.CalledProcessError as e:
        output.error('Upstream remote/tracking branch lookup',
                     '%s\nDid you run repo start?  Is your HEAD detached?' %
                     (e,))
        return False

    project = rh.Project(name=project_name, dir=proj_dir, remote=remote)
    rel_proj_dir = os.path.relpath(proj_dir, rh.git.find_repo_root())

    os.environ.update({
        'REPO_LREV': rh.git.get_commit_for_ref(upstream_branch),
        'REPO_PATH': rel_proj_dir,
        'REPO_PROJECT': project_name,
        'REPO_REMOTE': remote,
        'REPO_RREV': rh.git.get_remote_revision(upstream_branch, remote),
    })

    if not commit_list:
        commit_list = rh.git.get_commits(
            ignore_merged_commits=config.ignore_merged_commits)

    ret = True
    fixup_func_list = []

    for commit in commit_list:
        # Mix in some settings for our hooks.
        os.environ['PREUPLOAD_COMMIT'] = commit
        diff = rh.git.get_affected_files(commit)
        desc = rh.git.get_commit_desc(commit)
        os.environ['PREUPLOAD_COMMIT_MESSAGE'] = desc

        commit_summary = desc.split('\n', 1)[0]
        output.commit_start(commit=commit, commit_summary=commit_summary)

        for name, hook, exclusion_scope in hooks:
            output.hook_start(name)
            if rel_proj_dir in exclusion_scope:
                break
            hook_results = hook(project, commit, desc, diff)
            output.hook_finish()
            (error, warning) = _process_hook_results(hook_results)
            if error is not None or warning is not None:
                if warning is not None:
                    output.hook_warning(warning)
                if error is not None:
                    ret = False
                    output.hook_error(error)
                for result in hook_results:
                    if result.fixup_func:
                        fixup_func_list.append((name, commit,
                                                result.fixup_func))

    if fixup_func_list:
        _attempt_fixes(fixup_func_list, commit_list)

    return ret


def _run_project_hooks(project_name, proj_dir=None, commit_list=None):
    """Run the project-specific hooks in |proj_dir|.

    Args:
      project_name: The name of project to run hooks for.
      proj_dir: If non-None, this is the directory the project is in.  If None,
          we'll ask repo.
      commit_list: A list of commits to run hooks against.  If None or empty
          list then we'll automatically get the list of commits that would be
          uploaded.

    Returns:
      False if any errors were found, else True.
    """
    output = Output(project_name)

    if proj_dir is None:
        cmd = ['repo', 'forall', project_name, '-c', 'pwd']
        result = rh.utils.run(cmd, capture_output=True)
        proj_dirs = result.stdout.split()
        if not proj_dirs:
            print('%s cannot be found.' % project_name, file=sys.stderr)
            print('Please specify a valid project.', file=sys.stderr)
            return False
        if len(proj_dirs) > 1:
            print('%s is associated with multiple directories.' % project_name,
                  file=sys.stderr)
            print('Please specify a directory to help disambiguate.',
                  file=sys.stderr)
            return False
        proj_dir = proj_dirs[0]

    pwd = os.getcwd()
    try:
        # Hooks assume they are run from the root of the project.
        os.chdir(proj_dir)
        return _run_project_hooks_in_cwd(project_name, proj_dir, output,
                                         commit_list=commit_list)
    finally:
        output.finish()
        os.chdir(pwd)


def main(project_list, worktree_list=None, **_kwargs):
    """Main function invoked directly by repo.

    We must use the name "main" as that is what repo requires.

    This function will exit directly upon error so that repo doesn't print some
    obscure error message.

    Args:
      project_list: List of projects to run on.
      worktree_list: A list of directories.  It should be the same length as
          project_list, so that each entry in project_list matches with a
          directory in worktree_list.  If None, we will attempt to calculate
          the directories automatically.
      kwargs: Leave this here for forward-compatibility.
    """
    found_error = False
    if not worktree_list:
        worktree_list = [None] * len(project_list)
    for project, worktree in zip(project_list, worktree_list):
        if not _run_project_hooks(project, proj_dir=worktree):
            found_error = True
            # If a repo had failures, add a blank line to help break up the
            # output.  If there were no failures, then the output should be
            # very minimal, so we don't add it then.
            print('', file=sys.stderr)

    if found_error:
        color = rh.terminal.Color()
        print('%s: Preupload failed due to above error(s).\n'
              'For more info, please see:\n%s' %
              (color.color(color.RED, 'FATAL'), REPOHOOKS_URL),
              file=sys.stderr)
        sys.exit(1)


def _identify_project(path):
    """Identify the repo project associated with the given path.

    Returns:
      A string indicating what project is associated with the path passed in or
      a blank string upon failure.
    """
    cmd = ['repo', 'forall', '.', '-c', 'echo ${REPO_PROJECT}']
    return rh.utils.run(cmd, capture_output=True, cwd=path).stdout.strip()


def direct_main(argv):
    """Run hooks directly (outside of the context of repo).

    Args:
      argv: The command line args to process.

    Returns:
      0 if no pre-upload failures, 1 if failures.

    Raises:
      BadInvocation: On some types of invocation errors.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dir', default=None,
                        help='The directory that the project lives in.  If not '
                        'specified, use the git project root based on the cwd.')
    parser.add_argument('--project', default=None,
                        help='The project repo path; this can affect how the '
                        'hooks get run, since some hooks are project-specific.'
                        'If not specified, `repo` will be used to figure this '
                        'out based on the dir.')
    parser.add_argument('commits', nargs='*',
                        help='Check specific commits')
    opts = parser.parse_args(argv)

    # Check/normalize git dir; if unspecified, we'll use the root of the git
    # project from CWD.
    if opts.dir is None:
        cmd = ['git', 'rev-parse', '--git-dir']
        git_dir = rh.utils.run(cmd, capture_output=True).stdout.strip()
        if not git_dir:
            parser.error('The current directory is not part of a git project.')
        opts.dir = os.path.dirname(os.path.abspath(git_dir))
    elif not os.path.isdir(opts.dir):
        parser.error('Invalid dir: %s' % opts.dir)
    elif not rh.git.is_git_repository(opts.dir):
        parser.error('Not a git repository: %s' % opts.dir)

    # Identify the project if it wasn't specified; this _requires_ the repo
    # tool to be installed and for the project to be part of a repo checkout.
    if not opts.project:
        opts.project = _identify_project(opts.dir)
        if not opts.project:
            parser.error("Repo couldn't identify the project of %s" % opts.dir)

    if _run_project_hooks(opts.project, proj_dir=opts.dir,
                          commit_list=opts.commits):
        return 0
    return 1


if __name__ == '__main__':
    sys.exit(direct_main(sys.argv[1:]))
