/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.dynamicmime.testapp.preferred;

import static android.dynamicmime.common.Constants.ACTIVITY_BOTH;
import static android.dynamicmime.common.Constants.ACTIVITY_FIRST;
import static android.dynamicmime.common.Constants.APK_PREFERRED_APP;
import static android.dynamicmime.common.Constants.GROUP_FIRST;
import static android.dynamicmime.common.Constants.GROUP_SECOND;
import static android.dynamicmime.common.Constants.GROUP_THIRD;
import static android.dynamicmime.common.Constants.MIME_IMAGE_PNG;
import static android.dynamicmime.common.Constants.MIME_TEXT_PLAIN;
import static android.dynamicmime.common.Constants.PACKAGE_PREFERRED_APP;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assume.assumeTrue;

import android.content.Intent;
import android.content.res.Resources;
import android.dynamicmime.testapp.BaseDynamicMimeTest;
import android.dynamicmime.testapp.assertions.MimeGroupAssertions;
import android.dynamicmime.testapp.commands.MimeGroupCommands;
import android.dynamicmime.testapp.util.Utils;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.uiautomator.By;
import androidx.test.uiautomator.BySelector;
import androidx.test.uiautomator.Direction;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject2;
import androidx.test.uiautomator.Until;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public class PreferredActivitiesTest extends BaseDynamicMimeTest {
    private static final String ACTION = "android.dynamicmime.preferred.TEST_ACTION";

    private static final BySelector BUTTON_ALWAYS = By.res("android:id/button_always");

    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(60L);

    private TestStrategy mTest;

    public PreferredActivitiesTest() {
        super(MimeGroupCommands.preferredApp(context()), MimeGroupAssertions.notUsed());
    }

    @Before
    public void setUp() {
        Utils.installApk(APK_PREFERRED_APP);
    }

    @After
    public void tearDown() {
        super.tearDown();
        Utils.uninstallApp(PACKAGE_PREFERRED_APP);
    }

    @Test
    public void testRemoveFromGroup() {
        setStrategyAndRun(new TestStrategy() {
            @Override
            public void prepareMimeGroups() {
                addMimeTypeToGroup(GROUP_FIRST, mimeType());
            }

            @Override
            public void changeMimeGroups() {
                removeMimeTypeFromGroup(GROUP_FIRST, mimeType());
            }

            @Override
            public String preferredActivity() {
                return ACTIVITY_FIRST;
            }
        });
    }

    @Test
    public void testAddToGroup() {
        setStrategyAndRun(new TestStrategy() {
            @Override
            public void prepareMimeGroups() {
                addMimeTypeToGroup(GROUP_FIRST, mimeType());
            }

            @Override
            public void changeMimeGroups() {
                addMimeTypeToGroup(GROUP_FIRST, MIME_IMAGE_PNG);
            }

            @Override
            public void revertMimeGroupsChange() {
                removeMimeTypeFromGroup(GROUP_FIRST, MIME_IMAGE_PNG);
            }

            @Override
            public String preferredActivity() {
                return ACTIVITY_FIRST;
            }
        });
    }

    @Test
    public void testClearGroup() {
        setStrategyAndRun(new TestStrategy() {
            @Override
            public void prepareMimeGroups() {
                addMimeTypeToGroup(GROUP_FIRST, mimeType());
            }

            @Override
            public void changeMimeGroups() {
                clearMimeGroup(GROUP_FIRST);
            }

            @Override
            public String preferredActivity() {
                return ACTIVITY_FIRST;
            }
        });
    }

    @Test
    public void testModifyGroupWithoutActualGroupChanges() {
        setStrategyAndRun(new TestStrategy() {
            @Override
            public void prepareMimeGroups() {
                addMimeTypeToGroup(GROUP_FIRST, mimeType());
            }

            @Override
            public void changeMimeGroups() {
                addMimeTypeToGroup(GROUP_FIRST, mimeType());
            }

            @Override
            public String preferredActivity() {
                return ACTIVITY_FIRST;
            }

            @Override
            public boolean isActivityPreferredAfterRevert() {
                return true;
            }

            @Override
            public boolean isActivityPreferredAfterChange() {
                return true;
            }
        });
    }

    @Test
    public void testModifyGroupWithoutActualIntentFilterChanges() {
        setStrategyAndRun(new TestStrategy() {
            @Override
            public void prepareMimeGroups() {
                addMimeTypeToGroup(GROUP_THIRD, mimeType());
                addMimeTypeToGroup(GROUP_SECOND, mimeType());
            }

            @Override
            public void changeMimeGroups() {
                removeMimeTypeFromGroup(GROUP_THIRD, mimeType());
            }

            @Override
            public void revertMimeGroupsChange() {
                addMimeTypeToGroup(GROUP_THIRD, mimeType());
            }

            @Override
            public String preferredActivity() {
                return ACTIVITY_BOTH;
            }

            @Override
            public boolean isActivityPreferredAfterRevert() {
                return true;
            }

            @Override
            public boolean isActivityPreferredAfterChange() {
                return true;
            }
        });
    }

    private void setStrategyAndRun(TestStrategy test) {
        mTest = test;
        runTest();
    }

    private void runTest() {
        mTest.prepareMimeGroups();
        setPreferredActivity();

        mTest.changeMimeGroups();
        checkPreferredActivityAfterChange();

        mTest.revertMimeGroupsChange();
        checkPreferredActivityAfterRevert();

        getUiDevice().pressHome();
    }

    private void setPreferredActivity() {
        triggerResolutionDialog();

        verifyDialogIsShown(true);

        chooseActivity("TestApp" + mTest.preferredActivity());
    }

    private void checkPreferredActivityAfterChange() {
        checkPreferredActivity(mTest.isActivityPreferredAfterChange());
    }

    private void checkPreferredActivityAfterRevert() {
        checkPreferredActivity(mTest.isActivityPreferredAfterRevert());
    }

    private void checkPreferredActivity(boolean hasPreferredActivity) {
        triggerResolutionDialog();
        verifyResolutionDialog(hasPreferredActivity);
    }

    private void triggerResolutionDialog() {
        getUiDevice().pressHome();
        sendIntent(mTest.mimeType());
    }

    private void verifyResolutionDialog(boolean shouldLaunchActivity) {
        verifyDialogIsShown(!shouldLaunchActivity);
        getUiDevice().pressBack();
    }

    private void verifyDialogIsShown(boolean shouldBeShown) {
        UiObject2 buttonAlways = getUiDevice().wait(Until.findObject(BUTTON_ALWAYS), TIMEOUT);

        if (shouldBeShown) {
            assertNotNull(buttonAlways);
        } else {
            assertNull(buttonAlways);
        }
    }

    private void chooseActivity(String label) {
        findActivityInDialog(label).click();
        chooseUseAlways();

        getUiDevice().pressBack();
    }

    private UiObject2 findActivityInDialog(String label) {
        return getUiDevice().findObject(By.text(label));
    }

    private void chooseUseAlways() {
        getUiDevice()
                .wait(Until.findObject(BUTTON_ALWAYS), TIMEOUT)
                .click();
    }

    private interface TestStrategy {
        void prepareMimeGroups();

        void changeMimeGroups();

        default void revertMimeGroupsChange() {
            prepareMimeGroups();
        }

        default String mimeType() {
            return MIME_TEXT_PLAIN;
        }

        String preferredActivity();

        default boolean isActivityPreferredAfterChange() {
            return false;
        }

        default boolean isActivityPreferredAfterRevert() {
            return false;
        }
    }

    private static UiDevice getUiDevice() {
        return UiDevice.getInstance(instrumentation());
    }

    private static void sendIntent(String mimeType) {
        Intent sendIntent = new Intent();
        sendIntent.setAction(ACTION);
        sendIntent.setType(mimeType);
        sendIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        targetContext().startActivity(sendIntent, null);
    }
}
