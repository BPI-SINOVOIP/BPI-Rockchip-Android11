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

#define LOG_TAG "memtrack_rk322x"
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

const char* mali_path = "/sys/kernel/debug/mali/gpu_memory";
const char* drm_path = "/sys/kernel/debug/dri/0/mm_dump";


struct memtrack_record record_templates[] = {
    {
        .flags = MEMTRACK_FLAG_SMAPS_ACCOUNTED |
                 MEMTRACK_FLAG_PRIVATE |
                 MEMTRACK_FLAG_NONSECURE,
     },
     {
        .flags = MEMTRACK_FLAG_SMAPS_UNACCOUNTED |
                 MEMTRACK_FLAG_PRIVATE |
                 MEMTRACK_FLAG_NONSECURE,
    },
};

        //ALOGI("FIND:%s", dp->d_name );


int gl_memtrack_get_memory(pid_t pid,
                             int type  __unused,
                             struct memtrack_record *records,
                             size_t *num_records)
{
	ALOGV("mali(%d) :*num_records=%zd",pid,*num_records);
    size_t allocated_records = MIN(*num_records, ARRAY_SIZE(record_templates));
    FILE *fp;
    char line[128];
    int native_buffer = 0;
	  int ret,mpid,buffer = 0;


    *num_records = ARRAY_SIZE(record_templates);
	ALOGV("mali:allocated_records=%zd,*num_records=%zd",allocated_records,*num_records);
    /* fastpath to return the necessary number of records */
    if (allocated_records == 0) {
		ALOGV("mali_memtrack_get_memory allocated_records=0,return 0");
        return 0;
    }
    memcpy(records,
           record_templates,
           sizeof(struct memtrack_record) * allocated_records);

    ALOGV("Open mali file: %s",mali_path);
    fp = fopen(mali_path, "r");
    if (fp == NULL) {
       //ALOGE("num_records is invalid when for:%s",ion_file);
        return -errno;
    }
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    for (;;)
    {
      if (fgets(line, sizeof(line), fp) == NULL) {
                break;
      }
	   ret=sscanf(line,"%*s %d %d",&mpid,&buffer);
	   if(ret!=2)
	   	 continue;
	   ALOGV("line:%s,pid:%d\n",line,pid);
      //TODO:flit mali size from all lines.
      //line format:Total allocated memory: 5411552
 		 if(mpid==pid)
      {
			 ALOGV("pid:%d,size:%d",pid,buffer);
			 native_buffer += buffer;
			 ALOGV("pid:%d,native_buffer:%d",pid,native_buffer);
			 break;
      }
      memset(line, 0, sizeof(line));
	}
    if(allocated_records > 1)
      records[1].size_in_bytes = native_buffer;
    fclose(fp);
    return 0;
}
int egl_memtrack_get_memory(pid_t pid,
                             int type	__unused,
                             struct memtrack_record *records,
                             size_t *num_records)
{
	ALOGV("drm(%d):*num_records=%zd",pid,*num_records);
    size_t allocated_records = MIN(*num_records, ARRAY_SIZE(record_templates));
    FILE *fp;
    char line[1024];
    char tmp[128];
    char *p;
    int is_surfaceflinger = 0;
    int unaccounted_size = 0;
    int ret,size= 0;
    *num_records = ARRAY_SIZE(record_templates);
	ALOGV("drm:allocated_records=%zd,num_records=%zd",allocated_records,*num_records);
    if (allocated_records == 0) {
        return 0;
    }

    snprintf(tmp, sizeof(tmp), "/proc/%d/cmdline", pid);
    fp = fopen(tmp, "r");
    if (fp != NULL) {
        if (fgets(line, sizeof(line), fp)) {
            if (strcmp(line, "/system/bin/surfaceflinger") == 0){
                is_surfaceflinger = 1;
		ALOGV("pid=%d,is_surfaceflinger is true",pid);
            }
        }
        fclose(fp);
    }
    if (is_surfaceflinger == 0) {
        return -EINVAL;
    }
    memcpy(records, record_templates,
           sizeof(struct memtrack_record) * allocated_records);
    fp = fopen(drm_path, "r");
    if (fp == NULL) {
        return -errno;
    }
    for (;;)
        {
           if (fgets(line, sizeof(line), fp) == NULL) {
                    break;
           }
           p=strstr(line,"used");
           if(p == NULL)
                 continue;
           ret=sscanf(p,"%*s %d ",&size);
           if(ret!=1)
                 continue;
           ALOGV("line:%s,size:%d\n",p,size);
                unaccounted_size = size;
        	ALOGV("DRM unaccounted_size:%d",size);
        }
     ALOGV("unaccounted_size:%d",size);
     if(allocated_records > 1)
        records[1].size_in_bytes = unaccounted_size;
    fclose(fp);

    return 0;
}



