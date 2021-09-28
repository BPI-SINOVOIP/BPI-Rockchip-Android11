# Copyright 2016 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Common Utilities."""
# pylint: disable=too-many-lines
from __future__ import print_function

from distutils.spawn import find_executable
import base64
import binascii
import collections
import errno
import getpass
import grp
import logging
import os
import platform
import shlex
import shutil
import signal
import struct
import socket
import subprocess
import sys
import tarfile
import tempfile
import time
import uuid
import webbrowser
import zipfile

import six

from acloud import errors
from acloud.internal import constants


logger = logging.getLogger(__name__)

SSH_KEYGEN_CMD = ["ssh-keygen", "-t", "rsa", "-b", "4096"]
SSH_KEYGEN_PUB_CMD = ["ssh-keygen", "-y"]
SSH_ARGS = ["-o", "UserKnownHostsFile=/dev/null",
            "-o", "StrictHostKeyChecking=no"]
SSH_CMD = ["ssh"] + SSH_ARGS
SCP_CMD = ["scp"] + SSH_ARGS
GET_BUILD_VAR_CMD = ["build/soong/soong_ui.bash", "--dumpvar-mode"]
DEFAULT_RETRY_BACKOFF_FACTOR = 1
DEFAULT_SLEEP_MULTIPLIER = 0

_SSH_TUNNEL_ARGS = ("-i %(rsa_key_file)s -o UserKnownHostsFile=/dev/null "
                    "-o StrictHostKeyChecking=no "
                    "-L %(vnc_port)d:127.0.0.1:%(target_vnc_port)d "
                    "-L %(adb_port)d:127.0.0.1:%(target_adb_port)d "
                    "-N -f -l %(ssh_user)s %(ip_addr)s")
_ADB_CONNECT_ARGS = "connect 127.0.0.1:%(adb_port)d"
# Store the ports that vnc/adb are forwarded to, both are integers.
ForwardedPorts = collections.namedtuple("ForwardedPorts", [constants.VNC_PORT,
                                                           constants.ADB_PORT])
AVD_PORT_DICT = {
    constants.TYPE_GCE: ForwardedPorts(constants.GCE_VNC_PORT,
                                       constants.GCE_ADB_PORT),
    constants.TYPE_CF: ForwardedPorts(constants.CF_VNC_PORT,
                                      constants.CF_ADB_PORT),
    constants.TYPE_GF: ForwardedPorts(constants.GF_VNC_PORT,
                                      constants.GF_ADB_PORT),
    constants.TYPE_CHEEPS: ForwardedPorts(constants.CHEEPS_VNC_PORT,
                                          constants.CHEEPS_ADB_PORT)
}

_VNC_BIN = "ssvnc"
_CMD_KILL = ["pkill", "-9", "-f"]
_CMD_SG = "sg "
_CMD_START_VNC = "%(bin)s vnc://127.0.0.1:%(port)d"
_CMD_INSTALL_SSVNC = "sudo apt-get --assume-yes install ssvnc"
_ENV_DISPLAY = "DISPLAY"
_SSVNC_ENV_VARS = {"SSVNC_NO_ENC_WARN": "1", "SSVNC_SCALE": "auto", "VNCVIEWER_X11CURSOR": "1"}
_DEFAULT_DISPLAY_SCALE = 1.0
_DIST_DIR = "DIST_DIR"

# For webrtc
_WEBRTC_URL = "https://"
_WEBRTC_PORT = "8443"

_CONFIRM_CONTINUE = ("In order to display the screen to the AVD, we'll need to "
                     "install a vnc client (ssvnc). \nWould you like acloud to "
                     "install it for you? (%s) \nPress 'y' to continue or "
                     "anything else to abort it[y/N]: ") % _CMD_INSTALL_SSVNC
_EvaluatedResult = collections.namedtuple("EvaluatedResult",
                                          ["is_result_ok", "result_message"])
# dict of supported system and their distributions.
_SUPPORTED_SYSTEMS_AND_DISTS = {"Linux": ["Ubuntu", "Debian"]}
_DEFAULT_TIMEOUT_ERR = "Function did not complete within %d secs."
_SSVNC_VIEWER_PATTERN = "vnc://127.0.0.1:%(vnc_port)d"


class TempDir(object):
    """A context manager that ceates a temporary directory.

    Attributes:
        path: The path of the temporary directory.
    """

    def __init__(self):
        self.path = tempfile.mkdtemp()
        os.chmod(self.path, 0o700)
        logger.debug("Created temporary dir %s", self.path)

    def __enter__(self):
        """Enter."""
        return self.path

    def __exit__(self, exc_type, exc_value, traceback):
        """Exit.

        Args:
            exc_type: Exception type raised within the context manager.
                      None if no execption is raised.
            exc_value: Exception instance raised within the context manager.
                       None if no execption is raised.
            traceback: Traceback for exeception that is raised within
                       the context manager.
                       None if no execption is raised.
        Raises:
            EnvironmentError or OSError when failed to delete temp directory.
        """
        try:
            if self.path:
                shutil.rmtree(self.path)
                logger.debug("Deleted temporary dir %s", self.path)
        except EnvironmentError as e:
            # Ignore error if there is no exception raised
            # within the with-clause and the EnvironementError is
            # about problem that directory or file does not exist.
            if not exc_type and e.errno != errno.ENOENT:
                raise
        except Exception as e:  # pylint: disable=W0703
            if exc_type:
                logger.error(
                    "Encountered error while deleting %s: %s",
                    self.path,
                    str(e),
                    exc_info=True)
            else:
                raise


def RetryOnException(retry_checker,
                     max_retries,
                     sleep_multiplier=0,
                     retry_backoff_factor=1):
    """Decorater which retries the function call if |retry_checker| returns true.

    Args:
        retry_checker: A callback function which should take an exception instance
                       and return True if functor(*args, **kwargs) should be retried
                       when such exception is raised, and return False if it should
                       not be retried.
        max_retries: Maximum number of retries allowed.
        sleep_multiplier: Will sleep sleep_multiplier * attempt_count seconds if
                          retry_backoff_factor is 1.  Will sleep
                          sleep_multiplier * (
                              retry_backoff_factor ** (attempt_count -  1))
                          if retry_backoff_factor != 1.
        retry_backoff_factor: See explanation of sleep_multiplier.

    Returns:
        The function wrapper.
    """

    def _Wrapper(func):
        def _FunctionWrapper(*args, **kwargs):
            return Retry(retry_checker, max_retries, func, sleep_multiplier,
                         retry_backoff_factor, *args, **kwargs)

        return _FunctionWrapper

    return _Wrapper


def Retry(retry_checker, max_retries, functor, sleep_multiplier,
          retry_backoff_factor, *args, **kwargs):
    """Conditionally retry a function.

    Args:
        retry_checker: A callback function which should take an exception instance
                       and return True if functor(*args, **kwargs) should be retried
                       when such exception is raised, and return False if it should
                       not be retried.
        max_retries: Maximum number of retries allowed.
        functor: The function to call, will call functor(*args, **kwargs).
        sleep_multiplier: Will sleep sleep_multiplier * attempt_count seconds if
                          retry_backoff_factor is 1.  Will sleep
                          sleep_multiplier * (
                              retry_backoff_factor ** (attempt_count -  1))
                          if retry_backoff_factor != 1.
        retry_backoff_factor: See explanation of sleep_multiplier.
        *args: Arguments to pass to the functor.
        **kwargs: Key-val based arguments to pass to the functor.

    Returns:
        The return value of the functor.

    Raises:
        Exception: The exception that functor(*args, **kwargs) throws.
    """
    attempt_count = 0
    while attempt_count <= max_retries:
        try:
            attempt_count += 1
            return_value = functor(*args, **kwargs)
            return return_value
        except Exception as e:  # pylint: disable=W0703
            if retry_checker(e) and attempt_count <= max_retries:
                if retry_backoff_factor != 1:
                    sleep = sleep_multiplier * (retry_backoff_factor**
                                                (attempt_count - 1))
                else:
                    sleep = sleep_multiplier * attempt_count
                time.sleep(sleep)
            else:
                raise


def RetryExceptionType(exception_types, max_retries, functor, *args, **kwargs):
    """Retry exception if it is one of the given types.

    Args:
        exception_types: A tuple of exception types, e.g. (ValueError, KeyError)
        max_retries: Max number of retries allowed.
        functor: The function to call. Will be retried if exception is raised and
                 the exception is one of the exception_types.
        *args: Arguments to pass to Retry function.
        **kwargs: Key-val based arguments to pass to Retry functions.

    Returns:
        The value returned by calling functor.
    """
    return Retry(lambda e: isinstance(e, exception_types), max_retries,
                 functor, *args, **kwargs)


def PollAndWait(func, expected_return, timeout_exception, timeout_secs,
                sleep_interval_secs, *args, **kwargs):
    """Call a function until the function returns expected value or times out.

    Args:
        func: Function to call.
        expected_return: The expected return value.
        timeout_exception: Exception to raise when it hits timeout.
        timeout_secs: Timeout seconds.
                      If 0 or less than zero, the function will run once and
                      we will not wait on it.
        sleep_interval_secs: Time to sleep between two attemps.
        *args: list of args to pass to func.
        **kwargs: dictionary of keyword based args to pass to func.

    Raises:
        timeout_exception: if the run of function times out.
    """
    # TODO(fdeng): Currently this method does not kill
    # |func|, if |func| takes longer than |timeout_secs|.
    # We can use a more robust version from chromite.
    start = time.time()
    while True:
        return_value = func(*args, **kwargs)
        if return_value == expected_return:
            return
        elif time.time() - start > timeout_secs:
            raise timeout_exception
        else:
            if sleep_interval_secs > 0:
                time.sleep(sleep_interval_secs)


def GenerateUniqueName(prefix=None, suffix=None):
    """Generate a random unique name using uuid4.

    Args:
        prefix: String, desired prefix to prepend to the generated name.
        suffix: String, desired suffix to append to the generated name.

    Returns:
        String, a random name.
    """
    name = uuid.uuid4().hex
    if prefix:
        name = "-".join([prefix, name])
    if suffix:
        name = "-".join([name, suffix])
    return name


def MakeTarFile(src_dict, dest):
    """Archive files in tar.gz format to a file named as |dest|.

    Args:
        src_dict: A dictionary that maps a path to be archived
                  to the corresponding name that appears in the archive.
        dest: String, path to output file, e.g. /tmp/myfile.tar.gz
    """
    logger.info("Compressing %s into %s.", src_dict.keys(), dest)
    with tarfile.open(dest, "w:gz") as tar:
        for src, arcname in six.iteritems(src_dict):
            tar.add(src, arcname=arcname)

def CreateSshKeyPairIfNotExist(private_key_path, public_key_path):
    """Create the ssh key pair if they don't exist.

    Case1. If the private key doesn't exist, we will create both the public key
           and the private key.
    Case2. If the private key exists but public key doesn't, we will create the
           public key by using the private key.
    Case3. If the public key exists but the private key doesn't, we will create
           a new private key and overwrite the public key.

    Args:
        private_key_path: Path to the private key file.
                          e.g. ~/.ssh/acloud_rsa
        public_key_path: Path to the public key file.
                         e.g. ~/.ssh/acloud_rsa.pub

    Raises:
        error.DriverError: If failed to create the key pair.
    """
    public_key_path = os.path.expanduser(public_key_path)
    private_key_path = os.path.expanduser(private_key_path)
    public_key_exist = os.path.exists(public_key_path)
    private_key_exist = os.path.exists(private_key_path)
    if public_key_exist and private_key_exist:
        logger.debug(
            "The ssh private key (%s) and public key (%s) already exist,"
            "will not automatically create the key pairs.", private_key_path,
            public_key_path)
        return
    key_folder = os.path.dirname(private_key_path)
    if not os.path.exists(key_folder):
        os.makedirs(key_folder)
    try:
        if private_key_exist:
            cmd = SSH_KEYGEN_PUB_CMD + ["-f", private_key_path]
            with open(public_key_path, 'w') as outfile:
                stream_content = subprocess.check_output(cmd)
                outfile.write(
                    stream_content.rstrip('\n') + " " + getpass.getuser())
            logger.info(
                "The ssh public key (%s) do not exist, "
                "automatically creating public key, calling: %s",
                public_key_path, " ".join(cmd))
        else:
            cmd = SSH_KEYGEN_CMD + [
                "-C", getpass.getuser(), "-f", private_key_path
            ]
            logger.info(
                "Creating public key from private key (%s) via cmd: %s",
                private_key_path, " ".join(cmd))
            subprocess.check_call(cmd, stdout=sys.stderr, stderr=sys.stdout)
    except subprocess.CalledProcessError as e:
        raise errors.DriverError("Failed to create ssh key pair: %s" % str(e))
    except OSError as e:
        raise errors.DriverError(
            "Failed to create ssh key pair, please make sure "
            "'ssh-keygen' is installed: %s" % str(e))

    # By default ssh-keygen will create a public key file
    # by append .pub to the private key file name. Rename it
    # to what's requested by public_key_path.
    default_pub_key_path = "%s.pub" % private_key_path
    try:
        if default_pub_key_path != public_key_path:
            os.rename(default_pub_key_path, public_key_path)
    except OSError as e:
        raise errors.DriverError(
            "Failed to rename %s to %s: %s" % (default_pub_key_path,
                                               public_key_path, str(e)))

    logger.info("Created ssh private key (%s) and public key (%s)",
                private_key_path, public_key_path)


def VerifyRsaPubKey(rsa):
    """Verify the format of rsa public key.

    Args:
        rsa: content of rsa public key. It should follow the format of
             ssh-rsa AAAAB3NzaC1yc2EA.... test@test.com

    Raises:
        DriverError if the format is not correct.
    """
    if not rsa or not all(ord(c) < 128 for c in rsa):
        raise errors.DriverError(
            "rsa key is empty or contains non-ascii character: %s" % rsa)

    elements = rsa.split()
    if len(elements) != 3:
        raise errors.DriverError("rsa key is invalid, wrong format: %s" % rsa)

    key_type, data, _ = elements
    try:
        binary_data = base64.decodestring(data)
        # number of bytes of int type
        int_length = 4
        # binary_data is like "7ssh-key..." in a binary format.
        # The first 4 bytes should represent 7, which should be
        # the length of the following string "ssh-key".
        # And the next 7 bytes should be string "ssh-key".
        # We will verify that the rsa conforms to this format.
        # ">I" in the following line means "big-endian unsigned integer".
        type_length = struct.unpack(">I", binary_data[:int_length])[0]
        if binary_data[int_length:int_length + type_length] != key_type:
            raise errors.DriverError("rsa key is invalid: %s" % rsa)
    except (struct.error, binascii.Error) as e:
        raise errors.DriverError(
            "rsa key is invalid: %s, error: %s" % (rsa, str(e)))


def Decompress(sourcefile, dest=None):
    """Decompress .zip or .tar.gz.

    Args:
        sourcefile: A string, a source file path to decompress.
        dest: A string, a folder path as decompress destination.

    Raises:
        errors.UnsupportedCompressionFileType: Not supported extension.
    """
    logger.info("Start to decompress %s!", sourcefile)
    dest_path = dest if dest else "."
    if sourcefile.endswith(".tar.gz"):
        with tarfile.open(sourcefile, "r:gz") as compressor:
            compressor.extractall(dest_path)
    elif sourcefile.endswith(".zip"):
        with zipfile.ZipFile(sourcefile, 'r') as compressor:
            compressor.extractall(dest_path)
    else:
        raise errors.UnsupportedCompressionFileType(
            "Sorry, we could only support compression file type "
            "for zip or tar.gz.")


# pylint: disable=old-style-class,no-init
class TextColors:
    """A class that defines common color ANSI code."""

    HEADER = "\033[95m"
    OKBLUE = "\033[94m"
    OKGREEN = "\033[92m"
    WARNING = "\033[33m"
    FAIL = "\033[91m"
    ENDC = "\033[0m"
    BOLD = "\033[1m"
    UNDERLINE = "\033[4m"


def PrintColorString(message, colors=TextColors.OKBLUE, **kwargs):
    """A helper function to print out colored text.

    Use print function "print(message, end="")" to show message in one line.
    Example code:
        DisplayMessages("Creating GCE instance...", end="")
        # Job execute 20s
        DisplayMessages("Done! (20s)")
    Display:
        Creating GCE instance...
        # After job finished, messages update as following:
        Creating GCE instance...Done! (20s)

    Args:
        message: String, the message text.
        colors: String, color code.
        **kwargs: dictionary of keyword based args to pass to func.
    """
    print(colors + message + TextColors.ENDC, **kwargs)
    sys.stdout.flush()


def InteractWithQuestion(question, colors=TextColors.WARNING):
    """A helper function to define the common way to run interactive cmd.

    Args:
        question: String, the question to ask user.
        colors: String, color code.

    Returns:
        String, input from user.
    """
    return str(six.moves.input(colors + question + TextColors.ENDC).strip())


def GetUserAnswerYes(question):
    """Ask user about acloud setup question.

    Args:
        question: String of question for user. Enter is equivalent to pressing
                  n. We should hint user with upper case N surrounded in square
                  brackets.
                  Ex: "Are you sure to change bucket name[y/N]:"

    Returns:
        Boolean, True if answer is "Yes", False otherwise.
    """
    answer = InteractWithQuestion(question)
    return answer.lower() in constants.USER_ANSWER_YES


class BatchHttpRequestExecutor(object):
    """A helper class that executes requests in batch with retry.

    This executor executes http requests in a batch and retry
    those that have failed. It iteratively updates the dictionary
    self._final_results with latest results, which can be retrieved
    via GetResults.
    """

    def __init__(self,
                 execute_once_functor,
                 requests,
                 retry_http_codes=None,
                 max_retry=None,
                 sleep=None,
                 backoff_factor=None,
                 other_retriable_errors=None):
        """Initializes the executor.

        Args:
            execute_once_functor: A function that execute requests in batch once.
                                  It should return a dictionary like
                                  {request_id: (response, exception)}
            requests: A dictionary where key is request id picked by caller,
                      and value is a apiclient.http.HttpRequest.
            retry_http_codes: A list of http codes to retry.
            max_retry: See utils.Retry.
            sleep: See utils.Retry.
            backoff_factor: See utils.Retry.
            other_retriable_errors: A tuple of error types that should be retried
                                    other than errors.HttpError.
        """
        self._execute_once_functor = execute_once_functor
        self._requests = requests
        # A dictionary that maps request id to pending request.
        self._pending_requests = {}
        # A dictionary that maps request id to a tuple (response, exception).
        self._final_results = {}
        self._retry_http_codes = retry_http_codes
        self._max_retry = max_retry
        self._sleep = sleep
        self._backoff_factor = backoff_factor
        self._other_retriable_errors = other_retriable_errors

    def _ShoudRetry(self, exception):
        """Check if an exception is retriable.

        Args:
            exception: An exception instance.
        """
        if isinstance(exception, self._other_retriable_errors):
            return True

        if (isinstance(exception, errors.HttpError)
                and exception.code in self._retry_http_codes):
            return True
        return False

    def _ExecuteOnce(self):
        """Executes pending requests and update it with failed, retriable ones.

        Raises:
            HasRetriableRequestsError: if some requests fail and are retriable.
        """
        results = self._execute_once_functor(self._pending_requests)
        # Update final_results with latest results.
        self._final_results.update(results)
        # Clear pending_requests
        self._pending_requests.clear()
        for request_id, result in six.iteritems(results):
            exception = result[1]
            if exception is not None and self._ShoudRetry(exception):
                # If this is a retriable exception, put it in pending_requests
                self._pending_requests[request_id] = self._requests[request_id]
        if self._pending_requests:
            # If there is still retriable requests pending, raise an error
            # so that Retry will retry this function with pending_requests.
            raise errors.HasRetriableRequestsError(
                "Retriable errors: %s" %
                [str(results[rid][1]) for rid in self._pending_requests])

    def Execute(self):
        """Executes the requests and retry if necessary.

        Will populate self._final_results.
        """

        def _ShouldRetryHandler(exc):
            """Check if |exc| is a retriable exception.

            Args:
                exc: An exception.

            Returns:
                True if exception is of type HasRetriableRequestsError; False otherwise.
            """
            should_retry = isinstance(exc, errors.HasRetriableRequestsError)
            if should_retry:
                logger.info("Will retry failed requests.", exc_info=True)
                logger.info("%s", exc)
            return should_retry

        try:
            self._pending_requests = self._requests.copy()
            Retry(
                _ShouldRetryHandler,
                max_retries=self._max_retry,
                functor=self._ExecuteOnce,
                sleep_multiplier=self._sleep,
                retry_backoff_factor=self._backoff_factor)
        except errors.HasRetriableRequestsError:
            logger.debug("Some requests did not succeed after retry.")

    def GetResults(self):
        """Returns final results.

        Returns:
            results, a dictionary in the following format
            {request_id: (response, exception)}
            request_ids are those from requests; response
            is the http response for the request or None on error;
            exception is an instance of DriverError or None if no error.
        """
        return self._final_results


def DefaultEvaluator(result):
    """Default Evaluator always return result is ok.

    Args:
        result:the return value of the target function.

    Returns:
        _EvaluatedResults namedtuple.
    """
    return _EvaluatedResult(is_result_ok=True, result_message=result)


def ReportEvaluator(report):
    """Evalute the acloud operation by the report.

    Args:
        report: acloud.public.report() object.

    Returns:
        _EvaluatedResults namedtuple.
    """
    if report is None or report.errors:
        return _EvaluatedResult(is_result_ok=False,
                                result_message=report.errors)

    return _EvaluatedResult(is_result_ok=True, result_message=None)


def BootEvaluator(boot_dict):
    """Evaluate if the device booted successfully.

    Args:
        boot_dict: Dict of instance_name:boot error.

    Returns:
        _EvaluatedResults namedtuple.
    """
    if boot_dict:
        return _EvaluatedResult(is_result_ok=False, result_message=boot_dict)
    return _EvaluatedResult(is_result_ok=True, result_message=None)


class TimeExecute(object):
    """Count the function execute time."""

    def __init__(self, function_description=None, print_before_call=True,
                 print_status=True, result_evaluator=DefaultEvaluator,
                 display_waiting_dots=True):
        """Initializes the class.

        Args:
            function_description: String that describes function (e.g."Creating
                                  Instance...")
            print_before_call: Boolean, print the function description before
                               calling the function, default True.
            print_status: Boolean, print the status of the function after the
                          function has completed, default True ("OK" or "Fail").
            result_evaluator: Func object. Pass func to evaluate result.
                              Default evaluator always report result is ok and
                              failed result will be identified only in exception
                              case.
            display_waiting_dots: Boolean, if true print the function_description
                                  followed by waiting dot.
        """
        self._function_description = function_description
        self._print_before_call = print_before_call
        self._print_status = print_status
        self._result_evaluator = result_evaluator
        self._display_waiting_dots = display_waiting_dots

    def __call__(self, func):
        def DecoratorFunction(*args, **kargs):
            """Decorator function.

            Args:
                *args: Arguments to pass to the functor.
                **kwargs: Key-val based arguments to pass to the functor.

            Raises:
                Exception: The exception that functor(*args, **kwargs) throws.
            """
            timestart = time.time()
            if self._print_before_call:
                waiting_dots = "..." if self._display_waiting_dots else ""
                PrintColorString("%s %s"% (self._function_description,
                                           waiting_dots), end="")
            try:
                result = func(*args, **kargs)
                result_time = time.time() - timestart
                if not self._print_before_call:
                    PrintColorString("%s (%ds)" % (self._function_description,
                                                   result_time),
                                     TextColors.OKGREEN)
                if self._print_status:
                    evaluated_result = self._result_evaluator(result)
                    if evaluated_result.is_result_ok:
                        PrintColorString("OK! (%ds)" % (result_time),
                                         TextColors.OKGREEN)
                    else:
                        PrintColorString("Fail! (%ds)" % (result_time),
                                         TextColors.FAIL)
                        PrintColorString("Error: %s" %
                                         evaluated_result.result_message,
                                         TextColors.FAIL)
                return result
            except:
                if self._print_status:
                    PrintColorString("Fail! (%ds)" % (time.time() - timestart),
                                     TextColors.FAIL)
                raise
        return DecoratorFunction


def PickFreePort():
    """Helper to pick a free port.

    Returns:
        Integer, a free port number.
    """
    tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_socket.bind(("", 0))
    port = tcp_socket.getsockname()[1]
    tcp_socket.close()
    return port


def CheckPortFree(port):
    """Check the availablity of the tcp port.

    Args:
        Integer, a port number.

    Raises:
        PortOccupied: This port is not available.
    """
    tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        tcp_socket.bind(("", port))
    except socket.error:
        raise errors.PortOccupied("Port (%d) is taken, please choose another "
                                  "port." % port)
    tcp_socket.close()


def _ExecuteCommand(cmd, args):
    """Execute command.

    Args:
        cmd: Strings of execute binary name.
        args: List of args to pass in with cmd.

    Raises:
        errors.NoExecuteBin: Can't find the execute bin file.
    """
    bin_path = FindExecutable(cmd)
    if not bin_path:
        raise errors.NoExecuteCmd("unable to locate %s" % cmd)
    command = [bin_path] + args
    logger.debug("Running '%s'", ' '.join(command))
    with open(os.devnull, "w") as dev_null:
        subprocess.check_call(command, stderr=dev_null, stdout=dev_null)


# TODO(147337696): create ssh tunnels tear down as adb and vnc.
# pylint: disable=too-many-locals
def AutoConnect(ip_addr, rsa_key_file, target_vnc_port, target_adb_port,
                ssh_user, client_adb_port=None, extra_args_ssh_tunnel=None):
    """Autoconnect to an AVD instance.

    Args:
        ip_addr: String, use to build the adb & vnc tunnel between local
                 and remote instance.
        rsa_key_file: String, Private key file path to use when creating
                      the ssh tunnels.
        target_vnc_port: Integer of target vnc port number.
        target_adb_port: Integer of target adb port number.
        ssh_user: String of user login into the instance.
        client_adb_port: Integer, Specified adb port to establish connection.
        extra_args_ssh_tunnel: String, extra args for ssh tunnel connection.

    Returns:
        NamedTuple of (vnc_port, adb_port) SSHTUNNEL of the connect, both are
        integers.
    """
    local_free_vnc_port = PickFreePort()
    local_adb_port = client_adb_port or PickFreePort()
    try:
        ssh_tunnel_args = _SSH_TUNNEL_ARGS % {
            "rsa_key_file": rsa_key_file,
            "vnc_port": local_free_vnc_port,
            "adb_port": local_adb_port,
            "target_vnc_port": target_vnc_port,
            "target_adb_port": target_adb_port,
            "ssh_user": ssh_user,
            "ip_addr": ip_addr}
        ssh_tunnel_args_list = shlex.split(ssh_tunnel_args)
        if extra_args_ssh_tunnel:
            ssh_tunnel_args_list.extend(shlex.split(extra_args_ssh_tunnel))
        _ExecuteCommand(constants.SSH_BIN, ssh_tunnel_args_list)
    except subprocess.CalledProcessError as e:
        PrintColorString("\n%s\nFailed to create ssh tunnels, retry with '#acloud "
                         "reconnect'." % e, TextColors.FAIL)
        return ForwardedPorts(vnc_port=None, adb_port=None)

    try:
        adb_connect_args = _ADB_CONNECT_ARGS % {"adb_port": local_adb_port}
        _ExecuteCommand(constants.ADB_BIN, adb_connect_args.split())
    except subprocess.CalledProcessError:
        PrintColorString("Failed to adb connect, retry with "
                         "'#acloud reconnect'", TextColors.FAIL)

    return ForwardedPorts(vnc_port=local_free_vnc_port,
                          adb_port=local_adb_port)


def GetAnswerFromList(answer_list, enable_choose_all=False):
    """Get answer from a list.

    Args:
        answer_list: list of the answers to choose from.
        enable_choose_all: True to choose all items from answer list.

    Return:
        List holding the answer(s).
    """
    print("[0] to exit.")
    start_index = 1
    max_choice = len(answer_list)

    for num, item in enumerate(answer_list, start_index):
        print("[%d] %s" % (num, item))
    if enable_choose_all:
        max_choice += 1
        print("[%d] for all." % max_choice)

    choice = -1

    while True:
        try:
            choice = six.moves.input("Enter your choice[0-%d]: " % max_choice)
            choice = int(choice)
        except ValueError:
            print("'%s' is not a valid integer.", choice)
            continue
        # Filter out choices
        if choice == 0:
            sys.exit(constants.EXIT_BY_USER)
        if enable_choose_all and choice == max_choice:
            return answer_list
        if choice < 0 or choice > max_choice:
            print("please choose between 0 and %d" % max_choice)
        else:
            return [answer_list[choice-start_index]]


def LaunchVNCFromReport(report, avd_spec, no_prompts=False):
    """Launch vnc client according to the instances report.

    Args:
        report: Report object, that stores and generates report.
        avd_spec: AVDSpec object that tells us what we're going to create.
        no_prompts: Boolean, True to skip all prompts.
    """
    for device in report.data.get("devices", []):
        if device.get(constants.VNC_PORT):
            LaunchVncClient(device.get(constants.VNC_PORT),
                            avd_width=avd_spec.hw_property["x_res"],
                            avd_height=avd_spec.hw_property["y_res"],
                            no_prompts=no_prompts)
        else:
            PrintColorString("No VNC port specified, skipping VNC startup.",
                             TextColors.FAIL)

def LaunchBrowserFromReport(report):
    """Open browser when autoconnect to webrtc according to the instances report.

    Args:
        report: Report object, that stores and generates report.
    """
    PrintColorString("(This is an experimental project for webrtc, and since "
                     "the certificate is self-signed, Chrome will mark it as "
                     "an insecure website. keep going.)",
                     TextColors.WARNING)

    for device in report.data.get("devices", []):
        if device.get("ip"):
            webrtc_link = "%s%s:%s" % (_WEBRTC_URL, device.get("ip"),
                                       _WEBRTC_PORT)
            if os.environ.get(_ENV_DISPLAY, None):
                webbrowser.open_new_tab(webrtc_link)
            else:
                PrintColorString("Remote terminal can't support launch webbrowser.",
                                 TextColors.FAIL)
                PrintColorString("Open %s to remotely control AVD on the "
                                 "browser." % webrtc_link)
        else:
            PrintColorString("Auto-launch devices webrtc in browser failed!",
                             TextColors.FAIL)

def LaunchVncClient(port, avd_width=None, avd_height=None, no_prompts=False):
    """Launch ssvnc.

    Args:
        port: Integer, port number.
        avd_width: String, the width of avd.
        avd_height: String, the height of avd.
        no_prompts: Boolean, True to skip all prompts.
    """
    try:
        os.environ[_ENV_DISPLAY]
    except KeyError:
        PrintColorString("Remote terminal can't support VNC. "
                         "Skipping VNC startup.", TextColors.FAIL)
        return

    if IsSupportedPlatform() and not FindExecutable(_VNC_BIN):
        if no_prompts or GetUserAnswerYes(_CONFIRM_CONTINUE):
            try:
                PrintColorString("Installing ssvnc vnc client... ", end="")
                sys.stdout.flush()
                subprocess.check_output(_CMD_INSTALL_SSVNC, shell=True)
                PrintColorString("Done", TextColors.OKGREEN)
            except subprocess.CalledProcessError as cpe:
                PrintColorString("Failed to install ssvnc: %s" %
                                 cpe.output, TextColors.FAIL)
                return
        else:
            return
    ssvnc_env = os.environ.copy()
    ssvnc_env.update(_SSVNC_ENV_VARS)
    # Override SSVNC_SCALE
    if avd_width or avd_height:
        scale_ratio = CalculateVNCScreenRatio(avd_width, avd_height)
        ssvnc_env["SSVNC_SCALE"] = str(scale_ratio)
        logger.debug("SSVNC_SCALE:%s", scale_ratio)

    ssvnc_args = _CMD_START_VNC % {"bin": FindExecutable(_VNC_BIN),
                                   "port": port}
    subprocess.Popen(ssvnc_args.split(), env=ssvnc_env)


def PrintDeviceSummary(report):
    """Display summary of devices.

    -Display device details from the report instance.
        report example:
            'data': [{'devices':[{'instance_name': 'ins-f6a397-none-53363',
                                  'ip': u'35.234.10.162'}]}]
    -Display error message from report.error.

    Args:
        report: A Report instance.
    """
    PrintColorString("\n")
    PrintColorString("Device summary:")
    for device in report.data.get("devices", []):
        adb_serial = "(None)"
        adb_port = device.get("adb_port")
        if adb_port:
            adb_serial = constants.LOCALHOST_ADB_SERIAL % adb_port
        instance_name = device.get("instance_name")
        instance_ip = device.get("ip")
        instance_details = "" if not instance_name else "(%s[%s])" % (
            instance_name, instance_ip)
        PrintColorString(" - device serial: %s %s" % (adb_serial,
                                                      instance_details))
        PrintColorString("   export ANDROID_SERIAL=%s" % adb_serial)

    # TODO(b/117245508): Help user to delete instance if it got created.
    if report.errors:
        error_msg = "\n".join(report.errors)
        PrintColorString("Fail in:\n%s\n" % error_msg, TextColors.FAIL)


def CalculateVNCScreenRatio(avd_width, avd_height):
    """calculate the vnc screen scale ratio to fit into user's monitor.

    Args:
        avd_width: String, the width of avd.
        avd_height: String, the height of avd.
    Return:
        Float, scale ratio for vnc client.
    """
    try:
        import Tkinter
    # Some python interpreters may not be configured for Tk, just return default scale ratio.
    except ImportError:
        return _DEFAULT_DISPLAY_SCALE
    root = Tkinter.Tk()
    margin = 100 # leave some space on user's monitor.
    screen_height = root.winfo_screenheight() - margin
    screen_width = root.winfo_screenwidth() - margin

    scale_h = _DEFAULT_DISPLAY_SCALE
    scale_w = _DEFAULT_DISPLAY_SCALE
    if float(screen_height) < float(avd_height):
        scale_h = round(float(screen_height) / float(avd_height), 1)

    if float(screen_width) < float(avd_width):
        scale_w = round(float(screen_width) / float(avd_width), 1)

    logger.debug("scale_h: %s (screen_h: %s/avd_h: %s),"
                 " scale_w: %s (screen_w: %s/avd_w: %s)",
                 scale_h, screen_height, avd_height,
                 scale_w, screen_width, avd_width)

    # Return the larger scale-down ratio.
    return scale_h if scale_h < scale_w else scale_w


def IsCommandRunning(command):
    """Check if command is running.

    Args:
        command: String of command name.

    Returns:
        Boolean, True if command is running. False otherwise.
    """
    try:
        with open(os.devnull, "w") as dev_null:
            subprocess.check_call([constants.CMD_PGREP, "-af", command],
                                  stderr=dev_null, stdout=dev_null)
        return True
    except subprocess.CalledProcessError:
        return False


def AddUserGroupsToCmd(cmd, user_groups):
    """Add the user groups to the command if necessary.

    As part of local host setup to enable local instance support, the user is
    added to certain groups. For those settings to take effect systemwide
    requires the user to log out and log back in. In the scenario where the
    user has run setup and hasn't logged out, we still want them to be able to
    launch a local instance so add the user to the groups as part of the
    command to ensure success.

    The reason using here-doc instead of '&' is all operations need to be ran in
    ths same pid.  Here's an example cmd:
    $ sg kvm  << EOF
    sg libvirt
    sg cvdnetwork
    launch_cvd --cpus 2 --x_res 1280 --y_res 720 --dpi 160 --memory_mb 4096
    EOF

    Args:
        cmd: String of the command to prepend the user groups to.
        user_groups: List of user groups name.(String)

    Returns:
        String of the command with the user groups prepended to it if necessary,
        otherwise the same existing command.
    """
    user_group_cmd = ""
    if not CheckUserInGroups(user_groups):
        logger.debug("Need to add user groups to the command")
        for idx, group in enumerate(user_groups):
            user_group_cmd += _CMD_SG + group
            if idx == 0:
                user_group_cmd += " <<EOF\n"
            else:
                user_group_cmd += "\n"
        cmd += "\nEOF"
    user_group_cmd += cmd
    logger.debug("user group cmd: %s", user_group_cmd)
    return user_group_cmd


def CheckUserInGroups(group_name_list):
    """Check if the current user is in the group.

    Args:
        group_name_list: The list of group name.
    Returns:
        True if current user is in all the groups.
    """
    logger.info("Checking if user is in following groups: %s", group_name_list)
    current_groups = [grp.getgrgid(g).gr_name for g in os.getgroups()]
    all_groups_present = True
    for group in group_name_list:
        if group not in current_groups:
            all_groups_present = False
            logger.info("missing group: %s", group)
    return all_groups_present


def IsSupportedPlatform(print_warning=False):
    """Check if user's os is the supported platform.

    Args:
        print_warning: Boolean, print the unsupported warning
                       if True.
    Returns:
        Boolean, True if user is using supported platform.
    """
    system = platform.system()
    # TODO(b/143197659): linux_distribution() deprecated in python 3. To fix it
    # try to use another package "import distro".
    dist = platform.linux_distribution()[0]
    platform_supported = (system in _SUPPORTED_SYSTEMS_AND_DISTS and
                          dist in _SUPPORTED_SYSTEMS_AND_DISTS[system])

    logger.info("supported system and dists: %s",
                _SUPPORTED_SYSTEMS_AND_DISTS)
    platform_supported_msg = ("%s[%s] %s supported platform" %
                              (system,
                               dist,
                               "is a" if platform_supported else "is not a"))
    if print_warning and not platform_supported:
        PrintColorString(platform_supported_msg, TextColors.WARNING)
    else:
        logger.info(platform_supported_msg)

    return platform_supported


def GetDistDir():
    """Return the absolute path to the dist dir."""
    android_build_top = os.environ.get(constants.ENV_ANDROID_BUILD_TOP)
    if not android_build_top:
        return None
    dist_cmd = GET_BUILD_VAR_CMD[:]
    dist_cmd.append(_DIST_DIR)
    try:
        dist_dir = subprocess.check_output(dist_cmd, cwd=android_build_top)
    except subprocess.CalledProcessError:
        return None
    return os.path.join(android_build_top, dist_dir.strip())


def CleanupProcess(pattern):
    """Cleanup process with pattern.

    Args:
        pattern: String, string of process pattern.
    """
    if IsCommandRunning(pattern):
        command_kill = _CMD_KILL + [pattern]
        subprocess.check_call(command_kill)


def TimeoutException(timeout_secs, timeout_error=_DEFAULT_TIMEOUT_ERR):
    """Decorater which function timeout setup and raise custom exception.

    Args:
        timeout_secs: Number of maximum seconds of waiting time.
        timeout_error: String to describe timeout exception.

    Returns:
        The function wrapper.
    """
    if timeout_error == _DEFAULT_TIMEOUT_ERR:
        timeout_error = timeout_error % timeout_secs

    def _Wrapper(func):
        # pylint: disable=unused-argument
        def _HandleTimeout(signum, frame):
            raise errors.FunctionTimeoutError(timeout_error)

        def _FunctionWrapper(*args, **kwargs):
            signal.signal(signal.SIGALRM, _HandleTimeout)
            signal.alarm(timeout_secs)
            try:
                result = func(*args, **kwargs)
            finally:
                signal.alarm(0)
            return result

        return _FunctionWrapper

    return _Wrapper


def GetBuildEnvironmentVariable(variable_name):
    """Get build environment variable.

    Args:
        variable_name: String of variable name.

    Returns:
        String, the value of the variable.

    Raises:
        errors.GetAndroidBuildEnvVarError: No environment variable found.
    """
    try:
        return os.environ[variable_name]
    except KeyError:
        raise errors.GetAndroidBuildEnvVarError(
            "Could not get environment var: %s\n"
            "Try to run 'source build/envsetup.sh && lunch <target>'"
            % variable_name
        )


# pylint: disable=no-member
def FindExecutable(filename):
    """A compatibility function to find execution file path.

    Args:
        filename: String of execution filename.

    Returns:
        String: execution file path.
    """
    return find_executable(filename) if six.PY2 else shutil.which(filename)


def GetDictItems(namedtuple_object):
    """A compatibility function to access the OrdereDict object from the given namedtuple object.

    Args:
        namedtuple_object: namedtuple object.

    Returns:
        collections.namedtuple.__dict__.items() when using python2.
        collections.namedtuple._asdict().items() when using python3.
    """
    return (namedtuple_object.__dict__.items() if six.PY2
            else namedtuple_object._asdict().items())


def CleanupSSVncviewer(vnc_port):
    """Cleanup the old disconnected ssvnc viewer.

    Args:
        vnc_port: Integer, port number of vnc.
    """
    ssvnc_viewer_pattern = _SSVNC_VIEWER_PATTERN % {"vnc_port":vnc_port}
    CleanupProcess(ssvnc_viewer_pattern)
