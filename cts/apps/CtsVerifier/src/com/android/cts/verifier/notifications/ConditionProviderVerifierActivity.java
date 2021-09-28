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

package com.android.cts.verifier.notifications;

import static android.app.NotificationManager.AUTOMATIC_RULE_STATUS_DISABLED;
import static android.app.NotificationManager.AUTOMATIC_RULE_STATUS_ENABLED;
import static android.app.NotificationManager.AUTOMATIC_RULE_STATUS_REMOVED;
import static android.app.NotificationManager.AUTOMATIC_RULE_STATUS_UNKNOWN;
import static android.app.NotificationManager.INTERRUPTION_FILTER_PRIORITY;

import android.app.AutomaticZenRule;
import android.app.NotificationManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.provider.Settings;
import android.service.notification.ZenPolicy;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import com.android.cts.verifier.R;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Objects;

public class ConditionProviderVerifierActivity extends InteractiveVerifierActivity
        implements Runnable {
    private static final String TAG = "CPVerifier";
    protected static final String CP_PACKAGE = "com.android.cts.verifier";
    protected static final String CP_PATH = CP_PACKAGE +
            "/com.android.cts.verifier.notifications.MockConditionProvider";

    protected static final String PREFS = "zen_prefs";
    private static final String BROADCAST_RULE_NAME = "123";

    @Override
    protected int getTitleResource() {
        return R.string.cp_test;
    }

    @Override
    protected int getInstructionsResource() {
        return R.string.cp_info;
    }

    // Test Setup

    @Override
    protected List<InteractiveTestCase> createTestItems() {
        List<InteractiveTestCase> tests = new ArrayList<>(9);
        tests.add(new IsEnabledTest());
        tests.add(new ServiceStartedTest());
        tests.add(new CreateAutomaticZenRuleTest());
        tests.add(new CreateAutomaticZenRuleWithZenPolicyTest());
        tests.add(new UpdateAutomaticZenRuleTest());
        tests.add(new UpdateAutomaticZenRuleWithZenPolicyTest());
        tests.add(new GetAutomaticZenRuleTest());
        tests.add(new GetAutomaticZenRulesTest());
        tests.add(new VerifyRulesIntent());
        tests.add(new VerifyRulesAvailableToUsers());
        tests.add(new ReceiveRuleDisableNoticeTest());
        tests.add(new ReceiveRuleEnabledNoticeTest());
        tests.add(new ReceiveRuleDeletedNoticeTest());
        tests.add(new SubscribeAutomaticZenRuleTest());
        tests.add(new DeleteAutomaticZenRuleTest());
        tests.add(new UnsubscribeAutomaticZenRuleTest());
        tests.add(new RequestUnbindTest());
        tests.add(new RequestBindTest());
        tests.add(new IsDisabledTest());
        tests.add(new ServiceStoppedTest());
        return tests;
    }

    protected class IsEnabledTest extends InteractiveTestCase {
        @Override
        protected View inflate(ViewGroup parent) {
            return createSettingsItem(parent, R.string.cp_enable_service);
        }

        @Override
        boolean autoStart() {
            return true;
        }

        @Override
        protected void test() {
            Intent settings = new Intent(Settings.ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS);
            if (settings.resolveActivity(mPackageManager) == null) {
                logFail("no settings activity");
                status = FAIL;
            } else {
                if (mNm.isNotificationPolicyAccessGranted()) {
                    status = PASS;
                } else {
                    status = WAIT_FOR_USER;
                }
                next();
            }
        }

        protected void tearDown() {
            // wait for the service to start
            delay();
        }

        @Override
        protected Intent getIntent() {
            return new Intent(Settings.ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS);
        }
    }

    protected class ServiceStartedTest extends InteractiveTestCase {
        int mRetries = 5;
        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.cp_service_started);
        }

        @Override
        protected void test() {
            if (MockConditionProvider.getInstance().isConnected()
                    && MockConditionProvider.getInstance().isBound()) {
                status = PASS;
                next();
            } else {
                if (--mRetries > 0) {
                    status = RETEST;
                    next();
                } else {
                    logFail();
                    status = FAIL;
                }
            }
        }
    }

    private class RequestUnbindTest extends InteractiveTestCase {
        int mRetries = 5;

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.nls_snooze);

        }

        @Override
        protected void setUp() {
            status = READY;
        }

        @Override
        protected void test() {
            if (status == READY) {
                MockConditionProvider.getInstance().requestUnbind();
                status = RETEST;
            } else {
                if (MockConditionProvider.getInstance() == null ||
                        !MockConditionProvider.getInstance().isConnected()) {
                    status = PASS;
                } else {
                    if (--mRetries > 0) {
                        status = RETEST;
                    } else {
                        logFail();
                        status = FAIL;
                    }
                }
                next();
            }
        }
    }

    private class RequestBindTest extends InteractiveTestCase {
        int mRetries = 5;

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.nls_unsnooze);

        }

        @Override
        protected void test() {
            if (status == READY) {
                MockConditionProvider.requestRebind(MockConditionProvider.COMPONENT_NAME);
                status = RETEST;
            } else {
                if (MockConditionProvider.getInstance().isConnected()
                        && MockConditionProvider.getInstance().isBound()) {
                    status = PASS;
                    next();
                } else {
                    if (--mRetries > 0) {
                        status = RETEST;
                        next();
                    } else {
                        logFail();
                        status = FAIL;
                    }
                }
            }
        }
    }


    private class CreateAutomaticZenRuleTest extends InteractiveTestCase {
        private String id = null;

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.cp_create_rule);
        }

        @Override
        protected void test() {
            AutomaticZenRule ruleToCreate =
                    createRule("Rule", "value", NotificationManager.INTERRUPTION_FILTER_ALARMS);
            id = mNm.addAutomaticZenRule(ruleToCreate);

            if (!TextUtils.isEmpty(id)) {
                status = PASS;
            } else {
                logFail();
                status = FAIL;
            }
            next();
        }

        @Override
        protected void tearDown() {
            if (id != null) {
                mNm.removeAutomaticZenRule(id);
            }
            MockConditionProvider.getInstance().resetData();
        }
    }

    private class CreateAutomaticZenRuleWithZenPolicyTest extends InteractiveTestCase {
        private String id = null;

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.cp_create_rule_with_zen_policy);
        }

        @Override
        protected void test() {
            AutomaticZenRule ruleToCreate =
                    createRuleWithZenPolicy("Rule", "value",
                            new ZenPolicy.Builder().allowReminders(true).build());
            id = mNm.addAutomaticZenRule(ruleToCreate);

            if (!TextUtils.isEmpty(id)) {
                AutomaticZenRule rule = mNm.getAutomaticZenRule(id);
                if (rule != null && ruleToCreate.getName().equals(rule.getName())
                        && ruleToCreate.getOwner().equals(rule.getOwner())
                        && ruleToCreate.getConditionId().equals(rule.getConditionId())
                        && ruleToCreate.isEnabled() == rule.isEnabled()
                        && ruleToCreate.getInterruptionFilter() == rule.getInterruptionFilter()
                        && Objects.equals(ruleToCreate.getConfigurationActivity(),
                        rule.getConfigurationActivity())
                        && Objects.equals(ruleToCreate.getZenPolicy(), rule.getZenPolicy())) {
                    status = PASS;
                } else {
                    logFail("created rule doesn't equal actual rule");
                    status = FAIL;
                }
            } else {
                logFail("rule wasn't created");
                status = FAIL;
            }
            next();
        }

        @Override
        protected void tearDown() {
            if (id != null) {
                mNm.removeAutomaticZenRule(id);
            }
            MockConditionProvider.getInstance().resetData();
        }
    }

    private class UpdateAutomaticZenRuleTest extends InteractiveTestCase {
        private String id = null;

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.cp_update_rule);
        }

        @Override
        protected void setUp() {
            id = mNm.addAutomaticZenRule(createRule("BeforeUpdate", "beforeValue",
                    NotificationManager.INTERRUPTION_FILTER_ALARMS));
            status = READY;
            delay();
        }

        @Override
        protected void test() {
            AutomaticZenRule updated = mNm.getAutomaticZenRule(id);
            updated.setName("AfterUpdate");
            updated.setConditionId(MockConditionProvider.toConditionId("afterValue"));
            updated.setInterruptionFilter(NotificationManager.INTERRUPTION_FILTER_NONE);

            try {
                boolean success = mNm.updateAutomaticZenRule(id, updated);
                if (success && updated.equals(mNm.getAutomaticZenRule(id))) {
                    status = PASS;
                } else {
                    logFail();
                    status = FAIL;
                }
            } catch (Exception e) {
                logFail("update failed", e);
                status = FAIL;
            }
            next();
        }

        @Override
        protected void tearDown() {
            if (id != null) {
                mNm.removeAutomaticZenRule(id);
            }
            MockConditionProvider.getInstance().resetData();
        }
    }

    private class UpdateAutomaticZenRuleWithZenPolicyTest extends InteractiveTestCase {
        private String id1 = null; // no zen policy
        private String id2 = null; // has zen policy

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.cp_update_rule_use_zen_policy);
        }

        @Override
        protected void setUp() {
            // create rule without a zen policy
            id1 = mNm.addAutomaticZenRule(createRule("BeforeUpdate1",
                    "beforeValue1", NotificationManager.INTERRUPTION_FILTER_ALARMS));
            id2 = mNm.addAutomaticZenRule(createRuleWithZenPolicy(
                    "BeforeUpdate2", "beforeValue2",
                    new ZenPolicy.Builder().allowReminders(true).build()));
            status = READY;
            delay();
        }

        @Override
        protected void test() {
            ZenPolicy.Builder builder = new ZenPolicy.Builder().allowAlarms(true);

            // update rule with zen policy
            AutomaticZenRule updated1 = mNm.getAutomaticZenRule(id1);
            updated1.setName("AfterUpdate1");
            updated1.setConditionId(MockConditionProvider.toConditionId("afterValue1"));
            updated1.setInterruptionFilter(INTERRUPTION_FILTER_PRIORITY);
            updated1.setZenPolicy(builder.build());

            AutomaticZenRule updated2 = mNm.getAutomaticZenRule(id2);
            updated2.setName("AfterUpdate2");
            updated2.setConditionId(MockConditionProvider.toConditionId("afterValue2"));
            updated2.setInterruptionFilter(INTERRUPTION_FILTER_PRIORITY);
            updated2.setZenPolicy(builder.build());

            try {
                boolean success1 = mNm.updateAutomaticZenRule(id1, updated1);
                boolean success2 = mNm.updateAutomaticZenRule(id2, updated2);
                if (success1 && success2) {
                    boolean rule1UpdateSuccess =
                            updated1.equals(mNm.getAutomaticZenRule(id1));
                    boolean rule2UpdateSuccess =
                            updated2.equals(mNm.getAutomaticZenRule(id2));
                    if (rule1UpdateSuccess && rule2UpdateSuccess) {
                        status = PASS;
                    } else {
                        if (!rule1UpdateSuccess) {
                            logFail("Updated rule1 is not expected expected=" + updated1.toString()
                                    + " actual=" + mNm.getAutomaticZenRule(id1));
                        }
                        if (!rule2UpdateSuccess) {
                            logFail("Updated rule2 is not expected expected=" + updated2.toString()
                                    + " actual=" + mNm.getAutomaticZenRule(id2));
                        }
                        status = FAIL;
                    }
                } else {
                    logFail("Did not successfully update rules");
                    status = FAIL;
                }
            } catch (Exception e) {
                logFail("update failed", e);
                status = FAIL;
            }
            next();
        }

        @Override
        protected void tearDown() {
            if (id1 != null) {
                mNm.removeAutomaticZenRule(id1);
            }

            if (id2 != null) {
                mNm.removeAutomaticZenRule(id2);
            }
            MockConditionProvider.getInstance().resetData();
        }
    }

    private class SubscribeAutomaticZenRuleTest extends InteractiveTestCase {
        private String id = null;
        private AutomaticZenRule ruleToCreate;
        private int mRetries = 3;

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.cp_subscribe_rule);
        }

        @Override
        protected void setUp() {
            ruleToCreate = createRule("RuleSubscribe", "Subscribevalue",
                    NotificationManager.INTERRUPTION_FILTER_ALARMS);
            id = mNm.addAutomaticZenRule(ruleToCreate);
            status = READY;
            delay();
        }

        @Override
        protected void test() {
            boolean foundMatch = false;
            List<Uri> subscriptions = MockConditionProvider.getInstance().getSubscriptions();
            for (Uri actual : subscriptions) {
                if (ruleToCreate.getConditionId().equals(actual)) {
                    status = PASS;
                    foundMatch = true;
                    break;
                }
            }
            if (foundMatch) {
                status = PASS;
                next();
            } else if (--mRetries > 0) {
                setFailed();
            } else {
                status = RETEST;
                next();
            }
        }

        @Override
        protected void tearDown() {
            if (id != null) {
                mNm.removeAutomaticZenRule(id);
            }
            MockConditionProvider.getInstance().resetData();
        }
    }

    private class GetAutomaticZenRuleTest extends InteractiveTestCase {
        private String id1 = null;
        private String id2 = null;
        private AutomaticZenRule ruleToCreate1; // no zen policy
        private AutomaticZenRule ruleToCreate2; // has zen policy

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.cp_get_rule);
        }

        @Override
        protected void setUp() {
            ruleToCreate1 = createRule("RuleGet no zen policy", "valueGet",
                    NotificationManager.INTERRUPTION_FILTER_ALARMS);
            ruleToCreate2 = createRuleWithZenPolicy("RuleGet zen policy", "valueGet",
                    new ZenPolicy.Builder().allowReminders(true).build());
            id1 = mNm.addAutomaticZenRule(ruleToCreate1);
            id2 = mNm.addAutomaticZenRule(ruleToCreate2);
            status = READY;
            delay();
        }

        @Override
        protected void test() {
            AutomaticZenRule queriedRule1 = mNm.getAutomaticZenRule(id1);
            AutomaticZenRule queriedRule2 = mNm.getAutomaticZenRule(id2);
            if (queriedRule1 != null
                    && ruleToCreate1.getName().equals(queriedRule1.getName())
                    && ruleToCreate1.getOwner().equals(queriedRule1.getOwner())
                    && ruleToCreate1.getConditionId().equals(queriedRule1.getConditionId())
                    && ruleToCreate1.isEnabled() == queriedRule1.isEnabled()
                    && Objects.equals(ruleToCreate1.getZenPolicy(), queriedRule1.getZenPolicy())
                    && queriedRule2 != null
                    && ruleToCreate2.getName().equals(queriedRule2.getName())
                    && ruleToCreate2.getOwner().equals(queriedRule2.getOwner())
                    && ruleToCreate2.getConditionId().equals(queriedRule2.getConditionId())
                    && ruleToCreate2.isEnabled() == queriedRule2.isEnabled()
                    && Objects.equals(ruleToCreate2.getZenPolicy(), queriedRule2.getZenPolicy())) {
                status = PASS;
            } else {
                logFail();
                status = FAIL;
            }
            next();
        }

        @Override
        protected void tearDown() {
            if (id1 != null) {
                mNm.removeAutomaticZenRule(id1);
            }

            if (id2 != null) {
                mNm.removeAutomaticZenRule(id2);
            }
            MockConditionProvider.getInstance().resetData();
        }
    }

    private class GetAutomaticZenRulesTest extends InteractiveTestCase {
        private List<String> ids = new ArrayList<>();
        private AutomaticZenRule rule1; // no ZenPolicy
        private AutomaticZenRule rule2; // has ZenPolicy

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.cp_get_rules);
        }

        @Override
        protected void setUp() {
            rule1 = createRule("Rule without ZenPolicy", "value1",
                    NotificationManager.INTERRUPTION_FILTER_ALARMS);
            rule2 = createRuleWithZenPolicy("Rule with ZenPolicy", "value2",
                    new ZenPolicy.Builder().allowReminders(true).build());
            ids.add(mNm.addAutomaticZenRule(rule1));
            ids.add(mNm.addAutomaticZenRule(rule2));
            status = READY;
            delay();
        }

        @Override
        protected void test() {
            Map<String, AutomaticZenRule> rules = mNm.getAutomaticZenRules();

            if (rules == null || rules.size() != 2) {
                logFail();
                status = FAIL;
                next();
                return;
            }

            for (AutomaticZenRule createdRule : rules.values()) {
                if (!compareRules(createdRule, rule1) && !compareRules(createdRule, rule2)) {
                    logFail();
                    status = FAIL;
                    break;
                }
            }
            status = PASS;
            next();
        }

        @Override
        protected void tearDown() {
            for (String id : ids) {
                mNm.removeAutomaticZenRule(id);
            }
            MockConditionProvider.getInstance().resetData();
        }
    }

    protected class VerifyRulesIntent extends InteractiveTestCase {
        @Override
        protected View inflate(ViewGroup parent) {
            return createSettingsItem(parent, R.string.cp_show_rules);
        }

        @Override
        boolean autoStart() {
            return true;
        }

        @Override
        protected void test() {
            Intent settings = new Intent(Settings.ACTION_CONDITION_PROVIDER_SETTINGS);
            if (settings.resolveActivity(mPackageManager) == null) {
                logFail("no settings activity");
                status = FAIL;
            } else {
                if (buttonPressed) {
                    status = PASS;
                } else {
                    status = RETEST_AFTER_LONG_DELAY;
                }
                next();
            }
        }

        protected void tearDown() {
            // wait for the service to start
            delay();
        }

        @Override
        protected Intent getIntent() {
            return new Intent(Settings.ACTION_CONDITION_PROVIDER_SETTINGS);
        }
    }

    protected class VerifyRulesAvailableToUsers extends InteractiveTestCase {
        @Override
        protected View inflate(ViewGroup parent) {
            return createPassFailItem(parent, R.string.cp_show_rules_verification);
        }

        @Override
        boolean autoStart() {
            return true;
        }

        @Override
        protected void test() {
            status = WAIT_FOR_USER;
            next();
        }
    }

    /**
     * Sends the user to settings to disable the rule. Waits to receive the broadcast that the rule
     * was disabled, and confirms that the broadcast contains the correct extras.
     */
    protected class ReceiveRuleDisableNoticeTest extends InteractiveTestCase {
        private final int EXPECTED_STATUS = AUTOMATIC_RULE_STATUS_DISABLED;
        private int mRetries = 2;
        private View mView;
        private String mId;
        @Override
        protected View inflate(ViewGroup parent) {
            mView = createNlsSettingsItem(parent, R.string.cp_disable_rule);
            Button button = mView.findViewById(R.id.nls_action_button);
            button.setEnabled(false);
            return mView;
        }

        @Override
        protected void setUp() {
            status = READY;
            // create enabled so it's ready to be disabled in app
            AutomaticZenRule rule = new AutomaticZenRule(BROADCAST_RULE_NAME, null,
                    new ComponentName(CP_PACKAGE,
                            ConditionProviderVerifierActivity.this.getClass().getName()),
                    Uri.EMPTY, null, INTERRUPTION_FILTER_PRIORITY, true);
            mId = mNm.addAutomaticZenRule(rule);
            Button button = mView.findViewById(R.id.nls_action_button);
            button.setEnabled(true);
        }

        @Override
        boolean autoStart() {
            return true;
        }

        @Override
        protected void test() {
            SharedPreferences prefs = mContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE);

            AutomaticZenRule rule = mNm.getAutomaticZenRule(mId);

            if (!rule.isEnabled()) {
                Log.d(TAG, "Check pref for broadcast " + prefs.contains(mId)
                        + " " + prefs.getInt(mId, AUTOMATIC_RULE_STATUS_UNKNOWN));
                if (prefs.contains(mId)
                        && EXPECTED_STATUS == prefs.getInt(mId, AUTOMATIC_RULE_STATUS_UNKNOWN)) {
                    status = PASS;
                } else {
                    if (mRetries > 0) {
                        mRetries--;
                        status = RETEST;
                    } else {
                        status = FAIL;
                    }
                }
            } else {
                Log.d(TAG, "Waiting for user");
                // user hasn't jumped to settings yet
                status = WAIT_FOR_USER;
            }

            next();
        }

        protected void tearDown() {
            mNm.removeAutomaticZenRule(mId);
            SharedPreferences prefs = mContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
            prefs.edit().clear().commit();
        }

        @Override
        protected Intent getIntent() {
            return new Intent(Settings.ACTION_CONDITION_PROVIDER_SETTINGS)
                    .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);
        }
    }

    /**
     * Sends the user to settings to enable the rule. Waits to receive the broadcast that the rule
     * was enabled, and confirms that the broadcast contains the correct extras.
     */
    protected class ReceiveRuleEnabledNoticeTest extends InteractiveTestCase {
        private final int EXPECTED_STATUS = AUTOMATIC_RULE_STATUS_ENABLED;
        private int mRetries = 2;
        private View mView;
        private String mId;
        @Override
        protected View inflate(ViewGroup parent) {
            mView = createNlsSettingsItem(parent, R.string.cp_enable_rule);
            Button button = mView.findViewById(R.id.nls_action_button);
            button.setEnabled(false);
            return mView;
        }

        @Override
        protected void setUp() {
            status = READY;
            // create disabled so it's ready to be enabled in Settings
            AutomaticZenRule rule = new AutomaticZenRule(BROADCAST_RULE_NAME, null,
                    new ComponentName(CP_PACKAGE,
                            ConditionProviderVerifierActivity.this.getClass().getName()),
                    Uri.EMPTY, null, INTERRUPTION_FILTER_PRIORITY, false);
            mId = mNm.addAutomaticZenRule(rule);
            Button button = mView.findViewById(R.id.nls_action_button);
            button.setEnabled(true);
        }

        @Override
        boolean autoStart() {
            return true;
        }

        @Override
        protected void test() {
            SharedPreferences prefs = mContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE);

            AutomaticZenRule rule = mNm.getAutomaticZenRule(mId);

            if (rule.isEnabled()) {
                Log.d(TAG, "Check pref for broadcast " + prefs.contains(mId)
                        + " " + prefs.getInt(mId, AUTOMATIC_RULE_STATUS_UNKNOWN));
                if (prefs.contains(mId)
                        && EXPECTED_STATUS == prefs.getInt(mId, AUTOMATIC_RULE_STATUS_UNKNOWN)) {
                    status = PASS;
                } else {
                    if (mRetries > 0) {
                        mRetries--;
                        status = RETEST;
                    } else {
                        status = FAIL;
                    }
                }
            } else {
                Log.d(TAG, "Waiting for user");
                // user hasn't jumped to settings yet
                status = WAIT_FOR_USER;
            }

            next();
        }

        protected void tearDown() {
            mNm.removeAutomaticZenRule(mId);
            SharedPreferences prefs = mContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
            prefs.edit().clear().commit();
        }

        @Override
        protected Intent getIntent() {
            return new Intent(Settings.ACTION_CONDITION_PROVIDER_SETTINGS)
                    .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);
        }
    }

    /**
     * Sends the user to settings to delete the rule. Waits to receive the broadcast that the rule
     * was deleted, and confirms that the broadcast contains the correct extras.
     */
    protected class ReceiveRuleDeletedNoticeTest extends InteractiveTestCase {
        private final int EXPECTED_STATUS = AUTOMATIC_RULE_STATUS_REMOVED;
        private int mRetries = 2;
        private View mView;
        private String mId;
        @Override
        protected View inflate(ViewGroup parent) {
            mView = createNlsSettingsItem(parent, R.string.cp_delete_rule_broadcast);
            Button button = mView.findViewById(R.id.nls_action_button);
            button.setEnabled(false);
            return mView;
        }

        @Override
        protected void setUp() {
            status = READY;
            AutomaticZenRule rule = new AutomaticZenRule(BROADCAST_RULE_NAME, null,
                    new ComponentName(CP_PACKAGE,
                            ConditionProviderVerifierActivity.this.getClass().getName()),
                    Uri.EMPTY, null, INTERRUPTION_FILTER_PRIORITY, true);
            mId = mNm.addAutomaticZenRule(rule);
            Button button = mView.findViewById(R.id.nls_action_button);
            button.setEnabled(true);
        }

        @Override
        boolean autoStart() {
            return true;
        }

        @Override
        protected void test() {
            SharedPreferences prefs = mContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE);

            AutomaticZenRule rule = mNm.getAutomaticZenRule(mId);

            if (rule == null) {
                Log.d(TAG, "Check pref for broadcast " + prefs.contains(mId)
                        + " " + prefs.getInt(mId, AUTOMATIC_RULE_STATUS_UNKNOWN));
                if (prefs.contains(mId)
                        && EXPECTED_STATUS == prefs.getInt(mId, AUTOMATIC_RULE_STATUS_UNKNOWN)) {
                    status = PASS;
                } else {
                    if (mRetries > 0) {
                        mRetries--;
                        status = RETEST;
                    } else {
                        status = FAIL;
                    }
                }
            } else {
                Log.d(TAG, "Waiting for user");
                // user hasn't jumped to settings yet
                status = WAIT_FOR_USER;
            }

            next();
        }

        protected void tearDown() {
            mNm.removeAutomaticZenRule(mId);
            SharedPreferences prefs = mContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
            prefs.edit().clear().commit();
        }

        @Override
        protected Intent getIntent() {
            return new Intent(Settings.ACTION_CONDITION_PROVIDER_SETTINGS)
                    .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);
        }
    }

    private class DeleteAutomaticZenRuleTest extends InteractiveTestCase {
        private String id1 = null;
        private String id2 = null;

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.cp_delete_rule);
        }

        @Override
        protected void test() {
            AutomaticZenRule ruleToCreate1 = createRule("RuleDelete without ZenPolicy",
                    "Deletevalue", NotificationManager.INTERRUPTION_FILTER_ALARMS);
            AutomaticZenRule ruleToCreate2 = createRule("RuleDelete with ZenPolicy",
                    "Deletevalue", NotificationManager.INTERRUPTION_FILTER_ALARMS);
            id1 = mNm.addAutomaticZenRule(ruleToCreate1);
            id2 = mNm.addAutomaticZenRule(ruleToCreate2);

            if (id1 != null && id2 != null) {
                if (mNm.removeAutomaticZenRule(id1) && mNm.removeAutomaticZenRule(id2)) {
                    if (mNm.getAutomaticZenRule(id1) == null
                            && mNm.getAutomaticZenRule(id2) == null) {
                        status = PASS;
                    } else {
                        logFail();
                        status = FAIL;
                    }
                } else {
                    logFail();
                    status = FAIL;
                }
            } else {
                logFail("Couldn't test rule deletion; creation failed.");
                status = FAIL;
            }
            next();
        }

        @Override
        protected void tearDown() {
            MockConditionProvider.getInstance().resetData();
            delay();
        }
    }

    private class UnsubscribeAutomaticZenRuleTest extends InteractiveTestCase {
        private String id = null;
        private AutomaticZenRule ruleToCreate;
        private int mSubscribeRetries = 3;
        private int mUnsubscribeRetries = 3;
        private boolean mSubscribing = true;

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.cp_unsubscribe_rule);
        }

        @Override
        protected void setUp() {
            ruleToCreate = createRule("RuleUnsubscribe", "valueUnsubscribe",
                    INTERRUPTION_FILTER_PRIORITY);
            id = mNm.addAutomaticZenRule(ruleToCreate);
            status = READY;
            delay();
        }

        @Override
        protected void test() {
            if (mSubscribing) {
                // trying to subscribe
                boolean foundMatch = false;
                List<Uri> subscriptions = MockConditionProvider.getInstance().getSubscriptions();
                for (Uri actual : subscriptions) {
                    if (ruleToCreate.getConditionId().equals(actual)) {
                        status = PASS;
                        foundMatch = true;
                        break;
                    }
                }
                if (foundMatch) {
                    // Now that it's subscribed, remove the rule and verify that it
                    // unsubscribes.
                    Log.d(MockConditionProvider.TAG, "Found subscription, removing");
                    mNm.removeAutomaticZenRule(id);

                    mSubscribing = false;
                    status = RETEST;
                    next();
                } else if (--mSubscribeRetries > 0) {
                    setFailed();
                } else {
                    status = RETEST;
                    next();
                }
            } else {
                // trying to unsubscribe
                List<Uri> continuingSubscriptions
                        = MockConditionProvider.getInstance().getSubscriptions();
                boolean stillFoundMatch = false;
                for (Uri actual : continuingSubscriptions) {
                    if (ruleToCreate.getConditionId().equals(actual)) {
                        stillFoundMatch = true;
                        break;
                    }
                }
                if (!stillFoundMatch) {
                    status = PASS;
                    next();
                } else if (stillFoundMatch && --mUnsubscribeRetries > 0) {
                    Log.d(MockConditionProvider.TAG, "Still found subscription, retrying");
                    status = RETEST;
                    next();
                } else {
                    setFailed();
                }
            }
        }

        @Override
        protected void tearDown() {
            mNm.removeAutomaticZenRule(id);
            MockConditionProvider.getInstance().resetData();
        }
    }

    private class IsDisabledTest extends InteractiveTestCase {
        @Override
        protected View inflate(ViewGroup parent) {
            return createSettingsItem(parent, R.string.cp_disable_service);
        }

        @Override
        boolean autoStart() {
            return true;
        }

        @Override
        protected void test() {
            if (!mNm.isNotificationPolicyAccessGranted()) {
                status = PASS;
            } else {
                status = WAIT_FOR_USER;
            }
            next();
        }

        @Override
        protected Intent getIntent() {
            return new Intent(Settings.ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS);
        }
    }

    private class ServiceStoppedTest extends InteractiveTestCase {
        int mRetries = 5;

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.cp_service_stopped);
        }

        @Override
        protected void test() {
            if (MockConditionProvider.getInstance() == null ||
                    !MockConditionProvider.getInstance().isConnected()) {
                status = PASS;
            } else {
                if (--mRetries > 0) {
                    status = RETEST;
                } else {
                    logFail();
                    status = FAIL;
                }
            }
            next();
        }
    }

    private AutomaticZenRule createRule(String name, String queryValue, int status) {
        return new AutomaticZenRule(name,
                ComponentName.unflattenFromString(CP_PATH),
                MockConditionProvider.toConditionId(queryValue), status, true);
    }

    private AutomaticZenRule createRuleWithZenPolicy(String name, String queryValue,
            ZenPolicy policy) {
        return new AutomaticZenRule(name,
                ComponentName.unflattenFromString(CP_PATH), null,
                MockConditionProvider.toConditionId(queryValue), policy,
                INTERRUPTION_FILTER_PRIORITY, true);
    }

    private boolean compareRules(AutomaticZenRule rule1, AutomaticZenRule rule2) {
        return rule1.isEnabled() == rule2.isEnabled()
                && Objects.equals(rule1.getName(), rule2.getName())
                && rule1.getInterruptionFilter() == rule2.getInterruptionFilter()
                && Objects.equals(rule1.getConditionId(), rule2.getConditionId())
                && Objects.equals(rule1.getOwner(), rule2.getOwner())
                && Objects.equals(rule1.getZenPolicy(), rule2.getZenPolicy());
    }

    protected View createSettingsItem(ViewGroup parent, int messageId) {
        return createUserItem(parent, R.string.cp_start_settings, messageId);
    }
}
