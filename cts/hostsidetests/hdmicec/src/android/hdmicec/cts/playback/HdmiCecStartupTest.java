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

package android.hdmicec.cts.playback;

import static com.google.common.collect.Iterables.filter;
import static com.google.common.truth.Truth.assertWithMessage;
import static org.junit.Assume.assumeTrue;

import android.hdmicec.cts.CecOperand;
import android.hdmicec.cts.HdmiCecClientWrapper;
import android.hdmicec.cts.HdmiCecConstants;
import android.hdmicec.cts.LogicalAddress;
import android.hdmicec.cts.RequiredPropertyRule;
import android.hdmicec.cts.RequiredFeatureRule;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import com.google.common.base.Predicate;
import com.google.common.collect.ImmutableList;
import javax.annotation.Nullable;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.junit.Test;

import java.util.List;

/**
 * HDMI CEC test to verify physical address after device reboot (Section 10.2.3)
 */
@Ignore("b/149519706")
@RunWith(DeviceJUnit4ClassRunner.class)
public final class HdmiCecStartupTest extends BaseHostJUnit4Test {

  private static final LogicalAddress PLAYBACK_DEVICE = LogicalAddress.PLAYBACK_1;
  private static final ImmutableList<CecOperand> necessaryMessages =
      new ImmutableList.Builder<CecOperand>()
          .add(CecOperand.REPORT_PHYSICAL_ADDRESS, CecOperand.CEC_VERSION,
              CecOperand.DEVICE_VENDOR_ID, CecOperand.GIVE_POWER_STATUS).build();
  private static final ImmutableList<CecOperand> permissibleMessages =
      new ImmutableList.Builder<CecOperand>()
          .add(CecOperand.VENDOR_COMMAND, CecOperand.GIVE_DEVICE_VENDOR_ID,
              CecOperand.SET_OSD_NAME, CecOperand.GIVE_OSD_NAME).build();

  public HdmiCecClientWrapper hdmiCecClient = new HdmiCecClientWrapper(LogicalAddress.PLAYBACK_1);

  @Rule
  public RuleChain ruleChain =
      RuleChain
          .outerRule(new RequiredFeatureRule(this, LogicalAddress.HDMI_CEC_FEATURE))
          .around(new RequiredFeatureRule(this, LogicalAddress.LEANBACK_FEATURE))
          .around(RequiredPropertyRule.asCsvContainsValue(
              this,
              LogicalAddress.HDMI_DEVICE_TYPE_PROPERTY,
              PLAYBACK_DEVICE.getDeviceType()))
          .around(hdmiCecClient);

  /**
   * Tests that the device sends all the messages that should be sent on startup. It also ensures
   * that only the device only sends messages which are allowed by the spec.
   */
  @Test
  public void cectVerifyStartupMessages() throws Exception {
    ITestDevice device = getDevice();

    /* Make sure device is playback only. Not applicable to playback/audio combinations */
    String deviceTypeCsv = device.executeShellCommand("getprop ro.hdmi.device_type").trim();
    assumeTrue(deviceTypeCsv.equals(LogicalAddress.PLAYBACK_1.getDeviceType()));

    device.executeShellCommand("reboot");
    device.waitForBootComplete(HdmiCecConstants.REBOOT_TIMEOUT);
    /* Monitor CEC messages for 20s after reboot */
    final List<CecOperand> messagesReceived =
        hdmiCecClient.getAllMessages(LogicalAddress.PLAYBACK_1, 20);

    /* Predicate to apply on necessaryMessages to ensure that all necessaryMessages are received. */
    final Predicate<CecOperand> notReceived = new Predicate<CecOperand>() {
      @Override
      public boolean apply(@Nullable CecOperand cecOperand) {
        return !messagesReceived.contains(cecOperand);
      }
    };

    /* Predicate to apply on messagesReceived to ensure all messages received are permissible. */
    final Predicate<CecOperand> notAllowed = new Predicate<CecOperand>() {
      @Override
      public boolean apply(@Nullable CecOperand cecOperand) {
        return !(permissibleMessages.contains(cecOperand) || necessaryMessages.contains(cecOperand));
      }
    };

    assertWithMessage("Some necessary messages are missing").
        that(filter(necessaryMessages, notReceived)).isEmpty();

    assertWithMessage("Some non-permissible messages received").
        that(filter(messagesReceived, notAllowed)).isEmpty();
  }
}
