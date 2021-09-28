#!/usr/bin/env python
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

import unittest

try:
    from unittest import mock
except ImportError:
    import mock

from webapp.src.endpoint import build_info
from webapp.src.proto import model
from webapp.src.testing import unittest_base


class BuildInfoTest(unittest_base.UnitTestBase):
    """A class to test build_info endpoint API."""

    def setUp(self):
        """Initializes test"""
        super(BuildInfoTest, self).setUp()

    def testSetNewBuildModel(self):
        """Asserts build_info/set API receives a new build."""
        builds = model.BuildModel.query().fetch()
        self.assertEqual(len(builds), 0)
        container = (
            build_info.BUILD_INFO_RESOURCE.combined_message_class(
                manifest_branch=self.GetRandomString(),
                build_id=self.GetRandomString(),
                build_target=self.GetRandomString(),
                build_type=self.GetRandomString(),
                artifact_type=self.GetRandomString(),
            ))
        api = build_info.BuildInfoApi()
        response = api.set(container)

        self.assertEqual(response.return_code, model.ReturnCodeMessage.SUCCESS)
        builds = model.BuildModel.query().fetch()
        self.assertEqual(len(builds), 1)

    def testSetDuplicatedBuildModel(self):
        """Asserts build_info/set API receives a duplicated build."""
        manifest_branch = self.GetRandomString()
        build_id = self.GetRandomString()
        build_target = self.GetRandomString()
        build_type = self.GetRandomString()
        artifact_type = self.GetRandomString()

        builds = model.BuildModel.query().fetch()
        self.assertEqual(len(builds), 0)
        container = (
            build_info.BUILD_INFO_RESOURCE.combined_message_class(
                manifest_branch=manifest_branch,
                build_id=build_id,
                build_target=build_target,
                build_type=build_type,
                artifact_type=artifact_type,
            ))
        api = build_info.BuildInfoApi()
        response = api.set(container)

        self.assertEqual(response.return_code, model.ReturnCodeMessage.SUCCESS)
        builds = model.BuildModel.query().fetch()
        self.assertEqual(len(builds), 1)

        container = (
            build_info.BUILD_INFO_RESOURCE.combined_message_class(
                manifest_branch=manifest_branch,
                build_id=build_id,
                build_target=build_target,
                build_type=build_type,
                artifact_type=artifact_type,
            ))
        api = build_info.BuildInfoApi()
        response = api.set(container)
        self.assertEqual(response.return_code, model.ReturnCodeMessage.SUCCESS)
        builds = model.BuildModel.query().fetch()
        self.assertEqual(len(builds), 1)

    def testUpdateSignedBuildModel(self):
        """Asserts build_info/set API receives a duplicated build."""
        manifest_branch = self.GetRandomString()
        build_id = self.GetRandomString()
        build_target = self.GetRandomString()
        build_type = self.GetRandomString()
        artifact_type = self.GetRandomString()

        builds = model.BuildModel.query().fetch()
        self.assertEqual(len(builds), 0)
        container = (
            build_info.BUILD_INFO_RESOURCE.combined_message_class(
                manifest_branch=manifest_branch,
                build_id=build_id,
                build_target=build_target,
                build_type=build_type,
                artifact_type=artifact_type,
                signed=False,
            ))
        api = build_info.BuildInfoApi()
        response = api.set(container)

        self.assertEqual(response.return_code, model.ReturnCodeMessage.SUCCESS)
        builds = model.BuildModel.query().fetch()
        self.assertEqual(len(builds), 1)

        container = (
            build_info.BUILD_INFO_RESOURCE.combined_message_class(
                manifest_branch=manifest_branch,
                build_id=build_id,
                build_target=build_target,
                build_type=build_type,
                artifact_type=artifact_type,
                signed=True
            ))
        api = build_info.BuildInfoApi()
        response = api.set(container)
        self.assertEqual(response.return_code, model.ReturnCodeMessage.SUCCESS)
        builds = model.BuildModel.query().fetch()
        self.assertEqual(len(builds), 1)
        self.assertEqual(builds[0].signed, True)


if __name__ == "__main__":
    unittest.main()
