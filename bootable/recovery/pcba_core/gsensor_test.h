#ifndef _RK_GSENSOR_H
#define _RK_GSENSOR_H

#include "display_callback.h"

void* gsensor_test(void *argv, display_callback *hook);
void* gsensor_test_mpu(void *argv, display_callback *hook);
void* compass_test_mpu(void *argv, display_callback *hook);

struct gsensor_msg
{
	int result;
	int y;
};

#endif
