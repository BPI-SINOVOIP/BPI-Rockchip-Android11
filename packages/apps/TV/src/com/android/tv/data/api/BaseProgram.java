package com.android.tv.data.api;

import android.content.Context;
import android.media.tv.TvContentRating;
import android.support.annotation.Nullable;
import com.google.common.collect.ImmutableList;
import java.util.Comparator;
import java.util.Objects;

/**
 * Base class for {@link com.android.tv.data.Program} and {@link
 * com.android.tv.dvr.data.RecordedProgram}.
 */
public interface BaseProgram {

    /**
     * Comparator used to compare {@link BaseProgram} according to its season and episodes number.
     * If a program's season or episode number is null, it will be consider "smaller" than programs
     * with season or episode numbers.
     */
    Comparator<BaseProgram> EPISODE_COMPARATOR = new EpisodeComparator(false);
    /**
     * Comparator used to compare {@link BaseProgram} according to its season and episodes number
     * with season numbers in a reversed order. If a program's season or episode number is null, it
     * will be consider "smaller" than programs with season or episode numbers.
     */
    Comparator<BaseProgram> SEASON_REVERSED_EPISODE_COMPARATOR = new EpisodeComparator(true);

    String COLUMN_SERIES_ID = "series_id";
    String COLUMN_STATE = "state";

    /** Compares two strings represent season numbers or episode numbers of programs. */
    static int numberCompare(String s1, String s2) {
        if (Objects.equals(s1, s2)) {
            return 0;
        } else if (s1 == null) {
            return -1;
        } else if (s2 == null) {
            return 1;
        } else if (s1.equals(s2)) {
            return 0;
        }
        try {
            return Integer.compare(Integer.parseInt(s1), Integer.parseInt(s2));
        } catch (NumberFormatException e) {
            return s1.compareTo(s2);
        }
    }

    /** Generates the series ID for the other inputs than the tuner TV input. */
    static String generateSeriesId(String packageName, String title) {
        return packageName + "/" + title;
    }

    /** Returns ID of the program. */
    long getId();

    /** Returns the title of the program. */
    String getTitle();

    /** Returns the episode title. */
    String getEpisodeTitle();

    /** Returns the displayed title of the program episode. */
    @Nullable
    String getEpisodeDisplayTitle(Context context);

    /**
     * Returns the content description of the program episode, suitable for being spoken by an
     * accessibility service.
     */
    String getEpisodeContentDescription(Context context);

    /** Returns the description of the program. */
    String getDescription();

    /** Returns the long description of the program. */
    String getLongDescription();

    /** Returns the start time of the program in Milliseconds. */
    long getStartTimeUtcMillis();

    /** Returns the end time of the program in Milliseconds. */
    long getEndTimeUtcMillis();

    /** Returns the duration of the program in Milliseconds. */
    long getDurationMillis();

    /** Returns the series ID. */
    @Nullable
    String getSeriesId();

    /** Returns the season number. */
    String getSeasonNumber();

    /** Returns the episode number. */
    String getEpisodeNumber();

    /** Returns URI of the program's poster. */
    String getPosterArtUri();

    /** Returns URI of the program's thumbnail. */
    String getThumbnailUri();

    /** Returns the array of the ID's of the canonical genres. */
    int[] getCanonicalGenreIds();

    /** Returns the array of content ratings. */
    ImmutableList<TvContentRating> getContentRatings();

    /** Returns channel's ID of the program. */
    long getChannelId();

    /** Returns if the program is valid. */
    boolean isValid();

    /** Checks whether the program is episodic or not. */
    boolean isEpisodic();

    /** Generates the series ID for the other inputs than the tuner TV input. */
    class EpisodeComparator implements Comparator<BaseProgram> {
        private final boolean mReversedSeason;

        EpisodeComparator(boolean reversedSeason) {
            mReversedSeason = reversedSeason;
        }

        @Override
        public int compare(BaseProgram lhs, BaseProgram rhs) {
            if (lhs == rhs) {
                return 0;
            }
            int seasonNumberCompare = numberCompare(lhs.getSeasonNumber(), rhs.getSeasonNumber());
            if (seasonNumberCompare != 0) {
                return mReversedSeason ? -seasonNumberCompare : seasonNumberCompare;
            } else {
                return numberCompare(lhs.getEpisodeNumber(), rhs.getEpisodeNumber());
            }
        }
    }
}
