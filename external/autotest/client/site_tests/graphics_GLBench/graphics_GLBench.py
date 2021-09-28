# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros import perf
from autotest_lib.client.cros import service_stopper
from autotest_lib.client.cros.graphics import graphics_utils


class graphics_GLBench(graphics_utils.GraphicsTest):
  """Run glbench, a benchmark that times graphics intensive activities."""
  version = 1
  preserve_srcdir = True
  _services = None

  glbench_directory = '/usr/local/glbench/'
  # Good images.
  reference_images_file = os.path.join(glbench_directory,
                                       'files/glbench_reference_images.txt')
  # Images that are bad but for which the bug has not been fixed yet.
  knownbad_images_file = os.path.join(glbench_directory,
                                      'files/glbench_knownbad_images.txt')
  # Images that are bad and for which a fix has been submitted.
  fixedbad_images_file = os.path.join(glbench_directory,
                                      'files/glbench_fixedbad_images.txt')

  # These tests do not draw anything, they can only be used to check
  # performance.
  no_checksum_tests = set([
      'compositing_no_fill',
      'pixel_read',
      'texture_reuse_luminance_teximage2d',
      'texture_reuse_luminance_texsubimage2d',
      'texture_reuse_rgba_teximage2d',
      'texture_reuse_rgba_texsubimage2d',
      'context_glsimple',
      'swap_glsimple',
  ])

  blacklist = ''

  unit_higher_is_better = {
      'mbytes_sec': True,
      'mpixels_sec': True,
      'mtexel_sec': True,
      'mtri_sec': True,
      'mvtx_sec': True,
      'us': False,
      '1280x768_fps': True
  }

  def initialize(self):
    super(graphics_GLBench, self).initialize()
    # If UI is running, we must stop it and restore later.
    self._services = service_stopper.ServiceStopper(['ui'])
    self._services.stop_services()

  def cleanup(self):
    if self._services:
      self._services.restore_services()
    super(graphics_GLBench, self).cleanup()

  def is_no_checksum_test(self, testname):
    """Check if given test requires no screenshot checksum.

    @param testname: name of test to check.
    """
    for prefix in self.no_checksum_tests:
      if testname.startswith(prefix):
        return True
    return False

  def load_imagenames(self, filename):
    """Loads text file with MD5 file names.

    @param filename: name of file to load.
    """
    imagenames = os.path.join(self.autodir, filename)
    with open(imagenames, 'r') as f:
      imagenames = f.read()
      return imagenames

  @graphics_utils.GraphicsTest.failure_report_decorator('graphics_GLBench')
  def run_once(self, options='', hasty=False):
    """Run the test.

    @param options: String of options to run the glbench executable with.
    @param hasty: Run the test more quickly by running fewer iterations,
        lower resolution, and without waiting for the dut to cool down.
    """
    options += self.blacklist
    # Run the test, saving is optional and helps with debugging
    # and reference image management. If unknown images are
    # encountered one can take them from the outdir and copy
    # them (after verification) into the reference image dir.
    exefile = os.path.join(self.glbench_directory, 'bin/glbench')
    outdir = self.outputdir
    options += ' -save -outdir=' + outdir
    # Using the -hasty option we run only a subset of tests without waiting
    # for thermals to normalize. Test should complete in 15-20 seconds.
    if hasty:
      options += ' -hasty'

    cmd = '%s %s' % (exefile, options)
    summary = None
    pc_error_reason = None
    try:
      if hasty:
        # On BVT the test will not monitor thermals so we will not verify its
        # correct status using PerfControl
        summary = utils.run(cmd,
                            stderr_is_expected=False,
                            stdout_tee=utils.TEE_TO_LOGS,
                            stderr_tee=utils.TEE_TO_LOGS).stdout
      else:
        utils.report_temperature(self, 'temperature_1_start')
        # Wrap the test run inside of a PerfControl instance to make machine
        # behavior more consistent.
        with perf.PerfControl() as pc:
          if not pc.verify_is_valid():
            raise error.TestFail('Failed: %s' % pc.get_error_reason())
          utils.report_temperature(self, 'temperature_2_before_test')

          # Run the test. If it gets the CPU too hot pc should notice.
          summary = utils.run(cmd,
                              stderr_is_expected=False,
                              stdout_tee=utils.TEE_TO_LOGS,
                              stderr_tee=utils.TEE_TO_LOGS).stdout
          if not pc.verify_is_valid():
            # Defer error handling until after perf report.
            pc_error_reason = pc.get_error_reason()
    except error.CmdError:
      raise error.TestFail('Failed: CmdError running %s' % cmd)
    except error.CmdTimeoutError:
      raise error.TestFail('Failed: CmdTimeout running %s' % cmd)

    # Write a copy of stdout to help debug failures.
    results_path = os.path.join(self.outputdir, 'summary.txt')
    f = open(results_path, 'w+')
    f.write('# ---------------------------------------------------\n')
    f.write('# [' + cmd + ']\n')
    f.write(summary)
    f.write('\n# -------------------------------------------------\n')
    f.write('# [graphics_GLBench.py postprocessing]\n')

    # Analyze the output. Sample:
    ## board_id: NVIDIA Corporation - Quadro FX 380/PCI/SSE2
    ## Running: ../glbench -save -outdir=img
    #swap_swap = 221.36 us [swap_swap.pixmd5-20dbc...f9c700d2f.png]
    results = summary.splitlines()
    if not results:
      f.close()
      raise error.TestFail('Failed: No output from test. Check /tmp/' +
                           'test_that_latest/graphics_GLBench/summary.txt' +
                           ' for details.')

    # The good images, the silenced and the zombie/recurring failures.
    reference_imagenames = self.load_imagenames(self.reference_images_file)
    knownbad_imagenames = self.load_imagenames(self.knownbad_images_file)
    fixedbad_imagenames = self.load_imagenames(self.fixedbad_images_file)

    # Check if we saw GLBench end as expected (without crashing).
    test_ended_normal = False
    for line in results:
      if line.strip().startswith('@TEST_END'):
        test_ended_normal = True

    # Analyze individual test results in summary.
    # TODO(pwang): Raise TestFail if an error is detected during glbench.
    keyvals = {}
    failed_tests = {}
    for line in results:
      if not line.strip().startswith('@RESULT: '):
        continue
      keyval, remainder = line[9:].split('[')
      key, val = keyval.split('=')
      testname = key.strip()
      score, unit = val.split()
      testrating = float(score)
      imagefile = remainder.split(']')[0]

      if not hasty:
        higher = self.unit_higher_is_better.get(unit)
        if higher is None:
          raise error.TestFail('Failed: Unknown test unit "%s" for %s' %
                               (unit, testname))
        # Prepend unit to test name to maintain backwards compatibility with
        # existing per data.
        perf_value_name = '%s_%s' % (unit, testname)
        self.output_perf_value(
            description=perf_value_name,
            value=testrating,
            units=unit,
            higher_is_better=higher,
            graph=perf_value_name)

      # Classify result image.
      if testrating == -1.0:
        # Tests that generate GL Errors.
        glerror = imagefile.split('=')[1]
        f.write('# GLError ' + glerror + ' during test (perf set to -3.0)\n')
        keyvals[testname] = -3.0
        failed_tests[testname] = 'GLError'
      elif testrating == 0.0:
        # Tests for which glbench does not generate a meaningful perf score.
        f.write('# No score for test\n')
        keyvals[testname] = 0.0
      elif imagefile in fixedbad_imagenames:
        # We know the image looked bad at some point in time but we thought
        # it was fixed. Throw an exception as a reminder.
        keyvals[testname] = -2.0
        f.write('# fixedbad [' + imagefile + '] (setting perf as -2.0)\n')
        failed_tests[testname] = imagefile
      elif imagefile in knownbad_imagenames:
        # We have triaged the failure and have filed a tracking bug.
        # Don't throw an exception and remind there is a problem.
        keyvals[testname] = -1.0
        f.write('# knownbad [' + imagefile + '] (setting perf as -1.0)\n')
        # This failure is whitelisted so don't add to failed_tests.
      elif imagefile in reference_imagenames:
        # Known good reference images (default).
        keyvals[testname] = testrating
      elif imagefile == 'none':
        # Tests that do not write images can't fail because of them.
        keyvals[testname] = testrating
      elif self.is_no_checksum_test(testname):
        # TODO(ihf): these really should not write any images
        keyvals[testname] = testrating
      else:
        # Completely unknown images. Raise a failure.
        keyvals[testname] = -2.0
        failed_tests[testname] = imagefile
        f.write('# unknown [' + imagefile + '] (setting perf as -2.0)\n')
    f.close()
    if not hasty:
      utils.report_temperature(self, 'temperature_3_after_test')
      self.write_perf_keyval(keyvals)

    # Raise exception if images don't match.
    if failed_tests:
      logging.info('Some images are not matching their reference in %s.',
                   self.reference_images_file)
      logging.info('Please verify that the output images are correct '
                   'and if so copy them to the reference directory.')
      raise error.TestFail('Failed: Some images are not matching their '
                           'references. Check /tmp/'
                           'test_that_latest/graphics_GLBench/summary.txt'
                           ' for details.')

    if not test_ended_normal:
      raise error.TestFail(
          'Failed: No end marker. Presumed crash/missing images.')
    if pc_error_reason:
      raise error.TestFail('Failed: %s' % pc_error_reason)
