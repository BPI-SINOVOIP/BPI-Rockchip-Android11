package com.android.tv.data.api;

import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;

import com.android.tv.util.images.ImageLoader;

import java.io.Serializable;
import java.util.List;

/** A convenience interface to create and insert program information entries into the database. */
public interface Program extends BaseProgram, Comparable<Program> {

    /** Returns {@code true} if the program is valid and {@code false} otherwise. */
    static boolean isProgramValid(Program program) {
        return program != null && program.isValid();
    }

    static boolean isDuplicate(Program p1, Program p2) {
        if (p1 == null || p2 == null) {
            return false;
        }
        return p1.getChannelId() == p2.getChannelId()
                && p1.getStartTimeUtcMillis() == p2.getStartTimeUtcMillis()
                && p1.getEndTimeUtcMillis() == p2.getEndTimeUtcMillis();
    }

    /** True if the start or end times overlap. */
    static boolean isOverlapping(@Nullable Program p1, @Nullable Program p2) {
        return p1 != null
                && p2 != null
                && p1.getStartTimeUtcMillis() < p2.getEndTimeUtcMillis()
                && p1.getEndTimeUtcMillis() > p2.getStartTimeUtcMillis();
    }

    /** True if the channels IDs are the same. */
    static boolean sameChannel(@Nullable Program p1, @Nullable Program p2) {
        return p1 != null && p2 != null && p1.getChannelId() == p2.getChannelId();
    }

    /** Returns the package name of this program. */
    String getPackageName();

    /** Returns the season title */
    String getSeasonTitle();

    /** Gets the localized duration of the program */
    String getDurationString(Context context);

    int getVideoWidth();

    int getVideoHeight();

    /** Returns the list of Critic Scores for this program */
    @Nullable
    List<CriticScore> getCriticScores();

    /** Returns {@code true} if the recording of this program is prohibited. */
    boolean isRecordingProhibited();

    /** Returns array of canonical genres for this program. This is expected to be called rarely. */
    @Nullable
    String[] getCanonicalGenres();

    /** Returns if this program has the genre. */
    boolean hasGenre(int genreId);

    /** Prefetch the program poster art. */
    void prefetchPosterArt(Context context, int posterArtWidth, int posterArtHeight);

    /**
     * Loads the program poster art and returns it via {@code callback}.
     *
     * <p>Note that it may directly call {@code callback} if the program poster art already is
     * loaded.
     *
     * @return {@code true} if the load is complete and the callback is executed.
     */
    @UiThread
    boolean loadPosterArt(
            Context context,
            int posterArtWidth,
            int posterArtHeight,
            ImageLoader.ImageLoaderCallback<?> callback);

    /** Returns a {@link Parcelable} representation of this instance. */
    Parcelable toParcelable();

    /** Holds one type of critic score and its source. */
    final class CriticScore implements Serializable, Parcelable {
        /** The source of the rating. */
        public final String source;
        /** The score. */
        public final String score;
        /** The url of the logo image */
        public final String logoUrl;

        public static final Creator<CriticScore> CREATOR =
                new Creator<CriticScore>() {
                    @Override
                    public CriticScore createFromParcel(Parcel in) {
                        String source = in.readString();
                        String score = in.readString();
                        String logoUri = in.readString();
                        return new CriticScore(source, score, logoUri);
                    }

                    @Override
                    public CriticScore[] newArray(int size) {
                        return new CriticScore[size];
                    }
                };

        /**
         * Constructor for this class.
         *
         * @param source the source of the rating
         * @param score the score
         */
        public CriticScore(String source, String score, String logoUrl) {
            this.source = source;
            this.score = score;
            this.logoUrl = logoUrl;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel out, int i) {
            out.writeString(source);
            out.writeString(score);
            out.writeString(logoUrl);
        }
    }
}
