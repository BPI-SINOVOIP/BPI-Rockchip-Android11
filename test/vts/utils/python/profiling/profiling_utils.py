#
# Copyright (C) 2016 The Android Open Source Project
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

import logging
import os

from google.protobuf import text_format
from vts.proto import ComponentSpecificationMessage_pb2 as CompSpecMsg
from vts.proto import VtsProfilingMessage_pb2 as VtsProfilingMsg
from vts.proto import VtsReportMessage_pb2 as ReportMsg
from vts.runners.host import asserts
from vts.runners.host import const
from vts.runners.host import keys
from vts.utils.python.common import cmd_utils
from vts.utils.python.os import path_utils
from vts.utils.python.web import feature_utils

LOCAL_PROFILING_TRACE_PATH = "/tmp/vts-test-trace"
TARGET_PROFILING_TRACE_PATH = "/data/local/tmp/"
HAL_INSTRUMENTATION_LIB_PATH_32 = "/data/local/tmp/32/"
HAL_INSTRUMENTATION_LIB_PATH_64 = "/data/local/tmp/64/"
DEFAULT_HAL_ROOT = "android.hardware."

_PROFILING_DATA = "profiling_data"
_HOST_PROFILING_DATA = "host_profiling_data"


class VTSProfilingData(object):
    """Class to store the VTS profiling data.

    Attributes:
        values: A dict that stores the profiling data. e.g. latencies of each api.
        options: A set of strings where each string specifies an associated
                 option (which is the form of 'key=value').
    """

    def __init__(self):
        self.values = {}
        self.options = set()


EVENT_TYPE_DICT = {
    0: "SERVER_API_ENTRY",
    1: "SERVER_API_EXIT",
    2: "CLIENT_API_ENTRY",
    3: "CLIENT_API_EXIT",
    4: "SYNC_CALLBACK_ENTRY",
    5: "SYNC_CALLBACK_EXIT",
    6: "ASYNC_CALLBACK_ENTRY",
    7: "ASYNC_CALLBACK_EXIT",
    8: "PASSTHROUGH_ENTRY",
    9: "PASSTHROUGH_EXIT",
}


class VTSApiCoverageData(object):
    """Class to store the API coverage data.

    Attributes:
        package_name: sting, HAL package name (e.g. android.hardware.foo).
        version_major: string, HAL major version (e.g. 1).
        version_minor: string, HAL minor version (e.g. 0).
        interface_name: string, HAL interface name (e.g. IFoo).
        total_apis: A set of strings, each string represents an API name defined
                    in the given HAL.
        covered_apis: A set of strings, each string represents an API name
                      covered by the test.
    """

    def __init__(self, package_name, version, interface_name):
        self.package_name = package_name
        self.version_major, self.version_minor = version.split(".")
        self.interface_name = interface_name
        self.total_apis = set()
        self.covered_apis = set()


class ProfilingFeature(feature_utils.Feature):
    """Feature object for profiling functionality.

    Attributes:
        enabled: boolean, True if profiling is enabled, False otherwise
        web: (optional) WebFeature, object storing web feature util for test run.
        data_file_path: Path to the data directory within vts package.
        api_coverage_data: A dictionary from full HAL interface name
        (e.g. android.hardware.foo@1.0::IFoo) to VtsApiCoverageData.
    """

    _TOGGLE_PARAM = keys.ConfigKeys.IKEY_ENABLE_PROFILING
    _REQUIRED_PARAMS = [keys.ConfigKeys.IKEY_DATA_FILE_PATH]
    _OPTIONAL_PARAMS = [
        keys.ConfigKeys.IKEY_PROFILING_TRACING_PATH,
        keys.ConfigKeys.IKEY_TRACE_FILE_TOOL_NAME,
        keys.ConfigKeys.IKEY_SAVE_TRACE_FILE_REMOTE,
        keys.ConfigKeys.IKEY_ABI_BITNESS,
        keys.ConfigKeys.IKEY_PROFILING_ARG_VALUE,
    ]

    def __init__(self, user_params, web=None):
        """Initializes the profiling feature.

        Args:
            user_params: A dictionary from parameter name (String) to parameter value.
            web: (optional) WebFeature, object storing web feature util for test run
        """
        self.ParseParameters(self._TOGGLE_PARAM, self._REQUIRED_PARAMS,
                             self._OPTIONAL_PARAMS, user_params)
        self.web = web
        if self.enabled:
            logging.info("Profiling is enabled.")
        else:
            logging.debug("Profiling is disabled.")

        self.data_file_path = getattr(self,
                                      keys.ConfigKeys.IKEY_DATA_FILE_PATH, None)
        self.api_coverage_data = {}

    def _IsEventFromBinderizedHal(self, event_type):
        """Returns True if the event type is from a binderized HAL."""
        if event_type in [8, 9]:
            return False
        return True

    def GetTraceFiles(self,
                      dut,
                      host_profiling_trace_path=None,
                      trace_file_tool=None):
        """Pulls the trace file and save it under the profiling trace path.

        Args:
            dut: the testing device.
            host_profiling_trace_path: directory that stores trace files on host.
            trace_file_tool: tools that used to store the trace file.

        Returns:
            Name list of trace files that stored on host.
        """
        if not os.path.exists(LOCAL_PROFILING_TRACE_PATH):
            os.makedirs(LOCAL_PROFILING_TRACE_PATH)

        if not host_profiling_trace_path:
            host_profiling_trace_path = LOCAL_PROFILING_TRACE_PATH

        target_trace_file = path_utils.JoinTargetPath(
            TARGET_PROFILING_TRACE_PATH, "*.vts.trace")
        results = dut.shell.Execute("ls " + target_trace_file)
        asserts.assertTrue(results, "failed to find trace file")
        stdout_lines = results[const.STDOUT][0].split("\n")
        logging.debug("stdout: %s", stdout_lines)
        trace_files = []
        for line in stdout_lines:
            if line:
                temp_file_name = os.path.join(LOCAL_PROFILING_TRACE_PATH,
                                              os.path.basename(line.strip()))
                dut.adb.pull("%s %s" % (line, temp_file_name))
                trace_file_name = os.path.join(host_profiling_trace_path,
                                               os.path.basename(line.strip()))
                logging.info("Saving profiling traces: %s" % trace_file_name)
                if temp_file_name != trace_file_name:
                    file_cmd = ""
                    if trace_file_tool:
                        file_cmd += trace_file_tool
                    file_cmd += " cp " + temp_file_name + " " + trace_file_name
                    results = cmd_utils.ExecuteShellCommand(file_cmd)
                    if results[const.EXIT_CODE][0] != 0:
                        logging.error(results[const.STDERR][0])
                        logging.error("Fail to execute command: %s" % file_cmd)
                trace_files.append(temp_file_name)
        return trace_files

    def EnableVTSProfiling(self, shell, hal_instrumentation_lib_path=None):
        """ Enable profiling by setting the system property.

        Args:
            shell: shell to control the testing device.
            hal_instrumentation_lib_path: string, the path of directory that stores
                                          profiling libraries.
        """
        hal_instrumentation_lib_path_32 = HAL_INSTRUMENTATION_LIB_PATH_32
        hal_instrumentation_lib_path_64 = HAL_INSTRUMENTATION_LIB_PATH_64
        if hal_instrumentation_lib_path is not None:
            bitness = getattr(self, keys.ConfigKeys.IKEY_ABI_BITNESS, None)
            if bitness == '64':
                hal_instrumentation_lib_path_64 = hal_instrumentation_lib_path
            elif bitness == '32':
                hal_instrumentation_lib_path_32 = hal_instrumentation_lib_path
            else:
                logging.error('Unknown abi bitness "%s". Using 64bit hal '
                              'instrumentation lib path.', bitness)

        # cleanup any existing traces.
        shell.Execute(
            "rm " + os.path.join(TARGET_PROFILING_TRACE_PATH, "*.vts.trace"))
        logging.debug("enabling VTS profiling.")

        # give permission to write the trace file.
        shell.Execute("chmod 777 " + TARGET_PROFILING_TRACE_PATH)

        shell.Execute("setprop hal.instrumentation.lib.path.32 " +
                      hal_instrumentation_lib_path_32)
        shell.Execute("setprop hal.instrumentation.lib.path.64 " +
                      hal_instrumentation_lib_path_64)

        if getattr(self, keys.ConfigKeys.IKEY_PROFILING_ARG_VALUE, False):
            shell.Execute("setprop hal.instrumentation.profile.args true")
        else:
            shell.Execute("setprop hal.instrumentation.profile.args false")
        shell.Execute("setprop hal.instrumentation.enable true")

    def DisableVTSProfiling(self, shell):
        """ Disable profiling by resetting the system property.

        Args:
            shell: shell to control the testing device.
        """
        shell.Execute("setprop hal.instrumentation.lib.path \"\"")
        shell.Execute("setprop hal.instrumentation.profile.args \"\"")
        shell.Execute("setprop hal.instrumentation.enable false")

    def _ParseTraceData(self, trace_file, measure_api_coverage):
        """Parses the data stored in trace_file, calculates the avg/max/min
        latency for each API.

        Args:
            trace_file: file that stores the trace data.
            measure_api_coverage: whether to measure the api coverage data.

        Returns:
            VTSProfilingData which contain the list of API names and the avg/max/min
            latency for each API.
        """
        profiling_data = VTSProfilingData()
        api_timestamps = {}
        api_latencies = {}

        trace_processor_binary = os.path.join(self.data_file_path, "host",
                                              "bin", "trace_processor")
        trace_processor_lib = os.path.join(self.data_file_path, "host",
                                           "lib64")
        trace_processor_cmd = [
            "chmod a+x %s" % trace_processor_binary,
            "LD_LIBRARY_PATH=%s %s -m profiling_trace %s" %
            (trace_processor_lib, trace_processor_binary, trace_file)
        ]

        results = cmd_utils.ExecuteShellCommand(trace_processor_cmd)
        if any(results[cmd_utils.EXIT_CODE]):
            logging.error("Fail to execute command: %s" % trace_processor_cmd)
            logging.error("stdout: %s" % results[const.STDOUT])
            logging.error("stderr: %s" % results[const.STDERR])
            return profiling_data

        stdout_lines = results[const.STDOUT][1].split("\n")
        first_line = True
        for line in stdout_lines:
            if not line:
                continue
            if first_line:
                _, mode = line.split(":")
                profiling_data.options.add("hidl_hal_mode=%s" % mode)
                first_line = False
            else:
                full_api, latency = line.rsplit(":", 1)
                full_interface, api_name = full_api.rsplit("::", 1)
                if profiling_data.values.get(api_name):
                    profiling_data.values[api_name].append(long(latency))
                else:
                    profiling_data.values[api_name] = [long(latency)]

                if measure_api_coverage:
                    package, interface_name = full_interface.split("::")
                    package_name, version = package.split("@")

                    if full_interface in self.api_coverage_data:
                        self.api_coverage_data[
                            full_interface].covered_apis.add(api_name)
                    else:
                        total_apis = self._GetTotalApis(
                            package_name, version, interface_name)
                        if total_apis:
                            vts_api_coverage = VTSApiCoverageData(
                                package_name, version, interface_name)
                            vts_api_coverage.total_apis = total_apis
                            if api_name in total_apis:
                                vts_api_coverage.covered_apis.add(api_name)
                            else:
                                logging.warning("API %s is not supported by %s",
                                                api_name, full_interface)
                            self.api_coverage_data[
                                full_interface] = vts_api_coverage

        return profiling_data

    def _GetTotalApis(self, package_name, version, interface_name):
        """Parse the specified vts spec and get all APIs defined in the spec.

        Args:
            package_name: string, HAL package name.
            version: string, HAL version.
            interface_name: string, HAL interface name.

        Returns:
            A set of strings, each string represents an API defined in the spec.
        """
        total_apis = set()
        spec_proto = CompSpecMsg.ComponentSpecificationMessage()
        # TODO: support general package that does not start with android.hardware.
        if not package_name.startswith(DEFAULT_HAL_ROOT):
            logging.warning("Unsupported hal package: %s", package_name)
            return total_apis

        hal_package_path = package_name[len(DEFAULT_HAL_ROOT):].replace(
            ".", "/")
        vts_spec_path = os.path.join(
            self.data_file_path, "spec/hardware/interfaces", hal_package_path,
            version, "vts", interface_name[1:] + ".vts")
        logging.debug("vts_spec_path: %s", vts_spec_path)
        with open(vts_spec_path, 'r') as spec_file:
            spec_string = spec_file.read()
            text_format.Merge(spec_string, spec_proto)
        for api in spec_proto.interface.api:
            if not api.is_inherited:
                total_apis.add(api.name)
        return total_apis

    def StartHostProfiling(self, name):
        """Starts a profiling operation.

        Args:
            name: string, the name of a profiling point

        Returns:
            True if successful, False otherwise
        """
        if not self.enabled:
            return False

        if not hasattr(self, _HOST_PROFILING_DATA):
            setattr(self, _HOST_PROFILING_DATA, {})

        host_profiling_data = getattr(self, _HOST_PROFILING_DATA)

        if name in host_profiling_data:
            logging.error("profiling point %s is already active.", name)
            return False
        host_profiling_data[name] = feature_utils.GetTimestamp()
        return True

    def StopHostProfiling(self, name):
        """Stops a profiling operation.

        Args:
            name: string, the name of a profiling point
        """
        if not self.enabled:
            return

        if not hasattr(self, _HOST_PROFILING_DATA):
            setattr(self, _HOST_PROFILING_DATA, {})

        host_profiling_data = getattr(self, _HOST_PROFILING_DATA)

        if name not in host_profiling_data:
            logging.error("profiling point %s is not active.", name)
            return False

        start_timestamp = host_profiling_data[name]
        end_timestamp = feature_utils.GetTimestamp()
        if self.web and self.web.enabled:
            self.web.AddProfilingDataTimestamp(name, start_timestamp,
                                               end_timestamp)
        return True

    def ProcessTraceDataForTestCase(self, dut, measure_api_coverage=True):
        """Pulls the generated trace file to the host, parses the trace file to
        get the profiling data (e.g. latency of each API call) and stores these
        data in _profiling_data.

        Requires the feature to be enabled; no-op otherwise.

        Args:
            dut: the registered device.
        """
        if not self.enabled:
            return

        if not hasattr(self, _PROFILING_DATA):
            setattr(self, _PROFILING_DATA, [])

        profiling_data = getattr(self, _PROFILING_DATA)

        trace_files = []
        save_trace_remote = getattr(
            self, keys.ConfigKeys.IKEY_SAVE_TRACE_FILE_REMOTE, False)
        if save_trace_remote:
            trace_files = self.GetTraceFiles(
                dut,
                getattr(self, keys.ConfigKeys.IKEY_PROFILING_TRACING_PATH,
                        None),
                getattr(self, keys.ConfigKeys.IKEY_TRACE_FILE_TOOL_NAME, None))
        else:
            trace_files = self.GetTraceFiles(dut)

        for file in trace_files:
            logging.info("parsing trace file: %s.", file)
            data = self._ParseTraceData(file, measure_api_coverage)
            if data:
                profiling_data.append(data)

    def ProcessAndUploadTraceData(self, upload_api_coverage=True):
        """Process and upload profiling trace data.

        Requires the feature to be enabled; no-op otherwise.

        Merges the profiling data generated by each test case, calculates the
        aggregated max/min/avg latency for each API and uploads these latency
        metrics to webdb.

        Args:
            upload_api_coverage: whether to upload the API coverage data.
        """
        if not self.enabled:
            return

        merged_profiling_data = VTSProfilingData()
        for data in getattr(self, _PROFILING_DATA, []):
            for item in data.options:
                merged_profiling_data.options.add(item)
            for api, latences in data.values.items():
                if merged_profiling_data.values.get(api):
                    merged_profiling_data.values[api].extend(latences)
                else:
                    merged_profiling_data.values[api] = latences
        for api, latencies in merged_profiling_data.values.items():
            if not self.web or not self.web.enabled:
                continue

            self.web.AddProfilingDataUnlabeledVector(
                api,
                latencies,
                merged_profiling_data.options,
                x_axis_label="API processing latency (nano secs)",
                y_axis_label="Frequency")

        if upload_api_coverage:
            self.web.AddApiCoverageReport(self.api_coverage_data.values())
