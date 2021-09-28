/*
* Copyright (C) 2012 Invensense, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <MPLSupport.h>
#include <string.h>
#include <stdio.h>
#include "log.h"
#include "SensorBase.h"
#include <fcntl.h>
#include <unistd.h>
#include "ml_sysfs_helper.h"

int inv_read_data(char *fname, long *data)
{
    VFUNC_LOG;

    char buf[sizeof(long) * 4];
    int count, fd;

    fd = open(fname, O_RDONLY);
    if(fd < 0) {
        LOGE("HAL:Error opening %s", fname);
        return -1;
    }
    memset(buf, 0, sizeof(buf));
    count = read_attribute_sensor(fd, buf, sizeof(buf));
    if(count < 1) {
        close(fd);
        return -1;
    } else {
        count = sscanf(buf, "%ld", data);
        if(count)
            LOGV_IF(EXTRA_VERBOSE, "HAL:Data= %ld", *data);
    }
    close(fd);

    return 0;
}

/* This one DOES NOT close FDs for you */
int read_attribute_sensor(int fd, char* data, unsigned int size)
{
    VFUNC_LOG;

    int count = 0;
    if (fd > 0) {
        count = pread(fd, data, size, 0);
        if(count < 1) {
            LOGE("HAL:read fails with error code=%d", count);
        }
    }
    return count;
}

/**
 *  @brief  Enable a sensor through the sysfs file descriptor
 *          provided.
 *  @note   this function one closes FD after the write
 *  @param  fd
 *              the file descriptor to write into
 *  @param  en
 *              the value to write, typically 1 or 0
 *  @return the errno whenever applicable.
 */
int enable_sysfs_sensor(int fd, int en)
{
    VFUNC_LOG;

    int nb;
    int err = 0;

    char c = en ? '1' : '0';
    nb = write(fd, &c, 1);

    if (nb <= 0) {
        err = errno;
        LOGE("HAL:enable_sysfs_sensor - write %c returned %d (%s / %d)",
             c, nb, strerror(err), err);
    }
    close(fd);


    return -err;
}

/* This one closes FDs for you */
int write_attribute_sensor(int fd, long data)
{
    VFUNC_LOG;

    int num_b = 0;

    if (fd >= 0) {
        char buf[80];
        sprintf(buf, "%ld", data);
        num_b = write(fd, buf, strlen(buf) + 1);
        if (num_b <= 0) {
            int err = errno;
            LOGE("HAL:write fd %d returned '%s' (%d)", fd, strerror(err), err);
        } else {
            LOGV_IF(EXTRA_VERBOSE, "HAL:fd=%d write attribute to %ld", fd, data);
        }
        close(fd);
    }

    return num_b;
}

/* This one DOES NOT close FDs for you */
int write_attribute_sensor_continuous(int fd, long data)
{
    VFUNC_LOG;

    int num_b = 0;

    if (fd >= 0) {
        char buf[80];
        sprintf(buf, "%ld", data);
        num_b = write(fd, buf, strlen(buf) + 1);
        if (num_b <= 0) {
            int err = errno;
            LOGE("HAL:write fd %d returned '%s' (%d)", fd, strerror(err), err);
        } else {
            LOGV_IF(EXTRA_VERBOSE, "HAL:fd=%d write attribute to %ld", fd, data);
        }
    }

    return num_b;
}

int read_sysfs_int(char *filename, int *var)
{
    int res=0;
    FILE  *sysfsfp;

    sysfsfp = fopen(filename, "r");
    if (sysfsfp != NULL) {
        if (fscanf(sysfsfp, "%d\n", var) < 0 || fclose(sysfsfp) < 0) {    
            res = errno;
            LOGE("HAL:ERR open file %s to read with error %d", filename, res);
        }
    }
    return -res;
}

int write_sysfs_int(char *filename, int var)
{
    int res=0;
    FILE  *sysfsfp;

    sysfsfp = fopen(filename, "w");
    if (sysfsfp != NULL) {
        if (fprintf(sysfsfp, "%d\n", var) < 0 || fclose(sysfsfp) < 0) {
            res = errno;
            LOGE("HAL:ERR open file %s to write with error %d", filename, res);
        }
    }
    return -res;
}

int write_sysfs_longlong(char *filename, int64_t var)
{
    int res=0;
    FILE  *sysfsfp;

    sysfsfp = fopen(filename, "w");
    if (sysfsfp != NULL) {
        if (fprintf(sysfsfp, "%lld\n", var) < 0 || fclose(sysfsfp) < 0) {   
            res = errno;
            LOGE("HAL:ERR open file %s to write with error %d", filename, res);
        }
    }
    return -res;
}

int fill_dev_full_name_by_prefix(const char* dev_prefix, 
                                 char *dev_full_name, int len)
{
    char cand_name[20];
    int prefix_len = strlen(dev_prefix);
    strcpy(cand_name, dev_prefix);

    // try adding a number, 0-9
    for(int cand_postfix = 0; cand_postfix < 10; cand_postfix++) {
        snprintf(&cand_name[prefix_len],
                 sizeof(cand_name) / sizeof(cand_name[0]), 
                 "%d", cand_postfix);
        int dev_num = find_type_by_name(cand_name, "iio:device");
        if (dev_num != -ENODEV) {
            strcpy(dev_full_name, cand_name);
            return 0; 
        }
    }
    // try adding a small letter, a-z
    for(char cand_postfix = 'a'; cand_postfix <= 'z'; cand_postfix++) {
        snprintf(&cand_name[prefix_len], 
                 sizeof(cand_name) / sizeof(cand_name[0]), 
                 "%c", cand_postfix);
        int dev_num = find_type_by_name(cand_name, "iio:device");
        if (dev_num != -ENODEV) {
            strcpy(dev_full_name, cand_name);
            return 0; 
        }
    }
    // try adding a capital letter, A-Z
    for(char cand_postfix = 'A'; cand_postfix <= 'Z'; cand_postfix++) {
        snprintf(&cand_name[prefix_len],
                 sizeof(cand_name) / sizeof(cand_name[0]), 
                 "%c", cand_postfix);
        int dev_num = find_type_by_name(cand_name, "iio:device");
        if (dev_num != -ENODEV) {
            strcpy(dev_full_name, cand_name);
            return 0; 
        }
    }
    return 1;
}

