# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import subprocess
import threading
import time

# A path of the log file
_PATH_LOG_FILE = '/var/log/messages'
# Time margin as a number of seconds. It is used during parsing of the log file.
# Sometimes, logs are not in strict datetime order. This value defines maximum
# possible datetime error in logs sequence.
_TIME_MARGIN = 2
# Max time in seconds to wait for CUPS logs to appear in the log file.
_TIME_MAX_WAIT = 100
# A list of filters that are installed by loading components.
# Currently, they are Star and Epson drivers.
_COMPONENT_FILTERS = ['rastertostar', 'rastertostarlm', 'epson-escpr-wrapper']


class LogReader():
    """
    An instance of this class is responsible for gathering data about CUPS jobs
    and extracting their results and pipelines by parsing the main log file. It
    can check for internal errors during a job execution and build a bash script
    simulating an executed pipeline. CUPS server must have logging level set to
    'debug' to make a pipeline extraction possible.

    """

    def _read_last_lines(self, lines_count):
        """
        The function reads given number of lines from the end of the log file.
        The last line is omitted if it is not ended with EOL character. If a
        whole file was read, count of returned lines may be smaller than
        requested number. Always at least one line is returned; if the file has
        no EOL characters an exception is raised.

        @param lines_count: a number of lines to read

        @returns a list of lines

        """
        assert lines_count > 0
        argv = ['tail', '-n', '%d' % (lines_count+1), _PATH_LOG_FILE]
        p1 = subprocess.Popen(argv, stdout=subprocess.PIPE)
        out,err = p1.communicate()
        lines = out.split('\n')
        lines.pop()
        if len(lines) > lines_count:
            if len(lines) == 0:
                raise Exception('The file %s is empty' % _PATH_LOG_FILE)
            lines = lines[1:]
        return lines


    def _get_datetime_from_line(self, line):
        """
        Parse datetime from the given line read from the log file.

        @param line_count: single line from the log files

        @returns a datetime as a number of seconds from the epoch (float)

        """
        return time.mktime(time.strptime(line[:20], "%Y-%m-%dT%H:%M:%S."))


    def __init__(self):
        """
        Constructor.

        """
        # the last line read from the log file
        self._last_line_read = self._read_last_lines(1)[0]
        # logs obtained from the log file after preprocessing
        # key = a tuple (CUPS pid, job id)
        # value = list with logs
        self._logs = dict()
        # data extracted from logs
        # key = a tuple (CUPS pid, job id)
        # value = a tuple (result(bool),envp(dict),argv(list),filters(list))
        self._results = dict()
        # mapping from printer's name to a list of CUPS jobs
        # key = printer name
        # value = a list of tuples (CUPS pid, job id)
        self._printer_name_to_key = dict()
        # a lock for synchronization
        self._critical_section = threading.Lock()


    def _parse_logs(self):
        """
        This method read and parses new lines from the log file (all complete
        lines after the line saved in self._last_line_read). The lines are
        filtered and saved in self._logs. Dictionaries self._results and
        self._printer_name_to_key are updated accordingly.

        """
        # ==============================================
        # try to read lines from the end of the log file until the line from
        # self._last_line_read is found
        dt_last_line_read = self._get_datetime_from_line(self._last_line_read)
        lines_count = 1024
        pos_last_line_read = None
        while True:
            lines = self._read_last_lines(lines_count)
            # search for self._last_line_read in lines read from the file
            for index,line in enumerate(lines):
                dt_line = self._get_datetime_from_line(line)
                if dt_line > dt_last_line_read + _TIME_MARGIN:
                    # a datetime of the line is far behind, exit the loop
                    break
                if line == self._last_line_read:
                    # the line was found, save its index and exit the loop
                    pos_last_line_read = index
                    break
            # if the self._last_line_read was found, exit the 'while' loop
            if pos_last_line_read is not None:
                break
            # check if we can try to read more lines from the file to repeat
            # the search, if not then throw an exception
            if ( len(lines) < lines_count or
                    self._get_datetime_from_line(lines[0]) +
                    _TIME_MARGIN < dt_last_line_read ):
                raise Exception('Cannot find the last read line in the log'
                        ' file (maybe it has been erased?)')
            # increase the number of lines to read
            lines_count = lines_count * 2
        # check if there are any new lines, exit if not
        if pos_last_line_read + 1 == len(lines):
            return
        # update the field with the last line read from the log file
        self._last_line_read = lines[-1]
        # ==============================================
        # parse logs, select these ones from CUPS jobs and save them to
        # self._logs
        updated_jobs = set()
        for line in lines[pos_last_line_read+1:]:
            tokens = line.split(' ', 5)
            if ( len(tokens) < 6 or tokens[2][:6] != 'cupsd[' or
                    tokens[3] != '[Job' ):
                continue
            pid = int(tokens[2][6:-2])
            job_id = int(tokens[4][:-1])
            message = tokens[5]
            key = (pid,job_id)
            if key not in self._logs:
                self._logs[key] = []
            self._logs[key].append(message)
            updated_jobs.add(key)
        # ==============================================
        # try to interpret logs
        for key in updated_jobs:
            number_of_processes_to_check = None
            printer_name = None
            result = True
            envp = dict()
            argv = []
            filters = []
            for msg in self._logs[key]:
                if number_of_processes_to_check is None:
                    regexp = re.match(r'(\d) filters for job:', msg)
                    if regexp is not None:
                        number_of_processes_to_check = int(regexp.group(1)) + 1
                        assert number_of_processes_to_check > 0
                elif printer_name is None:
                    regexp = re.match(r'(\S+) \(\S+ to (\S+), cost \d+\)', msg)
                    if regexp is not None:
                        fltr = regexp.group(1)
                        if fltr == '-':
                            number_of_processes_to_check -= 1
                        else:
                            filters.append(fltr)
                        trg = regexp.group(2)
                        if trg.startswith('printer/') and '/' not in trg[8:]:
                            printer_name = trg[8:]
                else:
                    regexp = re.match(r'envp\[\d+\]="(\S+)=(\S+)"', msg)
                    if regexp is not None:
                        envp[regexp.group(1)] = regexp.group(2)
                        continue
                    regexp = re.match(r'argv\[(\d+)\]="(.+)"', msg)
                    if regexp is not None:
                        if len(argv) != int(regexp.group(1)):
                            raise Exception('Error when parsing argv for %s'
                                    % printer_name)
                        argv.append(regexp.group(2))
                        continue
                    if number_of_processes_to_check > 0:
                        regexp = re.match(r'PID \d+ \(\S+\) ', msg)
                        if regexp is not None:
                            number_of_processes_to_check -= 1
                            if ( not msg.endswith(' exited with no errors.') and
                                    'was terminated normally with signal'
                                    not in msg ):
                                result = False
            if number_of_processes_to_check == 0:
                # if it is a new job, update self._printer_name_to_key
                if key not in self._results:
                    if printer_name not in self._printer_name_to_key:
                        self._printer_name_to_key[printer_name] = [key]
                    else:
                        self._printer_name_to_key[printer_name].append(key)
                # update results
                self._results[key] = (result,envp,argv,filters)


    def _build_pipeline(
            self, printer_name, envp, argv, filters,
            path_ppd, path_doc, path_workdir):
        """
        This function tries to build a bash script containing a pipeline used
        internally by CUPS. All printer's parameters are passed by the field
        self._printers_data. Resultant script is saved in self._pipelines.

        @param printer_name: a printer's name
        @param envp: a dictionary with environment variables for the pipeline
        @param argv: a list with command line parameters for filters
        @param filters: a list of filters
        @param path_ppd: a path to PPD file (string)
        @param path_doc: a path to input document (string)
        @param path_workdir: a path to a directory, that should be used as home
                directory and temporary location by filters

        @returns a pipeline as a bash script (string)

        """
        # update paths set in environment variables
        envp['PPD'] = path_ppd
        envp['HOME'] = path_workdir
        envp['TMPDIR'] = path_workdir
        # drop the last argument, it is an input file
        argv.pop()
        # replace filter names by full path for Star and Epson drivers
        if filters[-1] in _COMPONENT_FILTERS:
            find_cmd = ['find', '/run/imageloader/', '-type', 'f', '-name']
            find_cmd.append(filters[-1])
            filters[-1] = subprocess.check_output(find_cmd).rstrip()
        # build and return the script
        script = '#!/bin/bash\nset -e\nset -o pipefail\n'
        for name, value in envp.iteritems():
            script += ('export %s=%s\n' % (name, value))
        for ind, filt in enumerate(filters):
            if ind > 0:
                script += ('gzip -dc %d.doc.gz | ' % ind)
            script += ('(exec -a "%s" %s' % (argv[0], filt))
            for arg in argv[1:]:
                script += (' "%s"' % arg)
            if ind == 0:
                script += (' "%s"' % path_doc)
            script += (') 2>%d.err | gzip -9 >%d.doc.gz\n' % (ind+1, ind+1))
        return script


    def extract_result(
            self, printer_name,
            path_ppd=None, path_doc=None, path_workdir=None):
        """
        This function extract from the log file CUPS logs related to printer
        with given name. Extracted logs are also interpreted, the function
        checks for any failures. If all optional parameters are set, a pipeline
        used by CUPS is extracted from the logs. If some of required results
        cannot be extracted, an exception is raised. This function is
        thread-safe, it can be called simultaneously by many Python threads.
        However, all simultaneous calls must have different values of the
        printer_name parameter (calls with the same printer_name must be done
        sequentially).

        @param printer_name: a printer's name
        @param path_ppd: a path to PPD file (string), required for pipelinie
                extraction only
        @param path_doc: a path to input document (string), required for
                pipelinie extraction only
        @param path_workdir: a path to a directory, that should be used as home
                directory and temporary location by filters, required for
                pipelinie extraction only

        @returns a tuple of three variables:
                1. True if no errors were detected, False otherwise
                2. logs extracted from the log file (string)
                3. pipeline as a bash script (string), None if not requested

        @raises Exception in case of any errors

        """
        # time when we started efforts to get logs
        time_start = time.time()
        # all three parameters must be set to extract a pipeline
        pipeline_required = (path_ppd is not None and path_doc is not None
                and path_workdir is not None)
        try:
            # try to get and parse logs
            while True:
                with self._critical_section:
                    # leave the loop if we have everything
                    if printer_name in self._printer_name_to_key:
                        if not pipeline_required:
                            break
                        key = self._printer_name_to_key[printer_name][0]
                        envp = self._results[key][1]
                        argv = self._results[key][2]
                        if len(envp) > 0 and len(argv) > 0:
                            break
                    # save current time and try to parse the log file
                    time_last_read = time.time()
                    self._parse_logs()
                    # leave the loop if we have everything
                    if printer_name in self._printer_name_to_key:
                        if not pipeline_required:
                            break
                        key = self._printer_name_to_key[printer_name][0]
                        envp = self._results[key][1]
                        argv = self._results[key][2]
                        if len(envp) > 0 and len(argv) > 0:
                            break
                # raise an exception if we have been waiting too long
                if time_last_read - time_start > _TIME_MAX_WAIT:
                    raise Exception('Cannot parse logs for %s or they did not'
                            ' appear in %s within %d seconds from printing the'
                            ' document' % (printer_name, _PATH_LOG_FILE,
                            _TIME_MAX_WAIT))
                # wait 1 second and try again to parse acquired logs
                time.sleep(1)
            # retrieve required data
            with self._critical_section:
                if pipeline_required:
                    # key, envp, argv are already set in the loop above
                    filters = self._results[key][3]
                else:
                    key = self._printer_name_to_key[printer_name][0]
                no_errors = self._results[key][0]
                logs = '\n'.join(self._logs[key])
            # build a pipeline script if required
            if pipeline_required:
                pipeline = self._build_pipeline(printer_name, envp, argv,
                        filters, path_ppd, path_doc, path_workdir)
            else:
                pipeline = None
            # return results
            return no_errors, logs, pipeline
        finally:
            # cleanup
            if printer_name in self._printer_name_to_key:
                with self._critical_section:
                    key = self._printer_name_to_key[printer_name].pop(0)
                    if len(self._printer_name_to_key[printer_name]) == 0:
                        self._printer_name_to_key.pop(printer_name)
                    self._logs.pop(key)
                    self._results.pop(key)
