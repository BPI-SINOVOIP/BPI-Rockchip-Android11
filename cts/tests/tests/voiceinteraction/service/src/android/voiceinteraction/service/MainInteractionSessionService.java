/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.voiceinteraction.service;

import android.content.Context;
import android.os.Bundle;
import android.service.voice.VoiceInteractionSession;
import android.service.voice.VoiceInteractionSessionService;
import android.voiceinteraction.common.Utils;

import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;

public class MainInteractionSessionService extends VoiceInteractionSessionService {
    @Override
    public VoiceInteractionSession onNewSession(Bundle args) {
        final String className = (args != null)
                ? args.getString(Utils.DIRECT_ACTIONS_KEY_CLASS) : null;
        if (className == null) {
            return new MainInteractionSession(this);
        } else {
            try {
                final Constructor<?> constructor = Class.forName(className)
                        .getConstructor(Context.class);
                return (VoiceInteractionSession) constructor.newInstance(this);
            } catch (ClassNotFoundException | NoSuchMethodException | IllegalAccessException
                    | InstantiationException | InvocationTargetException e) {
                throw new RuntimeException("Cannot instantiate class: " + className, e);
            }
        }
    }
}
