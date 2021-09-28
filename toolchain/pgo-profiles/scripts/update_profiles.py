#!/usr/bin/env python
#
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Sample Usage:
# $ python update_profiles.py 500000 ALL --profdata-suffix 2019-04-15
#
# Additional/frequently-used arguments:
#   -b BUG adds a 'Bug: <BUG>' to the commit message when adding the profiles.
#   --do-not-merge adds a 'DO NOT MERGE' tag to the commit message to restrict
#                  automerge of profiles from release branches.
#
# Try '-h' for a full list of command line arguments.

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

import utils

from android_build_client import AndroidBuildClient


class Benchmark(object):

    def __init__(self, name):
        self.name = name

    def apct_test_tag(self):
        raise NotImplementedError()

    def profdata_file(self, suffix=''):
        profdata = os.path.join(self.name, '{}.profdata'.format(self.name))
        if suffix:
            profdata += '.' + suffix
        return profdata

    def profraw_files(self):
        raise NotImplementedError()

    def merge_profraws(self, profile_dir, output):
        profraws = [
            os.path.join(profile_dir, p)
            for p in self.profraw_files(profile_dir)
        ]
        utils.run_llvm_profdata(profraws, output)


class NativeExeBenchmark(Benchmark):

    def apct_test_tag(self):
        return 'apct/perf/pgo/profile-collector'

    def profraw_files(self, profile_dir):
        if self.name == 'hwui':
            return [
                'hwuimacro.profraw', 'hwuimacro_64.profraw',
                'hwuimicro.profraw', 'hwuimicro_64.profraw',
                'skia_nanobench.profraw', 'skia_nanobench_64.profraw'
            ]
        elif self.name == 'hwbinder':
            return ['hwbinder.profraw', 'hwbinder_64.profraw']


class APKBenchmark(Benchmark):

    def apct_test_tag(self):
        return 'apct/perf/pgo/apk-profile-collector'

    def profdata_file(self, suffix=''):
        profdata = os.path.join('art',
                                '{}_arm_arm64.profdata'.format(self.name))
        if suffix:
            profdata += '.' + suffix
        return profdata

    def profraw_files(self, profile_dir):
        return os.listdir(profile_dir)


def BenchmarkFactory(benchmark_name):
    if benchmark_name == 'dex2oat':
        return APKBenchmark(benchmark_name)
    elif benchmark_name in ['hwui', 'hwbinder']:
        return NativeExeBenchmark(benchmark_name)
    else:
        raise RuntimeError('Unknown benchmark ' + benchmark_name)


def extract_profiles(build, test_tag, build_client, output_dir):
    pgo_zip = build_client.download_pgo_zip(build, test_tag, output_dir)

    zipfile_name = os.path.join(pgo_zip)
    zip_ref = zipfile.ZipFile(zipfile_name)
    zip_ref.extractall(output_dir)
    zip_ref.close()


KNOWN_BENCHMARKS = ['ALL', 'dex2oat', 'hwui', 'hwbinder']


def parse_args():
    """Parses and returns command line arguments."""
    parser = argparse.ArgumentParser()

    parser.add_argument(
        'build',
        metavar='BUILD',
        help='Build number to pull from the build server.')

    parser.add_argument(
        '-b', '--bug', type=int, help='Bug to reference in commit message.')

    parser.add_argument(
        '--use-current-branch',
        action='store_true',
        help='Do not repo start a new branch for the update.')

    parser.add_argument(
        '--add-do-not-merge',
        action='store_true',
        help='Add \'DO NOT MERGE\' to the commit message.')

    parser.add_argument(
        '--profdata-suffix',
        type=str,
        default='',
        help='Suffix to append to merged profdata file')

    parser.add_argument(
        'benchmark',
        metavar='BENCHMARK',
        help='Update profiles for BENCHMARK.  Choices are {}'.format(
            KNOWN_BENCHMARKS))

    parser.add_argument(
        '--skip-cleanup',
        '-sc',
        action='store_true',
        default=False,
        help='Skip the cleanup, and leave intermediate files (in /tmp/pgo-profiles-*)'
    )

    return parser.parse_args()


def get_current_profile(benchmark):
    profile = benchmark.profdata_file()
    dirname, basename = os.path.split(profile)

    old_profiles = [f for f in os.listdir(dirname) if f.startswith(basename)]
    if len(old_profiles) == 0:
        return ''
    return os.path.join(dirname, old_profiles[0])


def main():
    args = parse_args()

    if args.benchmark == 'ALL':
        worklist = KNOWN_BENCHMARKS[1:]
    else:
        worklist = [args.benchmark]

    profiles_project = os.path.join(utils.android_build_top(), 'toolchain',
                                    'pgo-profiles')
    os.chdir(profiles_project)

    if not args.use_current_branch:
        branch_name = 'update-profiles-' + args.build
        utils.check_call(['repo', 'start', branch_name, '.'])

    build_client = AndroidBuildClient()

    for benchmark_name in worklist:
        benchmark = BenchmarkFactory(benchmark_name)

        # Existing profile file, which gets 'rm'-ed from 'git' down below.
        current_profile = get_current_profile(benchmark)

        # Extract profiles to a temporary directory.  After extraction, we
        # expect to find one subdirectory with profraw files under the temporary
        # directory.
        extract_dir = tempfile.mkdtemp(prefix='pgo-profiles-' + benchmark_name)
        extract_profiles(args.build, benchmark.apct_test_tag(), build_client,
                         extract_dir)

        extract_subdirs = [
            os.path.join(extract_dir, sub)
            for sub in os.listdir(extract_dir)
            if os.path.isdir(os.path.join(extract_dir, sub))
        ]
        if len(extract_subdirs) != 1:
            raise RuntimeError(
                'Expected one subdir under {}'.format(extract_dir))

        # Merge profiles.
        profdata = benchmark.profdata_file(args.profdata_suffix)
        benchmark.merge_profraws(extract_subdirs[0], profdata)

        # Construct 'git' commit message.
        message_lines = [
            'Update PGO profiles for {}'.format(benchmark_name), '',
            'The profiles are from build {}.'.format(args.build), ''
        ]

        if args.add_do_not_merge:
            message_lines[0] = '[DO NOT MERGE] ' + message_lines[0]

        if args.bug:
            message_lines.append('')
            message_lines.append('Bug: http://b/{}'.format(args.bug))
        message_lines.append('Test: Build (TH)')
        message = '\n'.join(message_lines)

        # Invoke git: Delete current profile, add new profile and commit these
        # changes.
        if current_profile:
            utils.check_call(['git', 'rm', current_profile])
        utils.check_call(['git', 'add', profdata])
        utils.check_call(['git', 'commit', '-m', message])

        if not args.skip_cleanup:
            shutil.rmtree(extract_dir)


if __name__ == '__main__':
    main()
