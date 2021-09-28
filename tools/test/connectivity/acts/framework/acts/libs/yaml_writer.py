#!/usr/bin/env python3
#
# Copyright 2019 - The Android Open Source Project
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

import collections
import yaml

# Allow yaml to dump OrderedDict
yaml.add_representer(collections.OrderedDict,
                     lambda dumper, data: dumper.represent_dict(data),
                     Dumper=yaml.SafeDumper)


def _str_representer(dumper, data):
    if len(data.splitlines()) > 1:
        data = '\n'.join(line.rstrip() for line in data.splitlines())
        return dumper.represent_scalar('tag:yaml.org,2002:str', data, style='|')
    return dumper.represent_scalar('tag:yaml.org,2002:str', data)


# Automatically convert multiline strings into block literals
yaml.add_representer(str, _str_representer, Dumper=yaml.SafeDumper)


_DUMP_KWARGS = dict(explicit_start=True, allow_unicode=True, indent=4)
if yaml.__version__ >= '5.1':
    _DUMP_KWARGS.update(sort_keys=False)


def safe_dump(content, file):
    """Calls yaml.safe_dump to write content to the file, with additional
    parameters from _DUMP_KWARGS."""
    yaml.safe_dump(content, file, **_DUMP_KWARGS)
