# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
The server module contains the objects and methods used to manage servers in
Autotest.

The valid actions are:
list:      list all servers in the database
create:    create a server
delete:    deletes a server
modify:    modify a server's role or status.

The common options are:
--role / -r:     role that's related to server actions.

See topic_common.py for a High Level Design and Algorithm.
"""

import common

from autotest_lib.cli import action_common
from autotest_lib.cli import skylab_utils
from autotest_lib.cli import topic_common
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import global_config
from autotest_lib.client.common_lib import revision_control
# The django setup is moved here as test_that uses sqlite setup. If this line
# is in server_manager, test_that unittest will fail.
from autotest_lib.frontend import setup_django_environment
from autotest_lib.site_utils import server_manager
from autotest_lib.site_utils import server_manager_utils
from chromite.lib import gob_util

try:
    from skylab_inventory import text_manager
    from skylab_inventory import translation_utils
    from skylab_inventory.lib import server as skylab_server
except ImportError:
    pass


RESPECT_SKYLAB_SERVERDB = global_config.global_config.get_config_value(
        'SKYLAB', 'respect_skylab_serverdb', type=bool, default=False)
ATEST_DISABLE_MSG = ('Updating server_db via atest server command has been '
                     'disabled. Please use use go/cros-infra-inventory-tool '
                     'to update it in skylab inventory service.')


class server(topic_common.atest):
    """Server class

    atest server [list|create|delete|modify] <options>
    """
    usage_action = '[list|create|delete|modify]'
    topic = msg_topic = 'server'
    msg_items = '<server>'

    def __init__(self, hostname_required=True, allow_multiple_hostname=False):
        """Add to the parser the options common to all the server actions.

        @param hostname_required: True to require the command has hostname
                                  specified. Default is True.
        """
        super(server, self).__init__()

        self.parser.add_option('-r', '--role',
                               help='Name of a role',
                               type='string',
                               default=None,
                               metavar='ROLE')
        self.parser.add_option('-x', '--action',
                               help=('Set to True to apply actions when role '
                                     'or status is changed, e.g., restart '
                                     'scheduler when a drone is removed. %s' %
                                     skylab_utils.MSG_INVALID_IN_SKYLAB),
                               action='store_true',
                               default=False,
                               metavar='ACTION')

        self.add_skylab_options(enforce_skylab=True)

        self.topic_parse_info = topic_common.item_parse_info(
                attribute_name='hostname', use_leftover=True)

        self.hostname_required = hostname_required
        self.allow_multiple_hostname = allow_multiple_hostname


    def parse(self):
        """Parse command arguments.
        """
        role_info = topic_common.item_parse_info(attribute_name='role')
        kwargs = {}
        if self.hostname_required:
            kwargs['req_items'] = 'hostname'
        (options, leftover) = super(server, self).parse([role_info], **kwargs)
        if options.web_server:
            self.invalid_syntax('Server actions will access server database '
                                'defined in your local global config. It does '
                                'not rely on RPC, no autotest server needs to '
                                'be specified.')

        # self.hostname is a list. Action on server only needs one hostname at
        # most.
        if (not self.hostname and self.hostname_required):
            self.invalid_syntax('`server` topic requires hostname. '
                                'Use -h to see available options.')

        if (self.hostname_required and not self.allow_multiple_hostname and
            len(self.hostname) > 1):
            self.invalid_syntax('`server` topic can only manipulate 1 server. '
                                'Use -h to see available options.')

        if self.hostname:
            if not self.allow_multiple_hostname or not self.skylab:
                # Only support create multiple servers in skylab.
                # Override self.hostname with the first hostname in the list.
                self.hostname = self.hostname[0]

        self.role = options.role

        if self.skylab and self.role:
            translation_utils.validate_server_role(self.role)

        return (options, leftover)


    def output(self, results):
        """Display output.

        For most actions, the return is a string message, no formating needed.

        @param results: return of the execute call.
        """
        print results


class server_help(server):
    """Just here to get the atest logic working. Usage is set by its parent.
    """
    pass


class server_list(action_common.atest_list, server):
    """atest server list [--role <role>]"""

    def __init__(self):
        """Initializer.
        """
        super(server_list, self).__init__(hostname_required=False)

        self.parser.add_option('-s', '--status',
                               help='Only show servers with given status.',
                               type='string',
                               default=None,
                               metavar='STATUS')
        self.parser.add_option('--json',
                               help=('Format output as JSON.'),
                               action='store_true',
                               default=False)
        self.parser.add_option('-N', '--hostnames-only',
                               help=('Only return hostnames.'),
                               action='store_true',
                               default=False)
        # TODO(crbug.com/850344): support '--table' and '--summary' formats.


    def parse(self):
        """Parse command arguments.
        """
        (options, leftover) = super(server_list, self).parse()
        self.json = options.json
        self.status = options.status
        self.namesonly = options.hostnames_only

        if sum([self.json, self.namesonly]) > 1:
            self.invalid_syntax('May only specify up to 1 output-format flag.')
        return (options, leftover)


    def execute_skylab(self):
        """Execute 'atest server list --skylab'

        @return: A list of servers matched the given hostname and role.
        """
        inventory_repo = skylab_utils.InventoryRepo(
                        self.inventory_repo_dir)
        inventory_repo.initialize()
        infrastructure = text_manager.load_infrastructure(
                inventory_repo.get_data_dir())

        return skylab_server.get_servers(
                infrastructure,
                self.environment,
                hostname=self.hostname,
                role=self.role,
                status=self.status)


    def execute(self):
        """Execute the command.

        @return: A list of servers matched given hostname and role.
        """
        if self.skylab:
            try:
                return self.execute_skylab()
            except (skylab_server.SkylabServerActionError,
                    revision_control.GitError,
                    skylab_utils.InventoryRepoDirNotClean) as e:
                self.failure(e, what_failed='Failed to list servers from skylab'
                             ' inventory.', item=self.hostname, fatal=True)
        else:
            try:
                return server_manager_utils.get_servers(
                        hostname=self.hostname,
                        role=self.role,
                        status=self.status)
            except (server_manager_utils.ServerActionError,
                    error.InvalidDataError) as e:
                self.failure(e, what_failed='Failed to find servers',
                             item=self.hostname, fatal=True)


    def output(self, results):
        """Display output.

        @param results: return of the execute call, a list of server object that
                        contains server information.
        """
        if results:
            if self.json:
                if self.skylab:
                    formatter = skylab_server.format_servers_json
                else:
                    formatter = server_manager_utils.format_servers_json
            elif self.namesonly:
                formatter = server_manager_utils.format_servers_nameonly
            else:
                formatter = server_manager_utils.format_servers
            print formatter(results)
        else:
            self.failure('No server is found.',
                         what_failed='Failed to find servers',
                         item=self.hostname, fatal=True)


class server_create(server):
    """atest server create hostname --role <role> --note <note>
    """

    def __init__(self):
        """Initializer.
        """
        super(server_create, self).__init__(allow_multiple_hostname=True)
        self.parser.add_option('-n', '--note',
                               help='note of the server',
                               type='string',
                               default=None,
                               metavar='NOTE')


    def parse(self):
        """Parse command arguments.
        """
        (options, leftover) = super(server_create, self).parse()
        self.note = options.note

        if not self.role:
            self.invalid_syntax('--role is required to create a server.')

        return (options, leftover)


    def execute_skylab(self):
        """Execute the command for skylab inventory changes."""
        inventory_repo = skylab_utils.InventoryRepo(
                self.inventory_repo_dir)
        inventory_repo.initialize()
        data_dir = inventory_repo.get_data_dir()
        infrastructure = text_manager.load_infrastructure(data_dir)

        new_servers = []
        for hostname in self.hostname:
            new_servers.append(skylab_server.create(
                    infrastructure,
                    hostname,
                    self.environment,
                    role=self.role,
                    note=self.note))
        text_manager.dump_infrastructure(data_dir, infrastructure)

        message = skylab_utils.construct_commit_message(
                'Add new server: %s' % self.hostname)
        self.change_number = inventory_repo.upload_change(
                message, draft=self.draft, dryrun=self.dryrun,
                submit=self.submit)

        return new_servers


    def execute(self):
        """Execute the command.

        @return: A Server object if it is created successfully.
        """
        if RESPECT_SKYLAB_SERVERDB:
            self.failure(ATEST_DISABLE_MSG,
                         what_failed='Failed to create server',
                         item=self.hostname, fatal=True)

        if self.skylab:
            try:
                return self.execute_skylab()
            except (skylab_server.SkylabServerActionError,
                    revision_control.GitError,
                    gob_util.GOBError,
                    skylab_utils.InventoryRepoDirNotClean) as e:
                self.failure(e, what_failed='Failed to create server in skylab '
                             'inventory.', item=self.hostname, fatal=True)
        else:
            try:
                return server_manager.create(
                        hostname=self.hostname,
                        role=self.role,
                        note=self.note)
            except (server_manager_utils.ServerActionError,
                    error.InvalidDataError) as e:
                self.failure(e, what_failed='Failed to create server',
                             item=self.hostname, fatal=True)


    def output(self, results):
        """Display output.

        @param results: return of the execute call, a server object that
                        contains server information.
        """
        if results:
            print 'Server %s is added.\n' % self.hostname
            print results

            if self.skylab and not self.dryrun and not self.submit:
                print skylab_utils.get_cl_message(self.change_number)



class server_delete(server):
    """atest server delete hostname"""

    def execute_skylab(self):
        """Execute the command for skylab inventory changes."""
        inventory_repo = skylab_utils.InventoryRepo(
                self.inventory_repo_dir)
        inventory_repo.initialize()
        data_dir = inventory_repo.get_data_dir()
        infrastructure = text_manager.load_infrastructure(data_dir)

        skylab_server.delete(infrastructure, self.hostname, self.environment)
        text_manager.dump_infrastructure(data_dir, infrastructure)

        message = skylab_utils.construct_commit_message(
                'Delete server: %s' % self.hostname)
        self.change_number = inventory_repo.upload_change(
                message, draft=self.draft, dryrun=self.dryrun,
                submit=self.submit)


    def execute(self):
        """Execute the command.

        @return: True if server is deleted successfully.
        """
        if RESPECT_SKYLAB_SERVERDB:
            self.failure(ATEST_DISABLE_MSG,
                         what_failed='Failed to delete server',
                         item=self.hostname, fatal=True)

        if self.skylab:
            try:
                self.execute_skylab()
                return True
            except (skylab_server.SkylabServerActionError,
                    revision_control.GitError,
                    gob_util.GOBError,
                    skylab_utils.InventoryRepoDirNotClean) as e:
                self.failure(e, what_failed='Failed to delete server from '
                             'skylab inventory.', item=self.hostname,
                             fatal=True)
        else:
            try:
                server_manager.delete(hostname=self.hostname)
                return True
            except (server_manager_utils.ServerActionError,
                    error.InvalidDataError) as e:
                self.failure(e, what_failed='Failed to delete server',
                             item=self.hostname, fatal=True)


    def output(self, results):
        """Display output.

        @param results: return of the execute call.
        """
        if results:
            print ('Server %s is deleted.\n' %
                   self.hostname)

            if self.skylab and not self.dryrun and not self.submit:
                print skylab_utils.get_cl_message(self.change_number)



class server_modify(server):
    """atest server modify hostname

    modify action can only change one input at a time. Available inputs are:
    --status:       Status of the server.
    --note:         Note of the server.
    --role:         New role to be added to the server.
    --delete_role:  Existing role to be deleted from the server.
    """

    def __init__(self):
        """Initializer.
        """
        super(server_modify, self).__init__()
        self.parser.add_option('-s', '--status',
                               help='Status of the server',
                               type='string',
                               metavar='STATUS')
        self.parser.add_option('-n', '--note',
                               help='Note of the server',
                               type='string',
                               default=None,
                               metavar='NOTE')
        self.parser.add_option('-d', '--delete',
                               help=('Set to True to delete given role.'),
                               action='store_true',
                               default=False,
                               metavar='DELETE')
        self.parser.add_option('-a', '--attribute',
                               help='Name of the attribute of the server',
                               type='string',
                               default=None,
                               metavar='ATTRIBUTE')
        self.parser.add_option('-e', '--value',
                               help='Value for the attribute of the server',
                               type='string',
                               default=None,
                               metavar='VALUE')


    def parse(self):
        """Parse command arguments.
        """
        (options, leftover) = super(server_modify, self).parse()
        self.status = options.status
        self.note = options.note
        self.delete = options.delete
        self.attribute = options.attribute
        self.value = options.value
        self.action = options.action

        # modify supports various options. However, it's safer to limit one
        # option at a time so no complicated role-dependent logic is needed
        # to handle scenario that both role and status are changed.
        # self.parser is optparse, which does not have function in argparse like
        # add_mutually_exclusive_group. That's why the count is used here.
        flags = [self.status is not None, self.role is not None,
                 self.attribute is not None, self.note is not None]
        if flags.count(True) != 1:
            msg = ('Action modify only support one option at a time. You can '
                   'try one of following 5 options:\n'
                   '1. --status:                Change server\'s status.\n'
                   '2. --note:                  Change server\'s note.\n'
                   '3. --role with optional -d: Add/delete role from server.\n'
                   '4. --attribute --value:     Set/change the value of a '
                   'server\'s attribute.\n'
                   '5. --attribute -d:          Delete the attribute from the '
                   'server.\n'
                   '\nUse option -h to see a complete list of options.')
            self.invalid_syntax(msg)
        if (self.status != None or self.note != None) and self.delete:
            self.invalid_syntax('--delete does not apply to status or note.')
        if self.attribute != None and not self.delete and self.value == None:
            self.invalid_syntax('--attribute must be used with option --value '
                                'or --delete.')

        # TODO(nxia): crbug.com/832964 support --action with --skylab
        if self.skylab and self.action:
            self.invalid_syntax('--action is currently not supported with'
                                ' --skylab.')

        return (options, leftover)


    def execute_skylab(self):
        """Execute the command for skylab inventory changes."""
        inventory_repo = skylab_utils.InventoryRepo(
                        self.inventory_repo_dir)
        inventory_repo.initialize()
        data_dir = inventory_repo.get_data_dir()
        infrastructure = text_manager.load_infrastructure(data_dir)

        target_server = skylab_server.modify(
                infrastructure,
                self.hostname,
                self.environment,
                role=self.role,
                status=self.status,
                delete_role=self.delete,
                note=self.note,
                attribute=self.attribute,
                value=self.value,
                delete_attribute=self.delete)
        text_manager.dump_infrastructure(data_dir, infrastructure)

        status = inventory_repo.git_repo.status()
        if not status:
            print('Nothing is changed for server %s.' % self.hostname)
            return

        message = skylab_utils.construct_commit_message(
                'Modify server: %s' % self.hostname)
        self.change_number = inventory_repo.upload_change(
                message, draft=self.draft, dryrun=self.dryrun,
                submit=self.submit)

        return target_server


    def execute(self):
        """Execute the command.

        @return: The updated server object if it is modified successfully.
        """
        if RESPECT_SKYLAB_SERVERDB:
            self.failure(ATEST_DISABLE_MSG,
                         what_failed='Failed to modify server',
                         item=self.hostname, fatal=True)

        if self.skylab:
            try:
                return self.execute_skylab()
            except (skylab_server.SkylabServerActionError,
                    revision_control.GitError,
                    gob_util.GOBError,
                    skylab_utils.InventoryRepoDirNotClean) as e:
                self.failure(e, what_failed='Failed to modify server in skylab'
                             ' inventory.', item=self.hostname, fatal=True)
        else:
            try:
                return server_manager.modify(
                        hostname=self.hostname, role=self.role,
                        status=self.status, delete=self.delete,
                        note=self.note, attribute=self.attribute,
                        value=self.value, action=self.action)
            except (server_manager_utils.ServerActionError,
                    error.InvalidDataError) as e:
                self.failure(e, what_failed='Failed to modify server',
                             item=self.hostname, fatal=True)


    def output(self, results):
        """Display output.

        @param results: return of the execute call, which is the updated server
                        object.
        """
        if results:
            print 'Server %s is modified.\n' % self.hostname
            print results

            if self.skylab and not self.dryrun and not self.submit:
                print skylab_utils.get_cl_message(self.change_number)
