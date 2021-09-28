#
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import logging

from vts.proto import AndroidSystemControlMessage_pb2 as ASysCtrlMsg
from vts.runners.host import const
from vts.runners.host import errors
from vts.runners.host.tcp_client import vts_tcp_client
from vts.runners.host.tcp_server import callback_server
from vts.utils.python.mirror import hal_mirror
from vts.utils.python.mirror import lib_mirror
from vts.utils.python.mirror import shell_mirror
from vts.utils.python.mirror import resource_mirror

_DEFAULT_TARGET_BASE_PATHS = ["/system/lib64/hw"]
_DEFAULT_HWBINDER_SERVICE = "default"
_DEFAULT_SHELL_NAME = "_default"
_MAX_ADB_SHELL_LENGTH = 950


class MirrorTracker(object):
    """The class tracks all mirror objects on the host side.

    Attributes:
        _host_command_port: int, the host-side port for command-response
                            sessions.
        _host_callback_port: int, the host-side port for callback sessions.
        _adb: An AdbProxy object used for interacting with the device via adb.
        _registered_mirrors: dict, key is mirror handler name, value is the
                             mirror object.
        _callback_server: VtsTcpServer, the server that receives and handles
                          callback messages from target side.
        shell_default_nohup: bool, whether to use nohup by default in shell commands.
    """

    def __init__(self,
                 host_command_port,
                 host_callback_port=None,
                 start_callback_server=False,
                 adb=None):
        self._host_command_port = host_command_port
        self._host_callback_port = host_callback_port
        self._adb = adb
        self._registered_mirrors = {}
        self._callback_server = None
        self.shell_default_nohup = False
        if start_callback_server:
            self._StartCallbackServer()

    def __del__(self):
        self.CleanUp()

    def CleanUp(self):
        """Shutdown services and release resources held by the registered mirrors.
        """
        for mirror in self._registered_mirrors.values():
            mirror.CleanUp()
        self._registered_mirrors = {}
        if self._callback_server:
            self._callback_server.Stop()
            self._callback_server = None

    def RemoveMirror(self, mirror_name):
        self._registered_mirrors[mirror_name].CleanUp()
        self._registered_mirrors.pop(mirror_name)

    def _StartCallbackServer(self):
        """Starts the callback server.

        Raises:
            errors.ComponentLoadingError is raised if the callback server fails
            to start.
        """
        self._callback_server = callback_server.CallbackServer()
        _, port = self._callback_server.Start(self._host_callback_port)
        if port != self._host_callback_port:
            raise errors.ComponentLoadingError(
                "Failed to start a callback TcpServer at port %s" %
                self._host_callback_port)

    def Heal(self):
        """Performs a self healing.

        Includes self diagnosis that looks for any framework errors.

        Returns:
            bool, True if everything is ok; False otherwise.
        """
        res = all(map(lambda shell: shell.Heal(), self._registered_mirrors.values()))

        if not res:
            logging.error('Self diagnosis found problems in mirror_tracker.')

        return res

    def InitFmq(self,
                existing_queue=None,
                new_queue_name=None,
                data_type="uint16_t",
                sync=True,
                queue_size=0,
                blocking=False,
                reset_pointers=True,
                client=None):
        """Initializes a fast message queue object.

        This method will initialize a fast message queue object on the target side,
        create a mirror object for the FMQ, and register it in the tracker.

        Args:
            existing_queue: string or MirrorObject.
                This argument identifies an existing queue mirror object.
                If specified, it will tell the target driver to create a
                new message queue object based on an existing message queue.
                If it is None, that means creating a brand new message queue.
            new_queue_name: string, name of the new queue, used as key in the tracker.
                If not specified, this function dynamically generates a name.
            data_type: string, type of data in the queue.
            sync: bool, whether the queue is synchronized (only has one reader).
            queue_size: int, size of the queue.
            blocking: bool, whether blocking is enabled.
            reset_pointers: bool, whether to reset read/write pointers when
                creating a new message queue object based on an existing message queue.
            client: VtsTcpClient, if an existing session should be used.
                If not specified, creates a new one.

        Returns:
            ResourcFmqMirror object,
            it allows users to directly call methods on the mirror object.
        """
        # Check if queue name already exists in tracker.
        if new_queue_name is not None and new_queue_name in self._registered_mirrors:
            logging.error("Queue name already exists in tracker.")
            return None

        # Need to initialize a client if caller doesn't provide one.
        if client is None:
            client = vts_tcp_client.VtsTcpClient()
            client.Connect(
                command_port=self._host_command_port,
                callback_port=self._host_callback_port)

        # Create a new queue by default.
        existing_queue_id = -1
        # Check if caller wants to create a queue object based on
        # an existing queue object.
        if existing_queue is not None:
            # Check if caller provides a string.
            if type(existing_queue) == str:
                if existing_queue in self._registered_mirrors:
                    data_type = self._registered_mirrors[
                        existing_queue].dataType
                    sync = self._registered_mirrors[
                        existing_queue].sync
                    existing_queue_id = self._registered_mirrors[
                        existing_queue].queueId
                else:
                    logging.error("Nonexisting queue name in mirror_tracker.")
                    return None
            # Check if caller provides a resource mirror object.
            elif isinstance(existing_queue, resource_mirror.ResourceFmqMirror):
                data_type = existing_queue.dataType
                sync = existing_queue.sync
                existing_queue_id = existing_queue.queueId
            else:
                logging.error(
                    "Unsupported way of finding an existing queue object.")
                return None

        # Create a resource mirror object.
        mirror = resource_mirror.ResourceFmqMirror(data_type, sync, client)
        mirror._create(existing_queue_id, queue_size, blocking, reset_pointers)
        if mirror.queueId == -1:
            # Failed to create queue object, error logged in resource_mirror.
            return None

        # Needs to dynamically generate queue name if caller doesn't provide one
        if new_queue_name is None:
            new_queue_name = "queue_id_" + str(mirror._queue_id)
        self._registered_mirrors[new_queue_name] = mirror
        return mirror

    def InitHidlMemory(self, mem_size=0, client=None, mem_name=None):
        """Initialize a hidl_memory object.

        This method will initialize a hidl_memory object on the target side,
        create a mirror object, and register it in the tracker.

        Args:
            mem_size: int, size of the memory region.
            client: VtsTcpClient, if an existing session should be used.
                If not specified, creates a new one.
            mem_name: string, name of the memory region.
                If not specified, dynamically assign the memory region a name.

        Returns:
            ResourceHidlMemoryMirror object,
            it allows users to directly call methods on the mirror object.
        """
        # Check if mem_name already exists in tracker.
        if mem_name is not None and mem_name in self._registered_mirrors:
            logging.error("Memory name already exists in tracker.")
            return None

        # Need to initialize a client if caller doesn't provide one.
        if client is None:
            client = vts_tcp_client.VtsTcpClient()
            client.Connect(
                command_port=self._host_command_port,
                callback_port=self._host_callback_port)

        # Create a resource_mirror object.
        mirror = resource_mirror.ResourceHidlMemoryMirror(client)
        mirror._allocate(mem_size)
        if mirror.memId == -1:
            # Failed to create memory object, error logged in resource_mirror.
            return None

        # Need to dynamically assign a memory name
        # if caller doesn't provide one.
        if mem_name is None:
            mem_name = "mem_id_" + str(mirror._mem_id)
        self._registered_mirrors[mem_name] = mirror
        return mirror

    def InitHidlHandleForSingleFile(self,
                                    filepath,
                                    mode,
                                    ints=[],
                                    client=None,
                                    handle_name=None):
        """Initialize a hidl_handle object.

        This method will initialize a hidl_handle object on the target side,
        create a mirror object, and register it in the tracker.
        TODO: Currently only support creating a handle for a single file.
        In the future, need to support arbitrary file descriptor types
        (e.g. socket, pipe), and more than one file.

        Args:
            filepath: string, path to the file.
            mode: string, specifying the mode to open the file.
            ints: int list, useful integers to be stored in handle object.
            client: VtsTcpClient, if an existing session should be used.
                If not specified, create a new one.
            handle_name: string, name of the handle object.
                If not specified, dynamically assign the handle object a name.

        Returns:
            ResourceHidlHandleMirror object,
            it allows users to directly call methods on the mirror object.
        """
        # Check if handle_name already exists in tracker.
        if handle_name is not None and handle_name in self._registered_mirrors:
            logging.error("Handle name already exists in tracker.")
            return None

        # Need to initialize a client if caller doesn't provide one.
        if not client:
            client = vts_tcp_client.VtsTcpClient()
            client.Connect(
                command_port=self._host_command_port,
                callback_port=self._host_callback_port)

        # Create a resource_mirror object.
        mirror = resource_mirror.ResourceHidlHandleMirror(client)
        mirror._createHandleForSingleFile(filepath, mode, ints)
        if mirror.handleId == -1:
            # Failed to create handle object, error logged in resource_mirror.
            return None

        # Need to dynamically assign a handle name
        # if caller doesn't provide one.
        if handle_name is None:
            handle_name = "handle_id_" + str(mirror._handle_id)
        self._registered_mirrors[handle_name] = mirror
        return mirror

    def InitHidlHal(self,
                    target_type,
                    target_version=None,
                    target_package=None,
                    target_component_name=None,
                    target_basepaths=_DEFAULT_TARGET_BASE_PATHS,
                    handler_name=None,
                    hw_binder_service_name=_DEFAULT_HWBINDER_SERVICE,
                    bits=64,
                    target_version_major=None,
                    target_version_minor=None,
                    is_test_hal=False):
        """Initiates a handler for a particular HIDL HAL.

        This will initiate a driver service for a HAL on the target side, create
        a mirror object for a HAL, and register it in the tracker.

        Args:
            target_type: string, the target type name (e.g., light, camera).
            target_version (deprecated, now use major and minor versions):
              float, the target component version (e.g., 1.0).
            target_package: string, the package name of a target HIDL HAL.
            target_basepaths: list of strings, the paths to look for target
                              files in. Default is _DEFAULT_TARGET_BASE_PATHS.
            handler_name: string, the name of the handler. target_type is used
                          by default.
            hw_binder_service_name: string, the name of a HW binder service.
            bits: integer, processor architecture indicator: 32 or 64.
            target_version_major:
              int, the target component major version (e.g., 1.0 -> 1).
            target_version_minor:
              int, the target component minor version (e.g., 1.0 -> 0).
              If host doesn't provide major and minor versions separately,
              parse it from the float version of target_version.
            is_test_hal: bool, whether the HAL service is a test HAL
                         (e.g. msgq).

        Raises:
            USERError if user doesn't provide a version of the HAL service.
        """
        target_version_major, target_version_minor = self.GetTargetVersion(
            target_version, target_version_major, target_version_minor)
        if not handler_name:
            handler_name = target_type
        client = vts_tcp_client.VtsTcpClient()
        client.Connect(
            command_port=self._host_command_port,
            callback_port=self._host_callback_port)
        mirror = hal_mirror.HalMirror(client, self._callback_server)
        mirror.InitHalDriver(target_type, target_version_major,
                             target_version_minor, target_package,
                             target_component_name, hw_binder_service_name,
                             handler_name, bits, is_test_hal)
        self._registered_mirrors[target_type] = mirror

    def InitSharedLib(self,
                      target_type,
                      target_version=None,
                      target_basepaths=_DEFAULT_TARGET_BASE_PATHS,
                      target_package="",
                      target_filename=None,
                      handler_name=None,
                      bits=64,
                      target_version_major=None,
                      target_version_minor=None):
        """Initiates a handler for a particular lib.

        This will initiate a driver service for a lib on the target side, create
        a mirror object for a lib, and register it in the tracker.

        Args:
            target_type: string, the target type name (e.g., light, camera).
            target_version (deprecated, now use major and minor versions):
              float, the target component version (e.g., 1.0).
            target_basepaths: list of strings, the paths to look for target
                             files in. Default is _DEFAULT_TARGET_BASE_PATHS.
            target_package: . separated string (e.g., a.b.c) to denote the
                            package name of target component.
            target_filename: string, the target file name (e.g., libm.so).
            handler_name: string, the name of the handler. target_type is used
                          by default.
            bits: integer, processor architecture indicator: 32 or 64.
            target_version_major:
              int, the target component major version (e.g., 1.0 -> 1).
            target_version_minor:
              int, the target component minor version (e.g., 1.0 -> 0).
            If host doesn't provide major and minor versions separately,
            parse it from the float version of target_version.

        Raises:
            USERError if user doesn't provide a version of the HAL service.
        """
        target_version_major, target_version_minor = self.GetTargetVersion(
            target_version, target_version_major, target_version_minor)
        if not handler_name:
            handler_name = target_type
        client = vts_tcp_client.VtsTcpClient()
        client.Connect(command_port=self._host_command_port)
        mirror = lib_mirror.LibMirror(client)
        mirror.InitLibDriver(target_type, target_version_major,
                             target_version_minor, target_package,
                             target_filename, target_basepaths, handler_name,
                             bits)
        self._registered_mirrors[handler_name] = mirror

    def InvokeTerminal(self, instance_name, bits=32):
        """Initiates a handler for a particular shell terminal.

        This will initiate a driver service for a shell on the target side,
        create a mirror object for the shell, and register it in the tracker.

        Args:
            instance_name: string, the shell terminal instance name.
            bits: integer, processor architecture indicator: 32 or 64.
        """
        if not instance_name:
            raise error.ComponentLoadingError("instance_name is None")
        if bits not in [32, 64]:
            raise error.ComponentLoadingError(
                "Invalid value for bits: %s" % bits)

        if instance_name in self._registered_mirrors:
            logging.warning("shell driver %s already exists", instance_name)
            return

        client = vts_tcp_client.VtsTcpClient()
        client.Connect(command_port=self._host_command_port)

        logging.debug("Init the driver service for shell, %s", instance_name)
        launched = client.LaunchDriverService(
            driver_type=ASysCtrlMsg.VTS_DRIVER_TYPE_SHELL,
            service_name="shell_" + instance_name,
            bits=bits)

        if not launched:
            raise errors.ComponentLoadingError(
                "Failed to launch shell driver service %s" % instance_name)

        mirror = shell_mirror.ShellMirror(client, self._adb)
        self._registered_mirrors[instance_name] = mirror

    def DisableShell(self):
        """Disables all registered shell mirrors."""
        for mirror in self._registered_mirrors.values():
            if not isinstance(mirror, shell_mirror.ShellMirror):
                logging.error("mirror object is not a shell mirror")
                continue
            mirror.enabled = False

    def Execute(self, commands, no_except=False, nohup=None):
        """Execute shell command(s).

        This method automatically decide whether to use adb shell or vts shell
        driver on the device based on performance benchmark results.

        The difference in the decision logic will only have impact on the performance, but
        will be transparent to the user of this method.

        The current logic is:
            1. If shell_default_nohup is disabled and command
               list size is smaller or equal than 3, use adb shell. Otherwise, use
              shell driver (with nohup).

            2. If adb shell is used, no_except will always be true.

            This is subject to further optimization.

        Args:
            commands: string or list or tuple, commands to execute on device.
            no_except: bool, whether to throw exceptions. If set to True,
                       when exception happens, return code will be -1 and
                       str(err) will be in stderr. Result will maintain the
                       same length as with input commands.
            nohup: bool or None, True for using nohup for shell commands; False for
                   not using nohup; None for using default setting.

        Returns:
            dictionary of list, command results that contains stdout,
            stderr, and exit_code.
        """
        if not isinstance(commands, (list, tuple)):
            commands = [commands]

        if nohup is None:
            nohup = self.shell_default_nohup

        # TODO(yuexima): further optimize the threshold and nohup adb command
        non_nohup_adb_threshold = 3
        if (not nohup and len(commands) <= non_nohup_adb_threshold
            and not filter(lambda cmd: len(cmd) > _MAX_ADB_SHELL_LENGTH, commands)):
            return self._ExecuteShellCmdViaAdbShell(commands)
        else:
            return self._ExecuteShellCmdViaVtsDriver(commands, no_except)

    def _ExecuteShellCmdViaVtsDriver(self, commands, no_except):
        """Execute shell command(s) using default shell terminal.

        Args:
            commands: string or list or tuple, commands to execute on device
            no_except: bool, whether to throw exceptions. If set to True,
                       when exception happens, return code will be -1 and
                       str(err) will be in stderr. Result will maintain the
                       same length as with input command.

        Returns:
            dictionary of list, command results that contains stdout,
            stderr, and exit_code.
        """
        if _DEFAULT_SHELL_NAME not in self._registered_mirrors:
            self.InvokeTerminal(_DEFAULT_SHELL_NAME)

        return getattr(self, _DEFAULT_SHELL_NAME).Execute(commands, no_except)

    def _ExecuteShellCmdViaAdbShell(self, commands):
        """Execute shell command(s) using adb shell command.

        Args:
            commands: string or list or tuple, command to execute on device

        Returns:
            dictionary of list, command results that contains stdout,
            stderr, and exit_code.
        """
        all = {const.STDOUT: [],
               const.STDERR: [],
               const.EXIT_CODE: []}

        for cmd in commands:
            res = self._adb.shell(cmd, no_except=True)
            all[const.STDOUT].append(res[const.STDOUT])
            all[const.STDERR].append(res[const.STDERR])
            all[const.EXIT_CODE].append(res[const.EXIT_CODE])

        return all

    def SetConnTimeout(self, timeout):
        """Set remove shell connection timeout for default shell terminal.

        Args:
            timeout: int, TCP connection timeout in seconds.
        """
        if _DEFAULT_SHELL_NAME not in self._registered_mirrors:
            self.InvokeTerminal(_DEFAULT_SHELL_NAME)
        getattr(self, _DEFAULT_SHELL_NAME).SetConnTimeout(timeout)

    def GetTargetVersion(self, target_version, target_version_major,
                         target_version_minor):
        """Get the actual target version provided by the host.

        If the host provides major and minor versions separately, directly return them.
        Otherwise, manually parse it from the float version.
        If all of them are None, raise a user error.

        Args:
            target_version: float, the target component HAL version (e.g. 1.0).
            target_version_major:
                int, the target component HAL major version (e.g. 1.0 -> 1).
            target_version_minor:
                int, the target component HAL minor version (e.g. 1.0 -> 0).

        Returns:
            two integers, actual major and minor HAL versions.

        Raises: user error, if no version is provided.
        """
        # Check if host provides major and minor versions separately
        if (target_version_minor != None and target_version_minor != None):
            return target_version_major, target_version_minor

        # If not, manually parse it from float version
        if (target_version != None):
            target_version_str = str(target_version)
            [target_version_major,
             target_version_minor] = target_version_str.split(".")
            return int(target_version_major), int(target_version_minor)

        raise errors.USERError("User has to provide a target version.")

    def GetTcpClient(self, mirror_name):
        """Gets the TCP client used in this tracker.
        Useful for reusing session to access shared data.

        Args:
            mirror_name: used to identify mirror object.
        """
        if mirror_name in self._registered_mirrors:
            return self._registered_mirrors[mirror_name]._client
        return None

    def __getattr__(self, name):
        if name in self._registered_mirrors:
            return self._registered_mirrors[name]
        else:
            logging.error("No mirror found with name: %s", name)
            return None
