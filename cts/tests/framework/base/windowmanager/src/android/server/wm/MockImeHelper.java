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

package android.server.wm;

import com.android.cts.mockime.MockImeSession;

/**
 * Centralizes the creation of {@link MockImeSession}. This class ins't placed in utility group
 * because only this package uses it.
 */
public class MockImeHelper {

    /**
     * Leverage MockImeSession to ensure at least an IME exists as default.
     *
     * @see ObjectTracker#manage(AutoCloseable)
     */
    public static MockImeSession createManagedMockImeSession(ActivityManagerTestBase base) {
        try {
            return base.mObjectTracker.manage(MockImeSession.create(base.mContext));
        } catch (Exception e) {
            throw new RuntimeException("Failed to create MockImeSession", e);
        }
    }
}
