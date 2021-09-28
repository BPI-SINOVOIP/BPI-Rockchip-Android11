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
 * limitations under the License.
 */

package com.android.voicetrigger;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.service.voice.VoiceInteractionSession;

import com.android.internal.app.AssistUtils;

/**
 * The exported {@link BroadcastReceiver} which receives an Intent to trigger the current active
 * voice service. The voice service will be triggered as if the assistant button in the system UI
 * is clicked.
 *
 * Run adb shell am broadcast -a android.intent.action.VOICE_ASSIST -n
 * com.android.voicetrigger/.VoiceTriggerReceiver to use.
 */
public class VoiceTriggerReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        AssistUtils assistUtils = new AssistUtils(context);
        assistUtils.showSessionForActiveService(new Bundle(),
                VoiceInteractionSession.SHOW_SOURCE_AUTOMOTIVE_SYSTEM_UI, null, null);
    }
}
