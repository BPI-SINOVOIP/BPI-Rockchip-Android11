/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.tv.search;

import android.content.Intent;
import android.media.tv.TvContract;
import android.media.tv.TvContract.Programs;

import com.android.tv.data.api.Program;
import com.android.tv.search.LocalSearchProvider.SearchResult;

import java.util.ArrayList;
import java.util.List;

/** Fake {@link SearchInterface} for testing. */
public class FakeSearchInterface implements SearchInterface {
    public final List<Program> mPrograms = new ArrayList<>();

    @Override
    public List<SearchResult> search(String query, int limit, int action) {

        List<SearchResult> results = new ArrayList<>();
        for (Program program : mPrograms) {
            if (program.getTitle().contains(query) || program.getDescription().contains(query)) {
                results.add(fromProgram(program));
            }
        }
        return results;
    }

    public static SearchResult fromProgram(Program program) {
        SearchResult.Builder result = SearchResult.builder();
        result.setTitle(program.getTitle());
        result.setDescription(
                program.getStartTimeUtcMillis() + " - " + program.getEndTimeUtcMillis());
        result.setImageUri(program.getPosterArtUri());
        result.setIntentAction(Intent.ACTION_VIEW);
        result.setIntentData(TvContract.buildChannelUri(program.getChannelId()).toString());
        result.setIntentExtraData(TvContract.buildProgramUri(program.getId()).toString());
        result.setContentType(Programs.CONTENT_ITEM_TYPE);
        result.setIsLive(true);
        result.setVideoWidth(program.getVideoWidth());
        result.setVideoHeight(program.getVideoHeight());
        result.setDuration(program.getDurationMillis());
        result.setChannelId(program.getChannelId());
        return result.build();
    }
}
