#include <stdio.h>
#include <stdlib.h>
#include <android-base/logging.h>

#include "extra-functions.h"
#include "common.h"
#include "sdcard_test.h"
#include "test_case.h"
#include "language.h"

#define LOG_TAG	"[sdcard]: "
#define LOG(x...)	printf(LOG_TAG x)

#define SCAN_RESULT_LENGTH	128
#define SCAN_RESULT_FILE	"/data/sd_capacity"
#define SD_INSERT_RESULT_FILE	"/data/sd_insert_info"

void *sdcard_test(void *argv, display_callback *hook)
{
    struct testcase_info *tc_info = (struct testcase_info *)argv;
    int ret, y;
    double cap;
    FILE *fp;
    char results[SCAN_RESULT_LENGTH];

    LOG("start sdcard test.");
    hook->handle_refresh_screen(tc_info->y, PCBA_SDCARD);

    #if defined(RK3288_PCBA)
    ret = system("chmod 777 /pcba/emmctester.sh");
    #else
    ret = system("chmod 777 /pcba/mmctester.sh");
    #endif

    if (ret) PLOG(ERROR) << "chmod mmctester.sh failed :" << ret;

    #if defined(RK3288_PCBA)
    ret = system("/pcba/emmctester.sh");
    #else
    ret = system("/pcba/mmctester.sh");
    #endif
    char failed_msg[50];
    snprintf(failed_msg, sizeof(failed_msg), "%s:[%s]",PCBA_SDCARD, PCBA_FAILED);

    if (ret < 0) {
        LOG("mmc test failed.\n");
        hook->handle_refresh_screen_hl(tc_info->y, PCBA_SDCARD, true);
        tc_info->result = -1;
        return argv;
    }

    fp = fopen(SCAN_RESULT_FILE, "r");
    if (fp == NULL) {
        LOG("can not open %s.\n", SCAN_RESULT_FILE);
        hook->handle_refresh_screen_hl(tc_info->y, PCBA_SDCARD, true);
        tc_info->result = -1;
        return argv;
    }

    memset(results, 0, SCAN_RESULT_LENGTH);
    fgets(results, 50, fp);

    cap = strtod(results, NULL);
    if (cap) {
        snprintf(failed_msg, sizeof(failed_msg), "%s:[%s] { %2fG }", 
                 PCBA_SDCARD, PCBA_SECCESS,
                 cap * 1.0 / 1024 / 1024);
        hook->handle_refresh_screen_hl(tc_info->y, failed_msg, false);
        tc_info->result = 0;
    }
    fclose(fp);
    return argv;
}
