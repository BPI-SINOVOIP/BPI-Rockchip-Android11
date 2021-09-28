#include "igt.h"

igt_main
{
	igt_subtest("first-subtest")
		igt_debug("Running first subtest\n");

	igt_subtest("second-subtest")
		igt_debug("Running second subtest\n");
}
