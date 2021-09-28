/*
 * Copyright (C) 2020 The Android Open Source Project
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
package android.car.test.mocks;

import android.annotation.NonNull;
import android.util.Log;

import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.util.concurrent.CountDownLatch;

/**
 * Custom Mockito {@link Answer} used to block the test until it's invoked.
 *
 * <p>Example:
 *
 * <pre><code>
 *
 * SyncAnswer<UserInfo> syncUserInfo = SyncAnswer.forReturn(user);
 * when(mMock.preCreateUser(userType)).thenAnswer(syncUserInfo);
 * objectUnderTest.preCreate();
 * syncUserInfo.await(100);
 *
 * </code></pre>
 */
public final class SyncAnswer<T> implements Answer<T> {

    private static final String TAG = SyncAnswer.class.getSimpleName();

    private final CountDownLatch mLatch = new CountDownLatch(1);
    private final T mAnswer;
    private final Throwable mException;

    private SyncAnswer(T answer, Throwable exception) {
        mAnswer = answer;
        mException = exception;
    }

    /**
     * Creates an answer that returns a value.
     */
    @NonNull
    public static <T> SyncAnswer<T> forReturn(@NonNull T value) {
        return new SyncAnswer<T>(value, /* exception= */ null);
    }

    /**
     * Creates an answer that throws an exception.
     */
    @NonNull
    public static <T> SyncAnswer<T> forException(@NonNull Throwable t) {
        return new SyncAnswer<T>(/* value= */ null, t);
    }

    /**
     * Wait until the answer is invoked, or times out.
     */
    public void await(long timeoutMs) throws InterruptedException {
        JavaMockitoHelper.await(mLatch, timeoutMs);
    }

    @Override
    public T answer(InvocationOnMock invocation) throws Throwable {
        Log.v(TAG, "answering " + invocation);
        mLatch.countDown();
        if (mException != null) {
            throw mException;
        }
        return mAnswer;
    }

}
