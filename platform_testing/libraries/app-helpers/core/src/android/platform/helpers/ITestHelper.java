/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.platform.helpers;

/**
 * An interface for classes that will be loaded at runtime by HelperManager. All concrete helper
 * implementations to be loaded by HelperManager must implement some interface that inherits from
 * ITestHelper. This ensures that HelperManager does not attempt to load classes that were not
 * intended to be loaded as part of a test.
 *
 * <p>Because HelperManager can be used in widely varying scenarios (e.g. platform testing, CTS),
 * this interface does not impose any particular structure. Helper implementations for a specific
 * type of testing should almost certainly implement a more detailed subinterface (see IAppHelper
 * for an example).
 */
public interface ITestHelper {
    // Currently just a placeholder interface.
}
