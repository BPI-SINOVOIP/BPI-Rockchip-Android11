#
# Copyright (C) 2018 The Android Open Source Project
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
#
"""This file contains ELF utility functions."""


def ByteToInt(obj):
    """Converts an element of a bytes object to an integer."""
    return obj if isinstance(obj, int) else ord(obj)


def BytesToString(obj):
    """Converts bytes to a python3 string."""
    return obj if isinstance(obj, str) else obj.decode("utf-8")


def DecodeSLEB128(data, begin_offset=0):
    """Decode one int64 from SLEB128 encoded bytes.

    Args:
        data: A bytes object to decode.
        begin_offset: An integer, offset in data to start decode from.

    Returns:
        A tuple (value, num), the decoded value and number of consumed bytes.

    Raises:
        IndexError: String index out of range.
    """
    cur = begin_offset
    value = 0
    shift = 0
    while True:
        try:
            byte, cur = ByteToInt(data[cur]), cur + 1
        except IndexError:
            raise
        value |= (byte & 0x7F) << shift
        shift += 7
        if byte & 0x80 == 0:
            break
    if byte & 0x40:
        value |= (-1) << shift
    return value, cur - begin_offset
