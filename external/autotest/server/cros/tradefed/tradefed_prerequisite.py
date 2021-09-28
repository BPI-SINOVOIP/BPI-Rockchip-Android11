# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

_ERROR_PREFIX = 'CTS Test Precondition Failed'

def bluetooth(hosts):
    """Check for missing bluetooth hardware.
    """
    # TODO(ianrlee): Reenable, once a nice check is found in b/148621587.
    # for host in hosts:
    #    output = host.run('hcitool dev').stdout
    #    lines = output.splitlines()
    #    if len(lines) < 2 or not lines[0].startswith('Devices:'):
    #        return False, '%s: Bluetooth device is missing.'\
    #                      'Stdout of the command "hcitool dev"'\
    #                      'on host %s was %s' % (_ERROR_PREFIX, host, output)
    return True, ''


def region_us(hosts):
    """Check that region is set to "us".
    """
    for host in hosts:
        output = host.run('vpd -g region', ignore_status=True).stdout
        if output not in ['us', '']:
            return False, '%s: Region is not "us" or empty. '\
                          'STDOUT of the command "vpd -l '\
                          'region" on host %s was %s'\
                          % (_ERROR_PREFIX, host, output)
    return True, ''

prerequisite_map = {
    'bluetooth': bluetooth,
    'region_us': region_us,
}

def check(prereq, hosts):
    """Execute the prerequisite check.

    @return boolean indicating if check passes.
    @return string error message if check fails.
    """
    if prereq not in prerequisite_map:
        logging.info('%s is not a valid prerequisite.', prereq)
        return True, ''
    return prerequisite_map[prereq](hosts)
