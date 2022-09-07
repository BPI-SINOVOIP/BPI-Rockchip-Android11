#ifndef __WLAN_TEST_H_
#define __WLAN_TEST_H_

#include "display_callback.h"

struct wlan_msg {
	int result;
	int x;
	int y;
	int w;
	int h;
	char *ssid;
};

void *wlan_test(void *argv, display_callback *hook);

#endif
