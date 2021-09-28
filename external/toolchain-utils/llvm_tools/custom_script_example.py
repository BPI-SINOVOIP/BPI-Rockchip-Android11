#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A custom script example that utilizes the .JSON contents of the tryjob."""

from __future__ import print_function

import json
import sys

from update_tryjob_status import TryjobStatus


def main():
  """Determines the exit code based off of the contents of the .JSON file."""

  # Index 1 in 'sys.argv' is the path to the .JSON file which contains
  # the contents of the tryjob.
  #
  # Format of the tryjob contents:
  #   {
  #     "status" : [TRYJOB_STATUS],
  #     "buildbucket_id" : [BUILDBUCKET_ID],
  #     "extra_cls" : [A_LIST_OF_EXTRA_CLS_PASSED_TO_TRYJOB],
  #     "url" : [GERRIT_URL],
  #     "builder" : [TRYJOB_BUILDER_LIST],
  #     "rev" : [REVISION],
  #     "link" : [LINK_TO_TRYJOB],
  #     "options" : [A_LIST_OF_OPTIONS_PASSED_TO_TRYJOB]
  #   }
  abs_path_json_file = sys.argv[1]

  with open(abs_path_json_file) as f:
    tryjob_contents = json.load(f)

  CUTOFF_PENDING_REVISION = 369416

  SKIP_REVISION_CUTOFF_START = 369420
  SKIP_REVISION_CUTOFF_END = 369428

  if tryjob_contents['status'] == TryjobStatus.PENDING.value:
    if tryjob_contents['rev'] <= CUTOFF_PENDING_REVISION:
      # Exit code 0 means to set the tryjob 'status' as 'good'.
      sys.exit(0)

    # Exit code 124 means to set the tryjob 'status' as 'bad'.
    sys.exit(124)

  if tryjob_contents['status'] == TryjobStatus.BAD.value:
    # Need to take a closer look at the contents of the tryjob to then decide
    # what that tryjob's 'status' value should be.
    #
    # Since the exit code is not in the mapping, an exception will occur which
    # will save the file in the directory of this custom script example.
    sys.exit(1)

  if tryjob_contents['status'] == TryjobStatus.SKIP.value:
    # Validate that the 'skip value is really set between the cutoffs.
    if SKIP_REVISION_CUTOFF_START < tryjob_contents['rev'] < \
        SKIP_REVISION_CUTOFF_END:
      # Exit code 125 means to set the tryjob 'status' as 'skip'.
      sys.exit(125)

    if tryjob_contents['rev'] >= SKIP_REVISION_CUTOFF_END:
      sys.exit(124)


if __name__ == '__main__':
  main()
