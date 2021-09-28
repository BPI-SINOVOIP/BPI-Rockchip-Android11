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
package android.net.ipsec.ike;

import android.annotation.NonNull;
import android.annotation.SuppressLint;
import android.annotation.SystemApi;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.IpSecManager;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.CloseGuard;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.net.ipsec.ike.IkeSessionStateMachine;

import java.util.concurrent.Executor;

/**
 * This class represents an IKE Session management object that allows for keying and management of
 * {@link IpSecTransform}s.
 *
 * <p>An IKE/Child Session represents an IKE/Child SA as well as its rekeyed successors. A Child
 * Session is bounded by the lifecycle of the IKE Session under which it is set up. Closing an IKE
 * Session implicitly closes any remaining Child Sessions under it.
 *
 * <p>An IKE procedure is one or multiple IKE message exchanges that are used to create, delete or
 * rekey an IKE Session or Child Session.
 *
 * <p>This class provides methods for initiating IKE procedures, such as the Creation and Deletion
 * of a Child Session, or the Deletion of the IKE session. All procedures (except for IKE deletion)
 * will be initiated sequentially after IKE Session is set up.
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296">RFC 7296, Internet Key Exchange Protocol
 *     Version 2 (IKEv2)</a>
 * @hide
 */
@SystemApi
public final class IkeSession implements AutoCloseable {
    private final CloseGuard mCloseGuard = new CloseGuard();
    private final Context mContext;

    @VisibleForTesting final IkeSessionStateMachine mIkeSessionStateMachine;

    /**
     * Constructs a new IKE session.
     *
     * <p>This method will immediately return an instance of {@link IkeSession} and asynchronously
     * initiate the setup procedure of {@link IkeSession} as well as its first Child Session.
     * Callers will be notified of these two setup results via the callback arguments.
     *
     * <p>FEATURE_IPSEC_TUNNELS is required for setting up a tunnel mode Child SA.
     *
     * @param context a valid {@link Context} instance.
     * @param ikeSessionParams the {@link IkeSessionParams} that contains a set of valid {@link
     *     IkeSession} configurations.
     * @param firstChildSessionParams the {@link ChildSessionParams} that contains a set of valid
     *     configurations for the first Child Session.
     * @param userCbExecutor the {@link Executor} upon which all callbacks will be posted. For
     *     security and consistency, the callbacks posted to this executor MUST be executed serially
     *     and in the order they were posted, as guaranteed by executors such as {@link
     *     ExecutorService.newSingleThreadExecutor()}
     * @param ikeSessionCallback the {@link IkeSessionCallback} interface to notify callers of state
     *     changes within the {@link IkeSession}.
     * @param firstChildSessionCallback the {@link ChildSessionCallback} interface to notify callers
     *     of state changes within the first Child Session.
     * @return an instance of {@link IkeSession}.
     */
    public IkeSession(
            @NonNull Context context,
            @NonNull IkeSessionParams ikeSessionParams,
            @NonNull ChildSessionParams firstChildSessionParams,
            @NonNull Executor userCbExecutor,
            @NonNull IkeSessionCallback ikeSessionCallback,
            @NonNull ChildSessionCallback firstChildSessionCallback) {
        this(
                context,
                (IpSecManager) context.getSystemService(Context.IPSEC_SERVICE),
                ikeSessionParams,
                firstChildSessionParams,
                userCbExecutor,
                ikeSessionCallback,
                firstChildSessionCallback);
    }

    /** Package private */
    @VisibleForTesting
    IkeSession(
            Context context,
            IpSecManager ipSecManager,
            IkeSessionParams ikeSessionParams,
            ChildSessionParams firstChildSessionParams,
            Executor userCbExecutor,
            IkeSessionCallback ikeSessionCallback,
            ChildSessionCallback firstChildSessionCallback) {
        this(
                IkeThreadHolder.IKE_WORKER_THREAD.getLooper(),
                context,
                ipSecManager,
                ikeSessionParams,
                firstChildSessionParams,
                userCbExecutor,
                ikeSessionCallback,
                firstChildSessionCallback);
    }

    /** Package private */
    @VisibleForTesting
    IkeSession(
            Looper looper,
            Context context,
            IpSecManager ipSecManager,
            IkeSessionParams ikeSessionParams,
            ChildSessionParams firstChildSessionParams,
            Executor userCbExecutor,
            IkeSessionCallback ikeSessionCallback,
            ChildSessionCallback firstChildSessionCallback) {
        mContext = context;

        if (firstChildSessionParams instanceof TunnelModeChildSessionParams) {
            checkTunnelFeatureOrThrow(mContext);
        }

        mIkeSessionStateMachine =
                new IkeSessionStateMachine(
                        looper,
                        context,
                        ipSecManager,
                        ikeSessionParams,
                        firstChildSessionParams,
                        userCbExecutor,
                        ikeSessionCallback,
                        firstChildSessionCallback);
        mIkeSessionStateMachine.openSession();

        mCloseGuard.open("open");
    }

    /** @hide */
    @Override
    public void finalize() {
        if (mCloseGuard != null) {
            mCloseGuard.warnIfOpen();
        }
    }

    private void checkTunnelFeatureOrThrow(Context context) {
        // TODO(b/157754168): Also check if OP_MANAGE_IPSEC_TUNNELS is granted when it is exposed
        if (!context.getPackageManager().hasSystemFeature(PackageManager.FEATURE_IPSEC_TUNNELS)) {
            throw new IllegalStateException(
                    "Cannot set up tunnel mode Child SA due to FEATURE_IPSEC_TUNNELS missing");
        }
    }

    /** Initialization-on-demand holder */
    private static class IkeThreadHolder {
        static final HandlerThread IKE_WORKER_THREAD;

        static {
            IKE_WORKER_THREAD = new HandlerThread("IkeWorkerThread");
            IKE_WORKER_THREAD.start();
        }
    }

    // TODO: b/133340675 Destroy the worker thread when there is no more alive {@link IkeSession}.

    /**
     * Request a new Child Session.
     *
     * <p>Users MUST provide a unique {@link ChildSessionCallback} instance for each new Child
     * Session.
     *
     * <p>Upon setup, {@link ChildSessionCallback#onOpened(ChildSessionConfiguration)} will be
     * fired.
     *
     * <p>FEATURE_IPSEC_TUNNELS is required for setting up a tunnel mode Child SA.
     *
     * @param childSessionParams the {@link ChildSessionParams} that contains the Child Session
     *     configurations to negotiate.
     * @param childSessionCallback the {@link ChildSessionCallback} interface to notify users the
     *     state changes of the Child Session. It will be posted to the callback {@link Executor} of
     *     this {@link IkeSession}.
     * @throws IllegalArgumentException if the ChildSessionCallback is already in use.
     */
    // The childSessionCallback will be called on the same executor as was passed in the constructor
    // for security reasons.
    @SuppressLint("ExecutorRegistration")
    public void openChildSession(
            @NonNull ChildSessionParams childSessionParams,
            @NonNull ChildSessionCallback childSessionCallback) {
        if (childSessionParams instanceof TunnelModeChildSessionParams) {
            checkTunnelFeatureOrThrow(mContext);
        }

        mIkeSessionStateMachine.openChildSession(childSessionParams, childSessionCallback);
    }

    /**
     * Delete a Child Session.
     *
     * <p>Upon closure, {@link ChildSessionCallback#onClosed()} will be fired.
     *
     * @param childSessionCallback The {@link ChildSessionCallback} instance that uniquely identify
     *     the Child Session.
     * @throws IllegalArgumentException if no Child Session found bound with this callback.
     */
    // The childSessionCallback will be called on the same executor as was passed in the constructor
    // for security reasons.
    @SuppressLint("ExecutorRegistration")
    public void closeChildSession(@NonNull ChildSessionCallback childSessionCallback) {
        mIkeSessionStateMachine.closeChildSession(childSessionCallback);
    }

    /**
     * Close the IKE session gracefully.
     *
     * <p>Implements {@link AutoCloseable#close()}
     *
     * <p>Upon closure, {@link IkeSessionCallback#onClosed()} or {@link
     * IkeSessionCallback#onClosedExceptionally()} will be fired.
     *
     * <p>Closing an IKE Session implicitly closes any remaining Child Sessions negotiated under it.
     * Users SHOULD stop all outbound traffic that uses these Child Sessions({@link IpSecTransform}
     * pairs) before calling this method. Otherwise IPsec packets will be dropped due to the lack of
     * a valid {@link IpSecTransform}.
     *
     * <p>Closure of an IKE session will take priority over, and cancel other procedures waiting in
     * the queue (but will wait for ongoing locally initiated procedures to complete). After sending
     * the Delete request, the IKE library will wait until a Delete response is received or
     * retransmission timeout occurs.
     */
    @Override
    public void close() {
        mCloseGuard.close();
        mIkeSessionStateMachine.closeSession();
    }

    /**
     * Terminate (forcibly close) the IKE session.
     *
     * <p>Upon closing, {@link IkeSessionCallback#onClosed()} will be fired.
     *
     * <p>Closing an IKE Session implicitly closes any remaining Child Sessions negotiated under it.
     * Users SHOULD stop all outbound traffic that uses these Child Sessions({@link IpSecTransform}
     * pairs) before calling this method. Otherwise IPsec packets will be dropped due to the lack of
     * a valid {@link IpSecTransform}.
     *
     * <p>Forcible closure of an IKE session will take priority over, and cancel other procedures
     * waiting in the queue. It will also interrupt any ongoing locally initiated procedure.
     */
    public void kill() {
        mCloseGuard.close();
        mIkeSessionStateMachine.killSession();
    }
}
