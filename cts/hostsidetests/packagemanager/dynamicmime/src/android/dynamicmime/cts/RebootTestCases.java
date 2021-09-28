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

package android.dynamicmime.cts;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Reboot test cases
 *
 * Reuses existing test cases from {@link android.dynamicmime.testapp.ComplexFilterTest}
 * and {@link android.dynamicmime.testapp.SingleAppTest}
 * by "inserting" device reboot between setup part (MIME group commands) and verification part
 * (MIME group assertions) in each test case
 *
 * @see android.dynamicmime.testapp.reboot.PreRebootSingleAppTest
 * @see android.dynamicmime.testapp.reboot.PostRebootSingleAppTest
 * @see #runTestWithReboot(String, String)
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class RebootTestCases extends BaseHostJUnit4Test {
    private static final String PACKAGE_TEST_APP = "android.dynamicmime.testapp";
    private static final String PACKAGE_REBOOT_TESTS = PACKAGE_TEST_APP + ".reboot";

    @Test
    public void testGroupWithExactType() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testGroupWithExactType");
    }

    @Test
    public void testGroupWithWildcardType() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testGroupWithWildcardType");
    }

    @Test
    public void testGroupWithAnyType() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testGroupWithAnyType");
    }

    @Test
    public void testAddSimilarTypes() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testAddSimilarTypes");
    }

    @Test
    public void testAddDifferentTypes() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testAddDifferentTypes");
    }

    @Test
    public void testRemoveExactType() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testRemoveExactType");
    }

    @Test
    public void testRemoveDifferentType() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testRemoveDifferentType");
    }

    @Test
    public void testRemoveSameBaseType_keepSpecific() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testRemoveSameBaseType_keepSpecific");
    }

    @Test
    public void testRemoveSameBaseType_keepWildcard() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testRemoveSameBaseType_keepWildcard");
    }

    @Test
    public void testRemoveAnyType_keepSpecific() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testRemoveAnyType_keepSpecific");
    }

    @Test
    public void testRemoveAnyType_keepWildcard() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testRemoveAnyType_keepWildcard");
    }

    @Test
    public void testResetWithoutIntersection() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testResetWithoutIntersection");
    }

    @Test
    public void testResetWithIntersection() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testResetWithIntersection");
    }

    @Test
    public void testClear() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testClear");
    }

    @Test
    public void testDefinedGroupNotNullAfterRemove() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testDefinedGroupNotNullAfterRemove");
    }

    @Test
    public void testDefinedGroupNotNullAfterClear() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testDefinedGroupNotNullAfterClear");
    }

    @Test
    public void testMimeGroupsIndependentAdd() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testMimeGroupsIndependentAdd");
    }

    @Test
    public void testMimeGroupsIndependentAddToBoth() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testMimeGroupsIndependentAddToBoth");
    }

    @Test
    public void testMimeGroupsIndependentRemove() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testMimeGroupsIndependentRemove");
    }

    @Test
    public void testMimeGroupsIndependentClear() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testMimeGroupsIndependentClear");
    }

    @Test
    public void testMimeGroupsIndependentClearBoth() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testMimeGroupsIndependentClearBoth");
    }

    @Test
    public void testMimeGroupsIndependentSet() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testMimeGroupsIndependentSet");
    }

    @Test
    public void testMimeGroupsIndependentSetBoth() throws DeviceNotAvailableException {
        runTestWithReboot("SingleAppTest", "testMimeGroupsIndependentSetBoth");
    }

    @Test
    public void testMimeGroupsNotIntersect() throws DeviceNotAvailableException {
        runTestWithReboot("ComplexFilterTest", "testMimeGroupsNotIntersect");
    }

    @Test
    public void testMimeGroupsIntersect() throws DeviceNotAvailableException {
        runTestWithReboot("ComplexFilterTest", "testMimeGroupsIntersect");
    }

    @Test
    public void testRemoveTypeFromIntersection() throws DeviceNotAvailableException {
        runTestWithReboot("ComplexFilterTest", "testRemoveTypeFromIntersection");
    }

    @Test
    public void testRemoveIntersectionFromBothGroups() throws DeviceNotAvailableException {
        runTestWithReboot("ComplexFilterTest", "testRemoveIntersectionFromBothGroups");
    }

    @Test
    public void testClearGroupWithoutIntersection() throws DeviceNotAvailableException {
        runTestWithReboot("ComplexFilterTest", "testClearGroupWithoutIntersection");
    }

    @Test
    public void testClearGroupWithIntersection() throws DeviceNotAvailableException {
        runTestWithReboot("ComplexFilterTest", "testClearGroupWithIntersection");
    }

    @Test
    public void testMimeGroupNotIntersectWithStaticType() throws DeviceNotAvailableException {
        runTestWithReboot("ComplexFilterTest", "testMimeGroupNotIntersectWithStaticType");
    }

    @Test
    public void testMimeGroupIntersectWithStaticType() throws DeviceNotAvailableException {
        runTestWithReboot("ComplexFilterTest", "testMimeGroupIntersectWithStaticType");
    }

    @Test
    public void testRemoveStaticTypeFromMimeGroup() throws DeviceNotAvailableException {
        runTestWithReboot("ComplexFilterTest", "testRemoveStaticTypeFromMimeGroup");
    }

    @Test
    public void testClearGroupContainsStaticType() throws DeviceNotAvailableException {
        runTestWithReboot("ComplexFilterTest", "testClearGroupContainsStaticType");
    }

    @Test
    public void testClearGroupNotContainStaticType() throws DeviceNotAvailableException {
        runTestWithReboot("ComplexFilterTest", "testClearGroupNotContainStaticType");
    }

    private void runTestWithReboot(String testClassName, String testMethodName)
            throws DeviceNotAvailableException {
        runPreReboot(testClassName, testMethodName);
        getDevice().reboot();
        runPostReboot(testClassName, testMethodName);
    }

    private void runPostReboot(String testClassName, String testMethodName)
        throws DeviceNotAvailableException {
        runDeviceTests(PACKAGE_TEST_APP, PACKAGE_REBOOT_TESTS + ".PostReboot" + testClassName,
            testMethodName);
    }

    private void runPreReboot(String testClassName, String testMethodName)
        throws DeviceNotAvailableException {
        runDeviceTests(PACKAGE_TEST_APP, PACKAGE_REBOOT_TESTS + ".PreReboot" + testClassName,
            testMethodName);
    }
}
