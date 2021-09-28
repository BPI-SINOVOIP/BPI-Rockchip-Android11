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

import android.app.AlarmManager;
import android.content.Context;
import android.net.IpSecManager;
import android.net.ipsec.ike.ChildSessionCallback;
import android.net.ipsec.ike.ChildSessionParams;
import android.os.Looper;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.net.ipsec.ike.ChildSessionStateMachine.IChildSessionSmCallback;
import com.android.internal.net.ipsec.ike.utils.IpSecSpiGenerator;
import com.android.internal.net.ipsec.ike.utils.RandomnessFactory;

import java.util.concurrent.Executor;

/** Package private factory for making ChildSessionStateMachine. */
// TODO: Make it a inner Creator class of ChildSessionStateMachine
final class ChildSessionStateMachineFactory {

    private static IChildSessionFactoryHelper sChildSessionHelper = new ChildSessionFactoryHelper();

    /** Package private. */
    static ChildSessionStateMachine makeChildSessionStateMachine(
            Looper looper,
            Context context,
            int ikeSessionUniqueId,
            AlarmManager alarmManager,
            RandomnessFactory randomFactory,
            IpSecSpiGenerator ipSecSpiGenerator,
            ChildSessionParams sessionParams,
            Executor userCbExecutor,
            ChildSessionCallback userCallbacks,
            IChildSessionSmCallback childSmCallback) {
        return sChildSessionHelper.makeChildSessionStateMachine(
                looper,
                context,
                ikeSessionUniqueId,
                alarmManager,
                randomFactory,
                ipSecSpiGenerator,
                sessionParams,
                userCbExecutor,
                userCallbacks,
                childSmCallback);
    }

    @VisibleForTesting
    static void setChildSessionFactoryHelper(IChildSessionFactoryHelper helper) {
        sChildSessionHelper = helper;
    }

    /**
     * IChildSessionFactoryHelper provides a package private interface for constructing
     * ChildSessionStateMachine.
     *
     * <p>IChildSessionFactoryHelper exists so that the interface is injectable for testing.
     */
    interface IChildSessionFactoryHelper {
        ChildSessionStateMachine makeChildSessionStateMachine(
                Looper looper,
                Context context,
                int ikeSessionUniqueId,
                AlarmManager alarmManager,
                RandomnessFactory randomFactory,
                IpSecSpiGenerator ipSecSpiGenerator,
                ChildSessionParams sessionParams,
                Executor userCbExecutor,
                ChildSessionCallback userCallbacks,
                IChildSessionSmCallback childSmCallback);
    }

    /**
     * ChildSessionFactoryHelper implements a method for constructing ChildSessionStateMachine.
     *
     * <p>Package private.
     */
    static class ChildSessionFactoryHelper implements IChildSessionFactoryHelper {
        public ChildSessionStateMachine makeChildSessionStateMachine(
                Looper looper,
                Context context,
                int ikeSessionUniqueId,
                AlarmManager alarmManager,
                RandomnessFactory randomFactory,
                IpSecSpiGenerator ipSecSpiGenerator,
                ChildSessionParams sessionParams,
                Executor userCbExecutor,
                ChildSessionCallback userCallbacks,
                IChildSessionSmCallback childSmCallback) {
            ChildSessionStateMachine childSession =
                    new ChildSessionStateMachine(
                            looper,
                            context,
                            ikeSessionUniqueId,
                            alarmManager,
                            randomFactory,
                            (IpSecManager) context.getSystemService(Context.IPSEC_SERVICE),
                            ipSecSpiGenerator,
                            sessionParams,
                            userCbExecutor,
                            userCallbacks,
                            childSmCallback);
            childSession.start();
            return childSession;
        }
    }
}
