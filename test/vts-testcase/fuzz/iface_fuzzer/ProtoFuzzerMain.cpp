/*
 * Copyright 2016 The Android Open Source Project
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

#include "FuzzerInternal.h"
#include "ProtoFuzzerMutator.h"

#include "test/vts/proto/ComponentSpecificationMessage.pb.h"

#include <signal.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::make_unique;
using std::string;
using std::unique_ptr;
using std::vector;

// Executed when fuzzer raises SIGABRT signal. This function calls
// the signal handler from the libfuzzer library.
extern "C" void sig_handler(int signo) {
  if (signo == SIGABRT) {
    cerr << "SIGABRT noticed, please refer to device logcat for the root cause."
         << endl;
    fuzzer::Fuzzer::StaticCrashSignalCallback();
    exit(1);
  }
}

namespace android {
namespace vts {
namespace fuzzer {

#ifdef STATIC_TARGET_FQ_NAME
// Returns parameters used for static fuzzer executables.
extern ProtoFuzzerParams ExtractProtoFuzzerStaticParams(int argc, char **argv);
#endif

// 64-bit random number generator.
static unique_ptr<Random> random;
// Parameters that were passed in to fuzzer.
static ProtoFuzzerParams params;
// Used to mutate inputs to hal driver.
static unique_ptr<ProtoFuzzerMutator> mutator;
// Used to exercise HIDL HAL's API.
static unique_ptr<ProtoFuzzerRunner> runner;

static ProtoFuzzerMutatorConfig mutator_config{
    // Heuristic: values close to 0 are likely to be meaningful scalar input
    // values.
    [](Random &rand) {
      size_t dice_roll = rand(10);
      if (dice_roll < 3) {
        // With probability of 30% return an integer in range [0, 10).
        return rand(10);
      } else if (dice_roll >= 3 && dice_roll < 6) {
        // With probability of 30% return an integer in range [0, 100).
        return rand(100);
      } else if (dice_roll >= 6 && dice_roll < 9) {
        // With probability of 30% return an integer in range [0, 100).
        return rand(1000);
      }
      if (rand(10) == 0) {
        // With probability of 1% return 0xffffffffffffffff.
        return 0xffffffffffffffff;
      }
      // With probability 9% result is uniformly random.
      return rand.Rand();
    },
    // Odds of an enum being treated like a scalar are 1:1000.
    {1, 1000}};

// Executed when fuzzer process exits. We use this to print out useful
// information about the state of the fuzzer.
static void AtExit() {
  // Print currently opened interfaces.
  cerr << "Currently opened interfaces: " << endl;
  for (const auto &iface_desc : runner->GetOpenedIfaces()) {
    cerr << iface_desc.first << endl;
  }
  cerr << endl;
  cerr << runner->GetStats().StatsString();
}

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv) {
#ifdef STATIC_TARGET_FQ_NAME
  params = ExtractProtoFuzzerStaticParams(*argc, *argv);
#else
  params = ExtractProtoFuzzerParams(*argc, *argv);
#endif

  cerr << params.DebugString() << endl;

  random = make_unique<Random>(params.seed_);
  mutator = make_unique<ProtoFuzzerMutator>(
      *random.get(), ExtractPredefinedTypes(params.comp_specs_),
      mutator_config);
  runner = make_unique<ProtoFuzzerRunner>(params.comp_specs_,
                                          params.target_fq_name_.version());

  runner->Init(params.target_fq_name_.name(), params.binder_mode_);
  // Register atexit handler after all static objects' initialization.
  std::atexit(AtExit);
  // Register signal handler for SIGABRT.
  signal(SIGABRT, sig_handler);

  return 0;
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
                                          size_t max_size, unsigned int seed) {
  ExecSpec exec_spec{};
  // An Execution is randomly generated if:
  // 1. It can't be serialized from the given buffer OR
  // 2. The runner has opened interfaces that have not been touched.
  // Otherwise, the Execution is mutated.
  if (!FromArray(data, size, &exec_spec) || runner->UntouchedIfaces()) {
    exec_spec =
        mutator->RandomGen(runner->GetOpenedIfaces(), params.exec_size_);
  } else {
    mutator->Mutate(runner->GetOpenedIfaces(), &exec_spec);
  }

  if (static_cast<size_t>(exec_spec.ByteSize()) > max_size) {
    cerr << "execution specification message exceeded maximum size." << endl;
    cerr << max_size << endl;
    cerr << static_cast<size_t>(exec_spec.ByteSize()) << endl;
    std::abort();
  }
  return ToArray(data, max_size, &exec_spec);
}

extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t *data1, size_t size1,
                                            const uint8_t *data2, size_t size2,
                                            uint8_t *out, size_t max_out_size,
                                            unsigned int seed) {
  ExecSpec exec_spec1{};
  if (!FromArray(data1, size1, &exec_spec1)) {
    cerr << "Message 1 was invalid." << endl;
    exec_spec1 =
        mutator->RandomGen(runner->GetOpenedIfaces(), params.exec_size_);
  }

  ExecSpec exec_spec2{};
  if (!FromArray(data2, size2, &exec_spec2)) {
    cerr << "Message 2 was invalid." << endl;
    exec_spec2 =
        mutator->RandomGen(runner->GetOpenedIfaces(), params.exec_size_);
  }

  ExecSpec exec_spec_out{};
  for (int i = 0; i < static_cast<int>(params.exec_size_); i++) {
    FuncCall temp;
    int dice = rand() % 2;
    if (dice == 0) {
      temp = exec_spec1.function_call(i);
    } else {
      temp = exec_spec2.function_call(i);
    }
    exec_spec_out.add_function_call()->CopyFrom(temp);
  }

  if (static_cast<size_t>(exec_spec_out.ByteSize()) > max_out_size) {
    cerr << "execution specification message exceeded maximum size." << endl;
    cerr << max_out_size << endl;
    cerr << static_cast<size_t>(exec_spec_out.ByteSize()) << endl;
    std::abort();
  }
  return ToArray(out, max_out_size, &exec_spec_out);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  ExecSpec exec_spec{};
  if (!FromArray(data, size, &exec_spec)) {
    cerr << "Failed to deserialize an ExecSpec." << endl;
    // Don't generate an ExecSpec here so that libFuzzer knows that the provided
    // buffer doesn't provide any coverage.
    return 0;
  }
  runner->Execute(exec_spec);
  return 0;
}

}  // namespace fuzzer
}  // namespace vts
}  // namespace android
