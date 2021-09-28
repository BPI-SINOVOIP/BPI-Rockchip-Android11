#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import copy

# Based on JobModel defined in
# test/vti/test_serving/gae/webapp/src/proto/model.py
input_data = {
    "test_type": 1,
    "hostname": "my_hostname",
    "priority": "low",
    "test_name": "vts/vts",
    "require_signed_device_build": True,
    "has_bootloader_img": True,
    "has_radio_img": True,
    "device": "my_device",
    "serial": ["my_serial1", "my_serial2", "my_serial3"],

    # device image information
    "build_storage_type": 1,
    "manifest_branch": "my_branch",
    "build_target": "my_build_target",
    "build_id": "my_build_id",
    "pab_account_id": "my_pab_account_id",
    "shards": 3,
    "param": "",
    "status": 1,
    "period": 24 * 60,  # 1 day

    # GSI information
    "gsi_storage_type": 1,
    "gsi_branch": "my_gsi_branch",
    "gsi_build_target": "my_gsi_build_target",
    "gsi_build_id": "my_gsi_build_id",
    "gsi_pab_account_id": "my_gsi_pab_account_id",
    # gsi_vendor_version: "8.1.0"

    # test suite information
    "test_storage_type": 1,
    "test_branch": "my_test_branch",
    "test_build_target": "my_test_build_target",
    "test_build_id": "my_test_build_id",
    "test_pab_account_id": "my_test_pab_account_id",

    #timestamp = ndb.DateTimeProperty(auto_now=False)
    #heartbeat_stamp = ndb.DateTimeProperty(auto_now=False)
    "retry_count": 3,
    "infra_log_url": "infra_log_url",

    #parent_schedule = ndb.KeyProperty(kind="ScheduleModel")
    "image_package_repo_base": "image_package_repo_base",
    "report_bucket": ["report_bucket"],
    "report_spreadsheet_id": ["report_spreadsheet_id"],
}

expected_output = [
    'device --set_serial=my_serial1,my_serial2,my_serial3 --from_job_pool --interval=300',
    'fetch --type=pab --branch=my_branch --target=my_build_target --artifact_name=my_build_target-img-my_build_id.zip --build_id=my_build_id --account_id=my_pab_account_id --fetch_signed_build=True',
    'fetch --type=pab --branch=my_branch --target=my_build_target --artifact_name=bootloader.img --build_id=my_build_id --account_id=my_pab_account_id',
    'fetch --type=pab --branch=my_branch --target=my_build_target --artifact_name=radio.img --build_id=my_build_id --account_id=my_pab_account_id',
    'fetch --type=pab --branch=my_gsi_branch --target=my_gsi_build_target --gsi=True --artifact_name=my_gsi_build_target-img-{build_id}.zip --build_id=my_gsi_build_id --account_id=my_gsi_pab_account_id',
    'fetch --type=pab --branch=my_test_branch --target=my_test_build_target --artifact_name=android-{{test_suite}}.zip --build_id=my_test_build_id --account_id=my_test_pab_account_id',
    'info', 'gsispl --version_from_path=boot.img', 'info',
    [[
        'flash --current --serial my_serial1 --skip-vbmeta=True ',
        'adb -s my_serial1 root',
        'dut --operation=wifi_on --serial=my_serial1 --ap=GoogleGuest',
        'dut --operation=volume_mute --serial=my_serial1 --version=9.0'
    ], [
        'flash --current --serial my_serial2 --skip-vbmeta=True ',
        'adb -s my_serial2 root',
        'dut --operation=wifi_on --serial=my_serial2 --ap=GoogleGuest',
        'dut --operation=volume_mute --serial=my_serial2 --version=9.0'
    ], [
        'flash --current --serial my_serial3 --skip-vbmeta=True ',
        'adb -s my_serial3 root',
        'dut --operation=wifi_on --serial=my_serial3 --ap=GoogleGuest',
        'dut --operation=volume_mute --serial=my_serial3 --version=9.0'
    ]],
    'test --suite {{test_suite}} --keep-result -- {{test_plan}} --shards 3  --serial my_serial1 --serial my_serial2 --serial my_serial3',
    'retry --suite {{test_suite}} --count 3 {{retry_plan}} --shards 3 --serial my_serial1 --serial my_serial2 --serial my_serial3{{cleanup_device}}',
    'upload --src={result_full} --dest=report_bucket/{suite_plan}/{{test_plan}}/{branch}/{target}/my_build_target_{build_id}_{timestamp}/ --report_path=report_bucket/suite_result/{timestamp_year}/{timestamp_month}/{timestamp_day}',
    'sheet --src {result_zip} --dest report_spreadsheet_id --extra_rows logs,report_bucket/{suite_plan}/{{test_plan}}/{branch}/{target}/my_build_target_{build_id}_{timestamp}/ --primary_abi_only --client_secrets DATA/vtslab-gcs.json',
    'device --update=stop',
]


def GenerateInputData(test_name):
    """Returns an input data dict for a given `test_name`."""
    new_data = copy.copy(input_data)
    new_data["test_name"] = test_name
    return new_data


def GenerateOutputData(test_name):
    """Returns an output data list for a given `test_name`."""
    test_suite, test_plan = test_name.split("/")

    def ReplaceChars(line):
        line = line.replace('{{test_suite}}', test_suite)
        line = line.replace('{{test_plan}}', test_plan)
        if test_plan != "cts-on-gsi":
            line = line.replace(' --primary_abi_only', '')
        if (test_suite == "cts" or test_suite == "gts" or test_suite == "sts"
                or test_plan.startswith("cts-")):
            line = line.replace('--shards', "--shard-count")
            if test_suite == "vts":
                line = line.replace('{{retry_plan}}',
                                    '--retry_plan=%s-retry' % test_plan)
            else:
                line = line.replace('{{retry_plan}}', '--retry_plan=retry')
            line = line.replace('{{cleanup_device}}',
                                ' --cleanup_devices=True')
        else:
            line = line.replace('{{retry_plan}}', '')
            line = line.replace('{{cleanup_device}}', '')
        return line

    def RecursivelyApply(input_list, func):
        for number, item in enumerate(input_list):
            if type(item) is list:
                input_list[number] = RecursivelyApply(input_list[number], func)
            elif type(item) is str:
                input_list[number] = func(item)
            else:
                return None
        return input_list

    return RecursivelyApply(copy.copy(expected_output), ReplaceChars)
