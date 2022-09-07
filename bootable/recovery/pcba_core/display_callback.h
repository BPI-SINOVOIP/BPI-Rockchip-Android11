#ifndef _DISPLAY_CALLBACK_H_
#define _DISPLAY_CALLBACK_H_

#include <string>

typedef struct display_callback {
    void (*handle_refresh_screen)(int idx, std::string msg);
    void (*handle_refresh_screen_hl)(int idx, std::string msg, bool highlight);
} display_callback;

#endif
