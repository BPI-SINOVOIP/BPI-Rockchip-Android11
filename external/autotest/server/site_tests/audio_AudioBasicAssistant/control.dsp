# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.common_lib import utils

AUTHOR = "chromeos-chameleon"
NAME = "audio_AudioBasicAssistant.dsp"
PURPOSE = "Remotely controlled assistant test with chameleon."
CRITERIA = """This test will fail if the assistant can't open a tab requested by
voice command."""
TIME = "SHORT"
TEST_CATEGORY = "Functional"
TEST_CLASS = "audio"
TEST_TYPE = "server"
ATTRIBUTES = "suite:chameleon_audio_unstable"
DEPENDENCIES = "audio_box, hotwording"

DOC = """
A basic assistant voice command test with hotword dsp.
We need a DUT with hotwording function, chameleon with speaker and a quiet space
to run the test (audio_box).
"""

args_dict = utils.args_to_dict(args)
chameleon_args = hosts.CrosHost.get_chameleon_arguments(args_dict)

def run(machine):
    job.run_test('audio_AudioBasicAssistant',
            host=hosts.create_host(machine, chameleon_args=chameleon_args),
            enable_dsp_hotword=True)

parallel_simple(run, machines)
