#!/usr/bin/python
#
# Copyright 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import importlib
import sys
import unittest

import namespace

test_modules = [
    'anycast_test',
    'bpf_test',
    'csocket_test',
    'cstruct_test',
    'forwarding_test',
    'leak_test',
    'multinetwork_test',
    'neighbour_test',
    'nf_test',
    'pf_key_test',
    'ping6_test',
    'policy_crash_test',
    'qtaguid_test',
    'removed_feature_test',
    'resilient_rs_test',
    'sock_diag_test',
    'srcaddr_selection_test',
    'tcp_fastopen_test',
    'tcp_nuke_addr_test',
    'tcp_repair_test',
    'tcp_test',
    'xfrm_algorithm_test',
    'xfrm_test',
    'xfrm_tunnel_test',
]

if __name__ == '__main__':
  # Check whether ADB over TCP is occupying TCP port 5555.
  if namespace.HasEstablishedTcpSessionOnPort(5555):
    namespace.IfPossibleEnterNewNetworkNamespace()
  # First, run InjectTests on all modules, to ensure that any parameterized
  # tests in those modules are injected.
  for name in test_modules:
    importlib.import_module(name)
    if hasattr(sys.modules[name], 'InjectTests'):
      sys.modules[name].InjectTests()

  loader = unittest.defaultTestLoader
  if len(sys.argv) > 1:
    test_suite = loader.loadTestsFromNames(sys.argv[1:])
  else:
    test_suite = loader.loadTestsFromNames(test_modules)

  assert test_suite.countTestCases() > 0, (
      'Inconceivable: no tests found! Command line: %s' % ' '.join(sys.argv))

  runner = unittest.TextTestRunner(verbosity=2)
  result = runner.run(test_suite)
  sys.exit(not result.wasSuccessful())
