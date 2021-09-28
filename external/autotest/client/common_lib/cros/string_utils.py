# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A collection of classes/functions to manipulate strings."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import bisect


class StringTooLongError(Exception):
    """Raised when string is too long to manipulate."""


def join_longest_with_length_limit(string_list, length_limit, separator='',
                                   do_join=True):
    """Join strings to meet length limit and yield results.

    Join strings from |string_list| using |separator| and yield the results.
    Each result string should be as long as possible while still shorter than
    |length_limit|. In other words, this function yields minimum number of
    result strings.

    An error will be raised when any string in |string_list| is longer than
    |length_limit| because the result string joined must be longer than
    |length_limit| in any case.

    @param string_list: A list of strings to be joined.
    @param length_limit: The maximum length of the result string.
    @param separator: The separator to join strings.
    @param do_join: join the result list to string if True, else just return the
        result list.

    @yield The result string.
    @throws StringTooLongError when any string in |string_list| is longer than
        |length_limit|."""
    # The basic idea is, always select longest string which shorter than length
    # limit, then update the limit with subtracting the length of selected
    # string plus separator. Repeat the process until no more strings shorter
    # than the updated limit. Then yield the result and start next loop.

    string_list = sorted(string_list, key=len)
    # The length of longest string should shorter than the limit.
    if len(string_list[-1]) > length_limit:
        raise StringTooLongError('At least one string is longer than length '
                                 'limit: %s' % length_limit)

    length_list = [len(s) for s in string_list]
    len_sep = len(separator)
    length_limit += len_sep
    # Call str.join directly when possible.
    if sum(length_list) + len_sep * len(string_list) <= length_limit:
        yield separator.join(string_list) if do_join else string_list
        return

    result = []
    new_length_limit = length_limit
    while string_list:
        index = bisect.bisect_right(length_list,
                                    new_length_limit - len_sep) - 1
        if index < 0:  # All available strings are longer than the limit.
            yield separator.join(result) if do_join else result
            result = []
            new_length_limit = length_limit
            continue

        result.append(string_list.pop(index))
        new_length_limit -= length_list.pop(index) + len_sep

    if result:
        yield separator.join(result) if do_join else result
