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

package android.media.cts;

import static org.junit.Assert.assertNotEquals;

import android.icu.util.ULocale;
import android.media.AudioPresentation;
import android.util.Log;

import com.android.compatibility.common.util.CtsAndroidTestCase;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

@NonMediaMainlineTest
public class AudioPresentationTest extends CtsAndroidTestCase {
    private String TAG = "AudioPresentationTest";
    private static final String REPORT_LOG_NAME = "CtsMediaTestCases";

    public void testGetters() throws Exception {
        final int PRESENTATION_ID = 42;
        final int PROGRAM_ID = 43;
        final Map<Locale, CharSequence> LABELS = generateLabels();
        final Locale LOCALE = Locale.US;
        final int MASTERING_INDICATION = AudioPresentation.MASTERED_FOR_STEREO;
        final boolean HAS_AUDIO_DESCRIPTION = false;
        final boolean HAS_SPOKEN_SUBTITLES = true;
        final boolean HAS_DIALOGUE_ENHANCEMENT = true;

        AudioPresentation presentation = (new AudioPresentation.Builder(PRESENTATION_ID)
                .setProgramId(PROGRAM_ID)
                .setLocale(ULocale.forLocale(LOCALE))
                .setLabels(localeToULocale(LABELS))
                .setMasteringIndication(MASTERING_INDICATION)
                .setHasAudioDescription(HAS_AUDIO_DESCRIPTION)
                .setHasSpokenSubtitles(HAS_SPOKEN_SUBTITLES)
                .setHasDialogueEnhancement(HAS_DIALOGUE_ENHANCEMENT)).build();
        assertEquals(PRESENTATION_ID, presentation.getPresentationId());
        assertEquals(PROGRAM_ID, presentation.getProgramId());
        assertEquals(LABELS, presentation.getLabels());
        assertEquals(LOCALE, presentation.getLocale());
        assertEquals(MASTERING_INDICATION, presentation.getMasteringIndication());
        assertEquals(HAS_AUDIO_DESCRIPTION, presentation.hasAudioDescription());
        assertEquals(HAS_SPOKEN_SUBTITLES, presentation.hasSpokenSubtitles());
        assertEquals(HAS_DIALOGUE_ENHANCEMENT, presentation.hasDialogueEnhancement());
    }

    public void testEqualsAndHashCode() throws Exception {
        final int PRESENTATION_ID = 42;
        final int PROGRAM_ID = 43;
        final Map<Locale, CharSequence> LABELS = generateLabels();
        final Locale LOCALE = Locale.US;
        final Locale LOCALE_3 = Locale.FRENCH;
        final int MASTERING_INDICATION = AudioPresentation.MASTERED_FOR_STEREO;
        final int MASTERING_INDICATION_3 = AudioPresentation.MASTERED_FOR_HEADPHONE;
        final boolean HAS_AUDIO_DESCRIPTION = false;
        final boolean HAS_SPOKEN_SUBTITLES = true;
        final boolean HAS_DIALOGUE_ENHANCEMENT = true;

        {
            AudioPresentation presentation1 = (new AudioPresentation.Builder(PRESENTATION_ID))
                    .build();
            assertEquals(presentation1, presentation1);
            assertNotEquals(presentation1, null);
            assertNotEquals(presentation1, new Object());
            AudioPresentation presentation2 = (new AudioPresentation.Builder(PRESENTATION_ID))
                    .build();
            assertEquals(presentation1, presentation2);
            assertEquals(presentation2, presentation1);
            assertEquals(presentation1.hashCode(), presentation2.hashCode());
            AudioPresentation presentation3 = (new AudioPresentation.Builder(PRESENTATION_ID + 1))
                    .build();
            assertNotEquals(presentation1, presentation3);
            assertNotEquals(presentation1.hashCode(), presentation3.hashCode());
        }
        {
            AudioPresentation presentation1 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setProgramId(PROGRAM_ID)).build();
            AudioPresentation presentation2 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setProgramId(PROGRAM_ID)).build();
            assertEquals(presentation1, presentation2);
            assertEquals(presentation2, presentation1);
            assertEquals(presentation1.hashCode(), presentation2.hashCode());
            AudioPresentation presentation3 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setProgramId(PROGRAM_ID + 1)).build();
            assertNotEquals(presentation1, presentation3);
            assertNotEquals(presentation1.hashCode(), presentation3.hashCode());
        }
        {
            AudioPresentation presentation1 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setLocale(ULocale.forLocale(LOCALE))).build();
            AudioPresentation presentation2 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setLocale(ULocale.forLocale(LOCALE))).build();
            assertEquals(presentation1, presentation2);
            assertEquals(presentation2, presentation1);
            assertEquals(presentation1.hashCode(), presentation2.hashCode());
            AudioPresentation presentation3 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setLocale(ULocale.forLocale(LOCALE_3))).build();
            assertNotEquals(presentation1, presentation3);
            assertNotEquals(presentation1.hashCode(), presentation3.hashCode());
        }
        {
            AudioPresentation presentation1 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setLabels(localeToULocale(LABELS))).build();
            AudioPresentation presentation2 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setLabels(localeToULocale(LABELS))).build();
            assertEquals(presentation1, presentation2);
            assertEquals(presentation2, presentation1);
            assertEquals(presentation1.hashCode(), presentation2.hashCode());
            AudioPresentation presentation3 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setLabels(new HashMap<ULocale, CharSequence>())).build();
            assertNotEquals(presentation1, presentation3);
            assertNotEquals(presentation1.hashCode(), presentation3.hashCode());
        }
        {
            AudioPresentation presentation1 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setMasteringIndication(MASTERING_INDICATION)).build();
            AudioPresentation presentation2 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setMasteringIndication(MASTERING_INDICATION)).build();
            assertEquals(presentation1, presentation2);
            assertEquals(presentation2, presentation1);
            assertEquals(presentation1.hashCode(), presentation2.hashCode());
            AudioPresentation presentation3 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setMasteringIndication(MASTERING_INDICATION_3)).build();
            assertNotEquals(presentation1, presentation3);
            assertNotEquals(presentation1.hashCode(), presentation3.hashCode());
        }
        {
            AudioPresentation presentation1 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setHasAudioDescription(HAS_AUDIO_DESCRIPTION)).build();
            AudioPresentation presentation2 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setHasAudioDescription(HAS_AUDIO_DESCRIPTION)).build();
            assertEquals(presentation1, presentation2);
            assertEquals(presentation2, presentation1);
            assertEquals(presentation1.hashCode(), presentation2.hashCode());
            AudioPresentation presentation3 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setHasAudioDescription(!HAS_AUDIO_DESCRIPTION)).build();
            assertNotEquals(presentation1, presentation3);
            assertNotEquals(presentation1.hashCode(), presentation3.hashCode());
        }
        {
            AudioPresentation presentation1 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setHasSpokenSubtitles(HAS_SPOKEN_SUBTITLES)).build();
            AudioPresentation presentation2 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setHasSpokenSubtitles(HAS_SPOKEN_SUBTITLES)).build();
            assertEquals(presentation1, presentation2);
            assertEquals(presentation2, presentation1);
            assertEquals(presentation1.hashCode(), presentation2.hashCode());
            AudioPresentation presentation3 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setHasSpokenSubtitles(!HAS_SPOKEN_SUBTITLES)).build();
            assertNotEquals(presentation1, presentation3);
            assertNotEquals(presentation1.hashCode(), presentation3.hashCode());
        }
        {
            AudioPresentation presentation1 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setHasDialogueEnhancement(HAS_DIALOGUE_ENHANCEMENT)).build();
            AudioPresentation presentation2 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setHasDialogueEnhancement(HAS_DIALOGUE_ENHANCEMENT)).build();
            assertEquals(presentation1, presentation2);
            assertEquals(presentation2, presentation1);
            assertEquals(presentation1.hashCode(), presentation2.hashCode());
            AudioPresentation presentation3 = (new AudioPresentation.Builder(PRESENTATION_ID)
                    .setHasDialogueEnhancement(!HAS_DIALOGUE_ENHANCEMENT)).build();
            assertNotEquals(presentation1, presentation3);
            assertNotEquals(presentation1.hashCode(), presentation3.hashCode());
        }
    }

    private static Map<Locale, CharSequence> generateLabels() {
        Map<Locale, CharSequence> result = new HashMap<Locale, CharSequence>();
        result.put(Locale.US, Locale.US.getDisplayLanguage());
        result.put(Locale.FRENCH, Locale.FRENCH.getDisplayLanguage());
        result.put(Locale.GERMAN, Locale.GERMAN.getDisplayLanguage());
        return result;
    }

    private static Map<ULocale, CharSequence> localeToULocale(Map<Locale, CharSequence> locales) {
        Map<ULocale, CharSequence> ulocaleLabels = new HashMap<ULocale, CharSequence>();
        for (Map.Entry<Locale, CharSequence> entry : locales.entrySet()) {
            ulocaleLabels.put(ULocale.forLocale(entry.getKey()), entry.getValue());
        }
        return ulocaleLabels;
    }
}
