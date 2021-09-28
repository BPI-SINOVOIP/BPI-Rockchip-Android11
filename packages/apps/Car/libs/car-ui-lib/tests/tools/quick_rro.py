#!/usr/bin/env python
#
# Copyright 2019, The Android Open Source Project
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

from argparse import ArgumentParser as AP, RawDescriptionHelpFormatter
import os
import sys
import re
import subprocess
import time
from hashlib import sha1

def hex_to_letters(hex):
    """Converts numbers in a hex string to letters.

    Example: 0beec7b5 -> aBEEChBf"""
    hex = hex.upper()
    chars = []
    for char in hex:
        if ord('0') <= ord(char) <= ord('9'):
            # Convert 0-9 to a-j
            chars.append(chr(ord(char) - ord('0') + ord('a')))
        else:
            chars.append(char)
    return ''.join(chars)

def get_package_name(args):
    """Generates a package name for the quickrro.

    The name is quickrro.<hash>. The hash is based on
    all of the inputs to the RRO. (package to overlay and resources)
    The hash will be entirely lowercase/uppercase letters, since
    android package names can't have numbers."""
    hash = None
    if args.resources is not None:
        args.resources.sort()
        hash = sha1(''.join(args.resources) + args.package)
    else:
        hash = sha1(args.package)
        for root, dirs, files in os.walk(args.dir):
            for file in files:
                path = os.path.join(root, file)
                hash.update(path)
                with open(path, 'rb') as f:
                    while True:
                        buf = f.read(4096)
                        if not buf:
                            break
                        hash.update(buf)

    result = 'quickrro.' + hex_to_letters(hash.hexdigest())
    return result

def run_command(command_args):
    """Returns the stdout of a command, and throws an exception if the command fails"""
    result = subprocess.Popen(command_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = result.communicate()

    stdout = str(stdout)
    stderr = str(stderr)

    if result.returncode != 0:
        err = 'command failed: ' + ' '.join(command_args)
        if len(stdout) > 0:
            err += '\n' + stdout.strip()
        if len(stderr) > 0:
            err += '\n' + stderr.strip()
        raise Exception(err)

    return stdout

def get_android_dir_priority(dir):
    """Given the name of a directory under ~/Android/Sdk/platforms, returns an integer priority.

    The directory with the highest priority will be used. Currently android-stable is higest,
    and then after that the api level is the priority. eg android-28 has priority 28."""
    if len(dir) == 0:
        return -1
    if 'stable' in dir:
        return 999

    try:
        return int(dir.split('-')[1])
    except Exception:
        pass

    return 0

def find_android_jar(path=None):
    """Returns the path to framework-res.apk or android.jar, throwing an Exception when not found.

    First looks in the given path. Then looks in $OUT/system/framework/framework-res.apk.
    Finally, looks in ~/Android/Sdk/platforms."""
    if path is not None:
        if os.path.isfile(path):
            return path
        else:
            raise Exception('Invalid path: ' + path)

    framework_res_path = os.path.join(os.environ['OUT'], 'system/framework/framework-res.apk')
    if os.path.isfile(framework_res_path):
        return framework_res_path

    sdk_dir = os.path.expanduser('~/Android/Sdk/platforms')
    best_dir = ''
    for dir in os.listdir(sdk_dir):
        if os.path.isdir(os.path.join(sdk_dir, dir)):
            if get_android_dir_priority(dir) > get_android_dir_priority(best_dir):
                best_dir = dir

    if len(best_dir) == 0:
        raise Exception("Couldn't find android.jar")

    android_jar_path = os.path.join(sdk_dir, best_dir, 'android.jar')

    if not os.path.isfile(android_jar_path):
        raise Exception("Couldn't find android.jar")

    return android_jar_path

def uninstall_all():
    """Uninstall all RROs starting with 'quickrro'"""
    packages = re.findall('quickrro[a-zA-Z.]+',
               run_command(['adb', 'shell', 'cmd', 'overlay', 'list']))

    for package in packages:
        print('Uninstalling ' + package)
        run_command(['adb', 'uninstall', package])

    if len(packages) == 0:
        print('No quick RROs to uninstall')

def delete_flat_files(path):
    """Deletes all .flat files under `path`"""
    for filename in os.listdir(path):
        if filename.endswith('.flat'):
            os.remove(os.path.join(path, filename))

def build(args, package_name):
    """Builds the RRO apk"""
    try:
        android_jar_path = find_android_jar(args.I)
    except:
        print('Unable to find framework-res.apk / android.jar. Please build android, '
              'install an SDK via android studio, or supply a valid -I')
        sys.exit(1)

    print('Building...')
    root_folder = os.path.join(args.workspace, 'quick_rro')
    manifest_file = os.path.join(root_folder, 'AndroidManifest.xml')
    resource_folder = args.dir or os.path.join(root_folder, 'res')
    unsigned_apk = os.path.join(root_folder, package_name + '.apk.unsigned')
    signed_apk = os.path.join(root_folder, package_name + '.apk')

    if not os.path.exists(root_folder):
        os.makedirs(root_folder)

    if args.resources is not None:
        values_folder = os.path.join(resource_folder, 'values')
        resource_file = os.path.join(values_folder, 'values.xml')

        if not os.path.exists(values_folder):
            os.makedirs(values_folder)

        resources = map(lambda x: x.split(','), args.resources)
        for resource in resources:
            if len(resource) != 3:
                print("Resource format is type,name,value")
                sys.exit(1)

        with open(resource_file, 'w') as f:
            f.write('<?xml version="1.0" encoding="utf-8"?>\n')
            f.write('<resources>\n')
            for resource in resources:
                f.write('  <item type="' + resource[0] + '" name="'
                        + resource[1] + '">' + resource[2] + '</item>\n')
            f.write('</resources>\n')

    with open(manifest_file, 'w') as f:
        f.write('<?xml version="1.0" encoding="utf-8"?>\n')
        f.write('<manifest xmlns:android="http://schemas.android.com/apk/res/android"\n')
        f.write('          package="' + package_name + '">\n')
        f.write('    <application android:hasCode="false"/>\n')
        f.write('    <overlay android:priority="99"\n')
        f.write('             android:targetPackage="' + args.package + '"/>\n')
        f.write('</manifest>\n')

    run_command(['aapt2', 'compile', '-o', os.path.join(root_folder, 'compiled.zip'),
                 '--dir', resource_folder])

    delete_flat_files(root_folder)

    run_command(['unzip', os.path.join(root_folder, 'compiled.zip'),
                 '-d', root_folder])

    link_command = ['aapt2', 'link', '--auto-add-overlay',
                    '-o', unsigned_apk, '--manifest', manifest_file,
                    '-I', android_jar_path]
    for filename in os.listdir(root_folder):
        if filename.endswith('.flat'):
            link_command.extend(['-R', os.path.join(root_folder, filename)])
    run_command(link_command)

    # For some reason signapk.jar requires a relative path to out/soong/host/linux-x86/lib64
    os.chdir(os.environ['ANDROID_BUILD_TOP'])
    run_command(['java', '-Djava.library.path=out/soong/host/linux-x86/lib64',
                 '-jar', 'out/soong/host/linux-x86/framework/signapk.jar',
                 'build/target/product/security/platform.x509.pem',
                 'build/target/product/security/platform.pk8',
                 unsigned_apk, signed_apk])

    # No need to delete anything, but the unsigned apks might take a lot of space
    try:
        run_command(['rm', unsigned_apk])
    except Exception:
        pass

    print('Built ' + signed_apk)

def main():
    parser = AP(description="Create and deploy a RRO (Runtime Resource Overlay)",
                epilog='Examples:\n'
                       '   quick_rro.py -r bool,car_ui_scrollbar_enable,false\n'
                       '   quick_rro.py -r bool,car_ui_scrollbar_enable,false'
                       ' -p com.android.car.ui.paintbooth\n'
                       '   quick_rro.py -d vendor/auto/embedded/car-ui/sample1/rro/res\n'
                       '   quick_rro.py --uninstall-all\n',
                formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('-r', '--resources', action='append', nargs='+',
                        help='A resource in the form type,name,value. '
                             'ex: -r bool,car_ui_scrollbar_enable,false')
    parser.add_argument('-d', '--dir',
                        help='res folder rro')
    parser.add_argument('-p', '--package', default='com.android.car.ui.paintbooth',
                        help='The package to override. Defaults to paintbooth.')
    parser.add_argument('--uninstall-all', action='store_true',
                        help='Uninstall all RROs created by this script')
    parser.add_argument('-I',
                        help='Path to android.jar or framework-res.apk. If not provided, will '
                             'attempt to auto locate in $OUT/system/framework/framework-res.apk, '
                             'and then in ~/Android/Sdk/')
    parser.add_argument('--workspace', default='/tmp',
                        help='The location where temporary files are made. Defaults to /tmp. '
                             'Will make a "quickrro" folder here.')
    args = parser.parse_args()

    if args.resources is not None:
        # flatten 2d list
        args.resources = [x for sub in args.resources for x in sub]

    if args.uninstall_all:
        return uninstall_all()

    if args.dir is None and args.resources is None:
        print('Must include one of --resources, --dir, or --uninstall-all')
        parser.print_help()
        sys.exit(1)

    if args.dir is not None and args.resources is not None:
        print('Cannot specify both --resources and --dir')
        sys.exit(1)

    if not os.path.isdir(args.workspace):
        print(str(args.workspace) + ': No such directory')
        sys.exit(1)

    if 'ANDROID_BUILD_TOP' not in os.environ:
        print("Please run lunch first")
        sys.exit(1)

    if not os.path.isfile(os.path.join(
            os.environ['ANDROID_BUILD_TOP'], 'out/soong/host/linux-x86/framework/signapk.jar')):
        print('out/soong/host/linux-x86/framework/signapk.jar missing, please do an android build first')
        sys.exit(1)

    package_name = get_package_name(args)
    signed_apk = os.path.join(args.workspace, 'quick_rro', package_name + '.apk')

    if os.path.isfile(signed_apk):
        print("Found cached RRO: " + signed_apk)
    else:
        build(args, package_name)

    print('Installing...')
    run_command(['adb', 'install', '-r', signed_apk])

    print('Enabling...')
    # Enabling RROs sometimes fails shortly after installing them
    time.sleep(1)
    run_command(['adb', 'shell', 'cmd', 'overlay', 'enable', '--user', 'current', package_name])

    print('Done!')

if __name__ == "__main__":
    main()
