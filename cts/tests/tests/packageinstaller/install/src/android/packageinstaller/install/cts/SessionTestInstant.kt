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
package android.packageinstaller.install.cts

import android.content.pm.PackageInstaller
import android.platform.test.annotations.AppModeInstant
import androidx.test.InstrumentationRegistry
import androidx.test.runner.AndroidJUnit4
import org.junit.Test
import org.junit.runner.RunWith
import java.lang.NullPointerException

@AppModeInstant(reason = "This just makes sure that instant apps cannot run the SessionTest")
@RunWith(AndroidJUnit4::class)
class SessionTestInstant {
    private val context = InstrumentationRegistry.getTargetContext()
    private val pm = context.packageManager

    /**
     * Check that an instant app cannot install an app via a package-installer session
     */
    @Test(expected = NullPointerException::class)
    fun instantAppsCannotCreateInstallSessions() {
        pm.packageInstaller.createSession(
                PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL))
    }
}
