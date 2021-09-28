#!/bin/bash -eu
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This is a script crafted to make our Android friends' lives easier: when run
# on their copy of toolchain-utils, this script will do all of the necessary
# merging/branch creation/etc. to make keeping things up-to-date trivial.
#
# For example,
# https://android-review.googlesource.com/c/platform/external/toolchain-utils/+/1132504/1

local_branch_name="merge_with_upstream"
local_upstream="aosp/master"
remote="aosp"
remote_branch="${remote}/upstream-mirror-master"

my_dir="$(dirname "$(readlink -m "$0")")"
cd "${my_dir}"

ensure_head_is_upstream_master() {
  local current_rev master_rev
  current_rev="$(git rev-parse HEAD)"
  master_rev="$(git rev-parse ${local_upstream})"
  if [[ "${current_rev}" != "${master_rev}" ]]; then
    echo "Please checkout ${local_upstream} and rerun this" >&2
    exit
  fi
}

ensure_no_local_branch_present() {
  if ! git rev-parse "${local_branch_name}" >& /dev/null; then
    return 0
  fi

  echo -n "${local_branch_name} is a valid branch already. Delete? [y/N] " >&2

  local line
  read -r line
  if [[ "${line}" != y* && "${line}" != Y* ]]; then
    echo "Aborted" >&2
    exit 1
  fi

  # If we're *on* that branch, deleting it is difficult.
  local current_branch
  current_branch="$(git branch --show-current)"
  if [[ "${current_branch}" == "${local_branch_name}" ]]; then
    local rev
    rev="$(git rev-parse HEAD)"
    # This is fine, since we assume HEAD == upstream-mirror-master anyway
    # (e.g., the existing branch was pointless.)
    git checkout "${rev}"
  fi
  git branch -D "${local_branch_name}"
}

get_merge_commit_list() {
  local merge_base
  merge_base="$(git merge-base HEAD ${remote_branch})"
  git log --oneline "${merge_base}..${remote_branch}"
}

ensure_head_is_upstream_master
ensure_no_local_branch_present

echo "Ensuring repository is up-to-date..."
git fetch "${remote}"
repo start "${local_branch_name}"

commit_list="$(get_merge_commit_list)"
num_commits="$(wc -l <<< "${commit_list}")"
commit_message="Merging ${num_commits} commit(s) from Chromium's toolchain-utils

Merged commit digest:
$(sed 's/^/  /' <<< "${commit_list}")
"

git merge "${remote_branch}" -m "${commit_message}"
echo 'NOTE: When you try to `repo upload`, repo might show a scary warning'
echo 'about the number of changes are being uploaded. That should be fine,'
echo 'since repo will only create CLs for commits not known to our remote.'
