#ifndef __UDISK_TEST_H
#define __UDISK_TEST_H

#include "display_callback.h"

void * udisk_test(void * argv, display_callback *hook);

struct udisk_msg 
{
	int result;
	int y;
};
#endif

