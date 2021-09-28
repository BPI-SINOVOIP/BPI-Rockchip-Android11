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

#include "gwp_asan/common.h"
#include "gwp_asan/optional/backtrace.h"
#include "gwp_asan/optional/segv_handler.h"

#include <unwindstack/LocalUnwinder.h>
#include <unwindstack/Unwinder.h>

namespace gwp_asan {
namespace options {

// In reality, on Android, we use two separate unwinders. GWP-ASan internally
// uses a fast, frame-pointer unwinder for allocation/deallocation stack traces
// (android_unsafe_frame_pointer_chase, provided by bionic libc). When a process
// crashes, debuggerd unwinds the access trace using libunwindstack, which is a
// slow CFI-based unwinder. We don't split the unwinders in the test harness,
// and frame-pointer unwinding doesn't work properly though a signal handler, so
// we opt to use libunwindstack in this test. This has the effect that we get
// potentially more detailed stack frames in the allocation/deallocation traces
// (as we don't use the production unwinder), but that's fine for test-only.
size_t BacktraceUnwindstack(uintptr_t *TraceBuffer, size_t Size) {
  unwindstack::LocalUnwinder unwinder;
  if (!unwinder.Init()) {
    return 0;
  }
  std::vector<unwindstack::LocalFrameData> frames;
  if (!unwinder.Unwind(&frames, Size)) {
    return 0;
  }
  for (const auto& frame : frames) {
    *TraceBuffer = frame.pc;
    TraceBuffer++;
  }
  return frames.size();
}

Backtrace_t getBacktraceFunction() {
  return BacktraceUnwindstack;
}

// Build a frame for symbolization using the maps from the provided unwinder.
// The constructed frame contains just enough information to be used to
// symbolize a GWP-ASan stack trace.
static unwindstack::FrameData BuildFrame(unwindstack::Unwinder* unwinder, uintptr_t pc) {
  unwindstack::FrameData frame;

  unwindstack::Maps* maps = unwinder->GetMaps();
  unwindstack::MapInfo* map_info = maps->Find(pc);
  if (!map_info) {
    frame.rel_pc = pc;
    return frame;
  }

  unwindstack::Elf* elf =
      map_info->GetElf(unwinder->GetProcessMemory(), unwindstack::Regs::CurrentArch());

  uint64_t relative_pc = elf->GetRelPc(pc, map_info);

  // Create registers just to get PC adjustment. Doesn't matter what they point
  // to.
  unwindstack::Regs* regs = unwindstack::Regs::CreateFromLocal();
  uint64_t pc_adjustment = regs->GetPcAdjustment(relative_pc, elf);
  relative_pc -= pc_adjustment;
  // The debug PC may be different if the PC comes from the JIT.
  uint64_t debug_pc = relative_pc;

  // If we don't have a valid ELF file, check the JIT.
  if (!elf->valid()) {
    unwindstack::JitDebug jit_debug(unwinder->GetProcessMemory());
    uint64_t jit_pc = pc - pc_adjustment;
    unwindstack::Elf* jit_elf = jit_debug.GetElf(maps, jit_pc);
    if (jit_elf != nullptr) {
      debug_pc = jit_pc;
      elf = jit_elf;
    }
  }

  // Copy all the things we need into the frame for symbolization.
  frame.rel_pc = relative_pc;
  frame.pc = pc - pc_adjustment;
  frame.map_name = map_info->name;
  frame.map_elf_start_offset = map_info->elf_start_offset;
  frame.map_exact_offset = map_info->offset;
  frame.map_start = map_info->start;
  frame.map_end = map_info->end;
  frame.map_flags = map_info->flags;
  frame.map_load_bias = elf->GetLoadBias();

  if (!elf->GetFunctionName(relative_pc, &frame.function_name, &frame.function_offset)) {
    frame.function_name = "";
    frame.function_offset = 0;
  }
  return frame;
}

// This function is a good mimic as to what's happening in the out-of-process
// tombstone daemon (see debuggerd for more information). In our case, we want
// to provide symbolized backtraces during ***testing only*** here. This
// function called from a signal handler, and is extraordinarily not
// signal-safe, but works for our purposes.
void PrintBacktraceUnwindstack(uintptr_t *TraceBuffer, size_t TraceLength,
                               crash_handler::Printf_t Print) {
  unwindstack::UnwinderFromPid unwinder(
      AllocationMetadata::kMaxTraceLengthToCollect, getpid());
  unwinder.Init(unwindstack::Regs::CurrentArch());
  unwinder.SetRegs(unwindstack::Regs::CreateFromLocal());

  for (size_t i = 0; i < TraceLength; ++i) {
    unwindstack::FrameData frame_data = BuildFrame(&unwinder, TraceBuffer[i]);
    frame_data.num = i;
    Print("  %s\n", unwinder.FormatFrame(frame_data).c_str());
  }
}

crash_handler::PrintBacktrace_t getPrintBacktraceFunction() {
  return PrintBacktraceUnwindstack;
}
} // namespace options
} // namespace gwp_asan
