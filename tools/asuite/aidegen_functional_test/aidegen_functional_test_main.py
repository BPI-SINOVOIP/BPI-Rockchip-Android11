#!/usr/bin/env python3
#
# Copyright 2018 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Functional test for aidegen project files."""

from __future__ import absolute_import

import argparse
import functools
import itertools
import json
import logging
import os
import subprocess
import sys
import xml.etree.ElementTree
import xml.parsers.expat

from aidegen import aidegen_main
from aidegen import constant
from aidegen.lib import clion_project_file_gen
from aidegen.lib import common_util
from aidegen.lib import errors
from aidegen.lib import module_info_util
from aidegen.lib import project_config
from aidegen.lib import project_file_gen

from atest import module_info

_PRODUCT_DIR = '$PROJECT_DIR$'
_ROOT_DIR = os.path.join(common_util.get_android_root_dir(),
                         'tools/asuite/aidegen_functional_test')
_TEST_DATA_PATH = os.path.join(_ROOT_DIR, 'test_data')
_VERIFY_COMMANDS_JSON = os.path.join(_TEST_DATA_PATH, 'verify_commands.json')
_GOLDEN_SAMPLES_JSON = os.path.join(_TEST_DATA_PATH, 'golden_samples.json')
_VERIFY_BINARY_JSON = os.path.join(_TEST_DATA_PATH, 'verify_binary_upload.json')
_ANDROID_COMMON = 'android_common'
_LINUX_GLIBC_COMMON = 'linux_glibc_common'
_SRCS = 'srcs'
_JARS = 'jars'
_URL = 'url'
_TEST_ERROR = 'AIDEGen functional test error: {}-{} is different.'
_MSG_NOT_IN_PROJECT_FILE = ('{} is expected, but not found in the created '
                            'project file: {}')
_MSG_NOT_IN_SAMPLE_DATA = ('{} is unexpected, but found in the created project '
                           'file: {}')
_ALL_PASS = 'All tests passed!'
_GIT_COMMIT_ID_JSON = os.path.join(
    _TEST_DATA_PATH, 'baseline_code_commit_id.json')
_GIT = 'git'
_CHECKOUT = 'checkout'
_BRANCH = 'branch'
_COMMIT = 'commit'
_LOG = 'log'
_ALL = '--all'
_COMMIT_ID_NOT_EXIST_ERROR = ('Commit ID: {} does not exist in path: {}. '
                              'Please use "git log" command to check if it '
                              'exists. If it does not, try to update your '
                              'source files to the latest version or do not '
                              'use "repo init --depth=1" command.')
_GIT_LOG_ERROR = 'Command "git log -n 1" failed.'
_BE_REPLACED = '${config.X86_64GccRoot}'
_TO_REPLACE = 'prebuilts/gcc/linux-x86/x86/x86_64-linux-android-4.9'


def _parse_args(args):
    """Parse command line arguments.

    Args:
        args: A list of arguments.

    Returns:
        An argparse.Namespace object holding parsed args.
    """
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        usage='aidegen_functional_test [-c | -u | -b | -a] -v -r')
    group = parser.add_mutually_exclusive_group()
    parser.required = False
    parser.add_argument(
        'targets',
        type=str,
        nargs='*',
        default=[''],
        help='Android module name or path.e.g. frameworks/base')
    group.add_argument(
        '-c',
        '--create-sample',
        action='store_true',
        dest='create_sample',
        help=('Create aidegen project files and write data to sample json file '
              'for aidegen_functional_test to compare.'))
    parser.add_argument(
        '-v',
        '--verbose',
        action='store_true',
        help='Show DEBUG level logging.')
    parser.add_argument(
        '-r',
        '--remove_bp_json',
        action='store_true',
        help='Remove module_bp_java_deps.json for each use case test.')
    group.add_argument(
        '-u',
        '--use_cases',
        action='store_true',
        dest='use_cases_verified',
        help='Verify various use cases of executing aidegen.')
    group.add_argument(
        '-b',
        action='store_true',
        dest='binary_upload_verified',
        help=('Verify aidegen\'s use cases by executing different aidegen '
              'commands.'))
    group.add_argument(
        '-a',
        '--test-all',
        action='store_true',
        dest='test_all_samples',
        help='Test all modules listed in module-info.json.')
    group.add_argument(
        '-n',
        '--compare-sample-native',
        action='store_true',
        dest='compare_sample_native',
        help=('Compare if sample native project file is the same as generated '
              'by the build system.'))
    return parser.parse_args(args)


def _import_project_file_xml_etree(filename):
    """Import iml project file and load its data into a dictionary.

    Args:
        filename: The input project file name.

    Returns:
        A dictionary contains dependent files' data of project file's contents.
        The samples are like:
        "srcs": [
            ...
            "file://$PROJECT_DIR$/frameworks/base/cmds/am/src",
            "file://$PROJECT_DIR$/frameworks/base/cmds/appwidget/src",
            ...
        ]
        "jars": [
            ...
            "jar://$PROJECT_DIR$/out/host/common/obj/**/classes-header.jar!/"
            ...
        ]

    Raises:
        EnvironmentError and xml parsing or format errors.
    """
    data = {}
    try:
        tree = xml.etree.ElementTree.parse(filename)
        data[_SRCS] = []
        root = tree.getroot()
        for element in root.iter('sourceFolder'):
            src = element.get(_URL).replace(common_util.get_android_root_dir(),
                                            _PRODUCT_DIR)
            data[_SRCS].append(src)
        data[_JARS] = []
        for element in root.iter('root'):
            jar = element.get(_URL).replace(common_util.get_android_root_dir(),
                                            _PRODUCT_DIR)
            data[_JARS].append(jar)
    except (EnvironmentError, ValueError, LookupError,
            xml.parsers.expat.ExpatError) as err:
        print("{0}: import error: {1}".format(os.path.basename(filename), err))
        raise
    return data


def _get_project_file_names(abs_path):
    """Get project file name and depenencies name by relative path.

    Args:
        abs_path: an absolute module's path.

    Returns:
        file_name: a string of the project file name.
        dep_name: a string of the merged project and dependencies file's name,
                  e.g., frameworks-dependencies.iml.
    """
    code_name = project_file_gen.ProjectFileGenerator.get_unique_iml_name(
        abs_path)
    file_name = ''.join([code_name, '.iml'])
    dep_name = ''.join([constant.KEY_DEPENDENCIES, '.iml'])
    return file_name, dep_name


def _get_unique_module_name(rel_path, filename):
    """Get a unique project file name or dependencies name by relative path.

    Args:
        rel_path: a relative module's path to aosp root path.
        filename: the file name to be generated in module_in type file name.

    Returns:
        A string, the unique file name for the whole module-info.json.
    """
    code_names = rel_path.split(os.sep)
    code_names[-1] = filename
    return '-'.join(code_names)


def _get_git_current_commit_id(abs_path):
    """Get target's git checkout command list.

    When we run command 'git log -n 1' and get the top first git log record, the
    commit id is next to the specific word 'commit'.

    Args:
        abs_path: a string of the absolute path of the target branch.

    Return:
        The current git commit id.

    Raises:
        Call subprocess.check_output cause subprocess.CalledProcessError.
    """
    cwd = os.getcwd()
    os.chdir(abs_path)
    git_log_cmds = [_GIT, _LOG, '-n', '1']
    try:
        out_put = subprocess.check_output(git_log_cmds).decode("utf-8")
    except subprocess.CalledProcessError:
        logging.error(_GIT_LOG_ERROR)
        raise
    finally:
        os.chdir(cwd)
    com_list = out_put.split()
    return com_list[com_list.index(_COMMIT) + 1]


def _get_commit_id_dictionary():
    """Get commit id from dictionary of key, value 'module': 'commit id'."""
    data_id_dict = {}
    with open(_GIT_COMMIT_ID_JSON, 'r') as jsfile:
        data_id_dict = json.load(jsfile)
    return data_id_dict


def _git_checkout_commit_id(abs_path, commit_id):
    """Command to checkout specific commit id.

    Change directory to the module's absolute path which users want to get its
    specific commit id.

    Args:
        abs_path: the absolute path of the target branch. E.g., abs_path/.git
        commit_id: the commit id users want to checkout.

    Raises:
        Call git checkout commit id failed, raise errors.CommitIDNotExistError.
    """
    git_chekout_cmds = [_GIT, _CHECKOUT, commit_id]
    cwd = os.getcwd()
    try:
        os.chdir(abs_path)
        subprocess.check_output(git_chekout_cmds)
    except subprocess.CalledProcessError:
        err = _COMMIT_ID_NOT_EXIST_ERROR.format(commit_id, abs_path)
        logging.error(err)
        raise errors.CommitIDNotExistError(err)
    finally:
        os.chdir(cwd)


def _git_checkout_target_commit_id(target, commit_id):
    """Command to checkout target commit id.

    Switch code base to specific commit id which is kept in data_id_dict with
    target: commit_id as key: value. If the data is missing in data_id_dict, the
    target isn't a selected golden sample raise error for it.

    Args:
        target: the string of target's module name or module path to checkout
                the relevant git to its specific commit id.
        commit_id: a string represent target's specific commit id.

    Returns:
        current_commit_id: the current commit id of the target which should be
            switched back to.
    """
    atest_module_info = module_info.ModuleInfo()
    _, abs_path = common_util.get_related_paths(atest_module_info, target)
    current_commit_id = _get_git_current_commit_id(abs_path)
    _git_checkout_commit_id(abs_path, commit_id)
    return current_commit_id


def _checkout_baseline_code_to_spec_commit_id():
    """Get a dict of target, commit id listed in baseline_code_commit_id.json.

    To make sure all samples run on the same environment, we need to keep all
    the baseline code in a specific commit id. For example, all samples should
    be created in the same specific commit id of the baseline code
    'frameworks/base' for comparison except 'frameworks/base' itself.

    Returns:
        A dictionary contains target, specific and current commit id.
    """
    spec_and_cur_commit_id_dict = {}
    data_id_dict = _get_commit_id_dictionary()
    for target in data_id_dict:
        commit_id = data_id_dict.get(target, '')
        current_commit_id = _git_checkout_target_commit_id(target, commit_id)
        spec_and_cur_commit_id_dict[target] = {}
        spec_and_cur_commit_id_dict[target]['current'] = current_commit_id
    return spec_and_cur_commit_id_dict


def _generate_target_real_iml_data(target):
    """Generate single target's real iml file content's data.

    Args:
        target: Android module name or path to be create iml data.

    Returns:
        data: A dictionary contains key: unique file name and value: iml
              content.
    """
    data = {}
    try:
        aidegen_main.main([target, '-s', '-n', '-v'])
    except (errors.FakeModuleError,
            errors.ProjectOutsideAndroidRootError,
            errors.ProjectPathNotExistError,
            errors.NoModuleDefinedInModuleInfoError) as err:
        logging.error(str(err))
        return data

    atest_module_info = module_info.ModuleInfo()
    rel_path, abs_path = common_util.get_related_paths(atest_module_info,
                                                       target)
    for filename in iter(_get_project_file_names(abs_path)):
        real_iml_file = os.path.join(abs_path, filename)
        item_name = _get_unique_module_name(rel_path, filename)
        data[item_name] = _import_project_file_xml_etree(real_iml_file)
    return data


def _generate_sample_json(test_list):
    """Generate sample iml data.

    We use all baseline code samples on the version of their own specific commit
    id which is kept in _GIT_COMMIT_ID_JSON file. We need to switch back to
    their current commit ids after generating golden samples' data.

    Args:
        test_list: a list of module name and module path.
    Returns:
        data: a dictionary contains dependent files' data of project file's
              contents.
        The sample is like:
            "frameworks-base.iml": {
                "srcs": [
                    ....
                    "file://$PROJECT_DIR$/frameworks/base/cmds/am/src",
                    "file://$PROJECT_DIR$/frameworks/base/cmds/appwidget/src",
                    ....
                ],
                "jars": [
                    ....
                    "jar://$PROJECT_DIR$/out/target/common/**/aapt2.srcjar!/",
                    ....
                ]
            }
    """
    _make_clean()
    data = {}
    spec_and_cur_commit_id_dict = _checkout_baseline_code_to_spec_commit_id()
    for target in test_list:
        data.update(_generate_target_real_iml_data(target))
    atest_module_info = module_info.ModuleInfo()
    for target in spec_and_cur_commit_id_dict:
        _, abs_path = common_util.get_related_paths(atest_module_info, target)
        _git_checkout_commit_id(
            abs_path, spec_and_cur_commit_id_dict[target]['current'])
    return data


def _create_some_sample_json_file(targets):
    """Write some samples' iml data into a json file.

    Args:
        targets: Android module name or path to be create iml data.

    linked_function: _generate_sample_json()
    """
    data = _generate_sample_json(targets)
    data_sample = {}
    with open(_GOLDEN_SAMPLES_JSON, 'r') as infile:
        try:
            data_sample = json.load(infile)
        except json.JSONDecodeError as err:
            print("Json decode error: {}".format(err))
            data_sample = {}
    data_sample.update(data)
    with open(_GOLDEN_SAMPLES_JSON, 'w') as outfile:
        json.dump(data_sample, outfile, indent=4, sort_keys=False)


def test_samples(func):
    """Decorate a function to deal with preparing and verifying staffs of it.

    Args:
        func: a function is to be compared its iml data with the json file's
              data.

    Returns:
        The wrapper function.
    """

    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        """A wrapper function."""

        test_successful = True
        with open(_GOLDEN_SAMPLES_JSON, 'r') as outfile:
            data_sample = json.load(outfile)

        data_real = func(*args, **kwargs)

        for name in data_real:
            for item in [_SRCS, _JARS]:
                s_items = data_sample[name][item]
                r_items = data_real[name][item]
                if set(s_items) != set(r_items):
                    diff_iter = _compare_content(name, item, s_items, r_items)
                    if diff_iter:
                        print('\n{} {}'.format(
                            common_util.COLORED_INFO('Test error:'),
                            _TEST_ERROR.format(name, item)))
                        print('{} {} contents are different:'.format(
                            name, item))
                        for diff in diff_iter:
                            print(diff)
                        test_successful = False
        if test_successful:
            print(common_util.COLORED_PASS(_ALL_PASS))
        return test_successful

    return wrapper


@test_samples
def _test_some_sample_iml(targets=None):
    """Compare with sample iml's data to assure the project's contents is right.

    Args:
        targets: Android module name or path to be create iml data.
    """
    if targets:
        return _generate_sample_json(targets)
    return _generate_sample_json(_get_commit_id_dictionary().keys())


@test_samples
def _test_all_samples_iml():
    """Compare all imls' data with all samples' data.

    It's to make sure each iml's contents is right. The function is implemented
    but hasn't been used yet.
    """
    all_module_list = module_info.ModuleInfo().name_to_module_info.keys()
    return _generate_sample_json(all_module_list)


def _compare_content(module_name, item_type, s_items, r_items):
    """Compare src or jar files' data of two dictionaries.

    Args:
        module_name: the test module name.
        item_type: the type is src or jar.
        s_items: sample jars' items.
        r_items: real jars' items.

    Returns:
        An iterator of not equal sentences after comparison.
    """
    if item_type == _SRCS:
        cmp_iter1 = _compare_srcs_content(module_name, s_items, r_items,
                                          _MSG_NOT_IN_PROJECT_FILE)
        cmp_iter2 = _compare_srcs_content(module_name, r_items, s_items,
                                          _MSG_NOT_IN_SAMPLE_DATA)
    else:
        cmp_iter1 = _compare_jars_content(module_name, s_items, r_items,
                                          _MSG_NOT_IN_PROJECT_FILE)
        cmp_iter2 = _compare_jars_content(module_name, r_items, s_items,
                                          _MSG_NOT_IN_SAMPLE_DATA)
    return itertools.chain(cmp_iter1, cmp_iter2)


def _compare_srcs_content(module_name, s_items, r_items, msg):
    """Compare src or jar files' data of two dictionaries.

    Args:
        module_name: the test module name.
        s_items: sample jars' items.
        r_items: real jars' items.
        msg: the message will be written into log file.

    Returns:
        An iterator of not equal sentences after comparison.
    """
    for sample in s_items:
        if sample not in r_items:
            yield msg.format(sample, module_name)


def _compare_jars_content(module_name, s_items, r_items, msg):
    """Compare src or jar files' data of two dictionaries.

    AIDEGen treats the jars in folder names 'linux_glib_common' and
    'android_common' as the same content. If the paths are different only
    because of these two names, we ignore it.

    Args:
        module_name: the test module name.
        s_items: sample jars' items.
        r_items: real jars' items.
        msg: the message will be written into log file.

    Returns:
        An iterator of not equal sentences after comparison.
    """
    for sample in s_items:
        if sample not in r_items:
            lnew = sample
            if constant.LINUX_GLIBC_COMMON in sample:
                lnew = sample.replace(constant.LINUX_GLIBC_COMMON,
                                      constant.ANDROID_COMMON)
            else:
                lnew = sample.replace(constant.ANDROID_COMMON,
                                      constant.LINUX_GLIBC_COMMON)
            if not lnew in r_items:
                yield msg.format(sample, module_name)


# pylint: disable=broad-except
# pylint: disable=eval-used
@common_util.back_to_cwd
@common_util.time_logged
def _verify_aidegen(verified_file_path, forced_remove_bp_json):
    """Verify various use cases of executing aidegen.

    There are two types of running commands:
    1. Use 'eval' to run the commands for present codes in aidegen_main.py,
       such as:
           aidegen_main.main(['tradefed', '-n', '-v'])
    2. Use 'subprocess.check_call' to run the commands for the binary codes of
       aidegen such as:
       aidegen tradefed -n -v

    Remove module_bp_java_deps.json in the beginning of running use cases. If
    users need to remove module_bp_java_deps.json between each use case they
    can set forced_remove_bp_json true.

    Args:
        verified_file_path: The json file path to be verified.
        forced_remove_bp_json: Remove module_bp_java_deps.json for each use case
                               test.

    Raises:
        There are two type of exceptions:
        1. aidegen.lib.errors for projects' or modules' issues such as,
           ProjectPathNotExistError.
        2. Any exceptions other than aidegen.lib.errors such as,
           subprocess.CalledProcessError.
    """
    bp_json_path = common_util.get_blueprint_json_path(
        constant.BLUEPRINT_JAVA_JSONFILE_NAME)
    use_eval = (verified_file_path == _VERIFY_COMMANDS_JSON)
    try:
        with open(verified_file_path, 'r') as jsfile:
            data = json.load(jsfile)
    except IOError as err:
        raise errors.JsonFileNotExistError(
            '%s does not exist, error: %s.' % (verified_file_path, err))

    _make_clean()

    _compare_sample_native_content()
    os.chdir(common_util.get_android_root_dir())
    for use_case in data:
        print('Use case "{}" is running.'.format(use_case))
        if forced_remove_bp_json and os.path.exists(bp_json_path):
            os.remove(bp_json_path)
        for cmd in data[use_case]:
            print('Command "{}" is running.'.format(cmd))
            try:
                if use_eval:
                    eval(cmd)
                else:
                    subprocess.check_call(cmd, shell=True)
            except (errors.ProjectOutsideAndroidRootError,
                    errors.ProjectPathNotExistError,
                    errors.NoModuleDefinedInModuleInfoError,
                    errors.IDENotExistError) as err:
                print('"{}" raises error: {}.'.format(use_case, err))
                continue
            except BaseException:
                exc_type, _, _ = sys.exc_info()
                print('"{}.{}" command {}.'.format(
                    use_case, cmd, common_util.COLORED_FAIL('executes failed')))
                raise BaseException(
                    'Unexpected command "{}" exception: {}.'.format(
                        use_case, exc_type))
        print('"{}" command {}!'.format(
            use_case, common_util.COLORED_PASS('test passed')))
    print(common_util.COLORED_PASS(_ALL_PASS))


@common_util.back_to_cwd
def _make_clean():
    """Make a command to clean out folder for a pure environment to test.

    Raises:
        Call subprocess.check_call to execute
        'build/soong/soong_ui.bash --make-mode clean' and cause
        subprocess.CalledProcessError.
    """
    try:
        os.chdir(common_util.get_android_root_dir())
        subprocess.check_call(
            ['build/soong/soong_ui.bash --make-mode clean', '-j'],
            shell=True)
    except subprocess.CalledProcessError:
        print('"build/soong/soong_ui.bash --make-mode clean" command failed.')
        raise


def _read_file_content(path):
    """Read file's content.

    Args:
        path: Path of input file.

    Returns:
        A list of content strings.
    """
    with open(path, encoding='utf8') as template:
        contents = []
        for cnt in template:
            if cnt.startswith('#'):
                continue
            contents.append(cnt.rstrip())
        return contents


# pylint: disable=protected-access
def _compare_sample_native_content():
    """Compares 'libui' sample module's project file.

    Compares 'libui' sample module's project file generated by AIDEGen with that
    generated by the soong build system. Check if their contents are the same.
    There should be only one different:
        ${config.X86_64GccRoot} # in the soong build sytem
        becomes
        prebuilts/gcc/linux-x86/x86/x86_64-linux-android-4.9 # in AIDEGen
    """
    env_off = {'SOONG_COLLECT_JAVA_DEPS': 'false'}
    target_arch_variant = 'x86_64'
    env_on = {
        'TARGET_PRODUCT': 'aosp_x86_64',
        'TARGET_BUILD_VARIANT': 'eng',
        'TARGET_ARCH_VARIANT': target_arch_variant,
        'SOONG_COLLECT_JAVA_DEPS': 'true',
        'SOONG_GEN_CMAKEFILES': '1',
        'SOONG_GEN_CMAKEFILES_DEBUG': '0',
        'SOONG_COLLECT_CC_DEPS': '1'
    }

    try:
        project_config.ProjectConfig(
            aidegen_main._parse_args(['-n', '-v'])).init_environment()
        module_info_util.generate_merged_module_info(env_off, env_on)
        cc_path = os.path.join(common_util.get_soong_out_path(),
                               constant.BLUEPRINT_CC_JSONFILE_NAME)
        mod_name = 'libui'
        mod_info = common_util.get_json_dict(cc_path)['modules'][mod_name]
        if mod_info:
            clion_project_file_gen.CLionProjectFileGenerator(
                mod_info).generate_cmakelists_file()
            out_dir = os.path.join(common_util.get_android_root_dir(),
                                   common_util.get_android_out_dir(),
                                   constant.RELATIVE_NATIVE_PATH,
                                   mod_info['path'][0])
            content1 = _read_file_content(os.path.join(
                out_dir, mod_name, constant.CLION_PROJECT_FILE_NAME))
            cc_file_name = ''.join(
                [mod_name, '-', target_arch_variant, '-android'])
            cc_file_path = os.path.join(
                out_dir, cc_file_name, constant.CLION_PROJECT_FILE_NAME)
            content2 = _read_file_content(cc_file_path)
            same = True
            for lino, (cnt1, cnt2) in enumerate(
                    zip(content1, content2), start=1):
                if _BE_REPLACED in cnt2:
                    cnt2 = cnt2.replace(_BE_REPLACED, _TO_REPLACE)
                if cnt1 != cnt2:
                    print('Contents {} and {} are different in line:{}.'.format(
                        cnt1, cnt2, lino))
                    same = False
            if same:
                print('Files {} and {} are the same.'.format(
                    mod_name, cc_file_name))
    except errors.BuildFailureError:
        print('Compare native content failed.')


def main(argv):
    """Main entry.

    1. Create the iml file data of each module in module-info.json and write it
       into single_module.json.
    2. Verify every use case of AIDEGen.
    3. Compare all or some iml project files' data to the data recorded in
       single_module.json.

    Args:
        argv: A list of system arguments.
    """
    args = _parse_args(argv)
    common_util.configure_logging(args.verbose)
    os.environ[constant.AIDEGEN_TEST_MODE] = 'true'
    if args.create_sample:
        _create_some_sample_json_file(args.targets)
    elif args.use_cases_verified:
        _verify_aidegen(_VERIFY_COMMANDS_JSON, args.remove_bp_json)
    elif args.binary_upload_verified:
        _verify_aidegen(_VERIFY_BINARY_JSON, args.remove_bp_json)
    elif args.test_all_samples:
        _test_all_samples_iml()
    elif args.compare_sample_native:
        _compare_sample_native_content()
    else:
        if not args.targets[0]:
            _test_some_sample_iml()
        else:
            _test_some_sample_iml(args.targets)
    del os.environ[constant.AIDEGEN_TEST_MODE]


if __name__ == '__main__':
    main(sys.argv[1:])
