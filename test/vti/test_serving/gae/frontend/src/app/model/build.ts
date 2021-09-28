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
export class Build {
  manifest_branch: string = void 0;
  build_id: string = void 0;
  build_target: string = void 0;
  build_type: string = void 0;
  artifact_type: string = void 0;
  artifacts: string[] = void 0;
  signed: boolean = void 0;
  timestamp: any = void 0;
}
