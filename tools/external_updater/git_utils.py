# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
'''Helper functions to communicate with Git.'''

import datetime
import re
import subprocess


def _run(cmd, cwd, redirect=True):
    """Runs a command with stdout and stderr redirected."""
    out = subprocess.PIPE if redirect else None
    return subprocess.run(cmd, stdout=out, stderr=out,
                          check=True, cwd=cwd)


def fetch(proj_path, remote_names):
    """Runs git fetch.

    Args:
        proj_path: Path to Git repository.
        remote_names: Array of string to specify remote names.
    """
    _run(['git', 'fetch', '--multiple'] + remote_names, cwd=proj_path)


def add_remote(proj_path, name, url):
    """Adds a git remote.

    Args:
        proj_path: Path to Git repository.
        name: Name of the new remote.
        url: Url of the new remote.
    """
    _run(['git', 'remote', 'add', name, url], cwd=proj_path)


def list_remotes(proj_path):
    """Lists all Git remotes.

    Args:
        proj_path: Path to Git repository.

    Returns:
        A dict from remote name to remote url.
    """
    out = _run(['git', 'remote', '-v'], proj_path)
    lines = out.stdout.decode('utf-8').splitlines()
    return dict([line.split()[0:2] for line in lines])


def get_commits_ahead(proj_path, branch, base_branch):
    """Lists commits in `branch` but not `base_branch`."""
    out = _run(['git', 'rev-list', '--left-only', '--ancestry-path',
                '{}...{}'.format(branch, base_branch)],
               proj_path)
    return out.stdout.decode('utf-8').splitlines()


def get_commit_time(proj_path, commit):
    """Gets commit time of one commit."""
    out = _run(['git', 'show', '-s', '--format=%ct', commit], cwd=proj_path)
    return datetime.datetime.fromtimestamp(int(out.stdout))


def list_remote_branches(proj_path, remote_name):
    """Lists all branches for a remote."""
    out = _run(['git', 'branch', '-r'], cwd=proj_path)
    lines = out.stdout.decode('utf-8').splitlines()
    stripped = [line.strip() for line in lines]
    remote_path = remote_name + '/'
    remote_path_len = len(remote_path)
    return [line[remote_path_len:] for line in stripped
            if line.startswith(remote_path)]


def _parse_remote_tag(line):
    tag_prefix = 'refs/tags/'
    tag_suffix = '^{}'
    try:
        line = line[line.index(tag_prefix):]
    except ValueError:
        return None
    line = line[len(tag_prefix):]
    if line.endswith(tag_suffix):
        line = line[:-len(tag_suffix)]
    return line


def list_remote_tags(proj_path, remote_name):
    """Lists all tags for a remote."""
    out = _run(['git', "ls-remote", "--tags", remote_name],
               cwd=proj_path)
    lines = out.stdout.decode('utf-8').splitlines()
    tags = [_parse_remote_tag(line) for line in lines]
    return list(set(tags))


COMMIT_PATTERN = r'^[a-f0-9]{40}$'
COMMIT_RE = re.compile(COMMIT_PATTERN)


def is_commit(commit):
    """Whether a string looks like a SHA1 hash."""
    return bool(COMMIT_RE.match(commit))


def merge(proj_path, branch):
    """Merges a branch."""
    try:
        out = _run(['git', 'merge', branch, '--no-commit'],
                   cwd=proj_path)
    except subprocess.CalledProcessError:
        # Merge failed. Error is already written to console.
        subprocess.run(['git', 'merge', '--abort'], cwd=proj_path)
        raise


def add_file(proj_path, file_name):
    """Stages a file."""
    _run(['git', 'add', file_name], cwd=proj_path)


def delete_branch(proj_path, branch_name):
    """Force delete a branch."""
    _run(['git', 'branch', '-D', branch_name], cwd=proj_path)


def start_branch(proj_path, branch_name):
    """Starts a new repo branch."""
    _run(['repo', 'start', branch_name], cwd=proj_path)


def commit(proj_path, message):
    """Commits changes."""
    _run(['git', 'commit', '-m', message], cwd=proj_path)


def checkout(proj_path, branch_name):
    """Checkouts a branch."""
    _run(['git', 'checkout', branch_name], cwd=proj_path)


def push(proj_path, remote_name):
    """Pushes change to remote."""
    return _run(['git', 'push', remote_name, 'HEAD:refs/for/master'],
                cwd=proj_path, redirect=False)
