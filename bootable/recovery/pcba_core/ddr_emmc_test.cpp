#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "math.h"
#include "test_case.h"
#include "Language/language.h"
#include "ddr_emmc_test.h"

#define READ_DDR_COMMAND "cat /proc/meminfo | grep MemTotal | cut -d ':' -f 2 | \
	cut -d 'k' -f 1 | sed 's/ //g'"

static char *emmc_path_name[] = {"/sys/block/rknand0/size",
                                 "/sys/bus/mmc/devices/mmc0:0001/block/mmcblk0/size",
                                 "/sys/bus/mmc/devices/mmc1:0001/block/mmcblk1/size",
                                 "/sys/bus/mmc/devices/mmc2:0001/block/mmcblk2/size"
                                };

/* for ddr  */
int ddr_exec(const char *cmd, char *tmp, unsigned int length)
{
    FILE *pp = popen(cmd, "r");

    if (!pp)
    return -1;
    if (fgets(tmp, length, pp) == NULL) {
        printf("popen read from cmd is NULL!\n");
        pclose(pp);
        return -1;
    }
    pclose(pp);
    return 0;
}

/* for emmc  */
static int readFromFile(const char *path, char *buf, size_t size)
{
    if (!path)
    return -1;
    FILE* fd = fopen(path,"r");
    printf("%s:%d\n", __func__,__LINE__);
    if (!fd) {
        printf("Could not open '%s'", path);
        return -1;
    }
    printf("%s:%d\n", __func__,__LINE__);
    fseek(fd, 0, SEEK_SET);
    ssize_t count = fread(buf, size,1,fd);
    printf("%s:%d buf:%s,count=%d\n", __func__,__LINE__,buf,(int)count);
    /*
    if (count == 1) {
    while (size > 0 && buf[size-1] == '\n')
    size--;
    buf[size] = '\0';
} else {
    buf[0] = '\0';
}
    */
    printf("%s:%d buf:%s\n", __func__,__LINE__,buf);
    fclose(fd);
    return count;
}

int get_emmc_size(char *size_data)
{
    char i;
    double size = (double)(atoi(size_data))/2/1024/1024;

    if (size > 0 && size <= 1)  /*1 GB */
    return 1;
    for (i = 0; i < 10; i++) {
        if (size > pow(2, i) && size <= pow(2, i+1))  /*2 - 512 GB*/
        return pow(2, i+1);
    }
    return -1;
}

void *ddr_test(void *argv, display_callback *hook)
{
    int ddr_ret = 0;
    char ddrsize_char[20];
    int ddr_size = 0;
    char out_string[128];
    struct testcase_info *tc_info = (struct testcase_info *)argv;
    int index = tc_info->y;

    memset(out_string, 0, sizeof(out_string));
    memset(ddrsize_char, 0, sizeof(ddrsize_char));
    ddr_ret = ddr_exec(READ_DDR_COMMAND,
                       ddrsize_char, sizeof(ddrsize_char));
    if (ddr_ret >= 0) {
        ddr_size = (int)(atoi(ddrsize_char)/1024);
        printf("=========== ddr_zize is : %dGB==========\n",ddr_size);
        snprintf(out_string, sizeof(out_string), "%s:[%s] { %s:%dMB }",
                 PCBA_DDR_EMMC, PCBA_SECCESS,
                 PCBA_DDR, ddr_size);
        hook->handle_refresh_screen_hl(index, out_string, false);
    } else {
        printf("=========== ddr_zize is : %dGB==========\n",ddr_size);
        snprintf(out_string, sizeof(out_string), "%s:[%s] { %s:%s }",
                 PCBA_DDR_EMMC, PCBA_FAILED,
                 PCBA_DDR, PCBA_FAILED);
        hook->handle_refresh_screen_hl(index, out_string, true);
    }
    return argv;
}

void *flash_test(void *argv, display_callback *hook) {
    int emmc_ret = 0;
    char emmcsize_char[20];
    int emmc_size = 0;
    int emmc_path_size = 0;
    int i = 0;
    char flash_type_name[20] = "null";
    char out_string[128];
    struct testcase_info *tc_info = (struct testcase_info *)argv;
    int index = tc_info->y;

    memset(out_string, 0, sizeof(out_string));
    memset(emmcsize_char, 0, sizeof(emmcsize_char));
    emmc_path_size = sizeof(emmc_path_name)/sizeof(*emmc_path_name);
    for (i=0; i < emmc_path_size ;i++) {
        printf("%s:%d,%s\n", __func__,__LINE__,emmc_path_name[i]);
        emmc_ret = readFromFile(emmc_path_name[i], emmcsize_char, sizeof(emmcsize_char));
        printf("%s:%d emmc_ret=%d\n", __func__,__LINE__,emmc_ret);
        if (emmc_ret >= 0) {  /*read back normal*/
                            emmc_size = get_emmc_size(emmcsize_char);
                            printf("%s:%d\n", __func__,__LINE__);
                            if (emmc_size < 0) {
                                emmc_ret = -1;
                            }
                            if (i==0) {
                                strcpy(flash_type_name, PCBA_NAND);
                            }else {
                                strcpy(flash_type_name, PCBA_EMMC);
                            }
                            printf("%s:%d\n", __func__,__LINE__);
                            break;
                           }
    }

    if(emmc_ret >= 0){
        snprintf(out_string, sizeof(out_string), "%s:[%s] { %s:%dGB }",
                 PCBA_DDR_EMMC, PCBA_SECCESS,
                 flash_type_name, emmc_size);
        hook->handle_refresh_screen_hl(index, out_string, false);
    }else{
        snprintf(out_string, sizeof(out_string), "%s:[%s] { %s:%dGB }",
                 PCBA_DDR_EMMC, PCBA_FAILED,
                 flash_type_name, emmc_size);
        hook->handle_refresh_screen_hl(index, out_string, true);
    }
    return argv;
}
