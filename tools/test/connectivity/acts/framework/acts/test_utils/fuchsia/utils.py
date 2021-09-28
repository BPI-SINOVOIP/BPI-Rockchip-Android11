#!/usr/bin/env python3
#
#   Copyright 2018 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import os

from acts.libs.proc.job import Error


def http_file_download_by_curl(fd,
                               url,
                               out_path='/tmp/',
                               curl_loc='/bin/curl',
                               remove_file_after_check=True,
                               timeout=3600,
                               limit_rate=None,
                               additional_args=None,
                               retry=3):
    """Download http file by ssh curl.

    Args:
        fd: Fuchsia Device Object.
        url: The url that file to be downloaded from.
        out_path: Optional. Where to download file to.
            out_path is /tmp by default.
        curl_loc: Location of curl binary on fd.
        remove_file_after_check: Whether to remove the downloaded file after
            check.
        timeout: timeout for file download to complete.
        limit_rate: download rate in bps. None, if do not apply rate limit.
        additional_args: Any additional args for curl.
        retry: the retry request times provided in curl command.
    """
    file_directory, file_name = _generate_file_directory_and_file_name(
        url, out_path)
    file_path = os.path.join(file_directory, file_name)
    curl_cmd = curl_loc
    if limit_rate:
        curl_cmd += ' --limit-rate %s' % limit_rate
    if retry:
        curl_cmd += ' --retry %s' % retry
    if additional_args:
        curl_cmd += ' %s' % additional_args
    curl_cmd += ' --url %s > %s' % (url, file_path)
    try:
        fd.log.info(
            'Download %s to %s by ssh command %s' % (url, file_path, curl_cmd))

        status = fd.send_command_ssh(curl_cmd, timeout=timeout)
        if isinstance(status, Error):
            status = status.result
        if not status.stderr:
            if int(status.exit_status) != 0:
                fd.log.warning('Curl command: "%s" failed with error %s' %
                               (curl_cmd, status.exit_status))
                return False

            if _check_file_existence(fd, file_path):
                fd.log.info(
                    '%s is downloaded to %s successfully' % (url, file_path))
                return True
        else:
            fd.log.warning('Fail to download %s' % url)
            return False
    except Exception as e:
        fd.log.warning('Download %s failed with exception %s' % (url, e))
        return False
    finally:
        if remove_file_after_check:
            fd.log.info('Remove the downloaded file %s' % file_path)
            fd.send_command_ssh('rm %s' % file_path)


def _generate_file_directory_and_file_name(url, out_path):
    """Splits the file from the url and specifies the appropriate location of
       where to store the downloaded file.

    Args:
        url: A url to the file that is going to be downloaded.
        out_path: The location of where to store the file that is downloaded.

    Returns:
        file_directory: The directory of where to store the downloaded file.
        file_name: The name of the file that is being downloaded.
    """
    file_name = url.split('/')[-1]
    if not out_path:
        file_directory = '/tmp/'
    elif not out_path.endswith('/'):
        file_directory, file_name = os.path.split(out_path)
    else:
        file_directory = out_path
    return file_directory, file_name


def _check_file_existence(fd, file_path):
    """Check file existence by file_path. If expected_file_size
       is provided, then also check if the file meet the file size requirement.

    Args:
        fd: A fuchsia device
        file_path: Where to store the file on the fuchsia device.
    """
    out = fd.send_command_ssh('ls -al "%s"' % file_path)
    if isinstance(out, Error):
        out = out.result
    if 'No such file or directory' in out.stdout:
        fd.log.debug('File %s does not exist.' % file_path)
        return False
    else:
        fd.log.debug('File %s exists.' % file_path)
        return True
