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
"""Unit tests for external updater."""

import unittest

import github_archive_updater


class ExternalUpdaterTest(unittest.TestCase):
    """Unit tests for external updater."""

    def test_url_selection(self):
        """Tests that GithubArchiveUpdater can choose the right url."""
        prefix = "https://github.com/author/project/"
        urls = [
            prefix + "releases/download/ver-1.0/ver-1.0-binary.zip",
            prefix + "releases/download/ver-1.0/ver-1.0-binary.tar.gz",
            prefix + "releases/download/ver-1.0/ver-1.0-src.zip",
            prefix + "releases/download/ver-1.0/ver-1.0-src.tar.gz",
            prefix + "archive/ver-1.0.zip",
            prefix + "archive/ver-1.0.tar.gz",
        ]

        previous_url = prefix + "releases/download/ver-0.9/ver-0.9-src.tar.gz"
        url = github_archive_updater.choose_best_url(urls, previous_url)
        expected_url = prefix + "releases/download/ver-1.0/ver-1.0-src.tar.gz"
        self.assertEqual(url, expected_url)

        previous_url = prefix + "archive/ver-0.9.zip"
        url = github_archive_updater.choose_best_url(urls, previous_url)
        expected_url = prefix + "archive/ver-1.0.zip"
        self.assertEqual(url, expected_url)


if __name__ == '__main__':
    unittest.main()
