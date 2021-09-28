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
package android.ext.services.autofill;

import android.app.PendingIntent;
import android.content.IntentSender;
import android.os.Bundle;
import android.service.autofill.InlinePresentation;
import android.service.autofill.InlineSuggestionRenderService;
import android.util.Log;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.autofill.inline.Renderer;
import androidx.core.util.Preconditions;

public class InlineSuggestionRenderServiceImpl extends InlineSuggestionRenderService {

    private static final String TAG = "InlinePresentationRendererServiceImpl";

    /**
     * @hide
     */
    @Nullable
    @Override
    public View onRenderSuggestion(@NonNull InlinePresentation presentation,
            int width, int height) {
        Preconditions.checkNotNull(presentation, TAG + ": InlinePresentation should not be null");
        Log.v(TAG, "onRenderSuggestion: width=" + width + ", height=" + height);

        final Bundle style = presentation.getInlinePresentationSpec().getStyle();
        View suggestionView = null;
        PendingIntent attributionIntent = null;
        if(style != null && !style.isEmpty()) {
            suggestionView = Renderer.render(this, presentation.getSlice(), style);
            attributionIntent = Renderer.getAttributionIntent(presentation.getSlice());
        }
        if (suggestionView != null && attributionIntent != null) {
            final IntentSender attributionIntentSender = attributionIntent.getIntentSender();
            suggestionView.setOnLongClickListener((v) -> {
                startIntentSender(attributionIntentSender);
                return true;
            });
        }
        return suggestionView;
    }
}
