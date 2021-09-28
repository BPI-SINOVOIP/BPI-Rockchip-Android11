package com.android.services.telephony;

import static junit.framework.Assert.assertEquals;

import android.os.Bundle;
import android.telecom.Connection;

import org.junit.Test;
import org.junit.runner.RunWith;

import androidx.test.runner.AndroidJUnit4;

@RunWith(AndroidJUnit4.class)
public class TelephonyConnectionTest {

    @Test
    public void testCodecInIms() {
        TestTelephonyConnection c = new TestTelephonyConnection();
        c.updateState();
        Bundle extras = c.getExtras();
        int codec = extras.getInt(Connection.EXTRA_AUDIO_CODEC, Connection.AUDIO_CODEC_NONE);
        assertEquals(codec, Connection.AUDIO_CODEC_AMR);
    }

}
