#include <async_safe/log.h>
#include <stdarg.h>
#include <unistd.h>

#include "gwp_asan/optional/segv_handler.h"
#include "gwp_asan/options.h"

namespace {
void PrintfWrapper(const char *Format, ...) {
  va_list List;
  va_start(List, Format);
  async_safe_fatal_va_list("GWP-ASan", Format, List);
  va_end(List);
}
}; // anonymous namespace

namespace gwp_asan {
namespace test {
// Android version of the Printf() function for use in gwp_asan_unittest. You
// can find the declaration of this function in gwp_asan/tests/harness.h
crash_handler::Printf_t getPrintfFunction() { return PrintfWrapper; }
}; // namespace test
}; // namespace gwp_asan
