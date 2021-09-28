/*
 * Copyright (C) 2019 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.car.developeroptions.homepage.contextualcards;

import java.util.List;

/** Feature provider for the contextual card feature. */
public interface ContextualCardFeatureProvider {

    /** Homepage displays. */
    void logHomepageDisplay(long latency);

    /** When user clicks dismiss in contextual card */
    void logContextualCardDismiss(ContextualCard card);

    /** After ContextualCardManager decides which cards will be displayed/hidden */
    void logContextualCardDisplay(List<ContextualCard> showedCards,
            List<ContextualCard> hiddenCards);

    /** When user clicks toggle/title area of a contextual card. */
    void logContextualCardClick(ContextualCard card, int sliceRow, int tapTarget, int uiPosition);
}
