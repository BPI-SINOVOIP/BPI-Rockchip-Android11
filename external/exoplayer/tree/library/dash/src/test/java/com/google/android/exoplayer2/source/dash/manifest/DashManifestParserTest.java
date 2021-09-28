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
package com.google.android.exoplayer2.source.dash.manifest;

import static com.google.common.truth.Truth.assertThat;

import android.net.Uri;
import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.Format;
import com.google.android.exoplayer2.metadata.emsg.EventMessage;
import com.google.android.exoplayer2.source.dash.manifest.SegmentBase.SegmentTimelineElement;
import com.google.android.exoplayer2.testutil.TestUtil;
import com.google.android.exoplayer2.util.MimeTypes;
import com.google.android.exoplayer2.util.Util;
import java.io.IOException;
import java.io.StringReader;
import java.nio.charset.Charset;
import java.util.Collections;
import java.util.List;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserFactory;

/** Unit tests for {@link DashManifestParser}. */
@RunWith(AndroidJUnit4.class)
public class DashManifestParserTest {

  private static final String SAMPLE_MPD = "mpd/sample_mpd";
  private static final String SAMPLE_MPD_UNKNOWN_MIME_TYPE = "mpd/sample_mpd_unknown_mime_type";
  private static final String SAMPLE_MPD_SEGMENT_TEMPLATE = "mpd/sample_mpd_segment_template";
  private static final String SAMPLE_MPD_EVENT_STREAM = "mpd/sample_mpd_event_stream";
  private static final String SAMPLE_MPD_LABELS = "mpd/sample_mpd_labels";
  private static final String SAMPLE_MPD_ASSET_IDENTIFIER = "mpd/sample_mpd_asset_identifier";
  private static final String SAMPLE_MPD_TEXT = "mpd/sample_mpd_text";
  private static final String SAMPLE_MPD_TRICK_PLAY = "mpd/sample_mpd_trick_play";

  private static final String NEXT_TAG_NAME = "Next";
  private static final String NEXT_TAG = "<" + NEXT_TAG_NAME + "/>";

  /** Simple test to ensure the sample manifests parse without any exceptions being thrown. */
  @Test
  public void parseMediaPresentationDescription() throws IOException {
    DashManifestParser parser = new DashManifestParser();
    parser.parse(
        Uri.parse("https://example.com/test.mpd"),
        TestUtil.getInputStream(ApplicationProvider.getApplicationContext(), SAMPLE_MPD));
    parser.parse(
        Uri.parse("https://example.com/test.mpd"),
        TestUtil.getInputStream(
            ApplicationProvider.getApplicationContext(), SAMPLE_MPD_UNKNOWN_MIME_TYPE));
  }

  @Test
  public void parseMediaPresentationDescription_segmentTemplate() throws IOException {
    DashManifestParser parser = new DashManifestParser();
    DashManifest manifest =
        parser.parse(
            Uri.parse("https://example.com/test.mpd"),
            TestUtil.getInputStream(
                ApplicationProvider.getApplicationContext(), SAMPLE_MPD_SEGMENT_TEMPLATE));

    assertThat(manifest.getPeriodCount()).isEqualTo(1);

    Period period = manifest.getPeriod(0);
    assertThat(period).isNotNull();
    assertThat(period.adaptationSets).hasSize(2);

    for (AdaptationSet adaptationSet : period.adaptationSets) {
      assertThat(adaptationSet).isNotNull();
      for (Representation representation : adaptationSet.representations) {
        if (representation instanceof Representation.MultiSegmentRepresentation) {
          Representation.MultiSegmentRepresentation multiSegmentRepresentation =
              (Representation.MultiSegmentRepresentation) representation;
          long firstSegmentIndex = multiSegmentRepresentation.getFirstSegmentNum();
          RangedUri uri = multiSegmentRepresentation.getSegmentUrl(firstSegmentIndex);
          assertThat(
                  uri.resolveUriString(representation.baseUrl)
                      .contains("redirector.googlevideo.com"))
              .isTrue();
        }
      }
    }
  }

  @Test
  public void parseMediaPresentationDescription_eventStream() throws IOException {
    DashManifestParser parser = new DashManifestParser();
    DashManifest manifest =
        parser.parse(
            Uri.parse("https://example.com/test.mpd"),
            TestUtil.getInputStream(
                ApplicationProvider.getApplicationContext(), SAMPLE_MPD_EVENT_STREAM));

    Period period = manifest.getPeriod(0);
    assertThat(period.eventStreams).hasSize(3);

    // assert text-only event stream
    EventStream eventStream1 = period.eventStreams.get(0);
    assertThat(eventStream1.events.length).isEqualTo(1);
    EventMessage expectedEvent1 =
        new EventMessage(
            "urn:uuid:XYZY",
            "call",
            10000,
            0,
            "+ 1 800 10101010".getBytes(Charset.forName(C.UTF8_NAME)));
    assertThat(eventStream1.events[0]).isEqualTo(expectedEvent1);
    assertThat(eventStream1.presentationTimesUs[0]).isEqualTo(0);

    // assert CData-structured event stream
    EventStream eventStream2 = period.eventStreams.get(1);
    assertThat(eventStream2.events.length).isEqualTo(1);
    EventMessage expectedEvent2 =
        new EventMessage(
            "urn:dvb:iptv:cpm:2014",
            "",
            1500000,
            1,
            Util.getUtf8Bytes(
                "<![CDATA[<BroadcastEvent>\n"
                    + "      <Program crid=\"crid://broadcaster.example.com/ABCDEF\"/>\n"
                    + "      <InstanceDescription>\n"
                    + "      <Title xml:lang=\"en\">The title</Title>\n"
                    + "      <Synopsis xml:lang=\"en\" length=\"medium\">"
                    + "The description</Synopsis>\n"
                    + "      <ParentalGuidance>\n"
                    + "      <mpeg7:ParentalRating href=\"urn:dvb:iptv:rating:2014:15\"/>\n"
                    + "      <mpeg7:Region>GB</mpeg7:Region>\n"
                    + "      </ParentalGuidance>\n"
                    + "      </InstanceDescription>\n"
                    + "      </BroadcastEvent>]]>"));

    assertThat(eventStream2.events[0]).isEqualTo(expectedEvent2);
    assertThat(eventStream2.presentationTimesUs[0]).isEqualTo(300000000);

    // assert xml-structured event stream
    EventStream eventStream3 = period.eventStreams.get(2);
    assertThat(eventStream3.events.length).isEqualTo(1);
    EventMessage expectedEvent3 =
        new EventMessage(
            "urn:scte:scte35:2014:xml+bin",
            "",
            1000000,
            2,
            Util.getUtf8Bytes(
                "<scte35:Signal>\n"
                    + "         <scte35:Binary>\n"
                    + "         /DAIAAAAAAAAAAAQAAZ/I0VniQAQAgBDVUVJQAAAAH+cAAAAAA==\n"
                    + "         </scte35:Binary>\n"
                    + "       </scte35:Signal>"));
    assertThat(eventStream3.events[0]).isEqualTo(expectedEvent3);
    assertThat(eventStream3.presentationTimesUs[0]).isEqualTo(1000000000);
  }

  @Test
  public void parseMediaPresentationDescription_programInformation() throws IOException {
    DashManifestParser parser = new DashManifestParser();
    DashManifest manifest =
        parser.parse(
            Uri.parse("https://example.com/test.mpd"),
            TestUtil.getInputStream(ApplicationProvider.getApplicationContext(), SAMPLE_MPD));
    ProgramInformation expectedProgramInformation =
        new ProgramInformation(
            "MediaTitle", "MediaSource", "MediaCopyright", "www.example.com", "enUs");
    assertThat(manifest.programInformation).isEqualTo(expectedProgramInformation);
  }

  @Test
  public void parseMediaPresentationDescription_labels() throws IOException {
    DashManifestParser parser = new DashManifestParser();
    DashManifest manifest =
        parser.parse(
            Uri.parse("https://example.com/test.mpd"),
            TestUtil.getInputStream(
                ApplicationProvider.getApplicationContext(), SAMPLE_MPD_LABELS));

    List<AdaptationSet> adaptationSets = manifest.getPeriod(0).adaptationSets;

    assertThat(adaptationSets.get(0).representations.get(0).format.label).isEqualTo("audio label");
    assertThat(adaptationSets.get(1).representations.get(0).format.label).isEqualTo("video label");
  }

  @Test
  public void parseMediaPresentationDescription_text() throws IOException {
    DashManifestParser parser = new DashManifestParser();
    DashManifest manifest =
        parser.parse(
            Uri.parse("https://example.com/test.mpd"),
            TestUtil.getInputStream(ApplicationProvider.getApplicationContext(), SAMPLE_MPD_TEXT));

    List<AdaptationSet> adaptationSets = manifest.getPeriod(0).adaptationSets;

    Format format = adaptationSets.get(0).representations.get(0).format;
    assertThat(format.containerMimeType).isEqualTo(MimeTypes.APPLICATION_RAWCC);
    assertThat(format.sampleMimeType).isEqualTo(MimeTypes.APPLICATION_CEA608);
    assertThat(format.codecs).isEqualTo("cea608");
    assertThat(adaptationSets.get(0).type).isEqualTo(C.TRACK_TYPE_TEXT);

    format = adaptationSets.get(1).representations.get(0).format;
    assertThat(format.containerMimeType).isEqualTo(MimeTypes.APPLICATION_MP4);
    assertThat(format.sampleMimeType).isEqualTo(MimeTypes.APPLICATION_TTML);
    assertThat(format.codecs).isEqualTo("stpp.ttml.im1t");
    assertThat(adaptationSets.get(1).type).isEqualTo(C.TRACK_TYPE_TEXT);

    format = adaptationSets.get(2).representations.get(0).format;
    assertThat(format.containerMimeType).isEqualTo(MimeTypes.APPLICATION_TTML);
    assertThat(format.sampleMimeType).isEqualTo(MimeTypes.APPLICATION_TTML);
    assertThat(format.codecs).isNull();
    assertThat(adaptationSets.get(2).type).isEqualTo(C.TRACK_TYPE_TEXT);
  }

  @Test
  public void parseMediaPresentationDescription_trickPlay() throws IOException {
    DashManifestParser parser = new DashManifestParser();
    DashManifest manifest =
        parser.parse(
            Uri.parse("https://example.com/test.mpd"),
            TestUtil.getInputStream(
                ApplicationProvider.getApplicationContext(), SAMPLE_MPD_TRICK_PLAY));

    List<AdaptationSet> adaptationSets = manifest.getPeriod(0).adaptationSets;

    AdaptationSet adaptationSet = adaptationSets.get(0);
    assertThat(adaptationSet.essentialProperties).isEmpty();
    assertThat(adaptationSet.supplementalProperties).isEmpty();
    assertThat(adaptationSet.representations.get(0).format.roleFlags).isEqualTo(0);

    adaptationSet = adaptationSets.get(1);
    assertThat(adaptationSet.essentialProperties).isEmpty();
    assertThat(adaptationSet.supplementalProperties).isEmpty();
    assertThat(adaptationSet.representations.get(0).format.roleFlags).isEqualTo(0);

    adaptationSet = adaptationSets.get(2);
    assertThat(adaptationSet.essentialProperties).hasSize(1);
    assertThat(adaptationSet.essentialProperties.get(0).schemeIdUri)
        .isEqualTo("http://dashif.org/guidelines/trickmode");
    assertThat(adaptationSet.essentialProperties.get(0).value).isEqualTo("0");
    assertThat(adaptationSet.supplementalProperties).isEmpty();
    assertThat(adaptationSet.representations.get(0).format.roleFlags)
        .isEqualTo(C.ROLE_FLAG_TRICK_PLAY);

    adaptationSet = adaptationSets.get(3);
    assertThat(adaptationSet.essentialProperties).isEmpty();
    assertThat(adaptationSet.supplementalProperties).hasSize(1);
    assertThat(adaptationSet.supplementalProperties.get(0).schemeIdUri)
        .isEqualTo("http://dashif.org/guidelines/trickmode");
    assertThat(adaptationSet.supplementalProperties.get(0).value).isEqualTo("1");
    assertThat(adaptationSet.representations.get(0).format.roleFlags)
        .isEqualTo(C.ROLE_FLAG_TRICK_PLAY);
  }

  @Test
  public void parseSegmentTimeline_repeatCount() throws Exception {
    DashManifestParser parser = new DashManifestParser();
    XmlPullParser xpp = XmlPullParserFactory.newInstance().newPullParser();
    xpp.setInput(
        new StringReader(
            "<SegmentTimeline><S d=\"96000\" r=\"2\"/><S d=\"48000\" r=\"0\"/></SegmentTimeline>"
                + NEXT_TAG));
    xpp.next();

    List<SegmentTimelineElement> elements =
        parser.parseSegmentTimeline(xpp, /* timescale= */ 48000, /* periodDurationMs= */ 10000);

    assertThat(elements)
        .containsExactly(
            new SegmentTimelineElement(/* startTime= */ 0, /* duration= */ 96000),
            new SegmentTimelineElement(/* startTime= */ 96000, /* duration= */ 96000),
            new SegmentTimelineElement(/* startTime= */ 192000, /* duration= */ 96000),
            new SegmentTimelineElement(/* startTime= */ 288000, /* duration= */ 48000))
        .inOrder();
    assertNextTag(xpp);
  }

  @Test
  public void parseSegmentTimeline_singleUndefinedRepeatCount() throws Exception {
    DashManifestParser parser = new DashManifestParser();
    XmlPullParser xpp = XmlPullParserFactory.newInstance().newPullParser();
    xpp.setInput(
        new StringReader(
            "<SegmentTimeline><S d=\"96000\" r=\"-1\"/></SegmentTimeline>" + NEXT_TAG));
    xpp.next();

    List<SegmentTimelineElement> elements =
        parser.parseSegmentTimeline(xpp, /* timescale= */ 48000, /* periodDurationMs= */ 10000);

    assertThat(elements)
        .containsExactly(
            new SegmentTimelineElement(/* startTime= */ 0, /* duration= */ 96000),
            new SegmentTimelineElement(/* startTime= */ 96000, /* duration= */ 96000),
            new SegmentTimelineElement(/* startTime= */ 192000, /* duration= */ 96000),
            new SegmentTimelineElement(/* startTime= */ 288000, /* duration= */ 96000),
            new SegmentTimelineElement(/* startTime= */ 384000, /* duration= */ 96000))
        .inOrder();
    assertNextTag(xpp);
  }

  @Test
  public void parseSegmentTimeline_timeOffsetsAndUndefinedRepeatCount() throws Exception {
    DashManifestParser parser = new DashManifestParser();
    XmlPullParser xpp = XmlPullParserFactory.newInstance().newPullParser();
    xpp.setInput(
        new StringReader(
            "<SegmentTimeline><S t=\"0\" "
                + "d=\"96000\" r=\"-1\"/><S t=\"192000\" d=\"48000\" r=\"-1\"/>"
                + "</SegmentTimeline>"
                + NEXT_TAG));
    xpp.next();

    List<SegmentTimelineElement> elements =
        parser.parseSegmentTimeline(xpp, /* timescale= */ 48000, /* periodDurationMs= */ 10000);

    assertThat(elements)
        .containsExactly(
            new SegmentTimelineElement(/* startTime= */ 0, /* duration= */ 96000),
            new SegmentTimelineElement(/* startTime= */ 96000, /* duration= */ 96000),
            new SegmentTimelineElement(/* startTime= */ 192000, /* duration= */ 48000),
            new SegmentTimelineElement(/* startTime= */ 240000, /* duration= */ 48000),
            new SegmentTimelineElement(/* startTime= */ 288000, /* duration= */ 48000),
            new SegmentTimelineElement(/* startTime= */ 336000, /* duration= */ 48000),
            new SegmentTimelineElement(/* startTime= */ 384000, /* duration= */ 48000),
            new SegmentTimelineElement(/* startTime= */ 432000, /* duration= */ 48000))
        .inOrder();
    assertNextTag(xpp);
  }

  @Test
  public void parseLabel() throws Exception {
    DashManifestParser parser = new DashManifestParser();
    XmlPullParser xpp = XmlPullParserFactory.newInstance().newPullParser();
    xpp.setInput(new StringReader("<Label>test label</Label>" + NEXT_TAG));
    xpp.next();

    String label = parser.parseLabel(xpp);
    assertThat(label).isEqualTo("test label");
    assertNextTag(xpp);
  }

  @Test
  public void parseLabel_noText() throws Exception {
    DashManifestParser parser = new DashManifestParser();
    XmlPullParser xpp = XmlPullParserFactory.newInstance().newPullParser();
    xpp.setInput(new StringReader("<Label/>" + NEXT_TAG));
    xpp.next();

    String label = parser.parseLabel(xpp);
    assertThat(label).isEqualTo("");
    assertNextTag(xpp);
  }

  @Test
  public void parseCea608AccessibilityChannel() {
    assertThat(
            DashManifestParser.parseCea608AccessibilityChannel(
                buildCea608AccessibilityDescriptors("CC1=eng")))
        .isEqualTo(1);
    assertThat(
            DashManifestParser.parseCea608AccessibilityChannel(
                buildCea608AccessibilityDescriptors("CC2=eng")))
        .isEqualTo(2);
    assertThat(
            DashManifestParser.parseCea608AccessibilityChannel(
                buildCea608AccessibilityDescriptors("CC3=eng")))
        .isEqualTo(3);
    assertThat(
            DashManifestParser.parseCea608AccessibilityChannel(
                buildCea608AccessibilityDescriptors("CC4=eng")))
        .isEqualTo(4);

    assertThat(
            DashManifestParser.parseCea608AccessibilityChannel(
                buildCea608AccessibilityDescriptors(null)))
        .isEqualTo(Format.NO_VALUE);
    assertThat(
            DashManifestParser.parseCea608AccessibilityChannel(
                buildCea608AccessibilityDescriptors("")))
        .isEqualTo(Format.NO_VALUE);
    assertThat(
            DashManifestParser.parseCea608AccessibilityChannel(
                buildCea608AccessibilityDescriptors("CC0=eng")))
        .isEqualTo(Format.NO_VALUE);
    assertThat(
            DashManifestParser.parseCea608AccessibilityChannel(
                buildCea608AccessibilityDescriptors("CC5=eng")))
        .isEqualTo(Format.NO_VALUE);
    assertThat(
            DashManifestParser.parseCea608AccessibilityChannel(
                buildCea608AccessibilityDescriptors("Wrong format")))
        .isEqualTo(Format.NO_VALUE);
  }

  @Test
  public void parseCea708AccessibilityChannel() {
    assertThat(
            DashManifestParser.parseCea708AccessibilityChannel(
                buildCea708AccessibilityDescriptors("1=lang:eng")))
        .isEqualTo(1);
    assertThat(
            DashManifestParser.parseCea708AccessibilityChannel(
                buildCea708AccessibilityDescriptors("2=lang:eng")))
        .isEqualTo(2);
    assertThat(
            DashManifestParser.parseCea708AccessibilityChannel(
                buildCea708AccessibilityDescriptors("3=lang:eng")))
        .isEqualTo(3);
    assertThat(
            DashManifestParser.parseCea708AccessibilityChannel(
                buildCea708AccessibilityDescriptors("62=lang:eng")))
        .isEqualTo(62);
    assertThat(
            DashManifestParser.parseCea708AccessibilityChannel(
                buildCea708AccessibilityDescriptors("63=lang:eng")))
        .isEqualTo(63);

    assertThat(
            DashManifestParser.parseCea708AccessibilityChannel(
                buildCea708AccessibilityDescriptors(null)))
        .isEqualTo(Format.NO_VALUE);
    assertThat(
            DashManifestParser.parseCea708AccessibilityChannel(
                buildCea708AccessibilityDescriptors("")))
        .isEqualTo(Format.NO_VALUE);
    assertThat(
            DashManifestParser.parseCea708AccessibilityChannel(
                buildCea708AccessibilityDescriptors("0=lang:eng")))
        .isEqualTo(Format.NO_VALUE);
    assertThat(
            DashManifestParser.parseCea708AccessibilityChannel(
                buildCea708AccessibilityDescriptors("64=lang:eng")))
        .isEqualTo(Format.NO_VALUE);
    assertThat(
            DashManifestParser.parseCea708AccessibilityChannel(
                buildCea708AccessibilityDescriptors("Wrong format")))
        .isEqualTo(Format.NO_VALUE);
  }

  @Test
  public void parsePeriodAssetIdentifier() throws IOException {
    DashManifestParser parser = new DashManifestParser();
    DashManifest manifest =
        parser.parse(
            Uri.parse("https://example.com/test.mpd"),
            TestUtil.getInputStream(
                ApplicationProvider.getApplicationContext(), SAMPLE_MPD_ASSET_IDENTIFIER));

    assertThat(manifest.getPeriodCount()).isEqualTo(1);

    Period period = manifest.getPeriod(0);
    assertThat(period).isNotNull();
    @Nullable Descriptor assetIdentifier = period.assetIdentifier;
    assertThat(assetIdentifier).isNotNull();

    assertThat(assetIdentifier.schemeIdUri).isEqualTo("urn:org:dashif:asset-id:2013");
    assertThat(assetIdentifier.value).isEqualTo("md:cid:EIDR:10.5240%2f0EFB-02CD-126E-8092-1E49-W");
    assertThat(assetIdentifier.id).isEqualTo("uniqueId");
  }

  private static List<Descriptor> buildCea608AccessibilityDescriptors(String value) {
    return Collections.singletonList(new Descriptor("urn:scte:dash:cc:cea-608:2015", value, null));
  }

  private static List<Descriptor> buildCea708AccessibilityDescriptors(String value) {
    return Collections.singletonList(new Descriptor("urn:scte:dash:cc:cea-708:2015", value, null));
  }

  private static void assertNextTag(XmlPullParser xpp) throws Exception {
    xpp.next();
    assertThat(xpp.getEventType()).isEqualTo(XmlPullParser.START_TAG);
    assertThat(xpp.getName()).isEqualTo(NEXT_TAG_NAME);
  }
}
