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

import json
import logging
import requests
import threading
import time

from host_controller.utils.parser import pb2_utils

# Job status dict
JOB_STATUS_DICT = {
    # scheduled but not leased yet
    "ready": 0,
    # scheduled and in running
    "leased": 1,
    # completed job
    "complete": 2,
    # unexpected error during running
    "infra-err": 3,
    # never leased within schedule period
    "expired": 4,
    # device boot error after flashing the given img sets
    "bootup-err": 5
}

SCHEDULE_INFO_PB2_ATTR_FILTERS = {
    "pab_account_id": "device_pab_account_id",
    "name": "build_target",
}

# timeout seconds for requests
REQUESTS_TIMEOUT_SECONDS = 60


class VtiEndpointClient(object):
    """VTI (Vendor Test Infrastructure) endpoint client.

    Attributes:
        _headers: A dictionary, containing HTTP request header information.
        _url: string, the base URL of an endpoint API.
        _job: dict, currently leased job info.
    """

    def __init__(self, url):
        if url == "localhost":
            url = "http://localhost:8080/_ah/api/"
        else:
            if not url.startswith(("https://")) and not url.startswith("http://"):
                url = "https://" + url
            if url.endswith("appspot.com"):
                url += "/_ah/api/"
        self._headers = {"content-type": "application/json",
                   "Accept-Charset": "UTF-8"}
        self._url = url
        self._job = {}
        self._heartbeat_thread = None

    def UploadBuildInfo(self, builds):
        """Uploads the given build information to VTI.

        Args:
            builds: a list of dictionaries, containing info about all new
                    builds found.

        Returns:
            True if successful, False otherwise.
        """
        url = self._url + "build/v1/set"
        fail = False
        for build in builds:
            try:
                response = requests.post(url, data=json.dumps(build),
                                         headers=self._headers,
                                         timeout=REQUESTS_TIMEOUT_SECONDS)
                if response.status_code != requests.codes.ok:
                    logging.error("UploadBuildInfo error: %s", response)
                    fail = True
            except requests.exceptions.Timeout as e:
                logging.exception(e)
                fail = True
        if fail:
            return False
        return True

    def UploadDeviceInfo(self, hostname, devices):
        """Uploads the given device information to VTI.

        Args:
            hostname: string, the hostname of a target host.
            devices: a list of dicts, containing info about all detected
                     devices that are attached to the host.

        Returns:
            True if successful, False otherwise.
        """
        url = self._url + "host/v1/set"
        payload = {}
        payload["hostname"] = hostname
        payload["devices"] = []
        for device in devices:
            new_device = {
                "serial": device["serial"],
                "product": device["product"],
                "status": device["status"]}
            payload["devices"].append(new_device)

        try:
            response = requests.post(url, data=json.dumps(payload),
                                     headers=self._headers,
                                     timeout=REQUESTS_TIMEOUT_SECONDS)
        except (requests.exceptions.ConnectionError,
                requests.exceptions.Timeout) as e:
            logging.exception(e)
            return False
        if response.status_code != requests.codes.ok:
            logging.error("UploadDeviceInfo error: %s", response)
            return False
        return True

    def UploadScheduleInfo(self, pbs, clear_schedule):
        """Uploads the given schedule information to VTI.

        Args:
            pbs: a list of dicts, containing info about all task schedules.
            clear_schedule: bool, True to clear all schedule data exist on the
                            scheduler

        Returns:
            True if successful, False otherwise.
        """
        if pbs is None or len(pbs) == 0:
            return False

        url = self._url + "schedule/v1/clear"
        succ = True
        if clear_schedule:
            try:
                response = requests.post(
                    url, data=json.dumps({"manifest_branch": "na"}),
                    headers=self._headers, timeout=REQUESTS_TIMEOUT_SECONDS)
            except requests.exceptions.Timeout as e:
                logging.exception(e)
                return False
            if response.status_code != requests.codes.ok:
                logging.error("UploadScheduleInfo error: %s", response)
                succ = False

        if not succ:
            return False

        url = self._url + "schedule/v1/set"
        for pb in pbs:
            schedule = {}
            succ = succ and pb2_utils.FillDictAndPost(
                pb, schedule, url, self._headers,
                SCHEDULE_INFO_PB2_ATTR_FILTERS, "UploadScheduleInfo")

        return succ

    def UploadLabInfo(self, pbs, clear_labinfo):
        """Uploads the given lab information to VTI.

        Args:
            pbs: a list of dicts, containing info about all known labs.
            clear_labinfo: bool, True to clear all lab data exist on the
                           scheduler

        Returns:
            True if successful, False otherwise.
        """
        if pbs is None or len(pbs) == 0:
            return

        url = self._url + "lab/v1/clear"
        succ = True
        if clear_labinfo:
            try:
                response = requests.post(url, data=json.dumps({"name": "na"}),
                                         headers=self._headers,
                                         timeout=REQUESTS_TIMEOUT_SECONDS)
            except requests.exceptions.Timeout as e:
                logging.exception(e)
                return False
            if response.status_code != requests.codes.ok:
                logging.error("UploadLabInfo error: %s", response)
                succ = False

        if not succ:
            return False

        url = self._url + "lab/v1/set"
        for pb in pbs:
            lab = {}
            lab["name"] = pb.name
            lab["owner"] = pb.owner
            lab["admin"] = []
            lab["admin"].extend(pb.admin)
            lab["host"] = []
            for host in pb.host:
                new_host = {}
                new_host["hostname"] = host.hostname
                new_host["ip"] = host.ip
                new_host["script"] = host.script
                if host.host_equipment:
                    new_host["host_equipment"] = []
                    new_host["host_equipment"].extend(host.host_equipment)
                new_host["device"] = []
                if host.device:
                    for device in host.device:
                        new_device = {}
                        new_device["serial"] = device.serial
                        new_device["product"] = device.product
                        if device.device_equipment:
                            new_device["device_equipment"] = []
                            new_device["device_equipment"].extend(
                                device.device_equipment)
                        new_host["device"].append(new_device)
                lab["host"].append(new_host)
            try:
                response = requests.post(url, data=json.dumps(lab),
                                         headers=self._headers,
                                         timeout=REQUESTS_TIMEOUT_SECONDS)
                if response.status_code != requests.codes.ok:
                    logging.error("UploadLabInfo error: %s", response)
                    succ = False
            except requests.exceptions.Timeout as e:
                logging.exception(e)
                succ = False
        return succ

    def LeaseJob(self, hostname, execute=True):
        """Leases a job for the given host, 'hostname'.

        Args:
            hostname: string, the hostname of a target host.
            execute: boolean, True to lease and execute StartHeartbeat, which is
                     the case that the leased job will be executed on this
                     process's context.

        Returns:
            True if successful, False otherwise.
        """
        if not hostname:
            return None, {}

        url = self._url + "job/v1/lease"
        try:
            response = requests.post(url, data=json.dumps({"hostname": hostname}),
                                     headers=self._headers,
                                     timeout=REQUESTS_TIMEOUT_SECONDS)
        except requests.exceptions.Timeout as e:
            logging.exception(e)
            return None, {}

        if response.status_code != requests.codes.ok:
            logging.error("LeaseJob error: %s", response.status_code)
            return None, {}

        response_json = json.loads(response.text)
        if ("return_code" in response_json
                and response_json["return_code"] != "SUCCESS"):
            logging.debug("LeaseJob error: %s", response_json)
            return None, {}

        if "jobs" not in response_json:
            logging.error(
                "LeaseJob jobs not found in response json %s", response.text)
            return None, {}

        jobs = response_json["jobs"]
        if jobs and len(jobs) > 0:
            for job in jobs:
                if execute == True:
                    self._job = job
                    self.StartHeartbeat("leased", 60)
                return job["test_name"].split("/")[0], job
        return None, {}

    def ExecuteJob(self, job):
        """Executes leased job passed from parent process.

        Args:
            job: dict, information the on leased job.

        Returns:
            a string which is path to a script file for onecmd().
            a dict contains info on the leased job, will be passed to onecmd().
        """
        logging.info("Job info : {}".format(json.dumps(job)))
        if job is not None:
            self._job = job
            self.StartHeartbeat("leased", 60)
            return job["test_name"].split("/")[0], job

        return None, {}

    def UpdateLeasedJobStatus(self, status, update_interval):
        """Updates the status of the leased job.

        Args:
            status: string, status value.
            update_interval: int, time between heartbeats in second.
        """
        if self._job is None:
            return

        url = self._url + "job/v1/heartbeat"
        self._job["status"] = JOB_STATUS_DICT[status]

        thread = threading.currentThread()
        while getattr(thread, 'keep_running', True):
            try:
                response = requests.post(url, data=json.dumps(self._job),
                                         headers=self._headers,
                                         timeout=REQUESTS_TIMEOUT_SECONDS)
                if response.status_code != requests.codes.ok:
                    logging.error("UpdateLeasedJobStatus error: %s", response)
            except requests.exceptions.Timeout as e:
                logging.exception(e)
            time.sleep(update_interval)

    def StartHeartbeat(self, status="leased", update_interval=60):
        """Starts the hearbeat_thread.

        Args:
            status: string, status value.
            update_interval: int, time between heartbeats in second.
        """
        if (self._heartbeat_thread is None
                or hasattr(self._heartbeat_thread, 'keep_running')):
            self._heartbeat_thread = threading.Thread(
                target=self.UpdateLeasedJobStatus,
                args=(
                    status,
                    update_interval,
                ))
            self._heartbeat_thread.daemon = True
            self._heartbeat_thread.start()

    def StopHeartbeat(self, status="complete", infra_log_url=""):
        """Stops the hearbeat_thread and sets current job's status.

        Args:
            status: string, status value.
            infra_log_url: string, URL to the uploaded infra log.
        """
        self._heartbeat_thread.keep_running = False

        if self._job is None:
            return

        url = self._url + "job/v1/heartbeat"
        self.SetJobStatusFromLeasedTo(status)
        self._job["infra_log_url"] = infra_log_url

        try:
            response = requests.post(url, data=json.dumps(self._job),
                                     headers=self._headers,
                                     timeout=REQUESTS_TIMEOUT_SECONDS)
            if response.status_code != requests.codes.ok:
                logging.error("StopHeartbeat error: %s", response)
        except requests.exceptions.Timeout as e:
            logging.exception(e)

        self._job = None

    def SetJobStatusFromLeasedTo(self, status):
        """Sets current job's status only when the job's status is 'leased'.

        Args:
            status: string, status value.
        """
        if (self._job is not None and
            self._job["status"] == JOB_STATUS_DICT["leased"]):
            self._job["status"] = JOB_STATUS_DICT[status]

    def UploadHostVersion(self, hostname, vtslab_version):
        """Uploads vtslab version.

        Args:
            hostname: string, the name of the host.
            vtslab_version: string, current version of vtslab package.
        """
        url = self._url + "lab/v1/set_version"
        host = {}
        host["hostname"] = hostname
        host["vtslab_version"] = vtslab_version

        try:
            response = requests.post(url, data=json.dumps(host),
                                     headers=self._headers,
                                     timeout=REQUESTS_TIMEOUT_SECONDS)
        except (requests.exceptions.ConnectionError,
                requests.exceptions.Timeout) as e:
            logging.exception(e)
            return
        if response.status_code != requests.codes.ok:
            logging.error("UploadHostVersion error: %s", response)

    def CheckBootUpStatus(self):
        """Checks whether the device_img + gsi from the job fails to boot up.

        Returns:
            True if the devices flashed with the given imgs from the leased job
            succeed to boot up. False otherwise.
        """
        if self._job:
            return (self._job["status"] != JOB_STATUS_DICT["bootup-err"])
        return False

    def GetJobTestType(self):
        """Returns the test type of the leased job.

        Returns:
            int, test_type attr in the job message. 0 when there is no job
            leased to this vti_endpoint_client.
        """
        if self._job and "test_type" in self._job:
            try:
                return int(self._job["test_type"])
            except ValueError as e:
                logging.exception(e)
        return 0

    def GetJobDeviceProductName(self):
        """Returns the product name of the DUTs of the leased job.

        Returns:
            string, product name. An empty string if there is no job leased or
            "device" attr of the job obj is not well formatted.
        """
        if self._job and "device" in self._job:
            try:
                return self._job["device"].split("/")[1]
            except IndexError as e:
                logging.exception(e)
        return ""
