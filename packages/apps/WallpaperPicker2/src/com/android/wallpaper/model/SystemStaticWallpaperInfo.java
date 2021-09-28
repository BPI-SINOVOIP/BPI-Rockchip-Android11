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
package com.android.wallpaper.model;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.Parcel;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;

import androidx.annotation.Nullable;

import com.android.wallpaper.asset.Asset;
import com.android.wallpaper.asset.ResourceAsset;
import com.android.wallpaper.asset.SystemStaticAsset;

import java.util.ArrayList;
import java.util.List;

/**
 * Represents a wallpaper coming from the resources of the partner static wallpaper
 * container APK.
 */
public class SystemStaticWallpaperInfo extends WallpaperInfo {
    public static final Creator<SystemStaticWallpaperInfo> CREATOR =
            new Creator<SystemStaticWallpaperInfo>() {
                @Override
                public SystemStaticWallpaperInfo createFromParcel(Parcel in) {
                    return new SystemStaticWallpaperInfo(in);
                }

                @Override
                public SystemStaticWallpaperInfo[] newArray(int size) {
                    return new SystemStaticWallpaperInfo[size];
                }
            };
    public static final String TAG_NAME = "static-wallpaper";

    private static final String TAG = "PartnerStaticWPInfo";
    private static final String DRAWABLE_DEF_TYPE = "drawable";
    private static final String STRING_DEF_TYPE = "string";
    private static final String INTEGER_DEF_TYPE = "integer";
    private static final String ARRAY_DEF_TYPE = "array";
    private static final String WALLPAPERS_RES_SUFFIX = "_wallpapers";
    private static final String TITLE_RES_SUFFIX = "_title";
    private static final String SUBTITLE1_RES_SUFFIX = "_subtitle1";
    private static final String SUBTITLE2_RES_SUFFIX = "_subtitle2";
    private static final String ACTION_TYPE_RES_SUFFIX = "_action_type";
    private static final String ACTION_URL_RES_SUFFIX = "_action_url";

    // Xml parsing attribute names
    public static final String ATTR_ID = "id";
    public static final String ATTR_SRC = "src";
    public static final String ATTR_TITLE_RES = "title";
    public static final String ATTR_SUBTITLE1_RES = "subtitle1";
    public static final String ATTR_SUBTITLE2_RES = "subtitle2";
    public static final String ATTR_ACTION_URL_RES = "actionUrl";

    /**
     * Create and return a new {@link SystemStaticWallpaperInfo} from the information in the given
     * XML's {@link AttributeSet}
     * @param packageName name of the package where the resources are read from
     * @param categoryId id of the category the new wallpaper will belong to
     * @param attrs {@link AttributeSet} from the XML with the information for the new wallpaper
     *                                  info
     * @return a new {@link SystemStaticWallpaperInfo} or {@code null} if no id could be found in
     * the given {@link AttributeSet}
     */
    @Nullable
    public static SystemStaticWallpaperInfo fromAttributeSet(String packageName,
            String categoryId, AttributeSet attrs) {
        String wallpaperId = attrs.getAttributeValue(null, ATTR_ID);
        if (TextUtils.isEmpty(wallpaperId)) {
            return null;
        }
        int drawableResId = attrs.getAttributeResourceValue(null, ATTR_SRC, 0);
        int wallpaperTitleResId = attrs.getAttributeResourceValue(null, ATTR_TITLE_RES, 0);
        int wallpaperSubtitle1ResId = attrs.getAttributeResourceValue(null, ATTR_SUBTITLE1_RES, 0);
        int wallpaperSubtitle2ResId = attrs.getAttributeResourceValue(null, ATTR_SUBTITLE2_RES, 0);
        int actionUrlResId = attrs.getAttributeResourceValue(null, ATTR_ACTION_URL_RES, 0);

        return new SystemStaticWallpaperInfo(packageName, wallpaperId,
                categoryId, drawableResId, wallpaperTitleResId, wallpaperSubtitle1ResId,
                wallpaperSubtitle2ResId, 0, actionUrlResId);
    }

    /**
     * Read from the given stub apk the available static categories and wallpapers
     * @deprecated this is left for backwards compatibility with legacy stub format,
     * use {@link #fromAttributeSet(String, String, AttributeSet)} instead for
     * the new stub format.
     */
    @Deprecated
    public static List<WallpaperInfo> getAll(String partnerStubPackageName,
            Resources stubApkResources, String categoryId) {
        ArrayList<WallpaperInfo> wallpapers = new ArrayList<>();

        int listResId = stubApkResources.getIdentifier(categoryId + WALLPAPERS_RES_SUFFIX,
                ARRAY_DEF_TYPE, partnerStubPackageName);
        String[] wallpaperResNames = stubApkResources.getStringArray(listResId);

        for (String wallpaperResName : wallpaperResNames) {
            int drawableResId = stubApkResources.getIdentifier(wallpaperResName, DRAWABLE_DEF_TYPE,
                    partnerStubPackageName);
            int wallpaperTitleResId = stubApkResources.getIdentifier(
                    wallpaperResName + TITLE_RES_SUFFIX, STRING_DEF_TYPE, partnerStubPackageName);
            int wallpaperSubtitle1ResId = stubApkResources.getIdentifier(
                    wallpaperResName + SUBTITLE1_RES_SUFFIX, STRING_DEF_TYPE,
                    partnerStubPackageName);
            int wallpaperSubtitle2ResId = stubApkResources.getIdentifier(
                    wallpaperResName + SUBTITLE2_RES_SUFFIX, STRING_DEF_TYPE,
                    partnerStubPackageName);
            int actionTypeResId = stubApkResources.getIdentifier(
                    wallpaperResName + ACTION_TYPE_RES_SUFFIX, INTEGER_DEF_TYPE,
                    partnerStubPackageName);
            int actionUrlResId = stubApkResources.getIdentifier(
                    wallpaperResName + ACTION_URL_RES_SUFFIX, STRING_DEF_TYPE,
                    partnerStubPackageName);

            SystemStaticWallpaperInfo wallpaperInfo = new SystemStaticWallpaperInfo(
                    partnerStubPackageName, wallpaperResName, categoryId,
                    drawableResId, wallpaperTitleResId, wallpaperSubtitle1ResId,
                    wallpaperSubtitle2ResId, actionTypeResId, actionUrlResId);
            wallpapers.add(wallpaperInfo);
        }

        return wallpapers;
    }

    private final int mDrawableResId;
    private final String mWallpaperId;
    private final String mCollectionId;
    private final int mTitleResId;
    private final int mSubtitle1ResId;
    private final int mSubtitle2ResId;
    private final int mActionTypeResId;
    private final int mActionUrlResId;
    private ResourceAsset mAsset;
    private Resources mResources;
    private final String mPackageName;
    private List<String> mAttributions;
    private String mActionUrl;
    private int mActionType;

    /**
     * Constructs a new Nexus static wallpaper model object.
     *
     * @param resName        The unique name of the wallpaper resource, e.g. "z_wp001".
     * @param collectionId   Unique name of the collection this wallpaper belongs in;
     *                       used for logging.
     * @param drawableResId  Resource ID of the raw wallpaper image.
     * @param titleResId     Resource ID of the string for the title attribution.
     * @param subtitle1ResId Resource ID of the string for the first subtitle attribution.
     * @param subtitle2ResId Resource ID of the string for the second subtitle attribution.
     */
    public SystemStaticWallpaperInfo(String packageName, String resName, String collectionId,
            int drawableResId, int titleResId, int subtitle1ResId, int subtitle2ResId,
            int actionTypeResId, int actionUrlResId) {
        mPackageName = packageName;
        mWallpaperId = resName;
        mCollectionId = collectionId;
        mDrawableResId = drawableResId;
        mTitleResId = titleResId;
        mSubtitle1ResId = subtitle1ResId;
        mSubtitle2ResId = subtitle2ResId;
        mActionTypeResId = actionTypeResId;
        mActionUrlResId = actionUrlResId;
    }

    private SystemStaticWallpaperInfo(Parcel in) {
        mPackageName = in.readString();
        mWallpaperId = in.readString();
        mCollectionId = in.readString();
        mDrawableResId = in.readInt();
        mTitleResId = in.readInt();
        mSubtitle1ResId = in.readInt();
        mSubtitle2ResId = in.readInt();
        mActionTypeResId = in.readInt();
        mActionUrlResId = in.readInt();
    }

    @Override
    public Asset getAsset(Context context) {
        if (mAsset == null) {
            Resources res = getPackageResources(context);
            mAsset = new SystemStaticAsset(res, mDrawableResId, mWallpaperId);
        }

        return mAsset;
    }

    @Override
    public Asset getThumbAsset(Context context) {
        return getAsset(context);
    }

    @Override
    public List<String> getAttributions(Context context) {
        if (mAttributions == null) {
            Resources res = getPackageResources(context);
            mAttributions = new ArrayList<>();
            if (mTitleResId != 0) {
                mAttributions.add(res.getString(mTitleResId));
            }
            if (mSubtitle1ResId != 0) {
                mAttributions.add(res.getString(mSubtitle1ResId));
            }
            if (mSubtitle2ResId != 0) {
                mAttributions.add(res.getString(mSubtitle2ResId));
            }
        }

        return mAttributions;
    }

    @Override
    public String getActionUrl(Context context) {
        if (mActionUrl == null && mActionUrlResId != 0) {
            mActionUrl = getPackageResources(context).getString(mActionUrlResId);
        }
        return mActionUrl;
    }

    @Override
    public int getActionLabelRes(Context context) {
        return WallpaperInfo.getDefaultActionLabel();
    }

    @Override
    public int getActionIconRes(Context context) {
        return WallpaperInfo.getDefaultActionIcon();
    }

    @Override
    public void showPreview(Activity srcActivity, InlinePreviewIntentFactory factory,
                            int requestCode) {
        srcActivity.startActivityForResult(factory.newIntent(srcActivity, this), requestCode);
    }

    @Override
    public String getCollectionId(Context unused) {
        return mCollectionId;
    }

    @Override
    public String getWallpaperId() {
        return mWallpaperId;
    }

    public String getResName() {
        return mWallpaperId;
    }

    private int getActionType(Context context) {
        if (mActionType == 0 && mActionTypeResId != 0) {
            mActionType = getPackageResources(context).getInteger(mActionTypeResId);
        }
        return mActionType;
    }

    /**
     * Returns the {@link Resources} instance for the Nexus static wallpapers stub APK.
     */
    private Resources getPackageResources(Context context) {
        if (mResources != null) {
            return mResources;
        }

        try {
            mResources = context.getPackageManager().getResourcesForApplication(mPackageName);
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Could not get app resources");
        }
        return mResources;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(mPackageName);
        dest.writeString(mWallpaperId);
        dest.writeString(mCollectionId);
        dest.writeInt(mDrawableResId);
        dest.writeInt(mTitleResId);
        dest.writeInt(mSubtitle1ResId);
        dest.writeInt(mSubtitle2ResId);
        dest.writeInt(mActionTypeResId);
        dest.writeInt(mActionUrlResId);
    }
}
