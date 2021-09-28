/**
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
export class Job {
  test_type: number = void 0;

  hostname: string = void 0;
  priority: string = void 0;
  test_name: string = void 0;
  require_signed_device_build: boolean = void 0;
  has_bootloader_img: boolean = void 0;
  has_radio_img: boolean = void 0;
  device: string = void 0;
  serial: string = void 0;

  // device image information
  build_storage_type: number = void 0;
  manifest_branch: string = void 0;
  build_target: string = void 0;
  build_id: string = void 0;
  pab_account_id: string = void 0;

  shards: number = void 0;
  param: string = void 0;
  status: number = void 0;
  period: number = void 0;

  // GSI information
  gsi_storage_type: number = void 0;
  gsi_branch: string = void 0;
  gsi_build_target: string = void 0;
  gsi_build_id: string = void 0;
  gsi_pab_account_id: string = void 0;
  gsi_vendor_version: string = void 0;

  // test suite information
  test_storage_type: number = void 0;
  test_branch: string = void 0;
  test_build_target: string = void 0;
  test_build_id: string = void 0;
  test_pab_account_id: string = void 0;

  retry_count: number = void 0;

  infra_log_url: string = void 0;

  image_package_repo_base: string = void 0;

  report_bucket: string = void 0;
  report_spreadsheet_id: string = void 0;

  timestamp = void 0;
  heartbeat_stamp = void 0;
}
