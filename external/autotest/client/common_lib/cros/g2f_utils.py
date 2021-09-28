# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from autotest_lib.client.common_lib import error

# USB ID for the virtual U2F HID Device.
U2F_VID = '18D1'
U2F_PID = '502C'

QUERY_U2F_DEVICE_ATTEMPTS=5
QUERY_U2F_RETRY_DELAY_SEC=1

def ChromeOSLogin(client):
    """Logs in to ChromeOS, so that u2fd can start up."""
    client.run('/usr/local/autotest/bin/autologin.py')

def ChromeOSLogout(client):
    """Logs out of ChromeOS, to return the device to a known state."""
    client.run('restart ui')

def StartU2fd(client):
    """Starts u2fd on the client.

    @param client: client object to run commands on.
    """
    client.run('touch /var/lib/u2f/force/u2f.force')
    client.run('restart u2fd')

    path = '/sys/bus/hid/devices/*:%s:%s.*/hidraw' % (U2F_VID, U2F_PID)
    attempts = 0
    while attempts < QUERY_U2F_DEVICE_ATTEMPTS:
      attempts += 1
      try:
        return '/dev/' + client.run('ls ' + path).stdout.strip()
      except error.AutoservRunError, e:
        logging.info('Could not find U2F device on attempt ' +
                     str(attempts))
      time.sleep(QUERY_U2F_RETRY_DELAY_SEC)

def G2fRegister(client, dev, challenge, application, p1=0):
    """Returns a dictionary with TPM status.

    @param client: client object to run commands on.
    """
    return client.run('g2ftool --reg --dev=' + dev +
                      ' --challenge=' + challenge +
                      ' --application=' + application +
                      ' --p1=' + str(p1),
                      ignore_status=True)

def G2fAuth(client, dev, challenge, application, key_handle, p1=0):
    """Returns a dictionary with TPM status.

    @param client: client object to run commands on.
    """
    return client.run('g2ftool --auth --dev=' + dev +
                      ' --challenge=' + challenge +
                      ' --application=' + application +
                      ' --key_handle=' + key_handle +
                      ' --p1=' + str(p1),
                      ignore_status=True)
