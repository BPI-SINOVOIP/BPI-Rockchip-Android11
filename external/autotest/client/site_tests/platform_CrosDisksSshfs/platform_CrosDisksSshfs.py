# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import logging
import json
import os
import tempfile

from autotest_lib.client.bin import test
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.cros_disks import CrosDisksTester


def try_remove(filename):
    try:
        os.remove(filename)
        return True
    except OSError:
        return False


class CrosDisksFuseTester(CrosDisksTester):
    """Common steps for all FUSE-based tests.
    """
    def __init__(self, test, test_configs):
        super(CrosDisksFuseTester, self).__init__(test)
        self._test_configs = test_configs

    def setup_test_case(self, config):
        pass

    def teardown_test_case(self, config):
        pass

    def verify_test_case(self, config, mount_result):
        pass

    def _test_case(self, config):
        logging.info('Testing "%s"', config['description'])
        self.setup_test_case(config)
        try:
            source = config['test_mount_source_uri']
            fstype = config.get('test_mount_filesystem_type')
            options = config.get('test_mount_options', [])
            expected_mount_completion = {
                'status': config['expected_mount_status'],
                'source_path': source,
            }
            if 'expected_mount_path' in config:
                expected_mount_completion['mount_path'] = \
                    config['expected_mount_path']

            self.cros_disks.mount(source, fstype, options)
            result = self.cros_disks.expect_mount_completion(
                    expected_mount_completion)
            try:
                self.verify_test_case(config, result)
            finally:
                self.cros_disks.unmount(source, ['lazy'])
        finally:
            self.teardown_test_case(config)

    def _run_all_test_cases(self):
        try:
            for config in self._test_configs:
                self._test_case(config)
        except RuntimeError:
            cmd = 'ls -la %s' % tempfile.gettempdir()
            logging.debug(utils.run(cmd))
            raise

    def get_tests(self):
        return [self._run_all_test_cases]


SSH_DIR_PATH = '/home/chronos/user/.ssh'
AUTHORIZED_KEYS = os.path.join(SSH_DIR_PATH, 'authorized_keys')
AUTHORIZED_KEYS_BACKUP = AUTHORIZED_KEYS + '.sshfsbak'

# Some discardable SSH key.
SSH_PRIVATE_KEY = '''-----BEGIN RSA PRIVATE KEY-----
MIIEogIBAAKCAQEAvKhQn82O9F+SzDTYgpI+qnCD6E6cYroLvflLp8/onYdqD1xK
ES4wDTGC68DNbS9tIo1hEjwbD79UltQT9NTmJg8DERUrQbNayYXtwxqZ2tSo1Hg5
dpAKLd3GBhwK1Eob+bNgcqEu3iIZq+QRVtlM92Uj4vBFuy8qgvGs4x+n3lACsyk8
4GZGtiFpqTPlTZ6BOEdknZpB0K3HIZ7NjZ8uD9fXJYFuUgQhQvhp1N8aZaf7JtWr
GLQ8Pwq6UYEVb8veHgLVAJ9p/5ko/WNVWf1h78v95pEHSYrQ0opcSDizbquW/1Fs
Fk6elrQcKctJ1FsXMxlWYOzN31yNxcPqT6rzhQIDAQABAoIBAAcD50OZ/DfgGfBY
ArkQQR5LYsxPqAcPzgH5dDPASnEZKPt7PhHXetfywGCN4dWujstbIIHyFDuIrNeS
+U8AX7KIml+XPu2JgtW9kjLQGWqGv+RuuAxNnONJvORbRJfSTaoCXpLEpZ6C/Btl
NrPZDsCgVS5KKv2j6lvGKtyjP7XHiXIXLvlhOJkpWRk4a1IBISP8HPt2w/bG0raD
CW6e6XYYPI4ZPwMlPRympQPGo8mVpNkhFAMHKnaN+E7HplsWXvb0daAVUeCBDVId
QSat88e7PbK2FMsinZvsCZSrHdggS+4u1h6LjMI3GO1PYEjvrMorkHz2w1KS3n1S
n4Eas50CgYEA9JR6JCauiZqJAV4azOylZaeiClkAtsK1IG98XkHyIDfn634U7o5c
6w1Uf0zwxRKx12EPQhzKiYRtp+nPirMAZHmm+gJqExakDV7uJlHNo/6qY7m1Z8I7
Ww/my1Oi5ASQ6Emyrpecvo8xTTl52Kf+l3mQk/EqitZLNWgkX5HdwTMCgYEAxXdh
/DLRDrBz7b+lYahAvBCr+VqUxWdjiarpnC/NZmXIWsI7U0nFpf3H4JzEQVdu5gdV
DKLrU8uw1dGwytgH3zA8s1VMWVg1uvVFduQk+pZeRj9ekGEHvViUEkylg3CaNCyO
2Kl72VS8W/ls5uX74mFcx9fwc9jUue807+406GcCgYAjfpTHQFHeKG4vo5+SE9nh
CdXrWIVRAKrWnTdYWouv/00KEQ8qm8CCYDneC6V5hEAI+M4FEzaVhIGBd94ly9qH
ulvwNn98a7G9OwSmzQJiBWhm9qGMAFUq3wDoiye9nagF/gQPcHNP+Gn4Qhobxi2d
gAfqYHqDEZxykL2OnRWonwKBgBvcl1+9T9ARx5mxI8WettuSQqGhTUJ5Lws6qVGX
URT0oYtkwngi/ZdJMo2XsP1DN+uO90ocJrYhFGdm+dn1F08/gCERlP86OgKSHuYC
lNEirFSfFlmqxyvJNsNKO0RLfAaGjvU1HLtygE096Ua/BoZPlIbCCjReUM2XWdHM
u3xbAoGARqG0gGpNCY7pEjSQ33TdLEXV7O0S4hJiN+IKPS2q8q8k21X7ckkPxsEG
h4dIuHzdLGZsmIXel4Mx3rvyKbboj93K3ia5rbU05keVi9duMv1PlbdQzu9mq2qu
A5CmV2fYpStHZTHsv5BcYWxkhc4aAmvUJwyAzlWEhyijwFK5wSQ=
-----END RSA PRIVATE KEY-----
'''

SSH_PUBLIC_KEY = 'ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC8qFCfzY' \
        '70X5LMNNiCkj6qcIPoTpxiugu9+Uunz+idh2oPXEoRLjANMYLrwM1tL' \
        '20ijWESPBsPv1SW1BP01OYmDwMRFStBs1rJhe3DGpna1KjUeDl2kAot' \
        '3cYGHArUShv5s2ByoS7eIhmr5BFW2Uz3ZSPi8EW7LyqC8azjH6feUAK' \
        'zKTzgZka2IWmpM+VNnoE4R2SdmkHQrcchns2Nny4P19clgW5SBCFC+G' \
        'nU3xplp/sm1asYtDw/CrpRgRVvy94eAtUAn2n/mSj9Y1VZ/WHvy/3mk' \
        'QdJitDSilxIOLNuq5b/UWwWTp6WtBwpy0nUWxczGVZg7M3fXI3Fw+pP' \
        'qvOF root@localhost'


class CrosDisksSshfsTester(CrosDisksFuseTester):
    """A tester to verify sshfs support in CrosDisks.
    """
    def __init__(self, test, test_configs):
        super(CrosDisksSshfsTester, self).__init__(test, test_configs)

    def setup_test_case(self, config):
        if os.path.exists(AUTHORIZED_KEYS):
            # Make backup of the current authorized_keys
            utils.run('mv -f ' + AUTHORIZED_KEYS + ' ' + AUTHORIZED_KEYS_BACKUP,
                      ignore_status=True)

        self._register_key(SSH_PUBLIC_KEY)

        identity = base64.b64encode(SSH_PRIVATE_KEY)
        known_hosts = base64.b64encode(self._generate_known_hosts())

        options = config.get('test_mount_options', [])
        options.append('IdentityBase64=' + identity)
        options.append('UserKnownHostsBase64=' + known_hosts)
        config['test_mount_options'] = options

    def teardown_test_case(self, config):
        if os.path.exists(AUTHORIZED_KEYS_BACKUP):
            # Restore authorized_keys from backup.
            utils.run('mv -f ' + AUTHORIZED_KEYS_BACKUP + ' ' + AUTHORIZED_KEYS,
                      ignore_status=True)

    def verify_test_case(self, config, mount_result):
        if 'expected_file' in config:
            f = config['expected_file']
            if not os.path.exists(f):
                raise error.TestFail('Expected file "' + f + '" not found')

    def _register_key(self, pubkey):
        utils.run('sudo -u chronos mkdir -p ' + SSH_DIR_PATH,
                  ignore_status=True)
        utils.run('sudo -u chronos touch ' + AUTHORIZED_KEYS)
        with open(AUTHORIZED_KEYS, 'wb') as f:
            f.write(pubkey)
        utils.run('sudo -u chronos chmod 0600 ' + AUTHORIZED_KEYS)

    def _generate_known_hosts(self):
        hostkey = '/mnt/stateful_partition/etc/ssh/ssh_host_ed25519_key.pub'
        with open(hostkey, 'rb') as f:
            keydata = f.readline().split()
        return 'localhost {} {}\n'.format(keydata[0], keydata[1])


class platform_CrosDisksSshfs(test.test):
    version = 1

    def run_once(self, *args, **kwargs):
        test_configs = []
        config_file = '%s/%s' % (self.bindir, kwargs['config_file'])
        with open(config_file, 'rb') as f:
            test_configs.extend(json.load(f))

        tester = CrosDisksSshfsTester(self, test_configs)
        tester.run(*args, **kwargs)
