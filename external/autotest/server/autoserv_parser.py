import argparse
import ast
import logging
import os
import shlex
import sys


class autoserv_parser(object):
    """Custom command-line options parser for autoserv.

    We can't use the general getopt methods here, as there will be unknown
    extra arguments that we pass down into the control file instead.
    Thus we process the arguments by hand, for which we are duly repentant.
    Making a single function here just makes it harder to read. Suck it up.
    """
    def __init__(self):
        self.args = sys.argv[1:]
        self.parser = argparse.ArgumentParser(
                usage='%(prog)s [options] [control-file]')
        self.setup_options()

        # parse an empty list of arguments in order to set self.options
        # to default values so that codepaths that assume they are always
        # reached from an autoserv process (when they actually are not)
        # will still work
        self.options = self.parser.parse_args(args=[])


    def setup_options(self):
        """Setup options to call autoserv command.
        """
        self.parser.add_argument('-m', action='store', type=str,
                                 dest='machines',
                                 help='list of machines')
        self.parser.add_argument('-M', action='store', type=str,
                                 dest='machines_file',
                                 help='list of machines from file')
        self.parser.add_argument('-c', action='store_true',
                                 dest='client', default=False,
                                 help='control file is client side')
        self.parser.add_argument('-s', action='store_true',
                                 dest='server', default=False,
                                 help='control file is server side')
        self.parser.add_argument('-r', action='store', type=str,
                                 dest='results', default=None,
                                 help='specify results directory')
        self.parser.add_argument('-l', action='store', type=str,
                                 dest='label', default='',
                                 help='label for the job')
        self.parser.add_argument('-G', action='store', type=str,
                                 dest='group_name', default='',
                                 help='The host_group_name to store in keyvals')
        self.parser.add_argument('-u', action='store', type=str,
                                 dest='user',
                                 default=os.environ.get('USER'),
                                 help='username for the job')
        self.parser.add_argument('-P', action='store', type=str,
                                 dest='parse_job',
                                 default='',
                                 help=('DEPRECATED.'
                                       ' Use --execution-tag instead.'))
        self.parser.add_argument('--execution-tag', action='store',
                                 type=str, dest='execution_tag',
                                 default='',
                                 help=('Accessible in control files as job.tag;'
                                       ' Defaults to the value passed to -P.'))
        self.parser.add_argument('-v', action='store_true',
                                 dest='verify', default=False,
                                 help='verify the machines only')
        self.parser.add_argument('-R', action='store_true',
                                 dest='repair', default=False,
                                 help='repair the machines')
        self.parser.add_argument('-C', '--cleanup', action='store_true',
                                 default=False,
                                 help='cleanup all machines after the job')
        self.parser.add_argument('--provision', action='store_true',
                                 default=False,
                                 help='Provision the machine.')
        self.parser.add_argument('--job-labels', action='store',
                                 help='Comma seperated job labels.')
        self.parser.add_argument('-T', '--reset', action='store_true',
                                 default=False,
                                 help=('Reset (cleanup and verify) all machines'
                                       ' after the job'))
        self.parser.add_argument('-n', action='store_true',
                                 dest='no_tee', default=False,
                                 help='no teeing the status to stdout/err')
        self.parser.add_argument('-N', action='store_true',
                                 dest='no_logging', default=False,
                                 help='no logging')
        self.parser.add_argument('--verbose', action='store_true',
                                 help=('Include DEBUG messages in console '
                                       'output'))
        self.parser.add_argument('--no_console_prefix', action='store_true',
                                 help=('Disable the logging prefix on console '
                                       'output'))
        self.parser.add_argument('-p', '--write-pidfile', action='store_true',
                                 dest='write_pidfile', default=False,
                                 help=('write pidfile (pidfile name is '
                                       'determined by --pidfile-label'))
        self.parser.add_argument('--pidfile-label', action='store',
                                 default='autoserv',
                                 help=('Determines filename to use as pidfile '
                                       '(if -p is specified). Pidfile will be '
                                       '.<label>_execute. Default to '
                                       'autoserv.'))
        self.parser.add_argument('--use-existing-results', action='store_true',
                                 help=('Indicates that autoserv is working with'
                                       ' an existing results directory'))
        self.parser.add_argument('-a', '--args', dest='args',
                                 help='additional args to pass to control file')
        self.parser.add_argument('--ssh-user', action='store',
                                 type=str, dest='ssh_user', default='root',
                                 help='specify the user for ssh connections')
        self.parser.add_argument('--ssh-port', action='store',
                                 type=int, dest='ssh_port', default=22,
                                 help=('specify the port to use for ssh '
                                       'connections'))
        self.parser.add_argument('--ssh-pass', action='store',
                                 type=str, dest='ssh_pass',
                                 default='',
                                 help=('specify the password to use for ssh '
                                       'connections'))
        self.parser.add_argument('--install-in-tmpdir', action='store_true',
                                 dest='install_in_tmpdir', default=False,
                                 help=('by default install autotest clients in '
                                       'a temporary directory'))
        self.parser.add_argument('--collect-crashinfo', action='store_true',
                                 dest='collect_crashinfo', default=False,
                                 help='just run crashinfo collection')
        self.parser.add_argument('--control-filename', action='store',
                                 type=str, default=None,
                                 help=('filename to use for the server control '
                                       'file in the results directory'))
        self.parser.add_argument('--verify_job_repo_url', action='store_true',
                                 dest='verify_job_repo_url', default=False,
                                 help=('Verify that the job_repo_url of the '
                                       'host has staged packages for the job.'))
        self.parser.add_argument('--no_collect_crashinfo', action='store_true',
                                 dest='skip_crash_collection', default=False,
                                 help=('Turns off crash collection to shave '
                                       'time off test runs.'))
        self.parser.add_argument('--disable_sysinfo', action='store_true',
                                 dest='disable_sysinfo', default=False,
                                 help=('Turns off sysinfo collection to shave '
                                       'time off test runs.'))
        self.parser.add_argument('--ssh_verbosity', action='store',
                                 dest='ssh_verbosity', default=0,
                                 type=str, choices=['0', '1', '2', '3'],
                                 help=('Verbosity level for ssh, between 0 '
                                       'and 3 inclusive. '
                                       '[default: %(default)s]'))
        self.parser.add_argument('--ssh_options', action='store',
                                 dest='ssh_options', default='',
                                 help=('A string giving command line flags '
                                       'that will be included in ssh commands'))
        self.parser.add_argument('--require-ssp', action='store_true',
                                 dest='require_ssp', default=False,
                                 help=('Force the autoserv process to run with '
                                       'server-side packaging'))
        self.parser.add_argument('--no_use_packaging', action='store_true',
                                 dest='no_use_packaging', default=False,
                                 help=('Disable install modes that use the '
                                       'packaging system.'))
        self.parser.add_argument('--source_isolate', action='store',
                                 type=str, default='',
                                 dest='isolate',
                                 help=('Hash for isolate containing build '
                                       'contents needed for server-side '
                                       'packaging. Takes precedence over '
                                       'test_source_build, if present.'))
        self.parser.add_argument('--test_source_build', action='store',
                                 type=str, default='',
                                 dest='test_source_build',
                                 help=('Alternative build that contains the '
                                       'test code for server-side packaging. '
                                       'Default is to use the build on the '
                                       'target DUT.'))
        self.parser.add_argument('--parent_job_id', action='store',
                                 type=str, default=None,
                                 dest='parent_job_id',
                                 help=('ID of the parent job. Default to None '
                                       'if the job does not have a parent job'))
        self.parser.add_argument('--host_attributes', action='store',
                                 dest='host_attributes', default='{}',
                                 help=('Host attribute to be applied to all '
                                       'machines/hosts for this autoserv run. '
                                       'Must be a string-encoded dict. '
                                       'Example: {"key1":"value1", "key2":'
                                       '"value2"}'))
        self.parser.add_argument('--lab', action='store', type=str,
                                 dest='lab', default='',
                                 help=argparse.SUPPRESS)
        self.parser.add_argument('--cloud_trace_context', type=str, default='',
                                 action='store', dest='cloud_trace_context',
                                 help=('Global trace context to configure '
                                       'emission of data to Cloud Trace.'))
        self.parser.add_argument('--cloud_trace_context_enabled', type=str,
                                 default='False', action='store',
                                 dest='cloud_trace_context_enabled',
                                 help=('Global trace context to configure '
                                       'emission of data to Cloud Trace.'))
        self.parser.add_argument(
                '--host-info-subdir',
                default='host_info_store',
                help='Optional path to a directory containing host '
                     'information for the machines. The path is relative to '
                     'the results directory (see -r) and must be inside '
                     'the directory.'
        )
        self.parser.add_argument(
                '--local-only-host-info',
                default='False',
                help='By default, host status are immediately reported back to '
                     'the AFE, shadowing the updates to disk. This flag '
                     'disables the AFE interaction completely, and assumes '
                     'that initial host information is supplied to autoserv. '
                     'See also: --host-info-subdir',
        )
        self.parser.add_argument(
                '--control-name',
                action='store',
                help='NAME attribute of the control file to stage and run. '
                     'This overrides the control file provided via the '
                     'positional args.',
        )

        #
        # Warning! Please read before adding any new arguments!
        #
        # New arguments will be ignored if a test runs with server-side
        # packaging and if the test source build does not have the new
        # arguments.
        #
        # New argument should NOT set action to `store_true`. A workaround is to
        # use string value of `True` or `False`, then convert them to boolean in
        # code.
        # The reason is that parse_args will always ignore the argument name and
        # value. An unknown argument without a value will lead to positional
        # argument being removed unexpectedly.
        #


    def parse_args(self):
        """Parse and process command line arguments.
        """
        # Positional arguments from the end of the command line will be included
        # in the list of unknown_args.
        self.options, unknown_args = self.parser.parse_known_args()
        # Filter out none-positional arguments
        removed_args = []
        while unknown_args and unknown_args[0] and unknown_args[0][0] == '-':
            removed_args.append(unknown_args.pop(0))
            # Always assume the argument has a value.
            if unknown_args:
                removed_args.append(unknown_args.pop(0))
        if removed_args:
            logging.warn('Unknown arguments are removed from the options: %s',
                         removed_args)

        self.args = unknown_args + shlex.split(self.options.args or '')

        self.options.host_attributes = ast.literal_eval(
                self.options.host_attributes)
        if self.options.lab and self.options.host_attributes:
            logging.warn(
                    '--lab and --host-attributes are mutually exclusive. '
                    'Ignoring custom host attributes: %s',
                    str(self.options.host_attributes))
            self.options.host_attributes = []

        self.options.local_only_host_info = _interpret_bool_arg(
                self.options.local_only_host_info)
        if not self.options.execution_tag:
            self.options.execution_tag = self.options.parse_job


def _interpret_bool_arg(value):
    return value.lower() in {'yes', 'true'}


# create the one and only one instance of autoserv_parser
autoserv_parser = autoserv_parser()
