# Copyright (C) 2020 Google LLC
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

r"""Utility class for handling protobuf message."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from google.protobuf import descriptor
from six.moves import xrange  # pylint:disable=redefined-builtin


def NormalizeRepeatedFields(protobuf_message):
  """Sorts all repeated fields and removes duplicates.

  Modifies pb in place.

  Args:
    protobuf_message: The Message object to normalize.

  Returns:
    protobuf_message, modified in place.
  """
  for desc, values in protobuf_message.ListFields():
    if desc.label is descriptor.FieldDescriptor.LABEL_REPEATED:
      # Sort then de-dup
      values.sort()
      # De-dupe in place. Can't use set, etc. because messages aren't
      # hashable.
      for i in xrange(len(values) - 1, 0, -1):
        if values[i] == values[i - 1]:
          del values[i]

  return protobuf_message


def Proto2Equals(a, b):
  """Tests if two proto2 objects are equal.

  Recurses into nested messages. Uses list (not set) semantics for comparing
  repeated fields, ie duplicates and order matter.

  Returns:
    boolean
  """
  return (a.SerializeToString(deterministic=True)
          == b.SerializeToString(deterministic=True))
