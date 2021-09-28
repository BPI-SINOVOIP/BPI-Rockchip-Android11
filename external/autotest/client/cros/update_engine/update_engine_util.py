# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import json
import logging
import os
import re
import requests
import shutil
import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.cros.update_engine import update_engine_event

_DEFAULT_RUN = utils.run
_DEFAULT_COPY = shutil.copy

class UpdateEngineUtil(object):
    """Utility code shared between client and server update_engine autotests"""

    # Update engine status lines.
    _LAST_CHECKED_TIME = 'LAST_CHECKED_TIME'
    _PROGRESS = 'PROGRESS'
    _CURRENT_OP = 'CURRENT_OP'
    _NEW_VERSION = 'NEW VERSION'
    _NEW_SIZE = 'NEW_SIZE'

    # Update engine statuses.
    _UPDATE_STATUS_IDLE = 'UPDATE_STATUS_IDLE'
    _UPDATE_ENGINE_DOWNLOADING = 'UPDATE_STATUS_DOWNLOADING'
    _UPDATE_ENGINE_FINALIZING = 'UPDATE_STATUS_FINALIZING'
    _UPDATE_STATUS_UPDATED_NEED_REBOOT = 'UPDATE_STATUS_UPDATED_NEED_REBOOT'
    _UPDATE_STATUS_REPORTING_ERROR_EVENT = 'UPDATE_STATUS_REPORTING_ERROR_EVENT'

    # Files used by the tests.
    _UPDATE_ENGINE_LOG = '/var/log/update_engine.log'
    _UPDATE_ENGINE_LOG_DIR = '/var/log/update_engine/'
    _CUSTOM_LSB_RELEASE = '/mnt/stateful_partition/etc/lsb-release'
    _UPDATE_ENGINE_PREFS_DIR = '/var/lib/update_engine/prefs/'

    # Public key used to force update_engine to verify omaha response data on
    # test images.
    _IMAGE_PUBLIC_KEY = 'LS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS0KTUlJQklqQU5CZ2txaGtpRzl3MEJBUUVGQUFPQ0FROEFNSUlCQ2dLQ0FRRUFxZE03Z25kNDNjV2ZRenlydDE2UQpESEUrVDB5eGcxOE9aTys5c2M4aldwakMxekZ0b01Gb2tFU2l1OVRMVXArS1VDMjc0ZitEeElnQWZTQ082VTVECkpGUlBYVXp2ZTF2YVhZZnFsalVCeGMrSlljR2RkNlBDVWw0QXA5ZjAyRGhrckduZi9ya0hPQ0VoRk5wbTUzZG8Kdlo5QTZRNUtCZmNnMUhlUTA4OG9wVmNlUUd0VW1MK2JPTnE1dEx2TkZMVVUwUnUwQW00QURKOFhtdzRycHZxdgptWEphRm1WdWYvR3g3K1RPbmFKdlpUZU9POUFKSzZxNlY4RTcrWlppTUljNUY0RU9zNUFYL2xaZk5PM1JWZ0cyCk83RGh6emErbk96SjNaSkdLNVI0V3daZHVobjlRUllvZ1lQQjBjNjI4NzhxWHBmMkJuM05wVVBpOENmL1JMTU0KbVFJREFRQUIKLS0tLS1FTkQgUFVCTElDIEtFWS0tLS0tCg=='


    def __init__(self, run_func=_DEFAULT_RUN, get_file=_DEFAULT_COPY):
        """
        Initialize this class.

        @param run_func: the function to use to run commands on the client.
                         Defaults for use by client tests, but can be
                         overwritten to run remotely from a server test.
        @param get_file: the function to use when copying a file.  Defaults
                         for use by client tests.  Called with
                         (file, destination) syntax.

        """
        self._create_update_engine_variables(run_func, get_file)


    def _create_update_engine_variables(self, run_func=_DEFAULT_RUN,
                                        get_file=_DEFAULT_COPY):
        """See __init__()."""
        self._run = run_func
        self._get_file = get_file


    def _wait_for_progress(self, progress):
        """
        Waits until we reach the percentage passed as the input.

        @param progress: The progress [0.0 .. 1.0] we want to wait for.
        """
        while True:
            completed = self._get_update_progress()
            logging.debug('Checking if %s is greater than %s', completed,
                          progress)
            if completed >= progress:
                break
            time.sleep(1)


    def _is_update_started(self):
        """Checks if the update has started."""
        status = self._get_update_engine_status()
        if status is None:
            return False
        return any(arg == status[self._CURRENT_OP] for arg in
            [self._UPDATE_ENGINE_DOWNLOADING, self._UPDATE_ENGINE_FINALIZING])


    def _get_update_progress(self):
        """Returns the current payload downloaded progress."""
        while True:
            status = self._get_update_engine_status()
            if not status:
                continue
            if self._UPDATE_STATUS_IDLE == status[self._CURRENT_OP]:
                err_str = self._get_last_error_string()
                raise error.TestFail('Update status was idle while trying to '
                                     'get download status. Last error: %s' %
                                     err_str)
            if self._UPDATE_STATUS_REPORTING_ERROR_EVENT == status[
                self._CURRENT_OP]:
                err_str = self._get_last_error_string()
                raise error.TestFail('Update status reported error: %s' %
                                     err_str)
            # When the update has moved to the final stages, update engine
            # displays progress as 0.0  but for our needs we will return 1.0
            if status[self._CURRENT_OP] in [
                self._UPDATE_STATUS_UPDATED_NEED_REBOOT,
                self._UPDATE_ENGINE_FINALIZING]:
                return 1.0
            # If we call this right after reboot it may not be downloading yet.
            if self._UPDATE_ENGINE_DOWNLOADING != status[self._CURRENT_OP]:
                time.sleep(1)
                continue
            return float(status[self._PROGRESS])


    def _wait_for_update_to_fail(self):
        """Waits for the update to retry until failure."""
        timeout_minutes = 20
        timeout = time.time() + 60 * timeout_minutes
        while True:
            if self._check_update_engine_log_for_entry('Reached max attempts ',
                                                       raise_error=False):
                logging.debug('Found log entry for failed update.')
                if self._is_update_engine_idle():
                    break
            time.sleep(1)
            self._get_update_engine_status()
            if time.time() > timeout:
                raise error.TestFail('Update did not fail as expected. Timeout'
                                     ': %d minutes.' % timeout_minutes)


    def _wait_for_update_to_complete(self, finalizing_ok=False):
        """
        Checks if the update has completed.

        @param finalizing_ok: FINALIZING status counts as complete.

        """
        statuses = [self._UPDATE_STATUS_UPDATED_NEED_REBOOT]
        if finalizing_ok:
            statuses.append(self._UPDATE_ENGINE_FINALIZING)
        while True:
            status = self._get_update_engine_status()

            # During reboot, status will be None
            if status is not None:
                if self._UPDATE_STATUS_REPORTING_ERROR_EVENT == status[
                    self._CURRENT_OP]:
                    err_str = self._get_last_error_string()
                    raise error.TestFail('Update status reported error: %s' %
                                         err_str)
                if status[self._CURRENT_OP] == self._UPDATE_STATUS_IDLE:
                    err_str = self._get_last_error_string()
                    raise error.TestFail('Update status was unexpectedly '
                                         'IDLE when we were waiting for the '
                                         'update to complete: %s' % err_str)
                if any(arg in status[self._CURRENT_OP] for arg in statuses):
                    break
            time.sleep(1)


    def _get_update_engine_status(self, timeout=3600, ignore_timeout=True):
        """
        Gets a dictionary version of update_engine_client --status.

        @param timeout: How long to wait for the status to return.
        @param ignore_timeout: True to throw an exception if timeout occurs.

        @return Dictionary of values within update_engine_client --status.
        @raise: error.AutoservError if command times out

        """
        status = self._run('update_engine_client --status', timeout=timeout,
                           ignore_status=True, ignore_timeout=ignore_timeout)

        if status is None:
            return None
        logging.info(status)
        if status.exit_status != 0:
            return None
        status_dict = {}
        for line in status.stdout.splitlines():
            entry = line.partition('=')
            status_dict[entry[0]] = entry[2]
        return status_dict


    def _check_update_engine_log_for_entry(self, entry, raise_error=False,
                                           err_str=None,
                                           update_engine_log=None):
        """
        Checks for entries in the update_engine log.

        @param entry: The line to search for.
        @param raise_error: Fails tests if log doesn't contain entry.
        @param err_str: The error string to raise if we cannot find entry.
        @param update_engine_log: Update engine log string you want to
                                  search. If None, we will read from the
                                  current update engine log.

        @return Boolean if the update engine log contains the entry.

        """
        if isinstance(entry, str):
            # Create a tuple of strings so we can itarete over it.
            entry = (entry,)

        if not update_engine_log:
            update_engine_log = self._run(
                'cat %s' % self._UPDATE_ENGINE_LOG).stdout

        if all(msg in update_engine_log for msg in entry):
            return True

        if not raise_error:
            return False

        error_str = ('Did not find expected string(s) in update_engine log: '
                     '%s' % entry)
        logging.debug(error_str)
        raise error.TestFail(err_str if err_str else error_str)


    def _is_update_finished_downloading(self):
        """Checks if the update has moved to the final stages."""
        s = self._get_update_engine_status()
        return s[self._CURRENT_OP] in [self._UPDATE_ENGINE_FINALIZING,
                                       self._UPDATE_STATUS_UPDATED_NEED_REBOOT]


    def _is_update_engine_idle(self):
        """Checks if the update engine is idle."""
        status = self._get_update_engine_status()
        return status[self._CURRENT_OP] == self._UPDATE_STATUS_IDLE


    def _update_continued_where_it_left_off(self, progress):
        """
        Checks that the update did not restart after an interruption.

        @param progress: The progress the last time we checked.

        @returns True if update continued. False if update restarted.

        """
        completed = self._get_update_progress()
        logging.info('New value: %f, old value: %f', completed, progress)
        return completed >= progress


    def _get_payload_properties_file(self, payload_url, target_dir, **kwargs):
        """
        Downloads the payload properties file into a directory.

        @param payload_url: The URL to the update payload file.
        @param target_dir: The directory to download the file into.
        @param kwargs: A dictionary of key/values that needs to be overridden on
                the payload properties file.

        """
        payload_props_url = payload_url + '.json'
        _, _, file_name = payload_props_url.rpartition('/')
        try:
            response = json.loads(requests.get(payload_props_url).text)

            # Override existing keys if any.
            for k, v in kwargs.iteritems():
                # Don't set default None values. We don't want to override good
                # values to None.
                if v is not None:
                    response[k] = v

            with open(os.path.join(target_dir, file_name), 'w') as fp:
                json.dump(response, fp)

        except (requests.exceptions.RequestException,
                IOError,
                ValueError) as err:
            raise error.TestError(
                'Failed to get update payload properties: %s with error: %s' %
                (payload_props_url, err))


    def _check_for_update(self, server='http://127.0.0.1', port=8082,
                          update_path='update', interactive=True,
                          ignore_status=False, wait_for_completion=False,
                          **kwargs):
        """
        Starts a background update check.

        @param server: The omaha server to call in the update url.
        @param port: The omaha port to call in the update url.
        @param update_path: The /update part of the URL. When using a lab
                            devserver, pass update/<board>-release/RXX-X.X.X.
        @param interactive: True if we are doing an interactive update.
        @param ignore_status: True if we should ignore exceptions thrown.
        @param wait_for_completion: True for --update, False for
                --check_for_update.
        @param kwargs: The dictionary to be converted to a query string
                and appended to the end of the update URL. e.g:
                {'critical_update': True, 'foo': 'bar'} ->
                'http:/127.0.0.1:8080/update?critical_update=True&foo=bar'
                Look at nebraska.py or devserver.py for the list of accepted
                values.
        """
        update = 'update' if wait_for_completion else 'check_for_update'
        update_path = update_path.lstrip('/')
        query = '&'.join('%s=%s' % (k, v) for k, v in kwargs.items())
        cmd = 'update_engine_client --%s --omaha_url="%s:%d/%s?%s"' % (
            update, server, port, update_path, query)

        if not interactive:
          cmd += ' --interactive=false'
        self._run(cmd, ignore_status=ignore_status)


    def _save_extra_update_engine_logs(self, number_of_logs=2):
        """
        Get the last X number of update_engine logs on the DUT.

        @param number_of_logs: The number of logs to save.

        """
        files = self._run('ls -t -1 %s' %
                          self._UPDATE_ENGINE_LOG_DIR).stdout.splitlines()

        for i in range(number_of_logs if number_of_logs <= len(files) else
                       len(files)):
            file = os.path.join(self._UPDATE_ENGINE_LOG_DIR, files[i])
            self._get_file(file, self.resultsdir)


    def _get_second_last_update_engine_log(self):
        """
        Gets second last update engine log text.

        This is useful for getting the last update engine log before a reboot.

        """
        files = self._run('ls -t -1 %s' %
                          self._UPDATE_ENGINE_LOG_DIR).stdout.splitlines()
        return self._run('cat %s%s' % (self._UPDATE_ENGINE_LOG_DIR,
                                       files[1])).stdout


    def _create_custom_lsb_release(self, update_url, build='0.0.0.0', **kwargs):
        """
        Create a custom lsb-release file.

        In order to tell OOBE to ping a different update server than the
        default we need to create our own lsb-release. We will include a
        deserver update URL.

        @param update_url: String of url to use for update check.
        @param build: String of the build number to use. Represents the
                      Chrome OS build this device thinks it is on.
        @param kwargs: A dictionary of key/values to be made into a query string
                       and appended to the update_url

        """
        # TODO(ahassani): This is quite fragile as the given URL can already
        # have a search query. We need to unpack the URL and update the search
        # query portion of it with kwargs.
        update_url = (update_url + '?' + '&'.join('%s=%s' % (k, v)
                                                  for k, v in kwargs.items()))
        self._run('mkdir %s' % os.path.dirname(self._CUSTOM_LSB_RELEASE),
                  ignore_status=True)
        self._run('touch %s' % self._CUSTOM_LSB_RELEASE)
        self._run('echo CHROMEOS_RELEASE_VERSION=%s > %s' %
                  (build, self._CUSTOM_LSB_RELEASE))
        self._run('echo CHROMEOS_AUSERVER=%s >> %s' %
                  (update_url, self._CUSTOM_LSB_RELEASE))


    def _clear_custom_lsb_release(self):
        """
        Delete the custom release file, if any.

        Intended to clear work done by _create_custom_lsb_release().

        """
        self._run('rm %s' % self._CUSTOM_LSB_RELEASE, ignore_status=True)


    def _get_update_requests(self):
        """
        Get the contents of all the update requests from the most recent log.

        @returns: a sequential list of <request> xml blocks or None if none.

        """
        update_log = self._run('cat %s' %
                               self._UPDATE_ENGINE_LOG).stdout

        # Matches <request ... /request>.  The match can be on multiple
        # lines and the search is not greedy so it only matches one block.
        return re.findall(r'<request.*?/request>', update_log, re.DOTALL)


    def _get_time_of_last_update_request(self):
        """
        Get the time of the last update request from most recent logfile.

        @returns: seconds since epoch of when last update request happened
                  (second accuracy), or None if no such timestamp exists.

        """
        update_log = ''
        with open(self._UPDATE_ENGINE_LOG) as fh:
            update_log = fh.read()

        # Matches any single line with "MMDD/HHMMSS ... Request ... xml", e.g.
        # "[0723/133526:INFO:omaha_request_action.cc(794)] Request: <?xml".
        result = re.findall(r'([0-9]{4}/[0-9]{6}).*Request.*xml', update_log)
        if not result:
            return None

        LOG_TIMESTAMP_FORMAT = '%m%d/%H%M%S'
        match = result[-1]

        # The log does not include the year, so set it as this year.
        # This assumption could cause incorrect behavior, but is unlikely to.
        current_year = datetime.datetime.now().year
        log_datetime = datetime.datetime.strptime(match, LOG_TIMESTAMP_FORMAT)
        log_datetime = log_datetime.replace(year=current_year)

        return time.mktime(log_datetime.timetuple())


    def _take_screenshot(self, filename):
        """
        Take a screenshot and save in resultsdir.

        @param filename: The name of the file to save

        """
        try:
            file_location = os.path.join('/tmp', filename)
            self._run('screenshot %s' % file_location)
            self._get_file(file_location, self.resultsdir)
        except error.AutoservRunError:
            logging.exception('Failed to take screenshot.')


    def _get_last_error_string(self):
        """
        Gets the last error message in the update engine log.

        @returns: The error message.

        """
        err_str = 'Updating payload state for error code: '
        log = self._run('cat %s' % self._UPDATE_ENGINE_LOG).stdout.splitlines()
        targets = [line for line in log if err_str in line]
        logging.debug('Error lines found: %s', targets)
        if not targets:
          return None
        else:
          return targets[-1].rpartition(err_str)[2]


    def _get_latest_initial_request(self):
        """
        Return the most recent initial update request.

        AU requests occur in a chain of messages back and forth, e.g. the
        initial request for an update -> the reply with the update -> the
        report that install has started -> report that install has finished,
        etc.  This function finds the first request in the latest such chain.

        This message has no eventtype listed, or is rebooted_after_update
        type (as an artifact from a previous update since this one).
        Subsequent messages in the chain have different eventtype values.

        @returns: string of the entire update request or None.

        """
        requests = self._get_update_requests()
        if not requests:
            return None

        MATCH_STR = r'eventtype="(.*?)"'
        for i in xrange(len(requests) - 1, -1, -1):
            search = re.search(MATCH_STR, requests[i])
            if (not search or
                (search.group(1) ==
                 update_engine_event.EVENT_TYPE_REBOOTED_AFTER_UPDATE)):
                return requests[i]

        return None
