/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.view.textclassifier.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.PendingIntent;
import android.app.RemoteAction;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.icu.util.ULocale;
import android.os.Bundle;
import android.os.LocaleList;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.URLSpan;
import android.view.View;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextClassifier;
import android.view.textclassifier.TextLanguage;
import android.view.textclassifier.TextLinks;
import android.view.textclassifier.TextSelection;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.google.common.collect.ImmutableMap;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.time.Instant;
import java.time.ZoneId;
import java.time.ZonedDateTime;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * TextClassifier value objects tests.
 *
 * <p>Contains unit tests for value objects passed to/from TextClassifier APIs.
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class TextClassifierValueObjectsTest {

    private static final float EPSILON = 0.000001f;
    private static final String BUNDLE_KEY = "key";
    private static final String BUNDLE_VALUE = "value";
    private static final Bundle BUNDLE = new Bundle();

    static {
        BUNDLE.putString(BUNDLE_KEY, BUNDLE_VALUE);
    }

    private static final double ACCEPTED_DELTA = 0.0000001;
    private static final String TEXT = "abcdefghijklmnopqrstuvwxyz";
    private static final int START = 5;
    private static final int END = 20;
    private static final int ANOTHER_START = 22;
    private static final int ANOTHER_END = 24;
    private static final String ID = "id123";
    private static final LocaleList LOCALES = LocaleList.forLanguageTags("fr,en,de,es");

    @Test
    public void testTextSelection() {
        final float addressScore = 0.1f;
        final float emailScore = 0.9f;

        final TextSelection selection = new TextSelection.Builder(START, END)
                .setEntityType(TextClassifier.TYPE_ADDRESS, addressScore)
                .setEntityType(TextClassifier.TYPE_EMAIL, emailScore)
                .setId(ID)
                .setExtras(BUNDLE)
                .build();

        assertEquals(START, selection.getSelectionStartIndex());
        assertEquals(END, selection.getSelectionEndIndex());
        assertEquals(2, selection.getEntityCount());
        assertEquals(TextClassifier.TYPE_EMAIL, selection.getEntity(0));
        assertEquals(TextClassifier.TYPE_ADDRESS, selection.getEntity(1));
        assertEquals(addressScore, selection.getConfidenceScore(TextClassifier.TYPE_ADDRESS),
                ACCEPTED_DELTA);
        assertEquals(emailScore, selection.getConfidenceScore(TextClassifier.TYPE_EMAIL),
                ACCEPTED_DELTA);
        assertEquals(0, selection.getConfidenceScore("random_type"), ACCEPTED_DELTA);
        assertEquals(ID, selection.getId());
        assertEquals(BUNDLE_VALUE, selection.getExtras().getString(BUNDLE_KEY));
    }

    @Test
    public void testTextSelection_differentParams() {
        final int start = 0;
        final int end = 1;
        final float confidenceScore = 0.5f;
        final String id = "2hukwu3m3k44f1gb0";

        final TextSelection selection = new TextSelection.Builder(start, end)
                .setEntityType(TextClassifier.TYPE_URL, confidenceScore)
                .setId(id)
                .build();

        assertEquals(start, selection.getSelectionStartIndex());
        assertEquals(end, selection.getSelectionEndIndex());
        assertEquals(1, selection.getEntityCount());
        assertEquals(TextClassifier.TYPE_URL, selection.getEntity(0));
        assertEquals(confidenceScore, selection.getConfidenceScore(TextClassifier.TYPE_URL),
                ACCEPTED_DELTA);
        assertEquals(0, selection.getConfidenceScore("random_type"), ACCEPTED_DELTA);
        assertEquals(id, selection.getId());
    }

    @Test
    public void testTextSelection_defaultValues() {
        TextSelection selection = new TextSelection.Builder(START, END).build();
        assertEquals(0, selection.getEntityCount());
        assertNull(selection.getId());
        assertTrue(selection.getExtras().isEmpty());
    }

    @Test
    public void testTextSelection_prunedConfidenceScore() {
        final float phoneScore = -0.1f;
        final float prunedPhoneScore = 0f;
        final float otherScore = 1.5f;
        final float prunedOtherScore = 1.0f;

        final TextSelection selection = new TextSelection.Builder(START, END)
                .setEntityType(TextClassifier.TYPE_PHONE, phoneScore)
                .setEntityType(TextClassifier.TYPE_OTHER, otherScore)
                .build();

        assertEquals(prunedPhoneScore, selection.getConfidenceScore(TextClassifier.TYPE_PHONE),
                ACCEPTED_DELTA);
        assertEquals(prunedOtherScore, selection.getConfidenceScore(TextClassifier.TYPE_OTHER),
                ACCEPTED_DELTA);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testTextSelection_invalidStartParams() {
        new TextSelection.Builder(-1 /* start */, END)
                .build();
    }

    @Test(expected = IllegalArgumentException.class)
    public void testTextSelection_invalidEndParams() {
        new TextSelection.Builder(START, 0 /* end */)
                .build();
    }

    @Test(expected = IndexOutOfBoundsException.class)
    public void testTextSelection_entityIndexOutOfBounds() {
        final TextSelection selection = new TextSelection.Builder(START, END).build();
        final int outOfBoundsIndex = selection.getEntityCount();
        selection.getEntity(outOfBoundsIndex);
    }

    @Test
    public void testTextSelectionRequest() {
        final TextSelection.Request request = new TextSelection.Request.Builder(TEXT, START, END)
                .setDefaultLocales(LOCALES)
                .setExtras(BUNDLE)
                .build();
        assertEquals(TEXT, request.getText().toString());
        assertEquals(START, request.getStartIndex());
        assertEquals(END, request.getEndIndex());
        assertEquals(LOCALES, request.getDefaultLocales());
        assertEquals(BUNDLE_VALUE, request.getExtras().getString(BUNDLE_KEY));
    }

    @Test
    public void testTextSelectionRequest_nullValues() {
        final TextSelection.Request request =
                new TextSelection.Request.Builder(TEXT, START, END)
                        .setDefaultLocales(null)
                        .build();
        assertNull(request.getDefaultLocales());
    }

    @Test
    public void testTextSelectionRequest_defaultValues() {
        final TextSelection.Request request =
                new TextSelection.Request.Builder(TEXT, START, END).build();
        assertNull(request.getDefaultLocales());
        assertTrue(request.getExtras().isEmpty());
    }

    @Test
    public void testTextClassification() {
        final float addressScore = 0.1f;
        final float emailScore = 0.9f;
        final PendingIntent intent1 = PendingIntent.getActivity(
                InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);
        final String label1 = "label1";
        final String description1 = "description1";
        final Icon icon1 = generateTestIcon(16, 16, Color.RED);
        final PendingIntent intent2 = PendingIntent.getActivity(
                InstrumentationRegistry.getTargetContext(), 0, new Intent(), 0);
        final String label2 = "label2";
        final String description2 = "description2";
        final Icon icon2 = generateTestIcon(16, 16, Color.GREEN);

        final TextClassification classification = new TextClassification.Builder()
                .setText(TEXT)
                .setEntityType(TextClassifier.TYPE_ADDRESS, addressScore)
                .setEntityType(TextClassifier.TYPE_EMAIL, emailScore)
                .addAction(new RemoteAction(icon1, label1, description1, intent1))
                .addAction(new RemoteAction(icon2, label2, description2, intent2))
                .setId(ID)
                .setExtras(BUNDLE)
                .build();

        assertEquals(TEXT, classification.getText());
        assertEquals(2, classification.getEntityCount());
        assertEquals(TextClassifier.TYPE_EMAIL, classification.getEntity(0));
        assertEquals(TextClassifier.TYPE_ADDRESS, classification.getEntity(1));
        assertEquals(addressScore, classification.getConfidenceScore(TextClassifier.TYPE_ADDRESS),
                ACCEPTED_DELTA);
        assertEquals(emailScore, classification.getConfidenceScore(TextClassifier.TYPE_EMAIL),
                ACCEPTED_DELTA);
        assertEquals(0, classification.getConfidenceScore("random_type"), ACCEPTED_DELTA);

        // Legacy API
        assertNull(classification.getIntent());
        assertNull(classification.getLabel());
        assertNull(classification.getIcon());
        assertNull(classification.getOnClickListener());

        assertEquals(2, classification.getActions().size());
        assertEquals(label1, classification.getActions().get(0).getTitle());
        assertEquals(description1, classification.getActions().get(0).getContentDescription());
        assertEquals(intent1, classification.getActions().get(0).getActionIntent());
        assertNotNull(classification.getActions().get(0).getIcon());
        assertEquals(label2, classification.getActions().get(1).getTitle());
        assertEquals(description2, classification.getActions().get(1).getContentDescription());
        assertEquals(intent2, classification.getActions().get(1).getActionIntent());
        assertNotNull(classification.getActions().get(1).getIcon());
        assertEquals(ID, classification.getId());
        assertEquals(BUNDLE_VALUE, classification.getExtras().getString(BUNDLE_KEY));
    }

    @Test
    public void testTextClassificationLegacy() {
        final float addressScore = 0.1f;
        final float emailScore = 0.9f;
        final Intent intent = new Intent();
        final String label = "label";
        final Drawable icon = new ColorDrawable(Color.RED);
        final View.OnClickListener onClick = v -> {
        };

        final TextClassification classification = new TextClassification.Builder()
                .setText(TEXT)
                .setEntityType(TextClassifier.TYPE_ADDRESS, addressScore)
                .setEntityType(TextClassifier.TYPE_EMAIL, emailScore)
                .setIntent(intent)
                .setLabel(label)
                .setIcon(icon)
                .setOnClickListener(onClick)
                .setId(ID)
                .build();

        assertEquals(TEXT, classification.getText());
        assertEquals(2, classification.getEntityCount());
        assertEquals(TextClassifier.TYPE_EMAIL, classification.getEntity(0));
        assertEquals(TextClassifier.TYPE_ADDRESS, classification.getEntity(1));
        assertEquals(addressScore, classification.getConfidenceScore(TextClassifier.TYPE_ADDRESS),
                ACCEPTED_DELTA);
        assertEquals(emailScore, classification.getConfidenceScore(TextClassifier.TYPE_EMAIL),
                ACCEPTED_DELTA);
        assertEquals(0, classification.getConfidenceScore("random_type"), ACCEPTED_DELTA);

        assertEquals(intent, classification.getIntent());
        assertEquals(label, classification.getLabel());
        assertEquals(icon, classification.getIcon());
        assertEquals(onClick, classification.getOnClickListener());
        assertEquals(ID, classification.getId());
    }

    @Test
    public void testTextClassification_defaultValues() {
        final TextClassification classification = new TextClassification.Builder().build();

        assertEquals(null, classification.getText());
        assertEquals(0, classification.getEntityCount());
        assertEquals(null, classification.getIntent());
        assertEquals(null, classification.getLabel());
        assertEquals(null, classification.getIcon());
        assertEquals(null, classification.getOnClickListener());
        assertEquals(0, classification.getActions().size());
        assertNull(classification.getId());
        assertTrue(classification.getExtras().isEmpty());
    }

    @Test
    public void testTextClassificationRequest() {
        final TextClassification.Request request =
                new TextClassification.Request.Builder(TEXT, START, END)
                        .setDefaultLocales(LOCALES)
                        .setExtras(BUNDLE)
                        .build();

        assertEquals(LOCALES, request.getDefaultLocales());
        assertEquals(BUNDLE_VALUE, request.getExtras().getString(BUNDLE_KEY));
    }

    @Test
    public void testTextClassificationRequest_nullValues() {
        final TextClassification.Request request =
                new TextClassification.Request.Builder(TEXT, START, END)
                        .setDefaultLocales(null)
                        .build();

        assertNull(request.getDefaultLocales());
        assertTrue(request.getExtras().isEmpty());
    }

    @Test
    public void testTextClassificationRequest_defaultValues() {
        final TextClassification.Request request =
                new TextClassification.Request.Builder(TEXT, START, END).build();
        assertNull(request.getDefaultLocales());
        assertTrue(request.getExtras().isEmpty());
    }

    @Test
    public void testTextLinks_defaultValues() {
        final TextLinks textLinks = new TextLinks.Builder(TEXT).build();

        assertEquals(TEXT, textLinks.getText());
        assertTrue(textLinks.getExtras().isEmpty());
        assertTrue(textLinks.getLinks().isEmpty());
    }

    @Test
    public void testTextLinks_full() {
        final TextLinks textLinks = new TextLinks.Builder(TEXT)
                .setExtras(BUNDLE)
                .addLink(START, END, Collections.singletonMap(TextClassifier.TYPE_ADDRESS, 1.0f))
                .addLink(START, END, Collections.singletonMap(TextClassifier.TYPE_PHONE, 1.0f),
                        BUNDLE)
                .build();

        assertEquals(TEXT, textLinks.getText());
        assertEquals(BUNDLE_VALUE, textLinks.getExtras().getString(BUNDLE_KEY));
        assertEquals(2, textLinks.getLinks().size());

        final List<TextLinks.TextLink> resultList = new ArrayList<>(textLinks.getLinks());
        final TextLinks.TextLink textLinkNoExtra = resultList.get(0);
        assertEquals(TextClassifier.TYPE_ADDRESS, textLinkNoExtra.getEntity(0));
        assertEquals(1.0f, textLinkNoExtra.getConfidenceScore(TextClassifier.TYPE_ADDRESS),
                EPSILON);
        assertEquals(Bundle.EMPTY, textLinkNoExtra.getExtras());

        final TextLinks.TextLink textLinkHasExtra = resultList.get(1);
        assertEquals(TextClassifier.TYPE_PHONE, textLinkHasExtra.getEntity(0));
        assertEquals(1.0f, textLinkHasExtra.getConfidenceScore(TextClassifier.TYPE_PHONE),
                EPSILON);
        assertEquals(BUNDLE_VALUE, textLinkHasExtra.getExtras().getString(BUNDLE_KEY));
    }

    @Test
    public void testTextLinks_clearTextLinks() {
        final TextLinks textLinks = new TextLinks.Builder(TEXT)
                .setExtras(BUNDLE)
                .addLink(START, END, Collections.singletonMap(TextClassifier.TYPE_ADDRESS, 1.0f))
                .clearTextLinks()
                .build();
        assertEquals(0, textLinks.getLinks().size());
    }

    @Test
    public void testTextLinks_apply() {
        final SpannableString spannableString = SpannableString.valueOf(TEXT);
        final TextLinks textLinks = new TextLinks.Builder(TEXT)
                .addLink(START, END, Collections.singletonMap(TextClassifier.TYPE_ADDRESS, 1.0f))
                .addLink(ANOTHER_START, ANOTHER_END,
                        ImmutableMap.of(TextClassifier.TYPE_PHONE, 1.0f,
                                TextClassifier.TYPE_ADDRESS, 0.5f))
                .build();

        final int status = textLinks.apply(
                spannableString, TextLinks.APPLY_STRATEGY_IGNORE, null);
        final TextLinks.TextLinkSpan[] textLinkSpans = spannableString.getSpans(0,
                spannableString.length() - 1,
                TextLinks.TextLinkSpan.class);

        assertEquals(TextLinks.STATUS_LINKS_APPLIED, status);
        assertEquals(2, textLinkSpans.length);

        final TextLinks.TextLink textLink = textLinkSpans[0].getTextLink();
        final TextLinks.TextLink anotherTextLink = textLinkSpans[1].getTextLink();

        assertEquals(START, textLink.getStart());
        assertEquals(END, textLink.getEnd());
        assertEquals(1, textLink.getEntityCount());
        assertEquals(TextClassifier.TYPE_ADDRESS, textLink.getEntity(0));
        assertEquals(1.0f, textLink.getConfidenceScore(TextClassifier.TYPE_ADDRESS),
                ACCEPTED_DELTA);
        assertEquals(ANOTHER_START, anotherTextLink.getStart());
        assertEquals(ANOTHER_END, anotherTextLink.getEnd());
        assertEquals(2, anotherTextLink.getEntityCount());
        assertEquals(TextClassifier.TYPE_PHONE, anotherTextLink.getEntity(0));
        assertEquals(1.0f, anotherTextLink.getConfidenceScore(TextClassifier.TYPE_PHONE),
                ACCEPTED_DELTA);
        assertEquals(TextClassifier.TYPE_ADDRESS, anotherTextLink.getEntity(1));
        assertEquals(0.5f, anotherTextLink.getConfidenceScore(TextClassifier.TYPE_ADDRESS),
                ACCEPTED_DELTA);
    }

    @Test
    public void testTextLinks_applyStrategyReplace() {
        final SpannableString spannableString = SpannableString.valueOf(TEXT);
        final URLSpan urlSpan = new URLSpan("http://www.google.com");
        spannableString.setSpan(urlSpan, START, END, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        final TextLinks textLinks = new TextLinks.Builder(TEXT)
                .addLink(START, END, Collections.singletonMap(TextClassifier.TYPE_ADDRESS, 1.0f))
                .build();

        final int status = textLinks.apply(
                spannableString, TextLinks.APPLY_STRATEGY_REPLACE, null);
        final TextLinks.TextLinkSpan[] textLinkSpans = spannableString.getSpans(0,
                spannableString.length() - 1,
                TextLinks.TextLinkSpan.class);
        final URLSpan[] urlSpans = spannableString.getSpans(0, spannableString.length() - 1,
                URLSpan.class);

        assertEquals(TextLinks.STATUS_LINKS_APPLIED, status);
        assertEquals(1, textLinkSpans.length);
        assertEquals(0, urlSpans.length);
    }

    @Test
    public void testTextLinks_applyStrategyIgnore() {
        final SpannableString spannableString = SpannableString.valueOf(TEXT);
        final URLSpan urlSpan = new URLSpan("http://www.google.com");
        spannableString.setSpan(urlSpan, START, END, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        final TextLinks textLinks = new TextLinks.Builder(TEXT)
                .addLink(START, END, Collections.singletonMap(TextClassifier.TYPE_ADDRESS, 1.0f))
                .build();

        final int status = textLinks.apply(
                spannableString, TextLinks.APPLY_STRATEGY_IGNORE, null);
        final TextLinks.TextLinkSpan[] textLinkSpans = spannableString.getSpans(0,
                spannableString.length() - 1,
                TextLinks.TextLinkSpan.class);
        final URLSpan[] urlSpans = spannableString.getSpans(0, spannableString.length() - 1,
                URLSpan.class);

        assertEquals(TextLinks.STATUS_NO_LINKS_APPLIED, status);
        assertEquals(0, textLinkSpans.length);
        assertEquals(1, urlSpans.length);
    }

    @Test
    public void testTextLinks_applyWithCustomSpanFactory() {
        final class CustomTextLinkSpan extends TextLinks.TextLinkSpan {
            private CustomTextLinkSpan(TextLinks.TextLink textLink) {
                super(textLink);
            }
        }
        final SpannableString spannableString = SpannableString.valueOf(TEXT);
        final TextLinks textLinks = new TextLinks.Builder(TEXT)
                .addLink(START, END, Collections.singletonMap(TextClassifier.TYPE_ADDRESS, 1.0f))
                .build();

        final int status = textLinks.apply(
                spannableString, TextLinks.APPLY_STRATEGY_IGNORE, CustomTextLinkSpan::new);
        final CustomTextLinkSpan[] customTextLinkSpans = spannableString.getSpans(0,
                spannableString.length() - 1,
                CustomTextLinkSpan.class);

        assertEquals(TextLinks.STATUS_LINKS_APPLIED, status);
        assertEquals(1, customTextLinkSpans.length);

        final TextLinks.TextLink textLink = customTextLinkSpans[0].getTextLink();

        assertEquals(START, textLink.getStart());
        assertEquals(END, textLink.getEnd());
        assertEquals(1, textLink.getEntityCount());
        assertEquals(TextClassifier.TYPE_ADDRESS, textLink.getEntity(0));
        assertEquals(1.0f, textLink.getConfidenceScore(TextClassifier.TYPE_ADDRESS),
                ACCEPTED_DELTA);
    }

    @Test
    public void testTextLinksRequest_defaultValues() {
        final TextLinks.Request request = new TextLinks.Request.Builder(TEXT).build();

        assertEquals(TEXT, request.getText());
        assertNull(request.getDefaultLocales());
        assertTrue(request.getExtras().isEmpty());
        assertNull(request.getEntityConfig());
    }

    @Test
    public void testTextLinksRequest_full() {
        final ZonedDateTime referenceTime = ZonedDateTime.ofInstant(Instant.ofEpochMilli(1000L),
                ZoneId.of("UTC"));
        final TextLinks.Request request = new TextLinks.Request.Builder(TEXT)
                .setDefaultLocales(LOCALES)
                .setExtras(BUNDLE)
                .setEntityConfig(TextClassifier.EntityConfig.createWithHints(
                        Collections.singletonList(TextClassifier.HINT_TEXT_IS_EDITABLE)))
                .setReferenceTime(referenceTime)
                .build();

        assertEquals(TEXT, request.getText());
        assertEquals(LOCALES, request.getDefaultLocales());
        assertEquals(BUNDLE_VALUE, request.getExtras().getString(BUNDLE_KEY));
        assertEquals(1, request.getEntityConfig().getHints().size());
        assertEquals(
                TextClassifier.HINT_TEXT_IS_EDITABLE,
                request.getEntityConfig().getHints().iterator().next());
        assertEquals(referenceTime, request.getReferenceTime());
    }

    @Test
    public void testTextLanguage() {
        final TextLanguage language = new TextLanguage.Builder()
                .setId(ID)
                .putLocale(ULocale.ENGLISH, 0.6f)
                .putLocale(ULocale.CHINESE, 0.3f)
                .putLocale(ULocale.JAPANESE, 0.1f)
                .setExtras(BUNDLE)
                .build();

        assertEquals(ID, language.getId());
        assertEquals(3, language.getLocaleHypothesisCount());
        assertEquals(ULocale.ENGLISH, language.getLocale(0));
        assertEquals(ULocale.CHINESE, language.getLocale(1));
        assertEquals(ULocale.JAPANESE, language.getLocale(2));
        assertEquals(0.6f, language.getConfidenceScore(ULocale.ENGLISH), EPSILON);
        assertEquals(0.3f, language.getConfidenceScore(ULocale.CHINESE), EPSILON);
        assertEquals(0.1f, language.getConfidenceScore(ULocale.JAPANESE), EPSILON);
        assertEquals(BUNDLE_VALUE, language.getExtras().getString(BUNDLE_KEY));
    }

    @Test
    public void testTextLanguage_clippedScore() {
        final TextLanguage language = new TextLanguage.Builder()
                .putLocale(ULocale.ENGLISH, 2f)
                .putLocale(ULocale.CHINESE, -2f)
                .build();

        assertEquals(1, language.getLocaleHypothesisCount());
        assertEquals(ULocale.ENGLISH, language.getLocale(0));
        assertEquals(1f, language.getConfidenceScore(ULocale.ENGLISH), EPSILON);
        assertNull(language.getId());
    }

    @Test
    public void testTextLanguageRequest() {
        final TextLanguage.Request request = new TextLanguage.Request.Builder(TEXT)
                .setExtras(BUNDLE)
                .build();

        assertEquals(TEXT, request.getText());
        assertEquals(BUNDLE_VALUE, request.getExtras().getString(BUNDLE_KEY));
        assertNull(request.getCallingPackageName());
    }

    // TODO: Add more tests.

    /** Helper to generate Icons for testing. */
    private Icon generateTestIcon(int width, int height, int colorValue) {
        final int numPixels = width * height;
        final int[] colors = new int[numPixels];
        for (int i = 0; i < numPixels; ++i) {
            colors[i] = colorValue;
        }
        final Bitmap bitmap = Bitmap.createBitmap(colors, width, height, Bitmap.Config.ARGB_8888);
        return Icon.createWithBitmap(bitmap);
    }
}
