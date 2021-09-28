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
export class Host {
  name: string = void 0;  // lab name
  owner: string = void 0;
  admin: string[] = void 0;
  hostname: string = void 0;
  ip: string = void 0;
  devices: string = void 0;
  vtslab_version: string = void 0;
  host_equipment: string[] = void 0;
}
