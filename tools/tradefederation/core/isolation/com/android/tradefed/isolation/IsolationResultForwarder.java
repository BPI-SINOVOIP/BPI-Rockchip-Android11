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
package com.android.tradefed.isolation;

import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunListener;

import java.io.IOException;
import java.io.OutputStream;

/**
 * This implements a listener for JUnit to send results back to the TradeFed host.
 *
 * <p>This just maps the various JUnit events into TestEvents that are then sent over the wire to
 * the main TradeFed process.
 */
final class IsolationResultForwarder extends RunListener {

    private OutputStream mOutput;
    private long mStartMillis;

    /**
     * Instantiates a new IsolationResultForwarder
     *
     * @param output an OutputStream over which to send results of testing. Should end up back at
     *     the main TradeFed process.
     */
    public IsolationResultForwarder(OutputStream output) {
        mOutput = output;
    }

    @Override
    public void testFailure(Failure failure) throws IOException {
        Description desc = failure.getDescription();
        RunnerReply.newBuilder()
                .setRunnerStatus(RunnerStatus.RUNNER_STATUS_UNSPECIFIED)
                .setTestEvent(
                        JUnitEvent.newBuilder()
                                .setTopic(Topic.TOPIC_FAILURE)
                                .setMessage(failure.getMessage())
                                .setMethodName(desc.getMethodName())
                                .setClassName(desc.getClassName())
                                .build())
                .build()
                .writeDelimitedTo(mOutput);
    }

    @Override
    public void testAssumptionFailure(Failure failure) {
        try {
            Description desc = failure.getDescription();
            RunnerReply.newBuilder()
                    .setRunnerStatus(RunnerStatus.RUNNER_STATUS_UNSPECIFIED)
                    .setTestEvent(
                            JUnitEvent.newBuilder()
                                    .setTopic(Topic.TOPIC_ASSUMPTION_FAILURE)
                                    .setMessage(failure.getMessage())
                                    .setMethodName(desc.getMethodName())
                                    .setClassName(desc.getClassName())
                                    .build())
                    .build()
                    .writeDelimitedTo(mOutput);
        } catch (Exception e) {

        }
    }

    @Override
    public void testStarted(Description description) throws IOException {
        RunnerReply.newBuilder()
                .setRunnerStatus(RunnerStatus.RUNNER_STATUS_UNSPECIFIED)
                .setTestEvent(
                        JUnitEvent.newBuilder()
                                .setTopic(Topic.TOPIC_STARTED)
                                .setMessage("")
                                .setMethodName(description.getMethodName())
                                .setClassName(description.getClassName())
                                .build())
                .build()
                .writeDelimitedTo(mOutput);
    }

    @Override
    public void testFinished(Description description) throws IOException {
        RunnerReply.newBuilder()
                .setRunnerStatus(RunnerStatus.RUNNER_STATUS_UNSPECIFIED)
                .setTestEvent(
                        JUnitEvent.newBuilder()
                                .setTopic(Topic.TOPIC_FINISHED)
                                .setMessage("")
                                .setMethodName(description.getMethodName())
                                .setClassName(description.getClassName())
                                .build())
                .build()
                .writeDelimitedTo(mOutput);
    }

    @Override
    public void testIgnored(Description description) throws IOException {
        RunnerReply.newBuilder()
                .setRunnerStatus(RunnerStatus.RUNNER_STATUS_UNSPECIFIED)
                .setTestEvent(
                        JUnitEvent.newBuilder()
                                .setTopic(Topic.TOPIC_IGNORED)
                                .setMessage("")
                                .setMethodName(description.getMethodName())
                                .setClassName(description.getClassName())
                                .build())
                .build()
                .writeDelimitedTo(mOutput);
    }

    @Override
    public void testRunStarted(Description description) throws IOException {
        mStartMillis = System.currentTimeMillis();
        RunnerReply.newBuilder()
                .setRunnerStatus(RunnerStatus.RUNNER_STATUS_UNSPECIFIED)
                .setTestEvent(
                        JUnitEvent.newBuilder()
                                .setTopic(Topic.TOPIC_RUN_STARTED)
                                .setMessage("")
                                .setClassName(description.getClassName())
                                .setTestCount(description.testCount())
                                .build())
                .build()
                .writeDelimitedTo(mOutput);
    }

    @Override
    public void testRunFinished(Result result) throws IOException {
        RunnerReply.newBuilder()
                .setRunnerStatus(RunnerStatus.RUNNER_STATUS_UNSPECIFIED)
                .setTestEvent(
                        JUnitEvent.newBuilder()
                                .setTopic(Topic.TOPIC_RUN_FINISHED)
                                .setMessage("")
                                .setElapsedTime(System.currentTimeMillis() - mStartMillis)
                                .build())
                .build()
                .writeDelimitedTo(mOutput);
    }
}
