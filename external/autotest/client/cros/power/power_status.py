# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import contextlib
import ctypes
import fcntl
import glob
import itertools
import json
import logging
import math
import numpy
import os
import re
import struct
import threading
import time

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error, enum
from autotest_lib.client.common_lib.utils import poll_for_condition_ex
from autotest_lib.client.cros import kernel_trace
from autotest_lib.client.cros.power import power_utils

BatteryDataReportType = enum.Enum('CHARGE', 'ENERGY')

# battery data reported at 1e6 scale
BATTERY_DATA_SCALE = 1e6
# number of times to retry reading the battery in the case of bad data
BATTERY_RETRY_COUNT = 3
# default filename when saving CheckpointLogger data to file
CHECKPOINT_LOG_DEFAULT_FNAME = 'checkpoint_log.json'


class DevStat(object):
    """
    Device power status. This class implements generic status initialization
    and parsing routines.
    """

    def __init__(self, fields, path=None):
        self.fields = fields
        self.path = path


    def reset_fields(self):
        """
        Reset all class fields to None to mark their status as unknown.
        """
        for field in self.fields.iterkeys():
            setattr(self, field, None)


    def read_val(self,  file_name, field_type):
        """Read a value from file.
        """
        try:
            path = file_name
            if not file_name.startswith('/'):
                path = os.path.join(self.path, file_name)
            f = open(path, 'r')
            out = f.readline().rstrip('\n')
            val = field_type(out)
            return val

        except:
            return field_type(0)


    def read_all_vals(self):
        """Read all values.
        """
        for field, prop in self.fields.iteritems():
            if prop[0]:
                val = self.read_val(prop[0], prop[1])
                setattr(self, field, val)

    def update(self):
        """Update the DevStat.

        Need to implement in subclass.
        """
        pass

class ThermalStatACPI(DevStat):
    """
    ACPI-based thermal status.

    Fields:
    (All temperatures are in millidegrees Celsius.)

    str   enabled:            Whether thermal zone is enabled
    int   temp:               Current temperature
    str   type:               Thermal zone type
    int   num_trip_points:    Number of thermal trip points that activate
                                cooling devices
    int   num_points_tripped: Temperature is above this many trip points
    str   trip_point_N_type:  Trip point #N's type
    int   trip_point_N_temp:  Trip point #N's temperature value
    int   cdevX_trip_point:   Trip point o cooling device #X (index)
    """

    MAX_TRIP_POINTS = 20

    thermal_fields = {
        'enabled':              ['enabled', str],
        'temp':                 ['temp', int],
        'type':                 ['type', str],
        'num_points_tripped':   ['', '']
        }
    path = '/sys/class/thermal/thermal_zone*'

    def __init__(self, path=None):
        # Browse the thermal folder for trip point fields.
        self.num_trip_points = 0

        thermal_fields = glob.glob(path + '/*')
        for file in thermal_fields:
            field = file[len(path + '/'):]
            if field.find('trip_point') != -1:
                if field.find('temp'):
                    field_type = int
                else:
                    field_type = str
                self.thermal_fields[field] = [field, field_type]

                # Count the number of trip points.
                if field.find('_type') != -1:
                    self.num_trip_points += 1

        super(ThermalStatACPI, self).__init__(self.thermal_fields, path)
        self.update()

    def update(self):
        if not os.path.exists(self.path):
            return

        self.read_all_vals()
        self.num_points_tripped = 0

        for field in self.thermal_fields:
            if field.find('trip_point_') != -1 and field.find('_temp') != -1 \
                    and self.temp > self.read_val(field, int):
                self.num_points_tripped += 1
                logging.info('Temperature trip point #%s tripped.', \
                            field[len('trip_point_'):field.rfind('_temp')])


class ThermalStatHwmon(DevStat):
    """
    hwmon-based thermal status.

    Fields:
    int   <tname>_temp<num>_input: Current temperature in millidegrees Celsius
      where:
          <tname> : name of hwmon device in sysfs
          <num>   : number of temp as some hwmon devices have multiple

    """
    path = '/sys/class/hwmon'

    thermal_fields = {}
    def __init__(self, rootpath=None):
        if not rootpath:
            rootpath = self.path
        for subpath1 in glob.glob('%s/hwmon*' % rootpath):
            for subpath2 in ['','device/']:
                gpaths = glob.glob("%s/%stemp*_input" % (subpath1, subpath2))
                for gpath in gpaths:
                    bname = os.path.basename(gpath)
                    field_path = os.path.join(subpath1, subpath2, bname)

                    tname_path = os.path.join(os.path.dirname(gpath), "name")
                    tname = utils.read_one_line(tname_path)

                    field_key = "%s_%s" % (tname, bname)
                    self.thermal_fields[field_key] = [field_path, int]

        super(ThermalStatHwmon, self).__init__(self.thermal_fields, rootpath)
        self.update()

    def update(self):
        if not os.path.exists(self.path):
            return

        self.read_all_vals()

    def read_val(self,  file_name, field_type):
        try:
            path = os.path.join(self.path, file_name)
            f = open(path, 'r')
            out = f.readline()
            return field_type(out)
        except:
            return field_type(0)


class ThermalStat(object):
    """helper class to instantiate various thermal devices."""
    def __init__(self):
        self._thermals = []
        self.min_temp = 999999999
        self.max_temp = -999999999

        thermal_stat_types = [(ThermalStatHwmon.path, ThermalStatHwmon),
                              (ThermalStatACPI.path, ThermalStatACPI)]
        for thermal_glob_path, thermal_type in thermal_stat_types:
            try:
                thermal_path = glob.glob(thermal_glob_path)[0]
                logging.debug('Using %s for thermal info.', thermal_path)
                self._thermals.append(thermal_type(thermal_path))
            except:
                logging.debug('Could not find thermal path %s, skipping.',
                              thermal_glob_path)


    def get_temps(self):
        """Get temperature readings.

        Returns:
            string of temperature readings.
        """
        temp_str = ''
        for thermal in self._thermals:
            thermal.update()
            for kname in thermal.fields:
                if kname is 'temp' or kname.endswith('_input'):
                    val = getattr(thermal, kname)
                    temp_str += '%s:%d ' % (kname, val)
                    if val > self.max_temp:
                        self.max_temp = val
                    if val < self.min_temp:
                        self.min_temp = val


        return temp_str


class BatteryStat(DevStat):
    """
    Battery status.

    Fields:

    float charge_full:        Last full capacity reached [Ah]
    float charge_full_design: Full capacity by design [Ah]
    float charge_now:         Remaining charge [Ah]
    float current_now:        Battery discharge rate [A]
    float energy:             Current battery charge [Wh]
    float energy_full:        Last full capacity reached [Wh]
    float energy_full_design: Full capacity by design [Wh]
    float energy_rate:        Battery discharge rate [W]
    float power_now:          Battery discharge rate [W]
    float remaining_time:     Remaining discharging time [h]
    float voltage_min_design: Minimum voltage by design [V]
    float voltage_max_design: Maximum voltage by design [V]
    float voltage_now:        Voltage now [V]
    """

    battery_fields = {
        'status':               ['status', str],
        'charge_full':          ['charge_full', float],
        'charge_full_design':   ['charge_full_design', float],
        'charge_now':           ['charge_now', float],
        'current_now':          ['current_now', float],
        'voltage_min_design':   ['voltage_min_design', float],
        'voltage_max_design':   ['voltage_max_design', float],
        'voltage_now':          ['voltage_now', float],
        'energy':               ['energy_now', float],
        'energy_full':          ['energy_full', float],
        'energy_full_design':   ['energy_full_design', float],
        'power_now':            ['power_now', float],
        'present':              ['present', int],
        'energy_rate':          ['', ''],
        'remaining_time':       ['', '']
        }

    def __init__(self, path=None):
        super(BatteryStat, self).__init__(self.battery_fields, path)
        self.update()


    def update(self):
        for _ in xrange(BATTERY_RETRY_COUNT):
            try:
                self._read_battery()
                return
            except error.TestError as e:
                logging.warn(e)
                for field, prop in self.battery_fields.iteritems():
                    logging.warn(field + ': ' + repr(getattr(self, field)))
                continue
        raise error.TestError('Failed to read battery state')


    def _read_battery(self):
        self.read_all_vals()

        if self.charge_full == 0 and self.energy_full != 0:
            battery_type = BatteryDataReportType.ENERGY
        else:
            battery_type = BatteryDataReportType.CHARGE

        if self.voltage_min_design != 0:
            voltage_nominal = self.voltage_min_design
        else:
            voltage_nominal = self.voltage_now

        if voltage_nominal == 0:
            raise error.TestError('Failed to determine battery voltage')

        # Since charge data is present, calculate parameters based upon
        # reported charge data.
        if battery_type == BatteryDataReportType.CHARGE:
            self.charge_full = self.charge_full / BATTERY_DATA_SCALE
            self.charge_full_design = self.charge_full_design / \
                                      BATTERY_DATA_SCALE
            self.charge_now = self.charge_now / BATTERY_DATA_SCALE

            self.current_now = math.fabs(self.current_now) / \
                               BATTERY_DATA_SCALE

            self.energy =  voltage_nominal * \
                           self.charge_now / \
                           BATTERY_DATA_SCALE
            self.energy_full = voltage_nominal * \
                               self.charge_full / \
                               BATTERY_DATA_SCALE
            self.energy_full_design = voltage_nominal * \
                                      self.charge_full_design / \
                                      BATTERY_DATA_SCALE

        # Charge data not present, so calculate parameters based upon
        # reported energy data.
        elif battery_type == BatteryDataReportType.ENERGY:
            self.charge_full = self.energy_full / voltage_nominal
            self.charge_full_design = self.energy_full_design / \
                                      voltage_nominal
            self.charge_now = self.energy / voltage_nominal

            # TODO(shawnn): check if power_now can really be reported
            # as negative, in the same way current_now can
            self.current_now = math.fabs(self.power_now) / \
                               voltage_nominal

            self.energy = self.energy / BATTERY_DATA_SCALE
            self.energy_full = self.energy_full / BATTERY_DATA_SCALE
            self.energy_full_design = self.energy_full_design / \
                                      BATTERY_DATA_SCALE

        self.voltage_min_design = self.voltage_min_design / \
                                  BATTERY_DATA_SCALE
        self.voltage_max_design = self.voltage_max_design / \
                                  BATTERY_DATA_SCALE
        self.voltage_now = self.voltage_now / \
                           BATTERY_DATA_SCALE
        voltage_nominal = voltage_nominal / \
                          BATTERY_DATA_SCALE

        self.energy_rate =  self.voltage_now * self.current_now

        self.remaining_time = 0
        if self.current_now and self.energy_rate:
            self.remaining_time =  self.energy / self.energy_rate

        if self.charge_full > (self.charge_full_design * 1.5):
            raise error.TestError('Unreasonable charge_full value')
        if self.charge_now > (self.charge_full_design * 1.5):
            raise error.TestError('Unreasonable charge_now value')


class LineStatDummy(DevStat):
    """
    Dummy line stat for devices which don't provide power_supply related sysfs
    interface.
    """
    def __init__(self):
        self.online = True


    def update(self):
        pass

class LineStat(DevStat):
    """
    Power line status.

    Fields:

    bool online:              Line power online
    """

    linepower_fields = {
        'is_online':             ['online', int],
        'status':                ['status', str]
        }


    def __init__(self, path=None):
        super(LineStat, self).__init__(self.linepower_fields, path)
        logging.debug("line path: %s", path)
        self.update()


    def update(self):
        self.read_all_vals()
        self.online = self.is_online == 1


class SysStat(object):
    """
    System power status for a given host.

    Fields:

    battery:   A BatteryStat object.
    linepower: A list of LineStat objects.
    """
    psu_types = ['Mains', 'USB', 'USB_ACA', 'USB_C', 'USB_CDP', 'USB_DCP',
                 'USB_PD', 'USB_PD_DRP', 'Unknown']

    def __init__(self):
        power_supply_path = '/sys/class/power_supply/*'
        self.battery = None
        self.linepower = []
        self.thermal = None
        self.battery_path = None
        self.linepower_path = []

        power_supplies = glob.glob(power_supply_path)
        for path in power_supplies:
            type_path = os.path.join(path,'type')
            if not os.path.exists(type_path):
                continue
            power_type = utils.read_one_line(type_path)
            if power_type == 'Battery':
                scope_path = os.path.join(path,'scope')
                if (os.path.exists(scope_path) and
                        utils.read_one_line(scope_path) == 'Device'):
                    continue
                self.battery_path = path
            elif power_type in self.psu_types:
                self.linepower_path.append(path)

        if not self.battery_path or not self.linepower_path:
            logging.warning("System does not provide power sysfs interface")

        self.thermal = ThermalStat()
        if self.battery_path:
            self.sys_low_batt_p = float(utils.system_output(
                    'check_powerd_config --low_battery_shutdown_percent',
                    ignore_status=True) or 4.0)


    def refresh(self):
        """
        Initialize device power status objects.
        """
        self.linepower = []

        if self.battery_path:
            self.battery = BatteryStat(self.battery_path)

        for path in self.linepower_path:
            self.linepower.append(LineStat(path))
        if not self.linepower:
            self.linepower = [ LineStatDummy() ]

        temp_str = self.thermal.get_temps()
        if temp_str:
            logging.info('Temperature reading: %s', temp_str)
        else:
            logging.error('Could not read temperature, skipping.')


    def on_ac(self):
        """
        Returns true if device is currently running from AC power.
        """
        on_ac = False
        for linepower in self.linepower:
            on_ac |= linepower.online

        # Butterfly can incorrectly report AC online for some time after
        # unplug. Check battery discharge state to confirm.
        if utils.get_board() == 'butterfly':
            on_ac &= (not self.battery_discharging())
        return on_ac


    def battery_charging(self):
        """
        Returns true if battery is currently charging or false otherwise.
        """
        for linepower in self.linepower:
            if linepower.status == 'Charging':
                return True

        if not self.battery_path:
            logging.warn('Unable to determine battery charge status')
            return False

        return self.battery.status.rstrip() == 'Charging'


    def battery_discharging(self):
        """
        Returns true if battery is currently discharging or false otherwise.
        """
        if not self.battery_path:
            logging.warn('Unable to determine battery discharge status')
            return False

        return self.battery.status.rstrip() == 'Discharging'

    def battery_full(self):
        """
        Returns true if battery is currently full or false otherwise.
        """
        if not self.battery_path:
            logging.warn('Unable to determine battery fullness status')
            return False

        return self.battery.status.rstrip() == 'Full'


    def battery_discharge_ok_on_ac(self):
        """Returns True if battery is ok to discharge on AC presently.

        some devices cycle between charge & discharge above a certain
        SoC.  If AC is charging and SoC > 95% we can safely assume that.
        """
        return self.battery_charging() and (self.percent_current_charge() > 95)


    def percent_current_charge(self):
        """Returns current charge compare to design capacity in percent.
        """
        return self.battery.charge_now * 100 / \
               self.battery.charge_full_design


    def percent_display_charge(self):
        """Returns current display charge in percent.
        """
        keyvals = parse_power_supply_info()
        return float(keyvals['Battery']['display percentage'])


    def assert_battery_state(self, percent_initial_charge_min):
        """Check initial power configuration state is battery.

        Args:
          percent_initial_charge_min: float between 0 -> 1.00 of
            percentage of battery that must be remaining.
            None|0|False means check not performed.

        Raises:
          TestError: if one of battery assertions fails
        """
        if self.on_ac():
            raise error.TestError(
                'Running on AC power. Please remove AC power cable.')

        percent_initial_charge = self.percent_current_charge()

        if percent_initial_charge_min and percent_initial_charge < \
                                          percent_initial_charge_min:
            raise error.TestError('Initial charge (%f) less than min (%f)'
                      % (percent_initial_charge, percent_initial_charge_min))

    def assert_battery_in_range(self, min_level, max_level):
        """Raise a error.TestFail if the battery level is not in range."""
        current_percent = self.percent_display_charge()
        if not (min_level <= current_percent <= max_level):
            raise error.TestFail('battery must be in range [{}, {}]'.format(
                                 min_level, max_level))

    def is_low_battery(self, low_batt_margin_p=2.0):
        """Returns True if battery current charge is low

        @param low_batt_margin_p: percentage of battery that would be added to
                                  system low battery level.
        """
        return (self.battery_discharging() and
                self.percent_current_charge() < self.sys_low_batt_p +
                                                low_batt_margin_p)


def get_status():
    """
    Return a new power status object (SysStat). A new power status snapshot
    for a given host can be obtained by either calling this routine again and
    constructing a new SysStat object, or by using the refresh method of the
    SysStat object.
    """
    status = SysStat()
    status.refresh()
    return status


def poll_for_charging_behavior(behavior, timeout):
    """
    Wait up to |timeout| seconds for the charging behavior to become |behavior|.

    @param behavior: One of 'ON_AC_AND_CHARGING',
                            'ON_AC_AND_NOT_CHARGING',
                            'NOT_ON_AC_AND_NOT_CHARGING'.
    @param timeout: in seconds.

    @raises: error.TestFail if the behavior does not match in time, or another
             exception if something else fails along the way.
    """
    ps = get_status()

    def _verify_on_AC_and_charging():
        ps.refresh()
        if not ps.on_ac():
            raise error.TestFail('Device is not on AC, but should be')
        if not ps.battery_charging():
            raise error.TestFail('Device is not charging, but should be')
        return True

    def _verify_on_AC_and_not_charging():
        ps.refresh()
        if not ps.on_ac():
            raise error.TestFail('Device is not on AC, but should be')
        if ps.battery_charging():
            raise error.TestFail('Device is charging, but should not be')
        return True

    def _verify_not_on_AC_and_not_charging():
        ps.refresh()
        if ps.on_ac():
            raise error.TestFail('Device is on AC, but should not be')
        return True

    poll_functions = {
        'ON_AC_AND_CHARGING'        : _verify_on_AC_and_charging,
        'ON_AC_AND_NOT_CHARGING'    : _verify_on_AC_and_not_charging,
        'NOT_ON_AC_AND_NOT_CHARGING': _verify_not_on_AC_and_not_charging,
    }
    poll_for_condition_ex(poll_functions[behavior],
                          timeout=timeout,
                          sleep_interval=1)

class AbstractStats(object):
    """
    Common superclass for measurements of percentages per state over time.

    Public Attributes:
        incremental:  If False, stats returned are from a single
        _read_stats.  Otherwise, stats are from the difference between
        the current and last refresh.
    """

    @staticmethod
    def to_percent(stats):
        """
        Turns a dict with absolute time values into a dict with percentages.
        """
        total = sum(stats.itervalues())
        if total == 0:
            return {k: 0 for k in stats}
        return dict((k, v * 100.0 / total) for (k, v) in stats.iteritems())


    @staticmethod
    def do_diff(new, old):
        """
        Returns a dict with value deltas from two dicts with matching keys.
        """
        return dict((k, new[k] - old.get(k, 0)) for k in new.iterkeys())


    @staticmethod
    def format_results_percent(results, name, percent_stats):
        """
        Formats autotest result keys to format:
          percent_<name>_<key>_time
        """
        for key in percent_stats:
            results['percent_%s_%s_time' % (name, key)] = percent_stats[key]


    @staticmethod
    def format_results_wavg(results, name, wavg):
        """
        Add an autotest result keys to format: wavg_<name>
        """
        if wavg is not None:
            results['wavg_%s' % (name)] = wavg


    def __init__(self, name=None, incremental=True):
        if not name:
            raise error.TestFail("Need to name AbstractStats instance please.")
        self.name = name
        self.incremental = incremental
        self._stats = self._read_stats()


    def refresh(self):
        """
        Returns dict mapping state names to percentage of time spent in them.
        """
        raw_stats = result = self._read_stats()
        if self.incremental:
            result = self.do_diff(result, self._stats)
        self._stats = raw_stats
        return self.to_percent(result)


    def _automatic_weighted_average(self):
        """
        Turns a dict with absolute times (or percentages) into a weighted
        average value.
        """
        total = sum(self._stats.itervalues())
        if total == 0:
            return None

        return sum((float(k)*v) / total for (k, v) in self._stats.iteritems())


    def _supports_automatic_weighted_average(self):
        """
        Override!

        Returns True if stats collected can be automatically converted from
        percent distribution to weighted average. False otherwise.
        """
        return False


    def weighted_average(self):
        """
        Return weighted average calculated using the automated average method
        (if supported) or using a custom method defined by the stat.
        """
        if self._supports_automatic_weighted_average():
            return self._automatic_weighted_average()

        return self._weighted_avg_fn()


    def _weighted_avg_fn(self):
        """
        Override! Custom weighted average function.

        Returns weighted average as a single floating point value.
        """
        return None


    def _read_stats(self):
        """
        Override! Reads the raw data values that shall be measured into a dict.
        """
        raise NotImplementedError('Override _read_stats in the subclass!')


CPU_BASE_PATH = '/sys/devices/system/cpu/'

def count_all_cpus():
    """
    Return count of cpus on system.
    """
    path = '%s/cpu[0-9]*' % CPU_BASE_PATH
    return len(glob.glob(path))

def get_online_cpus():
    """
    Return list of integer cpu numbers that are online.
    """
    cpus = [int(f.split('/')[-1]) for f in glob.iglob('/dev/cpu/[0-9]*')]
    return frozenset(cpus)

def get_cpus_filepaths_for_suffix(cpus, suffix):
    """
    For each cpu in |cpus| check whether |CPU_BASE_PATH|/cpu%d/|suffix| exists.
    Return tuple of two lists t:
                    t[0]: all cpu ids where the condition above holds
                    t[1]: all full paths where condition above holds.
    """
    available_cpus = []
    available_paths = []
    for c in cpus:
        c_file_path = os.path.join(CPU_BASE_PATH, 'cpu%d' % c, suffix)
        if os.path.exists(c_file_path):
          available_cpus.append(c)
          available_paths.append(c_file_path)
    return (available_cpus, available_paths)

class CPUFreqStats(AbstractStats):
    """
    CPU Frequency statistics
    """
    MSR_PLATFORM_INFO = 0xce
    MSR_IA32_MPERF = 0xe7
    MSR_IA32_APERF = 0xe8

    def __init__(self, cpus=None):
        name = 'cpufreq'
        stats_suffix = 'cpufreq/stats/time_in_state'
        key_suffix = 'cpufreq/scaling_available_frequencies'
        cpufreq_driver = '/sys/devices/system/cpu/cpu0/cpufreq/scaling_driver'
        if not cpus:
            cpus = get_online_cpus()
        _, self._file_paths = get_cpus_filepaths_for_suffix(cpus,
                                                            stats_suffix)

        if len(cpus) and len(cpus) < count_all_cpus():
            name = '%s_%s' % (name, '_'.join([str(c) for c in cpus]))
        self._running_intel_pstate = False
        self._initial_perf = None
        self._current_perf = None
        self._max_freq = 0
        self._cpus = cpus
        self._available_freqs = set()

        if not self._file_paths:
            logging.debug('time_in_state file not found')

        # assumes cpufreq driver for CPU0 is the same as the others.
        freq_driver = utils.read_one_line(cpufreq_driver)
        if freq_driver == 'intel_pstate':
            logging.debug('intel_pstate driver active')
            self._running_intel_pstate = True
        else:
            _, cpufreq_key_paths = get_cpus_filepaths_for_suffix(cpus,
                                                                 key_suffix)
            for path in cpufreq_key_paths:
                self._available_freqs |= set(int(x) for x in
                                             utils.read_file(path).split())

        super(CPUFreqStats, self).__init__(name=name)


    def _read_stats(self):
        if self._running_intel_pstate:
            aperf = 0
            mperf = 0

            for cpu in self._cpus:
                aperf += utils.rdmsr(self.MSR_IA32_APERF, cpu)
                mperf += utils.rdmsr(self.MSR_IA32_MPERF, cpu)

            # max_freq is supposed to be the same for all CPUs and remain
            # constant throughout. So, we set the entry only once.
            # Note that this is max non-turbo frequency, some CPU can run at
            # higher turbo frequency in some condition.
            if not self._max_freq:
                platform_info = utils.rdmsr(self.MSR_PLATFORM_INFO)
                mul = platform_info >> 8 & 0xff
                bclk = utils.get_intel_bclk_khz()
                self._max_freq = mul * bclk

            if not self._initial_perf:
                self._initial_perf = (aperf, mperf)

            self._current_perf = (aperf, mperf)

        stats = dict((k, 0) for k in self._available_freqs)
        for path in self._file_paths:
            if not os.path.exists(path):
                logging.debug('%s is not found', path)
                continue

            data = utils.read_file(path)
            for line in data.splitlines():
                pair = line.split()
                freq = int(pair[0])
                timeunits = int(pair[1])
                if freq in stats:
                    stats[freq] += timeunits
                else:
                    stats[freq] = timeunits
        return stats


    def _supports_automatic_weighted_average(self):
        return not self._running_intel_pstate


    def _weighted_avg_fn(self):
        if not self._running_intel_pstate:
            return None

        if self._current_perf[1] != self._initial_perf[1]:
            # Avg freq = max_freq * aperf_delta / mperf_delta
            return self._max_freq * \
                float(self._current_perf[0] - self._initial_perf[0]) / \
                (self._current_perf[1] - self._initial_perf[1])
        return 1.0


class CPUCStateStats(AbstractStats):
    """
    Base class for C-state residency statistics
    """
    def __init__(self, name, non_c0_stat=''):
        self._non_c0_stat = non_c0_stat
        super(CPUCStateStats, self).__init__(name=name)


    def to_percent(self, stats):
        """
        Turns a dict with absolute time values into a dict with percentages.
        Ignore the |non_c0_stat_name| which is aggegate stat in the total count.
        """
        total = sum(v for k, v in stats.iteritems() if k != self._non_c0_stat)
        if total == 0:
            return {k: 0 for k in stats}
        return {k: v * 100.0 / total for k, v in stats.iteritems()}


class CPUIdleStats(CPUCStateStats):
    """
    CPU Idle statistics (refresh() will not work with incremental=False!)
    """
    # TODO (snanda): Handle changes in number of c-states due to events such
    # as ac <-> battery transitions.
    # TODO (snanda): Handle non-S0 states. Time spent in suspend states is
    # currently not factored out.
    def __init__(self, cpus=None):
        name = 'cpuidle'
        cpuidle_suffix = 'cpuidle'
        if not cpus:
            cpus = get_online_cpus()
        cpus, self._cpus = get_cpus_filepaths_for_suffix(cpus, cpuidle_suffix)
        if len(cpus) and len(cpus) < count_all_cpus():
            name = '%s_%s' % (name, '_'.join([str(c) for c in cpus]))
        super(CPUIdleStats, self).__init__(name=name, non_c0_stat='non-C0')


    def _read_stats(self):
        cpuidle_stats = collections.defaultdict(int)
        epoch_usecs = int(time.time() * 1000 * 1000)
        for cpu in self._cpus:
            state_path = os.path.join(cpu, 'state*')
            states = glob.glob(state_path)
            cpuidle_stats['C0'] += epoch_usecs

            for state in states:
                name = utils.read_one_line(os.path.join(state, 'name'))
                latency = utils.read_one_line(os.path.join(state, 'latency'))

                if not int(latency) and name == 'POLL':
                    # C0 state. Kernel stats aren't right, so calculate by
                    # subtracting all other states from total time (using epoch
                    # timer since we calculate differences in the end anyway).
                    # NOTE: Only x86 lists C0 under cpuidle, ARM does not.
                    continue

                usecs = int(utils.read_one_line(os.path.join(state, 'time')))
                cpuidle_stats['C0'] -= usecs

                if name == '<null>':
                    # Kernel race condition that can happen while a new C-state
                    # gets added (e.g. AC->battery). Don't know the 'name' of
                    # the state yet, but its 'time' would be 0 anyway.
                    logging.warning('Read name: <null>, time: %d from %s...'
                                    'skipping.', usecs, state)
                    continue

                cpuidle_stats[name] += usecs
                cpuidle_stats['non-C0'] += usecs

        return cpuidle_stats


class CPUPackageStats(CPUCStateStats):
    """
    Package C-state residency statistics for modern Intel CPUs.
    """

    ATOM         =              {'C2': 0x3F8, 'C4': 0x3F9, 'C6': 0x3FA}
    NEHALEM      =              {'C3': 0x3F8, 'C6': 0x3F9, 'C7': 0x3FA}
    SANDY_BRIDGE = {'C2': 0x60D, 'C3': 0x3F8, 'C6': 0x3F9, 'C7': 0x3FA}
    SILVERMONT   = {'C6': 0x3FA}
    GOLDMONT     = {'C2': 0x60D, 'C3': 0x3F8, 'C6': 0x3F9,'C10': 0x632}
    BROADWELL    = {'C2': 0x60D, 'C3': 0x3F8, 'C6': 0x3F9, 'C7': 0x3FA,
                                 'C8': 0x630, 'C9': 0x631,'C10': 0x632}

    def __init__(self):
        def _get_platform_states():
            """
            Helper to decide what set of microarchitecture-specific MSRs to use.

            Returns: dict that maps C-state name to MSR address, or None.
            """
            cpu_uarch = utils.get_intel_cpu_uarch()

            return {
                # model groups pulled from Intel SDM, volume 4
                # Group same package cstate using the older uarch name
                #
                # TODO(harry.pan): As the keys represent microarchitecture
                # names, we could consider to rename the PC state groups
                # to avoid ambiguity.
                'Airmont':      self.SILVERMONT,
                'Atom':         self.ATOM,
                'Broadwell':    self.BROADWELL,
                'Comet Lake':   self.BROADWELL,
                'Goldmont':     self.GOLDMONT,
                'Haswell':      self.SANDY_BRIDGE,
                'Ice Lake':     self.BROADWELL,
                'Ivy Bridge':   self.SANDY_BRIDGE,
                'Ivy Bridge-E': self.SANDY_BRIDGE,
                'Kaby Lake':    self.BROADWELL,
                'Nehalem':      self.NEHALEM,
                'Sandy Bridge': self.SANDY_BRIDGE,
                'Silvermont':   self.SILVERMONT,
                'Skylake':      self.BROADWELL,
                'Tiger Lake':   self.BROADWELL,
                'Westmere':     self.NEHALEM,
                }.get(cpu_uarch, None)

        self._platform_states = _get_platform_states()
        super(CPUPackageStats, self).__init__(name='cpupkg',
                                              non_c0_stat='non-C0_C1')


    def _read_stats(self):
        packages = set()
        template = '/sys/devices/system/cpu/cpu%s/topology/physical_package_id'
        if not self._platform_states:
            return {}
        stats = dict((state, 0) for state in self._platform_states)
        stats['C0_C1'] = 0
        stats['non-C0_C1'] = 0

        for cpu in os.listdir('/dev/cpu'):
            if not os.path.exists(template % cpu):
                continue
            package = utils.read_one_line(template % cpu)
            if package in packages:
                continue
            packages.add(package)

            stats['C0_C1'] += utils.rdmsr(0x10, cpu) # TSC
            for (state, msr) in self._platform_states.iteritems():
                ticks = utils.rdmsr(msr, cpu)
                stats[state] += ticks
                stats['non-C0_C1'] += ticks
                stats['C0_C1'] -= ticks

        return stats


class DevFreqStats(AbstractStats):
    """
    Devfreq device frequency stats.
    """

    _DIR = '/sys/class/devfreq'


    def __init__(self, f):
        """Constructs DevFreqStats Object that track frequency stats
        for the path of the given Devfreq device.

        The frequencies for devfreq devices are listed in Hz.

        Args:
            path: the path to the devfreq device

        Example:
            /sys/class/devfreq/dmc
        """
        self._path = os.path.join(self._DIR, f)
        if not os.path.exists(self._path):
            raise error.TestError('DevFreqStats: devfreq device does not exist')

        fname = os.path.join(self._path, 'available_frequencies')
        af = utils.read_one_line(fname).strip()
        self._available_freqs = sorted(af.split(), key=int)

        super(DevFreqStats, self).__init__(name=f)

    def _read_stats(self):
        stats = dict((freq, 0) for freq in self._available_freqs)
        fname = os.path.join(self._path, 'trans_stat')

        with open(fname) as fd:
            # The lines that contain the time in each frequency start on the 3rd
            # line, so skip the first 2 lines. The last line contains the number
            # of transitions, so skip that line too.
            # The time in each frequency is at the end of the line.
            freq_pattern = re.compile(r'\d+(?=:)')
            for line in fd.readlines()[2:-1]:
                freq = freq_pattern.search(line)
                if freq and freq.group() in self._available_freqs:
                    stats[freq.group()] = int(line.strip().split()[-1])

        return stats


class GPUFreqStats(AbstractStats):
    """GPU Frequency statistics class.

    TODO(tbroch): add stats for other GPUs
    """

    _MALI_DEV = '/sys/class/misc/mali0/device'
    _MALI_EVENTS = ['mali_dvfs:mali_dvfs_set_clock']
    _MALI_TRACE_CLK_RE = \
            r'kworker.* (\d+\.\d+): mali_dvfs_set_clock: frequency=(\d+)\d{6}'

    _I915_ROOT = '/sys/kernel/debug/dri/0'
    _I915_EVENTS = ['i915:intel_gpu_freq_change']
    _I915_CLKS_FILES = ['i915_cur_delayinfo', 'i915_frequency_info']
    _I915_TRACE_CLK_RE = \
            r'kworker.* (\d+\.\d+): intel_gpu_freq_change: new_freq=(\d+)'
    _I915_CUR_FREQ_RE = r'CAGF:\s+(\d+)MHz'
    _I915_MIN_FREQ_RE = r'Lowest \(RPN\) frequency:\s+(\d+)MHz'
    _I915_MAX_FREQ_RE = r'Max non-overclocked \(RP0\) frequency:\s+(\d+)MHz'
    # There are 6 frequency steps per 100 MHz
    _I915_FREQ_STEPS = [0, 17, 33, 50, 67, 83]

    _gpu_type = None


    def _get_mali_freqs(self):
        """Get mali clocks based on kernel version.

        For 3.8-3.18:
            # cat /sys/class/misc/mali0/device/clock
            100000000
            # cat /sys/class/misc/mali0/device/available_frequencies
            100000000
            160000000
            266000000
            350000000
            400000000
            450000000
            533000000
            533000000

        For 4.4+:
            Tracked in DevFreqStats

        Returns:
          cur_mhz: string of current GPU clock in mhz
        """
        cur_mhz = None
        fqs = []

        fname = os.path.join(self._MALI_DEV, 'clock')
        if os.path.exists(fname):
            cur_mhz = str(int(int(utils.read_one_line(fname).strip()) / 1e6))
            fname = os.path.join(self._MALI_DEV, 'available_frequencies')
            with open(fname) as fd:
                for ln in fd.readlines():
                    freq = int(int(ln.strip()) / 1e6)
                    fqs.append(str(freq))
                fqs.sort()

        self._freqs = fqs
        return cur_mhz


    def __init__(self, incremental=False):


        min_mhz = None
        max_mhz = None
        cur_mhz = None
        events = None
        i915_path = None
        self._freqs = []
        self._prev_sample = None
        self._trace = None

        if os.path.exists(self._MALI_DEV) and \
           not os.path.exists(os.path.join(self._MALI_DEV, "devfreq")):
            self._set_gpu_type('mali')
        else:
            for file_name in self._I915_CLKS_FILES:
                full_path = os.path.join(self._I915_ROOT, file_name)
                if os.path.exists(full_path):
                    self._set_gpu_type('i915')
                    i915_path = full_path
                    break
            else:
                # We either don't know how to track GPU stats (yet) or the stats
                # are tracked in DevFreqStats.
                self._set_gpu_type(None)

        logging.debug("gpu_type is %s", self._gpu_type)

        if self._gpu_type is 'mali':
            events = self._MALI_EVENTS
            cur_mhz = self._get_mali_freqs()
            if self._freqs:
                min_mhz = self._freqs[0]
                max_mhz = self._freqs[-1]

        elif self._gpu_type is 'i915':
            events = self._I915_EVENTS
            with open(i915_path) as fd:
                for ln in fd.readlines():
                    logging.debug("ln = %s", ln)
                    result = re.findall(self._I915_CUR_FREQ_RE, ln)
                    if result:
                        cur_mhz = result[0]
                        continue
                    result = re.findall(self._I915_MIN_FREQ_RE, ln)
                    if result:
                        min_mhz = result[0]
                        continue
                    result = re.findall(self._I915_MAX_FREQ_RE, ln)
                    if result:
                        max_mhz = result[0]
                        continue
                if min_mhz and max_mhz:
                    for i in xrange(int(min_mhz), int(max_mhz) + 1):
                        if i % 100 in self._I915_FREQ_STEPS:
                            self._freqs.append(str(i))

        logging.debug("cur_mhz = %s, min_mhz = %s, max_mhz = %s", cur_mhz,
                      min_mhz, max_mhz)

        if cur_mhz and min_mhz and max_mhz:
            self._trace = kernel_trace.KernelTrace(events=events)

        # Not all platforms or kernel versions support tracing.
        if not self._trace or not self._trace.is_tracing():
            logging.warning("GPU frequency tracing not enabled.")
        else:
            self._prev_sample = (cur_mhz, self._trace.uptime_secs())
            logging.debug("Current GPU freq: %s", cur_mhz)
            logging.debug("All GPU freqs: %s", self._freqs)

        super(GPUFreqStats, self).__init__(name='gpufreq', incremental=incremental)


    @classmethod
    def _set_gpu_type(cls, gpu_type):
        cls._gpu_type = gpu_type


    def _read_stats(self):
        if self._gpu_type:
            return getattr(self, "_%s_read_stats" % self._gpu_type)()
        return {}


    def _trace_read_stats(self, regexp):
        """Read GPU stats from kernel trace outputs.

        Args:
            regexp: regular expression to match trace output for frequency

        Returns:
            Dict with key string in mhz and val float in seconds.
        """
        if not self._prev_sample:
            return {}

        stats = dict((k, 0.0) for k in self._freqs)
        results = self._trace.read(regexp=regexp)
        for (tstamp_str, freq) in results:
            tstamp = float(tstamp_str)

            # do not reparse lines in trace buffer
            if tstamp <= self._prev_sample[1]:
                continue
            delta = tstamp - self._prev_sample[1]
            logging.debug("freq:%s tstamp:%f - %f delta:%f",
                          self._prev_sample[0],
                          tstamp, self._prev_sample[1],
                          delta)
            stats[self._prev_sample[0]] += delta
            self._prev_sample = (freq, tstamp)

        # Do last record
        delta = self._trace.uptime_secs() - self._prev_sample[1]
        logging.debug("freq:%s tstamp:uptime - %f delta:%f",
                      self._prev_sample[0],
                      self._prev_sample[1], delta)
        stats[self._prev_sample[0]] += delta

        logging.debug("GPU freq percents:%s", stats)
        return stats


    def _mali_read_stats(self):
        """Read Mali GPU stats

        Frequencies are reported in Hz, so use a regex that drops the last 6
        digits.

        Output in trace looks like this:

            kworker/u:24-5220  [000] .... 81060.329232: mali_dvfs_set_clock: frequency=400
            kworker/u:24-5220  [000] .... 81061.830128: mali_dvfs_set_clock: frequency=350

        Returns:
            Dict with frequency in mhz as key and float in seconds for time
              spent at that frequency.
        """
        return self._trace_read_stats(self._MALI_TRACE_CLK_RE)


    def _i915_read_stats(self):
        """Read i915 GPU stats.

        Output looks like this (kernel >= 3.8):

          kworker/u:0-28247 [000] .... 259391.579610: intel_gpu_freq_change: new_freq=400
          kworker/u:0-28247 [000] .... 259391.581797: intel_gpu_freq_change: new_freq=350

        Returns:
            Dict with frequency in mhz as key and float in seconds for time
              spent at that frequency.
        """
        return self._trace_read_stats(self._I915_TRACE_CLK_RE)


    def _supports_automatic_weighted_average(self):
        return self._gpu_type is not None


class USBSuspendStats(AbstractStats):
    """
    USB active/suspend statistics (over all devices)
    """
    # TODO (snanda): handle hot (un)plugging of USB devices
    # TODO (snanda): handle duration counters wraparound

    def __init__(self):
        usb_stats_path = '/sys/bus/usb/devices/*/power'
        self._file_paths = glob.glob(usb_stats_path)
        if not self._file_paths:
            logging.debug('USB stats path not found')
        super(USBSuspendStats, self).__init__(name='usb')


    def _read_stats(self):
        usb_stats = {'active': 0, 'suspended': 0}

        for path in self._file_paths:
            active_duration_path = os.path.join(path, 'active_duration')
            total_duration_path = os.path.join(path, 'connected_duration')

            if not os.path.exists(active_duration_path) or \
               not os.path.exists(total_duration_path):
                logging.debug('duration paths do not exist for: %s', path)
                continue

            active = int(utils.read_file(active_duration_path))
            total = int(utils.read_file(total_duration_path))
            logging.debug('device %s active for %.2f%%',
                          path, active * 100.0 / total)

            usb_stats['active'] += active
            usb_stats['suspended'] += total - active

        return usb_stats


def get_cpu_sibling_groups():
    """
    Get CPU core groups in HMP systems.

    In systems with both small core and big core,
    returns groups of small and big sibling groups.
    """
    siblings_suffix = 'topology/core_siblings_list'
    sibling_groups = []
    cpus_processed = set()
    cpus, sibling_file_paths = get_cpus_filepaths_for_suffix(get_online_cpus(),
                                                             siblings_suffix)
    for c, siblings_path in zip(cpus, sibling_file_paths):
        if c in cpus_processed:
            # This cpu is already part of a sibling group. Skip.
            continue
        siblings_data = utils.read_file(siblings_path)
        sibling_group = set()
        for sibling_entry in siblings_data.split(','):
            entry_data = sibling_entry.split('-')
            sibling_start = sibling_end = int(entry_data[0])
            if len(entry_data) > 1:
              sibling_end = int(entry_data[1])
            siblings = set(range(sibling_start, sibling_end + 1))
            sibling_group |= siblings
        cpus_processed |= sibling_group
        sibling_groups.append(frozenset(sibling_group))
    return tuple(sibling_groups)


def get_available_cpu_stats():
    """Return CPUFreq/CPUIdleStats groups by big-small siblings groups."""
    ret = [CPUPackageStats()]
    cpu_sibling_groups = get_cpu_sibling_groups()
    if not cpu_sibling_groups:
        ret.append(CPUFreqStats())
        ret.append(CPUIdleStats())
    for cpu_group in cpu_sibling_groups:
        ret.append(CPUFreqStats(cpu_group))
        ret.append(CPUIdleStats(cpu_group))
    if has_rc6_support():
        ret.append(GPURC6Stats())
    return ret


class StatoMatic(object):
    """Class to aggregate and monitor a bunch of power related statistics."""
    def __init__(self):
        self._start_uptime_secs = kernel_trace.KernelTrace.uptime_secs()
        self._astats = [USBSuspendStats(), GPUFreqStats(incremental=False)]
        self._astats.extend(get_available_cpu_stats())
        if os.path.isdir(DevFreqStats._DIR):
            self._astats.extend([DevFreqStats(f) for f in \
                                 os.listdir(DevFreqStats._DIR)])

        self._disk = DiskStateLogger()
        self._disk.start()


    def publish(self):
        """Publishes results of various statistics gathered.

        Returns:
            dict with
              key = string 'percent_<name>_<key>_time'
              value = float in percent
        """
        results = {}
        tot_secs = kernel_trace.KernelTrace.uptime_secs() - \
            self._start_uptime_secs
        for stat_obj in self._astats:
            percent_stats = stat_obj.refresh()
            logging.debug("pstats = %s", percent_stats)
            if stat_obj.name is 'gpu':
                # TODO(tbroch) remove this once GPU freq stats have proved
                # reliable
                stats_secs = sum(stat_obj._stats.itervalues())
                if stats_secs < (tot_secs * 0.9) or \
                        stats_secs > (tot_secs * 1.1):
                    logging.warning('%s stats dont look right.  Not publishing.',
                                 stat_obj.name)
                    continue
            new_res = {}
            AbstractStats.format_results_percent(new_res, stat_obj.name,
                                                 percent_stats)
            wavg = stat_obj.weighted_average()
            if wavg:
                AbstractStats.format_results_wavg(new_res, stat_obj.name, wavg)

            results.update(new_res)

        new_res = {}
        if self._disk.get_error():
            new_res['disk_logging_error'] = str(self._disk.get_error())
        else:
            AbstractStats.format_results_percent(new_res, 'disk',
                                                 self._disk.result())
        results.update(new_res)

        return results


class PowerMeasurement(object):
    """Class to measure power.

    Public attributes:
        domain: String name of the power domain being measured.  Example is
          'system' for total system power

    Public methods:
        refresh: Performs any power/energy sampling and calculation and returns
            power as float in watts.  This method MUST be implemented in
            subclass.
    """

    def __init__(self, domain):
        """Constructor."""
        self.domain = domain


    def refresh(self):
        """Performs any power/energy sampling and calculation.

        MUST be implemented in subclass

        Returns:
            float, power in watts.
        """
        raise NotImplementedError("'refresh' method should be implemented in "
                                  "subclass.")


def parse_power_supply_info():
    """Parses power_supply_info command output.

    Command output from power_manager ( tools/power_supply_info.cc ) looks like
    this:

        Device: Line Power
          path:               /sys/class/power_supply/cros_ec-charger
          ...
        Device: Battery
          path:               /sys/class/power_supply/sbs-9-000b
          ...

    """
    rv = collections.defaultdict(dict)
    dev = None
    for ln in utils.system_output('power_supply_info').splitlines():
        logging.debug("psu: %s", ln)
        result = re.findall(r'^Device:\s+(.*)', ln)
        if result:
            dev = result[0]
            continue
        result = re.findall(r'\s+(.+):\s+(.+)', ln)
        if result and dev:
            kname = re.findall(r'(.*)\s+\(\w+\)', result[0][0])
            if kname:
                rv[dev][kname[0]] = result[0][1]
            else:
                rv[dev][result[0][0]] = result[0][1]

    return rv


class SystemPower(PowerMeasurement):
    """Class to measure system power.

    TODO(tbroch): This class provides a subset of functionality in BatteryStat
    in hopes of minimizing power draw.  Investigate whether its really
    significant and if not, deprecate.

    Private Attributes:
      _voltage_file: path to retrieve voltage in uvolts
      _current_file: path to retrieve current in uamps
    """

    def __init__(self, battery_dir):
        """Constructor.

        Args:
            battery_dir: path to dir containing the files to probe and log.
                usually something like /sys/class/power_supply/BAT0/
        """
        super(SystemPower, self).__init__('system')
        # Files to log voltage and current from
        self._voltage_file = os.path.join(battery_dir, 'voltage_now')
        self._current_file = os.path.join(battery_dir, 'current_now')


    def refresh(self):
        """refresh method.

        See superclass PowerMeasurement for details.
        """
        keyvals = parse_power_supply_info()
        return float(keyvals['Battery']['energy rate'])


class CheckpointLogger(object):
    """Class to log checkpoint data.

    Public attributes:
        checkpoint_data: dictionary of (tname, tlist).
            tname: String of testname associated with these time intervals
            tlist: list of tuples.  Tuple contains:
                tstart: Float of time when subtest started
                tend: Float of time when subtest ended

    Public methods:
        start: records a start timestamp
        checkpoint
        checkblock
        save_checkpoint_data
        load_checkpoint_data

    Static methods:
        load_checkpoint_data_static

    Private attributes:
       _start_time: start timestamp for checkpoint logger
    """

    def __init__(self):
        self.checkpoint_data = collections.defaultdict(list)
        self.start()

    # If multiple MeasurementLoggers call start() on the same CheckpointLogger,
    # the latest one will register start time.
    def start(self):
        """Set start time for CheckpointLogger."""
        self._start_time = time.time()

    @contextlib.contextmanager
    def checkblock(self, tname=''):
        """Check point for the following block with test tname.

        Args:
            tname: String of testname associated with this time interval
        """
        start_time = time.time()
        yield
        self.checkpoint(tname, start_time)

    def checkpoint(self, tname='', tstart=None, tend=None):
        """Check point the times in seconds associated with test tname.

        Args:
            tname: String of testname associated with this time interval
            tstart: Float in seconds of when tname test started. Should be based
                off time.time(). If None, use start timestamp for the checkpoint
                logger.
            tend: Float in seconds of when tname test ended. Should be based
                off time.time(). If None, then value computed in the method.
        """
        if not tstart and self._start_time:
            tstart = self._start_time
        if not tend:
            tend = time.time()
        self.checkpoint_data[tname].append((tstart, tend))
        logging.info('Finished test "%s" between timestamps [%s, %s]',
                     tname, tstart, tend)

    def convert_relative(self, start_time=None):
        """Convert data from power_status.CheckpointLogger object to relative
        checkpoint data dictionary. Timestamps are converted to time in seconds
        since the test started.

        Args:
            start_time: Float in seconds of the desired start time reference.
                Should be based off time.time(). If None, use start timestamp
                for the checkpoint logger.
        """
        if start_time is None:
            start_time = self._start_time

        checkpoint_dict = {}
        for tname, tlist in self.checkpoint_data.iteritems():
            checkpoint_dict[tname] = [(tstart - start_time, tend - start_time)
                    for tstart, tend in tlist]

        return checkpoint_dict

    def save_checkpoint_data(self, resultsdir, fname=CHECKPOINT_LOG_DEFAULT_FNAME):
        """Save checkpoint data.

        Args:
            resultsdir: String, directory to write results to
            fname: String, name of file to write results to
        """
        fname = os.path.join(resultsdir, fname)
        with file(fname, 'wt') as f:
            json.dump(self.checkpoint_data, f, indent=4, separators=(',', ': '))

    def load_checkpoint_data(self, resultsdir,
                             fname=CHECKPOINT_LOG_DEFAULT_FNAME):
        """Load checkpoint data.

        Args:
            resultsdir: String, directory to load results from
            fname: String, name of file to load results from
        """
        fname = os.path.join(resultsdir, fname)
        try:
            with open(fname, 'r') as f:
                self.checkpoint_data = json.load(f,
                                                 object_hook=to_checkpoint_data)
                # Set start time to the earliest start timestamp in file.
                self._start_time = min(
                        ts_pair[0] for ts_pair in itertools.chain.from_iterable(
                                self.checkpoint_data.itervalues()))
        except Exception as exc:
            logging.warning('Failed to load checkpoint data from json file %s, '
                            'see exception: %s', fname, exc)

    @staticmethod
    def load_checkpoint_data_static(resultsdir,
                                    fname=CHECKPOINT_LOG_DEFAULT_FNAME):
        """Load checkpoint data.

        Args:
            resultsdir: String, directory to load results from
            fname: String, name of file to load results from
        """
        fname = os.path.join(resultsdir, fname)
        with file(fname, 'r') as f:
            checkpoint_data = json.load(f)
        return checkpoint_data


def to_checkpoint_data(json_dict):
    """Helper method to translate json object into CheckpointLogger format.

    Args:
        json_dict: a json object in the format of python dict
    Returns:
        a defaultdict in CheckpointLogger data format
    """
    checkpoint_data = collections.defaultdict(list)
    for tname, tlist in json_dict.iteritems():
        checkpoint_data[tname].extend([tuple(ts_pair) for ts_pair in tlist])
    return checkpoint_data


def get_checkpoint_logger_from_file(resultsdir,
                                    fname=CHECKPOINT_LOG_DEFAULT_FNAME):
    """Create a CheckpointLogger and load checkpoint data from file.

    Args:
        resultsdir: String, directory to load results from
        fname: String, name of file to load results from
    Returns:
        CheckpointLogger with data from file
    """
    checkpoint_logger = CheckpointLogger()
    checkpoint_logger.load_checkpoint_data(resultsdir, fname)
    return checkpoint_logger


class MeasurementLogger(threading.Thread):
    """A thread that logs measurement readings.

    Example code snippet:
        my_logger = MeasurementLogger([Measurement1, Measurement2])
        my_logger.start()
        for testname in tests:
            # Option 1: use checkblock
            with my_logger.checkblock(testname):
               # run the test method for testname

            # Option 2: use checkpoint
            start_time = time.time()
            # run the test method for testname
            my_logger.checkpoint(testname, start_time)

        keyvals = my_logger.calc()

    or using CheckpointLogger:
        checkpoint_logger = CheckpointLogger()
        my_logger = MeasurementLogger([Measurement1, Measurement2],
                                      checkpoint_logger)
        my_logger.start()
        for testname in tests:
            # Option 1: use checkblock
            with checkpoint_logger.checkblock(testname):
               # run the test method for testname

            # Option 2: use checkpoint
            start_time = time.time()
            # run the test method for testname
            checkpoint_logger.checkpoint(testname, start_time)

        keyvals = my_logger.calc()

    Public attributes:
        seconds_period: float, probing interval in seconds.
        readings: list of lists of floats of measurements.
        times: list of floats of time (since Epoch) of when measurements
            occurred.  len(time) == len(readings).
        done: flag to stop the logger.
        domains: list of  domain strings being measured

    Public methods:
        run: launches the thread to gather measurements
        refresh: perform data samplings for every measurements
        calc: calculates
        save_results:

    Private attributes:
        _measurements: list of Measurement objects to be sampled.
        _checkpoint_data: dictionary of (tname, tlist).
            tname: String of testname associated with these time intervals
            tlist: list of tuples.  Tuple contains:
                tstart: Float of time when subtest started
                tend: Float of time when subtest ended
        _results: list of results tuples.  Tuple contains:
            prefix: String of subtest
            mean: Float of mean  in watts
            std: Float of standard deviation of measurements
            tstart: Float of time when subtest started
            tend: Float of time when subtest ended
    """
    def __init__(self, measurements, seconds_period=1.0, checkpoint_logger=None):
        """Initialize a logger.

        Args:
            _measurements: list of Measurement objects to be sampled.
            seconds_period: float, probing interval in seconds.  Default 1.0
        """
        threading.Thread.__init__(self)

        self.seconds_period = seconds_period

        self.readings = []
        self.times = []

        self.domains = []
        self._measurements = measurements
        for meas in self._measurements:
            self.domains.append(meas.domain)

        self._checkpoint_logger = \
            checkpoint_logger if checkpoint_logger else CheckpointLogger()

        self.done = False

    def start(self):
        self._checkpoint_logger.start()
        super(MeasurementLogger, self).start()

    def refresh(self):
        """Perform data samplings for every measurements.

        Returns:
            list of sampled data for every measurements.
        """
        readings = []
        for meas in self._measurements:
            readings.append(meas.refresh())
        return readings

    def run(self):
        """Threads run method."""
        loop = 0
        start_time = time.time()
        while(not self.done):
            # TODO (dbasehore): We probably need proper locking in this file
            # since there have been race conditions with modifying and accessing
            # data.
            self.readings.append(self.refresh())
            current_time = time.time()
            self.times.append(current_time)
            loop += 1
            next_measurement_time = start_time + loop * self.seconds_period
            time.sleep(next_measurement_time - current_time)

    @contextlib.contextmanager
    def checkblock(self, tname=''):
        """Check point for the following block with test tname.

        Args:
            tname: String of testname associated with this time interval
        """
        start_time = time.time()
        yield
        self.checkpoint(tname, start_time)

    def checkpoint(self, tname='', tstart=None, tend=None):
        """Just a thin method calling the CheckpointLogger checkpoint method.

        Args:
           tname: String of testname associated with this time interval
           tstart: Float in seconds of when tname test started.  Should be based
                off time.time()
           tend: Float in seconds of when tname test ended.  Should be based
                off time.time().  If None, then value computed in the method.
        """
        self._checkpoint_logger.checkpoint(tname, tstart, tend)

    # TODO(seankao): It might be useful to pull this method to CheckpointLogger,
    # to allow checkpoint usage without an explicit MeasurementLogger.
    def calc(self, mtype=None):
        """Calculate average measurement during each of the sub-tests.

        Method performs the following steps:
            1. Signals the thread to stop running.
            2. Calculates mean, max, min, count on the samples for each of the
               measurements.
            3. Stores results to be written later.
            4. Creates keyvals for autotest publishing.

        Args:
            mtype: string of measurement type.  For example:
                   pwr == power
                   temp == temperature
        Returns:
            dict of keyvals suitable for autotest results.
        """
        if not mtype:
            mtype = 'meas'

        t = numpy.array(self.times)
        keyvals = {}
        results  = [('domain', 'mean', 'std', 'duration (s)', 'start ts',
                     'end ts')]
        # TODO(coconutruben): ensure that values is meaningful i.e. go through
        # the Loggers and add a unit attribute to each so that the raw
        # data is readable.
        raw_results = [('domain', 'values (%s)' % mtype)]

        if not self.done:
            self.done = True
        # times 2 the sleep time in order to allow for readings as well.
        self.join(timeout=self.seconds_period * 2)

        if not self._checkpoint_logger.checkpoint_data:
            self._checkpoint_logger.checkpoint()

        for i, domain_readings in enumerate(zip(*self.readings)):
            meas = numpy.array(domain_readings)
            domain = self.domains[i]

            for tname, tlist in self._checkpoint_logger.checkpoint_data.iteritems():
                if tname:
                    prefix = '%s_%s' % (tname, domain)
                else:
                    prefix = domain
                keyvals[prefix+'_duration'] = 0
                # Select all readings taken between tstart and tend
                # timestamps in tlist.
                masks = []
                for tstart, tend in tlist:
                    keyvals[prefix+'_duration'] += tend - tstart
                    # Try block just in case
                    # code.google.com/p/chromium/issues/detail?id=318892
                    # is not fixed.
                    try:
                        masks.append(numpy.logical_and(tstart < t, t < tend))
                    except ValueError, e:
                        logging.debug('Error logging measurements: %s', str(e))
                        logging.debug('timestamps %d %s', t.len, t)
                        logging.debug('timestamp start, end %f %f', tstart, tend)
                        logging.debug('measurements %d %s', meas.len, meas)
                mask = numpy.logical_or.reduce(masks)
                meas_array = meas[mask]

                # If sub-test terminated early, avoid calculating avg, std and
                # min
                if not meas_array.size:
                    continue
                meas_mean = meas_array.mean()
                meas_std = meas_array.std()

                # Results list can be used for pretty printing and saving as csv
                # TODO(seankao): new results format?
                result = (prefix, meas_mean, meas_std)
                for tstart, tend in tlist:
                    result = result + (tend - tstart, tstart, tend)
                results.append(result)
                raw_results.append((prefix,) + tuple(meas_array.tolist()))

                keyvals[prefix + '_' + mtype + '_avg'] = meas_mean
                keyvals[prefix + '_' + mtype + '_cnt'] = meas_array.size
                keyvals[prefix + '_' + mtype + '_max'] = meas_array.max()
                keyvals[prefix + '_' + mtype + '_min'] = meas_array.min()
                keyvals[prefix + '_' + mtype + '_std'] = meas_std
        self._results = results
        self._raw_results = raw_results
        return keyvals


    def save_results(self, resultsdir, fname_prefix=None):
        """Save computed results in a nice tab-separated format.
        This is useful for long manual runs.

        Args:
            resultsdir: String, directory to write results to
            fname_prefix: prefix to use for fname. If provided outfiles
                          will be [fname]_[raw|summary].txt
        """
        if not fname_prefix:
          fname_prefix = 'meas_results_%.0f' % time.time()
        fname = '%s_summary.txt' % fname_prefix
        raw_fname = fname.replace('summary', 'raw')
        for name, data in [(fname, self._results),
                           (raw_fname, self._raw_results)]:
          with open(os.path.join(resultsdir, name), 'wt') as f:
              # First row contains the headers
              f.write('%s\n' % '\t'.join(data[0]))
              for row in data[1:]:
                  # First column name, rest are numbers. See _calc_power()
                  fmt_row = [row[0]] + ['%.2f' % x for x in row[1:]]
                  f.write('%s\n' % '\t'.join(fmt_row))


class CPUStatsLogger(MeasurementLogger):
    """Class to measure CPU Frequency and CPU Idle Stats.

    CPUStatsLogger derived from MeasurementLogger class but overload data
    samplings method because MeasurementLogger assumed that each sampling is
    independent to each other. However, in this case it is not. For example,
    CPU time spent in C0 state is measure by time not spent in all other states.

    CPUStatsLogger also collects the weight average in each time period if the
    underlying AbstractStats support weight average function.

    Private attributes:
       _stats: list of CPU AbstractStats objects to be sampled.
       _refresh_count: number of times refresh() has been called.
       _last_wavg: dict of wavg when refresh() was last called.
    """
    def __init__(self, seconds_period=1.0, checkpoint_logger=None):
        """Initialize a CPUStatsLogger.

        Args:
            seconds_period: float, probing interval in seconds.  Default 1.0
        """
        # We don't use measurements since CPU stats can't measure separately.
        super(CPUStatsLogger, self).__init__([], seconds_period, checkpoint_logger)

        self._stats = get_available_cpu_stats()
        self._stats.append(GPUFreqStats())
        self.domains = []
        for stat in self._stats:
            self.domains.extend([stat.name + '_' + str(state_name)
                                 for state_name in stat.refresh()])
            if stat.weighted_average():
                self.domains.append('wavg_' + stat.name)
        self._refresh_count = 0
        self._last_wavg = collections.defaultdict(int)

    def refresh(self):
        self._refresh_count += 1
        count = self._refresh_count
        ret = []
        for stat in self._stats:
            ret.extend(stat.refresh().values())
            wavg = stat.weighted_average()
            if wavg:
                if stat.incremental:
                    last_wavg = self._last_wavg[stat.name]
                    self._last_wavg[stat.name] = wavg
                    # Calculate weight average in this period using current
                    # total weight average and last total weight average.
                    # The result will lose some precision with higher number of
                    # count but still good enough for 11 significant digits even
                    # if we logged the data every 1 second for a day.
                    ret.append(wavg * count - last_wavg * (count - 1))
                else:
                    ret.append(wavg)
        return ret

    def save_results(self, resultsdir, fname_prefix=None):
        if not fname_prefix:
            fname_prefix = 'cpu_results_%.0f' % time.time()
        super(CPUStatsLogger, self).save_results(resultsdir, fname_prefix)


class PowerLogger(MeasurementLogger):
    """Class to measure power consumption.
    """
    def save_results(self, resultsdir, fname_prefix=None):
        if not fname_prefix:
            fname_prefix = 'power_results_%.0f' % time.time()
        super(PowerLogger, self).save_results(resultsdir, fname_prefix)


    def calc(self, mtype='pwr'):
        return super(PowerLogger, self).calc(mtype)


class TempMeasurement(object):
    """Class to measure temperature.

    Public attributes:
        domain: String name of the temperature domain being measured.  Example is
          'cpu' for cpu temperature

    Private attributes:
        _path: Path to temperature file to read ( in millidegrees Celsius )

    Public methods:
        refresh: Performs any temperature sampling and calculation and returns
            temperature as float in degrees Celsius.
    """
    def __init__(self, domain, path):
        """Constructor."""
        self.domain = domain
        self._path = path


    def refresh(self):
        """Performs temperature

        Returns:
            float, temperature in degrees Celsius
        """
        return int(utils.read_one_line(self._path)) / 1000.


class BatteryTempMeasurement(TempMeasurement):
    """Class to measure battery temperature."""
    def __init__(self):
        super(BatteryTempMeasurement, self).__init__('battery', 'battery_temp')


    def refresh(self):
        """Perform battery temperature reading.

        Returns:
            float, temperature in degrees Celsius.
        """
        result = utils.run(self._path, timeout=5, ignore_status=True)
        return float(result.stdout)


def has_battery_temp():
    """Determine if DUT can provide battery temperature.

    Returns:
        Boolean, True if battery temperature available.  False otherwise.
    """
    if not power_utils.has_battery():
        return False

    btemp = BatteryTempMeasurement()
    try:
        btemp.refresh()
    except ValueError:
        return False

    return True


class TempLogger(MeasurementLogger):
    """A thread that logs temperature readings in millidegrees Celsius."""
    def __init__(self, measurements, seconds_period=30.0, checkpoint_logger=None):
        if not measurements:
            domains = set()
            measurements = []
            tstats = ThermalStatHwmon()
            for kname in tstats.fields:
                match = re.match(r'(\S+)_temp(\d+)_input', kname)
                if not match:
                    continue
                domain = match.group(1) + '-t' + match.group(2)
                fpath = tstats.fields[kname][0]
                new_meas = TempMeasurement(domain, fpath)
                measurements.append(new_meas)
                domains.add(domain)

            if has_battery_temp():
                measurements.append(BatteryTempMeasurement())

            sysfs_paths = '/sys/class/thermal/thermal_zone*'
            paths = glob.glob(sysfs_paths)
            for path in paths:
                domain_path = os.path.join(path, 'type')
                temp_path = os.path.join(path, 'temp')

                domain = utils.read_one_line(domain_path)

                # Skip when thermal_zone and hwmon have same domain.
                if domain in domains:
                    continue

                domain = domain.replace(' ', '_')
                new_meas = TempMeasurement(domain, temp_path)
                measurements.append(new_meas)

        super(TempLogger, self).__init__(measurements, seconds_period, checkpoint_logger)


    def save_results(self, resultsdir, fname_prefix=None):
        if not fname_prefix:
            fname_prefix = 'temp_results_%.0f' % time.time()
        super(TempLogger, self).save_results(resultsdir, fname_prefix)


    def calc(self, mtype='temp'):
        return super(TempLogger, self).calc(mtype)


class VideoFpsLogger(MeasurementLogger):
    """Class to measure Video FPS."""

    def __init__(self, tab, seconds_period=1.0, checkpoint_logger=None):
        """Initialize a VideoFpsLogger.

        Args:
            tab: Chrome tab object
        """
        super(VideoFpsLogger, self).__init__([], seconds_period,
                                             checkpoint_logger)
        self._tab = tab
        names = self._tab.EvaluateJavaScript(
            'Array.from(document.getElementsByTagName("video")).map(v => v.id)')
        self.domains =  [n or 'video_' + str(i) for i, n in enumerate(names)]
        self._last = [0] * len(names)
        self.refresh()

    def refresh(self):
        current = self._tab.EvaluateJavaScript(
            'Array.from(document.getElementsByTagName("video")).map('
            'v => v.webkitDecodedFrameCount)')
        fps = [(b - a) / self.seconds_period
               for a, b in zip(self._last , current)]
        self._last = current
        return fps

    def save_results(self, resultsdir, fname_prefix=None):
        if not fname_prefix:
            fname_prefix = 'video_fps_results_%.0f' % time.time()
        super(VideoFpsLogger, self).save_results(resultsdir, fname_prefix)

    def calc(self, mtype='fps'):
        return super(VideoFpsLogger, self).calc(mtype)


class DiskStateLogger(threading.Thread):
    """Records the time percentages the disk stays in its different power modes.

    Example code snippet:
        mylogger = power_status.DiskStateLogger()
        mylogger.start()
        result = mylogger.result()

    Public methods:
        start: Launches the thread and starts measurements.
        result: Stops the thread if it's still running and returns measurements.
        get_error: Returns the exception in _error if it exists.

    Private functions:
        _get_disk_state: Returns the disk's current ATA power mode as a string.

    Private attributes:
        _seconds_period: Disk polling interval in seconds.
        _stats: Dict that maps disk states to seconds spent in them.
        _running: Flag that is True as long as the logger should keep running.
        _time: Timestamp of last disk state reading.
        _device_path: The file system path of the disk's device node.
        _error: Contains a TestError exception if an unexpected error occured
    """
    def __init__(self, seconds_period = 5.0, device_path = None):
        """Initializes a logger.

        Args:
            seconds_period: Disk polling interval in seconds. Default 5.0
            device_path: The path of the disk's device node. Default '/dev/sda'
        """
        threading.Thread.__init__(self)
        self._seconds_period = seconds_period
        self._device_path = device_path
        self._stats = {}
        self._running = False
        self._error = None

        result = utils.system_output('rootdev -s')
        # TODO(tbroch) Won't work for emmc storage and will throw this error in
        # keyvals : 'ioctl(SG_IO) error: [Errno 22] Invalid argument'
        # Lets implement something complimentary for emmc
        if not device_path:
            self._device_path = \
                re.sub('(sd[a-z]|mmcblk[0-9]+)p?[0-9]+', '\\1', result)
        logging.debug("device_path = %s", self._device_path)


    def start(self):
        logging.debug("inside DiskStateLogger.start")
        if os.path.exists(self._device_path):
            logging.debug("DiskStateLogger started")
            super(DiskStateLogger, self).start()


    def _get_disk_state(self):
        """Checks the disk's power mode and returns it as a string.

        This uses the SG_IO ioctl to issue a raw SCSI command data block with
        the ATA-PASS-THROUGH command that allows SCSI-to-ATA translation (see
        T10 document 04-262r8). The ATA command issued is CHECKPOWERMODE1,
        which returns the device's current power mode.
        """

        def _addressof(obj):
            """Shortcut to return the memory address of an object as integer."""
            return ctypes.cast(obj, ctypes.c_void_p).value

        scsi_cdb = struct.pack("12B", # SCSI command data block (uint8[12])
                               0xa1, # SCSI opcode: ATA-PASS-THROUGH
                               3 << 1, # protocol: Non-data
                               1 << 5, # flags: CK_COND
                               0, # features
                               0, # sector count
                               0, 0, 0, # LBA
                               1 << 6, # flags: ATA-USING-LBA
                               0xe5, # ATA opcode: CHECKPOWERMODE1
                               0, # reserved
                               0, # control (no idea what this is...)
                              )
        scsi_sense = (ctypes.c_ubyte * 32)() # SCSI sense buffer (uint8[32])
        sgio_header = struct.pack("iiBBHIPPPIIiPBBBBHHiII", # see <scsi/sg.h>
                                  83, # Interface ID magic number (int32)
                                  -1, # data transfer direction: none (int32)
                                  12, # SCSI command data block length (uint8)
                                  32, # SCSI sense data block length (uint8)
                                  0, # iovec_count (not applicable?) (uint16)
                                  0, # data transfer length (uint32)
                                  0, # data block pointer
                                  _addressof(scsi_cdb), # SCSI CDB pointer
                                  _addressof(scsi_sense), # sense buffer pointer
                                  500, # timeout in milliseconds (uint32)
                                  0, # flags (uint32)
                                  0, # pack ID (unused) (int32)
                                  0, # user data pointer (unused)
                                  0, 0, 0, 0, 0, 0, 0, 0, 0, # output params
                                 )
        try:
            with open(self._device_path, 'r') as dev:
                result = fcntl.ioctl(dev, 0x2285, sgio_header)
        except IOError, e:
            raise error.TestError('ioctl(SG_IO) error: %s' % str(e))
        _, _, _, _, status, host_status, driver_status = \
            struct.unpack("4x4xxx2x4xPPP4x4x4xPBxxxHH4x4x4x", result)
        if status != 0x2: # status: CHECK_CONDITION
            raise error.TestError('SG_IO status: %d' % status)
        if host_status != 0:
            raise error.TestError('SG_IO host status: %d' % host_status)
        if driver_status != 0x8: # driver status: SENSE
            raise error.TestError('SG_IO driver status: %d' % driver_status)

        if scsi_sense[0] != 0x72: # resp. code: current error, descriptor format
            raise error.TestError('SENSE response code: %d' % scsi_sense[0])
        if scsi_sense[1] != 0: # sense key: No Sense
            raise error.TestError('SENSE key: %d' % scsi_sense[1])
        if scsi_sense[7] < 14: # additional length (ATA status is 14 - 1 bytes)
            raise error.TestError('ADD. SENSE too short: %d' % scsi_sense[7])
        if scsi_sense[8] != 0x9: # additional descriptor type: ATA Return Status
            raise error.TestError('SENSE descriptor type: %d' % scsi_sense[8])
        if scsi_sense[11] != 0: # errors: none
            raise error.TestError('ATA error code: %d' % scsi_sense[11])

        if scsi_sense[13] == 0x00:
            return 'standby'
        if scsi_sense[13] == 0x80:
            return 'idle'
        if scsi_sense[13] == 0xff:
            return 'active'
        return 'unknown(%d)' % scsi_sense[13]


    def run(self):
        """The Thread's run method."""
        try:
            self._time = time.time()
            self._running = True
            while(self._running):
                time.sleep(self._seconds_period)
                state = self._get_disk_state()
                new_time = time.time()
                if state in self._stats:
                    self._stats[state] += new_time - self._time
                else:
                    self._stats[state] = new_time - self._time
                self._time = new_time
        except error.TestError, e:
            self._error = e
            self._running = False


    def result(self):
        """Stop the logger and return dict with result percentages."""
        if (self._running):
            self._running = False
            self.join(self._seconds_period * 2)
        return AbstractStats.to_percent(self._stats)


    def get_error(self):
        """Returns the _error exception... please only call after result()."""
        return self._error

def parse_pmc_s0ix_residency_info():
    """
    Parses S0ix residency for PMC based Intel systems
    (skylake/kabylake/apollolake), the debugfs paths might be
    different from platform to platform, yet the format is
    unified in microseconds.

    @returns residency in seconds.
    @raises error.TestNAError if the debugfs file not found.
    """
    info_path = None
    for node in ['/sys/kernel/debug/pmc_core/slp_s0_residency_usec',
                 '/sys/kernel/debug/telemetry/s0ix_residency_usec']:
        if os.path.exists(node):
            info_path = node
            break
    if not info_path:
        raise error.TestNAError('S0ix residency file not found')
    return float(utils.read_one_line(info_path)) * 1e-6


class S0ixResidencyStats(object):
    """
    Measures the S0ix residency of a given board over time.
    """
    def __init__(self):
        self._initial_residency = parse_pmc_s0ix_residency_info()

    def get_accumulated_residency_secs(self):
        """
        @returns S0ix Residency since the class has been initialized.
        """
        return parse_pmc_s0ix_residency_info() - self._initial_residency


class DMCFirmwareStats(object):
    """
    Collect DMC firmware stats of Intel based system (SKL+), (APL+).
    """
    # Intel CPUs only transition to DC6 from DC5. https://git.io/vppcG
    DC6_ENTRY_KEY = 'DC5 -> DC6 count'

    def __init__(self):
        self._initial_stat = DMCFirmwareStats._parse_dmc_info()

    def check_fw_loaded(self):
        """Check that DMC firmware is loaded

        @returns boolean of DMC firmware loaded.
        """
        return self._initial_stat['fw loaded']

    def is_dc6_supported(self):
        """Check that DMC support DC6 state."""
        return self.DC6_ENTRY_KEY in self._initial_stat

    def get_accumulated_dc6_entry(self):
        """Check number of DC6 state entry since the class has been initialized.

        @returns number of DC6 state entry.
        """
        if not self.is_dc6_supported():
            return 0

        key = self.DC6_ENTRY_KEY
        current_stat = DMCFirmwareStats._parse_dmc_info()
        return current_stat[key] - self._initial_stat[key]

    @staticmethod
    def _parse_dmc_info():
        """
        Parses DMC firmware info for Intel based systems.

        @returns dictionary of dmc_fw info
        @raises error.TestFail if the debugfs file not found.
        """
        path = '/sys/kernel/debug/dri/0/i915_dmc_info'
        if not os.path.exists(path):
            raise error.TestFail('DMC info file not found.')

        with open(path, 'r') as f:
            lines = [line.strip() for line in f.readlines()]

        # For pre 4.16 kernel. https://git.io/vhThb
        if lines[0] == 'not supported':
            raise error.TestFail('DMC not supported.')

        ret = dict()
        for line in lines:
            key, val = line.rsplit(': ', 1)

            if key == 'fw loaded':
                val = val == 'yes'
            elif re.match(r'DC\d -> DC\d count', key):
                val = int(val)
            ret[key] = val
        return ret


class RC6ResidencyStats(object):
    """
    Collect RC6 residency stats of Intel based system.
    """
    def __init__(self):
        self._rc6_enable_checked = False
        self._previous_stat = self._parse_rc6_residency_info()
        self._accumulated_stat = 0

        # Setup max RC6 residency count for modern chips. The table order
        # is in small/big-core first, follows by the uarch name. We don't
        # need to deal with the wraparound for devices with v4.17+ kernel
        # which has the commit 817cc0791823 ("drm/i915: Handle RC6 counter wrap").
        cpu_uarch = utils.get_intel_cpu_uarch()
        self._max_counter = {
          # Small-core w/ GEN9 LP graphics
          'Airmont':      3579125,
          'Goldmont':     3579125,
          # Big-core
          'Broadwell':    5497558,
          'Haswell':      5497558,
          'Kaby Lake':    5497558,
          'Skylake':      5497558,
        }.get(cpu_uarch, None)

    def get_accumulated_residency_msecs(self):
        """Check number of RC6 state entry since the class has been initialized.

        @returns int of RC6 residency in milliseconds since instantiation.
        """
        current_stat = self._parse_rc6_residency_info()

        # The problem here is that we cannot assume the rc6_residency_ms is
        # monotonically increasing by current kernel i915 implementation.
        #
        # Considering different hardware has different wraparound period,
        # this is a mitigation plan to deal with different wraparound period
        # on various platforms, in order to make the test platform agnostic.
        #
        # This scarifes the accuracy of RC6 residency a bit, up on the calling
        # period.
        #
        # Reference: Bug 94852 - [SKL] rc6_residency_ms unreliable
        # (https://bugs.freedesktop.org/show_bug.cgi?id=94852)
        #
        # However for modern processors with a known overflow count, apply
        # constant of RC6 max counter to improve accuracy.
        #
        # Note that the max counter is bound for sysfs overflow, while the
        # accumulated residency here is the diff against the first reading.
        if current_stat < self._previous_stat:
          if self._max_counter is None:
            logging.warning('GPU: Detect rc6_residency_ms wraparound')
            self._accumulated_stat += current_stat
          else:
            self._accumulated_stat += current_stat + (self._max_counter - self._previous_stat)
        else:
          self._accumulated_stat += current_stat - self._previous_stat

        self._previous_stat = current_stat
        return self._accumulated_stat

    def _is_rc6_enable(self):
        """
        Verified that RC6 is enable.

        @returns Boolean of RC6 enable status.
        @raises error.TestFail if the sysfs file not found.
        """
        path = '/sys/class/drm/card0/power/rc6_enable'
        if not os.path.exists(path):
            raise error.TestFail('RC6 enable file not found.')

        return (int(utils.read_one_line(path)) & 0x1) == 0x1

    def _parse_rc6_residency_info(self):
        """
        Parses RC6 residency info for Intel based systems.

        @returns int of RC6 residency in millisec since boot.
        @raises error.TestFail if the sysfs file not found or RC6 not enabled.
        """
        if not self._rc6_enable_checked:
            if not self._is_rc6_enable():
                raise error.TestFail('RC6 is not enabled.')
            self._rc6_enable_checked = True

        path = '/sys/class/drm/card0/power/rc6_residency_ms'
        if not os.path.exists(path):
            raise error.TestFail('RC6 residency file not found.')

        return int(utils.read_one_line(path))


class PCHPowergatingStats(object):
    """
    Collect PCH powergating status of intel based system.
    """
    PMC_CORE_PATH = '/sys/kernel/debug/pmc_core/pch_ip_power_gating_status'
    TELEMETRY_PATH = '/sys/kernel/debug/telemetry/soc_states'

    def __init__(self):
        self._stat = {}

    def check_s0ix_requirement(self):
        """
        Check PCH powergating status with S0ix requirement.

        @returns list of PCH IP block name that need to be powergated for low
                 power consumption S0ix, empty list if none.
        """
        # PCH IP block that is on for S0ix. Ignore these IP block.
        S0IX_WHITELIST = set([
                'PMC', 'OPI-DMI', 'SPI / eSPI', 'XHCI', 'xHCI', 'FUSE', 'Fuse',
                'PCIE0', 'NPKVRC', 'NPKVNN', 'NPK_VNN', 'PSF1', 'PSF2', 'PSF3',
                'PSF4', 'SBR0', 'SBR1', 'SBR2', 'SBR4', 'SBR5', 'SBR6', 'SBR7'])

        # PCH IP block that is on/off for S0ix depend on features enabled.
        # Add log when these IPs state are on.
        S0IX_WARNLIST = set([
                'HDA-PGD0', 'HDA-PGD1', 'HDA-PGD2', 'HDA-PGD3', 'LPSS',
                'AVSPGD1', 'AVSPGD4'])

        # CNV device has 0x31dc as devid .
        if len(utils.system_output('lspci -d :31dc')) > 0:
            S0IX_WHITELIST.add('CNV')

        # HrP2 device has 0x02f0 as devid.
        if len(utils.system_output('lspci -d :02f0')) > 0:
            S0IX_WHITELIST.update(['CNVI', 'NPK_AON'])

        on_ip = set(ip['name'] for ip in self._stat if ip['state'])
        on_ip -= S0IX_WHITELIST

        if on_ip:
            on_ip_in_warn_list = on_ip & S0IX_WARNLIST
            if on_ip_in_warn_list:
                logging.warn('Found PCH IP that may be able to powergate: %s',
                             ', '.join(on_ip_in_warn_list))
            on_ip -= S0IX_WARNLIST

        if on_ip:
            logging.error('Found PCH IP that need to powergate: %s',
                          ', '.join(on_ip))
            return False
        return True

    def read_pch_powergating_info(self, sleep_seconds=1):
        """
        Read PCH powergating status info for Intel based systems.

        Intel currently shows powergating status in 2 different place in debugfs
        depend on which CPU platform.

        @param sleep_seconds: sleep time to make DUT idle before read the data.

        @raises error.TestFail if the debugfs file not found or parsing error.
        """
        if os.path.exists(self.PMC_CORE_PATH):
            logging.info('Use PCH powergating info at %s', self.PMC_CORE_PATH)
            time.sleep(sleep_seconds)
            self._read_pcm_core_powergating_info()
            return

        if os.path.exists(self.TELEMETRY_PATH):
            logging.info('Use PCH powergating info at %s', self.TELEMETRY_PATH)
            time.sleep(sleep_seconds)
            self._read_telemetry_powergating_info()
            return

        raise error.TestFail('PCH powergating info file not found.')

    def _read_pcm_core_powergating_info(self):
        """
        read_pch_powergating_info() for Intel Core KBL+

        @raises error.TestFail if parsing error.
        """
        with open(self.PMC_CORE_PATH, 'r') as f:
            lines = [line.strip() for line in f.readlines()]

        # Example pattern to match:
        # PCH IP: 0  - PMC                                State: On
        # PCH IP: 1  - SATA                               State: Off
        pattern = r'PCH IP:\s+(?P<id>\d+)\s+' \
                  r'- (?P<name>.*\w)\s+'      \
                  r'State: (?P<state>Off|On)'
        matcher = re.compile(pattern)
        ret = []
        for i, line in enumerate(lines):
            match = matcher.match(line)
            if not match:
                raise error.TestFail('Can not parse PCH powergating info: ',
                                     line)

            index = int(match.group('id'))
            if i != index:
                raise error.TestFail('Wrong index for PCH powergating info: ',
                                     line)

            name = match.group('name')
            state = match.group('state') == 'On'

            ret.append({'name': name, 'state': state})
        self._stat = ret

    def _read_telemetry_powergating_info(self):
        """
        read_pch_powergating_info() for Intel Atom APL+

        @raises error.TestFail if parsing error.
        """
        with open(self.TELEMETRY_PATH, 'r') as f:
            raw_str = f.read()

        # Example pattern to match:
        # --------------------------------------
        # South Complex PowerGate Status
        # --------------------------------------
        # Device           PG
        # LPSS             1
        # SPI              1
        # FUSE             0
        #
        # ---------------------------------------
        trimed_pattern = r'.*South Complex PowerGate Status\n'    \
                         r'-+\n'                                  \
                         r'Device\s+PG\n'                         \
                         r'(?P<trimmed_section>(\w+\s+[0|1]\n)+)' \
                         r'\n-+\n.*'
        trimed_match = re.match(trimed_pattern, raw_str, re.DOTALL)
        if not trimed_match:
            raise error.TestFail('Can not parse PCH powergating info: ',
                                 raw_str)
        trimmed_str = trimed_match.group('trimmed_section').strip()
        lines = [line.strip() for line in trimmed_str.split('\n')]

        matcher = re.compile(r'(?P<name>\w+)\s+(?P<state>[0|1])')
        ret = []
        for line in lines:
            match = matcher.match(line)
            if not match:
                raise error.TestFail('Can not parse PCH powergating info: %s',
                                     line)

            name = match.group('name')
            state = match.group('state') == '0' # 0 means on and 1 means off

            ret.append({'name': name, 'state': state})
        self._stat = ret

def has_rc6_support():
    """
    Helper to examine that RC6 is enabled with residency counter.

    @returns Boolean of RC6 support status.
    """
    enable_path = '/sys/class/drm/card0/power/rc6_enable'
    residency_path = '/sys/class/drm/card0/power/rc6_residency_ms'

    has_rc6_enabled = os.path.exists(enable_path)
    has_rc6_residency = False
    rc6_enable_mask = 0

    if has_rc6_enabled:
        # TODO (harry.pan): Some old chip has RC6P and RC6PP
        # in the bits[1:2]; in case of that, ideally these time
        # slice will fall into RC0, fix it up if required.
        rc6_enable_mask = int(utils.read_one_line(enable_path))
        has_rc6_enabled &= (rc6_enable_mask) & 0x1 == 0x1
        has_rc6_residency = os.path.exists(residency_path)

    logging.debug("GPU: RC6 residency support: %s, mask: 0x%x",
                  {True: "yes", False: "no"} [has_rc6_enabled and has_rc6_residency],
                  rc6_enable_mask)

    return (has_rc6_enabled and has_rc6_residency)

class GPURC6Stats(AbstractStats):
    """
    GPU RC6 statistics to give ratio of RC6 and RC0 residency

    Protected Attributes:
      _rc6: object of RC6ResidencyStats
    """
    def __init__(self):
        self._rc6 = RC6ResidencyStats()
        super(GPURC6Stats, self).__init__(name='gpuidle')

    def _read_stats(self):
        total = int(time.time() * 1000)
        msecs = self._rc6.get_accumulated_residency_msecs()
        stats = collections.defaultdict(int)
        stats['RC6'] += msecs
        stats['RC0'] += total - msecs
        logging.debug("GPU: RC6 residency: %d ms", msecs)
        return stats
