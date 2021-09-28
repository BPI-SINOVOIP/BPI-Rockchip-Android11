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

package com.android.tv.twopanelsettings.slices;


import android.app.Fragment;
import android.graphics.drawable.Icon;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import com.android.tv.twopanelsettings.R;

/**
 * Fragment to display informational image and description text for slice.
 */
public class InfoFragment extends Fragment {

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.info_fragment, container, false);
        Icon image = getArguments().getParcelable(SlicesConstants.EXTRA_PREFERENCE_INFO_IMAGE);
        String text = getArguments().getString(SlicesConstants.EXTRA_PREFERENCE_INFO_TEXT);
        if (image != null) {
            ((ImageView) view.findViewById(R.id.info_image))
                    .setImageDrawable(image.loadDrawable(getContext()));
        }
        if (text != null) {
            ((TextView) view.findViewById(R.id.info_text)).setText(text);
        }
        return view;
    }
}
