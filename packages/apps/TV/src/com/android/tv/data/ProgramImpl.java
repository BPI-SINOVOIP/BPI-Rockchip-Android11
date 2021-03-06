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

package com.android.tv.data;

import android.annotation.SuppressLint;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.media.tv.TvContentRating;
import android.media.tv.TvContract;
import android.media.tv.TvContract.Programs;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.support.annotation.VisibleForTesting;
import android.support.annotation.WorkerThread;
import android.text.TextUtils;

import com.android.tv.common.TvContentRatingCache;
import com.android.tv.common.util.CollectionUtils;
import com.android.tv.common.util.CommonUtils;
import com.android.tv.data.api.BaseProgram;
import com.android.tv.data.api.Channel;
import com.android.tv.data.api.Program;
import com.android.tv.util.TvProviderUtils;
import com.android.tv.util.Utils;
import com.android.tv.util.images.ImageLoader;

import com.google.common.collect.ImmutableList;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;

/** A convenience class to create and insert program information entries into the database. */
public final class ProgramImpl extends BaseProgramImpl implements Parcelable, Program {
    private static final boolean DEBUG = false;
    private static final boolean DEBUG_DUMP_DESCRIPTION = false;
    private static final String TAG = "Program";

    private static final String[] PROJECTION_BASE = {
        // Columns must match what is read in Program.fromCursor()
        TvContract.Programs._ID,
        TvContract.Programs.COLUMN_PACKAGE_NAME,
        TvContract.Programs.COLUMN_CHANNEL_ID,
        TvContract.Programs.COLUMN_TITLE,
        TvContract.Programs.COLUMN_EPISODE_TITLE,
        TvContract.Programs.COLUMN_SHORT_DESCRIPTION,
        TvContract.Programs.COLUMN_LONG_DESCRIPTION,
        TvContract.Programs.COLUMN_POSTER_ART_URI,
        TvContract.Programs.COLUMN_THUMBNAIL_URI,
        TvContract.Programs.COLUMN_CANONICAL_GENRE,
        TvContract.Programs.COLUMN_CONTENT_RATING,
        TvContract.Programs.COLUMN_START_TIME_UTC_MILLIS,
        TvContract.Programs.COLUMN_END_TIME_UTC_MILLIS,
        TvContract.Programs.COLUMN_VIDEO_WIDTH,
        TvContract.Programs.COLUMN_VIDEO_HEIGHT,
        TvContract.Programs.COLUMN_INTERNAL_PROVIDER_DATA
    };

    // Columns which is deprecated in NYC
    @SuppressWarnings("deprecation")
    private static final String[] PROJECTION_DEPRECATED_IN_NYC = {
        TvContract.Programs.COLUMN_SEASON_NUMBER, TvContract.Programs.COLUMN_EPISODE_NUMBER
    };

    private static final String[] PROJECTION_ADDED_IN_NYC = {
        TvContract.Programs.COLUMN_SEASON_DISPLAY_NUMBER,
        TvContract.Programs.COLUMN_SEASON_TITLE,
        TvContract.Programs.COLUMN_EPISODE_DISPLAY_NUMBER,
        TvContract.Programs.COLUMN_RECORDING_PROHIBITED
    };

    public static final String[] PROJECTION = createProjection();

    public static final String[] PARTIAL_PROJECTION = {
        TvContract.Programs._ID,
        TvContract.Programs.COLUMN_CHANNEL_ID,
        TvContract.Programs.COLUMN_TITLE,
        TvContract.Programs.COLUMN_EPISODE_TITLE,
        TvContract.Programs.COLUMN_CANONICAL_GENRE,
        TvContract.Programs.COLUMN_START_TIME_UTC_MILLIS,
        TvContract.Programs.COLUMN_END_TIME_UTC_MILLIS,
    };

    private static String[] createProjection() {
        return CollectionUtils.concatAll(
                PROJECTION_BASE,
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                        ? PROJECTION_ADDED_IN_NYC
                        : PROJECTION_DEPRECATED_IN_NYC);
    }

    /**
     * Returns the column index for {@code column},-1 if the column doesn't exist in {@link
     * #PROJECTION}.
     */
    public static int getColumnIndex(String column) {
        for (int i = 0; i < PROJECTION.length; ++i) {
            if (PROJECTION[i].equals(column)) {
                return i;
            }
        }
        return -1;
    }

    /** Creates {@code Program} object from cursor. */
    public static Program fromCursor(Cursor cursor) {
        // Columns read must match the order of match {@link #PROJECTION}
        Builder builder = new Builder();
        int index = 0;
        builder.setId(cursor.getLong(index++));
        String packageName = cursor.getString(index++);
        builder.setPackageName(packageName);
        builder.setChannelId(cursor.getLong(index++));
        builder.setTitle(cursor.getString(index++));
        builder.setEpisodeTitle(cursor.getString(index++));
        builder.setDescription(cursor.getString(index++));
        builder.setLongDescription(cursor.getString(index++));
        builder.setPosterArtUri(cursor.getString(index++));
        builder.setThumbnailUri(cursor.getString(index++));
        builder.setCanonicalGenres(cursor.getString(index++));
        builder.setContentRatings(
                TvContentRatingCache.getInstance().getRatings(cursor.getString(index++)));
        builder.setStartTimeUtcMillis(cursor.getLong(index++));
        builder.setEndTimeUtcMillis(cursor.getLong(index++));
        builder.setVideoWidth((int) cursor.getLong(index++));
        builder.setVideoHeight((int) cursor.getLong(index++));
        if (CommonUtils.isInBundledPackageSet(packageName)) {
            InternalDataUtils.deserializeInternalProviderData(cursor.getBlob(index), builder);
        }
        index++;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            builder.setSeasonNumber(cursor.getString(index++));
            builder.setSeasonTitle(cursor.getString(index++));
            builder.setEpisodeNumber(cursor.getString(index++));
            builder.setRecordingProhibited(cursor.getInt(index++) == 1);
        } else {
            builder.setSeasonNumber(cursor.getString(index++));
            builder.setEpisodeNumber(cursor.getString(index++));
        }
        if (TvProviderUtils.getProgramHasSeriesIdColumn()) {
            String seriesId = cursor.getString(index);
            if (!TextUtils.isEmpty(seriesId)) {
                builder.setSeriesId(seriesId);
            }
        }
        return builder.build();
    }

    /** Creates {@code Program} object from cursor. */
    public static Program fromCursorPartialProjection(Cursor cursor) {
        // Columns read must match the order of match {@link #PARTIAL_PROJECTION}
        Builder builder = new Builder();
        int index = 0;
        builder.setId(cursor.getLong(index++));
        builder.setChannelId(cursor.getLong(index++));
        builder.setTitle(cursor.getString(index++));
        builder.setEpisodeTitle(cursor.getString(index++));
        builder.setCanonicalGenres(cursor.getString(index++));
        builder.setStartTimeUtcMillis(cursor.getLong(index++));
        builder.setEndTimeUtcMillis(cursor.getLong(index++));
        return builder.build();
    }

    public static ProgramImpl fromParcel(Parcel in) {
        ProgramImpl program = new ProgramImpl();
        program.mId = in.readLong();
        program.mPackageName = in.readString();
        program.mChannelId = in.readLong();
        program.mTitle = in.readString();
        program.mSeriesId = in.readString();
        program.mEpisodeTitle = in.readString();
        program.mSeasonNumber = in.readString();
        program.mSeasonTitle = in.readString();
        program.mEpisodeNumber = in.readString();
        program.mStartTimeUtcMillis = in.readLong();
        program.mEndTimeUtcMillis = in.readLong();
        program.mDescription = in.readString();
        program.mLongDescription = in.readString();
        program.mVideoWidth = in.readInt();
        program.mVideoHeight = in.readInt();
        program.mCriticScores = in.readArrayList(Thread.currentThread().getContextClassLoader());
        program.mPosterArtUri = in.readString();
        program.mThumbnailUri = in.readString();
        program.mCanonicalGenreIds = in.createIntArray();
        int length = in.readInt();
        if (length > 0) {
            ImmutableList.Builder<TvContentRating> ratingsBuilder =
                    ImmutableList.builderWithExpectedSize(length);
            for (int i = 0; i < length; ++i) {
                ratingsBuilder.add(TvContentRating.unflattenFromString(in.readString()));
            }
            program.mContentRatings = ratingsBuilder.build();
        } else {
            program.mContentRatings = ImmutableList.of();
        }
        program.mRecordingProhibited = in.readByte() != (byte) 0;
        return program;
    }

    public static final Parcelable.Creator<Program> CREATOR =
            new Parcelable.Creator<Program>() {
                @Override
                public Program createFromParcel(Parcel in) {
                    return ProgramImpl.fromParcel(in);
                }

                @Override
                public Program[] newArray(int size) {
                    return new Program[size];
                }
            };

    private long mId;
    private String mPackageName;
    private long mChannelId;
    private String mTitle;
    private String mSeriesId;
    private String mEpisodeTitle;
    private String mSeasonNumber;
    private String mSeasonTitle;
    private String mEpisodeNumber;
    private long mStartTimeUtcMillis;
    private long mEndTimeUtcMillis;
    private String mDurationString;
    private String mDescription;
    private String mLongDescription;
    private int mVideoWidth;
    private int mVideoHeight;
    private List<CriticScore> mCriticScores;
    private String mPosterArtUri;
    private String mThumbnailUri;
    private int[] mCanonicalGenreIds;
    private ImmutableList<TvContentRating> mContentRatings;
    private boolean mRecordingProhibited;

    private ProgramImpl() {
        // Do nothing.
    }

    @Override
    public long getId() {
        return mId;
    }

    @Override
    public String getPackageName() {
        return mPackageName;
    }

    @Override
    public long getChannelId() {
        return mChannelId;
    }

    /** Returns {@code true} if this program is valid or {@code false} otherwise. */
    @Override
    public boolean isValid() {
        return mChannelId >= 0;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    /** Returns the series ID. */
    @Override
    public String getSeriesId() {
        return mSeriesId;
    }

    /** Returns the episode title. */
    @Override
    public String getEpisodeTitle() {
        return mEpisodeTitle;
    }

    @Override
    public String getSeasonNumber() {
        return mSeasonNumber;
    }

    @Override
    public String getSeasonTitle() {
        return mSeasonTitle;
    }

    @Override
    public String getEpisodeNumber() {
        return mEpisodeNumber;
    }

    @Override
    public long getStartTimeUtcMillis() {
        return mStartTimeUtcMillis;
    }

    @Override
    public long getEndTimeUtcMillis() {
        return mEndTimeUtcMillis;
    }

    @Override
    public String getDurationString(Context context) {
        // TODO(b/71717446): expire the calculated string
        if (mDurationString == null) {
            mDurationString =
                    Utils.getDurationString(context, mStartTimeUtcMillis, mEndTimeUtcMillis, true);
        }
        return mDurationString;
    }

    /** Returns the program duration. */
    @Override
    public long getDurationMillis() {
        return mEndTimeUtcMillis - mStartTimeUtcMillis;
    }

    @Override
    public String getDescription() {
        return mDescription;
    }

    @Override
    public String getLongDescription() {
        return mLongDescription;
    }

    @Override
    public int getVideoWidth() {
        return mVideoWidth;
    }

    @Override
    public int getVideoHeight() {
        return mVideoHeight;
    }

    @Override
    @Nullable
    public List<CriticScore> getCriticScores() {
        return mCriticScores;
    }

    @Nullable
    @Override
    public ImmutableList<TvContentRating> getContentRatings() {
        return mContentRatings;
    }

    @Override
    public String getPosterArtUri() {
        return mPosterArtUri;
    }

    @Override
    public String getThumbnailUri() {
        return mThumbnailUri;
    }

    @Override
    public boolean isRecordingProhibited() {
        return mRecordingProhibited;
    }

    @Override
    @Nullable
    public String[] getCanonicalGenres() {
        if (mCanonicalGenreIds == null) {
            return null;
        }
        String[] genres = new String[mCanonicalGenreIds.length];
        for (int i = 0; i < mCanonicalGenreIds.length; i++) {
            genres[i] = GenreItems.getCanonicalGenre(mCanonicalGenreIds[i]);
        }
        return genres;
    }

    /** Returns array of canonical genre ID's for this program. */
    @Override
    public int[] getCanonicalGenreIds() {
        return mCanonicalGenreIds;
    }

    @Override
    public boolean hasGenre(int genreId) {
        if (genreId == GenreItems.ID_ALL_CHANNELS) {
            return true;
        }
        if (mCanonicalGenreIds != null) {
            for (int id : mCanonicalGenreIds) {
                if (id == genreId) {
                    return true;
                }
            }
        }
        return false;
    }

    @Override
    public int hashCode() {
        // Hash with all the properties because program ID can be invalid for the dummy programs.
        return Objects.hash(
                mChannelId,
                mStartTimeUtcMillis,
                mEndTimeUtcMillis,
                mTitle,
                mSeriesId,
                mEpisodeTitle,
                mDescription,
                mLongDescription,
                mVideoWidth,
                mVideoHeight,
                mPosterArtUri,
                mThumbnailUri,
                mContentRatings,
                Arrays.hashCode(mCanonicalGenreIds),
                mSeasonNumber,
                mSeasonTitle,
                mEpisodeNumber,
                mRecordingProhibited);
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof ProgramImpl)) {
            return false;
        }
        // Compare all the properties because program ID can be invalid for the dummy programs.
        ProgramImpl program = (ProgramImpl) other;
        return Objects.equals(mPackageName, program.mPackageName)
                && mChannelId == program.mChannelId
                && mStartTimeUtcMillis == program.mStartTimeUtcMillis
                && mEndTimeUtcMillis == program.mEndTimeUtcMillis
                && Objects.equals(mTitle, program.mTitle)
                && Objects.equals(mSeriesId, program.mSeriesId)
                && Objects.equals(mEpisodeTitle, program.mEpisodeTitle)
                && Objects.equals(mDescription, program.mDescription)
                && Objects.equals(mLongDescription, program.mLongDescription)
                && mVideoWidth == program.mVideoWidth
                && mVideoHeight == program.mVideoHeight
                && Objects.equals(mPosterArtUri, program.mPosterArtUri)
                && Objects.equals(mThumbnailUri, program.mThumbnailUri)
                && Objects.equals(mContentRatings, program.mContentRatings)
                && Arrays.equals(mCanonicalGenreIds, program.mCanonicalGenreIds)
                && Objects.equals(mSeasonNumber, program.mSeasonNumber)
                && Objects.equals(mSeasonTitle, program.mSeasonTitle)
                && Objects.equals(mEpisodeNumber, program.mEpisodeNumber)
                && mRecordingProhibited == program.mRecordingProhibited;
    }

    @Override
    public int compareTo(@NonNull Program other) {
        return Long.compare(mStartTimeUtcMillis, other.getStartTimeUtcMillis());
    }

    @Override
    public String toString() {
        StringBuilder builder = new StringBuilder();
        builder.append("Program[")
                .append(mId)
                .append("]{channelId=")
                .append(mChannelId)
                .append(", packageName=")
                .append(mPackageName)
                .append(", title=")
                .append(mTitle)
                .append(", seriesId=")
                .append(mSeriesId)
                .append(", episodeTitle=")
                .append(mEpisodeTitle)
                .append(", seasonNumber=")
                .append(mSeasonNumber)
                .append(", seasonTitle=")
                .append(mSeasonTitle)
                .append(", episodeNumber=")
                .append(mEpisodeNumber)
                .append(", startTimeUtcSec=")
                .append(Utils.toTimeString(mStartTimeUtcMillis))
                .append(", endTimeUtcSec=")
                .append(Utils.toTimeString(mEndTimeUtcMillis))
                .append(", videoWidth=")
                .append(mVideoWidth)
                .append(", videoHeight=")
                .append(mVideoHeight)
                .append(", contentRatings=")
                .append(TvContentRatingCache.contentRatingsToString(mContentRatings))
                .append(", posterArtUri=")
                .append(mPosterArtUri)
                .append(", thumbnailUri=")
                .append(mThumbnailUri)
                .append(", canonicalGenres=")
                .append(Arrays.toString(mCanonicalGenreIds))
                .append(", recordingProhibited=")
                .append(mRecordingProhibited);
        if (DEBUG_DUMP_DESCRIPTION) {
            builder.append(", description=")
                    .append(mDescription)
                    .append(", longDescription=")
                    .append(mLongDescription);
        }
        return builder.append("}").toString();
    }

    /**
     * Translates a {@link ProgramImpl} to {@link ContentValues} that are ready to be written into
     * Database.
     */
    @SuppressLint("InlinedApi")
    @SuppressWarnings("deprecation")
    @WorkerThread
    public static ContentValues toContentValues(Program program, Context context) {
        ContentValues values = new ContentValues();
        values.put(TvContract.Programs.COLUMN_CHANNEL_ID, program.getChannelId());
        if (!TextUtils.isEmpty(program.getPackageName())) {
            values.put(Programs.COLUMN_PACKAGE_NAME, program.getPackageName());
        }
        putValue(values, TvContract.Programs.COLUMN_TITLE, program.getTitle());
        putValue(values, TvContract.Programs.COLUMN_EPISODE_TITLE, program.getEpisodeTitle());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            putValue(
                    values,
                    TvContract.Programs.COLUMN_SEASON_DISPLAY_NUMBER,
                    program.getSeasonNumber());
            putValue(
                    values,
                    TvContract.Programs.COLUMN_EPISODE_DISPLAY_NUMBER,
                    program.getEpisodeNumber());
        } else {
            putValue(values, TvContract.Programs.COLUMN_SEASON_NUMBER, program.getSeasonNumber());
            putValue(values, TvContract.Programs.COLUMN_EPISODE_NUMBER, program.getEpisodeNumber());
        }
        if (TvProviderUtils.checkSeriesIdColumn(context, Programs.CONTENT_URI)) {
            putValue(values, COLUMN_SERIES_ID, program.getSeriesId());
        }

        putValue(values, TvContract.Programs.COLUMN_SHORT_DESCRIPTION, program.getDescription());
        putValue(values, TvContract.Programs.COLUMN_LONG_DESCRIPTION, program.getLongDescription());
        putValue(values, TvContract.Programs.COLUMN_POSTER_ART_URI, program.getPosterArtUri());
        putValue(values, TvContract.Programs.COLUMN_THUMBNAIL_URI, program.getThumbnailUri());
        String[] canonicalGenres = program.getCanonicalGenres();
        if (canonicalGenres != null && canonicalGenres.length > 0) {
            putValue(
                    values,
                    TvContract.Programs.COLUMN_CANONICAL_GENRE,
                    TvContract.Programs.Genres.encode(canonicalGenres));
        } else {
            putValue(values, TvContract.Programs.COLUMN_CANONICAL_GENRE, "");
        }
        putValue(
                values,
                Programs.COLUMN_CONTENT_RATING,
                TvContentRatingCache.contentRatingsToString(program.getContentRatings()));
        values.put(
                TvContract.Programs.COLUMN_START_TIME_UTC_MILLIS, program.getStartTimeUtcMillis());
        values.put(TvContract.Programs.COLUMN_END_TIME_UTC_MILLIS, program.getEndTimeUtcMillis());
        putValue(
                values,
                TvContract.Programs.COLUMN_INTERNAL_PROVIDER_DATA,
                InternalDataUtils.serializeInternalProviderData(program));
        return values;
    }

    private static void putValue(ContentValues contentValues, String key, String value) {
        if (TextUtils.isEmpty(value)) {
            contentValues.putNull(key);
        } else {
            contentValues.put(key, value);
        }
    }

    private static void putValue(ContentValues contentValues, String key, byte[] value) {
        if (value == null || value.length == 0) {
            contentValues.putNull(key);
        } else {
            contentValues.put(key, value);
        }
    }

    public void copyFrom(Program other) {
        if (this == other) {
            return;
        }

        mId = other.getId();
        mPackageName = other.getPackageName();
        mChannelId = other.getChannelId();
        mTitle = other.getTitle();
        mSeriesId = other.getSeriesId();
        mEpisodeTitle = other.getEpisodeTitle();
        mSeasonNumber = other.getSeasonNumber();
        mSeasonTitle = other.getSeasonTitle();
        mEpisodeNumber = other.getEpisodeNumber();
        mStartTimeUtcMillis = other.getStartTimeUtcMillis();
        mEndTimeUtcMillis = other.getEndTimeUtcMillis();
        mDurationString = null; // Recreate Duration when needed.
        mDescription = other.getDescription();
        mLongDescription = other.getLongDescription();
        mVideoWidth = other.getVideoWidth();
        mVideoHeight = other.getVideoHeight();
        mCriticScores = other.getCriticScores();
        mPosterArtUri = other.getPosterArtUri();
        mThumbnailUri = other.getThumbnailUri();
        mCanonicalGenreIds = other.getCanonicalGenreIds();
        mContentRatings = other.getContentRatings();
        mRecordingProhibited = other.isRecordingProhibited();
    }

    /** A Builder for the Program class */
    public static final class Builder {
        private final ProgramImpl mProgram;

        /** Creates a Builder for this Program class */
        public Builder() {
            mProgram = new ProgramImpl();
            // Fill initial data.
            mProgram.mPackageName = null;
            mProgram.mChannelId = Channel.INVALID_ID;
            mProgram.mTitle = null;
            mProgram.mSeasonNumber = null;
            mProgram.mSeasonTitle = null;
            mProgram.mEpisodeNumber = null;
            mProgram.mStartTimeUtcMillis = -1;
            mProgram.mEndTimeUtcMillis = -1;
            mProgram.mDurationString = null;
            mProgram.mDescription = null;
            mProgram.mLongDescription = null;
            mProgram.mRecordingProhibited = false;
            mProgram.mCriticScores = null;
        }

        /**
         * Creates a builder for this Program class by setting default values equivalent to another
         * Program
         *
         * @param other the program to be copied
         */
        @VisibleForTesting
        public Builder(Program other) {
            mProgram = new ProgramImpl();
            mProgram.copyFrom(other);
        }

        /**
         * Sets the ID of this program
         *
         * @param id the ID
         * @return a reference to this object
         */
        public Builder setId(long id) {
            mProgram.mId = id;
            return this;
        }

        /**
         * Sets the package name for this program
         *
         * @param packageName the package name
         * @return a reference to this object
         */
        public Builder setPackageName(String packageName) {
            mProgram.mPackageName = packageName;
            return this;
        }

        /**
         * Sets the channel ID for this program
         *
         * @param channelId the channel ID
         * @return a reference to this object
         */
        public Builder setChannelId(long channelId) {
            mProgram.mChannelId = channelId;
            return this;
        }

        /**
         * Sets the program title
         *
         * @param title the title
         * @return a reference to this object
         */
        public Builder setTitle(String title) {
            mProgram.mTitle = title;
            return this;
        }

        /**
         * Sets the series ID.
         *
         * @param seriesId the series ID
         * @return a reference to this object
         */
        public Builder setSeriesId(String seriesId) {
            mProgram.mSeriesId = seriesId;
            return this;
        }

        /**
         * Sets the episode title if this is a series program
         *
         * @param episodeTitle the episode title
         * @return a reference to this object
         */
        public Builder setEpisodeTitle(String episodeTitle) {
            mProgram.mEpisodeTitle = episodeTitle;
            return this;
        }

        /**
         * Sets the season number if this is a series program
         *
         * @param seasonNumber the season number
         * @return a reference to this object
         */
        public Builder setSeasonNumber(String seasonNumber) {
            mProgram.mSeasonNumber = seasonNumber;
            return this;
        }

        /**
         * Sets the season title if this is a series program
         *
         * @param seasonTitle the season title
         * @return a reference to this object
         */
        public Builder setSeasonTitle(String seasonTitle) {
            mProgram.mSeasonTitle = seasonTitle;
            return this;
        }

        /**
         * Sets the episode number if this is a series program
         *
         * @param episodeNumber the episode number
         * @return a reference to this object
         */
        public Builder setEpisodeNumber(String episodeNumber) {
            mProgram.mEpisodeNumber = episodeNumber;
            return this;
        }

        /**
         * Sets the start time of this program
         *
         * @param startTimeUtcMillis the start time in UTC milliseconds
         * @return a reference to this object
         */
        public Builder setStartTimeUtcMillis(long startTimeUtcMillis) {
            mProgram.mStartTimeUtcMillis = startTimeUtcMillis;
            return this;
        }

        /**
         * Sets the end time of this program
         *
         * @param endTimeUtcMillis the end time in UTC milliseconds
         * @return a reference to this object
         */
        public Builder setEndTimeUtcMillis(long endTimeUtcMillis) {
            mProgram.mEndTimeUtcMillis = endTimeUtcMillis;
            return this;
        }

        /**
         * Sets a description
         *
         * @param description the description
         * @return a reference to this object
         */
        public Builder setDescription(String description) {
            mProgram.mDescription = description;
            return this;
        }

        /**
         * Sets a long description
         *
         * @param longDescription the long description
         * @return a reference to this object
         */
        public Builder setLongDescription(String longDescription) {
            mProgram.mLongDescription = longDescription;
            return this;
        }

        /**
         * Defines the video width of this program
         *
         * @param width
         * @return a reference to this object
         */
        public Builder setVideoWidth(int width) {
            mProgram.mVideoWidth = width;
            return this;
        }

        /**
         * Defines the video height of this program
         *
         * @param height
         * @return a reference to this object
         */
        public Builder setVideoHeight(int height) {
            mProgram.mVideoHeight = height;
            return this;
        }

        /**
         * Sets the content ratings for this program
         *
         * @param contentRatings the content ratings
         * @return a reference to this object
         */
        public Builder setContentRatings(ImmutableList<TvContentRating> contentRatings) {
            mProgram.mContentRatings = contentRatings;
            return this;
        }

        /**
         * Sets the poster art URI
         *
         * @param posterArtUri the poster art URI
         * @return a reference to this object
         */
        public Builder setPosterArtUri(String posterArtUri) {
            mProgram.mPosterArtUri = posterArtUri;
            return this;
        }

        /**
         * Sets the thumbnail URI
         *
         * @param thumbnailUri the thumbnail URI
         * @return a reference to this object
         */
        public Builder setThumbnailUri(String thumbnailUri) {
            mProgram.mThumbnailUri = thumbnailUri;
            return this;
        }

        /**
         * Sets the canonical genres by id
         *
         * @param genres the genres
         * @return a reference to this object
         */
        public Builder setCanonicalGenres(String genres) {
            mProgram.mCanonicalGenreIds = Utils.getCanonicalGenreIds(genres);
            return this;
        }

        /**
         * Sets the recording prohibited flag
         *
         * @param recordingProhibited recording prohibited flag
         * @return a reference to this object
         */
        public Builder setRecordingProhibited(boolean recordingProhibited) {
            mProgram.mRecordingProhibited = recordingProhibited;
            return this;
        }

        /**
         * Adds a critic score
         *
         * @param criticScore the critic score
         * @return a reference to this object
         */
        public Builder addCriticScore(CriticScore criticScore) {
            if (criticScore.score != null) {
                if (mProgram.mCriticScores == null) {
                    mProgram.mCriticScores = new ArrayList<>();
                }
                mProgram.mCriticScores.add(criticScore);
            }
            return this;
        }

        /**
         * Sets the critic scores
         *
         * @param criticScores the critic scores
         * @return a reference to this objects
         */
        public Builder setCriticScores(List<CriticScore> criticScores) {
            mProgram.mCriticScores = criticScores;
            return this;
        }

        /**
         * Returns a reference to the Program object being constructed
         *
         * @return the Program object constructed
         */
        public ProgramImpl build() {
            // Generate the series ID for the episodic program of other TV input.
            if (TextUtils.isEmpty(mProgram.mTitle)) {
                // If title is null, series cannot be generated for this program.
                setSeriesId(null);
            } else if (TextUtils.isEmpty(mProgram.mSeriesId)
                    && !TextUtils.isEmpty(mProgram.mEpisodeNumber)) {
                // If series ID is not set, generate it for the episodic program of other TV input.
                setSeriesId(BaseProgram.generateSeriesId(mProgram.mPackageName, mProgram.mTitle));
            }
            ProgramImpl program = new ProgramImpl();
            program.copyFrom(mProgram);
            return program;
        }
    }

    @Override
    public void prefetchPosterArt(Context context, int posterArtWidth, int posterArtHeight) {
        if (mPosterArtUri == null) {
            return;
        }
        ImageLoader.prefetchBitmap(context, mPosterArtUri, posterArtWidth, posterArtHeight);
    }

    @Override
    @UiThread
    public boolean loadPosterArt(
            Context context,
            int posterArtWidth,
            int posterArtHeight,
            ImageLoader.ImageLoaderCallback<?> callback) {
        if (mPosterArtUri == null) {
            return false;
        }
        return ImageLoader.loadBitmap(
                context, mPosterArtUri, posterArtWidth, posterArtHeight, callback);
    }

    @Override
    public Parcelable toParcelable() {
        return this;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel out, int paramInt) {
        out.writeLong(mId);
        out.writeString(mPackageName);
        out.writeLong(mChannelId);
        out.writeString(mTitle);
        out.writeString(mSeriesId);
        out.writeString(mEpisodeTitle);
        out.writeString(mSeasonNumber);
        out.writeString(mSeasonTitle);
        out.writeString(mEpisodeNumber);
        out.writeLong(mStartTimeUtcMillis);
        out.writeLong(mEndTimeUtcMillis);
        out.writeString(mDescription);
        out.writeString(mLongDescription);
        out.writeInt(mVideoWidth);
        out.writeInt(mVideoHeight);
        out.writeList(mCriticScores);
        out.writeString(mPosterArtUri);
        out.writeString(mThumbnailUri);
        out.writeIntArray(mCanonicalGenreIds);
        out.writeInt(mContentRatings == null ? 0 : mContentRatings.size());
        if (mContentRatings != null) {
            for (TvContentRating rating : mContentRatings) {
                out.writeString(rating.flattenToString());
            }
        }
        out.writeByte((byte) (mRecordingProhibited ? 1 : 0));
    }
}
