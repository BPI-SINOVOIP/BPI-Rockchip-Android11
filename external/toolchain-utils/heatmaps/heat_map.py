#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper to generate heat maps for chrome."""

from __future__ import print_function

import argparse
import os
import shutil
import sys
import tempfile

from cros_utils import command_executer
from heatmaps import heatmap_generator


def IsARepoRoot(directory):
  """Returns True if directory is the root of a repo checkout."""
  return os.path.exists(
      os.path.join(os.path.realpath(os.path.expanduser(directory)), '.repo'))


class HeatMapProducer(object):
  """Class to produce heat map."""

  def __init__(self,
               chromeos_root,
               perf_data,
               hugepage,
               binary,
               title,
               logger=None):
    self.chromeos_root = os.path.realpath(os.path.expanduser(chromeos_root))
    self.perf_data = os.path.realpath(os.path.expanduser(perf_data))
    self.hugepage = hugepage
    self.dir = os.path.dirname(os.path.realpath(__file__))
    self.binary = binary
    self.ce = command_executer.GetCommandExecuter()
    self.temp_dir = ''
    self.temp_perf_inchroot = ''
    self.temp_dir_created = False
    self.perf_report = ''
    self.title = title
    self.logger = logger

  def _EnsureFileInChroot(self):
    chroot_prefix = os.path.join(self.chromeos_root, 'chroot')
    if self.perf_data.startswith(chroot_prefix):
      # If the path to perf_data starts with the same chromeos_root, assume
      # it's in the chromeos_root so no need for temporary directory and copy.
      self.temp_dir = self.perf_data.replace('perf.data', '')
      self.temp_perf_inchroot = self.temp_dir.replace(chroot_prefix, '')

    else:
      # Otherwise, create a temporary directory and copy perf.data into chroot.
      self.temp_dir = tempfile.mkdtemp(
          prefix=os.path.join(self.chromeos_root, 'src/'))
      temp_perf = os.path.join(self.temp_dir, 'perf.data')
      shutil.copy2(self.perf_data, temp_perf)
      self.temp_perf_inchroot = os.path.join('~/trunk/src',
                                             os.path.basename(self.temp_dir))
      self.temp_dir_created = True

  def _GeneratePerfReport(self):
    cmd = ('cd %s && perf report -D -i perf.data > perf_report.txt' %
           self.temp_perf_inchroot)
    retval = self.ce.ChrootRunCommand(self.chromeos_root, cmd)
    if retval:
      raise RuntimeError('Failed to generate perf report')
    self.perf_report = os.path.join(self.temp_dir, 'perf_report.txt')

  def _GetHeatMap(self, top_n_pages):
    generator = heatmap_generator.HeatmapGenerator(
        perf_report=self.perf_report,
        page_size=4096,
        hugepage=self.hugepage,
        title=self.title)
    generator.draw()
    # Analyze top N hottest symbols with the binary, if provided
    if self.binary:
      generator.analyze(self.binary, top_n_pages)

  def _RemoveFiles(self):
    files = [
        'out.txt', 'inst-histo.txt', 'inst-histo-hp.txt', 'inst-histo-sp.txt'
    ]
    for f in files:
      if os.path.exists(f):
        os.remove(f)

  def Run(self, top_n_pages):
    try:
      self._EnsureFileInChroot()
      self._GeneratePerfReport()
      self._GetHeatMap(top_n_pages)
    finally:
      self._RemoveFiles()
    msg = ('heat map and time histogram genereated in the current '
           'directory with name heat_map.png and timeline.png '
           'accordingly.')
    if self.binary:
      msg += ('\nThe hottest %d pages inside and outside hugepage '
              'is symbolized and saved to addr2symbol.txt' % top_n_pages)
    if self.logger:
      self.logger.LogOutput(msg)
    else:
      print(msg)


def main(argv):
  """Parse the options.

  Args:
    argv: The options with which this script was invoked.

  Returns:
    0 unless an exception is raised.
  """
  parser = argparse.ArgumentParser()

  parser.add_argument(
      '--chromeos_root',
      dest='chromeos_root',
      required=True,
      help='ChromeOS root to use for generate heatmaps.')
  parser.add_argument(
      '--perf_data',
      dest='perf_data',
      required=True,
      help='The raw perf data. Must be collected with -e instructions while '
      'disabling ASLR.')
  parser.add_argument(
      '--binary',
      dest='binary',
      help='The path to the Chrome binary. Only useful if want to print '
      'symbols on hottest pages',
      default=None)
  parser.add_argument(
      '--top_n',
      dest='top_n',
      type=int,
      default=10,
      help='Print out top N hottest pages within/outside huge page range. '
      'Must be used with --hugepage and --binary. (Default: %(default)s)')
  parser.add_argument(
      '--title', dest='title', help='Title of the heatmap', default='')
  parser.add_argument(
      '--hugepage',
      dest='hugepage',
      help='A range of addresses (start,end) where huge page starts and ends'
      ' in text section, separated by a comma.'
      ' Used to differentiate regions in heatmap.'
      ' Example: --hugepage=0,4096'
      ' If not specified, no effect on the heatmap.',
      default=None)

  options = parser.parse_args(argv)

  if not IsARepoRoot(options.chromeos_root):
    parser.error('%s does not contain .repo dir.' % options.chromeos_root)

  if not os.path.isfile(options.perf_data):
    parser.error('Cannot find perf_data: %s.' % options.perf_data)

  hugepage_range = None
  if options.hugepage:
    hugepage_range = options.hugepage.split(',')
    if len(hugepage_range) != 2 or \
     int(hugepage_range[0]) > int(hugepage_range[1]):
      parser.error('Wrong format of hugepage range: %s' % options.hugepage)
    hugepage_range = [int(x) for x in hugepage_range]

  heatmap_producer = HeatMapProducer(options.chromeos_root, options.perf_data,
                                     hugepage_range, options.binary,
                                     options.title)

  heatmap_producer.Run(options.top_n)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
