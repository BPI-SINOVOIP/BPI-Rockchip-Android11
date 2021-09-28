package com.android.customization.widget;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.Resources.Theme;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Matrix;
import android.graphics.Outline;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff.Mode;
import android.graphics.Rect;
import android.graphics.Region;
import android.graphics.Shader;
import android.graphics.Shader.TileMode;
import android.graphics.drawable.AdaptiveIconDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.DisplayMetrics;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;

/**
 * This is basically a copy of {@link AdaptiveIconDrawable} but which allows a custom path for
 * the icon mask instead of using the one defined in the system.
 * Note: Unlike AdaptiveIconDrawable we don't need to deal with densityOverride here so that
 * logic is omitted.
 */
public class DynamicAdaptiveIconDrawable extends Drawable implements Drawable.Callback {

    /**
     * Mask path is defined inside device configuration in following dimension: [100 x 100]
     */
    private static final float MASK_SIZE = 100f;

    /**
     * All four sides of the layers are padded with extra inset so as to provide
     * extra content to reveal within the clip path when performing affine transformations on the
     * layers.
     *
     * Each layers will reserve 25% of it's width and height.
     *
     * As a result, the view port of the layers is smaller than their intrinsic width and height.
     */
    private static final float EXTRA_INSET_PERCENTAGE = 1 / 4f;
    private static final float DEFAULT_VIEW_PORT_SCALE = 1f / (1 + 2 * EXTRA_INSET_PERCENTAGE);

    private final Path mOriginalMask;

    /**
     * Scaled mask based on the view bounds.
     */
    private final Path mMask;
    private final Path mMaskScaleOnly;
    private final Matrix mMaskMatrix;
    private final Region mTransparentRegion;

    /**
     * Indices used to access {@link #mLayerState.mChildren} array for foreground and
     * background layer.
     */
    private static final int BACKGROUND_ID = 0;
    private static final int FOREGROUND_ID = 1;

    /**
     * State variable that maintains the {@link ChildDrawable} array.
     */
    private LayerState mLayerState;

    private Shader mLayersShader;
    private Bitmap mLayersBitmap;

    private final Rect mTmpOutRect = new Rect();
    private Rect mHotspotBounds;
    private boolean mMutated;

    private boolean mSuspendChildInvalidation;
    private boolean mChildRequestedInvalidation;
    private final Canvas mCanvas;
    private Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.DITHER_FLAG |
            Paint.FILTER_BITMAP_FLAG);

    /**
     * Constructor used for xml inflation.
     */
    DynamicAdaptiveIconDrawable() {
        this((LayerState) null, null, null);
    }

    /**
     * The one constructor to rule them all. This is called by all public
     * constructors to set the state and initialize local properties.
     */
    private DynamicAdaptiveIconDrawable(@Nullable LayerState state, @Nullable Resources res,
            Path iconMask) {
        mLayerState = createConstantState(state, res);

        mOriginalMask = iconMask;
        mMask = new Path(iconMask);
        mMaskScaleOnly = new Path(mMask);
        mMaskMatrix = new Matrix();
        mCanvas = new Canvas();
        mTransparentRegion = new Region();
    }

    private ChildDrawable createChildDrawable(Drawable drawable) {
        final ChildDrawable layer = new ChildDrawable(mLayerState.mDensity);
        layer.mDrawable = drawable;
        layer.mDrawable.setCallback(this);
        mLayerState.mChildrenChangingConfigurations |=
                layer.mDrawable.getChangingConfigurations();
        return layer;
    }

    private LayerState createConstantState(@Nullable LayerState state, @Nullable Resources res) {
        return new LayerState(state, this, res);
    }

    /**
     * Constructor used to dynamically create this drawable.
     *
     * @param backgroundDrawable drawable that should be rendered in the background
     * @param foregroundDrawable drawable that should be rendered in the foreground
     * @param iconMask path to use to mask the icon
     */
    public DynamicAdaptiveIconDrawable(Drawable backgroundDrawable,
            Drawable foregroundDrawable, Path iconMask) {
        this((LayerState)null, null, iconMask);
        if (backgroundDrawable != null) {
            addLayer(BACKGROUND_ID, createChildDrawable(backgroundDrawable));
        }
        if (foregroundDrawable != null) {
            addLayer(FOREGROUND_ID, createChildDrawable(foregroundDrawable));
        }
    }

    /**
     * Sets the layer to the {@param index} and invalidates cache.
     *
     * @param index The index of the layer.
     * @param layer The layer to add.
     */
    private void addLayer(int index, @NonNull ChildDrawable layer) {
        mLayerState.mChildren[index] = layer;
        mLayerState.invalidateCache();
    }

    @Override
    public void inflate(@NonNull Resources r, @NonNull XmlPullParser parser,
            @NonNull AttributeSet attrs, @Nullable Theme theme)
            throws XmlPullParserException, IOException {
        super.inflate(r, parser, attrs, theme);

        final LayerState state = mLayerState;
        if (state == null) {
            return;
        }

        inflateLayers(r, parser, attrs, theme);
    }

    /**
     * When called before the bound is set, the returned path is identical to
     * R.string.config_icon_mask. After the bound is set, the
     * returned path's computed bound is same as the #getBounds().
     *
     * @return the mask path object used to clip the drawable
     */
    public Path getIconMask() {
        return mMask;
    }

    /**
     * Returns the foreground drawable managed by this class.
     *
     * @return the foreground drawable managed by this drawable
     */
    public Drawable getForeground() {
        return mLayerState.mChildren[FOREGROUND_ID].mDrawable;
    }

    /**
     * Returns the foreground drawable managed by this class. The bound of this drawable is
     * extended by {@link #getExtraInsetFraction()} * getBounds().width on left/right sides and by
     * {@link #getExtraInsetFraction()} * getBounds().height on top/bottom sides.
     *
     * @return the background drawable managed by this drawable
     */
    public Drawable getBackground() {
        return mLayerState.mChildren[BACKGROUND_ID].mDrawable;
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        if (bounds.isEmpty()) {
            return;
        }
        updateLayerBounds(bounds);
    }

    private void updateLayerBounds(Rect bounds) {
        if (bounds.isEmpty()) {
            return;
        }
        try {
            suspendChildInvalidation();
            updateLayerBoundsInternal(bounds);
            updateMaskBoundsInternal(bounds);
        } finally {
            resumeChildInvalidation();
        }
    }

    /**
     * Set the child layer bounds bigger than the view port size by {@link #DEFAULT_VIEW_PORT_SCALE}
     */
    private void updateLayerBoundsInternal(Rect bounds) {
        int cX = bounds.width() / 2;
        int cY = bounds.height() / 2;

        for (int i = 0, count = mLayerState.N_CHILDREN; i < count; i++) {
            final ChildDrawable r = mLayerState.mChildren[i];
            if (r == null) {
                continue;
            }
            final Drawable d = r.mDrawable;
            if (d == null) {
                continue;
            }

            int insetWidth = (int) (bounds.width() / (DEFAULT_VIEW_PORT_SCALE * 2));
            int insetHeight = (int) (bounds.height() / (DEFAULT_VIEW_PORT_SCALE * 2));
            final Rect outRect = mTmpOutRect;
            outRect.set(cX - insetWidth, cY - insetHeight, cX + insetWidth, cY + insetHeight);

            d.setBounds(outRect);
        }
    }

    private void updateMaskBoundsInternal(Rect b) {
        // reset everything that depends on the view bounds
        mMaskMatrix.setScale(b.width() / MASK_SIZE, b.height() / MASK_SIZE);
        mOriginalMask.transform(mMaskMatrix, mMaskScaleOnly);

        mMaskMatrix.postTranslate(b.left, b.top);
        mOriginalMask.transform(mMaskMatrix, mMask);

        if (mLayersBitmap == null || mLayersBitmap.getWidth() != b.width()
                || mLayersBitmap.getHeight() != b.height()) {
            mLayersBitmap = Bitmap.createBitmap(b.width(), b.height(), Bitmap.Config.ARGB_8888);
        }

        mPaint.setShader(null);
        mTransparentRegion.setEmpty();
        mLayersShader = null;
    }

    @Override
    public void draw(Canvas canvas) {
        if (mLayersBitmap == null) {
            return;
        }
        if (mLayersShader == null) {
            mCanvas.setBitmap(mLayersBitmap);
            mCanvas.drawColor(Color.BLACK);
            for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
                if (mLayerState.mChildren[i] == null) {
                    continue;
                }
                final Drawable dr = mLayerState.mChildren[i].mDrawable;
                if (dr != null) {
                    dr.draw(mCanvas);
                }
            }
            mLayersShader = new BitmapShader(mLayersBitmap, TileMode.CLAMP, TileMode.CLAMP);
            mPaint.setShader(mLayersShader);
        }
        if (mMaskScaleOnly != null) {
            Rect bounds = getBounds();
            canvas.translate(bounds.left, bounds.top);
            canvas.drawPath(mMaskScaleOnly, mPaint);
            canvas.translate(-bounds.left, -bounds.top);
        }
    }

    @Override
    public void invalidateSelf() {
        mLayersShader = null;
        super.invalidateSelf();
    }

    @Override
    public void getOutline(@NonNull Outline outline) {
        outline.setConvexPath(mMask);
    }

    @Override
    public @Nullable Region getTransparentRegion() {
        if (mTransparentRegion.isEmpty()) {
            mMask.toggleInverseFillType();
            mTransparentRegion.set(getBounds());
            mTransparentRegion.setPath(mMask, mTransparentRegion);
            mMask.toggleInverseFillType();
        }
        return mTransparentRegion;
    }

    @Override
    public void applyTheme(@NonNull Theme t) {
        super.applyTheme(t);

        final LayerState state = mLayerState;
        if (state == null) {
            return;
        }

        final ChildDrawable[] array = state.mChildren;
        for (int i = 0; i < state.N_CHILDREN; i++) {
            final ChildDrawable layer = array[i];

            final Drawable d = layer.mDrawable;
            if (d != null && d.canApplyTheme()) {
                d.applyTheme(t);

                // Update cached mask of child changing configurations.
                state.mChildrenChangingConfigurations |= d.getChangingConfigurations();
            }
        }
    }

    /**
     * Inflates child layers using the specified parser.
     */
    private void inflateLayers(@NonNull Resources r, @NonNull XmlPullParser parser,
            @NonNull AttributeSet attrs, @Nullable Theme theme)
            throws XmlPullParserException, IOException {
        final LayerState state = mLayerState;

        final int innerDepth = parser.getDepth() + 1;
        int type;
        int depth;
        int childIndex = 0;
        while ((type = parser.next()) != XmlPullParser.END_DOCUMENT
                && ((depth = parser.getDepth()) >= innerDepth || type != XmlPullParser.END_TAG)) {
            if (type != XmlPullParser.START_TAG) {
                continue;
            }

            if (depth > innerDepth) {
                continue;
            }
            String tagName = parser.getName();
            if (tagName.equals("background")) {
                childIndex = BACKGROUND_ID;
            } else if (tagName.equals("foreground")) {
                childIndex = FOREGROUND_ID;
            } else {
                continue;
            }

            final ChildDrawable layer = new ChildDrawable(state.mDensity);

            // If the layer doesn't have a drawable or unresolved theme
            // attribute for a drawable, attempt to parse one from the child
            // element. If multiple child elements exist, we'll only use the
            // first one.
            if (layer.mDrawable == null && (layer.mThemeAttrs == null)) {
                while ((type = parser.next()) == XmlPullParser.TEXT) {
                }
                if (type != XmlPullParser.START_TAG) {
                    throw new XmlPullParserException(parser.getPositionDescription()
                            + ": <foreground> or <background> tag requires a 'drawable'"
                            + "attribute or child tag defining a drawable");
                }

                // We found a child drawable. Take ownership.
                layer.mDrawable = Drawable.createFromXmlInner(r, parser, attrs, theme);
                layer.mDrawable.setCallback(this);
                state.mChildrenChangingConfigurations |=
                        layer.mDrawable.getChangingConfigurations();
            }
            addLayer(childIndex, layer);
        }
    }

    @Override
    public boolean canApplyTheme() {
        return (mLayerState != null && mLayerState.canApplyTheme()) || super.canApplyTheme();
    }

    @Override
    public boolean isProjected() {
        if (super.isProjected()) {
            return true;
        }

        final ChildDrawable[] layers = mLayerState.mChildren;
        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            if (layers[i].mDrawable != null && layers[i].mDrawable.isProjected()) {
                return true;
            }
        }
        return false;
    }

    /**
     * Temporarily suspends child invalidation.
     *
     * @see #resumeChildInvalidation()
     */
    private void suspendChildInvalidation() {
        mSuspendChildInvalidation = true;
    }

    /**
     * Resumes child invalidation after suspension, immediately performing an
     * invalidation if one was requested by a child during suspension.
     *
     * @see #suspendChildInvalidation()
     */
    private void resumeChildInvalidation() {
        mSuspendChildInvalidation = false;

        if (mChildRequestedInvalidation) {
            mChildRequestedInvalidation = false;
            invalidateSelf();
        }
    }

    @Override
    public void invalidateDrawable(@NonNull Drawable who) {
        if (mSuspendChildInvalidation) {
            mChildRequestedInvalidation = true;
        } else {
            invalidateSelf();
        }
    }

    @Override
    public void scheduleDrawable(@NonNull Drawable who, @NonNull Runnable what, long when) {
        scheduleSelf(what, when);
    }

    @Override
    public void unscheduleDrawable(@NonNull Drawable who, @NonNull Runnable what) {
        unscheduleSelf(what);
    }

    @Override
    public int getChangingConfigurations() {
        return super.getChangingConfigurations() | mLayerState.getChangingConfigurations();
    }

    @Override
    public void setHotspot(float x, float y) {
        final ChildDrawable[] array = mLayerState.mChildren;
        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            final Drawable dr = array[i].mDrawable;
            if (dr != null) {
                dr.setHotspot(x, y);
            }
        }
    }

    @Override
    public void setHotspotBounds(int left, int top, int right, int bottom) {
        final ChildDrawable[] array = mLayerState.mChildren;
        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            final Drawable dr = array[i].mDrawable;
            if (dr != null) {
                dr.setHotspotBounds(left, top, right, bottom);
            }
        }

        if (mHotspotBounds == null) {
            mHotspotBounds = new Rect(left, top, right, bottom);
        } else {
            mHotspotBounds.set(left, top, right, bottom);
        }
    }

    @Override
    public void getHotspotBounds(Rect outRect) {
        if (mHotspotBounds != null) {
            outRect.set(mHotspotBounds);
        } else {
            super.getHotspotBounds(outRect);
        }
    }

    @Override
    public boolean setVisible(boolean visible, boolean restart) {
        final boolean changed = super.setVisible(visible, restart);
        final ChildDrawable[] array = mLayerState.mChildren;

        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            final Drawable dr = array[i].mDrawable;
            if (dr != null) {
                dr.setVisible(visible, restart);
            }
        }

        return changed;
    }

    @Override
    public void setDither(boolean dither) {
        final ChildDrawable[] array = mLayerState.mChildren;
        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            final Drawable dr = array[i].mDrawable;
            if (dr != null) {
                dr.setDither(dither);
            }
        }
    }

    @Override
    public void setAlpha(int alpha) {
        mPaint.setAlpha(alpha);
    }

    @Override
    public int getAlpha() {
        return mPaint.getAlpha();
    }

    @Override
    public void setColorFilter(ColorFilter colorFilter) {
        final ChildDrawable[] array = mLayerState.mChildren;
        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            final Drawable dr = array[i].mDrawable;
            if (dr != null) {
                dr.setColorFilter(colorFilter);
            }
        }
    }

    @Override
    public void setTintList(ColorStateList tint) {
        final ChildDrawable[] array = mLayerState.mChildren;
        final int N = mLayerState.N_CHILDREN;
        for (int i = 0; i < N; i++) {
            final Drawable dr = array[i].mDrawable;
            if (dr != null) {
                dr.setTintList(tint);
            }
        }
    }

    @Override
    public void setTintMode(Mode tintMode) {
        final ChildDrawable[] array = mLayerState.mChildren;
        final int N = mLayerState.N_CHILDREN;
        for (int i = 0; i < N; i++) {
            final Drawable dr = array[i].mDrawable;
            if (dr != null) {
                dr.setTintMode(tintMode);
            }
        }
    }

    public void setOpacity(int opacity) {
        mLayerState.mOpacityOverride = opacity;
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public void setAutoMirrored(boolean mirrored) {
        mLayerState.mAutoMirrored = mirrored;

        final ChildDrawable[] array = mLayerState.mChildren;
        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            final Drawable dr = array[i].mDrawable;
            if (dr != null) {
                dr.setAutoMirrored(mirrored);
            }
        }
    }

    @Override
    public boolean isAutoMirrored() {
        return mLayerState.mAutoMirrored;
    }

    @Override
    public void jumpToCurrentState() {
        final ChildDrawable[] array = mLayerState.mChildren;
        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            final Drawable dr = array[i].mDrawable;
            if (dr != null) {
                dr.jumpToCurrentState();
            }
        }
    }

    @Override
    public boolean isStateful() {
        return mLayerState.isStateful();
    }

    @Override
    protected boolean onStateChange(int[] state) {
        boolean changed = false;

        final ChildDrawable[] array = mLayerState.mChildren;
        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            final Drawable dr = array[i].mDrawable;
            if (dr != null && dr.isStateful() && dr.setState(state)) {
                changed = true;
            }
        }

        if (changed) {
            updateLayerBounds(getBounds());
        }

        return changed;
    }

    @Override
    protected boolean onLevelChange(int level) {
        boolean changed = false;

        final ChildDrawable[] array = mLayerState.mChildren;
        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            final Drawable dr = array[i].mDrawable;
            if (dr != null && dr.setLevel(level)) {
                changed = true;
            }
        }

        if (changed) {
            updateLayerBounds(getBounds());
        }

        return changed;
    }

    @Override
    public int getIntrinsicWidth() {
        return (int)(getMaxIntrinsicWidth() * DEFAULT_VIEW_PORT_SCALE);
    }

    private int getMaxIntrinsicWidth() {
        int width = -1;
        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            final ChildDrawable r = mLayerState.mChildren[i];
            if (r.mDrawable == null) {
                continue;
            }
            final int w = r.mDrawable.getIntrinsicWidth();
            if (w > width) {
                width = w;
            }
        }
        return width;
    }

    @Override
    public int getIntrinsicHeight() {
        return (int)(getMaxIntrinsicHeight() * DEFAULT_VIEW_PORT_SCALE);
    }

    private int getMaxIntrinsicHeight() {
        int height = -1;
        for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
            final ChildDrawable r = mLayerState.mChildren[i];
            if (r.mDrawable == null) {
                continue;
            }
            final int h = r.mDrawable.getIntrinsicHeight();
            if (h > height) {
                height = h;
            }
        }
        return height;
    }

    @Override
    public ConstantState getConstantState() {
        if (mLayerState.canConstantState()) {
            mLayerState.mChangingConfigurations = getChangingConfigurations();
            return mLayerState;
        }
        return null;
    }

    @Override
    public Drawable mutate() {
        if (!mMutated && super.mutate() == this) {
            mLayerState = createConstantState(mLayerState, null);
            for (int i = 0; i < mLayerState.N_CHILDREN; i++) {
                final Drawable dr = mLayerState.mChildren[i].mDrawable;
                if (dr != null) {
                    dr.mutate();
                }
            }
            mMutated = true;
        }
        return this;
    }

    static class ChildDrawable {
        public Drawable mDrawable;
        public int[] mThemeAttrs;
        public int mDensity = DisplayMetrics.DENSITY_DEFAULT;

        ChildDrawable(int density) {
            mDensity = density;
        }

        ChildDrawable(@NonNull ChildDrawable orig, @NonNull DynamicAdaptiveIconDrawable owner,
                @Nullable Resources res) {

            final Drawable dr = orig.mDrawable;
            final Drawable clone;
            if (dr != null) {
                final ConstantState cs = dr.getConstantState();
                if (cs == null) {
                    clone = dr;
                } else if (res != null) {
                    clone = cs.newDrawable(res);
                } else {
                    clone = cs.newDrawable();
                }
                clone.setCallback(owner);
                clone.setBounds(dr.getBounds());
                clone.setLevel(dr.getLevel());
            } else {
                clone = null;
            }

            mDrawable = clone;
            mThemeAttrs = orig.mThemeAttrs;
        }

        public boolean canApplyTheme() {
            return mThemeAttrs != null
                    || (mDrawable != null && mDrawable.canApplyTheme());
        }

        public final void setDensity(int targetDensity) {
            if (mDensity != targetDensity) {
                mDensity = targetDensity;
            }
        }
    }

    static class LayerState extends ConstantState {
        private final DynamicAdaptiveIconDrawable mOwner;

        private int[] mThemeAttrs;

        final static int N_CHILDREN = 2;
        ChildDrawable[] mChildren;

        // The density at which to render the drawable and its children.
        int mDensity;

        // The density to use when inflating/looking up the children drawables. A value of 0 means
        // use the system's density.
        int mSrcDensityOverride = 0;

        int mOpacityOverride = PixelFormat.UNKNOWN;

        int mChangingConfigurations;
        int mChildrenChangingConfigurations;

        private boolean mCheckedOpacity;
        private int mOpacity;

        private boolean mCheckedStateful;
        private boolean mIsStateful;
        private boolean mAutoMirrored = false;

        LayerState(@Nullable LayerState orig, @NonNull DynamicAdaptiveIconDrawable owner,
                @Nullable Resources res) {
            mOwner = owner;
            mChildren = new ChildDrawable[N_CHILDREN];
            if (orig != null) {
                final ChildDrawable[] origChildDrawable = orig.mChildren;

                mChangingConfigurations = orig.mChangingConfigurations;
                mChildrenChangingConfigurations = orig.mChildrenChangingConfigurations;

                for (int i = 0; i < N_CHILDREN; i++) {
                    final ChildDrawable or = origChildDrawable[i];
                    mChildren[i] = new ChildDrawable(or, mOwner, res);
                }

                mCheckedOpacity = orig.mCheckedOpacity;
                mOpacity = orig.mOpacity;
                mCheckedStateful = orig.mCheckedStateful;
                mIsStateful = orig.mIsStateful;
                mAutoMirrored = orig.mAutoMirrored;
                mThemeAttrs = orig.mThemeAttrs;
                mOpacityOverride = orig.mOpacityOverride;
                mSrcDensityOverride = orig.mSrcDensityOverride;
            } else {
                for (int i = 0; i < N_CHILDREN; i++) {
                    mChildren[i] = new ChildDrawable(mDensity);
                }
            }
        }

        public final void setDensity(int targetDensity) {
            if (mDensity != targetDensity) {
                mDensity = targetDensity;
            }
        }

        @Override
        public boolean canApplyTheme() {
            if (mThemeAttrs != null || super.canApplyTheme()) {
                return true;
            }

            final ChildDrawable[] array = mChildren;
            for (int i = 0; i < N_CHILDREN; i++) {
                final ChildDrawable layer = array[i];
                if (layer.canApplyTheme()) {
                    return true;
                }
            }
            return false;
        }

        @Override
        public Drawable newDrawable() {
            return new DynamicAdaptiveIconDrawable(mOwner.getBackground(), mOwner.getForeground(),
                    mOwner.mOriginalMask);
        }

        @Override
        public Drawable newDrawable(@Nullable Resources res) {
            return newDrawable();
        }

        @Override
        public int getChangingConfigurations() {
            return mChangingConfigurations
                    | mChildrenChangingConfigurations;
        }

        public final int getOpacity() {
            if (mCheckedOpacity) {
                return mOpacity;
            }

            final ChildDrawable[] array = mChildren;

            // Seek to the first non-null drawable.
            int firstIndex = -1;
            for (int i = 0; i < N_CHILDREN; i++) {
                if (array[i].mDrawable != null) {
                    firstIndex = i;
                    break;
                }
            }

            int op;
            if (firstIndex >= 0) {
                op = array[firstIndex].mDrawable.getOpacity();
            } else {
                op = PixelFormat.TRANSPARENT;
            }

            // Merge all remaining non-null drawables.
            for (int i = firstIndex + 1; i < N_CHILDREN; i++) {
                final Drawable dr = array[i].mDrawable;
                if (dr != null) {
                    op = Drawable.resolveOpacity(op, dr.getOpacity());
                }
            }

            mOpacity = op;
            mCheckedOpacity = true;
            return op;
        }

        public final boolean isStateful() {
            if (mCheckedStateful) {
                return mIsStateful;
            }

            final ChildDrawable[] array = mChildren;
            boolean isStateful = false;
            for (int i = 0; i < N_CHILDREN; i++) {
                final Drawable dr = array[i].mDrawable;
                if (dr != null && dr.isStateful()) {
                    isStateful = true;
                    break;
                }
            }

            mIsStateful = isStateful;
            mCheckedStateful = true;
            return isStateful;
        }

        public final boolean canConstantState() {
            final ChildDrawable[] array = mChildren;
            for (int i = 0; i < N_CHILDREN; i++) {
                final Drawable dr = array[i].mDrawable;
                if (dr != null && dr.getConstantState() == null) {
                    return false;
                }
            }

            // Don't cache the result, this method is not called very often.
            return true;
        }

        public void invalidateCache() {
            mCheckedOpacity = false;
            mCheckedStateful = false;
        }
    }
}