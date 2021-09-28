#!/usr/bin/env python3
#
#   Copyright 2019 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import backoff
import os
import logging
import paramiko
import socket
import time

logging.getLogger("paramiko").setLevel(logging.WARNING)


def get_private_key(ip_address, ssh_config):
    """Tries to load various ssh key types.

    Args:
        ip_address: IP address of ssh server.
        ssh_config: ssh_config location for the ssh server.
    Returns:
        The ssh private key
    """
    exceptions = []
    try:
        logging.debug('Trying to load SSH key type: ed25519')
        return paramiko.ed25519key.Ed25519Key(
            filename=get_ssh_key_for_host(ip_address, ssh_config))
    except paramiko.SSHException as e:
        exceptions.append(e)
        logging.debug('Failed loading SSH key type: ed25519')

    try:
        logging.debug('Trying to load SSH key type: rsa')
        return paramiko.RSAKey.from_private_key_file(
            filename=get_ssh_key_for_host(ip_address, ssh_config))
    except paramiko.SSHException as e:
        exceptions.append(e)
        logging.debug('Failed loading SSH key type: rsa')

    raise Exception('No valid ssh key type found', exceptions)


@backoff.on_exception(
    backoff.constant,
    (paramiko.ssh_exception.SSHException,
     paramiko.ssh_exception.AuthenticationException, socket.timeout,
     socket.error, ConnectionRefusedError, ConnectionResetError),
    interval=1.5,
    max_tries=4)
def create_ssh_connection(ip_address,
                          ssh_username,
                          ssh_config,
                          connect_timeout=30):
    """Creates and ssh connection to a Fuchsia device

    Args:
        ip_address: IP address of ssh server.
        ssh_username: Username for ssh server.
        ssh_config: ssh_config location for the ssh server.
        connect_timeout: Timeout value for connecting to ssh_server.

    Returns:
        A paramiko ssh object
    """
    ssh_key = get_private_key(ip_address=ip_address, ssh_config=ssh_config)
    ssh_client = paramiko.SSHClient()
    ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh_client.connect(hostname=ip_address,
                       username=ssh_username,
                       allow_agent=False,
                       pkey=ssh_key,
                       timeout=connect_timeout,
                       banner_timeout=200)
    return ssh_client


def ssh_is_connected(ssh_client):
    """Checks to see if the SSH connection is alive.
    Args:
        ssh_client: A paramiko SSH client instance.
    Returns:
          True if connected, False or None if not connected.
    """
    return ssh_client and ssh_client.get_transport().is_active()


def get_ssh_key_for_host(host, ssh_config_file):
    """Gets the SSH private key path from a supplied ssh_config_file and the
       host.
    Args:
        host (str): The ip address or host name that SSH will connect to.
        ssh_config_file (str): Path to the ssh_config_file that will be used
            to connect to the host.

    Returns:
        path: A path to the private key for the SSH connection.
    """
    ssh_config = paramiko.SSHConfig()
    user_config_file = os.path.expanduser(ssh_config_file)
    if os.path.exists(user_config_file):
        with open(user_config_file) as f:
            ssh_config.parse(f)
    user_config = ssh_config.lookup(host)

    if 'identityfile' not in user_config:
        raise ValueError('Could not find identity file in %s.' % ssh_config)

    path = os.path.expanduser(user_config['identityfile'][0])
    if not os.path.exists(path):
        raise FileNotFoundError('Specified IdentityFile %s for %s in %s not '
                                'existing anymore.' % (path, host, ssh_config))
    return path


class SshResults:
    """Class representing the results from a SSH command to mimic the output
    of the job.Result class in ACTS.  This is to reduce the changes needed from
    swapping the ssh connection in ACTS to paramiko.

    Attributes:
        stdin: The file descriptor to the input channel of the SSH connection.
        stdout: The file descriptor to the stdout of the SSH connection.
        stderr: The file descriptor to the stderr of the SSH connection.
        exit_status: The file descriptor of the SSH command.
    """
    def __init__(self, stdin, stdout, stderr, exit_status):
        self._stdout = stdout.read().decode('utf-8', errors='replace')
        self._stderr = stderr.read().decode('utf-8', errors='replace')
        self._exit_status = exit_status.recv_exit_status()

    @property
    def stdout(self):
        return self._stdout

    @property
    def stderr(self):
        return self._stderr

    @property
    def exit_status(self):
        return self._exit_status
