// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>
#include <private/android_filesystem_config.h>
#include <stdio.h>

#include <set>
#include <unordered_map>
#include <vector>

#include "frameworks/base/cmds/statsd/src/statsd_config.pb.h"
#include "metrics/metrics_test_helper.h"
#include "src/condition/ConditionTracker.h"
#include "src/matchers/LogMatchingTracker.h"
#include "src/metrics/CountMetricProducer.h"
#include "src/metrics/GaugeMetricProducer.h"
#include "src/metrics/MetricProducer.h"
#include "src/metrics/ValueMetricProducer.h"
#include "src/metrics/metrics_manager_util.h"
#include "src/state/StateManager.h"
#include "statsd_test_util.h"

using namespace testing;
using android::sp;
using android::os::statsd::Predicate;
using std::map;
using std::set;
using std::unordered_map;
using std::vector;

#ifdef __ANDROID__

namespace android {
namespace os {
namespace statsd {

namespace {
const ConfigKey kConfigKey(0, 12345);
const long kAlertId = 3;

const long timeBaseSec = 1000;

StatsdConfig buildGoodConfig() {
    StatsdConfig config;
    config.set_id(12345);

    AtomMatcher* eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("SCREEN_IS_ON"));

    SimpleAtomMatcher* simpleAtomMatcher = eventMatcher->mutable_simple_atom_matcher();
    simpleAtomMatcher->set_atom_id(2 /*SCREEN_STATE_CHANGE*/);
    simpleAtomMatcher->add_field_value_matcher()->set_field(
            1 /*SCREEN_STATE_CHANGE__DISPLAY_STATE*/);
    simpleAtomMatcher->mutable_field_value_matcher(0)->set_eq_int(
            2 /*SCREEN_STATE_CHANGE__DISPLAY_STATE__STATE_ON*/);

    eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("SCREEN_IS_OFF"));

    simpleAtomMatcher = eventMatcher->mutable_simple_atom_matcher();
    simpleAtomMatcher->set_atom_id(2 /*SCREEN_STATE_CHANGE*/);
    simpleAtomMatcher->add_field_value_matcher()->set_field(
            1 /*SCREEN_STATE_CHANGE__DISPLAY_STATE*/);
    simpleAtomMatcher->mutable_field_value_matcher(0)->set_eq_int(
            1 /*SCREEN_STATE_CHANGE__DISPLAY_STATE__STATE_OFF*/);

    eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("SCREEN_ON_OR_OFF"));

    AtomMatcher_Combination* combination = eventMatcher->mutable_combination();
    combination->set_operation(LogicalOperation::OR);
    combination->add_matcher(StringToId("SCREEN_IS_ON"));
    combination->add_matcher(StringToId("SCREEN_IS_OFF"));

    CountMetric* metric = config.add_count_metric();
    metric->set_id(3);
    metric->set_what(StringToId("SCREEN_IS_ON"));
    metric->set_bucket(ONE_MINUTE);
    metric->mutable_dimensions_in_what()->set_field(2 /*SCREEN_STATE_CHANGE*/);
    metric->mutable_dimensions_in_what()->add_child()->set_field(1);

    config.add_no_report_metric(3);

    auto alert = config.add_alert();
    alert->set_id(kAlertId);
    alert->set_metric_id(3);
    alert->set_num_buckets(10);
    alert->set_refractory_period_secs(100);
    alert->set_trigger_if_sum_gt(100);
    return config;
}

StatsdConfig buildCircleMatchers() {
    StatsdConfig config;
    config.set_id(12345);

    AtomMatcher* eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("SCREEN_IS_ON"));

    SimpleAtomMatcher* simpleAtomMatcher = eventMatcher->mutable_simple_atom_matcher();
    simpleAtomMatcher->set_atom_id(2 /*SCREEN_STATE_CHANGE*/);
    simpleAtomMatcher->add_field_value_matcher()->set_field(
            1 /*SCREEN_STATE_CHANGE__DISPLAY_STATE*/);
    simpleAtomMatcher->mutable_field_value_matcher(0)->set_eq_int(
            2 /*SCREEN_STATE_CHANGE__DISPLAY_STATE__STATE_ON*/);

    eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("SCREEN_ON_OR_OFF"));

    AtomMatcher_Combination* combination = eventMatcher->mutable_combination();
    combination->set_operation(LogicalOperation::OR);
    combination->add_matcher(StringToId("SCREEN_IS_ON"));
    // Circle dependency
    combination->add_matcher(StringToId("SCREEN_ON_OR_OFF"));

    return config;
}

StatsdConfig buildAlertWithUnknownMetric() {
    StatsdConfig config;
    config.set_id(12345);

    AtomMatcher* eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("SCREEN_IS_ON"));

    CountMetric* metric = config.add_count_metric();
    metric->set_id(3);
    metric->set_what(StringToId("SCREEN_IS_ON"));
    metric->set_bucket(ONE_MINUTE);
    metric->mutable_dimensions_in_what()->set_field(2 /*SCREEN_STATE_CHANGE*/);
    metric->mutable_dimensions_in_what()->add_child()->set_field(1);

    auto alert = config.add_alert();
    alert->set_id(3);
    alert->set_metric_id(2);
    alert->set_num_buckets(10);
    alert->set_refractory_period_secs(100);
    alert->set_trigger_if_sum_gt(100);
    return config;
}

StatsdConfig buildMissingMatchers() {
    StatsdConfig config;
    config.set_id(12345);

    AtomMatcher* eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("SCREEN_IS_ON"));

    SimpleAtomMatcher* simpleAtomMatcher = eventMatcher->mutable_simple_atom_matcher();
    simpleAtomMatcher->set_atom_id(2 /*SCREEN_STATE_CHANGE*/);
    simpleAtomMatcher->add_field_value_matcher()->set_field(
            1 /*SCREEN_STATE_CHANGE__DISPLAY_STATE*/);
    simpleAtomMatcher->mutable_field_value_matcher(0)->set_eq_int(
            2 /*SCREEN_STATE_CHANGE__DISPLAY_STATE__STATE_ON*/);

    eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("SCREEN_ON_OR_OFF"));

    AtomMatcher_Combination* combination = eventMatcher->mutable_combination();
    combination->set_operation(LogicalOperation::OR);
    combination->add_matcher(StringToId("SCREEN_IS_ON"));
    // undefined matcher
    combination->add_matcher(StringToId("ABC"));

    return config;
}

StatsdConfig buildMissingPredicate() {
    StatsdConfig config;
    config.set_id(12345);

    CountMetric* metric = config.add_count_metric();
    metric->set_id(3);
    metric->set_what(StringToId("SCREEN_EVENT"));
    metric->set_bucket(ONE_MINUTE);
    metric->set_condition(StringToId("SOME_CONDITION"));

    AtomMatcher* eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("SCREEN_EVENT"));

    SimpleAtomMatcher* simpleAtomMatcher = eventMatcher->mutable_simple_atom_matcher();
    simpleAtomMatcher->set_atom_id(2);

    return config;
}

StatsdConfig buildDimensionMetricsWithMultiTags() {
    StatsdConfig config;
    config.set_id(12345);

    AtomMatcher* eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("BATTERY_VERY_LOW"));
    SimpleAtomMatcher* simpleAtomMatcher = eventMatcher->mutable_simple_atom_matcher();
    simpleAtomMatcher->set_atom_id(2);

    eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("BATTERY_VERY_VERY_LOW"));
    simpleAtomMatcher = eventMatcher->mutable_simple_atom_matcher();
    simpleAtomMatcher->set_atom_id(3);

    eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("BATTERY_LOW"));

    AtomMatcher_Combination* combination = eventMatcher->mutable_combination();
    combination->set_operation(LogicalOperation::OR);
    combination->add_matcher(StringToId("BATTERY_VERY_LOW"));
    combination->add_matcher(StringToId("BATTERY_VERY_VERY_LOW"));

    // Count process state changes, slice by uid, while SCREEN_IS_OFF
    CountMetric* metric = config.add_count_metric();
    metric->set_id(3);
    metric->set_what(StringToId("BATTERY_LOW"));
    metric->set_bucket(ONE_MINUTE);
    // This case is interesting. We want to dimension across two atoms.
    metric->mutable_dimensions_in_what()->add_child()->set_field(1);

    auto alert = config.add_alert();
    alert->set_id(kAlertId);
    alert->set_metric_id(3);
    alert->set_num_buckets(10);
    alert->set_refractory_period_secs(100);
    alert->set_trigger_if_sum_gt(100);
    return config;
}

StatsdConfig buildCirclePredicates() {
    StatsdConfig config;
    config.set_id(12345);

    AtomMatcher* eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("SCREEN_IS_ON"));

    SimpleAtomMatcher* simpleAtomMatcher = eventMatcher->mutable_simple_atom_matcher();
    simpleAtomMatcher->set_atom_id(2 /*SCREEN_STATE_CHANGE*/);
    simpleAtomMatcher->add_field_value_matcher()->set_field(
            1 /*SCREEN_STATE_CHANGE__DISPLAY_STATE*/);
    simpleAtomMatcher->mutable_field_value_matcher(0)->set_eq_int(
            2 /*SCREEN_STATE_CHANGE__DISPLAY_STATE__STATE_ON*/);

    eventMatcher = config.add_atom_matcher();
    eventMatcher->set_id(StringToId("SCREEN_IS_OFF"));

    simpleAtomMatcher = eventMatcher->mutable_simple_atom_matcher();
    simpleAtomMatcher->set_atom_id(2 /*SCREEN_STATE_CHANGE*/);
    simpleAtomMatcher->add_field_value_matcher()->set_field(
            1 /*SCREEN_STATE_CHANGE__DISPLAY_STATE*/);
    simpleAtomMatcher->mutable_field_value_matcher(0)->set_eq_int(
            1 /*SCREEN_STATE_CHANGE__DISPLAY_STATE__STATE_OFF*/);

    auto condition = config.add_predicate();
    condition->set_id(StringToId("SCREEN_IS_ON"));
    SimplePredicate* simplePredicate = condition->mutable_simple_predicate();
    simplePredicate->set_start(StringToId("SCREEN_IS_ON"));
    simplePredicate->set_stop(StringToId("SCREEN_IS_OFF"));

    condition = config.add_predicate();
    condition->set_id(StringToId("SCREEN_IS_EITHER_ON_OFF"));

    Predicate_Combination* combination = condition->mutable_combination();
    combination->set_operation(LogicalOperation::OR);
    combination->add_predicate(StringToId("SCREEN_IS_ON"));
    combination->add_predicate(StringToId("SCREEN_IS_EITHER_ON_OFF"));

    return config;
}

StatsdConfig buildConfigWithDifferentPredicates() {
    StatsdConfig config;
    config.set_id(12345);

    auto pulledAtomMatcher =
            CreateSimpleAtomMatcher("SUBSYSTEM_SLEEP", util::SUBSYSTEM_SLEEP_STATE);
    *config.add_atom_matcher() = pulledAtomMatcher;
    auto screenOnAtomMatcher = CreateScreenTurnedOnAtomMatcher();
    *config.add_atom_matcher() = screenOnAtomMatcher;
    auto screenOffAtomMatcher = CreateScreenTurnedOffAtomMatcher();
    *config.add_atom_matcher() = screenOffAtomMatcher;
    auto batteryNoneAtomMatcher = CreateBatteryStateNoneMatcher();
    *config.add_atom_matcher() = batteryNoneAtomMatcher;
    auto batteryUsbAtomMatcher = CreateBatteryStateUsbMatcher();
    *config.add_atom_matcher() = batteryUsbAtomMatcher;

    // Simple condition with InitialValue set to default (unknown).
    auto screenOnUnknownPredicate = CreateScreenIsOnPredicate();
    *config.add_predicate() = screenOnUnknownPredicate;

    // Simple condition with InitialValue set to false.
    auto screenOnFalsePredicate = config.add_predicate();
    screenOnFalsePredicate->set_id(StringToId("ScreenIsOnInitialFalse"));
    SimplePredicate* simpleScreenOnFalsePredicate =
            screenOnFalsePredicate->mutable_simple_predicate();
    simpleScreenOnFalsePredicate->set_start(screenOnAtomMatcher.id());
    simpleScreenOnFalsePredicate->set_stop(screenOffAtomMatcher.id());
    simpleScreenOnFalsePredicate->set_initial_value(SimplePredicate_InitialValue_FALSE);

    // Simple condition with InitialValue set to false.
    auto onBatteryFalsePredicate = config.add_predicate();
    onBatteryFalsePredicate->set_id(StringToId("OnBatteryInitialFalse"));
    SimplePredicate* simpleOnBatteryFalsePredicate =
            onBatteryFalsePredicate->mutable_simple_predicate();
    simpleOnBatteryFalsePredicate->set_start(batteryNoneAtomMatcher.id());
    simpleOnBatteryFalsePredicate->set_stop(batteryUsbAtomMatcher.id());
    simpleOnBatteryFalsePredicate->set_initial_value(SimplePredicate_InitialValue_FALSE);

    // Combination condition with both simple condition InitialValues set to false.
    auto screenOnFalseOnBatteryFalsePredicate = config.add_predicate();
    screenOnFalseOnBatteryFalsePredicate->set_id(StringToId("ScreenOnFalseOnBatteryFalse"));
    screenOnFalseOnBatteryFalsePredicate->mutable_combination()->set_operation(
            LogicalOperation::AND);
    addPredicateToPredicateCombination(*screenOnFalsePredicate,
                                       screenOnFalseOnBatteryFalsePredicate);
    addPredicateToPredicateCombination(*onBatteryFalsePredicate,
                                       screenOnFalseOnBatteryFalsePredicate);

    // Combination condition with one simple condition InitialValue set to unknown and one set to
    // false.
    auto screenOnUnknownOnBatteryFalsePredicate = config.add_predicate();
    screenOnUnknownOnBatteryFalsePredicate->set_id(StringToId("ScreenOnUnknowneOnBatteryFalse"));
    screenOnUnknownOnBatteryFalsePredicate->mutable_combination()->set_operation(
            LogicalOperation::AND);
    addPredicateToPredicateCombination(screenOnUnknownPredicate,
                                       screenOnUnknownOnBatteryFalsePredicate);
    addPredicateToPredicateCombination(*onBatteryFalsePredicate,
                                       screenOnUnknownOnBatteryFalsePredicate);

    // Simple condition metric with initial value false.
    ValueMetric* metric1 = config.add_value_metric();
    metric1->set_id(StringToId("ValueSubsystemSleepWhileScreenOnInitialFalse"));
    metric1->set_what(pulledAtomMatcher.id());
    *metric1->mutable_value_field() =
            CreateDimensions(util::SUBSYSTEM_SLEEP_STATE, {4 /* time sleeping field */});
    metric1->set_bucket(FIVE_MINUTES);
    metric1->set_condition(screenOnFalsePredicate->id());

    // Simple condition metric with initial value unknown.
    ValueMetric* metric2 = config.add_value_metric();
    metric2->set_id(StringToId("ValueSubsystemSleepWhileScreenOnInitialUnknown"));
    metric2->set_what(pulledAtomMatcher.id());
    *metric2->mutable_value_field() =
            CreateDimensions(util::SUBSYSTEM_SLEEP_STATE, {4 /* time sleeping field */});
    metric2->set_bucket(FIVE_MINUTES);
    metric2->set_condition(screenOnUnknownPredicate.id());

    // Combination condition metric with initial values false and false.
    ValueMetric* metric3 = config.add_value_metric();
    metric3->set_id(StringToId("ValueSubsystemSleepWhileScreenOnFalseDeviceUnpluggedFalse"));
    metric3->set_what(pulledAtomMatcher.id());
    *metric3->mutable_value_field() =
            CreateDimensions(util::SUBSYSTEM_SLEEP_STATE, {4 /* time sleeping field */});
    metric3->set_bucket(FIVE_MINUTES);
    metric3->set_condition(screenOnFalseOnBatteryFalsePredicate->id());

    // Combination condition metric with initial values unknown and false.
    ValueMetric* metric4 = config.add_value_metric();
    metric4->set_id(StringToId("ValueSubsystemSleepWhileScreenOnUnknownDeviceUnpluggedFalse"));
    metric4->set_what(pulledAtomMatcher.id());
    *metric4->mutable_value_field() =
            CreateDimensions(util::SUBSYSTEM_SLEEP_STATE, {4 /* time sleeping field */});
    metric4->set_bucket(FIVE_MINUTES);
    metric4->set_condition(screenOnUnknownOnBatteryFalsePredicate->id());

    return config;
}

bool isSubset(const set<int32_t>& set1, const set<int32_t>& set2) {
    return std::includes(set2.begin(), set2.end(), set1.begin(), set1.end());
}
}  // anonymous namespace

TEST(MetricsManagerTest, TestInitialConditions) {
    UidMap uidMap;
    sp<StatsPullerManager> pullerManager = new StatsPullerManager();
    sp<AlarmMonitor> anomalyAlarmMonitor;
    sp<AlarmMonitor> periodicAlarmMonitor;
    StatsdConfig config = buildConfigWithDifferentPredicates();
    set<int> allTagIds;
    vector<sp<LogMatchingTracker>> allAtomMatchers;
    vector<sp<ConditionTracker>> allConditionTrackers;
    vector<sp<MetricProducer>> allMetricProducers;
    std::vector<sp<AnomalyTracker>> allAnomalyTrackers;
    std::vector<sp<AlarmTracker>> allAlarmTrackers;
    unordered_map<int, std::vector<int>> conditionToMetricMap;
    unordered_map<int, std::vector<int>> trackerToMetricMap;
    unordered_map<int, std::vector<int>> trackerToConditionMap;
    unordered_map<int, std::vector<int>> activationAtomTrackerToMetricMap;
    unordered_map<int, std::vector<int>> deactivationAtomTrackerToMetricMap;
    unordered_map<int64_t, int> alertTrackerMap;
    vector<int> metricsWithActivation;
    std::set<int64_t> noReportMetricIds;

    EXPECT_TRUE(initStatsdConfig(
            kConfigKey, config, uidMap, pullerManager, anomalyAlarmMonitor, periodicAlarmMonitor,
            timeBaseSec, timeBaseSec, allTagIds, allAtomMatchers, allConditionTrackers,
            allMetricProducers, allAnomalyTrackers, allAlarmTrackers, conditionToMetricMap,
            trackerToMetricMap, trackerToConditionMap, activationAtomTrackerToMetricMap,
            deactivationAtomTrackerToMetricMap, alertTrackerMap, metricsWithActivation,
            noReportMetricIds));
    ASSERT_EQ(4u, allMetricProducers.size());
    ASSERT_EQ(5u, allConditionTrackers.size());

    ConditionKey queryKey;
    vector<ConditionState> conditionCache(5, ConditionState::kNotEvaluated);

    allConditionTrackers[3]->isConditionMet(queryKey, allConditionTrackers, false, conditionCache);
    allConditionTrackers[4]->isConditionMet(queryKey, allConditionTrackers, false, conditionCache);
    EXPECT_EQ(ConditionState::kUnknown, conditionCache[0]);
    EXPECT_EQ(ConditionState::kFalse, conditionCache[1]);
    EXPECT_EQ(ConditionState::kFalse, conditionCache[2]);
    EXPECT_EQ(ConditionState::kFalse, conditionCache[3]);
    EXPECT_EQ(ConditionState::kUnknown, conditionCache[4]);

    EXPECT_EQ(ConditionState::kFalse, allMetricProducers[0]->mCondition);
    EXPECT_EQ(ConditionState::kUnknown, allMetricProducers[1]->mCondition);
    EXPECT_EQ(ConditionState::kFalse, allMetricProducers[2]->mCondition);
    EXPECT_EQ(ConditionState::kUnknown, allMetricProducers[3]->mCondition);
}

TEST(MetricsManagerTest, TestGoodConfig) {
    UidMap uidMap;
    sp<StatsPullerManager> pullerManager = new StatsPullerManager();
    sp<AlarmMonitor> anomalyAlarmMonitor;
    sp<AlarmMonitor> periodicAlarmMonitor;
    StatsdConfig config = buildGoodConfig();
    set<int> allTagIds;
    vector<sp<LogMatchingTracker>> allAtomMatchers;
    vector<sp<ConditionTracker>> allConditionTrackers;
    vector<sp<MetricProducer>> allMetricProducers;
    std::vector<sp<AnomalyTracker>> allAnomalyTrackers;
    std::vector<sp<AlarmTracker>> allAlarmTrackers;
    unordered_map<int, std::vector<int>> conditionToMetricMap;
    unordered_map<int, std::vector<int>> trackerToMetricMap;
    unordered_map<int, std::vector<int>> trackerToConditionMap;
    unordered_map<int, std::vector<int>> activationAtomTrackerToMetricMap;
    unordered_map<int, std::vector<int>> deactivationAtomTrackerToMetricMap;
    unordered_map<int64_t, int> alertTrackerMap;
    vector<int> metricsWithActivation;
    std::set<int64_t> noReportMetricIds;

    EXPECT_TRUE(initStatsdConfig(kConfigKey, config, uidMap, pullerManager, anomalyAlarmMonitor,
                                 periodicAlarmMonitor, timeBaseSec, timeBaseSec, allTagIds,
                                 allAtomMatchers, allConditionTrackers, allMetricProducers,
                                 allAnomalyTrackers, allAlarmTrackers, conditionToMetricMap,
                                 trackerToMetricMap, trackerToConditionMap,
                                 activationAtomTrackerToMetricMap, deactivationAtomTrackerToMetricMap,
                                 alertTrackerMap, metricsWithActivation,
                                 noReportMetricIds));
    ASSERT_EQ(1u, allMetricProducers.size());
    ASSERT_EQ(1u, allAnomalyTrackers.size());
    ASSERT_EQ(1u, noReportMetricIds.size());
    ASSERT_EQ(1u, alertTrackerMap.size());
    EXPECT_NE(alertTrackerMap.find(kAlertId), alertTrackerMap.end());
    EXPECT_EQ(alertTrackerMap.find(kAlertId)->second, 0);
}

TEST(MetricsManagerTest, TestDimensionMetricsWithMultiTags) {
    UidMap uidMap;
    sp<StatsPullerManager> pullerManager = new StatsPullerManager();
    sp<AlarmMonitor> anomalyAlarmMonitor;
    sp<AlarmMonitor> periodicAlarmMonitor;
    StatsdConfig config = buildDimensionMetricsWithMultiTags();
    set<int> allTagIds;
    vector<sp<LogMatchingTracker>> allAtomMatchers;
    vector<sp<ConditionTracker>> allConditionTrackers;
    vector<sp<MetricProducer>> allMetricProducers;
    std::vector<sp<AnomalyTracker>> allAnomalyTrackers;
    std::vector<sp<AlarmTracker>> allAlarmTrackers;
    unordered_map<int, std::vector<int>> conditionToMetricMap;
    unordered_map<int, std::vector<int>> trackerToMetricMap;
    unordered_map<int, std::vector<int>> trackerToConditionMap;
    unordered_map<int, std::vector<int>> activationAtomTrackerToMetricMap;
    unordered_map<int, std::vector<int>> deactivationAtomTrackerToMetricMap;
    unordered_map<int64_t, int> alertTrackerMap;
    vector<int> metricsWithActivation;
    std::set<int64_t> noReportMetricIds;

    EXPECT_FALSE(initStatsdConfig(kConfigKey, config, uidMap, pullerManager, anomalyAlarmMonitor,
                                  periodicAlarmMonitor, timeBaseSec, timeBaseSec, allTagIds,
                                  allAtomMatchers, allConditionTrackers, allMetricProducers,
                                  allAnomalyTrackers, allAlarmTrackers, conditionToMetricMap,
                                  trackerToMetricMap, trackerToConditionMap,
                                  activationAtomTrackerToMetricMap, deactivationAtomTrackerToMetricMap,
                                  alertTrackerMap, metricsWithActivation,
                                  noReportMetricIds));
}

TEST(MetricsManagerTest, TestCircleLogMatcherDependency) {
    UidMap uidMap;
    sp<StatsPullerManager> pullerManager = new StatsPullerManager();
    sp<AlarmMonitor> anomalyAlarmMonitor;
    sp<AlarmMonitor> periodicAlarmMonitor;
    StatsdConfig config = buildCircleMatchers();
    set<int> allTagIds;
    vector<sp<LogMatchingTracker>> allAtomMatchers;
    vector<sp<ConditionTracker>> allConditionTrackers;
    vector<sp<MetricProducer>> allMetricProducers;
    std::vector<sp<AnomalyTracker>> allAnomalyTrackers;
    std::vector<sp<AlarmTracker>> allAlarmTrackers;
    unordered_map<int, std::vector<int>> conditionToMetricMap;
    unordered_map<int, std::vector<int>> trackerToMetricMap;
    unordered_map<int, std::vector<int>> trackerToConditionMap;
    unordered_map<int, std::vector<int>> activationAtomTrackerToMetricMap;
    unordered_map<int, std::vector<int>> deactivationAtomTrackerToMetricMap;
    unordered_map<int64_t, int> alertTrackerMap;
    vector<int> metricsWithActivation;
    std::set<int64_t> noReportMetricIds;

    EXPECT_FALSE(initStatsdConfig(kConfigKey, config, uidMap, pullerManager, anomalyAlarmMonitor,
                                  periodicAlarmMonitor, timeBaseSec, timeBaseSec, allTagIds,
                                  allAtomMatchers, allConditionTrackers, allMetricProducers,
                                  allAnomalyTrackers, allAlarmTrackers, conditionToMetricMap,
                                  trackerToMetricMap, trackerToConditionMap,
                                  activationAtomTrackerToMetricMap, deactivationAtomTrackerToMetricMap,
                                  alertTrackerMap, metricsWithActivation,
                                  noReportMetricIds));
}

TEST(MetricsManagerTest, TestMissingMatchers) {
    UidMap uidMap;
    sp<StatsPullerManager> pullerManager = new StatsPullerManager();
    sp<AlarmMonitor> anomalyAlarmMonitor;
    sp<AlarmMonitor> periodicAlarmMonitor;
    StatsdConfig config = buildMissingMatchers();
    set<int> allTagIds;
    vector<sp<LogMatchingTracker>> allAtomMatchers;
    vector<sp<ConditionTracker>> allConditionTrackers;
    vector<sp<MetricProducer>> allMetricProducers;
    std::vector<sp<AnomalyTracker>> allAnomalyTrackers;
    std::vector<sp<AlarmTracker>> allAlarmTrackers;
    unordered_map<int, std::vector<int>> conditionToMetricMap;
    unordered_map<int, std::vector<int>> trackerToMetricMap;
    unordered_map<int, std::vector<int>> trackerToConditionMap;
    unordered_map<int, std::vector<int>> activationAtomTrackerToMetricMap;
    unordered_map<int, std::vector<int>> deactivationAtomTrackerToMetricMap;
    unordered_map<int64_t, int> alertTrackerMap;
    vector<int> metricsWithActivation;
    std::set<int64_t> noReportMetricIds;
    EXPECT_FALSE(initStatsdConfig(kConfigKey, config, uidMap, pullerManager, anomalyAlarmMonitor,
                                  periodicAlarmMonitor, timeBaseSec, timeBaseSec, allTagIds,
                                  allAtomMatchers, allConditionTrackers, allMetricProducers,
                                  allAnomalyTrackers, allAlarmTrackers, conditionToMetricMap,
                                  trackerToMetricMap, trackerToConditionMap,
                                  activationAtomTrackerToMetricMap, deactivationAtomTrackerToMetricMap,
                                  alertTrackerMap, metricsWithActivation,
                                  noReportMetricIds));
}

TEST(MetricsManagerTest, TestMissingPredicate) {
    UidMap uidMap;
    sp<StatsPullerManager> pullerManager = new StatsPullerManager();
    sp<AlarmMonitor> anomalyAlarmMonitor;
    sp<AlarmMonitor> periodicAlarmMonitor;
    StatsdConfig config = buildMissingPredicate();
    set<int> allTagIds;
    vector<sp<LogMatchingTracker>> allAtomMatchers;
    vector<sp<ConditionTracker>> allConditionTrackers;
    vector<sp<MetricProducer>> allMetricProducers;
    std::vector<sp<AnomalyTracker>> allAnomalyTrackers;
    std::vector<sp<AlarmTracker>> allAlarmTrackers;
    unordered_map<int, std::vector<int>> conditionToMetricMap;
    unordered_map<int, std::vector<int>> trackerToMetricMap;
    unordered_map<int, std::vector<int>> trackerToConditionMap;
    unordered_map<int, std::vector<int>> activationAtomTrackerToMetricMap;
    unordered_map<int, std::vector<int>> deactivationAtomTrackerToMetricMap;
    unordered_map<int64_t, int> alertTrackerMap;
    vector<int> metricsWithActivation;
    std::set<int64_t> noReportMetricIds;
    EXPECT_FALSE(initStatsdConfig(kConfigKey, config, uidMap, pullerManager, anomalyAlarmMonitor,
                                  periodicAlarmMonitor, timeBaseSec, timeBaseSec, allTagIds,
                                  allAtomMatchers, allConditionTrackers, allMetricProducers,
                                  allAnomalyTrackers, allAlarmTrackers, conditionToMetricMap,
                                  trackerToMetricMap, trackerToConditionMap,
                                  activationAtomTrackerToMetricMap, deactivationAtomTrackerToMetricMap,
                                  alertTrackerMap, metricsWithActivation, noReportMetricIds));
}

TEST(MetricsManagerTest, TestCirclePredicateDependency) {
    UidMap uidMap;
    sp<StatsPullerManager> pullerManager = new StatsPullerManager();
    sp<AlarmMonitor> anomalyAlarmMonitor;
    sp<AlarmMonitor> periodicAlarmMonitor;
    StatsdConfig config = buildCirclePredicates();
    set<int> allTagIds;
    vector<sp<LogMatchingTracker>> allAtomMatchers;
    vector<sp<ConditionTracker>> allConditionTrackers;
    vector<sp<MetricProducer>> allMetricProducers;
    std::vector<sp<AnomalyTracker>> allAnomalyTrackers;
    std::vector<sp<AlarmTracker>> allAlarmTrackers;
    unordered_map<int, std::vector<int>> conditionToMetricMap;
    unordered_map<int, std::vector<int>> trackerToMetricMap;
    unordered_map<int, std::vector<int>> trackerToConditionMap;
    unordered_map<int, std::vector<int>> activationAtomTrackerToMetricMap;
    unordered_map<int, std::vector<int>> deactivationAtomTrackerToMetricMap;
    unordered_map<int64_t, int> alertTrackerMap;
    vector<int> metricsWithActivation;
    std::set<int64_t> noReportMetricIds;

    EXPECT_FALSE(initStatsdConfig(kConfigKey, config, uidMap, pullerManager, anomalyAlarmMonitor,
                                  periodicAlarmMonitor, timeBaseSec, timeBaseSec, allTagIds,
                                  allAtomMatchers, allConditionTrackers, allMetricProducers,
                                  allAnomalyTrackers, allAlarmTrackers, conditionToMetricMap,
                                  trackerToMetricMap, trackerToConditionMap,
                                  activationAtomTrackerToMetricMap, deactivationAtomTrackerToMetricMap,
                                  alertTrackerMap, metricsWithActivation,
                                  noReportMetricIds));
}

TEST(MetricsManagerTest, testAlertWithUnknownMetric) {
    UidMap uidMap;
    sp<StatsPullerManager> pullerManager = new StatsPullerManager();
    sp<AlarmMonitor> anomalyAlarmMonitor;
    sp<AlarmMonitor> periodicAlarmMonitor;
    StatsdConfig config = buildAlertWithUnknownMetric();
    set<int> allTagIds;
    vector<sp<LogMatchingTracker>> allAtomMatchers;
    vector<sp<ConditionTracker>> allConditionTrackers;
    vector<sp<MetricProducer>> allMetricProducers;
    std::vector<sp<AnomalyTracker>> allAnomalyTrackers;
    std::vector<sp<AlarmTracker>> allAlarmTrackers;
    unordered_map<int, std::vector<int>> conditionToMetricMap;
    unordered_map<int, std::vector<int>> trackerToMetricMap;
    unordered_map<int, std::vector<int>> trackerToConditionMap;
    unordered_map<int, std::vector<int>> activationAtomTrackerToMetricMap;
    unordered_map<int, std::vector<int>> deactivationAtomTrackerToMetricMap;
    unordered_map<int64_t, int> alertTrackerMap;
    vector<int> metricsWithActivation;
    std::set<int64_t> noReportMetricIds;

    EXPECT_FALSE(initStatsdConfig(kConfigKey, config, uidMap, pullerManager, anomalyAlarmMonitor,
                                  periodicAlarmMonitor, timeBaseSec, timeBaseSec, allTagIds,
                                  allAtomMatchers, allConditionTrackers, allMetricProducers,
                                  allAnomalyTrackers, allAlarmTrackers, conditionToMetricMap,
                                  trackerToMetricMap, trackerToConditionMap,
                                  activationAtomTrackerToMetricMap, deactivationAtomTrackerToMetricMap,
                                  alertTrackerMap, metricsWithActivation,
                                  noReportMetricIds));
}

TEST(MetricsManagerTest, TestLogSources) {
    string app1 = "app1";
    set<int32_t> app1Uids = {1111, 11111};
    string app2 = "app2";
    set<int32_t> app2Uids = {2222};
    string app3 = "app3";
    set<int32_t> app3Uids = {3333, 1111};

    map<string, set<int32_t>> pkgToUids;
    pkgToUids[app1] = app1Uids;
    pkgToUids[app2] = app2Uids;
    pkgToUids[app3] = app3Uids;

    int32_t atom1 = 10;
    int32_t atom2 = 20;
    int32_t atom3 = 30;
    sp<MockUidMap> uidMap = new StrictMock<MockUidMap>();
    EXPECT_CALL(*uidMap, getAppUid(_))
            .Times(4)
            .WillRepeatedly(Invoke([&pkgToUids](const string& pkg) {
                const auto& it = pkgToUids.find(pkg);
                if (it != pkgToUids.end()) {
                    return it->second;
                }
                return set<int32_t>();
            }));
    sp<MockStatsPullerManager> pullerManager = new StrictMock<MockStatsPullerManager>();
    EXPECT_CALL(*pullerManager, RegisterPullUidProvider(kConfigKey, _)).Times(1);
    EXPECT_CALL(*pullerManager, UnregisterPullUidProvider(kConfigKey, _)).Times(1);

    sp<AlarmMonitor> anomalyAlarmMonitor;
    sp<AlarmMonitor> periodicAlarmMonitor;

    StatsdConfig config = buildGoodConfig();
    config.add_allowed_log_source("AID_SYSTEM");
    config.add_allowed_log_source(app1);
    config.add_default_pull_packages("AID_SYSTEM");
    config.add_default_pull_packages("AID_ROOT");

    const set<int32_t> defaultPullUids = {AID_SYSTEM, AID_ROOT};

    PullAtomPackages* pullAtomPackages = config.add_pull_atom_packages();
    pullAtomPackages->set_atom_id(atom1);
    pullAtomPackages->add_packages(app1);
    pullAtomPackages->add_packages(app3);

    pullAtomPackages = config.add_pull_atom_packages();
    pullAtomPackages->set_atom_id(atom2);
    pullAtomPackages->add_packages(app2);
    pullAtomPackages->add_packages("AID_STATSD");

    MetricsManager metricsManager(kConfigKey, config, timeBaseSec, timeBaseSec, uidMap,
                                  pullerManager, anomalyAlarmMonitor, periodicAlarmMonitor);

    EXPECT_TRUE(metricsManager.isConfigValid());

    ASSERT_EQ(metricsManager.mAllowedUid.size(), 1);
    EXPECT_EQ(metricsManager.mAllowedUid[0], AID_SYSTEM);

    ASSERT_EQ(metricsManager.mAllowedPkg.size(), 1);
    EXPECT_EQ(metricsManager.mAllowedPkg[0], app1);

    ASSERT_EQ(metricsManager.mAllowedLogSources.size(), 3);
    EXPECT_TRUE(isSubset({AID_SYSTEM}, metricsManager.mAllowedLogSources));
    EXPECT_TRUE(isSubset(app1Uids, metricsManager.mAllowedLogSources));

    ASSERT_EQ(metricsManager.mDefaultPullUids.size(), 2);
    EXPECT_TRUE(isSubset(defaultPullUids, metricsManager.mDefaultPullUids));
    ;

    vector<int32_t> atom1Uids = metricsManager.getPullAtomUids(atom1);
    ASSERT_EQ(atom1Uids.size(), 5);
    set<int32_t> expectedAtom1Uids;
    expectedAtom1Uids.insert(defaultPullUids.begin(), defaultPullUids.end());
    expectedAtom1Uids.insert(app1Uids.begin(), app1Uids.end());
    expectedAtom1Uids.insert(app3Uids.begin(), app3Uids.end());
    EXPECT_TRUE(isSubset(expectedAtom1Uids, set<int32_t>(atom1Uids.begin(), atom1Uids.end())));

    vector<int32_t> atom2Uids = metricsManager.getPullAtomUids(atom2);
    ASSERT_EQ(atom2Uids.size(), 4);
    set<int32_t> expectedAtom2Uids;
    expectedAtom1Uids.insert(defaultPullUids.begin(), defaultPullUids.end());
    expectedAtom1Uids.insert(app2Uids.begin(), app2Uids.end());
    expectedAtom1Uids.insert(AID_STATSD);
    EXPECT_TRUE(isSubset(expectedAtom2Uids, set<int32_t>(atom2Uids.begin(), atom2Uids.end())));

    vector<int32_t> atom3Uids = metricsManager.getPullAtomUids(atom3);
    ASSERT_EQ(atom3Uids.size(), 2);
    EXPECT_TRUE(isSubset(defaultPullUids, set<int32_t>(atom3Uids.begin(), atom3Uids.end())));
}

TEST(MetricsManagerTest, TestCheckLogCredentialsWhitelistedAtom) {
    sp<UidMap> uidMap;
    sp<StatsPullerManager> pullerManager = new StatsPullerManager();
    sp<AlarmMonitor> anomalyAlarmMonitor;
    sp<AlarmMonitor> periodicAlarmMonitor;

    StatsdConfig config = buildGoodConfig();
    config.add_whitelisted_atom_ids(3);
    config.add_whitelisted_atom_ids(4);

    MetricsManager metricsManager(kConfigKey, config, timeBaseSec, timeBaseSec, uidMap,
                                  pullerManager, anomalyAlarmMonitor, periodicAlarmMonitor);

    LogEvent event(0 /* uid */, 0 /* pid */);
    CreateNoValuesLogEvent(&event, 10 /* atom id */, 0 /* timestamp */);
    EXPECT_FALSE(metricsManager.checkLogCredentials(event));

    CreateNoValuesLogEvent(&event, 3 /* atom id */, 0 /* timestamp */);
    EXPECT_TRUE(metricsManager.checkLogCredentials(event));

    CreateNoValuesLogEvent(&event, 4 /* atom id */, 0 /* timestamp */);
    EXPECT_TRUE(metricsManager.checkLogCredentials(event));
}

TEST(MetricsManagerTest, TestWhitelistedAtomStateTracker) {
    sp<UidMap> uidMap;
    sp<StatsPullerManager> pullerManager = new StatsPullerManager();
    sp<AlarmMonitor> anomalyAlarmMonitor;
    sp<AlarmMonitor> periodicAlarmMonitor;

    StatsdConfig config = buildGoodConfig();
    config.add_allowed_log_source("AID_SYSTEM");
    config.add_whitelisted_atom_ids(3);
    config.add_whitelisted_atom_ids(4);

    State state;
    state.set_id(1);
    state.set_atom_id(3);

    *config.add_state() = state;

    config.mutable_count_metric(0)->add_slice_by_state(state.id());

    StateManager::getInstance().clear();

    MetricsManager metricsManager(kConfigKey, config, timeBaseSec, timeBaseSec, uidMap,
                                  pullerManager, anomalyAlarmMonitor, periodicAlarmMonitor);

    EXPECT_EQ(0, StateManager::getInstance().getStateTrackersCount());
    EXPECT_FALSE(metricsManager.isConfigValid());
}

}  // namespace statsd
}  // namespace os
}  // namespace android

#else
GTEST_LOG_(INFO) << "This test does nothing.\n";
#endif
