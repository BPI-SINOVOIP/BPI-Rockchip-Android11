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

package android.autofillservice.cts.inline;

import static android.autofillservice.cts.Timeouts.DATASET_PICKER_NOT_SHOWN_NAPTIME_MS;
import static android.autofillservice.cts.Timeouts.LONG_PRESS_MS;
import static android.autofillservice.cts.Timeouts.UI_TIMEOUT;

import android.autofillservice.cts.UiBot;
import android.content.pm.PackageManager;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.BySelector;
import android.support.test.uiautomator.Direction;
import android.support.test.uiautomator.UiObject2;

import com.android.compatibility.common.util.RequiredFeatureRule;
import com.android.compatibility.common.util.Timeout;
import com.android.cts.mockime.MockIme;

import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;

/**
 * UiBot for the inline suggestion.
 */
public final class InlineUiBot extends UiBot {

    private static final String TAG = "AutoFillInlineCtsUiBot";
    public static final String SUGGESTION_STRIP_DESC = "MockIme Inline Suggestion View";

    private static final BySelector SUGGESTION_STRIP_SELECTOR = By.desc(SUGGESTION_STRIP_DESC);

    private static final RequiredFeatureRule REQUIRES_IME_RULE = new RequiredFeatureRule(
            PackageManager.FEATURE_INPUT_METHODS);

    public InlineUiBot() {
        this(UI_TIMEOUT);
    }

    public InlineUiBot(Timeout defaultTimeout) {
        super(defaultTimeout);
    }

    public static RuleChain annotateRule(TestRule rule) {
        return RuleChain.outerRule(REQUIRES_IME_RULE).around(rule);
    }

    @Override
    public void assertNoDatasets() throws Exception {
        assertNoDatasetsEver();
    }

    @Override
    public void assertNoDatasetsEver() throws Exception {
        assertNeverShown("suggestion strip", SUGGESTION_STRIP_SELECTOR,
                DATASET_PICKER_NOT_SHOWN_NAPTIME_MS);
    }

    /**
     * Selects the suggestion in the {@link MockIme}'s suggestion strip by the given text.
     */
    public void selectSuggestion(String name) throws Exception {
        final UiObject2 strip = findSuggestionStrip(UI_TIMEOUT);
        final UiObject2 dataset = strip.findObject(By.text(name));
        if (dataset == null) {
            throw new AssertionError("no dataset " + name + " in " + getChildrenAsText(strip));
        }
        dataset.click();
    }

    @Override
    public void selectDataset(String name) throws Exception {
        selectSuggestion(name);
    }

    @Override
    public void longPressSuggestion(String name) throws Exception {
        final UiObject2 strip = findSuggestionStrip(UI_TIMEOUT);
        final UiObject2 dataset = strip.findObject(By.text(name));
        if (dataset == null) {
            throw new AssertionError("no dataset " + name + " in " + getChildrenAsText(strip));
        }
        dataset.click(LONG_PRESS_MS);
    }

    @Override
    public UiObject2 assertDatasets(String...names) throws Exception {
        final UiObject2 picker = findSuggestionStrip(UI_TIMEOUT);
        return assertDatasets(picker, names);
    }

    @Override
    public void assertSuggestion(String name) throws Exception {
        final UiObject2 strip = findSuggestionStrip(UI_TIMEOUT);
        final UiObject2 dataset = strip.findObject(By.text(name));
        if (dataset == null) {
            throw new AssertionError("no dataset " + name + " in " + getChildrenAsText(strip));
        }
    }

    @Override
    public void assertNoSuggestion(String name) throws Exception {
        final UiObject2 strip = findSuggestionStrip(UI_TIMEOUT);
        final UiObject2 dataset = strip.findObject(By.text(name));
        if (dataset != null) {
            throw new AssertionError("has dataset " + name + " in " + getChildrenAsText(strip));
        }
    }

    @Override
    public void scrollSuggestionView(Direction direction, int speed) throws Exception {
        final UiObject2 strip = findSuggestionStrip(UI_TIMEOUT);
        strip.fling(direction, speed);
    }

    private UiObject2 findSuggestionStrip(Timeout timeout) throws Exception {
        return waitForObject(SUGGESTION_STRIP_SELECTOR, timeout);
    }
}
