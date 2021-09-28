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

#include <gtest/gtest.h>

#include <android-base/file.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include <thread>

#include "cmd_stat_impl.h"
#include "command.h"
#include "environment.h"
#include "event_selection_set.h"
#include "get_test_data.h"
#include "test_util.h"

using namespace simpleperf;

static std::unique_ptr<Command> StatCmd() {
  return CreateCommandInstance("stat");
}

TEST(stat_cmd, no_options) { ASSERT_TRUE(StatCmd()->Run({"sleep", "1"})); }

TEST(stat_cmd, event_option) {
  ASSERT_TRUE(StatCmd()->Run({"-e", "cpu-clock,task-clock", "sleep", "1"}));
}

TEST(stat_cmd, system_wide_option) {
  TEST_IN_ROOT(ASSERT_TRUE(StatCmd()->Run({"-a", "sleep", "1"})));
}

TEST(stat_cmd, verbose_option) {
  ASSERT_TRUE(StatCmd()->Run({"--verbose", "sleep", "1"}));
}

TEST(stat_cmd, tracepoint_event) {
  TEST_IN_ROOT(ASSERT_TRUE(
      StatCmd()->Run({"-a", "-e", "sched:sched_switch", "sleep", "1"})));
}

TEST(stat_cmd, rN_event) {
  TEST_REQUIRE_HW_COUNTER();
  OMIT_TEST_ON_NON_NATIVE_ABIS();
  size_t event_number;
  if (GetBuildArch() == ARCH_ARM64 || GetBuildArch() == ARCH_ARM) {
    // As in D5.10.2 of the ARMv8 manual, ARM defines the event number space for PMU. part of the
    // space is for common event numbers (which will stay the same for all ARM chips), part of the
    // space is for implementation defined events. Here 0x08 is a common event for instructions.
    event_number = 0x08;
  } else if (GetBuildArch() == ARCH_X86_32 || GetBuildArch() == ARCH_X86_64) {
    // As in volume 3 chapter 19 of the Intel manual, 0x00c0 is the event number for instruction.
    event_number = 0x00c0;
  } else {
    GTEST_LOG_(INFO) << "Omit arch " << GetBuildArch();
    return;
  }
  std::string event_name = android::base::StringPrintf("r%zx", event_number);
  ASSERT_TRUE(StatCmd()->Run({"-e", event_name, "sleep", "1"}));
}

TEST(stat_cmd, pmu_event) {
  TEST_REQUIRE_PMU_COUNTER();
  TEST_REQUIRE_HW_COUNTER();
  std::string event_string;
  if (GetBuildArch() == ARCH_X86_64) {
    event_string = "cpu/instructions/";
  } else if (GetBuildArch() == ARCH_ARM64) {
    event_string = "armv8_pmuv3/inst_retired/";
  } else {
    GTEST_LOG_(INFO) << "Omit arch " << GetBuildArch();
    return;
  }
  TEST_IN_ROOT(ASSERT_TRUE(
      StatCmd()->Run({"-a", "-e", event_string, "sleep", "1"})));
}

TEST(stat_cmd, event_modifier) {
  TEST_REQUIRE_HW_COUNTER();
  ASSERT_TRUE(
      StatCmd()->Run({"-e", "cpu-cycles:u,cpu-cycles:k", "sleep", "1"}));
}

void RunWorkloadFunction() {
  while (true) {
    for (volatile int i = 0; i < 10000; ++i);
    usleep(1);
  }
}

void CreateProcesses(size_t count,
                     std::vector<std::unique_ptr<Workload>>* workloads) {
  workloads->clear();
  // Create workloads run longer than profiling time.
  for (size_t i = 0; i < count; ++i) {
    std::unique_ptr<Workload> workload;
    workload = Workload::CreateWorkload(RunWorkloadFunction);
    ASSERT_TRUE(workload != nullptr);
    ASSERT_TRUE(workload->Start());
    workloads->push_back(std::move(workload));
  }
}

TEST(stat_cmd, existing_processes) {
  std::vector<std::unique_ptr<Workload>> workloads;
  CreateProcesses(2, &workloads);
  std::string pid_list = android::base::StringPrintf(
      "%d,%d", workloads[0]->GetPid(), workloads[1]->GetPid());
  ASSERT_TRUE(StatCmd()->Run({"-p", pid_list, "sleep", "1"}));
}

TEST(stat_cmd, existing_threads) {
  std::vector<std::unique_ptr<Workload>> workloads;
  CreateProcesses(2, &workloads);
  // Process id can be used as thread id in linux.
  std::string tid_list = android::base::StringPrintf(
      "%d,%d", workloads[0]->GetPid(), workloads[1]->GetPid());
  ASSERT_TRUE(StatCmd()->Run({"-t", tid_list, "sleep", "1"}));
}

TEST(stat_cmd, no_monitored_threads) {
  ASSERT_FALSE(StatCmd()->Run({}));
  ASSERT_FALSE(StatCmd()->Run({""}));
}

TEST(stat_cmd, group_option) {
  TEST_REQUIRE_HW_COUNTER();
  ASSERT_TRUE(
      StatCmd()->Run({"--group", "cpu-clock,page-faults", "sleep", "1"}));
  ASSERT_TRUE(StatCmd()->Run({"--group", "cpu-cycles,instructions", "--group",
                              "cpu-cycles:u,instructions:u", "--group",
                              "cpu-cycles:k,instructions:k", "sleep", "1"}));
}

TEST(stat_cmd, auto_generated_summary) {
  TEST_REQUIRE_HW_COUNTER();
  TemporaryFile tmp_file;
  ASSERT_TRUE(StatCmd()->Run({"--group", "instructions:u,instructions:k", "-o",
                              tmp_file.path, "sleep", "1"}));
  std::string s;
  ASSERT_TRUE(android::base::ReadFileToString(tmp_file.path, &s));
  size_t pos = s.find("instructions:u");
  ASSERT_NE(s.npos, pos);
  pos = s.find("instructions:k", pos);
  ASSERT_NE(s.npos, pos);
  pos += strlen("instructions:k");
  // Check if the summary of instructions is generated.
  ASSERT_NE(s.npos, s.find("instructions", pos));
}

TEST(stat_cmd, duration_option) {
  ASSERT_TRUE(
      StatCmd()->Run({"--duration", "1.2", "-p", std::to_string(getpid()), "--in-app"}));
  ASSERT_TRUE(StatCmd()->Run({"--duration", "1", "sleep", "2"}));
}

TEST(stat_cmd, interval_option) {
  TemporaryFile tmp_file;
  ASSERT_TRUE(
    StatCmd()->Run({"--interval", "500.0", "--duration", "1.2", "-o",
          tmp_file.path, "sleep", "2"}));
  std::string s;
  ASSERT_TRUE(android::base::ReadFileToString(tmp_file.path, &s));
  size_t count = 0;
  size_t pos = 0;
  std::string subs = "statistics:";
  while((pos = s.find(subs, pos)) != s.npos) {
    pos += subs.size();
    ++count ;
  }
  ASSERT_EQ(count, 2UL);
}

TEST(stat_cmd, interval_option_in_system_wide) {
  TEST_IN_ROOT(ASSERT_TRUE(StatCmd()->Run({"-a", "--interval", "100", "--duration", "0.3"})));
}

TEST(stat_cmd, interval_only_values_option) {
  ASSERT_TRUE(StatCmd()->Run({"--interval", "500", "--interval-only-values", "sleep", "2"}));
  TEST_IN_ROOT(ASSERT_TRUE(StatCmd()->Run({"-a", "--interval", "100", "--interval-only-values",
                                           "--duration", "0.3"})));
}

TEST(stat_cmd, no_modifier_for_clock_events) {
  for (const std::string& e : {"cpu-clock", "task-clock"}) {
    for (const std::string& m : {"u", "k"}) {
      ASSERT_FALSE(StatCmd()->Run({"-e", e + ":" + m, "sleep", "0.1"}))
          << "event " << e << ":" << m;
    }
  }
}

TEST(stat_cmd, handle_SIGHUP) {
  std::thread thread([]() {
    sleep(1);
    kill(getpid(), SIGHUP);
  });
  thread.detach();
  ASSERT_TRUE(StatCmd()->Run({"sleep", "1000000"}));
}

TEST(stat_cmd, stop_when_no_more_targets) {
  std::atomic<int> tid(0);
  std::thread thread([&]() {
    tid = gettid();
    sleep(1);
  });
  thread.detach();
  while (tid == 0);
  ASSERT_TRUE(StatCmd()->Run({"-t", std::to_string(tid), "--in-app"}));
}

TEST(stat_cmd, sample_speed_should_be_zero) {
  TEST_REQUIRE_HW_COUNTER();
  EventSelectionSet set(true);
  ASSERT_TRUE(set.AddEventType("cpu-cycles"));
  set.AddMonitoredProcesses({getpid()});
  ASSERT_TRUE(set.OpenEventFiles({-1}));
  std::vector<EventAttrWithId> attrs = set.GetEventAttrWithId();
  ASSERT_GT(attrs.size(), 0u);
  for (auto& attr : attrs) {
    ASSERT_EQ(attr.attr->sample_period, 0u);
    ASSERT_EQ(attr.attr->sample_freq, 0u);
    ASSERT_EQ(attr.attr->freq, 0u);
  }
}

TEST(stat_cmd, calculating_cpu_frequency) {
  TEST_REQUIRE_HW_COUNTER();
  CaptureStdout capture;
  ASSERT_TRUE(capture.Start());
  ASSERT_TRUE(StatCmd()->Run({"--csv", "--group", "task-clock,cpu-cycles", "sleep", "1"}));
  std::string output = capture.Finish();
  double task_clock_in_ms = 0;
  uint64_t cpu_cycle_count = 0;
  double cpu_frequency = 0;
  for (auto& line : android::base::Split(output, "\n")) {
    if (line.find("task-clock") != std::string::npos) {
      ASSERT_EQ(sscanf(line.c_str(), "%lf(ms)", &task_clock_in_ms), 1);
    } else if (line.find("cpu-cycles") != std::string::npos) {
      ASSERT_EQ(sscanf(line.c_str(), "%" SCNu64 ",cpu-cycles,%lf", &cpu_cycle_count,
                       &cpu_frequency), 2);
    }
  }
  ASSERT_NE(task_clock_in_ms, 0.0f);
  ASSERT_NE(cpu_cycle_count, 0u);
  ASSERT_NE(cpu_frequency, 0.0f);
  double calculated_frequency = cpu_cycle_count / task_clock_in_ms / 1e6;
  // Accept error up to 1e-3. Because the stat cmd print values with precision 1e-6.
  ASSERT_NEAR(cpu_frequency, calculated_frequency, 1e-3);
}

TEST(stat_cmd, set_comm_in_another_thread) {
  // Test a kernel bug which was fixed in 3.15. If kernel panic happens, please cherry pick kernel
  // patch: e041e328c4b41e perf: Fix perf_event_comm() vs. exec() assumption
  TEST_REQUIRE_HW_COUNTER();

  for (size_t loop = 0; loop < 3; ++loop) {
    std::atomic<int> child_tid(0);
    std::atomic<bool> stop_child(false);
    std::thread child([&]() {
      child_tid = gettid();
      // stay on a cpu to make the monitored events of the child thread on that cpu.
      while (!stop_child) {}
    });

    while (child_tid == 0) {}

    {
      EventSelectionSet set(true);
      ASSERT_TRUE(set.AddEventType("cpu-cycles"));
      set.AddMonitoredThreads({child_tid});
      ASSERT_TRUE(set.OpenEventFiles({-1}));

      EventSelectionSet set2(true);
      ASSERT_TRUE(set2.AddEventType("instructions"));
      set2.AddMonitoredThreads({gettid()});
      ASSERT_TRUE(set2.OpenEventFiles({-1}));

      // For kernels with the bug, setting comm will make the monitored events of the child thread
      // on the cpu of the current thread.
      ASSERT_TRUE(android::base::WriteStringToFile("child",
                                                   "/proc/" + std::to_string(child_tid) + "/comm"));
      // Release monitored events. For kernels with the bug, the events still exist on the cpu of
      // the child thread.
    }

    stop_child = true;
    child.join();
    // Sleep 1s to enter and exit cpu idle, which may abort the kernel.
    sleep(1);
  }
}

static void TestStatingApps(const std::string& app_name) {
  // Bring the app to foreground.
  ASSERT_TRUE(Workload::RunCmd({"am", "start", app_name + "/.MainActivity"}));
  ASSERT_TRUE(StatCmd()->Run({"--app", app_name, "--duration", "3"}));
}

TEST(stat_cmd, app_option_for_debuggable_app) {
  TEST_REQUIRE_APPS();
  SetRunInAppToolForTesting(true, false);
  TestStatingApps("com.android.simpleperf.debuggable");
  SetRunInAppToolForTesting(false, true);
  TestStatingApps("com.android.simpleperf.debuggable");
}

TEST(stat_cmd, app_option_for_profileable_app) {
  TEST_REQUIRE_APPS();
  SetRunInAppToolForTesting(false, true);
  TestStatingApps("com.android.simpleperf.profileable");
}

TEST(stat_cmd, use_devfreq_counters_option) {
#if defined(__ANDROID__)
  TEST_IN_ROOT(StatCmd()->Run({"--use-devfreq-counters", "sleep", "0.1"}));
#else
  GTEST_LOG_(INFO) << "This test tests an option only available on Android.";
#endif
}

TEST(stat_cmd, per_thread_option) {
  ASSERT_TRUE(StatCmd()->Run({"--per-thread", "sleep", "0.1"}));
  TEST_IN_ROOT(StatCmd()->Run({"--per-thread", "-a", "--duration", "0.1"}));
}

TEST(stat_cmd, per_core_option) {
  ASSERT_TRUE(StatCmd()->Run({"--per-core", "sleep", "0.1"}));
  TEST_IN_ROOT(StatCmd()->Run({"--per-core", "-a", "--duration", "0.1"}));
}

TEST(stat_cmd, counter_sum) {
  PerfCounter counter;
  counter.value = 1;
  counter.time_enabled = 2;
  counter.time_running = 3;
  CounterSum a;
  a.FromCounter(counter);
  ASSERT_EQ(a.value, 1);
  ASSERT_EQ(a.time_enabled, 2);
  ASSERT_EQ(a.time_running, 3);
  CounterSum b = a + a;
  ASSERT_EQ(b.value, 2);
  ASSERT_EQ(b.time_enabled, 4);
  ASSERT_EQ(b.time_running, 6);
  CounterSum c = a - a;
  ASSERT_EQ(c.value, 0);
  ASSERT_EQ(c.time_enabled, 0);
  ASSERT_EQ(c.time_running, 0);
  b.ToCounter(counter);
  ASSERT_EQ(counter.value, 2);
  ASSERT_EQ(counter.time_enabled, 4);
  ASSERT_EQ(counter.time_running, 6);
}

class StatCmdSummaryBuilderTest : public ::testing::Test {
 protected:
  void AddCounter(int event_id, pid_t tid, int cpu, int value, int time_enabled, int time_running) {
    if (thread_map_.count(tid) == 0) {
      ThreadInfo& thread = thread_map_[tid];
      thread.pid = thread.tid = tid;
      thread.name = "thread" + std::to_string(tid);
    }
    if (event_id >= counters_.size()) {
      counters_.resize(event_id + 1);
      counters_[event_id].group_id = 0;
      counters_[event_id].event_name = "event" + std::to_string(event_id);
    }
    CountersInfo& info = counters_[event_id];
    info.counters.resize(info.counters.size() + 1);
    CounterInfo& counter = info.counters.back();
    counter.tid = tid;
    counter.cpu = cpu;
    counter.counter.id = 0;
    counter.counter.value = value;
    counter.counter.time_enabled = time_enabled;
    counter.counter.time_running = time_running;
  }

  std::vector<CounterSummary> BuildSummary(bool report_per_thread, bool report_per_core) {
    CounterSummaryBuilder builder(report_per_thread, report_per_core, false, thread_map_);
    for (auto& info : counters_) {
      builder.AddCountersForOneEventType(info);
    }
    return builder.Build();
  }

  std::unordered_map<pid_t, ThreadInfo> thread_map_;
  std::vector<CountersInfo> counters_;
};

TEST_F(StatCmdSummaryBuilderTest, multiple_events) {
  AddCounter(0, 0, 0, 1, 1, 1);
  AddCounter(1, 0, 0, 2, 2, 2);
  std::vector<CounterSummary> summaries = BuildSummary(false, false);
  ASSERT_EQ(summaries.size(), 2);
  ASSERT_EQ(summaries[0].type_name, "event0");
  ASSERT_EQ(summaries[0].count, 1);
  ASSERT_NEAR(summaries[0].scale, 1.0, 1e-5);
  ASSERT_EQ(summaries[1].type_name, "event1");
  ASSERT_EQ(summaries[1].count, 2);
  ASSERT_NEAR(summaries[1].scale, 1.0, 1e-5);
}

TEST_F(StatCmdSummaryBuilderTest, default_aggregate) {
  AddCounter(0, 0, 0, 1, 1, 1);
  AddCounter(0, 0, 1, 1, 1, 1);
  AddCounter(0, 1, 0, 1, 1, 1);
  AddCounter(0, 1, 1, 2, 2, 1);
  std::vector<CounterSummary> summaries = BuildSummary(false, false);
  ASSERT_EQ(summaries.size(), 1);
  ASSERT_EQ(summaries[0].count, 5);
  ASSERT_NEAR(summaries[0].scale, 1.25, 1e-5);
}

TEST_F(StatCmdSummaryBuilderTest, per_thread_aggregate) {
  AddCounter(0, 0, 0, 1, 1, 1);
  AddCounter(0, 0, 1, 1, 1, 1);
  AddCounter(0, 1, 0, 1, 1, 1);
  AddCounter(0, 1, 1, 2, 2, 1);
  std::vector<CounterSummary> summaries = BuildSummary(true, false);
  ASSERT_EQ(summaries.size(), 2);
  ASSERT_EQ(summaries[0].thread->tid, 1);
  ASSERT_EQ(summaries[0].cpu, -1);
  ASSERT_EQ(summaries[0].count, 3);
  ASSERT_NEAR(summaries[0].scale, 1.5, 1e-5);
  ASSERT_EQ(summaries[1].thread->tid, 0);
  ASSERT_EQ(summaries[0].cpu, -1);
  ASSERT_EQ(summaries[1].count, 2);
  ASSERT_NEAR(summaries[1].scale, 1.0, 1e-5);
}

TEST_F(StatCmdSummaryBuilderTest, per_core_aggregate) {
  AddCounter(0, 0, 0, 1, 1, 1);
  AddCounter(0, 0, 1, 1, 1, 1);
  AddCounter(0, 1, 0, 1, 1, 1);
  AddCounter(0, 1, 1, 2, 2, 1);
  std::vector<CounterSummary> summaries = BuildSummary(false, true);
  ASSERT_EQ(summaries.size(), 2);
  ASSERT_TRUE(summaries[0].thread == nullptr);
  ASSERT_EQ(summaries[0].cpu, 1);
  ASSERT_EQ(summaries[0].count, 3);
  ASSERT_NEAR(summaries[0].scale, 1.5, 1e-5);
  ASSERT_TRUE(summaries[1].thread == nullptr);
  ASSERT_EQ(summaries[1].cpu, 0);
  ASSERT_EQ(summaries[1].count, 2);
  ASSERT_NEAR(summaries[1].scale, 1.0, 1e-5);
}

TEST_F(StatCmdSummaryBuilderTest, per_thread_core_aggregate) {
  AddCounter(0, 0, 0, 1, 1, 1);
  AddCounter(0, 0, 1, 2, 1, 1);
  AddCounter(0, 1, 0, 3, 1, 1);
  AddCounter(0, 1, 1, 4, 2, 1);
  std::vector<CounterSummary> summaries = BuildSummary(true, true);
  ASSERT_EQ(summaries.size(), 4);
  ASSERT_EQ(summaries[0].thread->tid, 1);
  ASSERT_EQ(summaries[0].cpu, 1);
  ASSERT_EQ(summaries[0].count, 4);
  ASSERT_NEAR(summaries[0].scale, 2.0, 1e-5);
  ASSERT_EQ(summaries[1].thread->tid, 1);
  ASSERT_EQ(summaries[1].cpu, 0);
  ASSERT_EQ(summaries[1].count, 3);
  ASSERT_NEAR(summaries[1].scale, 1.0, 1e-5);
  ASSERT_EQ(summaries[2].thread->tid, 0);
  ASSERT_EQ(summaries[2].cpu, 1);
  ASSERT_EQ(summaries[2].count, 2);
  ASSERT_NEAR(summaries[2].scale, 1.0, 1e-5);
  ASSERT_EQ(summaries[3].thread->tid, 0);
  ASSERT_EQ(summaries[3].cpu, 0);
  ASSERT_EQ(summaries[3].count, 1);
  ASSERT_NEAR(summaries[3].scale, 1.0, 1e-5);
}

class StatCmdSummariesTest : public ::testing::Test {
 protected:
  void AddSummary(const std::string event_name, pid_t tid, int cpu, uint64_t count,
                  uint64_t runtime_in_ns) {
    ThreadInfo* thread = nullptr;
    if (tid != -1) {
      thread = &thread_map_[tid];
    }
    summary_v_.emplace_back(event_name, "", 0, thread, cpu, count, runtime_in_ns, 1.0, false,
                            false);
  }

  const std::string* GetComment(size_t index) {
    if (!summaries_) {
      summaries_.reset(new CounterSummaries(std::move(summary_v_), false));
      summaries_->GenerateComments(1.0);
    }
    if (index < summaries_->Summaries().size()) {
      return &(summaries_->Summaries()[index].comment);
    }
    return nullptr;
  }

  std::unordered_map<pid_t, ThreadInfo> thread_map_;
  std::vector<CounterSummary> summary_v_;
  std::unique_ptr<CounterSummaries> summaries_;
};

TEST_F(StatCmdSummariesTest, task_clock_comment) {
  AddSummary("task-clock", -1, -1, 1e9, 0);
  AddSummary("task-clock", 0, -1, 2e9, 0);
  AddSummary("task-clock", -1, 0, 0.5e9, 0);
  AddSummary("task-clock", 1, 1, 3e9, 0);
  ASSERT_EQ(*GetComment(0), "1.000000 cpus used");
  ASSERT_EQ(*GetComment(1), "2.000000 cpus used");
  ASSERT_EQ(*GetComment(2), "0.500000 cpus used");
  ASSERT_EQ(*GetComment(3), "3.000000 cpus used");
}

TEST_F(StatCmdSummariesTest, cpu_cycles_comment) {
  AddSummary("cpu-cycles", -1, -1, 100, 100);
  AddSummary("cpu-cycles", 0, -1, 200, 100);
  AddSummary("cpu-cycles", -1, 0, 50, 100);
  AddSummary("cpu-cycles", 1, 1, 300, 100);
  ASSERT_EQ(*GetComment(0), "1.000000 GHz");
  ASSERT_EQ(*GetComment(1), "2.000000 GHz");
  ASSERT_EQ(*GetComment(2), "0.500000 GHz");
  ASSERT_EQ(*GetComment(3), "3.000000 GHz");
}

TEST_F(StatCmdSummariesTest, rate_comment) {
  AddSummary("branch-misses", -1, -1, 1e9, 1e9);
  AddSummary("branch-misses", 0, -1, 1e6, 1e9);
  AddSummary("branch-misses", -1, 0, 1e3, 1e9);
  AddSummary("branch-misses", 1, 1, 1, 1e9);
  ASSERT_EQ(*GetComment(0), "1.000 G/sec");
  ASSERT_EQ(*GetComment(1), "1.000 M/sec");
  ASSERT_EQ(*GetComment(2), "1.000 K/sec");
  ASSERT_EQ(*GetComment(3), "1.000 /sec");
}