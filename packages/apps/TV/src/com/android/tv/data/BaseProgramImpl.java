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

package com.android.tv.data;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import com.android.tv.R;
import com.android.tv.data.api.BaseProgram;

/** Base class for {@link ProgramImpl} and {@link com.android.tv.dvr.data.RecordedProgram}. */
public abstract class BaseProgramImpl implements BaseProgram {

    @Override
    @Nullable
    public String getEpisodeDisplayTitle(Context context) {
        String episodeNumber = getEpisodeNumber();
        String episodeTitle = getEpisodeTitle();
        if (!TextUtils.isEmpty(episodeNumber)) {
            episodeTitle = episodeTitle == null ? "" : episodeTitle;
            String seasonNumber = getSeasonNumber();
            if (TextUtils.isEmpty(seasonNumber) || TextUtils.equals(seasonNumber, "0")) {
                // Do not show "S0: ".
                return context.getResources()
                        .getString(
                                R.string.display_episode_title_format_no_season_number,
                                episodeNumber,
                                episodeTitle);
            } else {
                return context.getResources()
                        .getString(
                                R.string.display_episode_title_format,
                                seasonNumber,
                                episodeNumber,
                                episodeTitle);
            }
        }
        return episodeTitle;
    }

    @Override
    public String getEpisodeContentDescription(Context context) {
        String episodeNumber = getEpisodeNumber();
        String episodeTitle = getEpisodeTitle();
        if (!TextUtils.isEmpty(episodeNumber)) {
            episodeTitle = episodeTitle == null ? "" : episodeTitle;
            String seasonNumber = getSeasonNumber();
            if (TextUtils.isEmpty(seasonNumber) || TextUtils.equals(seasonNumber, "0")) {
                // Do not list season if it is empty or 0
                return context.getResources()
                        .getString(
                                R.string.content_description_episode_format_no_season_number,
                                episodeNumber,
                                episodeTitle);
            } else {
                return context.getResources()
                        .getString(
                                R.string.content_description_episode_format,
                                seasonNumber,
                                episodeNumber,
                                episodeTitle);
            }
        }
        return episodeTitle;
    }

    @Override
    public boolean isEpisodic() {
        return !TextUtils.isEmpty(getSeriesId());
    }
}
