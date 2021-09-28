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

package android.app.appops.cts;

import android.app.appops.cts.IAppOpsUserClient;

interface IAppOpsUserService {
    void disableCollectorAndCallSyncOpsWhichWillNotBeCollected(in IAppOpsUserClient client);
    void disableCollectorAndCallASyncOpsWhichWillBeCollected(in IAppOpsUserClient client);
    void callApiThatNotesSyncOpAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesSyncOpAndClearLog(in IAppOpsUserClient client);
    void callApiThatNotesSyncOpWithAttributionAndCheckLog(in IAppOpsUserClient client);
    void callApiThatCallsBackIntoServiceAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesSyncOpFromNativeCodeAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesSyncOpFromNativeCodeAndCheckMessage(in IAppOpsUserClient client);
    void callApiThatNotesSyncOpAndCheckStackTrace(in IAppOpsUserClient client);
    void callApiThatNotesNonPermissionSyncOpAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesTwiceSyncOpAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesTwoSyncOpAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesSyncOpNativelyAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesNonPermissionSyncOpNativelyAndCheckLog(in IAppOpsUserClient client);
    void callOnewayApiThatNotesSyncOpAndCheckLog(in IAppOpsUserClient client);
    void callOnewayApiThatNotesSyncOpNativelyAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesSyncOpOtherUidAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesSyncOpOtherUidNativelyAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesAsyncOpAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesAsyncOpWithAttributionAndCheckLog(in IAppOpsUserClient client);
    void callApiThatNotesAsyncOpAndCheckDefaultMessage(in IAppOpsUserClient client);
    void callApiThatNotesAsyncOpAndCheckCustomMessage(in IAppOpsUserClient client);
    void callApiThatNotesAsyncOpNativelyAndCheckCustomMessage(in IAppOpsUserClient client);
    void callApiThatNotesAsyncOpNativelyAndCheckLog(in IAppOpsUserClient client);
}
