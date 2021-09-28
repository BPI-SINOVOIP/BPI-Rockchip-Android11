# Copyright 2019, The Android Open Source Project
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

"""
Cache Finder class.
"""

import atest_utils
from test_finders import test_finder_base
from test_finders import test_info

class CacheFinder(test_finder_base.TestFinderBase):
    """Cache Finder class."""
    NAME = 'CACHE'

    def __init__(self, **kwargs):
        super(CacheFinder, self).__init__()

    def _is_latest_testinfos(self, test_infos):
        """Check whether test_infos are up-to-date.

        Args:
            test_infos: A list of TestInfo.

        Returns:
            True if all keys in test_infos and TestInfo object are equal.
            Otherwise, False.
        """
        sorted_base_ti = sorted(
            vars(test_info.TestInfo(None, None, None)).keys())
        for cached_test_info in test_infos:
            sorted_cache_ti = sorted(vars(cached_test_info).keys())
            if not sorted_cache_ti == sorted_base_ti:
                return False
        return True

    def find_test_by_cache(self, test_reference):
        """Find the matched test_infos in saved caches.

        Args:
            test_reference: A string of the path to the test's file or dir.

        Returns:
            A list of TestInfo namedtuple if cache found and is in latest
            TestInfo format, else None.
        """
        test_infos = atest_utils.load_test_info_cache(test_reference)
        if test_infos and self._is_latest_testinfos(test_infos):
            return test_infos
        return None
