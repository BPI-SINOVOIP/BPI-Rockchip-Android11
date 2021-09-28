# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Provides utility class for stopping and restarting services

When using this class, one likely wishes to do the following:

    def initialize(self):
        self._services = service_stopper.ServiceStopper(['service'])
        self._services.stop_services()


    def cleanup(self):
        self._services.start_services()

As this ensures that the services will be off before the test code runs, and
the test framework will ensure that the services are restarted through any
code path out of the test.
"""

import logging
import os

from autotest_lib.client.bin import utils
from autotest_lib.client.cros import upstart


class ServiceStopper(object):
    """Class to manage CrOS services.
    Public attributes:
      services_to_stop: list of services that should be stopped

   Public constants:
      POWER_DRAW_SERVICES: list of services that influence power test in
    unpredictable/undesirable manners.

    Public methods:
      stop_sevices: stop running system services.
      restore_services: restore services that were previously stopped.

    Private attributes:
      _services_stopped: list of services that were successfully stopped
    """

    POWER_DRAW_SERVICES = ['powerd', 'update-engine', 'vnc']

    # List of thermal throttling services that should be disabled.
    # - temp_metrics for link.
    # - thermal for daisy, snow, pit etc.
    # - dptf for intel >= baytrail
    # TODO(ihf): cpu_quiet on nyan isn't a service. We still need to disable it
    #            on nyan. See crbug.com/357457.
    THERMAL_SERVICES = ['dptf', 'temp_metrics', 'thermal']

    def __init__(self, services_to_stop=[]):
        """Initialize instance of class.

        By Default sets an empty list of services.
        """
        self.services_to_stop = services_to_stop
        self._services_stopped = []


    def stop_services(self):
        """Turn off managed services."""

        for service in self.services_to_stop:
            if not upstart.has_service(service):
                continue
            if not upstart.is_running(service):
                continue
            upstart.stop_job(service)
            self._services_stopped.append(service)


    def restore_services(self):
        """Restore services that were stopped."""
        for service in reversed(self._services_stopped):
            upstart.restart_job(service)
        self._services_stopped = []


    def __enter__(self):
        self.stop_services()
        return self


    def __exit__(self, exnval, exntype, exnstack):
        self.restore_services()


    def close(self):
        """Equivalent to restore_services."""
        self.restore_services()


    def _dptf_fixup_pl1(self):
        """For intel devices that don't set their PL1 override in coreboot's
        devicetree.cb (See 'register "tdp_pl1_override') stopping DPTF will
        change the PL1 limit to the platform default.  For eve (KBL-Y) that
        would be 4.5W.  To workaround this until FW can be fixed we should
        instead query what the PL1 limit is for the proc_thermal driver and
        write it to the PL1 constraint.

        TODO(b/144020442)
        """
        pl1_max_path = \
        '/sys/devices/pci0000:00/0000:00:04.0/power_limits/power_limit_0_max_uw'
        pl1_constraint_path = \
        '/sys/class/powercap/intel-rapl:0/constraint_0_power_limit_uw'
        if not os.path.exists(pl1_max_path):
            return

        pl1 = int(utils.read_one_line(pl1_max_path))
        logging.debug('PL1 set to %d uw', pl1)
        utils.system('echo %d > %s' % (pl1, pl1_constraint_path))


def get_thermal_service_stopper():
    """Convenience method to retrieve thermal service stopper."""
    return ServiceStopper(services_to_stop=ServiceStopper.THERMAL_SERVICES)
