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
package com.android.tv.util;

import android.content.Context;
import android.media.tv.TvTrackInfo;
import android.os.Build;
import android.os.LocaleList;
import android.text.TextUtils;
import android.util.Log;

import com.android.tv.R;

import com.google.common.collect.Iterables;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.Set;

/** Static utilities for {@link TvTrackInfo}. */
public class TvTrackInfoUtils {

    private static final String TAG = "TvTrackInfoUtils";
    private static final int AUDIO_CHANNEL_NONE = 0;
    private static final int AUDIO_CHANNEL_MONO = 1;
    private static final int AUDIO_CHANNEL_STEREO = 2;
    private static final int AUDIO_CHANNEL_SURROUND_6 = 6;
    private static final int AUDIO_CHANNEL_SURROUND_8 = 8;

    /**
     * Compares how closely two {@link android.media.tv.TvTrackInfo}s match {@code languages},
     * {@code channelCount} and {@code id} in that precedence. A listed sorted with this comparator
     * has the worst matches first.
     *
     * @param id The track id to match.
     * @param languages The prioritized list of languages. Languages earlier in the list are a
     *     better match.
     * @param channelCount The channel count to match.
     * @return -1 if lhs is a worse match, 0 if lhs and rhs match equally and 1 if lhs is a better
     *     match.
     */
    public static Comparator<TvTrackInfo> createComparator(
            final String id, final List<String> languages, final int channelCount) {
        return (TvTrackInfo lhs, TvTrackInfo rhs) -> {
            if (Objects.equals(lhs, rhs)) {
                return 0;
            }
            if (lhs == null) {
                return -1;
            }
            if (rhs == null) {
                return 1;
            }
            // Find the first language that matches the lhs and rhs tracks. The  earlier match is
            // better. If there is no match set the index to the size of the language list since
            // its the worst match.
            int lhsLangIndex =
                    Iterables.indexOf(languages, s -> Utils.isEqualLanguage(lhs.getLanguage(), s));
            if (lhsLangIndex == -1) {
                lhsLangIndex = languages.size();
            }
            int rhsLangIndex =
                    Iterables.indexOf(languages, s -> Utils.isEqualLanguage(rhs.getLanguage(), s));
            if (rhsLangIndex == -1) {
                rhsLangIndex = languages.size();
            }
            if (lhsLangIndex != rhsLangIndex) {
                // Return the track with lower index as best
                return Integer.compare(rhsLangIndex, lhsLangIndex);
            }
            boolean lhsCountMatch =
                    lhs.getType() != TvTrackInfo.TYPE_AUDIO
                            || lhs.getAudioChannelCount() == channelCount;
            boolean rhsCountMatch =
                    rhs.getType() != TvTrackInfo.TYPE_AUDIO
                            || rhs.getAudioChannelCount() == channelCount;
            if (lhsCountMatch && rhsCountMatch) {
                return Boolean.compare(lhs.getId().equals(id), rhs.getId().equals(id));
            } else if (lhsCountMatch || rhsCountMatch) {
                return Boolean.compare(lhsCountMatch, rhsCountMatch);
            } else {
                return Integer.compare(lhs.getAudioChannelCount(), rhs.getAudioChannelCount());
            }
        };
    }

    /**
     * Selects the best TvTrackInfo available or the first if none matches.
     *
     * @param tracks The tracks to choose from
     * @param id The track id to match.
     * @param language The language to match.
     * @param channelCount The channel count to match.
     * @return the best matching track or the first one if none matches.
     */
    public static TvTrackInfo getBestTrackInfo(
            List<TvTrackInfo> tracks, String id, String language, int channelCount) {
        if (tracks == null) {
            return null;
        }
        List<String> languages = new ArrayList<>();
        if (language == null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                LocaleList locales = LocaleList.getDefault();
                for (int i = 0; i < locales.size(); i++) {
                    languages.add(locales.get(i).getLanguage());
                }
            } else {
                languages.add(Locale.getDefault().getLanguage());
            }
        } else {
            languages.add(language);
        }
        Comparator<TvTrackInfo> comparator = createComparator(id, languages, channelCount);
        TvTrackInfo best = null;
        for (TvTrackInfo track : tracks) {
            if (comparator.compare(track, best) > 0) {
                best = track;
            }
        }
        return best;
    }

    public static boolean needToShowSampleRate(Context context, List<TvTrackInfo> tracks) {
        Set<String> multiAudioStrings = new HashSet<>();
        for (TvTrackInfo track : tracks) {
            String multiAudioString = getMultiAudioString(context, track, false);
            if (multiAudioStrings.contains(multiAudioString)) {
                return true;
            }
            multiAudioStrings.add(multiAudioString);
        }
        return false;
    }

    public static String getMultiAudioString(
            Context context, TvTrackInfo track, boolean showSampleRate) {
        if (track.getType() != TvTrackInfo.TYPE_AUDIO) {
            throw new IllegalArgumentException("Not an audio track: " + toString(track));
        }
        String language = context.getString(R.string.multi_audio_unknown_language);
        if (!TextUtils.isEmpty(track.getLanguage())) {
            language = new Locale(track.getLanguage()).getDisplayName();
        } else {
            Log.d(TAG, "No language information found for the audio track: " + toString(track));
        }

        StringBuilder metadata = new StringBuilder();
        switch (track.getAudioChannelCount()) {
            case AUDIO_CHANNEL_NONE:
                break;
            case AUDIO_CHANNEL_MONO:
                metadata.append(context.getString(R.string.multi_audio_channel_mono));
                break;
            case AUDIO_CHANNEL_STEREO:
                metadata.append(context.getString(R.string.multi_audio_channel_stereo));
                break;
            case AUDIO_CHANNEL_SURROUND_6:
                metadata.append(context.getString(R.string.multi_audio_channel_surround_6));
                break;
            case AUDIO_CHANNEL_SURROUND_8:
                metadata.append(context.getString(R.string.multi_audio_channel_surround_8));
                break;
            default:
                if (track.getAudioChannelCount() > 0) {
                    metadata.append(
                            context.getString(
                                    R.string.multi_audio_channel_suffix,
                                    track.getAudioChannelCount()));
                } else {
                    Log.d(
                            TAG,
                            "Invalid audio channel count ("
                                    + track.getAudioChannelCount()
                                    + ") found for the audio track: "
                                    + toString(track));
                }
                break;
        }
        if (showSampleRate) {
            int sampleRate = track.getAudioSampleRate();
            if (sampleRate > 0) {
                if (metadata.length() > 0) {
                    metadata.append(", ");
                }
                int integerPart = sampleRate / 1000;
                int tenths = (sampleRate % 1000) / 100;
                metadata.append(integerPart);
                if (tenths != 0) {
                    metadata.append(".");
                    metadata.append(tenths);
                }
                metadata.append("kHz");
            }
        }

        if (metadata.length() == 0) {
            return language;
        }
        return context.getString(
                R.string.multi_audio_display_string_with_channel, language, metadata.toString());
    }

    private static String trackTypeToString(int trackType) {
        switch (trackType) {
            case TvTrackInfo.TYPE_AUDIO:
                return "Audio";
            case TvTrackInfo.TYPE_VIDEO:
                return "Video";
            case TvTrackInfo.TYPE_SUBTITLE:
                return "Subtitle";
            default:
                return "Invalid Type";
        }
    }

    public static String toString(TvTrackInfo info) {
        int trackType = info.getType();
        return "TvTrackInfo{"
                + "type="
                + trackTypeToString(trackType)
                + ", id="
                + info.getId()
                + ", language="
                + info.getLanguage()
                + ", description="
                + info.getDescription()
                + (trackType == TvTrackInfo.TYPE_AUDIO
                        ? (", audioChannelCount="
                                + info.getAudioChannelCount()
                                + ", audioSampleRate="
                                + info.getAudioSampleRate())
                        : "")
                + (trackType == TvTrackInfo.TYPE_VIDEO
                        ? (", videoWidth="
                                + info.getVideoWidth()
                                + ", videoHeight="
                                + info.getVideoHeight()
                                + ", videoFrameRate="
                                + info.getVideoFrameRate()
                                + ", videoPixelAspectRatio="
                                + info.getVideoPixelAspectRatio())
                        : "")
                + "}";
    }

    private TvTrackInfoUtils() {}
}
