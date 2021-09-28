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

package com.android.car.rotaryplayground;

import android.content.res.Resources;
import android.os.Bundle;
import android.util.Base64;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;
import android.widget.Button;
import android.widget.CheckBox;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import java.io.IOException;
import java.io.InputStream;

/** Fragment to demo a layout with a {@link WebView}. */
public class WebViewFragment extends Fragment {

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.rotary_web_view, container, false);
        Button toggleButtonsButton = view.findViewById(R.id.toggle_buttons);
        Button topButton = view.findViewById(R.id.top_button);
        WebView webView = view.findViewById(R.id.web_view);
        Button bottomButton = view.findViewById(R.id.bottom_button);

        toggleButtonsButton.setOnClickListener(v -> {
            int buttonVisibility =
                    topButton.getVisibility() == View.GONE ? View.VISIBLE : View.GONE;
            topButton.setVisibility(buttonVisibility);
            bottomButton.setVisibility(buttonVisibility);
        });

        Resources res = getResources();
        InputStream inputStream = res.openRawResource(R.raw.web_view_html);
        byte[] byteArray = new byte[0];
        try {
            byteArray = new byte[inputStream.available()];
            inputStream.read(byteArray);
        } catch (IOException e) {
            Log.w("WebViewFragment", "Can't read HTML");
        }
        String webViewHtml = new String(byteArray);
        String encodedHtml = Base64.encodeToString(webViewHtml.getBytes(), Base64.NO_PADDING);
        webView.loadData(encodedHtml, "text/html", "base64");
        return view;
    }
}
