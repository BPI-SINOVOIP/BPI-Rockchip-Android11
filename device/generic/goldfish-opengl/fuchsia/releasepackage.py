# Copyright 2019 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import shutil
import subprocess
import argparse
import re

parser = argparse.ArgumentParser(description="Upload goldfish libvulkan to CIPD")

parser.add_argument("--release-dir")
parser.add_argument("--arch")
parser.add_argument("--dry-run", action="store_true")
parser.add_argument("--ignore-branch", action="store_true")
parser.add_argument("--ignore-rebuild", action="store_true")
parser.add_argument("--ignore-buildtype", action="store_true")

args = parser.parse_args()

dir_path = os.path.dirname(os.path.realpath(__file__))

os.chdir(dir_path)

fuchsia_root = os.path.abspath(os.path.join(dir_path, "../../../"))
fx_path = os.path.join(fuchsia_root, "scripts/fx")

if args.release_dir:
  release_dir = os.path.abspath(args.release_dir)
else:
  release_dir = os.path.join(fuchsia_root, "out/default")

if not os.path.exists(release_dir):
  print "Release dir: %s doesn't exist" % release_dir
  sys.exit(1)

if args.arch:
  arch = args.arch
else:
  arch = "x64"

target_name = "%s-shared/lib.unstripped/libvulkan_goldfish.so" % arch
git_repo_location = "%s/third_party/goldfish-opengl" % fuchsia_root
package_dir = "libvulkan_goldfish/%s" % arch
package_name = "fuchsia/lib/libvulkan/%s" % package_dir

repo_name = "goldfish-opengl"
git_branch = subprocess.check_output([
    "git", "-C", git_repo_location, "rev-parse", "--abbrev-ref", "HEAD"
]).strip()
if git_branch != "master":
  print("Git repo %s on incorrect branch %s (should be master)" %
        (repo_name, git_branch))
  if args.ignore_branch:
    print("Ignoring")
  else:
    print("Use --ignore-branch flag to upload anyway")
    sys.exit(1)

# Force ninja dry-run
ninja_output = subprocess.check_output([
    fx_path, "ninja", "-C", release_dir, "-v", "-n", target_name
])

if "ninja: no work to do." not in ninja_output:
  print("Ninja reported work needed to be done for %s" % target_name)
  if args.ignore_rebuild:
    print("Ignoring")
  else:
    print("Use --ignore-rebuild flag to upload anyway")
    sys.exit(1)

gn_output = subprocess.check_output([
    fx_path, "gn", "args", release_dir, "--list=is_debug", "--short"
]).strip()
if gn_output != "is_debug = false":
  print("GN argument \"%s\" unexpected" % gn_output)
  if args.ignore_buildtype:
    print("Ignoring")
  else:
    print("Use --ignore-buildtype flag to upload anyway")
    sys.exit(1)

file_name = "libvulkan_goldfish.so"
full_name = os.path.join(package_dir, file_name)

source_file_name = os.path.join(release_dir, target_name)
try:
  os.remove(full_name)
except:
  pass
try:
  os.makedirs(package_dir)
except:
  pass
shutil.copyfile(source_file_name, full_name)

git_rev = subprocess.check_output(
    ["git", "-C", git_repo_location, "rev-parse", "HEAD"]).strip()

cipd_command = ("%s cipd create -in %s -name %s -ref latest"
                " -install-mode copy -tag git_revision:%s") % (
                    fx_path, package_dir, package_name, git_rev)
print cipd_command
if not args.dry_run:
  subprocess.check_call(cipd_command.split(" "))

print ("""
  <package name="%s"
           version="git_revision:%s"
           path="prebuild/third_party/%s"/>
""" % (package_name, git_rev, package_dir))[1:-1]
