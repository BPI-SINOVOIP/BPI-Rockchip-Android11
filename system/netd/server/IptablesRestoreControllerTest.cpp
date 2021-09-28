/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cinttypes>
#include <iostream>
#include <string>

#define LOG_TAG "IptablesRestoreControllerTest"
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <log/log.h>
#include <netdutils/MockSyscalls.h>
#include <netdutils/Stopwatch.h>

#include "IptablesRestoreController.h"
#include "NetdConstants.h"
#include "bpf/BpfUtils.h"

#define XT_LOCK_NAME "/system/etc/xtables.lock"
#define XT_LOCK_ATTEMPTS 10
#define XT_LOCK_POLL_INTERVAL_MS 100

#define PROC_STAT_MIN_ELEMENTS 52U
#define PROC_STAT_RSS_INDEX 23U

#define IPTABLES_COMM "(iptables-restor)"
#define IP6TABLES_COMM "(ip6tables-resto)"

using android::base::Join;
using android::base::StringAppendF;
using android::base::StringPrintf;
using android::netdutils::ScopedMockSyscalls;
using android::netdutils::Stopwatch;
using testing::Return;
using testing::StrictMock;

class IptablesRestoreControllerTest : public ::testing::Test {
public:
  IptablesRestoreController con;
  int mDefaultMaxRetries = con.MAX_RETRIES;
  int mDefaultPollTimeoutMs = con.POLL_TIMEOUT_MS;
  int mIptablesLock = -1;
  std::string mChainName;

  static void SetUpTestSuite() { blockSigpipe(); }

  void SetUp() {
    ASSERT_EQ(0, createTestChain());
  }

  void TearDown() {
    con.MAX_RETRIES = mDefaultMaxRetries;
    con.POLL_TIMEOUT_MS = mDefaultPollTimeoutMs;
    deleteTestChain();
  }

  void Init() {
    con.Init();
  }

  pid_t getIpRestorePid(const IptablesRestoreController::IptablesProcessType type) {
      return con.getIpRestorePid(type);
  };

  const std::string getProcStatPath(pid_t pid) { return StringPrintf("/proc/%d/stat", pid); }

  std::vector<std::string> parseProcStat(int fd, const std::string& path) {
      std::vector<std::string> procStat;

      char statBuf[1024];
      EXPECT_NE(-1, read(fd, statBuf, sizeof(statBuf)))
              << "Could not read from " << path << ": " << strerror(errno);

      std::stringstream stream(statBuf);
      std::string item;
      while (std::getline(stream, item, ' ')) {
          procStat.push_back(item);
      }

      EXPECT_LE(PROC_STAT_MIN_ELEMENTS, procStat.size()) << "Too few elements in " << path;
      return procStat;
  }

  void expectNoIptablesRestoreProcess(pid_t pid) {
    // We can't readlink /proc/PID/exe, because zombie processes don't have it.
    // Parse /proc/PID/stat instead.
    std::string statPath = getProcStatPath(pid);
    int fd = open(statPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
      // ENOENT means the process is gone (expected).
      ASSERT_EQ(errno, ENOENT)
        << "Unexpected error opening " << statPath << ": " << strerror(errno);
      return;
    }

    // If the PID exists, it's possible (though very unlikely) that the PID was reused. Check the
    // binary name as well, to ensure the test isn't flaky.
    std::vector<std::string> procStat = parseProcStat(fd, statPath);
    EXPECT_FALSE(procStat[1] == IPTABLES_COMM || procStat[1] == IP6TABLES_COMM)
            << "Previous iptables-restore or ip6tables-restore pid " << pid
            << " still alive: " << Join(procStat, " ");

    close(fd);
  }

  unsigned getRssPages(pid_t pid) {
      std::string statPath = getProcStatPath(pid);
      int fd = open(statPath.c_str(), O_RDONLY | O_CLOEXEC);
      EXPECT_NE(-1, fd) << "Unexpected error opening " << statPath << ": " << strerror(errno);
      if (fd == -1) return 0;

      const auto& procStat = parseProcStat(fd, statPath);
      close(fd);

      if (procStat.size() < PROC_STAT_MIN_ELEMENTS) return 0;
      EXPECT_TRUE(procStat[1] == IPTABLES_COMM || procStat[1] == IP6TABLES_COMM)
              << statPath << " is for unexpected process: " << procStat[1];

      return std::atoi(procStat[PROC_STAT_RSS_INDEX].c_str());
  }

  int createTestChain() {
    mChainName = StringPrintf("netd_unit_test_%u", arc4random_uniform(10000)).c_str();

    // Create a chain to list.
    std::vector<std::string> createCommands = {
        "*filter",
        StringPrintf(":%s -", mChainName.c_str()),
        StringPrintf("-A %s -j RETURN", mChainName.c_str()),
        "COMMIT",
        ""
    };

    int ret = con.execute(V4V6, Join(createCommands, "\n"), nullptr);
    if (ret) mChainName = "";
    return ret;
  }

  void deleteTestChain() {
    std::vector<std::string> deleteCommands = {
        "*filter",
        StringPrintf(":%s -", mChainName.c_str()),  // Flush chain (otherwise we can't delete it).
        StringPrintf("-X %s", mChainName.c_str()),  // Delete it.
        "COMMIT",
        ""
    };
    con.execute(V4V6, Join(deleteCommands, "\n"), nullptr);
    mChainName = "";
  }

  int acquireIptablesLock() {
    mIptablesLock = open(XT_LOCK_NAME, O_CREAT | O_CLOEXEC, 0600);
    if (mIptablesLock == -1) return mIptablesLock;
    int attempts;
    for (attempts = 0; attempts < XT_LOCK_ATTEMPTS; attempts++) {
      if (flock(mIptablesLock, LOCK_EX | LOCK_NB) == 0) {
        return 0;
      }
      usleep(XT_LOCK_POLL_INTERVAL_MS * 1000);
    }
    EXPECT_LT(attempts, XT_LOCK_ATTEMPTS) <<
        "Could not acquire iptables lock after " << XT_LOCK_ATTEMPTS << " attempts " <<
        XT_LOCK_POLL_INTERVAL_MS << "ms apart";
    return -1;
  }

  void releaseIptablesLock() {
    if (mIptablesLock != -1) {
      close(mIptablesLock);
    }
  }

  void setRetryParameters(int maxRetries, int pollTimeoutMs) {
    con.MAX_RETRIES = maxRetries;
    con.POLL_TIMEOUT_MS = pollTimeoutMs;
  }
};

TEST_F(IptablesRestoreControllerTest, TestBasicCommand) {
  std::string output;

  EXPECT_EQ(0, con.execute(IptablesTarget::V4V6, "#Test\n", nullptr));

  pid_t pid4 = getIpRestorePid(IptablesRestoreController::IPTABLES_PROCESS);
  pid_t pid6 = getIpRestorePid(IptablesRestoreController::IP6TABLES_PROCESS);

  EXPECT_EQ(0, con.execute(IptablesTarget::V6, "#Test\n", nullptr));
  EXPECT_EQ(0, con.execute(IptablesTarget::V4, "#Test\n", nullptr));

  EXPECT_EQ(0, con.execute(IptablesTarget::V4V6, "#Test\n", &output));
  EXPECT_EQ("#Test\n#Test\n", output);  // One for IPv4 and one for IPv6.

  // Check the PIDs are the same as they were before. If they're not, the child processes were
  // restarted, which causes a 30-60ms delay.
  EXPECT_EQ(pid4, getIpRestorePid(IptablesRestoreController::IPTABLES_PROCESS));
  EXPECT_EQ(pid6, getIpRestorePid(IptablesRestoreController::IP6TABLES_PROCESS));
}

TEST_F(IptablesRestoreControllerTest, TestRestartOnMalformedCommand) {
  std::string buffer;
  for (int i = 0; i < 50; i++) {
      IptablesTarget target = (IptablesTarget) (i % 3);
      std::string *output = (i % 2) ? &buffer : nullptr;
      ASSERT_EQ(-1, con.execute(target, "malformed command\n", output)) <<
          "Malformed command did not fail at iteration " << i;
      ASSERT_EQ(0, con.execute(target, "#Test\n", output)) <<
          "No-op command did not succeed at iteration " << i;
  }
}

TEST_F(IptablesRestoreControllerTest, TestRestartOnProcessDeath) {
  std::string output;

  // Run a command to ensure that the processes are running.
  EXPECT_EQ(0, con.execute(IptablesTarget::V4V6, "#Test\n", &output));

  pid_t pid4 = getIpRestorePid(IptablesRestoreController::IPTABLES_PROCESS);
  pid_t pid6 = getIpRestorePid(IptablesRestoreController::IP6TABLES_PROCESS);

  ASSERT_EQ(0, kill(pid4, 0)) << "iptables-restore pid " << pid4 << " does not exist";
  ASSERT_EQ(0, kill(pid6, 0)) << "ip6tables-restore pid " << pid6 << " does not exist";
  ASSERT_EQ(0, kill(pid4, SIGTERM)) << "Failed to send SIGTERM to iptables-restore pid " << pid4;
  ASSERT_EQ(0, kill(pid6, SIGTERM)) << "Failed to send SIGTERM to ip6tables-restore pid " << pid6;

  // Wait 100ms for processes to terminate.
  TEMP_FAILURE_RETRY(usleep(100 * 1000));

  // Ensure that running a new command properly restarts the processes.
  EXPECT_EQ(0, con.execute(IptablesTarget::V4V6, "#Test\n", nullptr));
  EXPECT_NE(pid4, getIpRestorePid(IptablesRestoreController::IPTABLES_PROCESS));
  EXPECT_NE(pid6, getIpRestorePid(IptablesRestoreController::IP6TABLES_PROCESS));

  // Check there are no zombies.
  expectNoIptablesRestoreProcess(pid4);
  expectNoIptablesRestoreProcess(pid6);
}

TEST_F(IptablesRestoreControllerTest, TestCommandTimeout) {
  // Don't wait 10 seconds for this test to fail.
  setRetryParameters(3, 50);

  // Expected contents of the chain.
  std::vector<std::string> expectedLines = {
      StringPrintf("Chain %s (0 references)", mChainName.c_str()),
      "target     prot opt source               destination         ",
      "RETURN     all  --  0.0.0.0/0            0.0.0.0/0           ",
      StringPrintf("Chain %s (0 references)", mChainName.c_str()),
      "target     prot opt source               destination         ",
      "RETURN     all      ::/0                 ::/0                ",
      ""
  };
  std::string expected = Join(expectedLines, "\n");

  std::vector<std::string> listCommands = {
      "*filter",
      StringPrintf("-n -L %s", mChainName.c_str()),  // List chain.
      "COMMIT",
      ""
  };
  std::string commandString = Join(listCommands, "\n");
  std::string output;

  EXPECT_EQ(0, con.execute(IptablesTarget::V4V6, commandString, &output));
  EXPECT_EQ(expected, output);

  ASSERT_EQ(0, acquireIptablesLock());
  EXPECT_EQ(-1, con.execute(IptablesTarget::V4V6, commandString, &output));
  EXPECT_EQ(-1, con.execute(IptablesTarget::V4V6, commandString, &output));
  releaseIptablesLock();

  EXPECT_EQ(0, con.execute(IptablesTarget::V4V6, commandString, &output));
  EXPECT_EQ(expected, output);
}


TEST_F(IptablesRestoreControllerTest, TestUidRuleBenchmark) {
    const std::vector<int> ITERATIONS = { 1, 5, 10 };

    const std::string IPTABLES_RESTORE_ADD =
            StringPrintf("*filter\n-I %s -m owner --uid-owner 2000000000 -j RETURN\nCOMMIT\n",
                         mChainName.c_str());
    const std::string IPTABLES_RESTORE_DEL =
            StringPrintf("*filter\n-D %s -m owner --uid-owner 2000000000 -j RETURN\nCOMMIT\n",
                         mChainName.c_str());

    for (const int iterations : ITERATIONS) {
        Stopwatch s;
        for (int i = 0; i < iterations; i++) {
            EXPECT_EQ(0, con.execute(V4V6, IPTABLES_RESTORE_ADD, nullptr));
            EXPECT_EQ(0, con.execute(V4V6, IPTABLES_RESTORE_DEL, nullptr));
        }
        int64_t timeTaken = s.getTimeAndResetUs();
        std::cerr << "    Add/del " << iterations << " UID rules via restore: " << timeTaken
                  << "us (" << (timeTaken / 2 / iterations) << "us per operation)" << std::endl;
    }
}

TEST_F(IptablesRestoreControllerTest, TestStartup) {
  // Tests that IptablesRestoreController::Init never sets its processes to null pointers if
  // fork() succeeds.
  {
    // Mock fork(), and check that initializing 100 times never results in a null pointer.
    constexpr int NUM_ITERATIONS = 100;  // Takes 100-150ms on angler.
    constexpr pid_t FAKE_PID = 2000000001;
    StrictMock<ScopedMockSyscalls> sys;

    EXPECT_CALL(sys, fork()).Times(NUM_ITERATIONS * 2).WillRepeatedly(Return(FAKE_PID));
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      Init();
      EXPECT_NE(0, getIpRestorePid(IptablesRestoreController::IPTABLES_PROCESS));
      EXPECT_NE(0, getIpRestorePid(IptablesRestoreController::IP6TABLES_PROCESS));
    }
  }

  // The controller is now in an invalid state: the pipes are connected to working iptables
  // processes, but the PIDs are set to FAKE_PID. Send a malformed command to ensure that the
  // processes terminate and close the pipes, then send a valid command to have the controller
  // re-initialize properly now that fork() is no longer mocked.
  EXPECT_EQ(-1, con.execute(V4V6, "malformed command\n", nullptr));
  EXPECT_EQ(0, con.execute(V4V6, "#Test\n", nullptr));
}

TEST_F(IptablesRestoreControllerTest, TestMemoryLeak) {
    std::string cmd = "*filter\n";

    // Keep command within PIPE_BUF (4096) just to make sure. Each line is 60 bytes including \n:
    // -I netd_unit_test_9999 -p udp -m udp --sport 12345 -j DROP
    for (int i = 0; i < 33; i++) {
        StringAppendF(&cmd, "-I %s -p udp -m udp --sport 12345 -j DROP\n", mChainName.c_str());
        StringAppendF(&cmd, "-D %s -p udp -m udp --sport 12345 -j DROP\n", mChainName.c_str());
    }
    StringAppendF(&cmd, "COMMIT\n");
    ASSERT_GE(4096U, cmd.size());

    // Run the command once in case it causes the first allocations for these iptables-restore
    // processes, and check they don't crash.
    pid_t pid4 = getIpRestorePid(IptablesRestoreController::IPTABLES_PROCESS);
    pid_t pid6 = getIpRestorePid(IptablesRestoreController::IP6TABLES_PROCESS);
    std::string output;
    EXPECT_EQ(0, con.execute(IptablesTarget::V4V6, cmd, nullptr));
    EXPECT_EQ(pid4, getIpRestorePid(IptablesRestoreController::IPTABLES_PROCESS));
    EXPECT_EQ(pid6, getIpRestorePid(IptablesRestoreController::IP6TABLES_PROCESS));

    // Check how much RAM the processes are using.
    unsigned pages4 = getRssPages(pid4);
    ASSERT_NE(0U, pages4);
    unsigned pages6 = getRssPages(pid6);
    ASSERT_NE(0U, pages6);

    // Run the command a few times and check that it doesn't crash.
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(0, con.execute(IptablesTarget::V4V6, cmd, nullptr));
    }
    EXPECT_EQ(pid4, getIpRestorePid(IptablesRestoreController::IPTABLES_PROCESS));
    EXPECT_EQ(pid6, getIpRestorePid(IptablesRestoreController::IP6TABLES_PROCESS));

    // Don't allow a leak of more than 5 pages (20kB).
    // This is more than enough for accuracy: the leak in b/162925719 fails with:
    // Expected: (5U) >= (getRssPages(pid4) - pages4), actual: 5 vs 66
    EXPECT_GE(5U, getRssPages(pid4) - pages4) << "iptables-restore leaked too many pages";
    EXPECT_GE(5U, getRssPages(pid6) - pages6) << "ip6tables-restore leaked too many pages";
}
