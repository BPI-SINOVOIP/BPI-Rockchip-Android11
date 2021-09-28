# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper class for power measurement with telemetry devices."""

import collections
import datetime
from distutils import sysconfig
import json
import logging
import numpy
import os
import re
import string
import threading
import time

import powerlog

from servo import measure_power

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.power import power_status
from autotest_lib.client.cros.power import power_telemetry_utils as utils
from autotest_lib.server.cros.power import power_dashboard


# If a sample has over 10% NaN values, the data might be very unreliable if
# interpolation is applied.
ACCEPTABLE_NAN_RATIO = 0.1

# If a sample has more than these NaN values in sequence, the data is also not
# reliable.
MAX_CONSECUTIVE_NAN_READINGS = 5

# If for over a second no values can be read, the data is also not reliable.
MAX_NAN_GAP_S = 1

# Dictionary to make passing the default arguments for loggers to the NaN
# interpolation utility easy.
INTERPOLATION_ARGS = {'max_nan_ratio': ACCEPTABLE_NAN_RATIO,
                      'max_sample_gap': MAX_CONSECUTIVE_NAN_READINGS,
                      'max_sample_time_gap': MAX_NAN_GAP_S}

def ts_processing(ts_str):
    """Parse autotest log timestamp into local time seconds since epoch.

    @param ts_str: a timestamp string from client.DEBUG file in local time zone.
    @return seconds since epoch, inserting the current year because ts_str does
            not include year. This introduces error if client side test is
            running across the turn of the year.
    """
    ts = datetime.datetime.strptime(ts_str, '%m/%d %H:%M:%S.%f ')
    # TODO(mqg): fix the wrong year at turn of the year.
    ts = ts.replace(year=datetime.datetime.today().year)
    return time.mktime(ts.timetuple()) + ts.microsecond / 1e6


class PowerTelemetryLogger(object):
    """A helper class for power autotests requiring telemetry devices.

    Telemetry: external pieces of hardware which help with measuring power
    data on the Chrome device. This is not to be confused with library
    telemetry.core, which is a required library / dependency for autotests
    involving Chrome and / or ARC. Examples of power telemetry devices include
    Servo and Sweetberry.

    This logger class detects telemetry devices connected to the DUT. It will
    then start and stop the measurement, trim the excessive power telemetry
    device data and report the data back to the workstation and the dashboard
    """

    DASHBOARD_UPLOAD_URL = 'http://chrome-power.appspot.com'
    DEFAULT_START = r'starting test\(run_once\(\)\), test details follow'
    DEFAULT_END = r'The test has completed successfully'

    def __init__(self, config, resultsdir, host):
        """Init PowerTelemetryLogger.

        @param config: the args argument from test_that in a dict. Settings for
                       power telemetry devices.
                       required data: {'test': 'test_TestName.tag'}
        @param resultsdir: path to directory where current autotest results are
                           stored, e.g. /tmp/test_that_results/
                           results-1-test_TestName.tag/test_TestName.tag/
                           results/
        @param host: CrosHost object representing the DUT.
        """
        logging.debug('%s initialize.', self.__class__.__name__)
        self._resultsdir = resultsdir
        self._host = host
        self._tagged_testname = config['test']
        self._pdash_note = config.get('pdash_note', '')

    def start_measurement(self):
        """Start power telemetry devices."""
        self._start_measurement()
        logging.info('%s starts.', self.__class__.__name__)
        self._start_ts = time.time()

    def _start_measurement(self):
        """Start power telemetry devices."""
        raise NotImplementedError('Subclasses must implement '
                '_start_measurement.')

    def end_measurement(self, client_test_dir):
        """End power telemetry devices.

        End power measurement with telemetry devices, get the power telemetry
        device data, trim the data recorded outside of actual testing, and
        upload statistics to dashboard.

        @param client_test_dir: directory of the client side test.
        """
        self._end_measurement()
        logging.info('%s finishes.', self.__class__.__name__)
        start_ts, end_ts = self._get_client_test_ts(client_test_dir)
        loggers = self._load_and_trim_data(start_ts, end_ts)
        # Call export after trimming to only export trimmed data.
        self._export_data_locally(client_test_dir)
        checkpoint_logger = self._get_client_test_checkpoint_logger(
                client_test_dir)
        self._upload_data(loggers, checkpoint_logger)

    def _end_measurement(self):
        """End power telemetry devices."""
        raise NotImplementedError('Subclasses must implement _end_measurement.')

    def _export_data_locally(self, client_test_dir):
        """Slot for the logger to export measurements locally."""
        raise NotImplementedError('Subclasses must implement '
                                  '_export_data_locally.')

    def _get_client_test_ts(self, client_test_dir):
        """Determine the start and end timestamp for the telemetry data.

        Power telemetry devices will run through the entire autotest, including
        the overhead time, but we only need the measurements of actual testing,
        so parse logs from client side test to determine the start and end
        timestamp for the telemetry data.

        @param client_test_dir: directory of the client side test.
        @return (start_ts, end_ts)
                start_ts: the start timestamp of the client side test in seconds
                          since epoch or None.
                end_ts: the end timestamp of the client side test in seconds
                        since epoch or None.
        """
        if not os.path.isdir(client_test_dir):
            logging.error('Cannot find client side test dir %s, no need to '
                          'trim power telemetry measurements.', client_test_dir)
            return (None, None)

        # Use timestamp in client side test power_log.json as start & end
        # timestamp.
        power_log_path = os.path.join(client_test_dir, 'results',
                                      'power_log.json')
        start_ts, end_ts = self._get_power_log_ts(power_log_path)
        if start_ts and end_ts:
            self._start_ts = start_ts
            return (start_ts, end_ts)

        # Parse timestamp in client side test debug log and use as start & end
        # timestamp.
        client_test_name = os.path.basename(client_test_dir)
        debug_file_path = os.path.join(client_test_dir, 'debug',
                                       '%s.DEBUG' % client_test_name)
        start_ts, end_ts = self._get_debug_log_ts(debug_file_path)
        if start_ts:
            self._start_ts = start_ts
        return (start_ts, end_ts)

    def _get_debug_log_ts(self, debug_file_path):
        """Parse client side test start and end timestamp from debug log.

        @param debug_file_path: path to client side test debug log.
        @return (start_ts, end_ts)
                start_ts: the start timestamp of the client side test in seconds
                          since epoch or None.
                end_ts: the end timestamp of the client side test in seconds
                        since epoch or None.
        """
        default_test_events = collections.defaultdict(dict)
        custom_test_events = collections.defaultdict(dict)
        default_test_events['start']['str'] = self.DEFAULT_START
        default_test_events['end']['str'] = self.DEFAULT_END
        custom_test_events['start']['str'] = utils.CUSTOM_START
        custom_test_events['end']['str'] = utils.CUSTOM_END
        for event in default_test_events:
            default_test_events[event]['re'] = re.compile(r'([\d\s\./:]+).+' +
                    default_test_events[event]['str'])
            default_test_events[event]['match'] = False
        for event in custom_test_events:
            custom_test_events[event]['re'] = re.compile(r'.*' +
                    custom_test_events[event]['str'] + r'\s+([\d\.]+)')
        events_ts = {
            'start': None,
            'end': None,
        }

        try:
            with open(debug_file_path, 'r') as debug_log:

                for line in debug_log:
                    for event in default_test_events:
                        match = default_test_events[event]['re'].match(line)
                        if match:
                            default_test_events[event]['ts'] = \
                                    ts_processing(match.group(1))
                            default_test_events[event]['match'] = True
                    for event in custom_test_events:
                        match = custom_test_events[event]['re'].match(line)
                        if match:
                            custom_test_events[event]['ts'] = \
                                    float(match.group(1))

            for event in default_test_events:
                if not default_test_events[event]['match']:
                    raise error.TestWarn('Cannot find %s timestamp in client '
                                         'side test debug log.')

            for event in events_ts:
                events_ts[event] = default_test_events[event].get(
                        'ts', events_ts[event])
                events_ts[event] = custom_test_events[event].get(
                        'ts', events_ts[event])

            return (events_ts['start'], events_ts['end'])

        except Exception as exc:
            logging.warning('Client side test debug log %s does not contain '
                            'valid start and end timestamp, see exception: %s',
                            debug_file_path, exc)
            return (None, None)

    def _get_power_log_ts(self, power_log_path):
        """Parse client side test start and end timestamp from power_log.json.

        @param power_log_path: path to client side test power_log.json.
        @return (start_ts, end_ts)
                start_ts: the start timestamp of the client side test in seconds
                          since epoch or None.
                end_ts: the end timestamp of the client side test in seconds
                        since epoch or None.
        """
        try:
            with open(power_log_path, 'r') as power_log:
                power_log_str = power_log.read()
                json_decoder = json.JSONDecoder()
                power_log_obj = []

                idx = 0
                start_ts = list()
                end_ts = list()
                while idx < len(power_log_str):
                    log_entry, idx = json_decoder.raw_decode(power_log_str, idx)
                    start_ts.append(log_entry['timestamp'])
                    end_ts.append(log_entry['timestamp'] +
                                  log_entry['power']['sample_duration'] *
                                  log_entry['power']['sample_count'])

                return (min(start_ts), max(end_ts))
        except Exception as exc:
            logging.warning('Client side test power_log %s does not contain '
                            'valid start and end timestamp, see exception: %s',
                            power_log_path, exc)
            return (None, None)

    def _load_and_trim_data(self, start_ts, end_ts):
        """Load data and trim data.

        Load and format data recorded by power telemetry devices. Trim data if
        necessary.

        @param start_ts: start timestamp in seconds since epoch, None if no
                         need to trim data.
        @param end_ts: end timestamp in seconds since epoch, None if no need to
                       trim data.
        @return a list of loggers, where each logger contains raw power data and
                statistics.

        logger format:
        {
            'sample_count' : 60,
            'sample_duration' : 60,
            'data' : {
                'domain_1' : [ 111.11, 123.45 , ... , 99.99 ],
                ...
                'domain_n' : [ 3999.99, 4242.42, ... , 4567.89 ]
            },
            'average' : {
                'domain_1' : 100.00,
                ...
                'domain_n' : 4300.00
            },
            'unit' : {
                'domain_1' : 'milliwatt',
                ...
                'domain_n' : 'milliwatt'
            },
            'type' : {
                'domain_1' : 'servod',
                ...
                'domain_n' : 'servod'
            },
        }
        """
        raise NotImplementedError('Subclasses must implement '
                '_load_and_trim_data and return a list of loggers.')

    def _get_client_test_checkpoint_logger(self, client_test_dir):
        client_test_resultsdir = os.path.join(client_test_dir, 'results')
        checkpoint_logger = power_status.get_checkpoint_logger_from_file(
                resultsdir=client_test_resultsdir)
        return checkpoint_logger

    def _upload_data(self, loggers, checkpoint_logger):
        """Upload the data to dashboard.

        @param loggers: a list of loggers, where each logger contains raw power
                        data and statistics.
        """

        for logger in loggers:
            pdash = power_dashboard.PowerTelemetryLoggerDashboard(
                    logger=logger, testname=self._tagged_testname,
                    host=self._host, start_ts=self._start_ts,
                    checkpoint_logger=checkpoint_logger,
                    resultsdir=self._resultsdir,
                    uploadurl=self.DASHBOARD_UPLOAD_URL, note=self._pdash_note)
            pdash.upload()


class ServodTelemetryLogger(PowerTelemetryLogger):
    """This logger class measures power by querying a servod instance.
    """

    DEFAULT_INA_RATE = 20.0
    DEFAULT_VBAT_RATE = 60.0

    def __init__(self, config, resultsdir, host):
        """Init ServodTelemetryLogger.

        @param config: the args argument from test_that in a dict. Settings for
                       power telemetry devices.
                       required data:
                       {'test': 'test_TestName.tag',
                        'servo_host': host of servod instance,
                        'servo_port: port that the servod instance is on}
        @param resultsdir: path to directory where current autotest results are
                           stored, e.g. /tmp/test_that_results/
                           results-1-test_TestName.tag/test_TestName.tag/
                           results/
        @param host: CrosHost object representing the DUT.
        """
        super(ServodTelemetryLogger, self).__init__(config, resultsdir, host)

        self._servo_host = host.servo._servo_host.hostname
        self._servo_port = host.servo._servo_host.servo_port
        self._ina_rate = float(config.get('ina_rate', self.DEFAULT_INA_RATE))
        self._vbat_rate = float(config.get('vbat_rate', self.DEFAULT_VBAT_RATE))
        self._pm = measure_power.PowerMeasurement(host=self._servo_host,
                                                  port=self._servo_port,
                                                  ina_rate=self._ina_rate,
                                                  vbat_rate=self._vbat_rate)

    def _start_measurement(self):
        """Start power measurement by querying servod."""
        setup_done = self._pm.MeasurePower()
        # Block the main thread until setup is done and measurement has started.
        setup_done.wait()

    def _end_measurement(self):
        """End querying servod."""
        self._pm.FinishMeasurement()

    def _export_data_locally(self, client_test_dir):
        """Output formatted text summaries locally."""
        # At this point the PowerMeasurement unit has been processed. Dump its
        # formatted summaries into the results directory.
        power_summaries_dir = os.path.join(self._resultsdir, 'power_summaries')
        self._pm.SaveSummary(outdir=power_summaries_dir)

    def _load_and_trim_data(self, start_ts, end_ts):
        """Load data and trim data.

        Load and format data recorded by servod. Trim data if necessary.
        """
        self._pm.ProcessMeasurement(start_ts, end_ts)

        summary = self._pm.GetSummary()
        raw_data = self._pm.GetRawData()

        loggers = list()

        # Domains in summary/raw_data that carry no power-data.
        metadata_domains = ['Sample_msecs', 'time', 'timeline']

        for source in summary:
            tl = raw_data[source]['timeline']
            samples = len(tl)
            data = {
                k[:-3] if k.endswith('_mw') else k: v
                for k, v in raw_data[source].iteritems()
                if k not in metadata_domains
            }

            # Add the timeline of this measurement to the interpolation
            # arguments. This is to detect and reject large measurement gaps.
            # See above for details or in power_telemetry_utils.
            INTERPOLATION_ARGS['timeline'] = tl

            try:
                # Smoothen out data to remove any NaN values by interpolating
                # the missing values. If too many values are NaN, or too many
                # values are NaN consecutively, fail the test.
                # Here r stands for rail and d stands for data.
                data = {r: utils.interpolate_missing_data(d,
                                                          **INTERPOLATION_ARGS)
                        for r, d in data.iteritems()}
            except utils.TelemetryUtilsError as e:
                raise error.TestFail('Issue at source %s: %s' % (source,
                                                                 str(e)))

            ave = {
                k[:-3] if k.endswith('_mw') else k: v['mean']
                for k, v in summary[source].iteritems()
                if k not in metadata_domains
            }
            if samples > 1:
                # Having more than one sample allows the code to properly set a
                # sample duration.
                sample_duration = (tl[-1] - tl[0]) / (samples - 1)
            else:
                # In thise case, it seems that there is only one sample as the
                # difference between start and end is 0. Use the entire duration
                # of the test as the sample start/end
                sample_duration = end_ts - start_ts

            logger = {
                # All data domains should have same sample count.
                'sample_count': summary[source]['time']['count'],
                'sample_duration': sample_duration,
                'data': data,
                'average': ave,
                # TODO(mqg): hard code the units for now because we are only
                # dealing with power so far. When we start to work with voltage
                # or current, read the units from the .json files.
                'unit': {k: 'milliwatt' for k in data},
                'type': {k: 'servod' for k in data},
            }

            loggers.append(logger)

        return loggers


class PowerlogTelemetryLogger(PowerTelemetryLogger):
    """This logger class measures power with Sweetberry via powerlog tool.
    """

    DEFAULT_SWEETBERRY_INTERVAL = 20.0
    SWEETBERRY_CONFIG_DIR = os.path.join(
            sysconfig.get_python_lib(standard_lib=False), 'servo', 'data')

    def __init__(self, config, resultsdir, host):
        """Init PowerlogTelemetryLogger.

        @param config: the args argument from test_that in a dict. Settings for
                       power telemetry devices.
                       required data: {'test': 'test_TestName.tag'}
        @param resultsdir: path to directory where current autotest results are
                           stored, e.g. /tmp/test_that_results/
                           results-1-test_TestName.tag/test_TestName.tag/
                           results/
        @param host: CrosHost object representing the DUT.
        """
        super(PowerlogTelemetryLogger, self).__init__(config, resultsdir, host)

        self._interval = float(config.get('sweetberry_interval',
                                          self.DEFAULT_SWEETBERRY_INTERVAL))
        self._logdir = os.path.join(resultsdir, 'sweetberry_log')
        self._end_flag = threading.Event()
        self._sweetberry_serial = config.get('sweetberry_serial', None)
        if 'sweetberry_config' in config:
            self._sweetberry_config = config['sweetberry_config']
        else:
            board = self._host.get_board().replace('board:', '')
            hardware_rev = self._host.get_hardware_revision()
            self._sweetberry_config = board + '_' + hardware_rev
        board_path, scenario_path = \
                self._get_sweetberry_config_path(self._sweetberry_config)
        self._sweetberry_thread = SweetberryThread(
                board=board_path,
                scenario=scenario_path,
                interval=self._interval,
                stats_json_dir=self._logdir,
                end_flag=self._end_flag,
                serial=self._sweetberry_serial)
        self._sweetberry_thread.setDaemon(True)

    def _start_measurement(self):
        """Start power measurement with Sweetberry via powerlog tool."""
        self._sweetberry_thread.start()

    def _export_data_locally(self, client_test_dir):
        """Output formatted text summaries locally."""
        #TODO(crbug.com/978665): implement this.
        pass

    def _end_measurement(self):
        """End querying Sweetberry."""
        self._end_flag.set()
        # Sweetberry thread should theoretically finish within 1 self._interval
        # but giving 2 here to be more lenient.
        self._sweetberry_thread.join(self._interval * 2)
        if self._sweetberry_thread.is_alive():
            logging.warning('%s %s thread did not finish. There might be extra '
                            'data at the end.', self.__class__.__name__,
                            self._sweetberry_thread.name)

    def _load_and_trim_data(self, start_ts, end_ts):
        """Load data and trim data.

        Load and format data recorded by powerlog tool. Trim data if necessary.
        """
        if not os.path.exists(self._logdir):
            logging.error('Cannot find %s, no Sweetberry measurements exist, '
                          'not uploading to dashboard.', self._logdir)
            return

        trimmed_log_dirs = list()
        # Adding a padding to both start and end timestamp because the timestamp
        # of each data point is taken at the end of its corresponding interval.
        start_ts = start_ts + self._interval / 2 if start_ts else 0
        end_ts = end_ts + self._interval / 2 if end_ts else time.time()
        for dirname in os.listdir(self._logdir):
            if dirname.startswith('sweetberry'):
                sweetberry_ts = float(string.lstrip(dirname, 'sweetberry'))
                if start_ts <= sweetberry_ts <= end_ts:
                    trimmed_log_dirs.append(dirname)

        data = collections.defaultdict(list)
        for sweetberry_file in sorted(trimmed_log_dirs):
            fname = os.path.join(self._logdir, sweetberry_file, 'summary.json')
            with open(fname, 'r') as f:
                d = json.load(f)
                for k, v in d.iteritems():
                    data[k].append(v['mean'])

        logger = {
            # All data domains should have same sample count.
            'sample_count': len(data.itervalues().next()),
            'sample_duration': self._interval,
            'data': data,
            'average': {k: numpy.average(v) for k, v in data.iteritems()},
            # TODO(mqg): hard code the units for now because we are only dealing
            # with power so far. When we start to work with voltage or current,
            # read the units from the .json files.
            'unit': {k: 'milliwatt' for k in data},
            'type': {k: 'sweetberry' for k in data},
        }

        return [logger]

    def _get_sweetberry_config_path(self, filename):
        """Get the absolute path for Sweetberry board and scenario file.

        @param filename: string of Sweetberry config filename.
        @return a tuple of the path to Sweetberry board file and the path to
                Sweetberry scenario file.
        @raises error.TestError if board file or scenario file does not exist in
                file system.
        """
        board_path = os.path.join(self.SWEETBERRY_CONFIG_DIR,
                                  '%s.board' % filename)
        if not os.path.isfile(board_path):
            msg = 'Sweetberry board file %s does not exist.' % board_path
            raise error.TestError(msg)

        scenario_path = os.path.join(self.SWEETBERRY_CONFIG_DIR,
                                     '%s.scenario' % filename)
        if not os.path.isfile(scenario_path):
            msg = 'Sweetberry scenario file %s does not exist.' % scenario_path
            raise error.TestError(msg)
        return (board_path, scenario_path)


class SweetberryThread(threading.Thread):
    """A thread that starts and ends Sweetberry measurement."""

    def __init__(self, board, scenario, interval, stats_json_dir, end_flag,
                 serial=None):
        """Initialize the Sweetberry thread.

        Once started, this thread will invoke Sweetberry powerlog tool every
        [interval] seconds, which will sample each rail in [scenario] file
        multiple times and write the average of those samples in json format to
        [stats_json_dir]. The resistor size of each power rail is specified in
        [board] file.

        See go/sweetberry and go/sweetberry-readme for more details.

        @param board: file name for Sweetberry board file.
        @param scenario: file name for Sweetberry scenario file.
        @param interval: time of each Sweetberry run cycle; print Sweetberry
                         data every <interval> seconds.
        @param stats_json_dir: directory to store Sweetberry stats in json.
        @param end_flag: event object, stop Sweetberry measurement when this is
                         set.
        @param serial: serial number of sweetberry.
        """
        threading.Thread.__init__(self, name='Sweetberry')
        self._end_flag = end_flag
        self._interval = interval
        self._argv = ['--board', board,
                      '--config', scenario,
                      '--save_stats_json', stats_json_dir,
                      '--no_print_raw_data',
                      '--mW']
        if serial:
            self._argv.extend(['--serial', serial])

    def run(self):
        """Start Sweetberry measurement until end_flag is set."""
        logging.debug('Sweetberry starts.')
        loop = 0
        start_timestamp = time.time()
        while not self._end_flag.is_set():
            # TODO(mqg): in the future use more of powerlog components
            # explicitly, make a long call and harvest data from Sweetberry,
            # instead of using it like a command line tool now.
            loop += 1
            next_loop_start_timestamp = start_timestamp + loop * self._interval
            current_timestamp = time.time()
            this_loop_duration = next_loop_start_timestamp - current_timestamp
            powerlog.main(self._argv + ['--seconds', str(this_loop_duration)])
        logging.debug('Sweetberry stops.')
