# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Config file for pass level bisection

Provides a mapping from pass info from -opt-bisect result to DebugCounter name.
"""
pass_name = {
    # The list now contains all the passes in LLVM that support DebugCounter at
    # transformation level.
    # We will need to keep updating this map after more DebugCounter added to
    # each pass in LLVM.
    # For users who make local changes to passes, please add a map from pass
    # description to newly introduced DebugCounter name for transformation
    # level bisection purpose.
    'Hoist/decompose integer division and remainder':
        'div-rem-pairs-transform',
    'Early CSE':
        'early-cse',
    'Falkor HW Prefetch Fix Late Phase':
        'falkor-hwpf',
    'Combine redundant instructions':
        'instcombine-visit',
    'Machine Copy Propagation Pass':
        'machine-cp-fwd',
    'Global Value Numbering':
        'newgvn-phi',
    'PredicateInfo Printer':
        'predicateinfo-rename',
    'SI Insert Waitcnts':
        'si-insert-waitcnts-forceexp',
}
