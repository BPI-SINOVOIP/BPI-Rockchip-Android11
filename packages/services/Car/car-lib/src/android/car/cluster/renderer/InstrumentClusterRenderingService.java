/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.car.cluster.renderer;

import static android.content.PermissionChecker.PERMISSION_GRANTED;

import android.annotation.CallSuper;
import android.annotation.MainThread;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.SystemApi;
import android.annotation.UserIdInt;
import android.app.ActivityOptions;
import android.app.Service;
import android.car.Car;
import android.car.CarLibLog;
import android.car.cluster.ClusterActivityState;
import android.car.navigation.CarNavigationInstrumentCluster;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.UserHandle;
import android.util.Log;
import android.util.LruCache;
import android.view.KeyEvent;

import com.android.internal.annotations.GuardedBy;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Supplier;
import java.util.stream.Collectors;

/**
 * A service used for interaction between Car Service and Instrument Cluster. Car Service may
 * provide internal navigation binder interface to Navigation App and all notifications will be
 * eventually land in the {@link NavigationRenderer} returned by {@link #getNavigationRenderer()}.
 *
 * <p>To extend this class, you must declare the service in your manifest file with
 * the {@code android.car.permission.BIND_INSTRUMENT_CLUSTER_RENDERER_SERVICE} permission
 * <pre>
 * &lt;service android:name=".MyInstrumentClusterService"
 *          android:permission="android.car.permission.BIND_INSTRUMENT_CLUSTER_RENDERER_SERVICE">
 * &lt;/service></pre>
 * <p>Also, you will need to register this service in the following configuration file:
 * {@code packages/services/Car/service/res/values/config.xml}
 *
 * @hide
 */
@SystemApi
public abstract class InstrumentClusterRenderingService extends Service {
    /**
     * Key to pass IInstrumentClusterHelper binder in onBind call {@link Intent} through extra
     * {@link Bundle). Both extra bundle and binder itself use this key.
     *
     * @hide
     */
    public static final String EXTRA_BUNDLE_KEY_FOR_INSTRUMENT_CLUSTER_HELPER =
            "android.car.cluster.renderer.IInstrumentClusterHelper";

    private static final String TAG = CarLibLog.TAG_CLUSTER;

    private static final String BITMAP_QUERY_WIDTH = "w";
    private static final String BITMAP_QUERY_HEIGHT = "h";
    private static final String BITMAP_QUERY_OFFLANESALPHA = "offLanesAlpha";

    private final Handler mUiHandler = new Handler(Looper.getMainLooper());

    private final Object mLock = new Object();
    // Main thread only
    private RendererBinder mRendererBinder;
    private ActivityOptions mActivityOptions;
    private ClusterActivityState mActivityState;
    private ComponentName mNavigationComponent;
    @GuardedBy("mLock")
    private ContextOwner mNavContextOwner;

    @GuardedBy("mLock")
    private IInstrumentClusterHelper mInstrumentClusterHelper;

    private static final int IMAGE_CACHE_SIZE_BYTES = 4 * 1024 * 1024; /* 4 mb */
    private final LruCache<String, Bitmap> mCache = new LruCache<String, Bitmap>(
            IMAGE_CACHE_SIZE_BYTES) {
        @Override
        protected int sizeOf(String key, Bitmap value) {
            return value.getByteCount();
        }
    };

    private static class ContextOwner {
        final int mUid;
        final int mPid;
        final Set<String> mPackageNames;
        final Set<String> mAuthorities;

        ContextOwner(int uid, int pid, PackageManager packageManager) {
            mUid = uid;
            mPid = pid;
            String[] packageNames = uid != 0 ? packageManager.getPackagesForUid(uid)
                    : null;
            mPackageNames = packageNames != null
                    ? Collections.unmodifiableSet(new HashSet<>(Arrays.asList(packageNames)))
                    : Collections.emptySet();
            mAuthorities = Collections.unmodifiableSet(mPackageNames.stream()
                    .map(packageName -> getAuthoritiesForPackage(packageManager, packageName))
                    .flatMap(Collection::stream)
                    .collect(Collectors.toSet()));
        }

        @Override
        public String toString() {
            return "{uid: " + mUid + ", pid: " + mPid + ", packagenames: " + mPackageNames
                    + ", authorities: " + mAuthorities + "}";
        }

        private List<String> getAuthoritiesForPackage(PackageManager packageManager,
                String packageName) {
            try {
                ProviderInfo[] providers = packageManager.getPackageInfo(packageName,
                        PackageManager.GET_PROVIDERS).providers;
                if (providers == null) {
                    return Collections.emptyList();
                }
                return Arrays.stream(providers)
                        .map(provider -> provider.authority)
                        .collect(Collectors.toList());
            } catch (PackageManager.NameNotFoundException e) {
                Log.w(TAG, "Package name not found while retrieving content provider authorities: "
                        + packageName);
                return Collections.emptyList();
            }
        }
    }

    @Override
    @CallSuper
    public IBinder onBind(Intent intent) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "onBind, intent: " + intent);
        }

        Bundle bundle = intent.getBundleExtra(EXTRA_BUNDLE_KEY_FOR_INSTRUMENT_CLUSTER_HELPER);
        IBinder binder = null;
        if (bundle != null) {
            binder = bundle.getBinder(EXTRA_BUNDLE_KEY_FOR_INSTRUMENT_CLUSTER_HELPER);
        }
        if (binder == null) {
            Log.wtf(TAG, "IInstrumentClusterHelper not passed through binder");
        } else {
            synchronized (mLock) {
                mInstrumentClusterHelper = IInstrumentClusterHelper.Stub.asInterface(binder);
            }
        }
        if (mRendererBinder == null) {
            mRendererBinder = new RendererBinder(getNavigationRenderer());
        }

        return mRendererBinder;
    }

    /**
     * Returns {@link NavigationRenderer} or null if it's not supported. This renderer will be
     * shared with the navigation context owner (application holding navigation focus).
     */
    @MainThread
    @Nullable
    public abstract NavigationRenderer getNavigationRenderer();

    /**
     * Called when key event that was addressed to instrument cluster display has been received.
     */
    @MainThread
    public void onKeyEvent(@NonNull KeyEvent keyEvent) {
    }

    /**
     * Called when a navigation application becomes a context owner (receives navigation focus) and
     * its {@link Car#CATEGORY_NAVIGATION} activity is launched.
     */
    @MainThread
    public void onNavigationComponentLaunched() {
    }

    /**
     * Called when the current context owner (application holding navigation focus) releases the
     * focus and its {@link Car#CAR_CATEGORY_NAVIGATION} activity is ready to be replaced by a
     * system default.
     */
    @MainThread
    public void onNavigationComponentReleased() {
    }

    @Nullable
    private IInstrumentClusterHelper getClusterHelper() {
        synchronized (mLock) {
            if (mInstrumentClusterHelper == null) {
                Log.w("mInstrumentClusterHelper still null, should wait until onBind",
                        new RuntimeException());
            }
            return mInstrumentClusterHelper;
        }
    }

    /**
     * Start Activity in fixed mode.
     *
     * <p>Activity launched in this way will stay visible across crash, package updatge
     * or other Activity launch. So this should be carefully used for case like apps running
     * in instrument cluster.</p>
     *
     * <p> Only one Activity can stay in this mode for a display and launching other Activity
     * with this call means old one get out of the mode. Alternatively
     * {@link #stopFixedActivityMode(int)} can be called to get the top activitgy out of this
     * mode.</p>
     *
     * @param intent Should include specific {@code ComponentName}.
     * @param options Should include target display.
     * @param userId Target user id
     * @return {@code true} if succeeded. {@code false} may mean the target component is not ready
     *         or available. Note that failure can happen during early boot-up stage even if the
     *         target Activity is in normal state and client should retry when it fails. Once it is
     *         successfully launched, car service will guarantee that it is running across crash or
     *         other events.
     */
    public boolean startFixedActivityModeForDisplayAndUser(@NonNull Intent intent,
            @NonNull ActivityOptions options, @UserIdInt int userId) {
        IInstrumentClusterHelper helper = getClusterHelper();
        if (helper == null) {
            return false;
        }
        if (mActivityState != null
                && intent.getBundleExtra(Car.CAR_EXTRA_CLUSTER_ACTIVITY_STATE) == null) {
            intent = new Intent(intent).putExtra(Car.CAR_EXTRA_CLUSTER_ACTIVITY_STATE,
                    mActivityState.toBundle());
        }
        try {
            return helper.startFixedActivityModeForDisplayAndUser(intent, options.toBundle(),
                    userId);
        } catch (RemoteException e) {
            Log.w("Remote exception from car service", e);
            // Probably car service will restart and rebind. So do nothing.
        }
        return false;
    }


    /**
     * Stop fixed mode for top Activity in the display. Crashing or launching other Activity
     * will not re-launch the top Activity any more.
     */
    public void stopFixedActivityMode(int displayId) {
        IInstrumentClusterHelper helper = getClusterHelper();
        if (helper == null) {
            return;
        }
        try {
            helper.stopFixedActivityMode(displayId);
        } catch (RemoteException e) {
            Log.w("Remote exception from car service, displayId:" + displayId, e);
            // Probably car service will restart and rebind. So do nothing.
        }
    }

    /**
     * Updates the cluster navigation activity by checking which activity to show (an activity of
     * the {@link #mNavContextOwner}). If not yet launched, it will do so.
     */
    private void updateNavigationActivity() {
        ContextOwner contextOwner = getNavigationContextOwner();

        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, String.format("updateNavigationActivity (mActivityOptions: %s, "
                            + "mActivityState: %s, mNavContextOwnerUid: %s)", mActivityOptions,
                    mActivityState, contextOwner));
        }

        if (contextOwner == null || contextOwner.mUid == 0 || mActivityOptions == null
                || mActivityState == null || !mActivityState.isVisible()) {
            // We are not yet ready to display an activity on the cluster
            if (mNavigationComponent != null) {
                mNavigationComponent = null;
                onNavigationComponentReleased();
            }
            return;
        }

        ComponentName component = getNavigationComponentByOwner(contextOwner);
        if (Objects.equals(mNavigationComponent, component)) {
            // We have already launched this component.
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Already launched component: " + component);
            }
            return;
        }

        if (component == null) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "No component found for owner: " + contextOwner);
            }
            return;
        }

        if (!startNavigationActivity(component)) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Unable to launch component: " + component);
            }
            return;
        }

        mNavigationComponent = component;
        onNavigationComponentLaunched();
    }

    /**
     * Returns a component with category {@link Car#CAR_CATEGORY_NAVIGATION} from the same package
     * as the given navigation context owner.
     */
    @Nullable
    private ComponentName getNavigationComponentByOwner(ContextOwner contextOwner) {
        for (String packageName : contextOwner.mPackageNames) {
            ComponentName component = getComponentFromPackage(packageName);
            if (component != null) {
                if (Log.isLoggable(TAG, Log.DEBUG)) {
                    Log.d(TAG, "Found component: " + component);
                }
                return component;
            }
        }
        return null;
    }

    private ContextOwner getNavigationContextOwner() {
        synchronized (mLock) {
            return mNavContextOwner;
        }
    }

    @Nullable
    private ComponentName getComponentFromPackage(@NonNull String packageName) {
        PackageManager packageManager = getPackageManager();

        // Check package permission.
        if (packageManager.checkPermission(Car.PERMISSION_CAR_DISPLAY_IN_CLUSTER, packageName)
                != PERMISSION_GRANTED) {
            Log.i(TAG, String.format("Package '%s' doesn't have permission %s", packageName,
                    Car.PERMISSION_CAR_DISPLAY_IN_CLUSTER));
            return null;
        }

        Intent intent = new Intent(Intent.ACTION_MAIN)
                .addCategory(Car.CAR_CATEGORY_NAVIGATION)
                .setPackage(packageName);
        List<ResolveInfo> resolveList = packageManager.queryIntentActivities(intent,
                PackageManager.GET_RESOLVED_FILTER);
        if (resolveList == null || resolveList.isEmpty()
                || resolveList.get(0).getComponentInfo() == null) {
            Log.i(TAG, "Failed to resolve an intent: " + intent);
            return null;
        }

        // In case of multiple matching activities in the same package, we pick the first one.
        return resolveList.get(0).getComponentInfo().getComponentName();
    }

    /**
     * Starts an activity on the cluster using the given component.
     *
     * @return false if the activity couldn't be started.
     */
    protected boolean startNavigationActivity(@NonNull ComponentName component) {
        // Create an explicit intent.
        Intent intent = new Intent();
        intent.setComponent(component);
        intent.putExtra(Car.CAR_EXTRA_CLUSTER_ACTIVITY_STATE, mActivityState.toBundle());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            startActivityAsUser(intent, mActivityOptions.toBundle(), UserHandle.CURRENT);
            Log.i(TAG, String.format("Activity launched: %s (options: %s, displayId: %d)",
                    mActivityOptions, intent, mActivityOptions.getLaunchDisplayId()));
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "Unable to find activity for intent: " + intent);
            return false;
        } catch (RuntimeException e) {
            // Catch all other possible exception to prevent service disruption by misbehaving
            // applications.
            Log.e(TAG, "Error trying to launch intent: " + intent + ". Ignored", e);
            return false;
        }
        return true;
    }

    /**
     * @hide
     * @deprecated Use {@link #setClusterActivityLaunchOptions(ActivityOptions)} instead.
     */
    @Deprecated
    public void setClusterActivityLaunchOptions(String category, ActivityOptions activityOptions) {
        setClusterActivityLaunchOptions(activityOptions);
    }

    /**
     * Sets configuration for activities that should be launched directly in the instrument
     * cluster.
     *
     * @param activityOptions contains information of how to start cluster activity (on what display
     *                        or activity stack).
     * @hide
     */
    public void setClusterActivityLaunchOptions(ActivityOptions activityOptions) {
        mActivityOptions = activityOptions;
        updateNavigationActivity();
    }

    /**
     * @hide
     * @deprecated Use {@link #setClusterActivityState(ClusterActivityState)} instead.
     */
    @Deprecated
    public void setClusterActivityState(String category, Bundle state) {
        setClusterActivityState(ClusterActivityState.fromBundle(state));
    }

    /**
     * Set activity state (such as unobscured bounds).
     *
     * @param state pass information about activity state, see
     *              {@link android.car.cluster.ClusterActivityState}
     * @hide
     */
    public void setClusterActivityState(ClusterActivityState state) {
        mActivityState = state;
        updateNavigationActivity();
    }

    @CallSuper
    @Override
    protected void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        synchronized (mLock) {
            writer.println("**" + getClass().getSimpleName() + "**");
            writer.println("renderer binder: " + mRendererBinder);
            if (mRendererBinder != null) {
                writer.println("navigation renderer: " + mRendererBinder.mNavigationRenderer);
            }
            writer.println("navigation focus owner: " + getNavigationContextOwner());
            writer.println("activity options: " + mActivityOptions);
            writer.println("activity state: " + mActivityState);
            writer.println("current nav component: " + mNavigationComponent);
            writer.println("current nav packages: " + getNavigationContextOwner().mPackageNames);
            writer.println("mInstrumentClusterHelper" + mInstrumentClusterHelper);
        }
    }

    private class RendererBinder extends IInstrumentCluster.Stub {
        private final NavigationRenderer mNavigationRenderer;

        RendererBinder(NavigationRenderer navigationRenderer) {
            mNavigationRenderer = navigationRenderer;
        }

        @Override
        public IInstrumentClusterNavigation getNavigationService() throws RemoteException {
            return new NavigationBinder(mNavigationRenderer);
        }

        @Override
        public void setNavigationContextOwner(int uid, int pid) throws RemoteException {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Updating navigation ownership to uid: " + uid + ", pid: " + pid);
            }
            synchronized (mLock) {
                mNavContextOwner = new ContextOwner(uid, pid, getPackageManager());
            }
            mUiHandler.post(InstrumentClusterRenderingService.this::updateNavigationActivity);
        }

        @Override
        public void onKeyEvent(KeyEvent keyEvent) throws RemoteException {
            mUiHandler.post(() -> InstrumentClusterRenderingService.this.onKeyEvent(keyEvent));
        }
    }

    private class NavigationBinder extends IInstrumentClusterNavigation.Stub {
        private final NavigationRenderer mNavigationRenderer;

        NavigationBinder(NavigationRenderer navigationRenderer) {
            mNavigationRenderer = navigationRenderer;
        }

        @Override
        @SuppressWarnings("deprecation")
        public void onNavigationStateChanged(@Nullable Bundle bundle) throws RemoteException {
            assertClusterManagerPermission();
            mUiHandler.post(() -> {
                if (mNavigationRenderer != null) {
                    mNavigationRenderer.onNavigationStateChanged(bundle);
                }
            });
        }

        @Override
        public CarNavigationInstrumentCluster getInstrumentClusterInfo() throws RemoteException {
            assertClusterManagerPermission();
            return runAndWaitResult(() -> mNavigationRenderer.getNavigationProperties());
        }
    }

    private void assertClusterManagerPermission() {
        if (checkCallingOrSelfPermission(Car.PERMISSION_CAR_NAVIGATION_MANAGER)
                != PackageManager.PERMISSION_GRANTED) {
            throw new SecurityException("requires " + Car.PERMISSION_CAR_NAVIGATION_MANAGER);
        }
    }

    private <E> E runAndWaitResult(final Supplier<E> supplier) {
        final CountDownLatch latch = new CountDownLatch(1);
        final AtomicReference<E> result = new AtomicReference<>();

        mUiHandler.post(() -> {
            result.set(supplier.get());
            latch.countDown();
        });

        try {
            latch.await();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
        return result.get();
    }

    /**
     * Fetches a bitmap from the navigation context owner (application holding navigation focus).
     * It returns null if:
     * <ul>
     * <li>there is no navigation context owner
     * <li>or if the {@link Uri} is invalid
     * <li>or if it references a process other than the current navigation context owner
     * </ul>
     * This is a costly operation. Returned bitmaps should be cached and fetching should be done on
     * a secondary thread.
     *
     * @param uri The URI of the bitmap
     *
     * @throws IllegalArgumentException if {@code uri} does not have width and height query params.
     *
     * @deprecated Replaced by {@link #getBitmap(Uri, int, int)}.
     */
    @Deprecated
    @Nullable
    public Bitmap getBitmap(Uri uri) {
        try {
            if (uri.getQueryParameter(BITMAP_QUERY_WIDTH).isEmpty() || uri.getQueryParameter(
                    BITMAP_QUERY_HEIGHT).isEmpty()) {
                throw new IllegalArgumentException(
                    "Uri must have '" + BITMAP_QUERY_WIDTH + "' and '" + BITMAP_QUERY_HEIGHT
                            + "' query parameters");
            }

            ContextOwner contextOwner = getNavigationContextOwner();
            if (contextOwner == null) {
                Log.e(TAG, "No context owner available while fetching: " + uri);
                return null;
            }

            String host = uri.getHost();
            if (!contextOwner.mAuthorities.contains(host)) {
                Log.e(TAG, "Uri points to an authority not handled by the current context owner: "
                        + uri + " (valid authorities: " + contextOwner.mAuthorities + ")");
                return null;
            }

            // Add user to URI to make the request to the right instance of content provider
            // (see ContentProvider#getUserIdFromAuthority()).
            int userId = UserHandle.getUserId(contextOwner.mUid);
            Uri filteredUid = uri.buildUpon().encodedAuthority(userId + "@" + host).build();

            // Fetch the bitmap
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Requesting bitmap: " + uri);
            }
            try (ParcelFileDescriptor fileDesc = getContentResolver()
                    .openFileDescriptor(filteredUid, "r")) {
                if (fileDesc != null) {
                    Bitmap bitmap = BitmapFactory.decodeFileDescriptor(
                            fileDesc.getFileDescriptor());
                    return bitmap;
                } else {
                    Log.e(TAG, "Failed to create pipe for uri string: " + uri);
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "Unable to fetch uri: " + uri, e);
        }
        return null;
    }

    /**
     * See {@link #getBitmap(Uri, int, int, float)}
     */
    @Nullable
    public Bitmap getBitmap(@NonNull Uri uri, int width, int height) {
        return getBitmap(uri, width, height, 1f);
    }

    /**
     * Fetches a bitmap from the navigation context owner (application holding navigation focus)
     * of the given width and height and off lane opacity. The fetched bitmaps are cached.
     * It returns null if:
     * <ul>
     * <li>there is no navigation context owner
     * <li>or if the {@link Uri} is invalid
     * <li>or if it references a process other than the current navigation context owner
     * </ul>
     * This is a costly operation. Returned bitmaps should be fetched on a secondary thread.
     *
     * @param uri           The URI of the bitmap
     * @param width         Requested width
     * @param height        Requested height
     * @param offLanesAlpha Opacity value of the off-lane images. Only used for lane guidance images
     * @throws IllegalArgumentException if width, height <= 0, or 0 > offLanesAlpha > 1
     */
    @Nullable
    public Bitmap getBitmap(@NonNull Uri uri, int width, int height, float offLanesAlpha) {
        if (width <= 0 || height <= 0) {
            throw new IllegalArgumentException("Width and height must be > 0");
        }
        if (offLanesAlpha < 0 || offLanesAlpha > 1) {
            throw new IllegalArgumentException("offLanesAlpha must be between [0, 1]");
        }

        try {
            ContextOwner contextOwner = getNavigationContextOwner();
            if (contextOwner == null) {
                Log.e(TAG, "No context owner available while fetching: " + uri);
                return null;
            }

            uri = uri.buildUpon()
                    .appendQueryParameter(BITMAP_QUERY_WIDTH, String.valueOf(width))
                    .appendQueryParameter(BITMAP_QUERY_HEIGHT, String.valueOf(height))
                    .appendQueryParameter(BITMAP_QUERY_OFFLANESALPHA, String.valueOf(offLanesAlpha))
                    .build();

            String host = uri.getHost();

            if (!contextOwner.mAuthorities.contains(host)) {
                Log.e(TAG, "Uri points to an authority not handled by the current context owner: "
                        + uri + " (valid authorities: " + contextOwner.mAuthorities + ")");
                return null;
            }

            // Add user to URI to make the request to the right instance of content provider
            // (see ContentProvider#getUserIdFromAuthority()).
            int userId = UserHandle.getUserId(contextOwner.mUid);
            Uri filteredUid = uri.buildUpon().encodedAuthority(userId + "@" + host).build();

            Bitmap bitmap = mCache.get(uri.toString());
            if (bitmap == null) {
                // Fetch the bitmap
                if (Log.isLoggable(TAG, Log.DEBUG)) {
                    Log.d(TAG, "Requesting bitmap: " + uri);
                }
                try (ParcelFileDescriptor fileDesc = getContentResolver()
                        .openFileDescriptor(filteredUid, "r")) {
                    if (fileDesc != null) {
                        bitmap = BitmapFactory.decodeFileDescriptor(fileDesc.getFileDescriptor());
                        return bitmap;
                    } else {
                        Log.e(TAG, "Failed to create pipe for uri string: " + uri);
                    }
                }
                if (bitmap.getWidth() != width || bitmap.getHeight() != height) {
                    bitmap = Bitmap.createScaledBitmap(bitmap, width, height, true);
                }
                mCache.put(uri.toString(), bitmap);
            }
            return bitmap;
        } catch (IOException e) {
            Log.e(TAG, "Unable to fetch uri: " + uri, e);
        }
        return null;
    }
}
