/*
 * Copyright (C) 2013 The Android Open Source Project
 * Copyright (C) 2015 Andreas Schneider <asn@cryptomilk.org>
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

#define LOG_TAG "rk3326_memtrack"
#define LOG_NDEBUG 1

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/mman.h>

#include <log/log.h>

#include <hardware/memtrack.h>

#include "memtrack.h"
#include <stdbool.h>
#include <stdlib.h>



#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

const char *mali_path = "/sys/kernel/debug/mali0/ctx";
const char *mem_profile="mem_profile";
const char *fmt_pid = "%d_";
const char *fmt_full = "%s/%s/%s";


struct memtrack_record record_templates[] = {
    {
        .flags = MEMTRACK_FLAG_SMAPS_UNACCOUNTED |
                 MEMTRACK_FLAG_PRIVATE |
                 MEMTRACK_FLAG_NONSECURE,
    },
};

bool find_dir(pid_t pid,char * pid_path)
{
  bool res=false;
  DIR *dirp;
  char mali_file[64] = { 0 };
  snprintf(mali_file, sizeof(mali_file), fmt_pid,pid);
  int len=strlen(mali_file);
  struct dirent *dp;
  dirp = opendir(mali_path);
  while ((dp = readdir(dirp)) != NULL) {
      if(strncmp (dp->d_name,mali_file,len)==0)
      {
        //ALOGI("FIND:%s", dp->d_name );
        strcpy(pid_path,dp->d_name);
        res=true;
        break;
      }
  }
  (void) closedir(dirp);
  return res;
}


int gl_memtrack_get_memory(pid_t pid,
                             int type  __unused,
                             struct memtrack_record *records,
                             size_t *num_records)
{
    size_t allocated_records = MIN(*num_records, ARRAY_SIZE(record_templates));
    FILE *fp;
    char line[128];
    int native_buffer = 0;
    char size[20];

    char mali_file[128] = { 0 };

    *num_records = ARRAY_SIZE(record_templates);
    ALOGV("mali:allocated_records=%zd",allocated_records);
    /* fastpath to return the necessary number of records */
    if (allocated_records == 0) {
        return 0;
    }
    memcpy(records,
           record_templates,
           sizeof(struct memtrack_record) * allocated_records);
    char pid_path[100]={0};
    if(!find_dir(pid,pid_path) )
    {
      return -errno;
    }
    snprintf(mali_file, sizeof(mali_file), fmt_full, mali_path, pid_path,mem_profile);

    ALOGV("Open mali file: %s", mali_file);
    fp = fopen(mali_file, "r");
    if (fp == NULL) {
       //ALOGE("num_records is invalid when for:%s",ion_file);
        return -errno;
    }
    for (;;)
    {
      if (fgets(line, sizeof(line), fp) == NULL) {
                break;
      }
      //TODO:flit mali size from all lines.
      //line format:Total allocated memory: 5411552
      if(line[22]==':')
      {
        //ALOGD("LINE:%s len=%zd",line,strlen(line));
        strcpy(size,line+24);
        native_buffer=atoi(size);
        ALOGV("size:%d",native_buffer);
      }
      memset(line, 0, sizeof(line));
    }

    records[0].size_in_bytes = native_buffer;
    fclose(fp);

    return 0;
}

int egl_memtrack_get_memory(pid_t pid __unused,
                            int type __unused,
                            struct memtrack_record *records __unused,
                            size_t *num_records __unused)
{
    //TBD
    return  -EINVAL;
}


