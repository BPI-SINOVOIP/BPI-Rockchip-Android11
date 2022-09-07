#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <fcntl.h>
#include <cutils/log.h>
#include "common.h"
#include "extra-functions.h"
#include "wlan_test.h"
#include "test_case.h"
#include "language.h"
#include "recovery_ui/ui.h"
#include "display_callback.h"

#include <hardware_legacy/rk_wifi.h>

#define TAG	"[PCBA,WIFI]: "
#define LOG(x...)	printf(TAG x)

#define MAX_SCAN_COUNTS	(64)
#define SCAN_RESULT_LENGTH	(128 * MAX_SCAN_COUNTS)
#define SCAN_RESULT_FILE	"/data/scan_result.txt"
#define SCAN_RESULT_FILE2	"/data/scan_result2.txt"

static char ssids[MAX_SCAN_COUNTS][128];
static char rssis[MAX_SCAN_COUNTS][128];

static char wifi_type[64] = {0};
extern int system (const char *__command) __wur;
static const char RECOGNIZE_WIFI_CHIP[] = "/data/wifi_chip";

/*
* RSSI Levels as used by notification icon
*
* Level 4  -55 <= RSSI
* Level 3  -66 <= RSSI < -55
* Level 2  -77 <= RSSI < -67
* Level 1  -88 <= RSSI < -78
* Level 0         RSSI < -88
*/
static int calc_rssi_lvl(int rssi)
{
    rssi *= -1;

    if (rssi >= -55)
    return 4;
    else if (rssi >= -66)
    return 3;
    else if (rssi >= -77)
    return 2;
    else if (rssi >= -88)
    return 1;
    else
    return 0;
}

static void process_ssid(char *dst, char *src, char *src2)
{
    char *p, *p2, *tmp, *tmp2;
    int i, j, dbm, dbm2 = 99, index = 0, rssi;

    for (i = 0; i < MAX_SCAN_COUNTS; i++) {
        /* ESSID:"PocketAP_Home" */
        tmp = &ssids[i][0];
        p = strstr(src, "ESSID:");
        if (p == NULL)
        break;
        /* skip "ESSID:" */
        p += strlen("ESSID:");
        while ((*p != '\0') && (*p != '\n'))
        *tmp++ = *p++;
        *tmp++ = '\0';
        src = p;
        /* LOG("src = %s\n", src); */

        /* Quality:4/5  Signal level:-59 dBm  Noise level:-96 dBm */
        tmp2 = &rssis[i][0];
        p2 = strstr(src2, "Signal level");
        if (p2 == NULL)
        break;
        /* skip "level=" */
        p2 += strlen("Signal level") + 1;
        /* like "-90 dBm", total 3 chars */
        *tmp2++ = *p2++;	/* '-' */
        *tmp2++ = *p2++;	/* '9' */
        *tmp2++ = *p2++;	/* '0' */
        *tmp2++ = *p2++;	/* ' ' */
        *tmp2++ = *p2++;	/* 'd' */
        *tmp2++ = *p2++;	/* 'B' */
        *tmp2++ = *p2++;	/* 'm' */
        *tmp2++ = '\0';
        src2 = p2;
        /* LOG("src2 = %s\n", src2); */
        LOG("i = %d, %s, %s\n", i, &ssids[i][0], &rssis[i][0]);
    }

    LOG("total = %d\n", i);
    if (i == 0)
    return;

    for (j = 0; j < i; j++) {
        dbm = atoi(&rssis[j][1]);	/* skip '-' */
        if (dbm == 0)
        continue;
        if (dbm < dbm2) {		/* get max rssi */
                         dbm2 = dbm;
                         index = j;
                        }
    }

    LOG("index = %d, dbm = %d\n", index, dbm2);
    LOG("select ap: %s, %s\n", &ssids[index][0], &rssis[index][0]);

    rssi = calc_rssi_lvl(atoi(&rssis[index][1]));

    sprintf(dst, "{ %s \"%d\" }", &ssids[index][0], rssi);
    }

int save_wifi_chip_type(char *type)
{
    int ret;
    int fd;
    int len;
    char buf[64];
    ret = access(RECOGNIZE_WIFI_CHIP, R_OK|W_OK);
    if ((ret == 0) || (errno == EACCES)) {
        if ((ret != 0) && (chmod(RECOGNIZE_WIFI_CHIP, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) != 0)) {
            LOG("Cannot set RW to %s : %s\n", RECOGNIZE_WIFI_CHIP,strerror(errno));
            return -1;
        }
        LOG("%s is exit\n", RECOGNIZE_WIFI_CHIP);
        return 0;
    }

    fd = TEMP_FAILURE_RETRY(open(RECOGNIZE_WIFI_CHIP, O_CREAT|O_RDWR, 0664));
    if (fd < 0) {
        LOG("Cannot create %s : %s\n", RECOGNIZE_WIFI_CHIP,strerror(errno));
        return -1;
    }
    LOG("is not exit,save wifi chip %s\n", RECOGNIZE_WIFI_CHIP);
    strcpy(buf, type);
    LOG("recognized wifi chip = %s , save to %s\n",buf, RECOGNIZE_WIFI_CHIP);
    len = strlen(buf)+1;
    if (TEMP_FAILURE_RETRY(write(fd, buf, len)) != len) {
        LOG("Error writing %s : %s\n", RECOGNIZE_WIFI_CHIP, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    if (chmod(RECOGNIZE_WIFI_CHIP, 0664) < 0) {
        LOG("Error changing permissions of %s to 0664: %s\n", RECOGNIZE_WIFI_CHIP, strerror(errno));
        unlink(RECOGNIZE_WIFI_CHIP);
        return -1;
    }
    return 1;
}

void *wlan_test(void *argv, display_callback *hook)
{
    int ret = 0, y;
    FILE *fp = NULL;
    FILE *fp2 = NULL;
    char *results = NULL;
    char *results2 = NULL;
    char ssid[100];
    struct testcase_info *tc_info = (struct testcase_info *)argv;
    char msg[50];
    snprintf(msg, sizeof(msg), "%s:[%s..]", PCBA_WIFI, PCBA_TESTING);

    y = tc_info->y;
    hook->handle_refresh_screen(tc_info->y, msg);
    usleep(2000000);

    if (wifi_type[0] == 0) {
        check_wifi_chip_type_string(wifi_type);
        save_wifi_chip_type(wifi_type);
    }
    ret = system("chmod 777 /pcba/wifi.sh");
    if (ret)
        LOG("chmod wifi.sh failed :%d\n", ret);

    ret = system("/pcba/wifi.sh");
    if (ret <= 0)
    goto error_exit;

    results = (char *)malloc(SCAN_RESULT_LENGTH);
    results2 = (char *)malloc(SCAN_RESULT_LENGTH);
    if (results == NULL || results2 == NULL)
    goto error_exit;

    fp = fopen(SCAN_RESULT_FILE, "r");
    fp2 = fopen(SCAN_RESULT_FILE2, "r");
    if (fp == NULL || fp2 == NULL)
    goto error_exit;

    memset(results, 0, SCAN_RESULT_LENGTH);
    fread(results, SCAN_RESULT_LENGTH, 1, fp);
    results[SCAN_RESULT_LENGTH - 1] = '\0';

    memset(results2, 0, SCAN_RESULT_LENGTH);
    fread(results2, SCAN_RESULT_LENGTH, 1, fp2);
    results2[SCAN_RESULT_LENGTH - 1] = '\0';

    memset(ssid, 0, 100);

    process_ssid(ssid, results, results2);

    snprintf(msg, sizeof(msg), "%s:[%s] %s", PCBA_WIFI, PCBA_SECCESS, ssid);
    hook->handle_refresh_screen_hl(tc_info->y, msg, false);
    usleep(1000000);
    tc_info->result = 0;

    fclose(fp);
    fclose(fp2);
    free(results);
    free(results2);

    LOG("wlan_test success.\n");

    return 0;

error_exit:
    if (fp)
    fclose(fp);
    if (fp2)
    fclose(fp2);
    if (results)
    free(results);
    if (results2)
    free(results2);

    snprintf(msg, sizeof(msg), "%s:[%s] %s", PCBA_WIFI, PCBA_FAILED, ssid);
    hook->handle_refresh_screen_hl(tc_info->y, msg, true);
    usleep(1000000);
    tc_info->result = -1;

    LOG("wlan_test failed.\n");

    return argv;
}
