#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Updates the ExoPlayer version in platform/external/exoplayer."""
import argparse
import os
import sys
import subprocess
import atexit
import shutil
import re
import datetime

TEMPORARY_TREE_CHECKOUT_DIR = ".temp_tree/"
TREE_LOCATION = "tree/"
METADATA_FILE = "METADATA"

def run(command, check=True):
  print(f"Running: {command}")
  return (subprocess.run(
      command, shell=True, check=check, capture_output=True, text=True)
          .stdout.strip())

# Argument parsing.
parser = argparse.ArgumentParser(
    description=f"Update the ExoPlayer version in the {TREE_LOCATION}"
        " directory and stage changes for commit.")
parser.add_argument(
    "--tag",
    help="The tag to update the ExoPlayer version to.")
parser.add_argument(
    "--commit",
    help="The commit SHA to update the ExoPlayer version to.")
parser.add_argument(
    "--branch",
    help="The branch to create for the change.",
    default="update-exoplayer")
args = parser.parse_args()

script_directory = os.path.dirname(os.path.abspath(sys.argv[0]))
os.chdir(script_directory)

if (args.tag is None) == (args.commit is None):
  parser.print_help()
  sys.exit("\nError: Please provide the tag or the commit. But not both.")

# Check whether the branch exists, and abort if it does.
if run(f"git rev-parse --verify --quiet {args.branch}", check=False):
  parser.print_help()
  sys.exit(f"\nBranch {args.branch} already exists. Please delete, or change "
      "branch.")

run(f"repo start {args.branch}")

# Cleanup function.
def cleanup():
  print(f"Restoring branch {args.branch}")
  run(f"git checkout {args.branch}")
  shutil.rmtree(TEMPORARY_TREE_CHECKOUT_DIR, ignore_errors=True)
atexit.register(cleanup)

# Update remote branches.
run("git fetch --all --tags")
if args.tag:
  # Get the commit SHA associated to the tag.
  commit = run(f"git rev-list -n 1 {args.tag}")
else: # a commit SHA was provided.
  commit = args.commit

# Checkout the version we want to update to.
run(f"git checkout {commit}")
# Checkout all files into a temporary dir.
run(f"git checkout-index -a --prefix={TEMPORARY_TREE_CHECKOUT_DIR}")
run(f"git checkout {args.branch}")
shutil.rmtree(TREE_LOCATION)
run(f"mv {TEMPORARY_TREE_CHECKOUT_DIR} {TREE_LOCATION}")
run(f"git add {TREE_LOCATION} {METADATA_FILE}")

with open(METADATA_FILE) as metadata_file:
  metadata_lines = metadata_file.readlines()

# Update the metadata file.
today = datetime.date.today()
with open(METADATA_FILE, "w") as metadata_file:
  for line in metadata_lines:
    line = re.sub(
        r"version: \".+\"", f"version: \"{args.tag or commit}\"", line)
    line = re.sub(r"last_upgrade_date {.+}", f"last_upgrade_date "
        f"{{ year: {today.year} month: {today.month} day: {today.day} }}",
        line)
    metadata_file.write(line)

run(f"git add {METADATA_FILE}")
print("All done. Ready to commit.")
