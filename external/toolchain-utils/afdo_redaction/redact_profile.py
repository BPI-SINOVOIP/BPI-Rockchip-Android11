#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to redact apparent ICF'ed symbolsfrom textual AFDO profiles.

AFDO sampling and ICF have an unfortunate interaction that causes a huge
inflation in sample counts. Essentially, if you have N functions ICF'ed to the
same location, one AFDO sample in any of those N functions will count as one
sample in *each* of those N functions.

In practice, there are a few forms of function bodies that are very heavily
ICF'ed (e.g. `ret`, `xor %eax, %eax; ret`, destructors for widely-used types
like std::map...). Recording 28,000 samples across all N thousand logical
functions that point to the same body really hurts our AFDO numbers, given that
our actual sample count across all of Chrome is something around 10,000,000.
(No, really, these are actual numbers. In practice, at the time of writing,
this script eliminates >90% of our AFDO samples by count. Sometimes as high as
98%.)

It reads a textual AFDO profile from stdin, and prints a 'fixed' version of it
to stdout. A summary of what the script actually did is printed to stderr.
"""

from __future__ import division, print_function

import collections
import re
import sys


def _count_samples(samples):
  """Count the total number of samples in a function."""
  line_re = re.compile(r'^(\s*)\d+(?:\.\d+)?: (\d+)\s*$')

  top_level_samples = 0
  all_samples = 0
  for line in samples:
    m = line_re.match(line)
    if not m:
      continue

    spaces, n = m.groups()
    n = int(n)
    all_samples += n
    if len(spaces) == 1:
      top_level_samples += n

  return top_level_samples, all_samples


# A ProfileRecord is a set of samples for a top-level symbol in a textual AFDO
# profile. function_line is the top line of said function, and `samples` is
# a list of all of the sample lines below that.
#
# We rely on the format of these strings in some places in this script. For
# reference, a full function sample will look something like:
#
# _ZNK5blink10PaintLayer19GetCompositingStateEv:4530:185
#  6: 83
#  15: 126
#  62832: 126
#  6: _ZNK5blink10PaintLayer14GroupedMappingEv:2349
#   1: 206
#   1: _ZNK5blink10PaintLayer14GroupedMappersEv:2060
#    1: 206
#  11: _ZNK5blink10PaintLayer25GetCompositedLayerMappingEv:800
#   2.1: 80
#
#
# In that case, function_line is
# '_ZNK5blink10PaintLayer19GetCompositingStateEv:4530:185', and samples will be
# every line below that.
#
# Function lines look like;
# function_symbol:entry_count:dont_care
#
# And samples look like one of:
#  arbitrary_number: sample_count
#  arbitrary_number: inlined_function_symbol:inlined_entry_count
ProfileRecord = collections.namedtuple('ProfileRecord',
                                       ['function_line', 'samples'])


def _normalize_samples(samples):
  """Normalizes the samples in the given function body.

  Normalization just means that we redact inlined function names. This is
  done so that a bit of templating doesn't make two function bodies look
  distinct. Namely:

  template <typename T>
  __attribute__((noinline))
  int getNumber() { return 1; }

  template <typename T>
  __attribute__((noinline))
  int getNumberIndirectly() { return getNumber<T>(); }

  int main() {
    return getNumber<int>() + getNumber<float>();
  }

  If the profile has the mangled name for getNumber<float> in
  getNumberIndirectly<float> (and similar for <int>), we'll consider them to
  be distinct when they're not.
  """

  # I'm not actually sure if this ends up being an issue in practice, but it's
  # simple enough to guard against.
  inlined_re = re.compile(r'(^\s*\d+): [^:]+:(\s*\d+)\s*$')
  result = []
  for s in samples:
    m = inlined_re.match(s)
    if m:
      result.append('%s: __REDACTED__:%s' % m.groups())
    else:
      result.append(s)
  return tuple(result)


def _read_textual_afdo_profile(stream):
  """Parses an AFDO profile from a line stream into ProfileRecords."""
  # ProfileRecords are actually nested, due to inlining. For the purpose of
  # this script, that doesn't matter.
  lines = (line.rstrip() for line in stream)
  function_line = None
  samples = []
  for line in lines:
    if not line:
      continue

    if line[0].isspace():
      assert function_line is not None, 'sample exists outside of a function?'
      samples.append(line)
      continue

    if function_line is not None:
      yield ProfileRecord(function_line=function_line, samples=tuple(samples))
    function_line = line
    samples = []

  if function_line is not None:
    yield ProfileRecord(function_line=function_line, samples=tuple(samples))


# The default of 100 is arbitrarily selected, but it does make the overwhelming
# majority of obvious sample duplication disappear.
#
# We experimented shortly with an nm-powered version of this script (rather
# than structural matching, we'd see which functions mapped to the same literal
# address). Running this with a high value (100) for max_repeats produced
# results basically indistinguishable from nm, so ...
#
# Non-nm based approaches are superior because they don't require any prior
# build artifacts; just an AFDO profile.
def dedup_records(profile_records, summary_file, max_repeats=100):
  """Removes heavily duplicated records from profile_records.

  profile_records is expected to be an iterable of ProfileRecord.
  max_repeats ia how many functions must share identical bodies for us to
    consider it 'heavily duplicated' and remove the results.
  """

  # Build a mapping of function structure -> list of functions with identical
  # structure and sample counts
  counts = collections.defaultdict(list)
  for record in profile_records:
    counts[_normalize_samples(record.samples)].append(record)

  # Be sure that we didn't see any duplicate functions, since that's bad...
  total_functions_recorded = sum(len(records) for records in counts.values())

  unique_function_names = {
      record.function_line.split(':')[0]
      for records in counts.values()
      for record in records
  }

  assert len(unique_function_names) == total_functions_recorded, \
      'duplicate function names?'

  num_kept = 0
  num_samples_kept = 0
  num_top_samples_kept = 0
  num_total = 0
  num_samples_total = 0
  num_top_samples_total = 0

  for normalized_samples, records in counts.items():
    top_sample_count, all_sample_count = _count_samples(normalized_samples)
    top_sample_count *= len(records)
    all_sample_count *= len(records)

    num_total += len(records)
    num_samples_total += all_sample_count
    num_top_samples_total += top_sample_count

    if len(records) >= max_repeats:
      continue

    num_kept += len(records)
    num_samples_kept += all_sample_count
    num_top_samples_kept += top_sample_count
    for record in records:
      yield record

  print(
      'Retained {:,}/{:,} functions'.format(num_kept, num_total),
      file=summary_file)
  print(
      'Retained {:,}/{:,} samples, total'.format(num_samples_kept,
                                                 num_samples_total),
      file=summary_file)
  print('Retained {:,}/{:,} top-level samples' \
            .format(num_top_samples_kept, num_top_samples_total),
        file=summary_file)


def run(profile_input_file, summary_output_file, profile_output_file):
  profile_records = _read_textual_afdo_profile(profile_input_file)

  # Sort this so we get deterministic output. AFDO doesn't care what order it's
  # in.
  deduped = sorted(
      dedup_records(profile_records, summary_output_file),
      key=lambda r: r.function_line)
  for function_line, samples in deduped:
    print(function_line, file=profile_output_file)
    print('\n'.join(samples), file=profile_output_file)


def _main():
  run(profile_input_file=sys.stdin,
      summary_output_file=sys.stderr,
      profile_output_file=sys.stdout)


if __name__ == '__main__':
  _main()
