#!/usr/bin/python

import re
import subprocess
import sys

# Looks for a string of the form [aosp/branch-name]
AOSP_BRANCH_REGEX = "\[aosp/[^\]]+\]"

AOSP_COMMIT_TAG_REGEX = "AOSP:"
AOSP_COMMIT_LINK_REGEX = "aosp/\d+"
AOSP_INFEASIBLE_REGEX = "Infeasible[ ]?\S+"

ERROR_MESSAGE = """
The source of truth for this project is AOSP. If you are uploading something to
a non-AOSP branch first, please provide a link in your commit message to the
corresponding patch in AOSP. The link should be formatted as follows:

  AOSP: aosp/<patch number>

If it's infeasible for the change to be included in AOSP (for example, if a
change contains confidential or security-sensitive information), please state
that it's infeasible and provide reasoning as follows:

  AOSP: Infeasible <your reasoning here>

If you need to cherry-pick your change from an internal branch to AOSP before
uploading, you can do so locally by adding the internal branch as a remote in
AOSP:
  git remote add goog-master /path/to/your/remote/branch/.git
starting a new branch in AOSP:
  repo start <your-branch-name>-cp
then fetching and cherry-picking the change:
  git fetch goog-master your-branch-name && git cherry-pick FETCH_HEAD
"""

def main():
  if _is_in_aosp():
    sys.exit(0)

  commit_msg = subprocess.check_output(["git", "show",
                                        sys.argv[1], "--no-notes"])
  for commit_line in commit_msg.splitlines():
    if re.search(AOSP_COMMIT_TAG_REGEX, commit_line, re.IGNORECASE):
      _check_aosp_message(commit_line)

  print(ERROR_MESSAGE)
  sys.exit(0)

def _is_in_aosp():
  branch_info = subprocess.check_output(["git", "branch", "-vv"])
  return re.search(AOSP_BRANCH_REGEX, branch_info) is not None

def _check_aosp_message(aosp_line):
  if re.search(AOSP_COMMIT_LINK_REGEX, aosp_line):
    sys.exit(0)

  if re.search(AOSP_INFEASIBLE_REGEX, aosp_line, re.IGNORECASE):
    sys.exit(0)

  print(ERROR_MESSAGE)
  sys.exit(0)

if __name__ == '__main__':
  main()
