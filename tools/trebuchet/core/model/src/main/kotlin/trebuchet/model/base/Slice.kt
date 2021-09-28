/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package trebuchet.model.base

interface Slice {
    /**
     * Beginning of slice in seconds
     */
    val startTime: Double
    /**
     * End of slice in seconds
     */
    val endTime: Double
    val name: String
    val didNotFinish: Boolean

    /**
     * Total time taken by this slice in seconds
     */
    val duration: Double get() = endTime - startTime

    /**
     * Duration excluding time spend in children.
     *
     * The duration is in seconds.
     */
    val durationSelf: Double get() = duration
}