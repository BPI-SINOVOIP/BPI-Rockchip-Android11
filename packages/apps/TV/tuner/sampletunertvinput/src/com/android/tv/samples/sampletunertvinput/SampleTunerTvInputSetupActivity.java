package com.android.tv.samples.sampletunertvinput;

import android.app.Activity;
import android.content.Intent;
import android.media.tv.TvInputInfo;
import android.os.Bundle;
import com.android.tv.testing.data.ChannelInfo;
import com.android.tv.testing.data.ChannelUtils;
import com.android.tv.testing.data.ProgramInfo;
import java.util.Collections;

/** Setup activity for SampleTunerTvInput */
public class SampleTunerTvInputSetupActivity extends Activity {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ChannelInfo channel =
            new ChannelInfo.Builder()
                .setNumber("1-1")
                .setName("Sample Channel")
                .setLogoUrl(
                    ChannelInfo.getUriStringForChannelLogo(this, 100))
                .setOriginalNetworkId(1)
                .setVideoWidth(640)
                .setVideoHeight(480)
                .setAudioChannel(2)
                .setAudioLanguageCount(1)
                .setHasClosedCaption(false)
                .setProgram(
                    new ProgramInfo(
                        "Sample Program",
                        "",
                        0,
                        0,
                        ProgramInfo.GEN_POSTER,
                        "Sample description",
                        ProgramInfo.GEN_DURATION,
                        null,
                        ProgramInfo.GEN_GENRE,
                        null))
                .build();

        Intent intent = getIntent();
        String inputId = intent.getStringExtra(TvInputInfo.EXTRA_INPUT_ID);
        ChannelUtils.updateChannels(this, inputId, Collections.singletonList(channel));
        setResult(Activity.RESULT_OK);
        finish();
    }

}
