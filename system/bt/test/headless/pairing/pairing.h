
#pragma once

#include "test/headless/get_options.h"
#include "test/headless/headless.h"

namespace bluetooth {
namespace test {
namespace headless {

class Pairing : public HeadlessTest<int> {
 public:
  Pairing(const bluetooth::test::headless::GetOpt& options)
      : HeadlessTest<int>(options) {}
  int Run() override;
};

}  // namespace headless
}  // namespace test
}  // namespace bluetooth
