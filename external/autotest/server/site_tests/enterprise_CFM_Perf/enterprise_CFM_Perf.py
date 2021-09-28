# Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import file_utils
from autotest_lib.client.common_lib.cros import system_metrics_collector
from autotest_lib.server.cros.cfm import cfm_base_test
from autotest_lib.server.cros.cfm.utils import bond_http_api
from autotest_lib.server.cros.cfm.utils import perf_metrics_collector


_BOT_PARTICIPANTS_COUNT = 10
_TOTAL_TEST_DURATION_SECONDS = 15 * 60 # 15 minutes

_DOWNLOAD_BASE = ('http://commondatastorage.googleapis.com/'
                  'chromiumos-test-assets-public/crowd/')
_VIDEO_NAME = 'crowd720_25frames.y4m'


class ParticipantCountMetric(system_metrics_collector.Metric):
    """
    Metric for getting the current participant count in a call.
    """
    def __init__(self, cfm_facade):
        """
        Initializes with a cfm_facade.

        @param cfm_facade object having a get_participant_count() method.
        """
        super(ParticipantCountMetric, self).__init__(
                'participant_count',
                'participants',
                higher_is_better=True)
        self.cfm_facade = cfm_facade

    def collect_metric(self):
        """
        Collects one metric value.
        """
        self.values.append(self.cfm_facade.get_participant_count())

class enterprise_CFM_Perf(cfm_base_test.CfmBaseTest):
    """This is a server test which clears device TPM and runs
    enterprise_RemoraRequisition client test to enroll the device in to hotrod
    mode. After enrollment is successful, it collects and logs cpu, memory and
    temperature data from the device under test."""
    version = 1

    def _download_test_video(self):
        """
        Downloads the test video to a temporary directory on host.

        @return the remote path of the downloaded video.
        """
        url = _DOWNLOAD_BASE + _VIDEO_NAME
        local_path = os.path.join(self.tmpdir, _VIDEO_NAME)
        logging.info('Downloading %s to %s', url, local_path)
        file_utils.download_file(url, local_path)
        # The directory returned by get_tmp_dir() is automatically deleted.
        tmp_dir = self._host.get_tmp_dir()
        remote_path = os.path.join(tmp_dir, _VIDEO_NAME)
        # The temporary directory has mode 700 by default. Chrome runs with a
        # different user so cannot access it unless we change the permissions.
        logging.info('chmodding tmpdir %s to 755', tmp_dir)
        self._host.run('chmod 755 %s' % tmp_dir)
        logging.info('Sending %s to %s on DUT', local_path, remote_path)
        self._host.send_file(local_path, remote_path)
        os.remove(local_path)
        return remote_path

    def initialize(self, host, run_test_only=False, use_bond=True):
        """
        Initializes common test properties.

        @param host: a host object representing the DUT.
        @param run_test_only: Whether to run only the test or to also perform
            deprovisioning, enrollment and system reboot. See cfm_base_test.
        @param use_bond: Whether to use BonD to add bots to the meeting. Useful
            for local testing.
        """
        super(enterprise_CFM_Perf, self).initialize(host, run_test_only)
        self._host = host
        self._use_bond = use_bond
        system_facade = self._facade_factory.create_system_facade()
        self._perf_metrics_collector = (
            perf_metrics_collector.PerfMetricsCollector(
                system_facade,
                self.cfm_facade,
                self.output_perf_value,
                additional_system_metrics=[
                    ParticipantCountMetric(self.cfm_facade),
                ]))

    def setup(self):
        """
        Download video for fake media and restart Chrome with fake media flags.

        This runs after initialize().
        """
        super(enterprise_CFM_Perf, self).setup()
        remote_video_path = self._download_test_video()
        # Restart chrome with fake media flags.
        extra_chrome_args=[
                '--use-fake-device-for-media-stream',
                '--use-file-for-fake-video-capture=%s' % remote_video_path
        ]
        self.cfm_facade.restart_chrome_for_cfm(extra_chrome_args)
        if self._use_bond:
            self.bond = bond_http_api.BondHttpApi()

    def run_once(self):
        """Joins a meeting and collects perf data."""
        self.cfm_facade.wait_for_meetings_landing_page()

        if self._use_bond:
            meeting_code = self.bond.CreateConference()
            logging.info('Started meeting "%s"', meeting_code)
            self._add_bots(_BOT_PARTICIPANTS_COUNT, meeting_code)
            self.cfm_facade.join_meeting_session(meeting_code)
        else:
            self.cfm_facade.start_meeting_session()

        self.cfm_facade.unmute_mic()

        self._perf_metrics_collector.start()
        time.sleep(_TOTAL_TEST_DURATION_SECONDS)
        self._perf_metrics_collector.stop()

        self.cfm_facade.end_meeting_session()
        self._perf_metrics_collector.upload_metrics()

    def _add_bots(self, bot_count, meeting_code):
        """Adds bots to a meeting and configures audio and pinning settings.

        If we were not able to start enough bots end the test run.
        """
        botIds = self.bond.AddBotsRequest(
            meeting_code,
            bot_count,
            _TOTAL_TEST_DURATION_SECONDS + 30);

        if len(botIds) < bot_count:
            # If we did not manage to start enough bots, free up the
            # resources and end the test run.
            self.bond.ExecuteScript('@all leave', meeting_code)
            raise error.TestNAError("Not enough bot resources.\n"
                "Wanted: %d. Started: %d" % (bot_count, len(botIds)))

        # Configure philosopher audio for one bot.
        self._start_philosopher_audio(botIds[0], meeting_code)

        # Pin the CfM from one bot so the device always sends HD.
        self.bond.ExecuteScript(
            '@b%d pin_participant_by_name "Unknown"' % botIds[0], meeting_code)
        # Explicitly request HD video from the CfM.
        self.bond.ExecuteScript(
            '@b%d set_resolution 1280 720' % botIds[0], meeting_code)

    def _start_philosopher_audio(self, bot_id, meeting_code):
        self.bond.ExecuteScript(
            '@b%d start_philosopher_audio' % bot_id, meeting_code)
