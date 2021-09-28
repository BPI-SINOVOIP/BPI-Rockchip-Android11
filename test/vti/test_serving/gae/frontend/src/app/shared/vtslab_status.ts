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
export enum JobStatus {
  Ready = 0,
  Leased,
  Complete,
  Infra_err,
  Expired,
  Bootup_err,
}

export enum DeviceStatus {
  Unknown = 0,
  Fastboot,
  Online,
  Ready,
  Use,
  Error,
  No_response,
}

export enum SchedulingStatus {
  Free = 0,
  Reserved,
  Use,
}

/**
 * bit 0-1  : version related test type
 *            00 - Unknown
 *            01 - ToT
 *            10 - OTA
 * bit 2    : device signed build
 * bit 3-4  : reserved for gerrit related test type
 *            01 - pre-submit
 * bit 5    : manually created test job
 */
export enum TestType {
  Unknown = 0,
  ToT = 1,
  OTA = 1 << 1,
  Signed = 1 << 2,
  Presubmit = 1 << 3,
  Manual = 1 << 5,
}
