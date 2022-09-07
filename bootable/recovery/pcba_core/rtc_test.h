#ifndef __RTC_TEST_H_
#define __RTC_TEST_H_

#include "display_callback.h"

void *rtc_test(void *argc, display_callback *hook);

struct rtc_msg {
	int result;
	char *date;
};

#endif
