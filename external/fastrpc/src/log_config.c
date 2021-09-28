/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VERIFY_PRINT_ERROR
#define VERIFY_PRINT_ERROR
#endif // VERIFY_PRINT_ERROR
#include <pthread.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "apps_std.h"
#include "AEEstd.h"
#include "AEEStdErr.h"
#include "verify.h"
#include "remote_priv.h"
#include "adsp_current_process.h"
#include "adsp_current_process1.h"
#include "adspmsgd_adsp.h"
#include "adspmsgd_adsp1.h"
#include "rpcmem.h"

#define EVENT_SIZE          ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN       ( 1024 * ( EVENT_SIZE + 16 ) )
#ifndef AEE_EUNSUPPORTED
#define AEE_EUNSUPPORTED         20 // API is not supported 	50
#endif
#define DEFAULT_ADSPMSGD_MEMORY_SIZE     8192
#define INVALID_HANDLE      (remote_handle64)(-1)
#define ERRNO (errno == 0 ? -1 : errno)

struct log_config_watcher_params {
    int fd;
    int event_fd; // Duplicate fd to quit the poll
    _cstring1_t* paths;
    int* wd;
    uint32 numPaths;
    pthread_attr_t attr;
    pthread_t thread;
    unsigned char stopThread;
    int asidToWatch;
    char* fileToWatch;
    char* asidFileToWatch;
    char* pidFileToWatch;
    boolean adspmsgdEnabled;
};

static struct log_config_watcher_params log_config_watcher[NUM_DOMAINS_EXTEND];
extern const char* __progname;

const char* get_domain_str(int domain);
remote_handle64 get_adsp_current_process1_handle(int domain);
remote_handle64 get_adspmsgd_adsp1_handle(int domain);

static int parseLogConfig(int dom, unsigned short mask, char* filenames){
    _cstring1_t* filesToLog = NULL;
    int filesToLogLen = 0;
    char* tempFiles = NULL;
    int nErr = AEE_SUCCESS;
    char *saveptr = NULL;
    char* path = NULL;
    char delim[] = {','};
    int maxPathLen = 0;
    int i = 0;
    remote_handle64 handle;

    VERIFYC(filenames != NULL, AEE_EINVALIDFILENAME);

    VERIFYC(NULL!= (tempFiles = malloc(sizeof(char) * (std_strlen(filenames) + 1))), AEE_ENOMEMORY);
    std_strlcpy(tempFiles,filenames,std_strlen(filenames) + 1);

    // Get the number of folders and max size needed
    path = strtok_r (tempFiles, delim, &saveptr);
    while (path != NULL){
        maxPathLen = STD_MAX(maxPathLen, std_strlen(path)) + 1;
        filesToLogLen++;
        path = strtok_r (NULL, delim, &saveptr);
    }

    VERIFY_IPRINTF("%s: #files: %d max_len: %d\n", log_config_watcher[dom].fileToWatch, filesToLogLen, maxPathLen);

    // Allocate memory
    VERIFYC(NULL != (filesToLog = malloc(sizeof(_cstring1_t)*filesToLogLen)), AEE_ENOMEMORY);
    for (i = 0; i < filesToLogLen; ++i){
        VERIFYC(NULL != (filesToLog[i].data = malloc(sizeof(char) * maxPathLen)), AEE_ENOMEMORY);
        filesToLog[i].dataLen = maxPathLen;
    }

    // Get the number of folders and max size needed
    std_strlcpy(tempFiles,filenames,std_strlen(filenames) + 1);
    i = 0;
    path = strtok_r (tempFiles, delim, &saveptr);
    while (path != NULL){
        VERIFYC((filesToLog != NULL) && (filesToLog[i].data != NULL) &&
               filesToLog[i].dataLen >= (int)strlen(path), AEE_EBADSIZE);
        std_strlcpy(filesToLog[i].data, path, filesToLog[i].dataLen);
        VERIFY_IPRINTF("%s: %s\n", log_config_watcher[dom].fileToWatch, filesToLog[i].data);
        path = strtok_r (NULL, delim, &saveptr);
        i++;
    }

    handle = get_adsp_current_process1_handle(dom);
    if (handle != INVALID_HANDLE) {
       VERIFY(AEE_SUCCESS == (nErr = adsp_current_process1_set_logging_params(handle, mask,filesToLog,filesToLogLen)));
    } else {
       VERIFY(AEE_SUCCESS == (nErr = adsp_current_process_set_logging_params(mask,filesToLog,filesToLogLen)));
    }

bail:
    if (filesToLog){
        for (i = 0; i < filesToLogLen; ++i){
            if (filesToLog[i].data != NULL){
                free (filesToLog[i].data);
                filesToLog[i].data = NULL;
            }
        }
        free(filesToLog);
        filesToLog = NULL;
    }

    if(tempFiles){
        free(tempFiles);
        tempFiles = NULL;
    }
    if(nErr != AEE_SUCCESS) {
	VERIFY_EPRINTF("Error %x: parse log config failed. domain %d, mask %x, filename %s\n", nErr, dom, mask, filenames);
    }
    return nErr;
}
// Read log config given the filename
static int readLogConfigFromPath(int dom, const char* base, const char* file){
    int nErr = 0;
    apps_std_FILE fp = -1;
    uint64 len;
    byte* buf = NULL;
    int readlen = 0, eof;
    unsigned short mask = 0;
    char* path = NULL;
    char* filenames = NULL;
    boolean fileExists = FALSE;
    int buf_addr = 0;
    remote_handle64 handle;

    len = std_snprintf(0, 0, "%s/%s", base, file) + 1;
    VERIFYC(NULL != (path =  malloc(sizeof(char) * len)), AEE_ENOMEMORY);
    std_snprintf(path, (int)len, "%s/%s", base, file);
    VERIFY(AEE_SUCCESS == (nErr = apps_std_fileExists(path,&fileExists)));
    if (fileExists == FALSE){
        VERIFY_IPRINTF("%s: Couldn't find file: %s\n",log_config_watcher[dom].fileToWatch, path);
        nErr = AEE_ENOSUCHFILE;
        goto bail;
    }
    if (log_config_watcher[dom].adspmsgdEnabled == FALSE){
        handle = get_adspmsgd_adsp1_handle(dom);
        if(handle != INVALID_HANDLE) {
           adspmsgd_adsp1_init2(handle);
        } else if(AEE_EUNSUPPORTED == (nErr = adspmsgd_adsp_init2())) {
            nErr = adspmsgd_adsp_init(0, RPCMEM_HEAP_DEFAULT, 0, DEFAULT_ADSPMSGD_MEMORY_SIZE, &buf_addr);
        }
        if (nErr != AEE_SUCCESS){
            VERIFY_EPRINTF("adspmsgd not supported. nErr=%x\n", nErr);
        }else{
            log_config_watcher[dom].adspmsgdEnabled = TRUE;
        }
        VERIFY_EPRINTF("Found %s. adspmsgd enabled \n", log_config_watcher[dom].fileToWatch);
    }

    VERIFY(AEE_SUCCESS == (nErr = apps_std_fopen(path, "r", &fp)));
    VERIFY(AEE_SUCCESS == (nErr = apps_std_flen(fp, &len)));

    VERIFYC(len < 511, AEE_EBADSIZE);
    VERIFYC(NULL != (buf = calloc(1, sizeof(byte) * (len + 1))), AEE_ENOMEMORY); // extra 1 byte for null character
    VERIFYC(NULL != (filenames = malloc(sizeof(byte) * len)), AEE_ENOMEMORY);
    VERIFY(AEE_SUCCESS == (nErr = apps_std_fread(fp, buf, len, &readlen, &eof)));
    VERIFYC((int)len == readlen, AEE_EFREAD);

    VERIFY_IPRINTF("%s: Config file %s contents: %s\n", log_config_watcher[dom].fileToWatch, path, buf);

    len = sscanf((const char*)buf, "0x%hx %511s", &mask, filenames);
    switch (len){
        case 1:
            VERIFY_IPRINTF("%s: Setting log mask:0x%x", log_config_watcher[dom].fileToWatch, mask);
            handle = get_adsp_current_process1_handle(dom);
            if (handle != INVALID_HANDLE) {
               VERIFY(AEE_SUCCESS == (nErr = adsp_current_process1_set_logging_params(handle,mask,NULL,0)));
            } else {
               VERIFY(AEE_SUCCESS == (nErr = adsp_current_process_set_logging_params(mask,NULL,0)));
            }
            break;
        case 2:
            VERIFY(AEE_SUCCESS == (nErr = parseLogConfig(dom, mask,filenames)));
            VERIFY_IPRINTF("%s: Setting log mask:0x%x, filename:%s", log_config_watcher[dom].fileToWatch, mask, filenames);
            break;
        default:
            VERIFY_EPRINTF("%s: No valid data found in config file %s", log_config_watcher[dom].fileToWatch, path);
            nErr = AEE_EUNSUPPORTED;
            goto bail;
    }

bail:
    if (buf != NULL){
        free(buf);
        buf = NULL;
    }

    if (filenames != NULL){
        free(filenames);
        filenames = NULL;
    }

    if (fp != -1){
        apps_std_fclose(fp);
    }

    if (path != NULL){
        free(path);
        path = NULL;
    }

    if(nErr != AEE_SUCCESS) {
	VERIFY_IPRINTF("Error %x: fopen failed for %s/%s. (%s)\n", nErr, base, file, strerror(ERRNO));
    }
    return nErr;
}


// Read log config given the watch descriptor
static int readLogConfigFromEvent(int dom, struct inotify_event *event){
    int i = 0;

    // Ensure we are looking at the right file
    for (i = 0; i < (int)log_config_watcher[dom].numPaths; ++i){
        if (log_config_watcher[dom].wd[i] == event->wd ){
            if(std_strcmp(log_config_watcher[dom].fileToWatch, event->name) == 0){
                return readLogConfigFromPath(dom, log_config_watcher[dom].paths[i].data, log_config_watcher[dom].fileToWatch);
            }else if (std_strcmp(log_config_watcher[dom].asidFileToWatch, event->name) == 0) {
                return readLogConfigFromPath(dom, log_config_watcher[dom].paths[i].data, log_config_watcher[dom].asidFileToWatch);
            }else if (std_strcmp(log_config_watcher[dom].pidFileToWatch, event->name) == 0){
                return readLogConfigFromPath(dom, log_config_watcher[dom].paths[i].data, log_config_watcher[dom].pidFileToWatch);
            }
        }
    }
    VERIFY_IPRINTF("%s: Watch descriptor %d not valid for current process", log_config_watcher[dom].fileToWatch, event->wd);
    return AEE_SUCCESS;
}


// Read log config given the watch descriptor
static int resetLogConfigFromEvent(int dom, struct inotify_event *event) {
    int i = 0;
    remote_handle64 handle;

    // Ensure we are looking at the right file
    for (i = 0; i < (int)log_config_watcher[dom].numPaths; ++i){
        if (log_config_watcher[dom].wd[i] == event->wd ){
            if( (std_strcmp(log_config_watcher[dom].fileToWatch, event->name) == 0)||
                (std_strcmp(log_config_watcher[dom].asidFileToWatch, event->name) == 0) ||
                (std_strcmp(log_config_watcher[dom].pidFileToWatch, event->name) == 0) ) {
                if (log_config_watcher[dom].adspmsgdEnabled == TRUE){
                   handle = get_adspmsgd_adsp1_handle(dom);
                   if(handle != INVALID_HANDLE) {
                      adspmsgd_adsp1_deinit(handle);
                   } else {
                      adspmsgd_adsp_deinit();
                   }
                }
                handle = get_adsp_current_process1_handle(dom);
                if (handle != INVALID_HANDLE) {
                   return adsp_current_process1_set_logging_params(handle,0,NULL,0);
                } else {
                   return adsp_current_process_set_logging_params(0,NULL,0);
                }
            }
        }
    }
    VERIFY_IPRINTF("%s: Watch descriptor %d not valid for current process", log_config_watcher[dom].fileToWatch, event->wd);
    return AEE_SUCCESS;
}


static void* file_watcher_thread(void *arg) {
    int dom = (int)(uintptr_t)arg;
    int ret = 0;
    int length = 0;
    int nErr = AEE_SUCCESS;
    int i = 0;
    char buffer[EVENT_BUF_LEN];
    struct pollfd pfd[] = {
        {log_config_watcher[dom].fd,     POLLIN, 0},
        {log_config_watcher[dom].event_fd, POLLIN, 0}
     };
    const char* fileExtension = ".farf";
    int len = 0;
    remote_handle64 handle;

    // Check for the presence of the <process_name>.farf file at bootup
    for (i = 0; i < (int)log_config_watcher[dom].numPaths; ++i){
        if (0 == readLogConfigFromPath(dom, log_config_watcher[dom].paths[i].data, log_config_watcher[dom].fileToWatch)){
            VERIFY_IPRINTF("%s: Log config File %s found.\n", log_config_watcher[dom].fileToWatch, log_config_watcher[dom].paths[i].data );
        }
    }

    while(log_config_watcher[dom].stopThread == 0){
        // Block forever
        ret = poll(pfd, 2, -1);
        if(ret < 0){
            VERIFY_EPRINTF("%s: Error polling for file change. Runtime FARF will not work for this process. errno=%x !", log_config_watcher[dom].fileToWatch, errno);
            break;
        } else if (pfd[1].revents & POLLIN) { // Check for exit
            VERIFY_IPRINTF("Received exit.\n");
            break;
        } else {
            length = read( log_config_watcher[dom].fd, buffer, EVENT_BUF_LEN );
            i = 0;
            while ( i < length ) {
                struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];
                if ( event->len ) {
                    // Get the asiD for the current process
                    // Do it once only
                    if (log_config_watcher[dom].asidToWatch == -1){
                        handle = get_adsp_current_process1_handle(dom);
                        if (handle != INVALID_HANDLE) {
                           VERIFY(AEE_SUCCESS == (nErr = adsp_current_process1_getASID(handle,(unsigned int*)&log_config_watcher[dom].asidToWatch )));
                        } else {
                           VERIFY(AEE_SUCCESS == (nErr = adsp_current_process_getASID((unsigned int*)&log_config_watcher[dom].asidToWatch )));
                        }
                        len = strlen(fileExtension) + strlen(__TOSTR__(INT_MAX));
                        VERIFYC(NULL != (log_config_watcher[dom].asidFileToWatch = malloc(sizeof(char) * len)), AEE_ENOMEMORY);
                        snprintf(log_config_watcher[dom].asidFileToWatch, len, "%d%s", log_config_watcher[dom].asidToWatch, fileExtension);
                        VERIFY_IPRINTF("%s: Watching ASID file %s\n", log_config_watcher[dom].fileToWatch, log_config_watcher[dom].asidFileToWatch);
                    }

                    VERIFY_IPRINTF("%s: %s %d.\n", log_config_watcher[dom].fileToWatch, event->name, event->mask );
                    if ( (event->mask & IN_CREATE) || (event->mask & IN_MODIFY)) {
                        VERIFY_IPRINTF("%s: File %s created.\n", log_config_watcher[dom].fileToWatch, event->name );
                        if (0 != readLogConfigFromEvent(dom, event)){
                            VERIFY_EPRINTF("%s: Error reading config file %s", log_config_watcher[dom].fileToWatch, log_config_watcher[dom].paths[i].data);
                        }
                    }
                    else if ( event->mask & IN_DELETE ) {
                        VERIFY_IPRINTF("%s: File %s deleted.\n", log_config_watcher[dom].fileToWatch, event->name );
                        if (0 != resetLogConfigFromEvent(dom, event)){
                            VERIFY_EPRINTF("%s: Error resetting FARF runtime log config" ,log_config_watcher[dom].fileToWatch);
                        }
                    }
                }

                i += EVENT_SIZE + event->len;
            }
        }
    }
bail:
    if (nErr != AEE_SUCCESS){
        VERIFY_EPRINTF("Error %x: file watcher thread exited. Runtime FARF will not work for this process. filename %s\n", nErr, log_config_watcher[dom].fileToWatch);
    }
    return NULL;
}

void deinitFileWatcher(int dom) {
    int i = 0;
    uint64 stop = 10;
    remote_handle64 handle;

    log_config_watcher[dom].stopThread = 1;
    if (0 < log_config_watcher[dom].event_fd) {
       if (write(log_config_watcher[dom].event_fd, &stop, sizeof(uint64)) != sizeof(uint64)) {
         VERIFY_EPRINTF("Error: write failed: Cannot set exit flag to watcher thread.\n");
	}
    }
    if (log_config_watcher[dom].thread) {
        pthread_join(log_config_watcher[dom].thread, NULL);
	log_config_watcher[dom].thread = 0;
    }
    if (log_config_watcher[dom].fileToWatch){
        free (log_config_watcher[dom].fileToWatch);
	log_config_watcher[dom].fileToWatch = 0;
    }
    if (log_config_watcher[dom].asidFileToWatch){
        free (log_config_watcher[dom].asidFileToWatch);
	log_config_watcher[dom].asidFileToWatch = 0;
    }
    if (log_config_watcher[dom].pidFileToWatch){
        free (log_config_watcher[dom].pidFileToWatch);
	log_config_watcher[dom].pidFileToWatch = 0;
    }
    if (log_config_watcher[dom].wd){
        for (i = 0; i < (int)log_config_watcher[dom].numPaths; ++i){
            if (log_config_watcher[dom].wd[i] != 0){
                inotify_rm_watch( log_config_watcher[dom].fd, log_config_watcher[dom].wd[i] );
            }
        }
        free(log_config_watcher[dom].wd);
        log_config_watcher[dom].wd = NULL;
    }
    if (log_config_watcher[dom].paths){
        for (i = 0; i < (int)log_config_watcher[dom].numPaths; ++i){
            if (log_config_watcher[dom].paths[i].data){
                free(log_config_watcher[dom].paths[i].data);
                log_config_watcher[dom].paths[i].data = NULL;
            }
        }
        free(log_config_watcher[dom].paths);
        log_config_watcher[dom].paths = NULL;
    }
    if(log_config_watcher[dom].fd != 0){
        close(log_config_watcher[dom].fd);
        log_config_watcher[dom].fd = 0;
    }
    if (log_config_watcher[dom].adspmsgdEnabled == TRUE){
        handle = get_adspmsgd_adsp1_handle(dom);
        if (handle != INVALID_HANDLE) {
           adspmsgd_adsp1_deinit(handle);
        } else {
           adspmsgd_adsp_deinit();
        }
	log_config_watcher[dom].adspmsgdEnabled = FALSE;
    }

    if(log_config_watcher[dom].event_fd != 0){
	close(log_config_watcher[dom].event_fd);
	log_config_watcher[dom].event_fd = 0;
    }

    log_config_watcher[dom].numPaths = 0;
}

int initFileWatcher(int dom) {
    int nErr = AEE_SUCCESS;
    const char* fileExtension = ".farf";
    uint32 len = 0;
    uint16 maxPathLen = 0;
    int i = 0;
    char* name = NULL;

    memset(&log_config_watcher[dom], 0, sizeof(struct log_config_watcher_params));
    log_config_watcher[dom].asidToWatch = 0;

    VERIFYC(NULL != (name = std_basename(__progname)), AEE_EINVALIDPROCNAME);

    len = strlen(name) + strlen(fileExtension) + 1;
    VERIFYC(NULL != (log_config_watcher[dom].fileToWatch = malloc(sizeof(char) * len)), AEE_ENOMEMORY);
    snprintf(log_config_watcher[dom].fileToWatch, len, "%s%s", name, fileExtension);

    len = strlen(fileExtension) + strlen(__TOSTR__(INT_MAX));
    VERIFYC(NULL != (log_config_watcher[dom].pidFileToWatch = malloc(sizeof(char) * len)), AEE_ENOMEMORY);
    snprintf(log_config_watcher[dom].pidFileToWatch, len, "%d%s", getpid(), fileExtension);

    VERIFY_IPRINTF("%s: Watching PID file: %s\n", log_config_watcher[dom].fileToWatch, log_config_watcher[dom].pidFileToWatch);

    log_config_watcher[dom].fd = inotify_init();
    if (log_config_watcher[dom].fd < 0){
        nErr = AEE_EINVALIDFD;
        VERIFY_EPRINTF("Error %x: inotify_init failed. errno = %s\n", nErr, strerror(errno));
        goto bail;
    }

    // Duplicate the fd, so we can use it to quit polling
    log_config_watcher[dom].event_fd = eventfd(0, 0);
    if (log_config_watcher[dom].event_fd < 0){
        nErr = AEE_EINVALIDFD;
        VERIFY_EPRINTF("Error %x: eventfd in dup failed. errno %s\n", nErr, strerror(errno));
        goto bail;
    }
    VERIFY_IPRINTF("fd = %d dupfd=%d\n", log_config_watcher[dom].fd, log_config_watcher[dom].event_fd);

    // Get the required size
    apps_std_get_search_paths_with_env("ADSP_LIBRARY_PATH", ";", NULL, 0,
        &log_config_watcher[dom].numPaths, &maxPathLen);

    maxPathLen += + 1;

    // Allocate memory
    VERIFYC(NULL != (log_config_watcher[dom].paths
           = malloc(sizeof(_cstring1_t) * log_config_watcher[dom].numPaths)), AEE_ENOMEMORY);
    VERIFYC(NULL != (log_config_watcher[dom].wd
           = malloc(sizeof(int) * log_config_watcher[dom].numPaths)), AEE_ENOMEMORY);

    for (i = 0; i < (int)log_config_watcher[dom].numPaths; ++i){
        VERIFYC( NULL != (log_config_watcher[dom].paths[i].data
               = malloc(sizeof(char) * maxPathLen)), AEE_ENOMEMORY);
        log_config_watcher[dom].paths[i].dataLen = maxPathLen;
    }

    // Get the paths
    VERIFY(AEE_SUCCESS == (nErr = apps_std_get_search_paths_with_env("ADSP_LIBRARY_PATH", ";",
           log_config_watcher[dom].paths, log_config_watcher[dom].numPaths, &len, &maxPathLen)));

    maxPathLen += 1;

    VERIFY_IPRINTF("%s: Watching folders:\n", log_config_watcher[dom].fileToWatch);
    for (i = 0; i < (int)log_config_watcher[dom].numPaths; ++i){
        // Watch for creation, deletion and modification of files in path
	VERIFY_IPRINTF("log file watcher: %s: %s\n",log_config_watcher[dom].fileToWatch, log_config_watcher[dom].paths[i].data);
	if((log_config_watcher[dom].wd[i] = inotify_add_watch (log_config_watcher[dom].fd,
				log_config_watcher[dom].paths[i].data,  IN_CREATE | IN_DELETE)) < 0) {
		VERIFY_EPRINTF("Unable to add watcher for folder %s : errno is %s\n", log_config_watcher[dom].paths[i].data, strerror(ERRNO));
	}
    }

    // Create a thread to watch for file changes
    log_config_watcher[dom].asidToWatch = -1;
    log_config_watcher[dom].stopThread = 0;
    pthread_create(&log_config_watcher[dom].thread, NULL, file_watcher_thread, (void*)(uintptr_t)dom);
bail:
    if (nErr!=AEE_SUCCESS){
        VERIFY_EPRINTF("Error %x: Failed to register with inotify file %s. Runtime FARF will not work for the process %s!", nErr, log_config_watcher[dom].fileToWatch, name);
        deinitFileWatcher(dom);
    }

    return nErr;
}
