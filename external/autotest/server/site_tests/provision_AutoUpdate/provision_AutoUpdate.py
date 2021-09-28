# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re
import sys
import time
import urllib2

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import global_config
from autotest_lib.client.common_lib.cros import dev_server
from autotest_lib.server import afe_utils
from autotest_lib.server import test
from autotest_lib.server import utils
from autotest_lib.server.cros import autoupdater
from autotest_lib.server.cros import provision


try:
    from chromite.lib import metrics
except ImportError:
    metrics = utils.metrics_mock


_CONFIG = global_config.global_config
# pylint: disable-msg=E1120
_IMAGE_URL_PATTERN = _CONFIG.get_config_value(
        'CROS', 'image_url_pattern', type=str)


def _metric_name(base_name):
    return 'chromeos/autotest/provision/' + base_name


def _get_build_metrics_fields(build_name):
    try:
        return utils.ParseBuildName(build_name)[0 : 2]
    except utils.ParseBuildNameException:
        logging.warning('Unable to parse build name %s for metrics. '
                        'Continuing anyway.', build_name)
        return ('', '')


def _emit_updater_metrics(name_prefix, build_name, failure_reason,
                          duration, fields):
    # reset_after=True is required for String gauges events to ensure that
    # the metrics are not repeatedly emitted until the server restarts.
    metrics.String(_metric_name(name_prefix + '_build_by_devserver_dut'),
                   reset_after=True).set(build_name, fields=fields)
    if failure_reason:
        metrics.String(
                _metric_name(name_prefix + '_failure_reason_by_devserver_dut'),
                reset_after=True).set(failure_reason, fields=fields)
    metrics.SecondsDistribution(
            _metric_name(name_prefix + '_duration_by_devserver_dut')).add(
                    duration, fields=fields)


def _emit_provision_metrics(update_url, dut_host_name,
                          exception, duration):
    # The following is high cardinality, but sparse.
    # Each DUT is of a single board type, and likely build type.
    #
    # TODO(jrbarnette) The devserver-triggered provisioning code
    # includes retries in certain cases.  For that reason, the metrics
    # distinguish 'provision' metrics which summarizes across all
    # retries, and 'auto_update' which summarizes an individual update
    # attempt.  ChromiumOSUpdater doesn't do retries, so we just report
    # the same information twice.  We should replace the metrics with
    # something better tailored to the current implementation.
    build_name = autoupdater.url_to_image_name(update_url)
    board, build_type = _get_build_metrics_fields(build_name)
    fields = {
        'board': board,
        'build_type': build_type,
        'dut_host_name': dut_host_name,
        'dev_server': dev_server.get_resolved_hostname(update_url),
        'success': not exception,
    }
    failure_reason = autoupdater.get_update_failure_reason(exception)
    _emit_updater_metrics('provision', build_name, failure_reason,
                          duration, fields)
    fields['attempt'] = 1
    _emit_updater_metrics('auto_update', build_name, failure_reason,
                          duration, fields)


class provision_AutoUpdate(test.test):
    """A test that can provision a machine to the correct ChromeOS version."""
    version = 1

    def initialize(self, host, value, is_test_na=False):
        """Initialize.

        @param host: The host object to update to |value|.
        @param value: The build type and version to install on the host.
        @param is_test_na: boolean, if True, will simply skip the test
                           and emit TestNAError. The control file
                           determines whether the test should be skipped
                           and passes the decision via this argument. Note
                           we can't raise TestNAError in control file as it won't
                           be caught and handled properly.
        """
        if is_test_na:
            raise error.TestNAError(
                'Test not available for test_that. chroot detected, '
                'you are probably using test_that.')
        # We check value in initialize so that it fails faster.
        if not value:
            raise error.TestFail('No build version specified.')


    def run_once(self, host, value, force_update_engine=False):
        """The method called by the control file to start the test.

        @param host: The host object to update to |value|.
        @param value: The host object to provision with a build corresponding
                      to |value|.
        @param force_update_engine: When true, the update flow must
                      perform the update unconditionally, using
                      update_engine.  Optimizations that could suppress
                      invoking update_engine, including quick-provision,
                      mustn't be used.
        """
        with_cheets = False
        logging.debug('Start provisioning %s to %s.', host, value)
        if value.endswith(provision.CHEETS_SUFFIX):
            image = re.sub(provision.CHEETS_SUFFIX + '$', '', value)
            with_cheets = True
        else:
            image = value

        # If the host is already on the correct build, we have nothing to do.
        # Note that this means we're not doing any sort of stateful-only
        # update, and that we're relying more on cleanup to do cleanup.
        if not force_update_engine:
            info = host.host_info_store.get()
            if info.build == value:
                # We can't raise a TestNA, as would make sense, as that makes
                # job.run_test return False as if the job failed.  However, it'd
                # still be nice to get this into the status.log, so we manually
                # emit an INFO line instead.
                self.job.record('INFO', None, None,
                                'Host already running %s' % value)
                return

        # We're about to reimage a machine, so we need full_payload and
        # stateful.  If something happened where the devserver doesn't have one
        # of these, then it's also likely that it'll be missing autotest.
        # Therefore, we require the devserver to also have autotest staged, so
        # that the test that runs after this provision finishes doesn't error
        # out because the devserver that its job_repo_url is set to is missing
        # autotest test code.
        ds = None
        try:
            ds = dev_server.ImageServer.resolve(image, host.hostname)
            ds.stage_artifacts(image, ['full_payload', 'stateful',
                                       'autotest_packages'])
        except dev_server.DevServerException as e:
            raise error.TestFail, str(e), sys.exc_info()[2]
        finally:
            # If a devserver is resolved, Log what has been downloaded so far.
            if ds:
                try:
                    ds.list_image_dir(image)
                except (dev_server.DevServerException, urllib2.URLError) as e2:
                    logging.warning('Failed to list_image_dir for build %s. '
                                    'Error: %s', image, e2)

        url = _IMAGE_URL_PATTERN % (ds.url(), image)

        logging.debug('Installing image')
        start_time = time.time()
        failure = None
        try:
            afe_utils.machine_install_and_update_labels(
                    host, url, not force_update_engine, with_cheets,
                    staging_server=ds)
        except BaseException as e:
            failure = e
            raise
        finally:
            _emit_provision_metrics(
                url, host.hostname, failure, time.time() - start_time)
        logging.debug('Finished provisioning %s to %s', host, value)
