#ifndef __CODEC_TEST_H_
#define __CODEC_TEST_H_
/* note: those codec copy from hardware/rockchip/audio/tinyalsa_hal 
 * So if error, pls update from audiohal then try again
 */
#include "display_callback.h"
int rec_play_test_async();
int rec_play_test_sync();
void *codec_test(void *argc, display_callback *hook);
void set_exit(int exit);
#endif
