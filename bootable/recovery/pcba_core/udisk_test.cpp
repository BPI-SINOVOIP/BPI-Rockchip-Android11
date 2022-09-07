#include<stdio.h>
#include <stdlib.h>

#include"common.h"
#include"extra-functions.h"

#include"udisk_test.h"
#include"test_case.h"
#include "language.h"

#define SCAN_RESULT_LENGTH 128
#define SCAN_RESULT_FILE "/data/udisk_capacity.txt"

void * udisk_test(void * argv, display_callback *hook)
{

    struct testcase_info *tc_info = (struct testcase_info*)argv;
    int ret,y;
    double cap;
    FILE *fp;
    char results[SCAN_RESULT_LENGTH];

    hook->handle_refresh_screen(tc_info->y, PCBA_UCARD);
    ret =  system("chmod 777 /pcba/udisktester.sh");
    if(ret) printf("chmod udisktester.sh failed :%d\n",ret);

    char failed_msg[50];
    snprintf(failed_msg, sizeof(failed_msg), "%s:[%s]",PCBA_UCARD,PCBA_FAILED);
    ret = system("/pcba/udisktester.sh");
    if(ret < 0) {
        printf("udisk test failed.\n");
        hook->handle_refresh_screen_hl(tc_info->y, failed_msg, true);
        tc_info->result = -1;
        return argv;
    }

    fp = fopen(SCAN_RESULT_FILE, "r");
    if(fp == NULL) {
        printf("can not open %s.\n", SCAN_RESULT_FILE);
        hook->handle_refresh_screen_hl(tc_info->y, failed_msg, true);
        tc_info->result = -1;
        return argv;
    }

    memset(results, 0, SCAN_RESULT_LENGTH);
    fgets(results,50,fp);

    cap = strtod(results,NULL);
    printf("capacity : %s\n", results);
    if(cap > 0) {
        snprintf(failed_msg, sizeof(failed_msg), "%s:[%s] { %2fG }",PCBA_UCARD,PCBA_SECCESS,cap*1.0/1024/1024);
        tc_info->result = 0;
    }
    else {
        tc_info->result = -1;
    }
    hook->handle_refresh_screen_hl(tc_info->y, failed_msg, tc_info->result != 0);

    fclose(fp);
    return argv;
}
