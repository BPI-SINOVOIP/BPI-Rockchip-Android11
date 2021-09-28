#!/usr/bin/env python2
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import contextlib
import copy
import logging
import os
import re
import shutil
import stat
import subprocess
import tempfile
import textwrap
import zipfile
# Use 'sudo pip install jinja2' to install.
from jinja2 import Template


# TODO(ihf): Assign better TIME to control files. Scheduling uses this to run
# LENGTHY first, then LONG, MEDIUM etc. But we need LENGTHY for the collect
# job, downgrade all others. Make sure this still works in CQ/smoke suite.
_CONTROLFILE_TEMPLATE = Template(
    textwrap.dedent("""\
    # Copyright {{year}} The Chromium OS Authors. All rights reserved.
    # Use of this source code is governed by a BSD-style license that can be
    # found in the LICENSE file.

    # This file has been automatically generated. Do not edit!
    {%- if servo_support_needed %}

    from autotest_lib.server import utils

    {%- endif %}

    AUTHOR = 'ARC++ Team'
    NAME = '{{name}}'
    ATTRIBUTES = '{{attributes}}'
    DEPENDENCIES = '{{dependencies}}'
    JOB_RETRIES = {{job_retries}}
    TEST_TYPE = 'server'
    TIME = '{{test_length}}'
    MAX_RESULT_SIZE_KB = {{max_result_size_kb}}
    {%- if sync_count and sync_count > 1 %}
    SYNC_COUNT = {{sync_count}}
    {%- endif %}
    {%- if priority %}
    PRIORITY = {{priority}}
    {%- endif %}
    DOC = '{{DOC}}'
    {%- if servo_support_needed %}

    # For local debugging, if your test setup doesn't have servo, REMOVE these
    # two lines.
    args_dict = utils.args_to_dict(args)
    servo_args = hosts.CrosHost.get_servo_arguments(args_dict)

    {%- endif %}
    {% if sync_count and sync_count > 1 %}
    from autotest_lib.server import utils as server_utils
    def {{test_func_name}}(ntuples):
        host_list = [hosts.create_host(machine) for machine in ntuples]
    {% else %}
    def {{test_func_name}}(machine):
        {%- if servo_support_needed %}
        # REMOVE 'servo_args=servo_args' arg for local debugging if your test
        # setup doesn't have servo.
        try:
            host_list = [hosts.create_host(machine, servo_args=servo_args)]
        except:
            # Just ignore any servo setup flakiness.
            host_list = [hosts.create_host(machine)]
        {%- else %}
        host_list = [hosts.create_host(machine)]
        {%- endif %}
    {%- endif %}
        job.run_test(
            '{{base_name}}',
    {%- if camera_facing %}
            camera_facing='{{camera_facing}}',
            cmdline_args=args,
    {%- endif %}
            hosts=host_list,
            iterations=1,
    {%- if max_retries != None %}
            max_retry={{max_retries}},
    {%- endif %}
    {%- if enable_default_apps %}
            enable_default_apps=True,
    {%- endif %}
    {%- if needs_push_media %}
            needs_push_media={{needs_push_media}},
    {%- endif %}
            tag='{{tag}}',
            test_name='{{name}}',
    {%- if authkey %}
            authkey='{{authkey}}',
    {%- endif %}
            run_template={{run_template}},
            retry_template={{retry_template}},
            target_module={% if target_module %}'{{target_module}}'{% else %}None{%endif%},
            target_plan={% if target_plan %}'{{target_plan}}'{% else %}None{% endif %},
    {%- if abi %}
            bundle='{{abi}}',
    {%- endif %}
    {%- if extra_artifacts %}
            extra_artifacts={{extra_artifacts}},
    {%- endif %}
    {%- if extra_artifacts_host %}
            extra_artifacts_host={{extra_artifacts_host}},
    {%- endif %}
    {%- if uri %}
            uri='{{uri}}',
    {%- endif %}
    {%- for arg in extra_args %}
            {{arg}},
    {%- endfor %}
    {%- if servo_support_needed %}
            hard_reboot_on_failure=True,
    {%- endif %}
            timeout={{timeout}})

    {% if sync_count and sync_count > 1 -%}
    ntuples, failures = server_utils.form_ntuples_from_machines(machines,
                                                                SYNC_COUNT)
    # Use log=False in parallel_simple to avoid an exception in setting up
    # the incremental parser when SYNC_COUNT > 1.
    parallel_simple({{test_func_name}}, ntuples, log=False)
    {% else -%}
    parallel_simple({{test_func_name}}, machines)
    {% endif %}
"""))

CONFIG = None

_COLLECT = 'tradefed-run-collect-tests-only-internal'
_PUBLIC_COLLECT = 'tradefed-run-collect-tests-only'

_TEST_LENGTH = {1: 'FAST', 2: 'SHORT', 3: 'MEDIUM', 4: 'LONG', 5: 'LENGTHY'}

_ALL = 'all'


def get_tradefed_build(line):
    """Gets the build of Android CTS from tradefed.

    @param line Tradefed identification output on startup. Example:
                Android Compatibility Test Suite 7.0 (3423912)
    @return Tradefed CTS build. Example: 2813453.
    """
    # Sample string:
    # - Android Compatibility Test Suite 7.0 (3423912)
    # - Android Compatibility Test Suite for Instant Apps 1.0 (4898911)
    # - Android Google Mobile Services (GMS) Test Suite 6.0_r1 (4756896)
    m = re.search(r' \((.*)\)', line)
    if m:
        return m.group(1)
    logging.warning('Could not identify build in line "%s".', line)
    return '<unknown>'


def get_tradefed_revision(line):
    """Gets the revision of Android CTS from tradefed.

    @param line Tradefed identification output on startup.
                Example:
                 Android Compatibility Test Suite 6.0_r6 (2813453)
                 Android Compatibility Test Suite for Instant Apps 1.0 (4898911)
    @return Tradefed CTS revision. Example: 6.0_r6.
    """
    m = re.search(r'Android Google Mobile Services \(GMS\) Test Suite (.*) \(',
                  line)
    if m:
        return m.group(1)

    m = re.search(
        r'Android Compatibility Test Suite(?: for Instant Apps)? (.*) \(', line)
    if m:
        return m.group(1)

    logging.warning('Could not identify revision in line "%s".', line)
    return None


def get_bundle_abi(filename):
    """Makes an educated guess about the ABI.

    In this case we chose to guess by filename, but we could also parse the
    xml files in the module. (Maybe this needs to be done in the future.)
    """
    if filename.endswith('arm.zip'):
        return 'arm'
    if filename.endswith('x86.zip'):
        return 'x86'

    assert(CONFIG['TRADEFED_CTS_COMMAND'] =='gts'), 'Only GTS has empty ABI'
    return ''


def get_extension(module, abi, revision, is_public=False, camera_facing=None):
    """Defines a unique string.

    Notice we chose module revision first, then abi, as the module revision
    changes regularly. This ordering makes it simpler to add/remove modules.
    @param module: CTS module which will be tested in the control file. If 'all'
                   is specified, the control file will runs all the tests.
    @param public: boolean variable to specify whether or not the bundle is from
                   public source or not.
    @param camera_facing: string or None indicate whether it's camerabox tests
                          for specific camera facing or not.
    @return string: unique string for specific tests. If public=True then the
                    string is "<abi>.<module>", otherwise, the unique string is
                    "<revision>.<abi>.<module>". Note that if abi is empty, the
                    abi part is omitted.
    """
    ext_parts = []
    if not is_public:
        ext_parts = [revision]
    if abi:
        ext_parts += [abi]
    ext_parts += [module]
    if camera_facing:
        ext_parts += ['camerabox', camera_facing]
    return '.'.join(ext_parts)


def get_doc(modules, abi, is_public):
    """Defines the control file DOC string."""
    if modules.intersection(get_collect_modules(is_public)):
        module_text = 'all'
    else:
        # Generate per-module DOC
        module_text = 'module ' + ', '.join(sorted(list(modules)))

    abi_text = (' using %s ABI' % abi) if abi else ''

    doc = ('Run %s of the %s%s in the ARC++ container.'
           % (module_text, CONFIG['DOC_TITLE'], abi_text))
    return doc


def servo_support_needed(modules, is_public=True):
    """Determines if servo support is needed for a module."""
    return not is_public and all(module in CONFIG['NEEDS_POWER_CYCLE']
                                 for module in modules)


def get_controlfile_name(module,
                         abi,
                         revision,
                         is_public=False,
                         camera_facing=None):
    """Defines the control file name.

    @param module: CTS module which will be tested in the control file. If 'all'
                   is specified, the control file will runs all the tests.
    @param public: boolean variable to specify whether or not the bundle is from
                   public source or not.
    @param camera_facing: string or None indicate whether it's camerabox tests
                          for specific camera facing or not.
    @return string: control file for specific tests. If public=True or
                    module=all, then the name will be "control.<abi>.<module>",
                    otherwise, the name will be
                    "control.<revision>.<abi>.<module>".
    """
    return 'control.%s' % get_extension(module, abi, revision, is_public,
                                        camera_facing)


def get_sync_count(_modules, _abi, _is_public):
    return 1


def get_suites(modules, abi, is_public):
    """Defines the suites associated with a module.

    @param module: CTS module which will be tested in the control file. If 'all'
                   is specified, the control file will runs all the tests.
    # TODO(ihf): Make this work with the "all" and "collect" generation,
    # which currently bypass this function.
    """
    if is_public:
        # On moblab everything runs in the same suite.
        return [CONFIG['MOBLAB_SUITE_NAME']]

    suites = set(CONFIG['INTERNAL_SUITE_NAMES'])

    for module in modules:
        if module in get_collect_modules(is_public):
            # We collect all tests both in arc-gts and arc-gts-qual as both have
            # a chance to be complete (and used for submission).
            suites |= set(CONFIG['QUAL_SUITE_NAMES'])
        if module in CONFIG['EXTRA_ATTRIBUTES']:
            # Special cases come with their own suite definitions.
            suites |= set(CONFIG['EXTRA_ATTRIBUTES'][module])
        if module in CONFIG['SMOKE'] and (abi == 'arm' or abi == ''):
            # Handle VMTest by adding a few jobs to suite:smoke.
            suites.add('suite:smoke')
        if module in CONFIG['HARDWARE_DEPENDENT_MODULES']:
            # CTS modules to be run on all unibuild models.
            suites.add('suite:arc-cts-unibuild-hw')
        if abi == 'x86':
            # Handle a special builder for running all of CTS in a betty VM.
            # TODO(ihf): figure out if this builder is still alive/needed.
            vm_suite = None
            for suite in CONFIG['VMTEST_INFO_SUITES']:
                if not vm_suite:
                    vm_suite = suite
                if module in CONFIG['VMTEST_INFO_SUITES'][suite]:
                    vm_suite = suite
            if vm_suite is not None:
                suites.add('suite:%s' % vm_suite)
        # One or two modules hould be in suite:bvt-arc to cover CQ/PFQ. A few
        # spare/fast modules can run in suite:bvt-perbuild in case we need a
        # replacement for the module in suite:bvt-arc (integration test for
        # cheets_CTS only, not a correctness test for CTS content).
        if module in CONFIG['BVT_ARC'] and (abi == 'arm' or abi == ''):
            suites.add('suite:bvt-arc')
        elif module in CONFIG['BVT_PERBUILD'] and (abi == 'arm' or abi == ''):
            suites.add('suite:bvt-perbuild')
    return sorted(list(suites))


def get_dependencies(modules, abi, is_public, is_camerabox_test):
    """Defines lab dependencies needed to schedule a module.

    @param module: CTS module which will be tested in the control file. If 'all'
                   is specified, the control file will runs all the tests.
    @param abi: string that specifies the application binary interface of the
                current test.
    @param is_public: boolean variable to specify whether or not the bundle is
                      from public source or not.
    @param is_camerabox_test: boolean variable to specify whether it's camerabox
                              related test.
    """
    dependencies = ['arc']
    if abi in CONFIG['LAB_DEPENDENCY']:
        dependencies += CONFIG['LAB_DEPENDENCY'][abi]

    if is_camerabox_test:
        dependencies.append('camerabox')

    for module in modules:
        if is_public and module in CONFIG['PUBLIC_DEPENDENCIES']:
            dependencies.extend(CONFIG['PUBLIC_DEPENDENCIES'][module])

    return ', '.join(dependencies)


def get_job_retries(modules, is_public):
    """Define the number of job retries associated with a module.

    @param module: CTS module which will be tested in the control file. If a
                   special module is specified, the control file will runs all
                   the tests without retry.
    """
    # TODO(haddowk): remove this when cts p has stabalized.
    if is_public:
        return CONFIG['CTS_JOB_RETRIES_IN_PUBLIC']
    retries = 1  # 0 is NO job retries, 1 is one retry etc.
    for module in modules:
        # We don't want job retries for module collection or special cases.
        if (module in get_collect_modules(is_public) or module == _ALL or
            ('CtsDeqpTestCases' in CONFIG['EXTRA_MODULES'] and
             module in CONFIG['EXTRA_MODULES']['CtsDeqpTestCases']['SUBMODULES']
             )):
            retries = 0
    return retries


def get_max_retries(modules, abi, suites, is_public):
    """Partners experiance issues where some modules are flaky and require more

       retries.  Calculate the retry number per module on moblab.
    @param module: CTS module which will be tested in the control file.
    """
    retry = -1
    if is_public:
        if _ALL in CONFIG['PUBLIC_MODULE_RETRY_COUNT']:
            retry = CONFIG['PUBLIC_MODULE_RETRY_COUNT'][_ALL]

        # In moblab at partners we may need many more retries than in lab.
        for module in modules:
            if module in CONFIG['PUBLIC_MODULE_RETRY_COUNT']:
                retry = max(retry, CONFIG['PUBLIC_MODULE_RETRY_COUNT'][module])
    else:
        # See if we have any special values for the module, chose the largest.
        for module in modules:
            if module in CONFIG['CTS_MAX_RETRIES']:
                retry = max(retry, CONFIG['CTS_MAX_RETRIES'][module])

    # Ugly overrides.
    # In bvt we don't want to hold the CQ/PFQ too long.
    if 'suite:bvt-arc' in suites or 'suite:bvt-perbuild' in suites:
        retry = 3
    # During qualification we want at least 9 retries, possibly more.
    # TODO(kinaba&yoshiki): do not abuse suite names
    if CONFIG.get('QUAL_SUITE_NAMES') and \
            set(CONFIG['QUAL_SUITE_NAMES']) & set(suites):
        retry = max(retry, CONFIG['CTS_QUAL_RETRIES'])
    # Collection should never have a retry. This needs to be last.
    if modules.intersection(get_collect_modules(is_public)):
        retry = 0

    if retry >= 0:
        return retry
    # Default case omits the retries in the control file, so tradefed_test.py
    # can chose its own value.
    return None


def get_max_result_size_kb(modules, is_public):
    """Returns the maximum expected result size in kB for autotest.

    @param modules: List of CTS modules to be tested by the control file.
    """
    for module in modules:
        if (module in get_collect_modules(is_public) or
            module == 'CtsDeqpTestCases'):
            # CTS tests and dump logs for android-cts.
            return CONFIG['LARGE_MAX_RESULT_SIZE']
    # Individual module normal produces less results than all modules.
    return CONFIG['NORMAL_MAX_RESULT_SIZE']


def get_extra_args(modules, is_public):
    """Generate a list of extra arguments to pass to the test.

    Some params are specific to a particular module, particular mode or
    combination of both, generate a list of arguments to pass into the template.

    @param modules: List of CTS modules to be tested by the control file.
    """
    extra_args = set()
    preconditions = []
    login_preconditions = []
    prerequisites = []
    for module in modules:
        if is_public:
            extra_args.add('warn_on_test_retry=False')
            extra_args.add('retry_manual_tests=True')
            preconditions.extend(CONFIG['PUBLIC_PRECONDITION'].get(module, []))
        else:
            preconditions.extend(CONFIG['PRECONDITION'].get(module, []))
            login_preconditions.extend(
                CONFIG['LOGIN_PRECONDITION'].get(module, []))
            prerequisites.extend(CONFIG['PREREQUISITES'].get(module,[]))

    # Notice: we are just squishing the preconditions for all modules together
    # with duplicated command removed. This may not always be correct.
    # In such a case one should split the bookmarks in a way that the modules
    # with conflicting preconditions end up in separate control files.
    def deduped(lst):
       """Keep only the first occurrence of each element."""
       return [e for i, e in enumerate(lst) if e not in lst[0:i]]
    if preconditions:
        # To properly escape the public preconditions we need to format the list
        # manually using join.
        extra_args.add('precondition_commands=[%s]' % ', '.join(
            deduped(preconditions)))
    if login_preconditions:
        extra_args.add('login_precondition_commands=[%s]' % ', '.join(
            deduped(login_preconditions)))
    if prerequisites:
        extra_args.add("prerequisites=['%s']" % "', '".join(
            deduped(prerequisites)))
    return sorted(list(extra_args))


def get_test_length(modules):
    """ Calculate the test length based on the module name.

    To better optimize DUT's connected to moblab, it is better to run the
    longest tests and tests that require limited resources.  For these modules
    override from the default test length.

    @param module: CTS module which will be tested in the control file. If 'all'
                   is specified, the control file will runs all the tests.

    @return string: one of the specified test lengths:
                    ['FAST', 'SHORT', 'MEDIUM', 'LONG', 'LENGTHY']
    """
    length = 3  # 'MEDIUM'
    for module in modules:
        if module in CONFIG['OVERRIDE_TEST_LENGTH']:
            length = max(length, CONFIG['OVERRIDE_TEST_LENGTH'][module])
    return _TEST_LENGTH[length]


def get_test_priority(modules, is_public):
    """ Calculate the test priority based on the module name.

    On moblab run all long running tests and tests that have some unique
    characteristic at a higher priority (50).

    This optimizes the total run time of the suite assuring the shortest
    time between suite kick off and 100% complete.

    @param module: CTS module which will be tested in the control file.

    @return int: 0 if priority not to be overridden, or priority number otherwise.
    """
    if not is_public:
        return 0

    priority = 0
    overide_test_priority_dict = CONFIG.get('PUBLIC_OVERRIDE_TEST_PRIORITY', {})
    for module in modules:
        if module in overide_test_priority_dict:
            priority = max(priority, overide_test_priority_dict[module])
        elif (module in CONFIG['OVERRIDE_TEST_LENGTH'] or
                module in CONFIG['PUBLIC_DEPENDENCIES'] or
                module in CONFIG['PUBLIC_PRECONDITION'] or
                module.split('.')[0] in CONFIG['OVERRIDE_TEST_LENGTH']):
            priority = max(priority, 50)
    return priority


def get_authkey(is_public):
    if is_public or not CONFIG['AUTHKEY']:
        return None
    return CONFIG['AUTHKEY']


def _format_collect_cmd(retry):
    """Returns a list specifying tokens for tradefed to list all tests."""
    if retry:
        return None
    cmd = ['run', 'commandAndExit', 'collect-tests-only']
    if CONFIG['TRADEFED_DISABLE_REBOOT_ON_COLLECTION']:
        cmd += ['--disable-reboot']
    for m in CONFIG['MEDIA_MODULES']:
        cmd.append('--module-arg')
        cmd.append('%s:skip-media-download:true' % m)
    return cmd


def _get_special_command_line(modules, _is_public):
    """This function allows us to split a module like Deqp into segments."""
    cmd = []
    for module in sorted(modules):
        cmd += CONFIG['EXTRA_COMMANDLINE'].get(module, [])
    return cmd


def _format_modules_cmd(is_public, modules=None, retry=False):
    """Returns list of command tokens for tradefed."""
    if retry:
        assert(CONFIG['TRADEFED_RETRY_COMMAND'] == 'cts' or
               CONFIG['TRADEFED_RETRY_COMMAND'] == 'retry')

        cmd = ['run', 'commandAndExit', CONFIG['TRADEFED_RETRY_COMMAND'],
               '--retry', '{session_id}']
    else:
        # For runs create a logcat file for each individual failure.
        cmd = ['run', 'commandAndExit', CONFIG['TRADEFED_CTS_COMMAND']]

        special_cmd = _get_special_command_line(modules, is_public)
        if special_cmd:
            cmd.extend(special_cmd)
        elif _ALL in modules:
            pass
        elif len(modules) == 1:
            cmd += ['--module', list(modules)[0]]
        else:
            assert (CONFIG['TRADEFED_CTS_COMMAND'] != 'cts-instant'), \
                   'cts-instant cannot include multiple modules'
            # We run each module with its own --include-filter command/option.
            # https://source.android.com/compatibility/cts/run
            for module in sorted(modules):
                cmd += ['--include-filter', module]

        # For runs create a logcat file for each individual failure.
        # Not needed on moblab, nobody is going to look at them.
        if (not modules.intersection(CONFIG['DISABLE_LOGCAT_ON_FAILURE']) and
            not is_public and
            CONFIG['TRADEFED_CTS_COMMAND'] != 'gts'):
            cmd.append('--logcat-on-failure')

        if CONFIG['TRADEFED_IGNORE_BUSINESS_LOGIC_FAILURE']:
            cmd.append('--ignore-business-logic-failure')

    if CONFIG['TRADEFED_DISABLE_REBOOT']:
        cmd.append('--disable-reboot')
    if (CONFIG['TRADEFED_MAY_SKIP_DEVICE_INFO'] and
        not (modules.intersection(CONFIG['BVT_ARC'] + CONFIG['SMOKE'] +
             CONFIG['NEEDS_DEVICE_INFO']))):
        cmd.append('--skip-device-info')

    return cmd


def get_run_template(modules, is_public, retry=False):
    """Command to run the modules specified by a control file."""
    cmd = None
    if modules.intersection(get_collect_modules(is_public)):
        if _COLLECT in modules or _PUBLIC_COLLECT in modules:
            cmd = _format_collect_cmd(retry=retry)
        elif _ALL in modules:
            cmd = _format_modules_cmd(is_public, modules, retry=retry)
    else:
        cmd = _format_modules_cmd(is_public, modules, retry=retry)
    return cmd


def get_retry_template(modules, is_public):
    """Command to retry the failed modules as specified by a control file."""
    return get_run_template(modules, is_public, retry=True)


def get_extra_modules_dict(is_public, abi):
    if not is_public:
        return CONFIG['EXTRA_MODULES']

    extra_modules = copy.deepcopy(CONFIG['PUBLIC_EXTRA_MODULES'])
    if abi in CONFIG['EXTRA_SUBMODULE_OVERRIDE']:
        for _, submodules in extra_modules.items():
            for old, news in CONFIG['EXTRA_SUBMODULE_OVERRIDE'][abi].items():
                submodules.remove(old)
                submodules.extend(news)
    return {
        module: {
            'SUBMODULES': submodules,
            'SUITES': [CONFIG['MOBLAB_SUITE_NAME']],
        } for module, submodules in extra_modules.items()
    }


def get_extra_artifacts(modules):
    artifacts = []
    for module in modules:
        if module in CONFIG['EXTRA_ARTIFACTS']:
            artifacts += CONFIG['EXTRA_ARTIFACTS'][module]
    return artifacts


def get_extra_artifacts_host(modules):
    if not 'EXTRA_ARTIFACTS_HOST' in CONFIG:
        return

    artifacts = []
    for module in modules:
        if module in CONFIG['EXTRA_ARTIFACTS_HOST']:
            artifacts += CONFIG['EXTRA_ARTIFACTS_HOST'][module]
    return artifacts


def calculate_timeout(modules, suites):
    """Calculation for timeout of tradefed run.

    Timeout is at least one hour, except if part of BVT_ARC.
    Notice these do get adjusted dynamically by number of ABIs on the DUT.
    """
    if 'suite:bvt-arc' in suites:
        return int(3600 * CONFIG['BVT_TIMEOUT'])
    if CONFIG.get('QUAL_SUITE_NAMES') and \
            CONFIG.get('QUAL_TIMEOUT') and \
            ((set(CONFIG['QUAL_SUITE_NAMES']) & set(suites)) and \
            not (_COLLECT in modules or _PUBLIC_COLLECT in modules)):
        return int(3600 * CONFIG['QUAL_TIMEOUT'])

    timeout = 0
    # First module gets 1h (standard), all other half hour extra (heuristic).
    default_timeout = int(3600 * CONFIG['CTS_TIMEOUT_DEFAULT'])
    delta = default_timeout
    for module in modules:
        if module in CONFIG['CTS_TIMEOUT']:
            # Modules that run very long are encoded here.
            timeout += int(3600 * CONFIG['CTS_TIMEOUT'][module])
        elif module.startswith('CtsDeqpTestCases.dEQP-VK.'):
            # TODO: Optimize this temporary hack by reducing this value or
            # setting appropriate values for each test if possible.
            timeout = max(timeout, int(3600 * 12))
        elif 'Jvmti' in module:
            # We have too many of these modules and they run fast.
            timeout += 300
        else:
            timeout += delta
            delta = default_timeout // 2
    return timeout


def needs_push_media(modules):
    """Oracle to determine if to push several GB of media files to DUT."""
    if modules.intersection(set(CONFIG['NEEDS_PUSH_MEDIA'])):
        return True
    return False


def enable_default_apps(modules):
    """Oracle to determine if to enable default apps (eg. Files.app)."""
    if modules.intersection(set(CONFIG['ENABLE_DEFAULT_APPS'])):
        return True
    return False


def get_controlfile_content(combined,
                            modules,
                            abi,
                            revision,
                            build,
                            uri,
                            suites=None,
                            is_public=False,
                            camera_facing=None):
    """Returns the text inside of a control file.

    @param combined: name to use for this combination of modules.
    @param modules: list of CTS modules which will be tested in the control
                   file. If 'all' is specified, the control file will runs
                   all the tests.
    """
    # We tag results with full revision now to get result directories containing
    # the revision. This fits stainless/ better.
    tag = '%s' % get_extension(combined, abi, revision, is_public,
                               camera_facing)
    # For test_that the NAME should be the same as for the control file name.
    # We could try some trickery here to get shorter extensions for a default
    # suite/ARM. But with the monthly uprevs this will quickly get confusing.
    name = '%s.%s' % (CONFIG['TEST_NAME'], tag)
    if not suites:
        suites = get_suites(modules, abi, is_public)
    attributes = ', '.join(suites)
    uri = None if is_public else uri
    target_module = None
    if (combined not in get_collect_modules(is_public) and combined != _ALL):
        target_module = combined
    for target, config in get_extra_modules_dict(is_public, abi).items():
        if combined in config['SUBMODULES']:
            target_module = target
    return _CONTROLFILE_TEMPLATE.render(
        year=CONFIG['COPYRIGHT_YEAR'],
        name=name,
        base_name=CONFIG['TEST_NAME'],
        test_func_name=CONFIG['CONTROLFILE_TEST_FUNCTION_NAME'],
        attributes=attributes,
        dependencies=get_dependencies(
            modules,
            abi,
            is_public,
            is_camerabox_test=(camera_facing is not None)),
        extra_artifacts=get_extra_artifacts(modules),
        extra_artifacts_host=get_extra_artifacts_host(modules),
        job_retries=get_job_retries(modules, is_public),
        max_result_size_kb=get_max_result_size_kb(modules, is_public),
        revision=revision,
        build=build,
        abi=abi,
        needs_push_media=needs_push_media(modules),
        enable_default_apps=enable_default_apps(modules),
        tag=tag,
        uri=uri,
        DOC=get_doc(modules, abi, is_public),
        servo_support_needed = servo_support_needed(modules, is_public),
        max_retries=get_max_retries(modules, abi, suites, is_public),
        timeout=calculate_timeout(modules, suites),
        run_template=get_run_template(modules, is_public),
        retry_template=get_retry_template(modules, is_public),
        target_module=target_module,
        target_plan=None,
        test_length=get_test_length(modules),
        priority=get_test_priority(modules, is_public),
        extra_args=get_extra_args(modules, is_public),
        authkey=get_authkey(is_public),
        sync_count=get_sync_count(modules, abi, is_public),
        camera_facing=camera_facing)


def get_tradefed_data(path, is_public, abi):
    """Queries tradefed to provide us with a list of modules.

    Notice that the parsing gets broken at times with major new CTS drops.
    """
    tradefed = os.path.join(path, CONFIG['TRADEFED_EXECUTABLE_PATH'])
    # Forgive me for I have sinned. Same as: chmod +x tradefed.
    os.chmod(tradefed, os.stat(tradefed).st_mode | stat.S_IEXEC)
    cmd_list = [tradefed, 'list', 'modules']
    logging.info('Calling tradefed for list of modules.')
    with open(os.devnull, 'w') as devnull:
        # tradefed terminates itself if stdin is not a tty.
        tradefed_output = subprocess.check_output(cmd_list, stdin=devnull)

    # TODO(ihf): Get a tradefed command which terminates then refactor.
    p = subprocess.Popen(cmd_list, stdout=subprocess.PIPE)
    modules = set()
    build = '<unknown>'
    line = ''
    revision = None
    is_in_intaractive_mode = True
    # The process does not terminate, but we know the last test is vm-tests-tf.
    while True:
        line = p.stdout.readline().strip()
        # Android Compatibility Test Suite 7.0 (3423912)
        if (line.startswith('Android Compatibility Test Suite ') or
            line.startswith('Android Google ')):
            logging.info('Unpacking: %s.', line)
            build = get_tradefed_build(line)
            revision = get_tradefed_revision(line)
        elif line.startswith('Non-interactive mode: '):
            is_in_intaractive_mode = False
        elif line.startswith('arm') or line.startswith('x86'):
            # Newer CTS shows ABI-module pairs like "arm64-v8a CtsNetTestCases"
            modules.add(line.split()[1])
        elif line.startswith('Cts'):
            modules.add(line)
        elif line.startswith('Gts'):
            # Older GTS plainly lists the module names
            modules.add(line)
        elif line.startswith('Vts'):
            modules.add(line)
        elif line.startswith('cts-'):
            modules.add(line)
        elif line.startswith('signed-Cts'):
            modules.add(line)
        elif line.startswith('vm-tests-tf'):
            modules.add(line)
            break  # TODO(ihf): Fix using this as EOS.
        elif not line:
            exit_code = p.poll()
            if exit_code is not None:
                # The process has automatically exited.
                if is_in_intaractive_mode or exit_code != 0:
                    # The process exited unexpectedly in interactive mode,
                    # or exited with error in non-interactive mode.
                    logging.warning(
                        'The process has exited unexpectedly (exit code: %d)',
                        exit_code)
                    modules = set()
                break
        elif line.isspace() or line.startswith('Use "help"'):
            pass
        else:
            logging.warning('Ignoring "%s"', line)
    if p.poll() is None:
      # Kill the process if alive.
      p.kill()
    p.wait()

    if not modules:
      raise Exception("no modules found.")
    return list(modules), build, revision


def download(uri, destination):
    """Download |uri| to local |destination|.

       |destination| must be a file path (not a directory path)."""
    if uri.startswith('http://') or uri.startswith('https://'):
        subprocess.check_call(['wget', uri, '-O', destination])
    elif uri.startswith('gs://'):
        subprocess.check_call(['gsutil', 'cp', uri, destination])
    else:
        raise Exception


@contextlib.contextmanager
def pushd(d):
    """Defines pushd."""
    current = os.getcwd()
    os.chdir(d)
    try:
        yield
    finally:
        os.chdir(current)


def unzip(filename, destination):
    """Unzips a zip file to the destination directory."""
    with pushd(destination):
        # We are trusting Android to have a sane zip file for us.
        with zipfile.ZipFile(filename) as zf:
            zf.extractall()


def get_collect_modules(is_public):
    if is_public:
        return set([_PUBLIC_COLLECT])
    return set([_COLLECT])


@contextlib.contextmanager
def TemporaryDirectory(prefix):
    """Poor man's python 3.2 import."""
    tmp = tempfile.mkdtemp(prefix=prefix)
    try:
        yield tmp
    finally:
        shutil.rmtree(tmp)


def get_word_pattern(m, l=1):
    """Return the first few words of the CamelCase module name.

    Break after l+1 CamelCase word.
    Example: CtsDebugTestCases -> CtsDebug.
    """
    s = re.findall('^[a-z]+|[A-Z]*[^A-Z0-9]*', m)[0:l + 1]
    # Ignore Test or TestCases at the end as they don't add anything.
    if len(s) > l:
        if s[l].startswith('Test'):
            return ''.join(s[0:l])
        if s[l - 1] == 'Test' and s[l].startswith('Cases'):
            return ''.join(s[0:l - 1])
    return ''.join(s[0:l + 1])


def combine_modules_by_common_word(modules):
    """Returns a dictionary of (combined name, set of module) pairs.

    This gives a mild compaction of control files (from about 320 to 135).
    Example:
    'CtsVoice' -> ['CtsVoiceInteractionTestCases', 'CtsVoiceSettingsTestCases']
    """
    d = dict()
    # On first pass group modules with common first word together.
    for module in modules:
        pattern = get_word_pattern(module)
        v = d.get(pattern, [])
        v.append(module)
        v.sort()
        d[pattern] = v
    # Second pass extend names to maximum common prefix. This keeps control file
    # names identical if they contain only one module and less ambiguous if they
    # contain multiple modules.
    combined = dict()
    for key in sorted(d):
        # Instead if a one syllable prefix use longest common prefix of modules.
        prefix = os.path.commonprefix(d[key])
        # Beautification: strip Tests/TestCases from end of prefix, but only if
        # there is more than one module in the control file. This avoids
        # slightly strange combination of having CtsDpiTestCases1/2 inside of
        # CtsDpiTestCases (now just CtsDpi to make it clearer there are several
        # modules in this control file).
        if len(d[key]) > 1:
            prefix = re.sub('TestCases$', '', prefix)
            prefix = re.sub('Tests$', '', prefix)
        # Beautification: CtsMedia files run very long and are unstable. Give
        # each module its own control file, even though this heuristic would
        # lump them together.
        if prefix.startswith('CtsMedia'):
            for media in d[key]:
                combined[media] = set([media])
        else:
            combined[prefix] = set(d[key])
    print 'Reduced number of control files from %d to %d.' % (len(modules),
                                                              len(combined))
    return combined


def combine_modules_by_bookmark(modules):
    """Return a manually curated list of name, module pairs.

    Ideally we split "all" into a dictionary of maybe 10-20 equal runtime parts.
    (Say 2-5 hours each.) But it is ok to run problematic modules alone.
    """
    d = dict()
    # Figure out sets of modules between bookmarks. Not optimum time complexity.
    for bookmark in CONFIG['QUAL_BOOKMARKS']:
        if modules:
            for module in sorted(modules):
                if module < bookmark:
                    v = d.get(bookmark, set())
                    v.add(module)
                    d[bookmark] = v
            # Remove processed modules.
            if bookmark in d:
                modules = modules - d[bookmark]
    # Clean up names.
    combined = dict()
    for key in sorted(d):
        v = sorted(d[key])
        # New name is first element '_-_' last element.
        # Notice there is a bug in $ADB_VENDOR_KEYS path name preventing
        # arbitrary characters.
        prefix = v[0] + '_-_' + v[-1]
        combined[prefix] = set(v)
    return combined


def write_controlfile(name, modules, abi, revision, build, uri, suites,
                      is_public):
    """Write a single control file."""
    filename = get_controlfile_name(name, abi, revision, is_public)
    content = get_controlfile_content(name, modules, abi, revision, build, uri,
                                      suites, is_public)
    with open(filename, 'w') as f:
        f.write(content)


def write_moblab_controlfiles(modules, abi, revision, build, uri, is_public):
    """Write all control files for moblab.

    Nothing gets combined.

    Moblab uses one module per job. In some cases like Deqp which can run super
    long it even creates several jobs per module. Moblab can do this as it has
    less relative overhead spinning up jobs than the lab.
    """
    for module in modules:
        write_controlfile(module, set([module]), abi, revision, build, uri,
                          [CONFIG['MOBLAB_SUITE_NAME']], is_public)


def write_regression_controlfiles(modules, abi, revision, build, uri,
                                  is_public):
    """Write all control files for stainless/ToT regression lab coverage.

    Regression coverage on tot currently relies heavily on watching stainless
    dashboard and sponge. So instead of running everything in a single run
    we split CTS into many jobs. It used to be one job per module, but that
    became too much in P (more than 300 per ABI). Instead we combine modules
    with similar names and run these in the same job (alphabetically).
    """
    combined = combine_modules_by_common_word(set(modules))
    for key in combined:
        write_controlfile(key, combined[key], abi, revision, build, uri, None,
                          is_public)


def write_qualification_controlfiles(modules, abi, revision, build, uri,
                                     is_public):
    """Write all control files to run "all" tests for qualification.

    Qualification was performed on N by running all tests using tradefed
    sharding (specifying SYNC_COUNT=2) in the control files. In skylab
    this is currently not implemented, so we fall back to autotest sharding
    all CTS tests into 10-20 hand chosen shards.
    """
    combined = combine_modules_by_bookmark(set(modules))
    for key in combined:
        write_controlfile('all.' + key, combined[key], abi, revision, build,
                          uri, CONFIG.get('QUAL_SUITE_NAMES'), is_public)


def write_qualification_and_regression_controlfile(abi, revision, build, uri,
                                                   is_public):
    """Write a control file to run "all" tests for qualification and regression.
    """
    # For cts-instant, qualication control files are expected to cover
    # regressions as well. Hence the 'suite:arc-cts' is added.
    suites = ['suite:arc-cts', 'suite:arc-cts-qual']
    write_controlfile('all', set([_ALL]), abi, revision, build, uri, suites,
                      is_public)


def write_collect_controlfiles(_modules, abi, revision, build, uri, is_public):
    """Write all control files for test collection used as reference to

    compute completeness (missing tests) on the CTS dashboard.
    """
    if is_public:
        suites = [CONFIG['MOBLAB_SUITE_NAME']]
    else:
        suites = CONFIG['INTERNAL_SUITE_NAMES'] \
               + CONFIG.get('QUAL_SUITE_NAMES', [])
    for module in get_collect_modules(is_public):
        write_controlfile(module, set([module]), abi, revision, build, uri,
                          suites, is_public)


def write_extra_controlfiles(_modules, abi, revision, build, uri,
                             is_public):
    """Write all extra control files as specified in config.

    This is used by moblab to load balance large modules like Deqp, as well as
    making custom modules such as WM presubmit. A similar approach was also used
    during bringup of grunt to split media tests.
    """
    for module, config in get_extra_modules_dict(is_public, abi).items():
        for submodule in config['SUBMODULES']:
            write_controlfile(submodule, set([submodule]), abi, revision, build,
                              uri, config['SUITES'], is_public)


def write_extra_camera_controlfiles(abi, revision, build, uri, is_public):
    """Control files for CtsCameraTestCases.camerabox.*"""
    module = 'CtsCameraTestCases'
    for facing in ['back', 'front']:
        name = get_controlfile_name(module, abi,
                                    revision, is_public, facing)
        content = get_controlfile_content(module, set([module]), abi,
                                          revision, build, uri,
                                          None, is_public, facing)
        with open(name, 'w') as f:
            f.write(content)


def run(uris, is_public, cache_dir):
    """Downloads each bundle in |uris| and generates control files for each

    module as reported to us by tradefed.
    """
    for uri in uris:
        abi = get_bundle_abi(uri)
        # Get tradefed data by downloading & unzipping the files
        with TemporaryDirectory(prefix='cts-android_') as tmp:
            if cache_dir is not None:
                assert(os.path.isdir(cache_dir))
                bundle = os.path.join(cache_dir, os.path.basename(uri))
                if not os.path.exists(bundle):
                    logging.info('Downloading to %s.', cache_dir)
                    download(uri, bundle)
            else:
                bundle = os.path.join(tmp, os.path.basename(uri))
                logging.info('Downloading to %s.', tmp)
                download(uri, bundle)
            logging.info('Extracting %s.', bundle)
            unzip(bundle, tmp)
            modules, build, revision = get_tradefed_data(tmp, is_public, abi)
            if not revision:
                raise Exception('Could not determine revision.')

            logging.info('Writing all control files.')
            if is_public:
                write_moblab_controlfiles(modules, abi, revision, build, uri,
                                          is_public)
            else:
                if CONFIG['CONTROLFILE_WRITE_SIMPLE_QUAL_AND_REGRESS']:
                    write_qualification_and_regression_controlfile(
                        abi, revision, build, uri, is_public)
                else:
                    write_regression_controlfiles(modules, abi, revision, build,
                                                  uri, is_public)
                    write_qualification_controlfiles(modules, abi, revision,
                                                     build, uri, is_public)

                if CONFIG['CONTROLFILE_WRITE_CAMERA']:
                    write_extra_camera_controlfiles(abi, revision, build, uri,
                                                    is_public)

            write_collect_controlfiles(modules, abi, revision, build, uri,
                                       is_public)

            if CONFIG['CONTROLFILE_WRITE_EXTRA']:
                write_extra_controlfiles(None, abi, revision, build, uri,
                                         is_public)


def main(config):
    """ Entry method of generator """

    global CONFIG
    CONFIG = config

    logging.basicConfig(level=logging.INFO)
    parser = argparse.ArgumentParser(
        description='Create control files for a CTS bundle on GS.',
        formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument(
        'uris',
        nargs='+',
        help='List of Google Storage URIs to CTS bundles. Example:\n'
        'gs://chromeos-arc-images/cts/bundle/P/'
        'android-cts-9.0_r9-linux_x86-x86.zip')
    parser.add_argument(
        '--is_public',
        dest='is_public',
        default=False,
        action='store_true',
        help='Generate the public control files for CTS, default generate'
        ' the internal control files')
    parser.add_argument(
        '--cache_dir',
        dest='cache_dir',
        default=None,
        action='store',
        help='Cache directory for downloaded bundle file. Uses the cached '
             'bundle file if exists, or caches a downloaded file to this '
             'directory if not.')
    args = parser.parse_args()
    run(args.uris, args.is_public, args.cache_dir)
