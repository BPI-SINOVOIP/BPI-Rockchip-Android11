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
package com.android.internal.net.ipsec.ike;

import static android.net.ipsec.ike.IkeManager.getIkeLog;

import android.os.Looper;
import android.os.Message;
import android.util.SparseArray;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;

import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/**
 * This class represents the common information of both IkeSessionStateMachine and
 * ChildSessionStateMachine
 */
abstract class AbstractSessionStateMachine extends StateMachine {
    private static final int CMD_SHARED_BASE = 0;
    protected static final int CMD_CATEGORY_SIZE = 100;

    /**
     * Commands of Child local request that will be used in both IkeSessionStateMachine and
     * ChildSessionStateMachine.
     */
    protected static final int CMD_CHILD_LOCAL_REQUEST_BASE = CMD_SHARED_BASE;

    @VisibleForTesting
    static final int CMD_LOCAL_REQUEST_CREATE_CHILD = CMD_CHILD_LOCAL_REQUEST_BASE + 1;

    @VisibleForTesting
    static final int CMD_LOCAL_REQUEST_DELETE_CHILD = CMD_CHILD_LOCAL_REQUEST_BASE + 2;

    @VisibleForTesting
    static final int CMD_LOCAL_REQUEST_REKEY_CHILD = CMD_CHILD_LOCAL_REQUEST_BASE + 3;

    /** Timeout commands. */
    protected static final int CMD_TIMEOUT_BASE = CMD_SHARED_BASE + CMD_CATEGORY_SIZE;
    /** Timeout when the remote side fails to send a Rekey-Delete request. */
    @VisibleForTesting static final int TIMEOUT_REKEY_REMOTE_DELETE = CMD_TIMEOUT_BASE + 1;

    /** Commands for testing only */
    protected static final int CMD_TEST_BASE = CMD_SHARED_BASE + 2 * CMD_CATEGORY_SIZE;
    /** Force state machine to a target state for testing purposes. */
    @VisibleForTesting static final int CMD_FORCE_TRANSITION = CMD_TEST_BASE + 1;

    /** Private commands for subclasses */
    protected static final int CMD_PRIVATE_BASE = CMD_SHARED_BASE + 3 * CMD_CATEGORY_SIZE;

    protected static final SparseArray<String> SHARED_CMD_TO_STR;

    static {
        SHARED_CMD_TO_STR = new SparseArray<>();
        SHARED_CMD_TO_STR.put(CMD_LOCAL_REQUEST_CREATE_CHILD, "Create Child");
        SHARED_CMD_TO_STR.put(CMD_LOCAL_REQUEST_DELETE_CHILD, "Delete Child");
        SHARED_CMD_TO_STR.put(CMD_LOCAL_REQUEST_REKEY_CHILD, "Rekey Child");
        SHARED_CMD_TO_STR.put(TIMEOUT_REKEY_REMOTE_DELETE, "Timout rekey remote delete");
        SHARED_CMD_TO_STR.put(CMD_FORCE_TRANSITION, "Force transition");
    }

    // Use a value greater than the retransmit-failure timeout.
    static final long REKEY_DELETE_TIMEOUT_MS = TimeUnit.SECONDS.toMillis(180L);

    // Default delay time for retrying a request
    static final long RETRY_INTERVAL_MS = TimeUnit.SECONDS.toMillis(15L);

    protected final Executor mUserCbExecutor;
    private final String mLogTag;

    protected AbstractSessionStateMachine(String name, Looper looper, Executor userCbExecutor) {
        super(name, looper);
        mLogTag = name;
        mUserCbExecutor = userCbExecutor;
    }

    /**
     * Top level state for handling uncaught exceptions for all subclasses.
     *
     * <p>All other state in SessionStateMachine MUST extend this state.
     *
     * <p>Only errors this state should catch are unexpected internal failures. Since this may be
     * run in critical processes, it must never take down the process if it fails
     */
    protected abstract class ExceptionHandlerBase extends State {
        @Override
        public final void enter() {
            try {
                enterState();
            } catch (RuntimeException e) {
                cleanUpAndQuit(e);
            }
        }

        @Override
        public final boolean processMessage(Message message) {
            try {
                String cmdName = SHARED_CMD_TO_STR.get(message.what);
                if (cmdName == null) {
                    cmdName = getCmdString(message.what);
                }

                // Unrecognized message will be logged by super class(Android StateMachine)
                if (cmdName != null) logd("processStateMessage: " + cmdName);

                return processStateMessage(message);
            } catch (RuntimeException e) {
                cleanUpAndQuit(e);
                return HANDLED;
            }
        }

        @Override
        public final void exit() {
            try {
                exitState();
            } catch (RuntimeException e) {
                cleanUpAndQuit(e);
            }
        }

        protected void enterState() {
            // Do nothing. Subclasses MUST override it if they care.
        }

        protected boolean processStateMessage(Message message) {
            return NOT_HANDLED;
        }

        protected void exitState() {
            // Do nothing. Subclasses MUST override it if they care.
        }

        protected abstract void cleanUpAndQuit(RuntimeException e);

        protected abstract String getCmdString(int cmd);
    }

    protected void executeUserCallback(Runnable r) {
        try {
            mUserCbExecutor.execute(r);
        } catch (Exception e) {
            logd("Callback execution failed", e);
        }
    }

    @Override
    protected void log(String s) {
        getIkeLog().d(mLogTag, s);
    }

    @Override
    protected void logd(String s) {
        getIkeLog().d(mLogTag, s);
    }

    protected void logd(String s, Throwable e) {
        getIkeLog().d(mLogTag, s, e);
    }

    @Override
    protected void logv(String s) {
        getIkeLog().v(mLogTag, s);
    }

    @Override
    protected void logi(String s) {
        getIkeLog().i(mLogTag, s);
    }

    protected void logi(String s, Throwable cause) {
        getIkeLog().i(mLogTag, s, cause);
    }

    @Override
    protected void logw(String s) {
        getIkeLog().w(mLogTag, s);
    }

    @Override
    protected void loge(String s) {
        getIkeLog().e(mLogTag, s);
    }

    @Override
    protected void loge(String s, Throwable e) {
        getIkeLog().e(mLogTag, s, e);
    }

    protected void logWtf(String s) {
        getIkeLog().wtf(mLogTag, s);
    }

    protected void logWtf(String s, Throwable e) {
        getIkeLog().wtf(mLogTag, s, e);
    }
}
