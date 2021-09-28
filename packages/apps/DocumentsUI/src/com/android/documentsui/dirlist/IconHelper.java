/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.documentsui.dirlist;

import static com.android.documentsui.base.SharedMinimal.VERBOSE;
import static com.android.documentsui.base.State.MODE_GRID;
import static com.android.documentsui.base.State.MODE_LIST;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.util.Log;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.android.documentsui.DocumentsApplication;
import com.android.documentsui.IconUtils;
import com.android.documentsui.ProviderExecutor;
import com.android.documentsui.R;
import com.android.documentsui.ThumbnailCache;
import com.android.documentsui.ThumbnailCache.Result;
import com.android.documentsui.ThumbnailLoader;
import com.android.documentsui.base.DocumentInfo;
import com.android.documentsui.base.MimeTypes;
import com.android.documentsui.base.State;
import com.android.documentsui.base.State.ViewMode;
import com.android.documentsui.base.UserId;

import java.util.function.BiConsumer;

/**
 * A class to assist with loading and managing the Images (i.e. thumbnails and icons) associated
 * with items in the directory listing.
 */
public class IconHelper {
    private static final String TAG = "IconHelper";

    private final Context mContext;
    private final ThumbnailCache mThumbnailCache;

    // The display mode (MODE_GRID, MODE_LIST, etc).
    private int mMode;
    private Point mCurrentSize;
    private boolean mThumbnailsEnabled = true;
    private final boolean mMaybeShowBadge;
    @Nullable
    private final UserId mManagedUser;

    /**
     * @param context
     * @param mode MODE_GRID or MODE_LIST
     */
    public IconHelper(Context context, int mode, boolean maybeShowBadge) {
        this(context, mode, maybeShowBadge, DocumentsApplication.getThumbnailCache(context),
                DocumentsApplication.getUserIdManager(context).getManagedUser());
    }

    @VisibleForTesting
    IconHelper(Context context, int mode, boolean maybeShowBadge, ThumbnailCache thumbnailCache,
            @Nullable UserId managedUser) {
        mContext = context;
        setViewMode(mode);
        mThumbnailCache = thumbnailCache;
        mManagedUser = managedUser;
        mMaybeShowBadge = maybeShowBadge;
    }

    /**
     * Enables or disables thumbnails. When thumbnails are disabled, mime icons (or custom icons, if
     * specified by the document) are used instead.
     *
     * @param enabled
     */
    public void setThumbnailsEnabled(boolean enabled) {
        mThumbnailsEnabled = enabled;
    }

    /**
     * Sets the current display mode. This affects the thumbnail sizes that are loaded.
     *
     * @param mode See {@link State.MODE_LIST} and {@link State.MODE_GRID}.
     */
    public void setViewMode(@ViewMode int mode) {
        mMode = mode;
        int thumbSize = getThumbSize(mode);
        mCurrentSize = new Point(thumbSize, thumbSize);
    }

    private int getThumbSize(int mode) {
        int thumbSize;
        switch (mode) {
            case MODE_GRID:
                thumbSize = mContext.getResources().getDimensionPixelSize(R.dimen.grid_width);
                break;
            case MODE_LIST:
                thumbSize = mContext.getResources().getDimensionPixelSize(
                        R.dimen.list_item_thumbnail_size);
                break;
            default:
                throw new IllegalArgumentException("Unsupported layout mode: " + mode);
        }
        return thumbSize;
    }

    /**
     * Cancels any ongoing load operations associated with the given ImageView.
     *
     * @param icon
     */
    public void stopLoading(ImageView icon) {
        final ThumbnailLoader oldTask = (ThumbnailLoader) icon.getTag();
        if (oldTask != null) {
            oldTask.preempt();
            icon.setTag(null);
        }
    }

    /**
     * Load thumbnails for a directory list item.
     *
     * @param doc The document
     * @param iconThumb The itemview's thumbnail icon.
     * @param iconMime The itemview's mime icon. Hidden when iconThumb is shown.
     * @param subIconMime The second itemview's mime icon. Always visible.
     * @return
     */
    public void load(
            DocumentInfo doc,
            ImageView iconThumb,
            ImageView iconMime,
            @Nullable ImageView subIconMime) {
        load(doc.derivedUri, doc.userId, doc.mimeType, doc.flags, doc.icon, doc.lastModified,
                iconThumb, iconMime, subIconMime);
    }

    /**
     * Load thumbnails for a directory list item.
     *
     * @param uri The URI for the file being represented.
     * @param mimeType The mime type of the file being represented.
     * @param docFlags Flags for the file being represented.
     * @param docIcon Custom icon (if any) for the file being requested.
     * @param docLastModified the last modified value of the file being requested.
     * @param iconThumb The itemview's thumbnail icon.
     * @param iconMime The itemview's mime icon. Hidden when iconThumb is shown.
     * @param subIconMime The second itemview's mime icon. Always visible.
     * @return
     */
    public void load(Uri uri, UserId userId, String mimeType, int docFlags, int docIcon,
            long docLastModified, ImageView iconThumb, ImageView iconMime,
            @Nullable ImageView subIconMime) {
        boolean loadedThumbnail = false;

        final String docAuthority = uri.getAuthority();

        final boolean supportsThumbnail = (docFlags & Document.FLAG_SUPPORTS_THUMBNAIL) != 0;
        final boolean allowThumbnail = (mMode == MODE_GRID)
                || MimeTypes.mimeMatches(MimeTypes.VISUAL_MIMES, mimeType);
        final boolean showThumbnail = supportsThumbnail && allowThumbnail && mThumbnailsEnabled;
        if (showThumbnail) {
            loadedThumbnail =
                loadThumbnail(uri, userId, docAuthority, docLastModified, iconThumb, iconMime);
        }

        final Drawable mimeIcon = getDocumentIcon(mContext, userId, docAuthority,
                DocumentsContract.getDocumentId(uri), mimeType, docIcon);
        if (subIconMime != null) {
            setMimeIcon(subIconMime, mimeIcon);
        }

        if (loadedThumbnail) {
            hideImageView(iconMime);
        } else {
            // Add a mime icon if the thumbnail is not shown.
            setMimeIcon(iconMime, mimeIcon);
            hideImageView(iconThumb);
        }
    }

    private boolean loadThumbnail(Uri uri, UserId userId, String docAuthority, long docLastModified,
            ImageView iconThumb, ImageView iconMime) {
        final Result result = mThumbnailCache.getThumbnail(uri, userId, mCurrentSize);

        try {
            final Bitmap cachedThumbnail = result.getThumbnail();
            iconThumb.setImageBitmap(cachedThumbnail);

            boolean stale = (docLastModified > result.getLastModified());
            if (VERBOSE) Log.v(TAG,
                    String.format("Load thumbnail for %s, got result %d and stale %b.",
                            uri.toString(), result.getStatus(), stale));
            if (!result.isExactHit() || stale) {
                final BiConsumer<View, View> animator =
                        (cachedThumbnail == null ? ThumbnailLoader.ANIM_FADE_IN :
                                ThumbnailLoader.ANIM_NO_OP);

                final ThumbnailLoader task = new ThumbnailLoader(uri, userId, iconThumb,
                        mCurrentSize, docLastModified,
                        bitmap -> {
                            if (bitmap != null) {
                                iconThumb.setImageBitmap(bitmap);
                                animator.accept(iconMime, iconThumb);
                            }
                        }, true /* addToCache */);

                ProviderExecutor.forAuthority(docAuthority).execute(task);
            }

            return result.isHit();
        } finally {
            result.recycle();
        }
    }

    private void setMimeIcon(ImageView view, Drawable icon) {
        view.setImageDrawable(icon);
        view.setAlpha(1f);
    }

    private void hideImageView(ImageView view) {
        view.setImageDrawable(null);
        view.setAlpha(0f);
    }

    private Drawable getDocumentIcon(Context context, UserId userId, String authority, String id,
            String mimeType, int icon) {
        if (icon != 0) {
            return IconUtils.loadPackageIcon(context, userId, authority, icon, mMaybeShowBadge);
        } else {
            return IconUtils.loadMimeIcon(context, mimeType, authority, id, mMode);
        }
    }

    /**
     * Returns a mime icon or package icon for a {@link DocumentInfo}.
     */
    public Drawable getDocumentIcon(Context context, DocumentInfo doc) {
        return getDocumentIcon(
                context, doc.userId, doc.authority, doc.documentId, doc.mimeType, doc.icon);
    }

    /**
     * Returns true if we should show a briefcase icon for the given user.
     */
    public boolean shouldShowBadge(int userIdIdentifier) {
        return mMaybeShowBadge && mManagedUser != null
                && mManagedUser.getIdentifier() == userIdIdentifier;
    }
}
