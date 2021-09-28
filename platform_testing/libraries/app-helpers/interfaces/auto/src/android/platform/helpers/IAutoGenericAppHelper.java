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

import java.util.Map;

/**
 * An interface to open any applications. One of the member methods must be invoked after creating
 * an instance of this class.
 */
public interface IAutoGenericAppHelper extends IAppHelper, Scrollable {
    /**
     * Set the package to open. The application will be opened using the info activity or launcher
     * activity of the package that has been injected here.
     */
    void setPackage(String pkg);

    /**
     * Set the launch activity. The application will be opened directly using the provided activity.
     */
    void setLaunchActivity(String pkg);

    /**
     * Set the launch action. The application will be opened using the provided launch action with
     * given extra arguments.
     */
    void setLaunchAction(String action, Map<String, String> extraArgs);
}
