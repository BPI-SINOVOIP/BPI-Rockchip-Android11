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

package com.android.tradefed.testtype.mobly;

/** Mobly yaml result handler factory which generates appropriate handler based on result type. */
public class MoblyYamlResultHandlerFactory {

    public IMoblyYamlResultHandler getHandler(String resultType)
            throws InvalidResultTypeException, IllegalAccessException, InstantiationException {
        IMoblyYamlResultHandler resultHandler;
        switch (resultType) {
            case "Record":
                resultHandler = Type.RECORD.getHandlerInstance();
                break;
            case "UserData":
                resultHandler = Type.USER_DATA.getHandlerInstance();
                break;
            case "TestNameList":
                resultHandler = Type.TEST_NAME_LIST.getHandlerInstance();
                break;
            case "ControllerInfo":
                resultHandler = Type.CONTROLLER_INFO.getHandlerInstance();
                break;
            case "Summary":
                resultHandler = Type.SUMMARY.getHandlerInstance();
                break;
            default:
                throw new InvalidResultTypeException(
                        "Invalid mobly test result type: " + resultType);
        }
        return resultHandler;
    }

    public class InvalidResultTypeException extends Exception {
        public InvalidResultTypeException(String errorMsg) {
            super(errorMsg);
        }
    }

    public enum Type {
        RECORD("Record", MoblyYamlResultRecordHandler.class),
        USER_DATA("UserData", MoblyYamlResultUserDataHandler.class),
        TEST_NAME_LIST("TestNameList", MoblyYamlResultTestNameListHandler.class),
        CONTROLLER_INFO("ControllerInfo", MoblyYamlResultControllerInfoHandler.class),
        SUMMARY("Summary", MoblyYamlResultSummaryHandler.class);

        private String tag;
        private Class handlerClass;

        Type(String tag, Class handlerClass) {
            this.tag = tag;
            this.handlerClass = handlerClass;
        }

        public String getTag() {
            return tag;
        }

        public <T> T getHandlerInstance() throws IllegalAccessException, InstantiationException {
            return (T) handlerClass.newInstance();
        }
    }
}
