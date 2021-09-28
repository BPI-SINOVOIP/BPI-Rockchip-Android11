#
# Copyright 2007 Google Inc. Released under the GPL v2

"""This is a convenience module to import all available types of hosts.

Implementation details:
You should 'import hosts' instead of importing every available host module.
"""


# host abstract classes
from base_classes import Host
from remote import RemoteHost

# host implementation classes
from ssh_host import SSHHost
from cros_host import CrosHost
from chameleon_host import ChameleonHost
from servo_host import ServoHost
from labstation_host import LabstationHost

# factory function
from factory import create_host
from factory import create_target_machine
