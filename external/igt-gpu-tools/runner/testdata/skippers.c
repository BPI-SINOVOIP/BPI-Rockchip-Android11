#include "igt.h"

igt_main
{
	igt_fixture {
		igt_require_f(false, "Skipping from fixture\n");
	}

	igt_subtest("skip-one")
		igt_debug("Should be skipped\n");

	igt_subtest("skip-two")
		igt_debug("Should be skipped\n");
}
