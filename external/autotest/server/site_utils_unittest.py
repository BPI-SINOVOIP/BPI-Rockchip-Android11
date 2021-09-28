# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mox
import unittest

import common
from autotest_lib.frontend import setup_django_lite_environment
from autotest_lib.server import frontend
from autotest_lib.server import site_utils
from autotest_lib.server.cros.dynamic_suite import tools
from autotest_lib.server.cros.dynamic_suite import suite_common


class SiteUtilsUnittests(mox.MoxTestBase):
    """Test functions in site_utils.py"""

    def testParseJobName(self):
        """Test method parse_job_name.
        """
        trybot_paladin_build = 'trybot-lumpy-paladin/R27-3837.0.0-b123'
        trybot_release_build = 'trybot-lumpy-release/R27-3837.0.0-b456'
        release_build = 'lumpy-release/R27-3837.0.0'
        paladin_build = 'lumpy-paladin/R27-3878.0.0-rc7'
        brillo_build = 'git_mnc-brillo-dev/lumpy-eng/1234'
        chrome_pfq_build = 'lumpy-chrome-pfq/R27-3837.0.0'
        chromium_pfq_build = 'lumpy-chromium-pfq/R27-3837.0.0'

        builds = [trybot_paladin_build, trybot_release_build, release_build,
                  paladin_build, brillo_build, chrome_pfq_build,
                  chromium_pfq_build]
        test_name = 'login_LoginSuccess'
        board = 'lumpy'
        suite = 'bvt'
        for build in builds:
            expected_info = {'board': board,
                             'suite': suite,
                             'build': build}
            build_parts = build.split('/')
            if len(build_parts) == 2:
                expected_info['build_version'] = build_parts[1]
            else:
                expected_info['build_version'] = build_parts[2]
            suite_job_name = ('%s-%s' %
                    (build, suite_common.canonicalize_suite_name(suite)))
            info = site_utils.parse_job_name(suite_job_name)
            self.assertEqual(info, expected_info, '%s failed to be parsed to '
                             '%s' % (suite_job_name, expected_info))
            test_job_name = tools.create_job_name(build, suite, test_name)
            info = site_utils.parse_job_name(test_job_name)
            self.assertEqual(info, expected_info, '%s failed to be parsed to '
                             '%s' % (test_job_name, expected_info))


    def testGetViewsFromTko(self):
        """Test method get_test_views_from_tko
        """
        test_results = [
            ('dummy_Pass', 'GOOD'),
            ('dummy_Fail.RetrySuccess', 'GOOD'),
            ('dummy_Fail.RetrySuccess', 'FAIL'),
            ('dummy_Fail.Fail', 'FAIL'),
            ('dummy_Fail.Fail', 'FAIL'),
        ]

        expected_test_views = {
            'dummy_Pass': ['GOOD'],
            'dummy_Fail.RetrySuccess': ['FAIL', 'GOOD'],
            'dummy_Fail.Fail': ['FAIL', 'FAIL'],
        }

        self.mox.UnsetStubs()
        tko = self.mox.CreateMock(frontend.TKO)
        tko.run('get_detailed_test_views', afe_job_id=0).AndReturn(
            [{'test_name':r[0], 'status':r[1]} for r in test_results])

        self.mox.ReplayAll()
        test_views = site_utils.get_test_views_from_tko(0, tko)
        self.mox.VerifyAll()

        self.assertEqual(sorted(test_views.keys()),
                         sorted(expected_test_views.keys()),
                         'Test list %s does not match expected test list %s.' %
                         (sorted(test_views.keys()),
                          sorted(expected_test_views.keys())))

        for test_name, test_status_list in test_views.iteritems():
            self.assertEqual(sorted(test_status_list),
                             sorted(expected_test_views[test_name]),
                             'For test %s the status list %s does not match '
                             'expected status list %s.' %
                             (test_name,
                              sorted(test_status_list),
                              sorted(expected_test_views[test_name])))


if __name__ == '__main__':
    unittest.main()
