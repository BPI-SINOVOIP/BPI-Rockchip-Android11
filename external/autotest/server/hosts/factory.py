# Copyright (c) 2008 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides a factory method to create a host object."""

import logging
from contextlib import closing

from autotest_lib.client.bin import local_host
from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import deprecation
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import global_config
from autotest_lib.server import utils as server_utils
from autotest_lib.server.cros.dynamic_suite import constants
from autotest_lib.server.hosts import cros_host
from autotest_lib.server.hosts import host_info
from autotest_lib.server.hosts import jetstream_host
from autotest_lib.server.hosts import moblab_host
from autotest_lib.server.hosts import gce_host
from autotest_lib.server.hosts import sonic_host
from autotest_lib.server.hosts import ssh_host
from autotest_lib.server.hosts import labstation_host


CONFIG = global_config.global_config

# Default ssh options used in creating a host.
DEFAULT_SSH_USER = 'root'
DEFAULT_SSH_PASS = ''
DEFAULT_SSH_PORT = 22
DEFAULT_SSH_VERBOSITY = ''
DEFAULT_SSH_OPTIONS = ''

# for tracking which hostnames have already had job_start called
_started_hostnames = set()

# A list of all the possible host types, ordered according to frequency of
# host types in the lab, so the more common hosts don't incur a repeated ssh
# overhead in checking for less common host types.
host_types = [cros_host.CrosHost, labstation_host.LabstationHost,
              moblab_host.MoblabHost, jetstream_host.JetstreamHost,
              sonic_host.SonicHost, gce_host.GceHost]
OS_HOST_DICT = {'cros': cros_host.CrosHost,
                'jetstream': jetstream_host.JetstreamHost,
                'moblab': moblab_host.MoblabHost,
                'labstation': labstation_host.LabstationHost}

# Timeout for early connectivity check to the host, in seconds.
_CONNECTIVITY_CHECK_TIMEOUT_S = 10


def _get_host_arguments(machine):
    """Get parameters to construct a host object.

    There are currently 2 use cases for creating a host.
    1. Through the server_job, in which case the server_job injects
       the appropriate ssh parameters into our name space and they
       are available as the variables ssh_user, ssh_pass etc.
    2. Directly through factory.create_host, in which case we use
       the same defaults as used in the server job to create a host.

    @param machine: machine dict
    @return: A dictionary containing arguments for host specifically hostname,
              afe_host, user, password, port, ssh_verbosity_flag and
              ssh_options.
    """
    hostname, afe_host = server_utils.get_host_info_from_machine(machine)
    connection_pool = server_utils.get_connection_pool_from_machine(machine)
    host_info_store = host_info.get_store_from_machine(machine)
    info = host_info_store.get()

    g = globals()
    user = info.attributes.get('ssh_user', g.get('ssh_user', DEFAULT_SSH_USER))
    password = info.attributes.get('ssh_pass', g.get('ssh_pass',
                                                     DEFAULT_SSH_PASS))
    port = info.attributes.get('ssh_port', g.get('ssh_port', DEFAULT_SSH_PORT))
    ssh_verbosity_flag = info.attributes.get('ssh_verbosity_flag',
                                             g.get('ssh_verbosity_flag',
                                                   DEFAULT_SSH_VERBOSITY))
    ssh_options = info.attributes.get('ssh_options',
                                      g.get('ssh_options',
                                            DEFAULT_SSH_OPTIONS))

    hostname, user, password, port = server_utils.parse_machine(hostname, user,
                                                                password, port)

    host_args = {
            'hostname': hostname,
            'afe_host': afe_host,
            'host_info_store': host_info_store,
            'user': user,
            'password': password,
            'port': int(port),
            'ssh_verbosity_flag': ssh_verbosity_flag,
            'ssh_options': ssh_options,
            'connection_pool': connection_pool,
    }
    return host_args


def _detect_host(connectivity_class, hostname, **args):
    """Detect host type.

    Goes through all the possible host classes, calling check_host with a
    basic host object. Currently this is an ssh host, but theoretically it
    can be any host object that the check_host method of appropriate host
    type knows to use.

    @param connectivity_class: connectivity class to use to talk to the host
                               (ParamikoHost or SSHHost)
    @param hostname: A string representing the host name of the device.
    @param args: Args that will be passed to the constructor of
                 the host class.

    @returns: Class type of the first host class that returns True to the
              check_host method.
    """
    with closing(connectivity_class(hostname, **args)) as host:
        for host_module in host_types:
            logging.info('Attempting to autodetect if host is of type %s',
                         host_module.__name__)
            if host_module.check_host(host, timeout=10):
                return host_module

    logging.warning('Unable to apply conventional host detection methods, '
                    'defaulting to chromeos host.')
    return cros_host.CrosHost


def _choose_connectivity_class(hostname, ssh_port):
    """Choose a connectivity class for this hostname.

    @param hostname: hostname that we need a connectivity class for.
    @param ssh_port: SSH port to connect to the host.

    @returns a connectivity host class.
    """
    if (hostname == 'localhost' and ssh_port == DEFAULT_SSH_PORT):
        return local_host.LocalHost
    else:
        return ssh_host.SSHHost


def _verify_connectivity(connectivity_class, hostname, **args):
    """Verify connectivity to the host.

    Any interaction with an unreachable host is guaranteed to fail later. By
    checking connectivity first, duplicate errors / timeouts can be avoided.
    """
    if connectivity_class == local_host.LocalHost:
        return True

    assert connectivity_class == ssh_host.SSHHost
    with closing(ssh_host.SSHHost(hostname, **args)) as host:
        host.run('test :', timeout=_CONNECTIVITY_CHECK_TIMEOUT_S,
                 ssh_failure_retry_ok=False,
                 ignore_timeout=False)


# TODO(kevcheng): Update the creation method so it's not a research project
# determining the class inheritance model.
def create_host(machine, host_class=None, connectivity_class=None, **args):
    """Create a host object.

    This method mixes host classes that are needed into a new subclass
    and creates a instance of the new class.

    @param machine: A dict representing the device under test or a String
                    representing the DUT hostname (for legacy caller support).
                    If it is a machine dict, the 'hostname' key is required.
                    Optional 'afe_host' key will pipe in afe_host
                    from the autoserv runtime or the AFE.
    @param host_class: Host class to use, if None, will attempt to detect
                       the correct class.
    @param connectivity_class: DEPRECATED. Connectivity class is determined
                               internally.
    @param args: Args that will be passed to the constructor of
                 the new host class.

    @returns: A host object which is an instance of the newly created
              host class.
    """
    # Argument deprecated
    if connectivity_class is not None:
        deprecation.warn('server.create_hosts:connectivity_class')
        connectivity_class = None

    detected_args = _get_host_arguments(machine)
    hostname = detected_args.pop('hostname')
    afe_host = detected_args['afe_host']
    info_store = detected_args['host_info_store'].get()
    args.update(detected_args)
    host_os = None
    full_os_prefix = constants.OS_PREFIX + ':'
    # Let's grab the os from the labels if we can for host class detection.
    for label in info_store.labels:
        if label.startswith(full_os_prefix):
            host_os = label[len(full_os_prefix):]
            break

    connectivity_class = _choose_connectivity_class(hostname, args['port'])
    # TODO(kevcheng): get rid of the host detection using host attributes.
    host_class = (host_class
                  or OS_HOST_DICT.get(afe_host.attributes.get('os_type'))
                  or OS_HOST_DICT.get(host_os))

    if host_class is None:
        # TODO(pprabhu) If we fail to verify connectivity, we skip the costly
        # host autodetection logic. We should ideally just error out in this
        # case, but there are a couple problems:
        # - VMs can take a while to boot up post provision, so SSH connections
        #   to moblab vms may not be available for ~2 minutes. This requires
        #   extended timeout in _verify_connectivity() so we don't get speed
        #   benefits from bailing early.
        # - We need to make sure stopping here does not block repair flows.
        try:
            _verify_connectivity(connectivity_class, hostname, **args)
            host_class = _detect_host(connectivity_class, hostname, **args)
        except (error.AutoservRunError, error.AutoservSSHTimeout):
            logging.exception('Failed to verify connectivity to host.'
                              ' Skipping host auto detection logic.')
            host_class = cros_host.CrosHost
            logging.debug('Defaulting to CrosHost.')

    # create a custom host class for this machine and return an instance of it
    classes = (host_class, connectivity_class)
    custom_host_class = type("%s_host" % hostname, classes, {})
    host_instance = custom_host_class(hostname, **args)

    # call job_start if this is the first time this host is being used
    if hostname not in _started_hostnames:
        host_instance.job_start()
        _started_hostnames.add(hostname)

    return host_instance


def create_target_machine(machine, **kwargs):
    """Create the target machine, accounting for containers.

    @param machine: A dict representing the test bed under test or a String
                    representing the testbed hostname (for legacy caller
                    support).
                    If it is a machine dict, the 'hostname' key is required.
                    Optional 'afe_host' key will pipe in afe_host
                    from the autoserv runtime or the AFE.
    @param kwargs: Keyword args to pass to the testbed initialization.

    @returns: The target machine to be used for verify/repair.
    """
    is_moblab = CONFIG.get_config_value('SSP', 'is_moblab', type=bool,
                                        default=False)
    hostname = machine['hostname'] if isinstance(machine, dict) else machine
    if (utils.is_in_container() and is_moblab and
        hostname in ['localhost', '127.0.0.1']):
        hostname = CONFIG.get_config_value('SSP', 'host_container_ip', type=str,
                                           default=None)
        if isinstance(machine, dict):
            machine['hostname'] = hostname
        else:
            machine = hostname
        logging.debug('Hostname of machine is converted to %s for the test to '
                      'run inside a container.', hostname)
    return create_host(machine, **kwargs)
