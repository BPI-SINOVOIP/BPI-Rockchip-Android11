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
 * limitations under the License
 */

package com.android.systemui.broadcast

import android.content.BroadcastReceiver
import android.content.Context
import android.content.IntentFilter
import android.os.Handler
import android.os.UserHandle
import android.test.suitebuilder.annotation.SmallTest
import android.testing.AndroidTestingRunner
import android.testing.TestableLooper
import com.android.systemui.SysuiTestCase
import com.android.systemui.broadcast.logging.BroadcastDispatcherLogger
import com.android.systemui.util.concurrency.FakeExecutor
import com.android.systemui.util.time.FakeSystemClock
import junit.framework.Assert.assertFalse
import junit.framework.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentCaptor
import org.mockito.Mock
import org.mockito.Mockito
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations
import java.util.concurrent.Executor

@RunWith(AndroidTestingRunner::class)
@TestableLooper.RunWithLooper
@SmallTest
class UserBroadcastDispatcherTest : SysuiTestCase() {

    companion object {
        private const val ACTION_1 = "com.android.systemui.tests.ACTION_1"
        private const val ACTION_2 = "com.android.systemui.tests.ACTION_2"
        private const val USER_ID = 0
        private val USER_HANDLE = UserHandle.of(USER_ID)

        fun <T> capture(argumentCaptor: ArgumentCaptor<T>): T = argumentCaptor.capture()
        fun <T> any(): T = Mockito.any()
        fun <T> eq(v: T) = Mockito.eq(v) ?: v
    }

    @Mock
    private lateinit var broadcastReceiver: BroadcastReceiver
    @Mock
    private lateinit var broadcastReceiverOther: BroadcastReceiver
    @Mock
    private lateinit var mockContext: Context
    @Mock
    private lateinit var logger: BroadcastDispatcherLogger

    private lateinit var testableLooper: TestableLooper
    private lateinit var userBroadcastDispatcher: UserBroadcastDispatcher
    private lateinit var intentFilter: IntentFilter
    private lateinit var intentFilterOther: IntentFilter
    private lateinit var handler: Handler
    private lateinit var fakeExecutor: FakeExecutor

    @Before
    fun setUp() {
        MockitoAnnotations.initMocks(this)
        testableLooper = TestableLooper.get(this)
        handler = Handler(testableLooper.looper)
        fakeExecutor = FakeExecutor(FakeSystemClock())

        userBroadcastDispatcher = object : UserBroadcastDispatcher(
                mockContext, USER_ID, testableLooper.looper, mock(Executor::class.java), logger) {
            override fun createActionReceiver(action: String): ActionReceiver {
                return mock(ActionReceiver::class.java)
            }
        }
    }

    @Test
    fun testSingleReceiverRegistered() {
        intentFilter = IntentFilter(ACTION_1)
        val receiverData = ReceiverData(broadcastReceiver, intentFilter, fakeExecutor, USER_HANDLE)

        userBroadcastDispatcher.registerReceiver(receiverData)
        testableLooper.processAllMessages()

        val actionReceiver = userBroadcastDispatcher.getActionReceiver(ACTION_1)
        assertNotNull(actionReceiver)
        verify(actionReceiver)?.addReceiverData(receiverData)
    }

    @Test
    fun testSingleReceiverRegistered_logging() {
        intentFilter = IntentFilter(ACTION_1)

        userBroadcastDispatcher.registerReceiver(
                ReceiverData(broadcastReceiver, intentFilter, fakeExecutor, USER_HANDLE))
        testableLooper.processAllMessages()

        verify(logger).logReceiverRegistered(USER_HANDLE.identifier, broadcastReceiver)
    }

    @Test
    fun testSingleReceiverUnregistered() {
        intentFilter = IntentFilter(ACTION_1)

        userBroadcastDispatcher.registerReceiver(
                ReceiverData(broadcastReceiver, intentFilter, fakeExecutor, USER_HANDLE))
        testableLooper.processAllMessages()

        userBroadcastDispatcher.unregisterReceiver(broadcastReceiver)
        testableLooper.processAllMessages()

        val actionReceiver = userBroadcastDispatcher.getActionReceiver(ACTION_1)
        assertNotNull(actionReceiver)
        verify(actionReceiver)?.removeReceiver(broadcastReceiver)
    }

    @Test
    fun testSingleReceiverUnregistered_logger() {
        intentFilter = IntentFilter(ACTION_1)

        userBroadcastDispatcher.registerReceiver(
                ReceiverData(broadcastReceiver, intentFilter, fakeExecutor, USER_HANDLE))
        testableLooper.processAllMessages()

        userBroadcastDispatcher.unregisterReceiver(broadcastReceiver)
        testableLooper.processAllMessages()

        verify(logger).logReceiverUnregistered(USER_HANDLE.identifier, broadcastReceiver)
    }

    @Test
    fun testRemoveReceiverReferences() {
        intentFilter = IntentFilter(ACTION_1)
        userBroadcastDispatcher.registerReceiver(
                ReceiverData(broadcastReceiver, intentFilter, fakeExecutor, USER_HANDLE))

        intentFilterOther = IntentFilter(ACTION_1)
        intentFilterOther.addAction(ACTION_2)
        userBroadcastDispatcher.registerReceiver(
                ReceiverData(broadcastReceiverOther, intentFilterOther, fakeExecutor, USER_HANDLE))

        userBroadcastDispatcher.unregisterReceiver(broadcastReceiver)
        testableLooper.processAllMessages()
        fakeExecutor.runAllReady()

        assertFalse(userBroadcastDispatcher.isReceiverReferenceHeld(broadcastReceiver))
    }

    private fun UserBroadcastDispatcher.getActionReceiver(action: String): ActionReceiver? {
        return actionsToActionsReceivers.get(action)
    }
}
