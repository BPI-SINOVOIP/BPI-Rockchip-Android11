#!/usr/bin/python2

# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os
import pipes
import re
import signal
import sys
import time

import common
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import logging_config

_ADB_POLLING_INTERVAL_SECONDS = 10
_ADB_CONNECT_INTERVAL_SECONDS = 1
_ADB_COMMAND_TIMEOUT_SECONDS = 5

_signum_to_name = {}


def _signal_handler(signum, frame):
    logging.info('Received %s, shutting down', _signum_to_name[signum])
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(0)


def _get_adb_options(target, socket):
    """Get adb global options."""
    # ADB 1.0.36 does not support -L adb socket option. Parse the host and port
    # part from the socket instead.
    # https://developer.android.com/studio/command-line/adb.html#issuingcommands
    pattern = r'^[^:]+:([^:]+):(\d+)$'
    match = re.match(pattern, socket)
    if not match:
        raise ValueError('Unrecognized socket format: %s' % socket)
    server_host, server_port = match.groups()
    return '-s %s -H %s -P %s' % (
        pipes.quote(target), pipes.quote(server_host), pipes.quote(server_port))


def _run_adb_cmd(cmd, adb_option="", **kwargs):
    """Run adb command.

    @param cmd: command to issue with adb. (Ex: connect, devices)
    @param target: Device to connect to.
    @param adb_option: adb global option configuration.

    @return: the stdout of the command.
    """
    adb_cmd = 'adb %s %s' % (adb_option, cmd)
    while True:
        try:
            output = utils.system_output(adb_cmd, **kwargs)
            break
        except error.CmdTimeoutError as e:
            logging.warning(e)
            logging.info('Retrying command %s', adb_cmd)
    logging.debug('%s: %s', adb_cmd, output)
    return output


def _is_adb_connected(target, adb_option=""):
    """Return true if adb is connected to the container.

    @param target: Device to connect to.
    @param adb_option: adb global option configuration.
    """
    output = _run_adb_cmd('get-state', adb_option=adb_option,
                          timeout=_ADB_COMMAND_TIMEOUT_SECONDS,
                          ignore_status=True)
    return output.strip() == 'device'


def _ensure_adb_connected(target, adb_option=""):
    """Ensures adb is connected to the container, reconnects otherwise.

    @param target: Device to connect to.
    @param adb_option: adb global options configuration.
    """
    did_reconnect = False
    while not _is_adb_connected(target, adb_option):
        if not did_reconnect:
            logging.info('adb not connected. attempting to reconnect')
            did_reconnect = True
        _run_adb_cmd('connect %s' % pipes.quote(target),
                     adb_option=adb_option,
                     timeout=_ADB_COMMAND_TIMEOUT_SECONDS, ignore_status=True)
        time.sleep(_ADB_CONNECT_INTERVAL_SECONDS)
    if did_reconnect:
        logging.info('Reconnection succeeded')


if __name__ == '__main__':
    logging_config.LoggingConfig().configure_logging()
    parser = argparse.ArgumentParser(description='ensure adb is connected')
    parser.add_argument('target', help='Device to connect to')
    parser.add_argument('--socket', help='ADB server socket.',
                        default='tcp:localhost:5037')
    args = parser.parse_args()
    adb_option = _get_adb_options(args.target, args.socket)

    # Setup signal handler for logging on exit
    for attr in dir(signal):
        if not attr.startswith('SIG') or attr.startswith('SIG_'):
            continue
        if attr in ('SIGCHLD', 'SIGCLD', 'SIGKILL', 'SIGSTOP'):
            continue
        signum = getattr(signal, attr)
        _signum_to_name[signum] = attr
        signal.signal(signum, _signal_handler)

    logging.info('Starting adb_keepalive for target %s on socket %s',
                 args.target, args.socket)
    while True:
        time.sleep(_ADB_POLLING_INTERVAL_SECONDS)
        _ensure_adb_connected(args.target, adb_option=adb_option)
