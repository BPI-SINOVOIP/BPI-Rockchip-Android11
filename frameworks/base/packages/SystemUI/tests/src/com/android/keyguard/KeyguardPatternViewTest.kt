/*
 * Copyright (C) 2018 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.keyguard

import androidx.test.filters.SmallTest
import android.testing.AndroidTestingRunner
import android.testing.TestableLooper
import android.view.LayoutInflater

import com.android.systemui.R
import com.android.systemui.SysuiTestCase
import com.android.systemui.statusbar.policy.ConfigurationController
import com.google.common.truth.Truth.assertThat

import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock

@SmallTest
@RunWith(AndroidTestingRunner::class)
@TestableLooper.RunWithLooper
class KeyguardPatternViewTest : SysuiTestCase() {

    private lateinit var mKeyguardPatternView: KeyguardPatternView
    private lateinit var mSecurityMessage: KeyguardMessageArea

    @Before
    fun setup() {
        val inflater = LayoutInflater.from(context)
        mDependency.injectMockDependency(KeyguardUpdateMonitor::class.java)
        mKeyguardPatternView = inflater.inflate(R.layout.keyguard_pattern_view, null)
                as KeyguardPatternView
        mSecurityMessage = KeyguardMessageArea(mContext, null,
                mock(KeyguardUpdateMonitor::class.java), mock(ConfigurationController::class.java))
        mKeyguardPatternView.mSecurityMessageDisplay = mSecurityMessage
    }

    @Test
    fun onPause_clearsTextField() {
        mSecurityMessage.setMessage("an old message")
        mKeyguardPatternView.onPause()
        assertThat(mSecurityMessage.text).isEqualTo("")
    }
}
