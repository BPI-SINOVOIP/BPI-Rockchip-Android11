#ifndef __SDCARD_TEST_H
#define __SDCARD_TEST_H

#include "display_callback.h"

struct sd_msg {
	int result;
	int y;
};

void *sdcard_test(void *argv, display_callback *hook);

#endif
