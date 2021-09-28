/*
 * Copyright (C) 2006 The Android Open Source Project
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

package android.view;

import static android.view.WindowManagerPolicyConstants.APPLICATION_MEDIA_OVERLAY_SUBLAYER;
import static android.view.WindowManagerPolicyConstants.APPLICATION_MEDIA_SUBLAYER;
import static android.view.WindowManagerPolicyConstants.APPLICATION_PANEL_SUBLAYER;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.compat.annotation.UnsupportedAppUsage;
import android.content.Context;
import android.content.res.CompatibilityInfo.Translator;
import android.graphics.BlendMode;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.Region;
import android.graphics.RenderNode;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceControl.Transaction;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.IAccessibilityEmbeddedConnection;

import com.android.internal.view.SurfaceCallbackHelper;

import java.util.ArrayList;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Provides a dedicated drawing surface embedded inside of a view hierarchy.
 * You can control the format of this surface and, if you like, its size; the
 * SurfaceView takes care of placing the surface at the correct location on the
 * screen
 *
 * <p>The surface is Z ordered so that it is behind the window holding its
 * SurfaceView; the SurfaceView punches a hole in its window to allow its
 * surface to be displayed. The view hierarchy will take care of correctly
 * compositing with the Surface any siblings of the SurfaceView that would
 * normally appear on top of it. This can be used to place overlays such as
 * buttons on top of the Surface, though note however that it can have an
 * impact on performance since a full alpha-blended composite will be performed
 * each time the Surface changes.
 *
 * <p> The transparent region that makes the surface visible is based on the
 * layout positions in the view hierarchy. If the post-layout transform
 * properties are used to draw a sibling view on top of the SurfaceView, the
 * view may not be properly composited with the surface.
 *
 * <p>Access to the underlying surface is provided via the SurfaceHolder interface,
 * which can be retrieved by calling {@link #getHolder}.
 *
 * <p>The Surface will be created for you while the SurfaceView's window is
 * visible; you should implement {@link SurfaceHolder.Callback#surfaceCreated}
 * and {@link SurfaceHolder.Callback#surfaceDestroyed} to discover when the
 * Surface is created and destroyed as the window is shown and hidden.
 *
 * <p>One of the purposes of this class is to provide a surface in which a
 * secondary thread can render into the screen. If you are going to use it
 * this way, you need to be aware of some threading semantics:
 *
 * <ul>
 * <li> All SurfaceView and
 * {@link SurfaceHolder.Callback SurfaceHolder.Callback} methods will be called
 * from the thread running the SurfaceView's window (typically the main thread
 * of the application). They thus need to correctly synchronize with any
 * state that is also touched by the drawing thread.
 * <li> You must ensure that the drawing thread only touches the underlying
 * Surface while it is valid -- between
 * {@link SurfaceHolder.Callback#surfaceCreated SurfaceHolder.Callback.surfaceCreated()}
 * and
 * {@link SurfaceHolder.Callback#surfaceDestroyed SurfaceHolder.Callback.surfaceDestroyed()}.
 * </ul>
 *
 * <p class="note"><strong>Note:</strong> Starting in platform version
 * {@link android.os.Build.VERSION_CODES#N}, SurfaceView's window position is
 * updated synchronously with other View rendering. This means that translating
 * and scaling a SurfaceView on screen will not cause rendering artifacts. Such
 * artifacts may occur on previous versions of the platform when its window is
 * positioned asynchronously.</p>
 */
public class SurfaceView extends View implements ViewRootImpl.SurfaceChangedCallback {
    private static final String TAG = "SurfaceView";
    private static final boolean DEBUG = false;
    private static final boolean DEBUG_POSITION = false;

    @UnsupportedAppUsage
    final ArrayList<SurfaceHolder.Callback> mCallbacks = new ArrayList<>();

    final int[] mLocation = new int[2];

    @UnsupportedAppUsage
    final ReentrantLock mSurfaceLock = new ReentrantLock();
    @UnsupportedAppUsage
    final Surface mSurface = new Surface();       // Current surface in use
    @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.P, trackingBug = 115609023)
    boolean mDrawingStopped = true;
    // We use this to track if the application has produced a frame
    // in to the Surface. Up until that point, we should be careful not to punch
    // holes.
    boolean mDrawFinished = false;

    final Rect mScreenRect = new Rect();
    private final SurfaceSession mSurfaceSession = new SurfaceSession();

    SurfaceControl mSurfaceControl;
    // In the case of format changes we switch out the surface in-place
    // we need to preserve the old one until the new one has drawn.
    SurfaceControl mDeferredDestroySurfaceControl;
    SurfaceControl mBackgroundControl;
    private boolean mDisableBackgroundLayer = false;

    /**
     * We use this lock in SOME cases when reading or writing SurfaceControl,
     * but use the following model so that the RenderThread can run locklessly
     * in the position up-date case.
     *
     * 1. UI Thread can read from mSurfaceControl (use in Transactions) without
     * holding the lock.
     * 2. UI Thread will hold the lock when writing to mSurfaceControl (calling release
     * or remove).
     * 3. Render thread will also hold the lock when writing to mSurfaceControl (e.g.
     * calling release from positionLost).
     * 3. RenderNode.PositionUpdateListener::positionChanged will only be called
     * when the UI thread is paused (blocked on the Render thread).
     * 4. positionChanged thus will not be required to hold the lock as the
     * UI thread is blocked, and the other writer is the RT itself.
     */
    final Object mSurfaceControlLock = new Object();
    final Rect mTmpRect = new Rect();

    Paint mRoundedViewportPaint;

    int mSubLayer = APPLICATION_MEDIA_SUBLAYER;

    @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.P, trackingBug = 115609023)
    boolean mIsCreating = false;
    private volatile boolean mRtHandlingPositionUpdates = false;
    private volatile boolean mRtReleaseSurfaces = false;

    private final ViewTreeObserver.OnScrollChangedListener mScrollChangedListener =
            this::updateSurface;

    @UnsupportedAppUsage
    private final ViewTreeObserver.OnPreDrawListener mDrawListener = () -> {
        // reposition ourselves where the surface is
        mHaveFrame = getWidth() > 0 && getHeight() > 0;
        updateSurface();
        return true;
    };

    boolean mRequestedVisible = false;
    boolean mWindowVisibility = false;
    boolean mLastWindowVisibility = false;
    boolean mViewVisibility = false;
    boolean mWindowStopped = false;

    @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.P, trackingBug = 115609023)
    int mRequestedWidth = -1;
    @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.P, trackingBug = 115609023)
    int mRequestedHeight = -1;
    /* Set SurfaceView's format to 565 by default to maintain backward
     * compatibility with applications assuming this format.
     */
    @UnsupportedAppUsage
    int mRequestedFormat = PixelFormat.RGB_565;

    boolean mUseAlpha = false;
    float mSurfaceAlpha = 1f;
    boolean mClipSurfaceToBounds;
    int mBackgroundColor = Color.BLACK;

    @UnsupportedAppUsage
    boolean mHaveFrame = false;
    boolean mSurfaceCreated = false;
    @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.P, trackingBug = 115609023)
    long mLastLockTime = 0;

    boolean mVisible = false;
    int mWindowSpaceLeft = -1;
    int mWindowSpaceTop = -1;
    int mSurfaceWidth = -1;
    int mSurfaceHeight = -1;
    float mCornerRadius;
    @UnsupportedAppUsage
    int mFormat = -1;
    @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.P, trackingBug = 115609023)
    final Rect mSurfaceFrame = new Rect();
    int mLastSurfaceWidth = -1, mLastSurfaceHeight = -1;

    private boolean mGlobalListenersAdded;
    private boolean mAttachedToWindow;

    private int mSurfaceFlags = SurfaceControl.HIDDEN;

    private int mPendingReportDraws;

    private SurfaceControl.Transaction mRtTransaction = new SurfaceControl.Transaction();
    private SurfaceControl.Transaction mTmpTransaction = new SurfaceControl.Transaction();
    private int mParentSurfaceGenerationId;

    private RemoteAccessibilityEmbeddedConnection mRemoteAccessibilityEmbeddedConnection;

    private final Matrix mScreenMatrixForEmbeddedHierarchy = new Matrix();
    private final Matrix mTmpMatrix = new Matrix();
    private final float[] mMatrixValues = new float[9];

    SurfaceControlViewHost.SurfacePackage mSurfacePackage;

    public SurfaceView(Context context) {
        this(context, null);
    }

    public SurfaceView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public SurfaceView(Context context, AttributeSet attrs, int defStyleAttr) {
        this(context, attrs, defStyleAttr, 0);
    }

    public SurfaceView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        this(context, attrs, defStyleAttr, defStyleRes, false);
    }

    /** @hide */
    public SurfaceView(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr,
            int defStyleRes, boolean disableBackgroundLayer) {
        super(context, attrs, defStyleAttr, defStyleRes);
        mRenderNode.addPositionUpdateListener(mPositionListener);

        setWillNotDraw(true);
        mDisableBackgroundLayer = disableBackgroundLayer;
    }

    /**
     * Return the SurfaceHolder providing access and control over this
     * SurfaceView's underlying surface.
     *
     * @return SurfaceHolder The holder of the surface.
     */
    public SurfaceHolder getHolder() {
        return mSurfaceHolder;
    }

    private void updateRequestedVisibility() {
        mRequestedVisible = mViewVisibility && mWindowVisibility && !mWindowStopped;
    }

    private void setWindowStopped(boolean stopped) {
        mWindowStopped = stopped;
        updateRequestedVisibility();
        updateSurface();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        getViewRootImpl().addSurfaceChangedCallback(this);
        mWindowStopped = false;

        mViewVisibility = getVisibility() == VISIBLE;
        updateRequestedVisibility();

        mAttachedToWindow = true;
        mParent.requestTransparentRegion(SurfaceView.this);
        if (!mGlobalListenersAdded) {
            ViewTreeObserver observer = getViewTreeObserver();
            observer.addOnScrollChangedListener(mScrollChangedListener);
            observer.addOnPreDrawListener(mDrawListener);
            mGlobalListenersAdded = true;
        }
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        mWindowVisibility = visibility == VISIBLE;
        updateRequestedVisibility();
        updateSurface();
    }

    @Override
    public void setVisibility(int visibility) {
        super.setVisibility(visibility);
        mViewVisibility = visibility == VISIBLE;
        boolean newRequestedVisible = mWindowVisibility && mViewVisibility && !mWindowStopped;
        if (newRequestedVisible != mRequestedVisible) {
            // our base class (View) invalidates the layout only when
            // we go from/to the GONE state. However, SurfaceView needs
            // to request a re-layout when the visibility changes at all.
            // This is needed because the transparent region is computed
            // as part of the layout phase, and it changes (obviously) when
            // the visibility changes.
            requestLayout();
        }
        mRequestedVisible = newRequestedVisible;
        updateSurface();
    }

    /**
     * Make alpha value of this view reflect onto the surface. This can only be called from at most
     * one SurfaceView within a view tree.
     *
     * <p class="note"><strong>Note:</strong> Alpha value of the view is ignored and the underlying
     * surface is rendered opaque by default.</p>
     *
     * @hide
     */
    public void setUseAlpha() {
        if (!mUseAlpha) {
            mUseAlpha = true;
            updateSurfaceAlpha();
        }
    }

    @Override
    public void setAlpha(float alpha) {
        // Sets the opacity of the view to a value, where 0 means the view is completely transparent
        // and 1 means the view is completely opaque.
        //
        // Note: Alpha value of this view is ignored by default. To enable alpha blending, you need
        // to call setUseAlpha() as well.
        // This view doesn't support translucent opacity if the view is located z-below, since the
        // logic to punch a hole in the view hierarchy cannot handle such case. See also
        // #clearSurfaceViewPort(Canvas)
        if (DEBUG) {
            Log.d(TAG, System.identityHashCode(this)
                    + " setAlpha: mUseAlpha = " + mUseAlpha + " alpha=" + alpha);
        }
        super.setAlpha(alpha);
        updateSurfaceAlpha();
    }

    private float getFixedAlpha() {
        // Compute alpha value to be set on the underlying surface.
        final float alpha = getAlpha();
        return mUseAlpha && (mSubLayer > 0 || alpha == 0f) ? alpha : 1f;
    }

    private void updateSurfaceAlpha() {
        if (!mUseAlpha) {
            if (DEBUG) {
                Log.d(TAG, System.identityHashCode(this)
                        + " updateSurfaceAlpha: setUseAlpha() is not called, ignored.");
            }
            return;
        }
        final float viewAlpha = getAlpha();
        if (mSubLayer < 0 && 0f < viewAlpha && viewAlpha < 1f) {
            Log.w(TAG, System.identityHashCode(this)
                    + " updateSurfaceAlpha:"
                    + " translucent color is not supported for a surface placed z-below.");
        }
        if (!mHaveFrame) {
            if (DEBUG) {
                Log.d(TAG, System.identityHashCode(this)
                        + " updateSurfaceAlpha: has no surface.");
            }
            return;
        }
        final ViewRootImpl viewRoot = getViewRootImpl();
        if (viewRoot == null) {
            if (DEBUG) {
                Log.d(TAG, System.identityHashCode(this)
                        + " updateSurfaceAlpha: ViewRootImpl not available.");
            }
            return;
        }
        if (mSurfaceControl == null) {
            if (DEBUG) {
                Log.d(TAG, System.identityHashCode(this)
                        + "updateSurfaceAlpha:"
                        + " surface is not yet created, or already released.");
            }
            return;
        }
        final Surface parent = viewRoot.mSurface;
        if (parent == null || !parent.isValid()) {
            if (DEBUG) {
                Log.d(TAG, System.identityHashCode(this)
                        + " updateSurfaceAlpha: ViewRootImpl has no valid surface");
            }
            return;
        }
        final float alpha = getFixedAlpha();
        if (alpha != mSurfaceAlpha) {
            if (isHardwareAccelerated()) {
                /*
                 * Schedule a callback that reflects an alpha value onto the underlying surfaces.
                 * This gets called on a RenderThread worker thread, so members accessed here must
                 * be protected by a lock.
                 */
                final boolean useBLAST = viewRoot.useBLAST();
                viewRoot.registerRtFrameCallback(frame -> {
                    try {
                        final SurfaceControl.Transaction t = useBLAST ?
                            viewRoot.getBLASTSyncTransaction() : new SurfaceControl.Transaction();
                        synchronized (mSurfaceControlLock) {
                            if (!parent.isValid()) {
                                if (DEBUG) {
                                    Log.d(TAG, System.identityHashCode(this)
                                            + " updateSurfaceAlpha RT:"
                                            + " ViewRootImpl has no valid surface");
                                }
                                return;
                            }
                            if (mSurfaceControl == null) {
                                if (DEBUG) {
                                    Log.d(TAG, System.identityHashCode(this)
                                            + "updateSurfaceAlpha RT:"
                                            + " mSurfaceControl has already released");
                                }
                                return;
                            }
                            if (DEBUG) {
                                Log.d(TAG, System.identityHashCode(this)
                                        + " updateSurfaceAlpha RT: set alpha=" + alpha);
                            }
                            t.setAlpha(mSurfaceControl, alpha);
                            if (!useBLAST) {
                                t.deferTransactionUntil(mSurfaceControl,
                                        viewRoot.getRenderSurfaceControl(), frame);
                            }
                        }
                        // It's possible that mSurfaceControl is released in the UI thread before
                        // the transaction completes. If that happens, an exception is thrown, which
                        // must be caught immediately.
                        t.apply();
                    } catch (Exception e) {
                        Log.e(TAG, System.identityHashCode(this)
                                + "updateSurfaceAlpha RT: Exception during surface transaction", e);
                    }
                });
                damageInParent();
            } else {
                if (DEBUG) {
                    Log.d(TAG, System.identityHashCode(this)
                            + " updateSurfaceAlpha: set alpha=" + alpha);
                }
                mTmpTransaction.setAlpha(mSurfaceControl, alpha).apply();
            }
            mSurfaceAlpha = alpha;
        }
    }

    private void performDrawFinished() {
        if (mDeferredDestroySurfaceControl != null) {
            synchronized (mSurfaceControlLock) {
                mTmpTransaction.remove(mDeferredDestroySurfaceControl).apply();
                mDeferredDestroySurfaceControl = null;
            }
        }

        if (mPendingReportDraws > 0) {
            mDrawFinished = true;
            if (mAttachedToWindow) {
                mParent.requestTransparentRegion(SurfaceView.this);
                notifyDrawFinished();
                invalidate();
            }
        } else {
            Log.e(TAG, System.identityHashCode(this) + "finished drawing"
                    + " but no pending report draw (extra call"
                    + " to draw completion runnable?)");
        }
    }

    void notifyDrawFinished() {
        ViewRootImpl viewRoot = getViewRootImpl();
        if (viewRoot != null) {
            viewRoot.pendingDrawFinished();
        }
        mPendingReportDraws--;
    }

    @Override
    protected void onDetachedFromWindow() {
        ViewRootImpl viewRoot = getViewRootImpl();
        // It's possible to create a SurfaceView using the default constructor and never
        // attach it to a view hierarchy, this is a common use case when dealing with
        // OpenGL. A developer will probably create a new GLSurfaceView, and let it manage
        // the lifecycle. Instead of attaching it to a view, he/she can just pass
        // the SurfaceHolder forward, most live wallpapers do it.
        if (viewRoot != null) {
            viewRoot.removeSurfaceChangedCallback(this);
        }

        mAttachedToWindow = false;
        if (mGlobalListenersAdded) {
            ViewTreeObserver observer = getViewTreeObserver();
            observer.removeOnScrollChangedListener(mScrollChangedListener);
            observer.removeOnPreDrawListener(mDrawListener);
            mGlobalListenersAdded = false;
        }

        while (mPendingReportDraws > 0) {
            notifyDrawFinished();
        }

        mRequestedVisible = false;

        updateSurface();
        releaseSurfaces();

        // We don't release this as part of releaseSurfaces as
        // that is also called on transient visibility changes. We can't
        // recreate this Surface, so only release it when we are fully
        // detached.
        if (mSurfacePackage != null) {
            mSurfacePackage.release();
            mSurfacePackage = null;
        }

        mHaveFrame = false;
        super.onDetachedFromWindow();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int width = mRequestedWidth >= 0
                ? resolveSizeAndState(mRequestedWidth, widthMeasureSpec, 0)
                : getDefaultSize(0, widthMeasureSpec);
        int height = mRequestedHeight >= 0
                ? resolveSizeAndState(mRequestedHeight, heightMeasureSpec, 0)
                : getDefaultSize(0, heightMeasureSpec);
        setMeasuredDimension(width, height);
    }

    /** @hide */
    @Override
    @UnsupportedAppUsage
    protected boolean setFrame(int left, int top, int right, int bottom) {
        boolean result = super.setFrame(left, top, right, bottom);
        updateSurface();
        return result;
    }

    @Override
    public boolean gatherTransparentRegion(Region region) {
        if (isAboveParent() || !mDrawFinished) {
            return super.gatherTransparentRegion(region);
        }

        boolean opaque = true;
        if ((mPrivateFlags & PFLAG_SKIP_DRAW) == 0) {
            // this view draws, remove it from the transparent region
            opaque = super.gatherTransparentRegion(region);
        } else if (region != null) {
            int w = getWidth();
            int h = getHeight();
            if (w>0 && h>0) {
                getLocationInWindow(mLocation);
                // otherwise, punch a hole in the whole hierarchy
                int l = mLocation[0];
                int t = mLocation[1];
                region.op(l, t, l+w, t+h, Region.Op.UNION);
            }
        }
        if (PixelFormat.formatHasAlpha(mRequestedFormat)) {
            opaque = false;
        }
        return opaque;
    }

    @Override
    public void draw(Canvas canvas) {
        if (mDrawFinished && !isAboveParent()) {
            // draw() is not called when SKIP_DRAW is set
            if ((mPrivateFlags & PFLAG_SKIP_DRAW) == 0) {
                // punch a whole in the view-hierarchy below us
                clearSurfaceViewPort(canvas);
            }
        }
        super.draw(canvas);
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        if (mDrawFinished && !isAboveParent()) {
            // draw() is not called when SKIP_DRAW is set
            if ((mPrivateFlags & PFLAG_SKIP_DRAW) == PFLAG_SKIP_DRAW) {
                // punch a whole in the view-hierarchy below us
                clearSurfaceViewPort(canvas);
            }
        }
        super.dispatchDraw(canvas);
    }

    /**
     * Control whether the surface is clipped to the same bounds as the View. If true, then
     * the bounds set by {@link #setClipBounds(Rect)} are applied to the surface as window-crop.
     *
     * @param enabled whether to enable surface clipping
     * @hide
     */
    public void setEnableSurfaceClipping(boolean enabled) {
        mClipSurfaceToBounds = enabled;
        invalidate();
    }

    @Override
    public void setClipBounds(Rect clipBounds) {
        super.setClipBounds(clipBounds);

        if (!mClipSurfaceToBounds) {
            return;
        }

        // When cornerRadius is non-zero, a draw() is required to update
        // the viewport (rounding the corners of the clipBounds).
        if (mCornerRadius > 0f && !isAboveParent()) {
            invalidate();
        }

        if (mSurfaceControl != null) {
            if (mClipBounds != null) {
                mTmpRect.set(mClipBounds);
            } else {
                mTmpRect.set(0, 0, mSurfaceWidth, mSurfaceHeight);
            }
            SyncRtSurfaceTransactionApplier applier = new SyncRtSurfaceTransactionApplier(this);
            applier.scheduleApply(
                    new SyncRtSurfaceTransactionApplier.SurfaceParams.Builder(mSurfaceControl)
                            .withWindowCrop(mTmpRect)
                            .build());
        }
    }

    private void clearSurfaceViewPort(Canvas canvas) {
        if (mCornerRadius > 0f) {
            canvas.getClipBounds(mTmpRect);
            if (mClipSurfaceToBounds && mClipBounds != null) {
                mTmpRect.intersect(mClipBounds);
            }
            canvas.drawRoundRect(mTmpRect.left, mTmpRect.top, mTmpRect.right, mTmpRect.bottom,
                    mCornerRadius, mCornerRadius, mRoundedViewportPaint);
        } else {
            canvas.drawColor(0, PorterDuff.Mode.CLEAR);
        }
    }

    /**
     * Sets the corner radius for the SurfaceView. This will round both the corners of the
     * underlying surface, as well as the corners of the hole created to expose the surface.
     *
     * @param cornerRadius the new radius of the corners in pixels
     * @hide
     */
    public void setCornerRadius(float cornerRadius) {
        mCornerRadius = cornerRadius;
        if (mCornerRadius > 0f && mRoundedViewportPaint == null) {
            mRoundedViewportPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            mRoundedViewportPaint.setBlendMode(BlendMode.CLEAR);
            mRoundedViewportPaint.setColor(0);
        }
        invalidate();
    }

    /**
     * Returns the corner radius for the SurfaceView.

     * @return the radius of the corners in pixels
     * @hide
     */
    public float getCornerRadius() {
        return mCornerRadius;
    }

    /**
     * Control whether the surface view's surface is placed on top of another
     * regular surface view in the window (but still behind the window itself).
     * This is typically used to place overlays on top of an underlying media
     * surface view.
     *
     * <p>Note that this must be set before the surface view's containing
     * window is attached to the window manager.
     *
     * <p>Calling this overrides any previous call to {@link #setZOrderOnTop}.
     */
    public void setZOrderMediaOverlay(boolean isMediaOverlay) {
        mSubLayer = isMediaOverlay
            ? APPLICATION_MEDIA_OVERLAY_SUBLAYER : APPLICATION_MEDIA_SUBLAYER;
    }

    /**
     * Control whether the surface view's surface is placed on top of its
     * window.  Normally it is placed behind the window, to allow it to
     * (for the most part) appear to composite with the views in the
     * hierarchy.  By setting this, you cause it to be placed above the
     * window.  This means that none of the contents of the window this
     * SurfaceView is in will be visible on top of its surface.
     *
     * <p>Note that this must be set before the surface view's containing
     * window is attached to the window manager. If you target {@link Build.VERSION_CODES#R}
     * the Z ordering can be changed dynamically if the backing surface is
     * created, otherwise it would be applied at surface construction time.
     *
     * <p>Calling this overrides any previous call to {@link #setZOrderMediaOverlay}.
     *
     * @param onTop Whether to show the surface on top of this view's window.
     */
    public void setZOrderOnTop(boolean onTop) {
        // In R and above we allow dynamic layer changes.
        final boolean allowDynamicChange = getContext().getApplicationInfo().targetSdkVersion
                > Build.VERSION_CODES.Q;
        setZOrderedOnTop(onTop, allowDynamicChange);
    }

    /**
     * @return Whether the surface backing this view appears on top of its parent.
     *
     * @hide
     */
    public boolean isZOrderedOnTop() {
        return mSubLayer > 0;
    }

    /**
     * Controls whether the surface view's surface is placed on top of its
     * window. Normally it is placed behind the window, to allow it to
     * (for the most part) appear to composite with the views in the
     * hierarchy. By setting this, you cause it to be placed above the
     * window. This means that none of the contents of the window this
     * SurfaceView is in will be visible on top of its surface.
     *
     * <p>Calling this overrides any previous call to {@link #setZOrderMediaOverlay}.
     *
     * @param onTop Whether to show the surface on top of this view's window.
     * @param allowDynamicChange Whether this can happen after the surface is created.
     * @return Whether the Z ordering changed.
     *
     * @hide
     */
    public boolean setZOrderedOnTop(boolean onTop, boolean allowDynamicChange) {
        final int subLayer;
        if (onTop) {
            subLayer = APPLICATION_PANEL_SUBLAYER;
        } else {
            subLayer = APPLICATION_MEDIA_SUBLAYER;
        }
        if (mSubLayer == subLayer) {
            return false;
        }
        mSubLayer = subLayer;

        if (!allowDynamicChange) {
            return false;
        }
        if (mSurfaceControl == null) {
            return true;
        }
        final ViewRootImpl viewRoot = getViewRootImpl();
        if (viewRoot == null) {
            return true;
        }
        final Surface parent = viewRoot.mSurface;
        if (parent == null || !parent.isValid()) {
            return true;
        }

        /*
         * Schedule a callback that reflects an alpha value onto the underlying surfaces.
         * This gets called on a RenderThread worker thread, so members accessed here must
         * be protected by a lock.
         */
        final boolean useBLAST = viewRoot.useBLAST();
        viewRoot.registerRtFrameCallback(frame -> {
            try {
                final SurfaceControl.Transaction t = useBLAST
                        ? viewRoot.getBLASTSyncTransaction()
                        : new SurfaceControl.Transaction();
                synchronized (mSurfaceControlLock) {
                    if (!parent.isValid() || mSurfaceControl == null) {
                        return;
                    }
                    updateRelativeZ(t);
                    if (!useBLAST) {
                        t.deferTransactionUntil(mSurfaceControl,
                                viewRoot.getRenderSurfaceControl(), frame);
                    }
                }
                // It's possible that mSurfaceControl is released in the UI thread before
                // the transaction completes. If that happens, an exception is thrown, which
                // must be caught immediately.
                t.apply();
             } catch (Exception e) {
                Log.e(TAG, System.identityHashCode(this)
                        + "setZOrderOnTop RT: Exception during surface transaction", e);
            }
        });

        invalidate();

        return true;
    }

    /**
     * Control whether the surface view's content should be treated as secure,
     * preventing it from appearing in screenshots or from being viewed on
     * non-secure displays.
     *
     * <p>Note that this must be set before the surface view's containing
     * window is attached to the window manager.
     *
     * <p>See {@link android.view.Display#FLAG_SECURE} for details.
     *
     * @param isSecure True if the surface view is secure.
     */
    public void setSecure(boolean isSecure) {
        if (isSecure) {
            mSurfaceFlags |= SurfaceControl.SECURE;
        } else {
            mSurfaceFlags &= ~SurfaceControl.SECURE;
        }
    }

    private void updateOpaqueFlag() {
        if (!PixelFormat.formatHasAlpha(mRequestedFormat)) {
            mSurfaceFlags |= SurfaceControl.OPAQUE;
        } else {
            mSurfaceFlags &= ~SurfaceControl.OPAQUE;
        }
    }

    private void updateBackgroundVisibility(Transaction t) {
        if (mBackgroundControl == null) {
            return;
        }
        if ((mSubLayer < 0) && ((mSurfaceFlags & SurfaceControl.OPAQUE) != 0)
                && !mDisableBackgroundLayer) {
            t.show(mBackgroundControl);
        } else {
            t.hide(mBackgroundControl);
        }
    }

    private Transaction updateBackgroundColor(Transaction t) {
        final float[] colorComponents = new float[] { Color.red(mBackgroundColor) / 255.f,
                Color.green(mBackgroundColor) / 255.f, Color.blue(mBackgroundColor) / 255.f };
        t.setColor(mBackgroundControl, colorComponents);
        return t;
    }

    private void releaseSurfaces() {
        mSurfaceAlpha = 1f;

        synchronized (mSurfaceControlLock) {
            mSurface.release();

            if (mRtHandlingPositionUpdates) {
                mRtReleaseSurfaces = true;
                return;
            }

            if (mSurfaceControl != null) {
                mTmpTransaction.remove(mSurfaceControl);
                mSurfaceControl = null;
            }
            if (mBackgroundControl != null) {
                mTmpTransaction.remove(mBackgroundControl);
                mBackgroundControl = null;
            }
            mTmpTransaction.apply();
        }
    }

    /** @hide */
    protected void updateSurface() {
        if (!mHaveFrame) {
            if (DEBUG) {
                Log.d(TAG, System.identityHashCode(this) + " updateSurface: has no frame");
            }
            return;
        }
        final ViewRootImpl viewRoot = getViewRootImpl();

        if (viewRoot == null) {
            return;
        }

        if (viewRoot.mSurface == null || !viewRoot.mSurface.isValid()) {
            notifySurfaceDestroyed();
            releaseSurfaces();
            return;
        }

        final Translator translator = viewRoot.mTranslator;
        if (translator != null) {
            mSurface.setCompatibilityTranslator(translator);
        }

        int myWidth = mRequestedWidth;
        if (myWidth <= 0) myWidth = getWidth();
        int myHeight = mRequestedHeight;
        if (myHeight <= 0) myHeight = getHeight();

        final float alpha = getFixedAlpha();
        final boolean formatChanged = mFormat != mRequestedFormat;
        final boolean visibleChanged = mVisible != mRequestedVisible;
        final boolean alphaChanged = mSurfaceAlpha != alpha;
        final boolean creating = (mSurfaceControl == null || formatChanged || visibleChanged)
                && mRequestedVisible;
        final boolean sizeChanged = mSurfaceWidth != myWidth || mSurfaceHeight != myHeight;
        final boolean windowVisibleChanged = mWindowVisibility != mLastWindowVisibility;
        boolean redrawNeeded = false;
        getLocationInSurface(mLocation);
        final boolean positionChanged = mWindowSpaceLeft != mLocation[0]
            || mWindowSpaceTop != mLocation[1];
        final boolean layoutSizeChanged = getWidth() != mScreenRect.width()
            || getHeight() != mScreenRect.height();


        if (creating || formatChanged || sizeChanged || visibleChanged ||
                (mUseAlpha && alphaChanged) || windowVisibleChanged ||
                positionChanged || layoutSizeChanged) {
            getLocationInWindow(mLocation);

            if (DEBUG) Log.i(TAG, System.identityHashCode(this) + " "
                    + "Changes: creating=" + creating
                    + " format=" + formatChanged + " size=" + sizeChanged
                    + " visible=" + visibleChanged + " alpha=" + alphaChanged
                    + " mUseAlpha=" + mUseAlpha
                    + " visible=" + visibleChanged
                    + " left=" + (mWindowSpaceLeft != mLocation[0])
                    + " top=" + (mWindowSpaceTop != mLocation[1]));

            try {
                final boolean visible = mVisible = mRequestedVisible;
                mWindowSpaceLeft = mLocation[0];
                mWindowSpaceTop = mLocation[1];
                mSurfaceWidth = myWidth;
                mSurfaceHeight = myHeight;
                mFormat = mRequestedFormat;
                mLastWindowVisibility = mWindowVisibility;

                mScreenRect.left = mWindowSpaceLeft;
                mScreenRect.top = mWindowSpaceTop;
                mScreenRect.right = mWindowSpaceLeft + getWidth();
                mScreenRect.bottom = mWindowSpaceTop + getHeight();
                if (translator != null) {
                    translator.translateRectInAppWindowToScreen(mScreenRect);
                }

                final Rect surfaceInsets = viewRoot.mWindowAttributes.surfaceInsets;
                mScreenRect.offset(surfaceInsets.left, surfaceInsets.top);

                if (creating) {
                    mDeferredDestroySurfaceControl = mSurfaceControl;

                    updateOpaqueFlag();
                    // SurfaceView hierarchy
                    // ViewRootImpl surface
                    //   - bounds layer (crops all child surfaces to parent surface insets)
                    //     - SurfaceView surface (drawn relative to ViewRootImpl surface)
                    //     - Background color layer (drawn behind all SurfaceView surfaces)
                    //
                    // The bounds layer is used to crop the surface view so it does not draw into
                    // the parent surface inset region. Since there can be multiple surface views
                    // below or above the parent surface, one option is to create multiple bounds
                    // layer for each z order. The other option, the one implement is to create
                    // a single bounds layer and set z order for each child surface relative to the
                    // parent surface.
                    // When creating the surface view, we parent it to the bounds layer and then
                    // set the relative z order. When the parent surface changes, we have to
                    // make sure to update the relative z via ViewRootImpl.SurfaceChangedCallback.
                    final String name = "SurfaceView - " + viewRoot.getTitle().toString();

                    mSurfaceControl = new SurfaceControl.Builder(mSurfaceSession)
                        .setName(name)
                        .setLocalOwnerView(this)
                        .setOpaque((mSurfaceFlags & SurfaceControl.OPAQUE) != 0)
                        .setBufferSize(mSurfaceWidth, mSurfaceHeight)
                        .setFormat(mFormat)
                        .setParent(viewRoot.getBoundsLayer())
                        .setFlags(mSurfaceFlags)
                        .setCallsite("SurfaceView.updateSurface")
                        .build();
                    mBackgroundControl = new SurfaceControl.Builder(mSurfaceSession)
                        .setName("Background for -" + name)
                        .setLocalOwnerView(this)
                        .setOpaque(true)
                        .setColorLayer()
                        .setParent(mSurfaceControl)
                        .setCallsite("SurfaceView.updateSurface")
                        .build();

                } else if (mSurfaceControl == null) {
                    return;
                }

                boolean realSizeChanged = false;

                mSurfaceLock.lock();
                try {
                    mDrawingStopped = !visible;

                    if (DEBUG) Log.i(TAG, System.identityHashCode(this) + " "
                            + "Cur surface: " + mSurface);

                    // If we are creating the surface control or the parent surface has not
                    // changed, then set relative z. Otherwise allow the parent
                    // SurfaceChangedCallback to update the relative z. This is needed so that
                    // we do not change the relative z before the server is ready to swap the
                    // parent surface.
                    if (creating || (mParentSurfaceGenerationId
                            == viewRoot.mSurface.getGenerationId())) {
                        updateRelativeZ(mTmpTransaction);
                    }
                    mParentSurfaceGenerationId = viewRoot.mSurface.getGenerationId();

                    if (mViewVisibility) {
                        mTmpTransaction.show(mSurfaceControl);
                    } else {
                        mTmpTransaction.hide(mSurfaceControl);
                    }

                    if (mSurfacePackage != null) {
                        reparentSurfacePackage(mTmpTransaction, mSurfacePackage);
                    }

                    updateBackgroundVisibility(mTmpTransaction);
                    updateBackgroundColor(mTmpTransaction);
                    if (mUseAlpha) {
                        mTmpTransaction.setAlpha(mSurfaceControl, alpha);
                        mSurfaceAlpha = alpha;
                    }

                    // While creating the surface, we will set it's initial
                    // geometry. Outside of that though, we should generally
                    // leave it to the RenderThread.
                    //
                    // There is one more case when the buffer size changes we aren't yet
                    // prepared to sync (as even following the transaction applying
                    // we still need to latch a buffer).
                    // b/28866173
                    if (sizeChanged || creating || !mRtHandlingPositionUpdates) {
                        onSetSurfacePositionAndScaleRT(mTmpTransaction, mSurfaceControl,
                                mScreenRect.left, /*positionLeft*/
                                mScreenRect.top /*positionTop*/ ,
                                mScreenRect.width() / (float) mSurfaceWidth /*postScaleX*/,
                                mScreenRect.height() / (float) mSurfaceHeight /*postScaleY*/);

                        // Set a window crop when creating the surface or changing its size to
                        // crop the buffer to the surface size since the buffer producer may
                        // use SCALING_MODE_SCALE and submit a larger size than the surface
                        // size.
                        if (mClipSurfaceToBounds && mClipBounds != null) {
                            mTmpTransaction.setWindowCrop(mSurfaceControl, mClipBounds);
                        } else {
                            mTmpTransaction.setWindowCrop(mSurfaceControl, mSurfaceWidth,
                                    mSurfaceHeight);
                        }
                    } else if ((layoutSizeChanged || positionChanged || visibleChanged) &&
                            viewRoot.useBLAST()) {
                        viewRoot.setUseBLASTSyncTransaction();
                    }
                    mTmpTransaction.setCornerRadius(mSurfaceControl, mCornerRadius);
                    if (sizeChanged && !creating) {
                        mTmpTransaction.setBufferSize(mSurfaceControl, mSurfaceWidth,
                                mSurfaceHeight);
                    }

                    mTmpTransaction.apply();
                    updateScreenMatrixForEmbeddedHierarchy();

                    if (sizeChanged || creating) {
                        redrawNeeded = true;
                    }

                    mSurfaceFrame.left = 0;
                    mSurfaceFrame.top = 0;
                    if (translator == null) {
                        mSurfaceFrame.right = mSurfaceWidth;
                        mSurfaceFrame.bottom = mSurfaceHeight;
                    } else {
                        float appInvertedScale = translator.applicationInvertedScale;
                        mSurfaceFrame.right = (int) (mSurfaceWidth * appInvertedScale + 0.5f);
                        mSurfaceFrame.bottom = (int) (mSurfaceHeight * appInvertedScale + 0.5f);
                    }

                    final int surfaceWidth = mSurfaceFrame.right;
                    final int surfaceHeight = mSurfaceFrame.bottom;
                    realSizeChanged = mLastSurfaceWidth != surfaceWidth
                            || mLastSurfaceHeight != surfaceHeight;
                    mLastSurfaceWidth = surfaceWidth;
                    mLastSurfaceHeight = surfaceHeight;
                } finally {
                    mSurfaceLock.unlock();
                }

                try {
                    redrawNeeded |= visible && !mDrawFinished;

                    SurfaceHolder.Callback[] callbacks = null;

                    final boolean surfaceChanged = creating;
                    if (mSurfaceCreated && (surfaceChanged || (!visible && visibleChanged))) {
                        mSurfaceCreated = false;
                        notifySurfaceDestroyed();
                    }

                    if (creating) {
                        mSurface.copyFrom(mSurfaceControl);
                    }

                    if (sizeChanged && getContext().getApplicationInfo().targetSdkVersion
                            < Build.VERSION_CODES.O) {
                        // Some legacy applications use the underlying native {@link Surface} object
                        // as a key to whether anything has changed. In these cases, updates to the
                        // existing {@link Surface} will be ignored when the size changes.
                        // Therefore, we must explicitly recreate the {@link Surface} in these
                        // cases.
                        mSurface.createFrom(mSurfaceControl);
                    }

                    if (visible && mSurface.isValid()) {
                        if (!mSurfaceCreated && (surfaceChanged || visibleChanged)) {
                            mSurfaceCreated = true;
                            mIsCreating = true;
                            if (DEBUG) Log.i(TAG, System.identityHashCode(this) + " "
                                    + "visibleChanged -- surfaceCreated");
                            if (callbacks == null) {
                                callbacks = getSurfaceCallbacks();
                            }
                            for (SurfaceHolder.Callback c : callbacks) {
                                c.surfaceCreated(mSurfaceHolder);
                            }
                        }
                        if (creating || formatChanged || sizeChanged
                                || visibleChanged || realSizeChanged) {
                            if (DEBUG) Log.i(TAG, System.identityHashCode(this) + " "
                                    + "surfaceChanged -- format=" + mFormat
                                    + " w=" + myWidth + " h=" + myHeight);
                            if (callbacks == null) {
                                callbacks = getSurfaceCallbacks();
                            }
                            for (SurfaceHolder.Callback c : callbacks) {
                                c.surfaceChanged(mSurfaceHolder, mFormat, myWidth, myHeight);
                            }
                        }
                        if (redrawNeeded) {
                            if (DEBUG) Log.i(TAG, System.identityHashCode(this) + " "
                                    + "surfaceRedrawNeeded");
                            if (callbacks == null) {
                                callbacks = getSurfaceCallbacks();
                            }

                            mPendingReportDraws++;
                            viewRoot.drawPending();
                            SurfaceCallbackHelper sch =
                                    new SurfaceCallbackHelper(this::onDrawFinished);
                            sch.dispatchSurfaceRedrawNeededAsync(mSurfaceHolder, callbacks);
                        }
                    }
                } finally {
                    mIsCreating = false;
                    if (mSurfaceControl != null && !mSurfaceCreated) {
                        releaseSurfaces();
                    }
                }
            } catch (Exception ex) {
                Log.e(TAG, "Exception configuring surface", ex);
            }
            if (DEBUG) Log.v(
                TAG, "Layout: x=" + mScreenRect.left + " y=" + mScreenRect.top
                + " w=" + mScreenRect.width() + " h=" + mScreenRect.height()
                + ", frame=" + mSurfaceFrame);
        }
    }

    private void onDrawFinished() {
        if (DEBUG) {
            Log.i(TAG, System.identityHashCode(this) + " "
                    + "finishedDrawing");
        }

        runOnUiThread(this::performDrawFinished);
    }

    /**
     * A place to over-ride for applying child-surface transactions.
     * These can be synchronized with the viewroot surface using deferTransaction.
     *
     * Called from RenderWorker while UI thread is paused.
     * @hide
     */
    protected void applyChildSurfaceTransaction_renderWorker(SurfaceControl.Transaction t,
            Surface viewRootSurface, long nextViewRootFrameNumber) {
    }

    /**
     * Sets the surface position and scale. Can be called on
     * the UI thread as well as on the renderer thread.
     *
     * @param transaction Transaction in which to execute.
     * @param surface Surface whose location to set.
     * @param positionLeft The left position to set.
     * @param positionTop The top position to set.
     * @param postScaleX The X axis post scale
     * @param postScaleY The Y axis post scale
     *
     * @hide
     */
    protected void onSetSurfacePositionAndScaleRT(@NonNull Transaction transaction,
            @NonNull SurfaceControl surface, int positionLeft, int positionTop,
            float postScaleX, float postScaleY) {
        transaction.setPosition(surface, positionLeft, positionTop);
        transaction.setMatrix(surface, postScaleX /*dsdx*/, 0f /*dtdx*/,
                0f /*dtdy*/, postScaleY /*dsdy*/);
    }

    /** @hide */
    public void requestUpdateSurfacePositionAndScale() {
        if (mSurfaceControl == null) {
            return;
        }
        onSetSurfacePositionAndScaleRT(mTmpTransaction, mSurfaceControl,
                mScreenRect.left, /*positionLeft*/
                mScreenRect.top/*positionTop*/ ,
                mScreenRect.width() / (float) mSurfaceWidth /*postScaleX*/,
                mScreenRect.height() / (float) mSurfaceHeight /*postScaleY*/);
        mTmpTransaction.apply();
    }

    private void applySurfaceTransforms(SurfaceControl surface, SurfaceControl.Transaction t,
            Rect position, long frameNumber) {
        final ViewRootImpl viewRoot = getViewRootImpl();
        if (frameNumber > 0 && viewRoot != null && !viewRoot.isDrawingToBLASTTransaction()) {
            t.deferTransactionUntil(surface, viewRoot.getRenderSurfaceControl(),
                    frameNumber);
        }

        onSetSurfacePositionAndScaleRT(t, surface,
                position.left /*positionLeft*/,
                position.top /*positionTop*/,
                position.width() / (float) mSurfaceWidth /*postScaleX*/,
                position.height() / (float) mSurfaceHeight /*postScaleY*/);

        if (mViewVisibility) {
            t.show(surface);
        }
    }

    /**
     * @return The last render position of the backing surface or an empty rect.
     *
     * @hide
     */
    public @NonNull Rect getSurfaceRenderPosition() {
        return mRTLastReportedPosition;
    }

    private void setParentSpaceRectangle(Rect position, long frameNumber) {
        final ViewRootImpl viewRoot = getViewRootImpl();
        final boolean useBLAST = viewRoot.isDrawingToBLASTTransaction();
        final SurfaceControl.Transaction t = useBLAST ? viewRoot.getBLASTSyncTransaction() :
            mRtTransaction;

        applySurfaceTransforms(mSurfaceControl, t, position, frameNumber);

        applyChildSurfaceTransaction_renderWorker(t, viewRoot.mSurface,
                frameNumber);

        if (!useBLAST) {
            t.apply();
        }
    }

    private Rect mRTLastReportedPosition = new Rect();

    private RenderNode.PositionUpdateListener mPositionListener =
            new RenderNode.PositionUpdateListener() {

        @Override
        public void positionChanged(long frameNumber, int left, int top, int right, int bottom) {
            if (mSurfaceControl == null) {
                return;
            }

            // TODO: This is teensy bit racey in that a brand new SurfaceView moving on
            // its 2nd frame if RenderThread is running slowly could potentially see
            // this as false, enter the branch, get pre-empted, then this comes along
            // and reports a new position, then the UI thread resumes and reports
            // its position. This could therefore be de-sync'd in that interval, but
            // the synchronization would violate the rule that RT must never block
            // on the UI thread which would open up potential deadlocks. The risk of
            // a single-frame desync is therefore preferable for now.
            synchronized(mSurfaceControlLock) {
                mRtHandlingPositionUpdates = true;
            }
            if (mRTLastReportedPosition.left == left
                    && mRTLastReportedPosition.top == top
                    && mRTLastReportedPosition.right == right
                    && mRTLastReportedPosition.bottom == bottom) {
                return;
            }
            try {
                if (DEBUG_POSITION) {
                    Log.d(TAG, String.format(
                            "%d updateSurfacePosition RenderWorker, frameNr = %d, "
                                    + "position = [%d, %d, %d, %d]",
                            System.identityHashCode(this), frameNumber,
                            left, top, right, bottom));
                }
                mRTLastReportedPosition.set(left, top, right, bottom);
                setParentSpaceRectangle(mRTLastReportedPosition, frameNumber);
                // Now overwrite mRTLastReportedPosition with our values
            } catch (Exception ex) {
                Log.e(TAG, "Exception from repositionChild", ex);
            }
        }

        @Override
        public void positionLost(long frameNumber) {
            final ViewRootImpl viewRoot = getViewRootImpl();
            boolean useBLAST = viewRoot != null && viewRoot.isDrawingToBLASTTransaction();
            if (DEBUG) {
                Log.d(TAG, String.format("%d windowPositionLost, frameNr = %d",
                        System.identityHashCode(this), frameNumber));
            }
            mRTLastReportedPosition.setEmpty();

            if (mSurfaceControl == null) {
                return;
            }

            final SurfaceControl.Transaction t = useBLAST ?
                (viewRoot != null ? viewRoot.getBLASTSyncTransaction() : mRtTransaction) :
                mRtTransaction;

            /**
             * positionLost can be called while UI thread is un-paused so we
             * need to hold the lock here.
             */
            synchronized (mSurfaceControlLock) {
                if (frameNumber > 0 && viewRoot !=  null && !useBLAST) {
                    if (viewRoot.mSurface.isValid()) {
                        mRtTransaction.deferTransactionUntil(mSurfaceControl,
                                viewRoot.getRenderSurfaceControl(), frameNumber);
                    }
                }
                t.hide(mSurfaceControl);

                if (mRtReleaseSurfaces) {
                    mRtReleaseSurfaces = false;
                    mRtTransaction.remove(mSurfaceControl);
                    mRtTransaction.remove(mBackgroundControl);
                    mSurfaceControl = null;
                    mBackgroundControl = null;
                }
                mRtHandlingPositionUpdates = false;
            }

            // If we aren't using BLAST, we apply the transaction locally, otherise we let the ViewRoot apply it for us.
            // If the ViewRoot is null, we behave as if we aren't using BLAST so we need to apply the transaction.
            if (!useBLAST || viewRoot == null) {
                mRtTransaction.apply();
            }
        }
    };

    private SurfaceHolder.Callback[] getSurfaceCallbacks() {
        SurfaceHolder.Callback[] callbacks;
        synchronized (mCallbacks) {
            callbacks = new SurfaceHolder.Callback[mCallbacks.size()];
            mCallbacks.toArray(callbacks);
        }
        return callbacks;
    }

    private void runOnUiThread(Runnable runnable) {
        Handler handler = getHandler();
        if (handler != null && handler.getLooper() != Looper.myLooper()) {
            handler.post(runnable);
        } else {
            runnable.run();
        }
    }

    /**
     * Check to see if the surface has fixed size dimensions or if the surface's
     * dimensions are dimensions are dependent on its current layout.
     *
     * @return true if the surface has dimensions that are fixed in size
     * @hide
     */
    @UnsupportedAppUsage
    public boolean isFixedSize() {
        return (mRequestedWidth != -1 || mRequestedHeight != -1);
    }

    private boolean isAboveParent() {
        return mSubLayer >= 0;
    }

    /**
     * Set an opaque background color to use with this {@link SurfaceView} when it's being resized
     * and size of the content hasn't updated yet. This color will fill the expanded area when the
     * view becomes larger.
     * @param bgColor An opaque color to fill the background. Alpha component will be ignored.
     * @hide
     */
    public void setResizeBackgroundColor(int bgColor) {
        if (mBackgroundControl == null) {
            return;
        }

        mBackgroundColor = bgColor;
        updateBackgroundColor(mTmpTransaction).apply();
    }

    @UnsupportedAppUsage
    private final SurfaceHolder mSurfaceHolder = new SurfaceHolder() {
        private static final String LOG_TAG = "SurfaceHolder";

        @Override
        public boolean isCreating() {
            return mIsCreating;
        }

        @Override
        public void addCallback(Callback callback) {
            synchronized (mCallbacks) {
                // This is a linear search, but in practice we'll
                // have only a couple callbacks, so it doesn't matter.
                if (!mCallbacks.contains(callback)) {
                    mCallbacks.add(callback);
                }
            }
        }

        @Override
        public void removeCallback(Callback callback) {
            synchronized (mCallbacks) {
                mCallbacks.remove(callback);
            }
        }

        @Override
        public void setFixedSize(int width, int height) {
            if (mRequestedWidth != width || mRequestedHeight != height) {
                mRequestedWidth = width;
                mRequestedHeight = height;
                requestLayout();
            }
        }

        @Override
        public void setSizeFromLayout() {
            if (mRequestedWidth != -1 || mRequestedHeight != -1) {
                mRequestedWidth = mRequestedHeight = -1;
                requestLayout();
            }
        }

        @Override
        public void setFormat(int format) {
            // for backward compatibility reason, OPAQUE always
            // means 565 for SurfaceView
            if (format == PixelFormat.OPAQUE)
                format = PixelFormat.RGB_565;

            mRequestedFormat = format;
            if (mSurfaceControl != null) {
                updateSurface();
            }
        }

        /**
         * @deprecated setType is now ignored.
         */
        @Override
        @Deprecated
        public void setType(int type) { }

        @Override
        public void setKeepScreenOn(boolean screenOn) {
            runOnUiThread(() -> SurfaceView.this.setKeepScreenOn(screenOn));
        }

        /**
         * Gets a {@link Canvas} for drawing into the SurfaceView's Surface
         *
         * After drawing into the provided {@link Canvas}, the caller must
         * invoke {@link #unlockCanvasAndPost} to post the new contents to the surface.
         *
         * The caller must redraw the entire surface.
         * @return A canvas for drawing into the surface.
         */
        @Override
        public Canvas lockCanvas() {
            return internalLockCanvas(null, false);
        }

        /**
         * Gets a {@link Canvas} for drawing into the SurfaceView's Surface
         *
         * After drawing into the provided {@link Canvas}, the caller must
         * invoke {@link #unlockCanvasAndPost} to post the new contents to the surface.
         *
         * @param inOutDirty A rectangle that represents the dirty region that the caller wants
         * to redraw.  This function may choose to expand the dirty rectangle if for example
         * the surface has been resized or if the previous contents of the surface were
         * not available.  The caller must redraw the entire dirty region as represented
         * by the contents of the inOutDirty rectangle upon return from this function.
         * The caller may also pass <code>null</code> instead, in the case where the
         * entire surface should be redrawn.
         * @return A canvas for drawing into the surface.
         */
        @Override
        public Canvas lockCanvas(Rect inOutDirty) {
            return internalLockCanvas(inOutDirty, false);
        }

        @Override
        public Canvas lockHardwareCanvas() {
            return internalLockCanvas(null, true);
        }

        private Canvas internalLockCanvas(Rect dirty, boolean hardware) {
            mSurfaceLock.lock();

            if (DEBUG) Log.i(TAG, System.identityHashCode(this) + " " + "Locking canvas... stopped="
                    + mDrawingStopped + ", surfaceControl=" + mSurfaceControl);

            Canvas c = null;
            if (!mDrawingStopped && mSurfaceControl != null) {
                try {
                    if (hardware) {
                        c = mSurface.lockHardwareCanvas();
                    } else {
                        c = mSurface.lockCanvas(dirty);
                    }
                } catch (Exception e) {
                    Log.e(LOG_TAG, "Exception locking surface", e);
                }
            }

            if (DEBUG) Log.i(TAG, System.identityHashCode(this) + " " + "Returned canvas: " + c);
            if (c != null) {
                mLastLockTime = SystemClock.uptimeMillis();
                return c;
            }

            // If the Surface is not ready to be drawn, then return null,
            // but throttle calls to this function so it isn't called more
            // than every 100ms.
            long now = SystemClock.uptimeMillis();
            long nextTime = mLastLockTime + 100;
            if (nextTime > now) {
                try {
                    Thread.sleep(nextTime-now);
                } catch (InterruptedException e) {
                }
                now = SystemClock.uptimeMillis();
            }
            mLastLockTime = now;
            mSurfaceLock.unlock();

            return null;
        }

        /**
         * Posts the new contents of the {@link Canvas} to the surface and
         * releases the {@link Canvas}.
         *
         * @param canvas The canvas previously obtained from {@link #lockCanvas}.
         */
        @Override
        public void unlockCanvasAndPost(Canvas canvas) {
            mSurface.unlockCanvasAndPost(canvas);
            mSurfaceLock.unlock();
        }

        @Override
        public Surface getSurface() {
            return mSurface;
        }

        @Override
        public Rect getSurfaceFrame() {
            return mSurfaceFrame;
        }
    };

    /**
     * Return a SurfaceControl which can be used for parenting Surfaces to
     * this SurfaceView.
     *
     * @return The SurfaceControl for this SurfaceView.
     */
    public SurfaceControl getSurfaceControl() {
        return mSurfaceControl;
    }

    /**
     * A token used for constructing {@link SurfaceControlViewHost}. This token should
     * be passed from the host process to the client process.
     *
     * @return The token
     */
    public @Nullable IBinder getHostToken() {
        final ViewRootImpl viewRoot = getViewRootImpl();
        if (viewRoot == null) {
            return null;
        }
        return viewRoot.getInputToken();
    }

    /**
     * Set window stopped to false and update surface visibility when ViewRootImpl surface is
     * created.
     * @hide
     */
    @Override
    public void surfaceCreated(SurfaceControl.Transaction t) {
        setWindowStopped(false);
    }

    /**
     * Set window stopped to true and update surface visibility when ViewRootImpl surface is
     * destroyed.
     * @hide
     */
    @Override
    public void surfaceDestroyed() {
        setWindowStopped(true);
        setRemoteAccessibilityEmbeddedConnection(null, null);
    }

    /**
     * Called when a valid ViewRootImpl surface is replaced by another valid surface. In this
     * case update relative z to the new parent surface.
     * @hide
     */
    @Override
    public void surfaceReplaced(Transaction t) {
        if (mSurfaceControl != null && mBackgroundControl != null) {
            updateRelativeZ(t);
        }
    }

    private void updateRelativeZ(Transaction t) {
        final ViewRootImpl viewRoot = getViewRootImpl();
        if (viewRoot == null) {
            // We were just detached.
            return;
        }
        final SurfaceControl viewRootControl = viewRoot.getSurfaceControl();
        t.setRelativeLayer(mBackgroundControl, viewRootControl, Integer.MIN_VALUE);
        t.setRelativeLayer(mSurfaceControl, viewRootControl, mSubLayer);
    }

    /**
     * Display the view-hierarchy embedded within a {@link SurfaceControlViewHost.SurfacePackage}
     * within this SurfaceView.
     *
     * This can be called independently of the SurfaceView lifetime callbacks. SurfaceView
     * will internally manage reparenting the package to our Surface as it is created
     * and destroyed.
     *
     * If this SurfaceView is above its host Surface (see
     * {@link #setZOrderOnTop} then the embedded Surface hierarchy will be able to receive
     * input.
     *
     * This will take ownership of the SurfaceControl contained inside the SurfacePackage
     * and free the caller of the obligation to call
     * {@link SurfaceControlViewHost.SurfacePackage#release}. However, note that
     * {@link SurfaceControlViewHost.SurfacePackage#release} and
     * {@link SurfaceControlViewHost#release} are not the same. While the ownership
     * of this particular {@link SurfaceControlViewHost.SurfacePackage} will be taken by the
     * SurfaceView the underlying {@link SurfaceControlViewHost} remains managed by it's original
     * remote-owner.
     *
     * @param p The SurfacePackage to embed.
     */
    public void setChildSurfacePackage(@NonNull SurfaceControlViewHost.SurfacePackage p) {
        final SurfaceControl sc = p != null ? p.getSurfaceControl() : null;
        final SurfaceControl lastSc = mSurfacePackage != null ?
            mSurfacePackage.getSurfaceControl() : null;
        if (mSurfaceControl != null && lastSc != null) {
            mTmpTransaction.reparent(lastSc, null).apply();
            mSurfacePackage.release();
        } else if (mSurfaceControl != null) {
            reparentSurfacePackage(mTmpTransaction, p);
            mTmpTransaction.apply();
        }
        mSurfacePackage = p;
    }

    private void reparentSurfacePackage(SurfaceControl.Transaction t,
            SurfaceControlViewHost.SurfacePackage p) {
        initEmbeddedHierarchyForAccessibility(p);
        final SurfaceControl sc = p.getSurfaceControl();
        t.reparent(sc, mSurfaceControl).show(sc);
    }

    /** @hide */
    @Override
    public void onInitializeAccessibilityNodeInfoInternal(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfoInternal(info);
        final RemoteAccessibilityEmbeddedConnection wrapper =
                getRemoteAccessibilityEmbeddedConnection();
        if (wrapper == null) {
            return;
        }
        // Add a leashed child when this SurfaceView embeds another view hierarchy. Getting this
        // leashed child would return the root node in the embedded hierarchy
        info.addChild(wrapper.getLeashToken());
    }

    @Override
    public int getImportantForAccessibility() {
        final int mode = super.getImportantForAccessibility();
        // If developers explicitly set the important mode for it, don't change the mode.
        // Only change the mode to important when this SurfaceView isn't explicitly set and has
        // an embedded hierarchy.
        if (mRemoteAccessibilityEmbeddedConnection == null
                || mode != IMPORTANT_FOR_ACCESSIBILITY_AUTO) {
            return mode;
        }
        return IMPORTANT_FOR_ACCESSIBILITY_YES;
    }

    private void initEmbeddedHierarchyForAccessibility(SurfaceControlViewHost.SurfacePackage p) {
        final IAccessibilityEmbeddedConnection connection = p.getAccessibilityEmbeddedConnection();
        final RemoteAccessibilityEmbeddedConnection wrapper =
                getRemoteAccessibilityEmbeddedConnection();

        // Do nothing if package is embedding the same view hierarchy.
        if (wrapper != null && wrapper.getConnection().equals(connection)) {
            return;
        }

        // If this SurfaceView embeds a different view hierarchy, unlink the previous one first.
        setRemoteAccessibilityEmbeddedConnection(null, null);

        try {
            final IBinder leashToken = connection.associateEmbeddedHierarchy(
                    getViewRootImpl().mLeashToken, getAccessibilityViewId());
            setRemoteAccessibilityEmbeddedConnection(connection, leashToken);
        } catch (RemoteException e) {
            Log.d(TAG, "Error while associateEmbeddedHierarchy " + e);
        }
        updateScreenMatrixForEmbeddedHierarchy();
    }

    private void setRemoteAccessibilityEmbeddedConnection(
            IAccessibilityEmbeddedConnection connection, IBinder leashToken) {
        try {
            if (mRemoteAccessibilityEmbeddedConnection != null) {
                mRemoteAccessibilityEmbeddedConnection.getConnection()
                        .disassociateEmbeddedHierarchy();
                mRemoteAccessibilityEmbeddedConnection.unlinkToDeath();
                mRemoteAccessibilityEmbeddedConnection = null;
            }
            if (connection != null && leashToken != null) {
                mRemoteAccessibilityEmbeddedConnection =
                        new RemoteAccessibilityEmbeddedConnection(connection, leashToken);
                mRemoteAccessibilityEmbeddedConnection.linkToDeath();
            }
        } catch (RemoteException e) {
            Log.d(TAG, "Error while setRemoteEmbeddedConnection " + e);
        }
    }

    private RemoteAccessibilityEmbeddedConnection getRemoteAccessibilityEmbeddedConnection() {
        return mRemoteAccessibilityEmbeddedConnection;
    }

    private void updateScreenMatrixForEmbeddedHierarchy() {
        getBoundsOnScreen(mTmpRect);
        mTmpMatrix.reset();
        mTmpMatrix.setTranslate(mTmpRect.left, mTmpRect.top);
        mTmpMatrix.postScale(mScreenRect.width() / (float) mSurfaceWidth,
                mScreenRect.height() / (float) mSurfaceHeight);

        // If the screen matrix is identity or doesn't change, do nothing.
        if (mTmpMatrix.isIdentity() || mTmpMatrix.equals(mScreenMatrixForEmbeddedHierarchy)) {
            return;
        }

        try {
            final RemoteAccessibilityEmbeddedConnection wrapper =
                    getRemoteAccessibilityEmbeddedConnection();
            if (wrapper == null) {
                return;
            }
            mTmpMatrix.getValues(mMatrixValues);
            wrapper.getConnection().setScreenMatrix(mMatrixValues);
            mScreenMatrixForEmbeddedHierarchy.set(mTmpMatrix);
        } catch (RemoteException e) {
            Log.d(TAG, "Error while setScreenMatrix " + e);
        }
    }

    private void notifySurfaceDestroyed() {
        if (mSurface.isValid()) {
            if (DEBUG) Log.i(TAG, System.identityHashCode(this) + " "
                    + "surfaceDestroyed");
            SurfaceHolder.Callback[] callbacks = getSurfaceCallbacks();
            for (SurfaceHolder.Callback c : callbacks) {
                c.surfaceDestroyed(mSurfaceHolder);
            }
            // Since Android N the same surface may be reused and given to us
            // again by the system server at a later point. However
            // as we didn't do this in previous releases, clients weren't
            // necessarily required to clean up properly in
            // surfaceDestroyed. This leads to problems for example when
            // clients don't destroy their EGL context, and try
            // and create a new one on the same surface following reuse.
            // Since there is no valid use of the surface in-between
            // surfaceDestroyed and surfaceCreated, we force a disconnect,
            // so the next connect will always work if we end up reusing
            // the surface.
            if (mSurface.isValid()) {
                mSurface.forceScopedDisconnect();
            }
        }
    }

    /**
     * Wrapper of accessibility embedded connection for embedded view hierarchy.
     */
    private final class RemoteAccessibilityEmbeddedConnection implements IBinder.DeathRecipient {
        private final IAccessibilityEmbeddedConnection mConnection;
        private final IBinder mLeashToken;

        RemoteAccessibilityEmbeddedConnection(IAccessibilityEmbeddedConnection connection,
                IBinder leashToken) {
            mConnection = connection;
            mLeashToken = leashToken;
        }

        IAccessibilityEmbeddedConnection getConnection() {
            return mConnection;
        }

        IBinder getLeashToken() {
            return mLeashToken;
        }

        void linkToDeath() throws RemoteException {
            mConnection.asBinder().linkToDeath(this, 0);
        }

        void unlinkToDeath() {
            mConnection.asBinder().unlinkToDeath(this, 0);
        }

        @Override
        public void binderDied() {
            unlinkToDeath();
            runOnUiThread(() -> {
                if (mRemoteAccessibilityEmbeddedConnection == this) {
                    mRemoteAccessibilityEmbeddedConnection = null;
                }
            });
        }
    }
}
