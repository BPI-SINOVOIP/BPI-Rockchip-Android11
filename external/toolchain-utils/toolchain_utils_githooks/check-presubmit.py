#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs presubmit checks against a bundle of files."""

# To keep `cros lint` happy
from __future__ import division, print_function

import argparse
import collections
import datetime
import multiprocessing
import multiprocessing.pool
import os
import re
import shlex
import shutil
import subprocess
import sys
import threading
import traceback


def run_command_unchecked(command, cwd, env=None):
  """Runs a command in the given dir, returning its exit code and stdio."""
  p = subprocess.Popen(
      command,
      cwd=cwd,
      stdin=subprocess.DEVNULL,
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      env=env,
  )

  stdout, _ = p.communicate()
  exit_code = p.wait()
  return exit_code, stdout.decode('utf-8', 'replace')


def has_executable_on_path(exe):
  """Returns whether we have `exe` somewhere on our $PATH"""
  return shutil.which(exe) is not None


def escape_command(command):
  """Returns a human-readable and copy-pastable shell command.

  Only intended for use in output to users. shell=True is strongly discouraged.
  """
  return ' '.join(shlex.quote(x) for x in command)


def remove_deleted_files(files):
  return [f for f in files if os.path.exists(f)]


def is_file_executable(file_path):
  return os.access(file_path, os.X_OK)


# As noted in our docs, some of our Python code depends on modules that sit in
# toolchain-utils/. Add that to PYTHONPATH to ensure that things like `cros
# lint` are kept happy.
def env_with_pythonpath(toolchain_utils_root):
  env = dict(os.environ)
  if 'PYTHONPATH' in env:
    env['PYTHONPATH'] += ':' + toolchain_utils_root
  else:
    env['PYTHONPATH'] = toolchain_utils_root
  return env


# Each checker represents an independent check that's done on our sources.
#
# They should:
#  - never write to stdout/stderr or read from stdin directly
#  - return either a CheckResult, or a list of [(subcheck_name, CheckResult)]
#  - ideally use thread_pool to check things concurrently
#    - though it's important to note that these *also* live on the threadpool
#      we've provided. It's the caller's responsibility to guarantee that at
#      least ${number_of_concurrently_running_checkers}+1 threads are present
#      in the pool. In order words, blocking on results from the provided
#      threadpool is OK.
CheckResult = collections.namedtuple('CheckResult',
                                     ('ok', 'output', 'autofix_commands'))


def get_check_result_or_catch(task):
  """Returns the result of task(); if that raises, returns a CheckResult."""
  try:
    return task.get()
  except Exception:
    return CheckResult(
        ok=False,
        output='Check exited with an unexpected exception:\n%s' %
        traceback.format_exc(),
        autofix_commands=[],
    )


def check_yapf(toolchain_utils_root, python_files):
  """Subchecker of check_py_format. Checks python file formats with yapf"""
  command = ['yapf', '-d'] + python_files
  exit_code, stdout_and_stderr = run_command_unchecked(
      command, cwd=toolchain_utils_root)

  # yapf fails when files are poorly formatted.
  if exit_code == 0:
    return CheckResult(
        ok=True,
        output='',
        autofix_commands=[],
    )

  bad_files = []
  bad_file_re = re.compile(r'^--- (.*)\s+\(original\)\s*$')
  for line in stdout_and_stderr.splitlines():
    m = bad_file_re.match(line)
    if not m:
      continue

    file_name, = m.groups()
    bad_files.append(file_name.strip())

  # ... and doesn't really differentiate "your files have broken formatting"
  # errors from general ones. So if we see nothing diffed, assume that a
  # general error happened.
  if not bad_files:
    return CheckResult(
        ok=False,
        output='`%s` failed; stdout/stderr:\n%s' % (escape_command(command),
                                                    stdout_and_stderr),
        autofix_commands=[],
    )

  autofix = ['yapf', '-i'] + bad_files
  return CheckResult(
      ok=False,
      output='The following file(s) have formatting errors: %s' % bad_files,
      autofix_commands=[autofix],
  )


def check_python_file_headers(python_files):
  """Subchecker of check_py_format. Checks python #!s"""
  add_hashbang = []
  remove_hashbang = []

  for python_file in python_files:
    needs_hashbang = is_file_executable(python_file)
    with open(python_file, encoding='utf-8') as f:
      has_hashbang = f.read(2) == '#!'
      if needs_hashbang == has_hashbang:
        continue

      if needs_hashbang:
        add_hashbang.append(python_file)
      else:
        remove_hashbang.append(python_file)

  autofix = []
  output = []
  if add_hashbang:
    output.append(
        'The following files have no #!, but need one: %s' % add_hashbang)
    autofix.append(['sed', '-i', '1i#!/usr/bin/env python3'] + add_hashbang)

  if remove_hashbang:
    output.append(
        "The following files have a #!, but shouldn't: %s" % remove_hashbang)
    autofix.append(['sed', '-i', '1d'] + remove_hashbang)

  if not output:
    return CheckResult(
        ok=True,
        output='',
        autofix_commands=[],
    )
  return CheckResult(
      ok=False,
      output='\n'.join(output),
      autofix_commands=autofix,
  )


def check_py_format(toolchain_utils_root, thread_pool, files):
  """Runs yapf on files to check for style bugs. Also checks for #!s."""
  yapf = 'yapf'
  if not has_executable_on_path(yapf):
    return CheckResult(
        ok=False,
        output="yapf isn't available on your $PATH. Please either "
        'enter a chroot, or place depot_tools on your $PATH.',
        autofix_commands=[],
    )

  python_files = [f for f in remove_deleted_files(files) if f.endswith('.py')]
  if not python_files:
    return CheckResult(
        ok=True,
        output='no python files to check',
        autofix_commands=[],
    )

  tasks = [
      ('check_yapf',
       thread_pool.apply_async(check_yapf,
                               (toolchain_utils_root, python_files))),
      ('check_file_headers',
       thread_pool.apply_async(check_python_file_headers, (python_files,))),
  ]
  return [(name, get_check_result_or_catch(task)) for name, task in tasks]


def check_cros_lint(toolchain_utils_root, thread_pool, files):
  """Runs `cros lint`"""

  fixed_env = env_with_pythonpath(toolchain_utils_root)

  # We have to support users who don't have a chroot. So we either run `cros
  # lint` (if it's been made available to us), or we try a mix of
  # pylint+golint.
  def try_run_cros_lint(cros_binary):
    exit_code, output = run_command_unchecked(
        [cros_binary, 'lint', '--py3', '--'] + files,
        toolchain_utils_root,
        env=fixed_env)

    # This is returned specifically if cros couldn't find the Chrome OS tree
    # root.
    if exit_code == 127:
      return None

    return CheckResult(
        ok=exit_code == 0,
        output=output,
        autofix_commands=[],
    )

  cros_lint = try_run_cros_lint('cros')
  if cros_lint is not None:
    return cros_lint

  cros_root = os.getenv('CHROMEOS_ROOT_DIRECTORY')
  if cros_root:
    cros_lint = try_run_cros_lint(os.path.join(cros_root, 'chromite/bin/cros'))
    if cros_lint is not None:
      return cros_lint

  tasks = []

  def check_result_from_command(command):
    exit_code, output = run_command_unchecked(
        command, toolchain_utils_root, env=fixed_env)
    return CheckResult(
        ok=exit_code == 0,
        output=output,
        autofix_commands=[],
    )

  python_files = [f for f in remove_deleted_files(files) if f.endswith('.py')]
  if python_files:

    def run_pylint():
      # pylint is required. Fail hard if it DNE.
      return check_result_from_command(['pylint'] + python_files)

    tasks.append(('pylint', thread_pool.apply_async(run_pylint)))

  go_files = [f for f in remove_deleted_files(files) if f.endswith('.go')]
  if go_files:

    def run_golint():
      if has_executable_on_path('golint'):
        return check_result_from_command(['golint', '-set_exit_status'] +
                                         go_files)

      complaint = '\n'.join((
          'WARNING: go linting disabled. golint is not on your $PATH.',
          'Please either enter a chroot, or install go locally. Continuing.',
      ))
      return CheckResult(
          ok=True,
          output=complaint,
          autofix_commands=[],
      )

    tasks.append(('golint', thread_pool.apply_async(run_golint)))

  complaint = '\n'.join((
      'WARNING: No Chrome OS checkout detected, and no viable CrOS tree',
      'found; falling back to linting only python and go. If you have a',
      'Chrome OS checkout, please either develop from inside of the source',
      'tree, or set $CHROMEOS_ROOT_DIRECTORY to the root of it.',
  ))

  results = [(name, get_check_result_or_catch(task)) for name, task in tasks]
  if not results:
    return CheckResult(
        ok=True,
        output=complaint,
        autofix_commands=[],
    )

  # We need to complain _somewhere_.
  name, angry_result = results[0]
  angry_complaint = (complaint + '\n\n' + angry_result.output).strip()
  results[0] = (name, angry_result._replace(output=angry_complaint))
  return results


def check_go_format(toolchain_utils_root, _thread_pool, files):
  """Runs gofmt on files to check for style bugs."""
  gofmt = 'gofmt'
  if not has_executable_on_path(gofmt):
    return CheckResult(
        ok=False,
        output="gofmt isn't available on your $PATH. Please either "
        'enter a chroot, or place your go bin/ directory on your $PATH.',
        autofix_commands=[],
    )

  go_files = [f for f in remove_deleted_files(files) if f.endswith('.go')]
  if not go_files:
    return CheckResult(
        ok=True,
        output='no go files to check',
        autofix_commands=[],
    )

  command = [gofmt, '-l'] + go_files
  exit_code, output = run_command_unchecked(command, cwd=toolchain_utils_root)

  if exit_code:
    return CheckResult(
        ok=False,
        output='%s failed; stdout/stderr:\n%s' % (escape_command(command),
                                                  output),
        autofix_commands=[],
    )

  output = output.strip()
  if not output:
    return CheckResult(
        ok=True,
        output='',
        autofix_commands=[],
    )

  broken_files = [x.strip() for x in output.splitlines()]
  autofix = [gofmt, '-w'] + broken_files
  return CheckResult(
      ok=False,
      output='The following Go files have incorrect '
      'formatting: %s' % broken_files,
      autofix_commands=[autofix],
  )


def check_tests(toolchain_utils_root, _thread_pool, files):
  """Runs tests."""
  exit_code, stdout_and_stderr = run_command_unchecked(
      [os.path.join(toolchain_utils_root, 'run_tests_for.py'), '--'] + files,
      toolchain_utils_root)
  return CheckResult(
      ok=exit_code == 0,
      output=stdout_and_stderr,
      autofix_commands=[],
  )


def detect_toolchain_utils_root():
  return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def process_check_result(check_name, check_results, start_time):
  """Prints human-readable output for the given check_results."""
  indent = '  '

  def indent_block(text):
    return indent + text.replace('\n', '\n' + indent)

  if isinstance(check_results, CheckResult):
    ok, output, autofix_commands = check_results
    if not ok and autofix_commands:
      recommendation = ('Recommended command(s) to fix this: %s' %
                        [escape_command(x) for x in autofix_commands])
      if output:
        output += '\n' + recommendation
      else:
        output = recommendation
  else:
    output_pieces = []
    autofix_commands = []
    for subname, (ok, output, autofix) in check_results:
      status = 'succeeded' if ok else 'failed'
      message = ['*** %s.%s %s' % (check_name, subname, status)]
      if output:
        message.append(indent_block(output))
      if not ok and autofix:
        message.append(
            indent_block('Recommended command(s) to fix this: %s' %
                         [escape_command(x) for x in autofix]))

      output_pieces.append('\n'.join(message))
      autofix_commands += autofix

    ok = all(x.ok for _, x in check_results)
    output = '\n\n'.join(output_pieces)

  time_taken = datetime.datetime.now() - start_time
  if ok:
    print('*** %s succeeded after %s' % (check_name, time_taken))
  else:
    print('*** %s failed after %s' % (check_name, time_taken))

  if output:
    print(indent_block(output))

  print()
  return ok, autofix_commands


def try_autofix(all_autofix_commands, toolchain_utils_root):
  """Tries to run all given autofix commands, if appropriate."""
  if not all_autofix_commands:
    return

  exit_code, output = run_command_unchecked(['git', 'status', '--porcelain'],
                                            cwd=toolchain_utils_root)
  if exit_code != 0:
    print("Autofix aborted: couldn't get toolchain-utils git status.")
    return

  if output.strip():
    # A clean repo makes checking/undoing autofix commands trivial. A dirty
    # one... less so. :)
    print('Git repo seems dirty; skipping autofix.')
    return

  anything_succeeded = False
  for command in all_autofix_commands:
    exit_code, output = run_command_unchecked(command, cwd=toolchain_utils_root)

    if exit_code:
      print('*** Autofix command `%s` exited with code %d; stdout/stderr:' %
            (escape_command(command), exit_code))
      print(output)
    else:
      print('*** Autofix `%s` succeeded' % escape_command(command))
      anything_succeeded = True

  if anything_succeeded:
    print('NOTE: Autofixes have been applied. Please check your tree, since '
          'some lints may now be fixed')


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--no_autofix',
      dest='autofix',
      action='store_false',
      help="Don't run any autofix commands")
  parser.add_argument('files', nargs='*')
  opts = parser.parse_args(argv)

  files = opts.files
  if not files:
    return 0

  files = [os.path.abspath(f) for f in files]

  # Note that we extract .__name__s from these, so please name them in a
  # user-friendly way.
  checks = [
      check_cros_lint,
      check_py_format,
      check_go_format,
      check_tests,
  ]

  toolchain_utils_root = detect_toolchain_utils_root()

  # NOTE: As mentioned above, checks can block on threads they spawn in this
  # pool, so we need at least len(checks)+1 threads to avoid deadlock. Use *2
  # so all checks can make progress at a decent rate.
  num_threads = max(multiprocessing.cpu_count(), len(checks) * 2)
  start_time = datetime.datetime.now()

  # For our single print statement...
  spawn_print_lock = threading.RLock()

  def run_check(check_fn):
    name = check_fn.__name__
    with spawn_print_lock:
      print('*** Spawning %s' % name)
    return name, check_fn(toolchain_utils_root, pool, files)

  # ThreadPool is a ContextManager in py3.
  # pylint: disable=not-context-manager
  with multiprocessing.pool.ThreadPool(num_threads) as pool:
    all_checks_ok = True
    all_autofix_commands = []
    for check_name, result in pool.imap_unordered(run_check, checks):
      ok, autofix_commands = process_check_result(check_name, result,
                                                  start_time)
      all_checks_ok = ok and all_checks_ok
      all_autofix_commands += autofix_commands

  # Run these after everything settles, so:
  # - we don't collide with checkers that are running concurrently
  # - we clearly print out everything that went wrong ahead of time, in case
  #   any of these fail
  if opts.autofix:
    try_autofix(all_autofix_commands, toolchain_utils_root)

  if not all_checks_ok:
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
