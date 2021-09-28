# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""check whether chrome was built with identical code folding."""

from __future__ import print_function

import os
import re
import subprocess


def check_identical_code_folding(dso_path):
  """check whether chrome was built with identical code folding.

  Args:
    dso_path: path to the dso.

  Returns:
    False if the dso is chrome and it was not built with icf,
    True otherwise.
  """

  if not dso_path.endswith('/chrome.debug'):
    return True

  # Run 'nm' on the chrome binary and read the output.
  nm = subprocess.Popen(['nm', dso_path],
                        stdout=subprocess.PIPE,
                        stderr=open(os.devnull, 'w'),
                        encoding='utf-8')
  nm_output, _ = nm.communicate()

  # Search for addresses of text symbols.
  text_addresses = re.findall('^[0-9a-f]+[ ]+[tT] ', nm_output, re.MULTILINE)

  # Calculate number of text symbols in chrome binary.
  num_text_addresses = len(text_addresses)

  # Calculate number of unique text symbols in chrome binary.
  num_unique_text_addresses = len(set(text_addresses))

  # Check that the number of duplicate symbols is at least 10,000.
  #   - https://crbug.com/813272#c18
  if num_text_addresses - num_unique_text_addresses >= 10000:
    return True

  print('%s was not built with ICF' % dso_path)
  print('    num_text_addresses = %d' % num_text_addresses)
  print('    num_unique_text_addresses = %d' % num_unique_text_addresses)
  return False
