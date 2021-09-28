#!/usr/bin/env python2
"""Generate summary report for ChromeOS toolchain waterfalls."""

from __future__ import print_function

import argparse
import datetime
import getpass
import json
import os
import re
import shutil
import sys
import time

from cros_utils import command_executer

# All the test suites whose data we might want for the reports.
TESTS = (('bvt-inline', 'HWTest [bvt-inline]'), ('bvt-cq', 'HWTest [bvt-cq]'),
         ('security', 'HWTest [security]'))

# The main waterfall builders, IN THE ORDER IN WHICH WE WANT THEM
# LISTED IN THE REPORT.
WATERFALL_BUILDERS = [
    'amd64-llvm-next-toolchain',
    'arm-llvm-next-toolchain',
    'arm64-llvm-next-toolchain',
]

DATA_DIR = '/google/data/rw/users/mo/mobiletc-prebuild/waterfall-report-data/'
ARCHIVE_DIR = '/google/data/rw/users/mo/mobiletc-prebuild/waterfall-reports/'
DOWNLOAD_DIR = '/tmp/waterfall-logs'
MAX_SAVE_RECORDS = 7
BUILD_DATA_FILE = '%s/build-data.txt' % DATA_DIR
LLVM_ROTATING_BUILDER = 'llvm_next_toolchain'
ROTATING_BUILDERS = [LLVM_ROTATING_BUILDER]

# For int-to-string date conversion.  Note, the index of the month in this
# list needs to correspond to the month's integer value.  i.e. 'Sep' must
# be as MONTHS[9].
MONTHS = [
    '', 'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct',
    'Nov', 'Dec'
]

DAYS_PER_MONTH = {
    1: 31,
    2: 28,
    3: 31,
    4: 30,
    5: 31,
    6: 30,
    7: 31,
    8: 31,
    9: 30,
    10: 31,
    11: 31,
    12: 31
}


def format_date(int_date, use_int_month=False):
  """Convert an integer date to a string date. YYYYMMDD -> YYYY-MMM-DD"""

  if int_date == 0:
    return 'today'

  tmp_date = int_date
  day = tmp_date % 100
  tmp_date = tmp_date / 100
  month = tmp_date % 100
  year = tmp_date / 100

  if use_int_month:
    date_str = '%d-%02d-%02d' % (year, month, day)
  else:
    month_str = MONTHS[month]
    date_str = '%d-%s-%d' % (year, month_str, day)
  return date_str


def EmailReport(report_file, report_type, date, email_to):
  """Emails the report to the approprite address."""
  subject = '%s Waterfall Summary report, %s' % (report_type, date)
  sendgmr_path = '/google/data/ro/projects/gws-sre/sendgmr'
  command = ('%s --to=%s --subject="%s" --body_file=%s' %
             (sendgmr_path, email_to, subject, report_file))
  command_executer.GetCommandExecuter().RunCommand(command)


def GetColor(status):
  """Given a job status string, returns appropriate color string."""
  if status.strip() == 'pass':
    color = 'green '
  elif status.strip() == 'fail':
    color = ' red  '
  elif status.strip() == 'warning':
    color = 'orange'
  else:
    color = '      '
  return color


def GenerateWaterfallReport(report_dict, waterfall_type, date):
  """Write out the actual formatted report."""

  filename = 'waterfall_report.%s_waterfall.%s.txt' % (waterfall_type, date)

  date_string = ''
  report_list = report_dict.keys()

  with open(filename, 'w') as out_file:
    # Write Report Header
    out_file.write('\nStatus of %s Waterfall Builds from %s\n\n' %
                   (waterfall_type, date_string))
    out_file.write('                                                        \n')
    out_file.write(
        '                                         Build       bvt-    '
        '     bvt-cq     '
        ' security \n')
    out_file.write(
        '                                         status     inline   '
        '              \n')

    # Write daily waterfall status section.
    for builder in report_list:
      build_dict = report_dict[builder]
      buildbucket_id = build_dict['buildbucket_id']
      overall_status = build_dict['status']
      if 'bvt-inline' in build_dict.keys():
        inline_status = build_dict['bvt-inline']
      else:
        inline_status = '    '
      if 'bvt-cq' in build_dict.keys():
        cq_status = build_dict['bvt-cq']
      else:
        cq_status = '    '
      if 'security' in build_dict.keys():
        security_status = build_dict['security']
      else:
        security_status = '    '
      inline_color = GetColor(inline_status)
      cq_color = GetColor(cq_status)
      security_color = GetColor(security_status)

      out_file.write(
          '%26s   %4s        %6s        %6s         %6s\n' %
          (builder, overall_status, inline_color, cq_color, security_color))
      if waterfall_type == 'main':
        out_file.write('     build url: https://cros-goldeneye.corp.google.com/'
                       'chromeos/healthmonitoring/buildDetails?buildbucketId=%s'
                       '\n' % buildbucket_id)
      else:
        out_file.write('     build url: https://ci.chromium.org/p/chromeos/'
                       'builds/b%s \n' % buildbucket_id)
        report_url = ('https://logs.chromium.org/v/?s=chromeos%2Fbuildbucket%2F'
                      'cr-buildbucket.appspot.com%2F' + buildbucket_id +
                      '%2F%2B%2Fsteps%2FReport%2F0%2Fstdout')
        out_file.write('\n     report status url: %s\n' % report_url)
      out_file.write('\n')

    print('Report generated in %s.' % filename)
    return filename


def GetTryjobData(date, rotating_builds_dict):
  """Read buildbucket id and board from stored file.

  buildbot_test_llvm.py, when it launches the rotating builders,
  records the buildbucket_id and board for each launch in a file.
  This reads that data out of the file so we can find the right
  tryjob data.
  """

  date_str = format_date(date, use_int_month=True)
  fname = '%s.builds' % date_str
  filename = os.path.join(DATA_DIR, 'rotating-builders', fname)

  if not os.path.exists(filename):
    print('Cannot find file: %s' % filename)
    print('Unable to generate rotating builder report for date %d.' % date)
    return

  with open(filename, 'r') as in_file:
    lines = in_file.readlines()

  for line in lines:
    l = line.strip()
    parts = l.split(',')
    if len(parts) != 2:
      print('Warning: Illegal line in data file.')
      print('File: %s' % filename)
      print('Line: %s' % l)
      continue
    buildbucket_id = parts[0]
    board = parts[1]
    rotating_builds_dict[board] = buildbucket_id

  return


def GetRotatingBuildData(date, report_dict, chromeos_root, board,
                         buildbucket_id, ce):
  """Gets rotating builder job results via 'cros buildresult'."""
  path = os.path.join(chromeos_root, 'chromite')
  save_dir = os.getcwd()
  date_str = format_date(date, use_int_month=True)
  os.chdir(path)

  command = (
      'cros buildresult --buildbucket-id %s --report json' % buildbucket_id)
  _, out, _ = ce.RunCommandWOutput(command)
  tmp_dict = json.loads(out)
  results = tmp_dict[buildbucket_id]

  board_dict = dict()
  board_dict['buildbucket_id'] = buildbucket_id
  stages_results = results['stages']
  for test in TESTS:
    key1 = test[0]
    key2 = test[1]
    if key2 in stages_results:
      board_dict[key1] = stages_results[key2]
  board_dict['status'] = results['status']
  report_dict[board] = board_dict
  os.chdir(save_dir)
  return


def GetMainWaterfallData(date, report_dict, chromeos_root, ce):
  """Gets main waterfall job results via 'cros buildresult'."""
  path = os.path.join(chromeos_root, 'chromite')
  save_dir = os.getcwd()
  date_str = format_date(date, use_int_month=True)
  os.chdir(path)
  for builder in WATERFALL_BUILDERS:
    command = ('cros buildresult --build-config %s --date %s --report json' %
               (builder, date_str))
    _, out, _ = ce.RunCommandWOutput(command)
    tmp_dict = json.loads(out)
    builder_dict = dict()
    for k in tmp_dict.keys():
      buildbucket_id = k
      results = tmp_dict[k]

    builder_dict['buildbucket_id'] = buildbucket_id
    builder_dict['status'] = results['status']
    stages_results = results['stages']
    for test in TESTS:
      key1 = test[0]
      key2 = test[1]
      builder_dict[key1] = stages_results[key2]
    report_dict[builder] = builder_dict
  os.chdir(save_dir)
  return


# Check for prodaccess.
def CheckProdAccess():
  """Verifies prodaccess is current."""
  status, output, _ = command_executer.GetCommandExecuter().RunCommandWOutput(
      'prodcertstatus')
  if status != 0:
    return False
  # Verify that status is not expired
  if 'expires' in output:
    return True
  return False


def ValidDate(date):
  """Ensures 'date' is a valid date."""
  min_year = 2018

  tmp_date = date
  day = tmp_date % 100
  tmp_date = tmp_date / 100
  month = tmp_date % 100
  year = tmp_date / 100

  if day < 1 or month < 1 or year < min_year:
    return False

  cur_year = datetime.datetime.now().year
  if year > cur_year:
    return False

  if month > 12:
    return False

  if month == 2 and cur_year % 4 == 0 and cur_year % 100 != 0:
    max_day = 29
  else:
    max_day = DAYS_PER_MONTH[month]

  if day > max_day:
    return False

  return True


def ValidOptions(parser, options):
  """Error-check the options passed to this script."""
  too_many_options = False
  if options.main:
    if options.rotating:
      too_many_options = True

  if too_many_options:
    parser.error('Can only specify one of --main, --rotating.')

  if not os.path.exists(options.chromeos_root):
    parser.error(
        'Invalid chromeos root. Cannot find: %s' % options.chromeos_root)

  email_ok = True
  if options.email and options.email.find('@') == -1:
    email_ok = False
    parser.error('"%s" is not a valid email address; it must contain "@..."' %
                 options.email)

  valid_date = ValidDate(options.date)

  return not too_many_options and valid_date and email_ok


def Main(argv):
  """Main function for this script."""
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--main',
      dest='main',
      default=False,
      action='store_true',
      help='Generate report only for main waterfall '
      'builders.')
  parser.add_argument(
      '--rotating',
      dest='rotating',
      default=False,
      action='store_true',
      help='Generate report only for rotating builders.')
  parser.add_argument(
      '--date',
      dest='date',
      required=True,
      type=int,
      help='The date YYYYMMDD of waterfall report.')
  parser.add_argument(
      '--email',
      dest='email',
      default='',
      help='Email address to use for sending the report.')
  parser.add_argument(
      '--chromeos_root',
      dest='chromeos_root',
      required=True,
      help='Chrome OS root in which to run chroot commands.')

  options = parser.parse_args(argv)

  if not ValidOptions(parser, options):
    return 1

  main_only = options.main
  rotating_only = options.rotating
  date = options.date

  prod_access = CheckProdAccess()
  if not prod_access:
    print('ERROR: Please run prodaccess first.')
    return

  waterfall_report_dict = dict()
  rotating_report_dict = dict()

  ce = command_executer.GetCommandExecuter()
  if not rotating_only:
    GetMainWaterfallData(date, waterfall_report_dict, options.chromeos_root, ce)

  if not main_only:
    rotating_builds_dict = dict()
    GetTryjobData(date, rotating_builds_dict)
    if len(rotating_builds_dict.keys()) > 0:
      for board in rotating_builds_dict.keys():
        buildbucket_id = rotating_builds_dict[board]
        GetRotatingBuildData(date, rotating_report_dict, options.chromeos_root,
                             board, buildbucket_id, ce)

  if options.email:
    email_to = options.email
  else:
    email_to = getpass.getuser()

  if waterfall_report_dict and not rotating_only:
    main_report = GenerateWaterfallReport(waterfall_report_dict, 'main', date)

    EmailReport(main_report, 'Main', format_date(date), email_to)
    shutil.copy(main_report, ARCHIVE_DIR)
  if rotating_report_dict and not main_only:
    rotating_report = GenerateWaterfallReport(rotating_report_dict, 'rotating',
                                              date)

    EmailReport(rotating_report, 'Rotating', format_date(date), email_to)
    shutil.copy(rotating_report, ARCHIVE_DIR)


if __name__ == '__main__':
  Main(sys.argv[1:])
  sys.exit(0)
