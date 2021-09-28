#!/usr/bin/env python2
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from autotest_lib.client.cros.graphics.graphics_utils import GraphicsKernelMemory

class GraphicsKernelMemoryTest(unittest.TestCase):
    def testParseSysfs_i915_gem_objects(self):
        """Test parsing kernel 5.x i915_gem_objects"""
        contents = '''274 shrinkable [0 free] objects, 249675776 bytes

frecon: 3 objects, 72192000 bytes (0 active, 0 inactive, 0 unbound, 0 closed)
chrome: 6 objects, 74629120 bytes (0 active, 0 inactive, 901120 unbound, 0 closed)
chrome: 14 objects, 1765376 bytes (0 active, 552960 inactive, 1212416 unbound, 0 closed)
chrome: 291 objects, 152686592 bytes (0 active, 0 inactive, 1183744 unbound, 0 closed)
chrome: 291 objects, 152686592 bytes (0 active, 75231232 inactive, 1183744 unbound, 0 closed)
chrome: 291 objects, 152686592 bytes (0 active, 155648 inactive, 1183744 unbound, 0 closed)
chrome: 291 objects, 152686592 bytes (64241664 active, 60248064 inactive, 1183744 unbound, 0 closed)
chrome: 291 objects, 152686592 bytes (0 active, 106496 inactive, 1183744 unbound, 0 closed)
chrome: 291 objects, 152686592 bytes (0 active, 724992 inactive, 1183744 unbound, 0 closed)
chrome: 291 objects, 152686592 bytes (0 active, 122880 inactive, 1183744 unbound, 0 closed)
chrome: 291 objects, 152686592 bytes (0 active, 122880 inactive, 1183744 unbound, 0 closed)
chrome: 291 objects, 152686592 bytes (0 active, 122880 inactive, 1183744 unbound, 0 closed)
chrome: 291 objects, 152686592 bytes (0 active, 479232 inactive, 1183744 unbound, 0 closed)
chrome: 291 objects, 152686592 bytes (0 active, 581632 inactive, 1183744 unbound, 0 closed)
[k]contexts: 4 objects, 221184 bytes (0 active, 221184 inactive, 0 unbound, 0 closed)'''
        expected_results = {'bytes': 249675776, 'objects': 274}

        self.assertEqual(expected_results,
                         GraphicsKernelMemory._parse_sysfs(contents))

    def testParseSysfs_i915_gem_objects_kernel_4_4(self):
        """Test parsing kernel 4.x i915_gem_objects"""
        contents = '''557 objects, 100163584 bytes
80 unbound objects, 3653632 bytes
469 bound objects, 95195136 bytes
47 purgeable objects, 2846720 bytes
23 mapped objects, 1757184 bytes
3 display objects (pinned), 8716288 bytes
4294967296 [268435456] gtt total

[k]contexts: 46 objects, 2134016 bytes (0 active, 2134016 inactive, 2134016 global, 0 shared, 0 unbound)
frecon: 3 objects, 12681216 bytes (0 active, 12681216 inactive, 12681216 global, 0 shared, 0 unbound)
chrome: 11 objects, 14626816 bytes (0 active, 12943360 inactive, 12943360 global, 14102528 shared, 1683456 unbound)
chrome: 24 objects, 4411392 bytes (0 active, 3117056 inactive, 0 global, 0 shared, 1294336 unbound)
chrome: 465 objects, 67448832 bytes (25202688 active, 76062720 inactive, 13443072 global, 14102528 shared, 3227648 unbound)
surfaceflinger: 11 objects, 270336 bytes (0 active, 86016 inactive, 0 global, 0 shared, 184320 unbound)'''
        expected_results  = {'bytes': 100163584, 'objects': 557}

        self.assertEqual(expected_results,
                         GraphicsKernelMemory._parse_sysfs(contents))

    def testParseSysfs_i915_gem_gtt(self):
        """Test parsing kernel 4.x i915_gem_gtt"""
        contents = '''   ffff88017a138000:              4KiB 41 00  uncached (pinned x 1) (ggtt offset: ffffe000, size: 00001000, normal) (stolen: 00001000)
   ffff88017a138300:              4KiB 01 01  uncached (pinned x 1) (ggtt offset: ffffd000, size: 00001000, normal)
   ffff88017a138600:     M       92KiB 01 01  uncached dirty (pinned x 1) (ggtt offset: fffe6000, size: 00017000, normal)
   ffff88017a138900:             16KiB 40 40  uncached dirty (pinned x 1) (ggtt offset: 00080000, size: 00004000, normal) (stolen: 00002000)
<snip>
   ffff880147fd4000:             32KiB 76 00  uncached dirty purgeable (pinned x 0) (ggtt offset: 01eae000, size: 00008000, normal) (ppgtt offset: f8010000, size: 00008000) (ppgtt offset: f8000000, size: 00008000) (ppgtt offset: f8010000, size: 00008000) (ppgtt offset: f8010000, size: 00008000) (ppgtt offset: f8010000, size: 00008000) (ppgtt offset: f8000000, size: 00008000)
   ffff88015bd42700: *            4KiB 02 00  uncached dirty (pinned x 0) (ggtt offset: 01dc9000, size: 00001000, normal) (ppgtt offset: ffffff00d000, size: 00001000) (ppgtt offset: ffffff00d000, size: 00001000) (ppgtt offset: ffffff00d000, size: 00001000)
   ffff88006409d800: * Y          4KiB 36 00  uncached dirty (pinned x 0) (ppgtt offset: ffffff04b000, size: 00001000) (ppgtt offset: ffffff04b000, size: 00001000)
Total 470 objects, 99713024 bytes, 32903168 GTT size'''
        expected_results = {'bytes': 99713024, 'objects': 470}

        self.assertEqual(expected_results,
                         GraphicsKernelMemory._parse_sysfs(contents))
