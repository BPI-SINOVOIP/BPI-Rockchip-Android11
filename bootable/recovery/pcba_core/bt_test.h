#ifndef __BT_TEST_H_
#define __BT_TEST_H_

#include "display_callback.h"

void* bt_test(void* argv, display_callback *hook);
struct bt_msg {
	int result;
	int x;
	int y;
	int w;
	int h;
	char *ssid;
};

#endif
