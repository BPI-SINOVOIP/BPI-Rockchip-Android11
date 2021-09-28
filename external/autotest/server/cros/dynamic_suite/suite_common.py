# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Shared functions by dynamic_suite/suite.py & skylab_suite/cros_suite.py."""

from __future__ import division
from __future__ import print_function

import datetime
import logging
import multiprocessing
import re

import common

from autotest_lib.client.common_lib import control_data
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import global_config
from autotest_lib.client.common_lib import time_utils
from autotest_lib.client.common_lib.cros import dev_server
from autotest_lib.server.cros import provision
from autotest_lib.server.cros.dynamic_suite import constants
from autotest_lib.server.cros.dynamic_suite import control_file_getter
from autotest_lib.server.cros.dynamic_suite import tools

ENABLE_CONTROLS_IN_BATCH = global_config.global_config.get_config_value(
        'CROS', 'enable_getting_controls_in_batch', type=bool, default=False)


def canonicalize_suite_name(suite_name):
    """Canonicalize the suite's name.

    @param suite_name: the name of the suite.
    """
    # Do not change this naming convention without updating
    # site_utils.parse_job_name.
    return 'test_suites/control.%s' % suite_name


def _formatted_now():
    """Format the current datetime."""
    return datetime.datetime.now().strftime(time_utils.TIME_FMT)


def make_builds_from_options(options):
    """Create a dict of builds for creating a suite job.

    The returned dict maps version label prefixes to build names. Together,
    each key-value pair describes a complete label.

    @param options: SimpleNamespace from argument parsing.

    @return: dict mapping version label prefixes to build names
    """
    builds = {}
    build_prefix = None
    if options.build:
        build_prefix = provision.get_version_label_prefix(options.build)
        builds[build_prefix] = options.build

    if options.cheets_build:
        builds[provision.CROS_ANDROID_VERSION_PREFIX] = options.cheets_build
        if build_prefix == provision.CROS_VERSION_PREFIX:
            builds[build_prefix] += provision.CHEETS_SUFFIX

    if options.firmware_rw_build:
        builds[provision.FW_RW_VERSION_PREFIX] = options.firmware_rw_build

    if options.firmware_ro_build:
        builds[provision.FW_RO_VERSION_PREFIX] = options.firmware_ro_build

    return builds


def get_test_source_build(builds, **dargs):
    """Get the build of test code.

    Get the test source build from arguments. If parameter
    `test_source_build` is set and has a value, return its value. Otherwise
    returns the ChromeOS build name if it exists. If ChromeOS build is not
    specified either, raise SuiteArgumentException.

    @param builds: the builds on which we're running this suite. It's a
                   dictionary of version_prefix:build.
    @param **dargs: Any other Suite constructor parameters, as described
                    in Suite.__init__ docstring.

    @return: The build contains the test code.
    @raise: SuiteArgumentException if both test_source_build and ChromeOS
            build are not specified.

    """
    if dargs.get('test_source_build', None):
        return dargs['test_source_build']

    cros_build = builds.get(provision.CROS_VERSION_PREFIX, None)
    if cros_build.endswith(provision.CHEETS_SUFFIX):
        test_source_build = re.sub(
                provision.CHEETS_SUFFIX + '$', '', cros_build)
    else:
        test_source_build = cros_build

    if not test_source_build:
        raise error.SuiteArgumentException(
                'test_source_build must be specified if CrOS build is not '
                'specified.')

    return test_source_build


def stage_build_artifacts(build, hostname=None, artifacts=[]):
    """
    Ensure components of |build| necessary for installing images are staged.

    @param build image we want to stage.
    @param hostname hostname of a dut may run test on. This is to help to locate
        a devserver closer to duts if needed. Default is None.
    @param artifacts A list of string artifact name to be staged.

    @raises StageControlFileFailure: if the dev server throws 500 while staging
        suite control files.

    @return: dev_server.ImageServer instance to use with this build.
    @return: timings dictionary containing staging start/end times.
    """
    timings = {}
    # Ensure components of |build| necessary for installing images are staged
    # on the dev server. However set synchronous to False to allow other
    # components to be downloaded in the background.
    ds = dev_server.resolve(build, hostname=hostname)
    ds_name = ds.hostname
    timings[constants.DOWNLOAD_STARTED_TIME] = _formatted_now()
    try:
        artifacts_to_stage = ['test_suites', 'control_files']
        artifacts_to_stage.extend(artifacts if artifacts else [])
        ds.stage_artifacts(image=build, artifacts=artifacts_to_stage)
    except dev_server.DevServerException as e:
        raise error.StageControlFileFailure(
                "Failed to stage %s on %s: %s" % (build, ds_name, e))
    timings[constants.PAYLOAD_FINISHED_TIME] = _formatted_now()
    return ds, timings


def get_control_file_by_build(build, ds, suite_name):
    """Return control file contents for |suite_name|.

    Query the dev server at |ds| for the control file |suite_name|, included
    in |build| for |board|.

    @param build: unique name by which to refer to the image from now on.
    @param ds: a dev_server.DevServer instance to fetch control file with.
    @param suite_name: canonicalized suite name, e.g. test_suites/control.bvt.
    @raises ControlFileNotFound if a unique suite control file doesn't exist.
    @raises NoControlFileList if we can't list the control files at all.
    @raises ControlFileEmpty if the control file exists on the server, but
                             can't be read.

    @return the contents of the desired control file.
    """
    getter = control_file_getter.DevServerGetter.create(build, ds)
    devserver_name = ds.hostname
    # Get the control file for the suite.
    try:
        control_file_in = getter.get_control_file_contents_by_name(suite_name)
    except error.CrosDynamicSuiteException as e:
        raise type(e)('Failed to get control file for %s '
                      '(devserver: %s) (error: %s)' %
                      (build, devserver_name, e))
    if not control_file_in:
        raise error.ControlFileEmpty(
            "Fetching %s returned no data. (devserver: %s)" %
            (suite_name, devserver_name))
    # Force control files to only contain ascii characters.
    try:
        control_file_in.encode('ascii')
    except UnicodeDecodeError as e:
        raise error.ControlFileMalformed(str(e))

    return control_file_in


def _should_batch_with(cf_getter):
    """Return whether control files should be fetched in batch.

    This depends on the control file getter and configuration options.

    If cf_getter is a File system ControlFileGetter, the cf_getter will
    perform a full parse of the root directory associated with the
    getter. This is the case when it's invoked from suite_preprocessor.

    If cf_getter is a devserver getter, this will look up the suite_name in a
    suite to control file map generated at build time, and parses the relevant
    control files alone. This lookup happens on the devserver, so as far
    as this method is concerned, both cases are equivalent. If
    enable_controls_in_batch is switched on, this function will call
    cf_getter.get_suite_info() to get a dict of control files and
    contents in batch.

    @param cf_getter: a control_file_getter.ControlFileGetter used to list
           and fetch the content of control files
    """
    return (ENABLE_CONTROLS_IN_BATCH
            and isinstance(cf_getter, control_file_getter.DevServerGetter))


def _get_cf_texts_for_suite_batched(cf_getter, suite_name):
    """Get control file content for given suite with batched getter.

    See get_cf_texts_for_suite for params & returns.
    """
    suite_info = cf_getter.get_suite_info(suite_name=suite_name)
    files = suite_info.keys()
    filtered_files = _filter_cf_paths(files)
    for path in filtered_files:
        yield path, suite_info[path]


def _get_cf_texts_for_suite_unbatched(cf_getter, suite_name):
    """Get control file content for given suite with unbatched getter.

    See get_cf_texts_for_suite for params & returns.
    """
    files = cf_getter.get_control_file_list(suite_name=suite_name)
    filtered_files = _filter_cf_paths(files)
    for path in filtered_files:
        yield path, cf_getter.get_control_file_contents(path)


def _filter_cf_paths(paths):
    """Remove certain control file paths.

    @param paths: Iterable of paths
    @returns: generator yielding paths
    """
    matcher = re.compile(r'[^/]+/(deps|profilers)/.+')
    return (path for path in paths if not matcher.match(path))


def get_cf_texts_for_suite(cf_getter, suite_name):
    """Get control file content for given suite.

    @param cf_getter: A control file getter object, e.g.
        a control_file_getter.DevServerGetter object.
    @param suite_name: If specified, this method will attempt to restrain
                       the search space to just this suite's control files.
    @returns: generator yielding (path, text) tuples
    """
    if _should_batch_with(cf_getter):
        return _get_cf_texts_for_suite_batched(cf_getter, suite_name)
    else:
        return _get_cf_texts_for_suite_unbatched(cf_getter, suite_name)


def parse_cf_text(path, text):
    """Parse control file text.

    @param path: path to control file
    @param text: control file text contents

    @returns: a ControlData object

    @raises ControlVariableException: There is a syntax error in a
                                      control file.
    """
    test = control_data.parse_control_string(
            text, raise_warnings=True, path=path)
    test.text = text
    return test

def parse_cf_text_process(data):
    """Worker process for parsing control file text

    @param data: Tuple of path, text, forgiving_error, and test_args.

    @returns: Tuple of the path and test ControlData

    @raises ControlVariableException: If forgiving_error is false parsing
                                      exceptions are raised instead of logged.
    """
    path, text, forgiving_error, test_args = data

    if test_args:
        text = tools.inject_vars(test_args, text)

    try:
        found_test = parse_cf_text(path, text)
    except control_data.ControlVariableException, e:
        if not forgiving_error:
            msg = "Failed parsing %s\n%s" % (path, e)
            raise control_data.ControlVariableException(msg)
        logging.warning("Skipping %s\n%s", path, e)
    except Exception, e:
        logging.error("Bad %s\n%s", path, e)
        import traceback
        logging.error(traceback.format_exc())
    else:
        return (path, found_test)


def get_process_limit():
    """Limit the number of CPUs to use.

    On a server many autotest instances can run in parallel. Avoid that
    each of them requests all the CPUs at the same time causing a spike.
    """
    return min(8, multiprocessing.cpu_count())


def parse_cf_text_many(control_file_texts,
                       forgiving_error=False,
                       test_args=None):
    """Parse control file texts.

    @param control_file_texts: iterable of (path, text) pairs
    @param test_args: The test args to be injected into test control file.

    @returns: a dictionary of ControlData objects
    """
    tests = {}

    control_file_texts_all = list(control_file_texts)
    if control_file_texts_all:
        # Construct input data for worker processes. Each row contains the
        # path, text, forgiving_error configuration, and test arguments.
        paths, texts = zip(*control_file_texts_all)
        worker_data = zip(paths, texts, [forgiving_error] * len(paths),
                          [test_args] * len(paths))
        pool = multiprocessing.Pool(processes=get_process_limit())
        result_list = pool.map(parse_cf_text_process, worker_data)
        pool.close()
        pool.join()

        # Convert [(path, test), ...] to {path: test, ...}
        tests = dict(result_list)

    return tests


def retrieve_control_data_for_test(cf_getter, test_name):
    """Retrieve a test's control file.

    @param cf_getter: a control_file_getter.ControlFileGetter object to
                      list and fetch the control files' content.
    @param test_name: Name of test to retrieve.

    @raises ControlVariableException: There is a syntax error in a
                                      control file.

    @returns a ControlData object
    """
    path = cf_getter.get_control_file_path(test_name)
    text = cf_getter.get_control_file_contents(path)
    return parse_cf_text(path, text)


def retrieve_for_suite(cf_getter, suite_name='', forgiving_error=False,
                       test_args=None):
    """Scan through all tests and find all tests.

    @param suite_name: If specified, retrieve this suite's control file.

    @raises ControlVariableException: If forgiving_parser is False and there
                                      is a syntax error in a control file.

    @returns a dictionary of ControlData objects that based on given
             parameters.
    """
    control_file_texts = get_cf_texts_for_suite(cf_getter, suite_name)
    return parse_cf_text_many(control_file_texts,
                              forgiving_error=forgiving_error,
                              test_args=test_args)


def filter_tests(tests, predicate=lambda t: True):
    """Filter child tests with predicates.

    @tests: A dict of ControlData objects as tests.
    @predicate: A test filter. By default it's None.

    @returns a list of ControlData objects as tests.
    """
    logging.info('Parsed %s child test control files.', len(tests))
    tests = [test for test in tests.itervalues() if predicate(test)]
    tests.sort(key=lambda t:
               control_data.ControlData.get_test_time_index(t.time),
               reverse=True)
    return tests


def name_in_tag_predicate(name):
    """Returns predicate that takes a control file and looks for |name|.

    Builds a predicate that takes in a parsed control file (a ControlData)
    and returns True if the SUITE tag is present and contains |name|.

    @param name: the suite name to base the predicate on.
    @return a callable that takes a ControlData and looks for |name| in that
            ControlData object's suite member.
    """
    return lambda t: name in t.suite_tag_parts


def test_name_in_list_predicate(name_list):
    """Returns a predicate that matches control files by test name.

    The returned predicate returns True for control files whose test name
    is present in name_list.
    """
    name_set = set(name_list)
    return lambda t: t.name in name_set
