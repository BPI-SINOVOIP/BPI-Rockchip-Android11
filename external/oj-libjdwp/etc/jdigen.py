# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from __future__ import print_function
import sys

args = sys.argv

if len(args) != 3:
  print("Usage: jdigen <input> <output>")
  sys.exit(1)

TEMPLATE = """
// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.sun.tools.jdi.resources;
import java.util.ListResourceBundle;
public final class jdi extends ListResourceBundle {{
  protected final Object[][] getContents() {{
    return new Object[][] {{
      {values}
    }};
  }}
}}
"""

INSTANCE_FORMAT = '{{ "{key}", "{value}" }},\n'

VALUES = ""
with open(args[1], 'r+') as inp:
  for l in inp.readlines():
    key, value = l.split('=')
    VALUES += INSTANCE_FORMAT.format(key = key.strip(), value = value.strip())

with open(args[2], 'w') as out:
  out.write(TEMPLATE.format(values = VALUES))
