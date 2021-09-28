/*
 * Copyright 2018 The Android Open Source Project
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

#ifndef AGQ_H
#define AGQ_H

#include <jni.h>
#include <android/native_activity.h>

namespace android {

class GameQualification {
private:
    class Impl;
    Impl* mImpl;

    GameQualification(const GameQualification& that);
public:
    GameQualification();
    ~GameQualification();

    /*
     * Signal Android Game Qualification library the start of a loop.
     *
     * For use with app with JNI.  context should be a jobject of type android.content.Context.
     */
    void startLoop(jobject context);

    /*
     * Signal Android Game Qualification library the start of a loop.
     *
     * For use with pure native app using ANativeActivity.
     */
    void startLoop(ANativeActivity* activity);
};

} // end of namespace android

#endif // AGQ_H
