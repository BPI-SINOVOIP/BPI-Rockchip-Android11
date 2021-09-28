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
"""Send notification email if new version is found.

Example usage:
external_updater_notifier \
    --history ~/updater/history \
    --generate_change \
    --recipients xxx@xxx.xxx \
    googletest
"""

from datetime import timedelta, datetime
import argparse
import json
import os
import re
import subprocess
import time

import git_utils

def parse_args():
    """Parses commandline arguments."""

    parser = argparse.ArgumentParser(
        description='Check updates for third party projects in external/.')
    parser.add_argument(
        '--history',
        help='Path of history file. If doesn'
        't exist, a new one will be created.')
    parser.add_argument(
        '--recipients',
        help='Comma separated recipients of notification email.')
    parser.add_argument(
        '--generate_change',
        help='If set, an upgrade change will be uploaded to Gerrit.',
        action='store_true', required=False)
    parser.add_argument(
        'paths', nargs='*',
        help='Paths of the project.')
    parser.add_argument(
        '--all', action='store_true',
        help='Checks all projects.')

    return parser.parse_args()


def _get_android_top():
    return os.environ['ANDROID_BUILD_TOP']


CHANGE_URL_PATTERN = r'(https:\/\/[^\s]*android-review[^\s]*) Upgrade'
CHANGE_URL_RE = re.compile(CHANGE_URL_PATTERN)


def _read_owner_file(proj):
    owner_file = os.path.join(_get_android_top(), 'external', proj, 'OWNERS')
    if not os.path.isfile(owner_file):
        return None
    with open(owner_file, 'r') as f:
        return f.read().strip()


def _send_email(proj, latest_ver, recipient, upgrade_log):
    print('Sending email for {}: {}'.format(proj, latest_ver))
    msg = "New version: {}".format(latest_ver)
    match = CHANGE_URL_RE.search(upgrade_log)
    if match is not None:
        msg += '\n\nAn upgrade change is generated at:\n{}'.format(
            match.group(1))

    owners = _read_owner_file(proj)
    if owners:
        msg += '\n\nOWNERS file: \n'
        msg += owners

    msg += '\n\n'
    msg += upgrade_log

    subprocess.run(['sendgmr', '--to=' + recipient,
                    '--subject=' + proj], check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                   input=msg, encoding='ascii')


NOTIFIED_TIME_KEY_NAME = 'latest_notified_time'


def _should_notify(latest_ver, proj_history):
    if latest_ver in proj_history:
        # Processed this version before.
        return False

    timestamp = proj_history.get(NOTIFIED_TIME_KEY_NAME, 0)
    time_diff = datetime.today() - datetime.fromtimestamp(timestamp)
    if git_utils.is_commit(latest_ver) and time_diff <= timedelta(days=30):
        return False

    return True


def _process_results(args, history, results):
    for proj, res in results.items():
        if 'latest' not in res:
            continue
        latest_ver = res['latest']
        current_ver = res['current']
        if latest_ver == current_ver:
            continue
        proj_history = history.setdefault(proj, {})
        if _should_notify(latest_ver, proj_history):
            upgrade_log = _upgrade(proj) if args.generate_change else ""
            try:
                _send_email(proj, latest_ver, args.recipients, upgrade_log)
                proj_history[latest_ver] = int(time.time())
                proj_history[NOTIFIED_TIME_KEY_NAME] = int(time.time())
            except subprocess.CalledProcessError as err:
                msg = """Failed to send email for {} ({}).
stdout: {}
stderr: {}""".format(proj, latest_ver, err.stdout, err.stderr)
                print(msg)


RESULT_FILE_PATH = '/tmp/update_check_result.json'


def send_notification(args):
    """Compare results and send notification."""
    results = {}
    with open(RESULT_FILE_PATH, 'r') as f:
        results = json.load(f)
    history = {}
    try:
        with open(args.history, 'r') as f:
            history = json.load(f)
    except FileNotFoundError:
        pass

    _process_results(args, history, results)

    with open(args.history, 'w') as f:
        json.dump(history, f, sort_keys=True, indent=4)


def _upgrade(proj):
    out = subprocess.run(['out/soong/host/linux-x86/bin/external_updater',
                          'update', '--branch_and_commit', '--push_change',
                          proj],
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         cwd=_get_android_top())
    stdout = out.stdout.decode('utf-8')
    stderr = out.stderr.decode('utf-8')
    return """
====================
|    Debug Info    |
====================
-=-=-=-=stdout=-=-=-=-
{}

-=-=-=-=stderr=-=-=-=-
{}
""".format(stdout, stderr)


def _check_updates(args):
    params = ['out/soong/host/linux-x86/bin/external_updater',
              'check', '--json_output', RESULT_FILE_PATH,
              '--delay', '30']
    if args.all:
        params.append('--all')
    else:
        params += args.paths

    print(_get_android_top())
    subprocess.run(params, cwd=_get_android_top())


def main():
    """The main entry."""

    args = parse_args()
    _check_updates(args)
    send_notification(args)


if __name__ == '__main__':
    main()
