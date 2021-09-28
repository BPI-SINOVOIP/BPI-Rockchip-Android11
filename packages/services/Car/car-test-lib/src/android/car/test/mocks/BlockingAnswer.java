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
import android.annotation.Nullable;
import android.car.test.util.Visitor;
import android.util.Log;

import com.android.internal.util.FunctionalUtils;

import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.util.concurrent.CountDownLatch;

/**
 * Custom Mockito {@link Answer} that blocks until a condition is unblocked by the test case.
 *
 * <p>Example:
 *
 * <pre><code>
 *
 *  BlockingAnswer<Void> blockingAnswer = BlockingAnswer.forVoidReturn(10_000, (invocation) -> {
 *     MyObject obj = (MyObject) invocation.getArguments()[0];
 *     obj.doSomething();
 *  });
 *  doAnswer(blockingAnswer).when(mMock).mockSomething();
 *  doSomethingOnTest();
 *  blockingAnswer.unblock();
 *
 * </code></pre>
 */
public final class BlockingAnswer<T> implements Answer<T> {

    private static final String TAG = BlockingAnswer.class.getSimpleName();

    private final CountDownLatch mLatch = new CountDownLatch(1);

    private final long mTimeoutMs;

    @NonNull
    private final Visitor<InvocationOnMock> mInvocator;

    @Nullable
    private final T mValue;

    private BlockingAnswer(long timeoutMs, @NonNull Visitor<InvocationOnMock> invocator,
            @Nullable T value) {
        mTimeoutMs = timeoutMs;
        mInvocator = invocator;
        mValue = value;
    }

    /**
     * Unblocks the answer so the test case can continue.
     */
    public void unblock() {
        Log.v(TAG, "Unblocking " + mLatch);
        mLatch.countDown();
    }

    /**
     * Creates a {@link BlockingAnswer} for a {@code void} method.
     *
     * @param timeoutMs maximum time the answer would block.
     *
     * @param invocator code executed after the answer is unblocked.
     */
    public static BlockingAnswer<Void> forVoidReturn(long timeoutMs,
            @NonNull Visitor<InvocationOnMock> invocator) {
        return new BlockingAnswer<Void>(timeoutMs, invocator, /* value= */ null);
    }

    /**
     * Creates a {@link BlockingAnswer} for a method that returns type {@code T}
     *
     * @param timeoutMs maximum time the answer would block.
     * @param invocator code executed after the answer is unblocked.
     *
     * @return what the answer should return
     */
    public static <T> BlockingAnswer<T> forReturn(long timeoutMs,
            @NonNull Visitor<InvocationOnMock> invocator, @Nullable T value) {
        return new BlockingAnswer<T>(timeoutMs, invocator, value);
    }

    @Override
    public T answer(InvocationOnMock invocation) throws Throwable {
        String threadName = "BlockingAnswerThread";
        Log.d(TAG, "Starting thread " + threadName);

        new Thread(() -> {
            JavaMockitoHelper.silentAwait(mLatch, mTimeoutMs);
            Log.d(TAG, "Invocating " + FunctionalUtils.getLambdaName(mInvocator));
            mInvocator.visit(invocation);
        }, threadName).start();
        Log.d(TAG, "Returning " + mValue);
        return mValue;
    }
}
