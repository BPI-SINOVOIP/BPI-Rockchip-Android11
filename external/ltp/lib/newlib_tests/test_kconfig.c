/*
 * Copyright (c) 2018 Cyril Hrubis <chrubis@suse.cz>
 */

#include "tst_test.h"
#include "tst_kconfig.h"

static void do_test(void)
{
	tst_res(TPASS, "Test passed!");
}

static const char *kconfigs[] = {
	"CONFIG_MMU",
	"CONFIG_EXT4_FS=m",
	"CONFIG_PGTABLE_LEVELS=4",
	NULL
};

static struct tst_test test = {
	.test_all = do_test,
	.needs_kconfigs = kconfigs,
};
