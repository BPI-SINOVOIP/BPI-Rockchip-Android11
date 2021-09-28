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
package android.voiceinteraction.common;

import android.app.VoiceInteractor.PickOptionRequest.Option;
import android.content.LocusId;
import android.os.Bundle;
import android.os.Parcelable;
import android.util.Log;

import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Condition;

public class Utils {
    public enum TestCaseType {
        COMPLETION_REQUEST_TEST,
        COMPLETION_REQUEST_CANCEL_TEST,
        CONFIRMATION_REQUEST_TEST,
        CONFIRMATION_REQUEST_CANCEL_TEST,
        ABORT_REQUEST_TEST,
        ABORT_REQUEST_CANCEL_TEST,
        PICKOPTION_REQUEST_TEST,
        PICKOPTION_REQUEST_CANCEL_TEST,
        COMMANDREQUEST_TEST,
        COMMANDREQUEST_CANCEL_TEST,
        SUPPORTS_COMMANDS_TEST
    }

    private static final String TAG = Utils.class.getSimpleName();

    public static final long OPERATION_TIMEOUT_MS = 5000;

    public static final String TESTCASE_TYPE = "testcase_type";
    public static final String TESTINFO = "testinfo";
    public static final String BROADCAST_INTENT = "android.intent.action.VOICE_TESTAPP";
    public static final String TEST_PROMPT = "testprompt";
    public static final String PICKOPTON_1 = "one";
    public static final String PICKOPTON_2 = "two";
    public static final String PICKOPTON_3 = "3";
    public static final String TEST_COMMAND = "test_command";
    public static final String TEST_ONCOMMAND_RESULT = "test_oncommand_result";
    public static final String TEST_ONCOMMAND_RESULT_VALUE = "test_oncommand_result value";

    public static final String CONFIRMATION_REQUEST_SUCCESS = "confirmation ok";
    public static final String COMPLETION_REQUEST_SUCCESS = "completion ok";
    public static final String ABORT_REQUEST_SUCCESS = "abort ok";
    public static final String PICKOPTION_REQUEST_SUCCESS = "pickoption ok";
    public static final String COMMANDREQUEST_SUCCESS = "commandrequest ok";
    public static final String SUPPORTS_COMMANDS_SUCCESS = "supportsCommands ok";

    public static final String CONFIRMATION_REQUEST_CANCEL_SUCCESS = "confirm cancel ok";
    public static final String COMPLETION_REQUEST_CANCEL_SUCCESS = "completion canel ok";
    public static final String ABORT_REQUEST_CANCEL_SUCCESS = "abort cancel ok";
    public static final String PICKOPTION_REQUEST_CANCEL_SUCCESS = "pickoption  cancel ok";
    public static final String COMMANDREQUEST_CANCEL_SUCCESS = "commandrequest cancel ok";
    public static final String TEST_ERROR = "Error In Test:";

    public static final String PRIVATE_OPTIONS_KEY = "private_key";
    public static final String PRIVATE_OPTIONS_VALUE = "private_value";

    public static final String DIRECT_ACTION_EXTRA_KEY = "directActionExtraKey";
    public static final String DIRECT_ACTION_EXTRA_VALUE = "directActionExtraValue";
    public static final String DIRECT_ACTION_FILE_NAME = "directActionFileName";
    public static final String DIRECT_ACTION_FILE_CONTENT = "directActionFileContent";
    public static final String DIRECT_ACTION_AUTHORITY =
            "android.voiceinteraction.testapp.fileprovider";

    public static final String DIRECT_ACTIONS_KEY_CALLBACK = "callback";
    public static final String DIRECT_ACTIONS_KEY_CANCEL_CALLBACK = "cancelCallback";
    public static final String DIRECT_ACTIONS_KEY_CONTROL = "control";
    public static final String DIRECT_ACTIONS_KEY_COMMAND = "command";
    public static final String DIRECT_ACTIONS_KEY_RESULT = "result";
    public static final String DIRECT_ACTIONS_KEY_ACTION = "action";
    public static final String DIRECT_ACTIONS_KEY_ARGUMENTS = "arguments";
    public static final String DIRECT_ACTIONS_KEY_CLASS = "class";

    public static final String DIRECT_ACTIONS_SESSION_CMD_PERFORM_ACTION = "performAction";
    public static final String DIRECT_ACTIONS_SESSION_CMD_PERFORM_ACTION_CANCEL =
            "performActionCancel";
    public static final String DIRECT_ACTIONS_SESSION_CMD_DETECT_ACTIONS_CHANGED =
            "detectActionsChanged";
    public static final String DIRECT_ACTIONS_SESSION_CMD_GET_ACTIONS = "getActions";
    public static final String DIRECT_ACTIONS_SESSION_CMD_FINISH = "hide";

    public static final String DIRECT_ACTIONS_ACTIVITY_CMD_DESTROYED_INTERACTOR =
            "destroyedInteractor";
    public static final String DIRECT_ACTIONS_ACTIVITY_CMD_FINISH = "finish";
    public static final String DIRECT_ACTIONS_ACTIVITY_CMD_INVALIDATE_ACTIONS = "invalidateActions";

    public static final String DIRECT_ACTIONS_RESULT_PERFORMED = "performed";
    public static final String DIRECT_ACTIONS_RESULT_CANCELLED = "cancelled";
    public static final String DIRECT_ACTIONS_RESULT_EXECUTING = "executing";

    public static final String DIRECT_ACTIONS_ACTION_ID = "actionId";
    public static final Bundle DIRECT_ACTIONS_ACTION_EXTRAS = new Bundle();
    static {
        DIRECT_ACTIONS_ACTION_EXTRAS.putString(DIRECT_ACTION_EXTRA_KEY,
                DIRECT_ACTION_EXTRA_VALUE);
    }
    public static final LocusId DIRECT_ACTIONS_LOCUS_ID = new LocusId("locusId");

    public static final String SERVICE_NAME =
            "android.voiceinteraction.service/.MainInteractionService";

    public static final String toBundleString(Bundle bundle) {
        if (bundle == null) {
            return "null_bundle";
        }
        StringBuffer buf = new StringBuffer("Bundle[ ");
        String testType = bundle.getString(TESTCASE_TYPE);
        boolean empty = true;
        if (testType != null) {
            empty = false;
            buf.append("testcase type = " + testType);
        }
        ArrayList<String> info = bundle.getStringArrayList(TESTINFO);
        if (info != null) {
            for (String s : info) {
                empty = false;
                buf.append(s + "\n\t\t");
            }
        } else {
            for (String key : bundle.keySet()) {
                empty = false;
                Object value = bundle.get(key);
                if (value instanceof Bundle) {
                    value = toBundleString((Bundle) value);
                }
                buf.append(key).append('=').append(value).append(' ');
            }
        }
        return empty ? "empty_bundle" : buf.append(']').toString();
    }

    public static final String toOptionsString(Option[] options) {
        StringBuilder sb = new StringBuilder();
        sb.append("{");
        for (int i = 0; i < options.length; i++) {
            if (i >= 1) {
                sb.append(", ");
            }
            sb.append(options[i].getLabel());
        }
        sb.append("}");
        return sb.toString();
    }

    public static final void addErrorResult(final Bundle testinfo, final String msg) {
        testinfo.getStringArrayList(testinfo.getString(Utils.TESTCASE_TYPE))
            .add(TEST_ERROR + " " + msg);
    }

    public static boolean await(CountDownLatch latch) {
        try {
            if (latch.await(OPERATION_TIMEOUT_MS, TimeUnit.MILLISECONDS)) return true;
            Log.e(TAG, "latch timed out");
        } catch (InterruptedException e) {
            /* ignore */
            Log.e(TAG, "Interrupted", e);
        }
        return false;
    }

    public static boolean await(Condition condition) {
        try {
            if (condition.await(OPERATION_TIMEOUT_MS, TimeUnit.MILLISECONDS)) return true;
            Log.e(TAG, "condition timed out");
        } catch (InterruptedException e) {
            /* ignore */
            Log.e(TAG, "Interrupted", e);
        }
        return false;
    }
}
