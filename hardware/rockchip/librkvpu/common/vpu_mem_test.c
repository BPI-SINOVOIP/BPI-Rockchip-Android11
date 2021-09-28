/*
 *
 * Copyright 2014 Rockchip Electronics S.LSI Co. LTD
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
/*
   * File:
   * vpu_mem_test.c
   * Description:
   * mem_test
   * Author:
   *     Alpha Lin
   * Date:
   *    2014-1-23
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cutils/log.h>
#include <include/vpu_mem.h>

#include "libion_vpu/ion_priv_vpu.h"
#include "vpu_mem_pool/vpu_mem_pool.h"

#if 0
void vpu_mem_share_test()

{
	struct ion_handle *ion_hnd;
#if 0
    int pipe_i[2];

    pipe(pipe_i);
#else
    int sd[2];
	int num_fd = 1;
	struct iovec count_vec = {
		.iov_base = &num_fd,
		.iov_len = sizeof num_fd,
	};
	char buf[CMSG_SPACE(sizeof(int))];
	socketpair(AF_UNIX, SOCK_STREAM, 0, sd);
#endif

    if (fork()) {
#if 1
        struct msghdr msg = {
			.msg_control = buf,
			.msg_controllen = sizeof buf,
			.msg_iov = &count_vec,
			.msg_iovlen = 1,
		};

		struct cmsghdr *cmsg;
#endif
        int ion_flags = 0;
        int shared_fd = 0;
        int ion_client = ion_open();
        unsigned char *cpu_ptr;

        ion_alloc(ion_client, 1920*1088*3/2, 0, /*ION_HEAP_SYSTEM_MASK*/ 2,
	                ion_flags, &ion_hnd );

        ion_share(ion_client, ion_hnd, &shared_fd);

        cpu_ptr = (unsigned char*)mmap( NULL, 1920*1088*3/2, PROT_READ | PROT_WRITE, MAP_SHARED, shared_fd, 0 );

        strcpy(cpu_ptr, "master");

        ALOGE("parent: fd = %d\n", shared_fd);
#if 0
        write(pipe_i[1], &shared_fd, 4);
#else
        cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		*(int *)CMSG_DATA(cmsg) = shared_fd;
		/* send the fd */
		sendmsg(sd[0], &msg, 0);

#endif
        sleep(5);

        ALOGE("parent quit\n");
	} else {

#if 0
        fd_set rfds;
        int status;
        struct timespec tv;
        int fd, recv_fd;
        char* ptr;
        struct ion_handle *hdl;
        int map_fd;

        FD_ZERO(&rfds);
        FD_SET (pipe_i[0], &rfds);
        tv.tv_sec = 1;
        tv.tv_nsec = 40e6;
        sigset_t set;
        sigemptyset (&set);
        sigaddset (&set, SIGALRM);

		status = pselect(pipe_i[0] + 1, &rfds, NULL, NULL, &tv, &set);
        if (0 == status) {
            // timeout
            ALOGE("child timeout\n");
        } else if (-1 == status) {
            ALOGE("%d :: Error in Select\n", __LINE__);
        } else if (FD_ISSET (pipe_i[0], &rfds)) {
            ALOGE("%d :: DATA pipe is set\n", __LINE__);
            status = read(pipe_i[0], &recv_fd, 4);
            if (status == -1) {
                ALOGE("%d :: Error while reading from the pipe\n", __LINE__);
            }
        }
#else
        struct msghdr msg;
		struct cmsghdr *cmsg;
		char* ptr;
		int fd, recv_fd;
		char* child_buf[100];
		/* child */
		struct iovec count_vec = {
			.iov_base = child_buf,
			.iov_len = sizeof child_buf,
		};

		struct msghdr child_msg = {
			.msg_control = buf,
			.msg_controllen = sizeof buf,
			.msg_iov = &count_vec,
			.msg_iovlen = 1,
		};

		if (recvmsg(sd[1], &child_msg, 0) < 0)
			perror("child recv msg 1");
		cmsg = CMSG_FIRSTHDR(&child_msg);
		if (cmsg == NULL) {
			printf("no cmsg rcvd in child");
			return;
		}
		recv_fd = *(int*)CMSG_DATA(cmsg);
        if (recv_fd < 0) {
			printf("could not get recv_fd from socket");
			return;
		}
#endif

		ALOGE("child %d\n", recv_fd);
		//fd = ion_open();
#if 1
		ptr = mmap(NULL, 1920*1088*3/2, PROT_READ | PROT_WRITE, MAP_SHARED, recv_fd, 0);
		if (ptr == MAP_FAILED) {
			return;
		}
#else
        int ion_client = ion_open();

        ion_import(ion_client, recv_fd, &hdl);

        ion_map(ion_client, hdl, 1920*1080*3/2, PROT_READ | PROT_WRITE, MAP_SHARED, 0, &ptr, &map_fd);
#endif
		ALOGE("child? [%10s] should be [master]\n", ptr);

        sleep(4);
	}
}
#else
void vpu_mem_share_test()
{
#if 0
    int pipe_i[2];

    pipe(pipe_i);
#else
    int sd[2];
	int num_fd = 1;
	struct iovec count_vec = {
		.iov_base = &num_fd,
		.iov_len = sizeof num_fd,
	};
	char buf[CMSG_SPACE(sizeof(int))];
	socketpair(AF_UNIX, SOCK_STREAM, 0, sd);
#endif

    if (fork()) {
#if 1
        struct msghdr msg = {
			.msg_control = buf,
			.msg_controllen = sizeof buf,
			.msg_iov = &count_vec,
			.msg_iovlen = 1,
		};

		struct cmsghdr *cmsg;
#endif

        VPUMemLinear_t vpumem;
        VPUMemLinear_t lnkmem;
        VPUMemLinear_t cpymem;
        /* parent */

        VPUMallocLinear(&vpumem, 1920*1088*3/2);

        ALOGE("parent: phy %08x, vir %08x\n", vpumem.phy_addr, vpumem.vir_addr);

        strcpy(vpumem.vir_addr, "master");

        VPUMemDuplicate(&lnkmem, &vpumem);

        VPUMemLink(&lnkmem);

        ALOGE("lnkmem: phy %08x, vir %08x\n", lnkmem.phy_addr, lnkmem.vir_addr);

        strcpy(lnkmem.vir_addr, "lnkmem");

        VPUMemDuplicate(&cpymem, &lnkmem);

        ALOGE("parent: fd = %d\n", cpymem.offset);
#if 0
        write(pipe_i[1], &shared_fd, 4);
#else
        cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		*(int *)CMSG_DATA(cmsg) = cpymem.offset;
		/* send the fd */
		sendmsg(sd[0], &msg, 0);

#endif

        sleep(5);

        VPUFreeLinear(&vpumem);
        VPUFreeLinear(&lnkmem);

        VPUMemLink(&cpymem);
        VPUFreeLinear(&cpymem);

        ALOGE("parent quit\n");
	} else {

#if 0
        fd_set rfds;
        int status;
        struct timespec tv;
        int fd, recv_fd;
        char* ptr;
        struct ion_handle *hdl;
        int map_fd;

        FD_ZERO(&rfds);
        FD_SET (pipe_i[0], &rfds);
        tv.tv_sec = 1;
        tv.tv_nsec = 40e6;
        sigset_t set;
        sigemptyset (&set);
        sigaddset (&set, SIGALRM);

		status = pselect(pipe_i[0] + 1, &rfds, NULL, NULL, &tv, &set);
        if (0 == status) {
            // timeout
            ALOGE("child timeout\n");
        } else if (-1 == status) {
            ALOGE("%d :: Error in Select\n", __LINE__);
        } else if (FD_ISSET (pipe_i[0], &rfds)) {
            ALOGE("%d :: DATA pipe is set\n", __LINE__);
            status = read(pipe_i[0], &recv_fd, 4);
            if (status == -1) {
                ALOGE("%d :: Error while reading from the pipe\n", __LINE__);
            }
        }
#else
        struct msghdr msg;
		struct cmsghdr *cmsg;
		char* ptr;
		int fd, recv_fd;
		char* child_buf[100];
		/* child */
		struct iovec count_vec = {
			.iov_base = child_buf,
			.iov_len = sizeof child_buf,
		};

		struct msghdr child_msg = {
			.msg_control = buf,
			.msg_controllen = sizeof buf,
			.msg_iov = &count_vec,
			.msg_iovlen = 1,
		};

		if (recvmsg(sd[1], &child_msg, 0) < 0)
			perror("child recv msg 1");
		cmsg = CMSG_FIRSTHDR(&child_msg);
		if (cmsg == NULL) {
			printf("no cmsg rcvd in child");
			return;
		}
		recv_fd = *(int*)CMSG_DATA(cmsg);
        if (recv_fd < 0) {
			printf("could not get recv_fd from socket");
			return;
		}
#endif

		ALOGE("child %d\n", recv_fd);
		//fd = ion_open();
#if 1
		ptr = mmap(NULL, 1920*1088*3/2, PROT_READ | PROT_WRITE, MAP_SHARED, recv_fd, 0);
		if (ptr == MAP_FAILED) {
			return;
		}
#else
        int ion_client = ion_open();

        ion_import(ion_client, recv_fd, &hdl);

        ion_map(ion_client, hdl, 1920*1080*3/2, PROT_READ | PROT_WRITE, MAP_SHARED, 0, &ptr, &map_fd);
#endif
		ALOGE("child? [%10s] should be [master]\n", ptr);

        sleep(4);
	}
}
#endif

int vpu_mem_alloc_test()
{
    ALOGE("%s in\n", __func__);
    do {
        int i = 0;

        int ion_client;
        ion_client = ion_open();

        while (i++ < 500) {
#if 1
            VPUMemLinear_t vpumem;
            VPUMemLinear_t cpymem;
            VPUMemLinear_t lnkmem;
            void *obj;
            /* parent */

            ALOGE("count %d\n", i);

            VPUMallocLinear(&vpumem, 1920*1088*3/2);

            ALOGE("parent: phy %08x, vir %08x\n", vpumem.phy_addr, vpumem.vir_addr);
            strcpy(vpumem.vir_addr, "master");

            VPUMemDuplicate(&cpymem, &vpumem);

            //ion_get_share(ion_client, cpymem.offset, &obj);

            VPUMemLink(&cpymem);

            VPUMemDuplicate(&lnkmem, &cpymem);
            VPUMemLink(&lnkmem);

            ALOGE("copy: phy %08x, vir %08x\n", cpymem.phy_addr, cpymem.vir_addr);

            VPUFreeLinear(&cpymem);
            VPUFreeLinear(&vpumem);
            VPUFreeLinear(&lnkmem);
#else
            struct ion_handle *handle;
            int len = 1920*1088*3/2;
            uint8_t *ptr;
            int map_fd;

            ALOGE("count %d\n", i);

            if (0 > ion_alloc(ion_client, len, 0, 2, 0, &handle)) {
                ALOGE("%s, ion_alloc failed\n", __func__);
            }
            ion_map(ion_client, handle, len, PROT_READ | PROT_WRITE, MAP_SHARED, 0, &ptr, &map_fd);

            {
                struct ion_custom_data data;
                int err;
                struct ion_phys_data phys_data;

                data.cmd = ION_IOC_GET_PHYS;
                phys_data.handle = handle;
                data.arg = (unsigned long)&phys_data;

                err = ioctl(ion_client, ION_IOC_CUSTOM, &data);
                if (err < 0) {
                    ALOGE("%s: ION_IOC_CUSTOM (%d) failed with error - %s",
                         __FUNCTION__, ION_IOC_GET_PHYS, strerror(errno));
                    //return err;
                }
            }

            ion_free(ion_client, handle);
            munmap(ptr, len);
            close(map_fd);
#endif
        }

        return 0;
    } while (0);

    return -1;
}

int vpu_mem_from_fd_test()
{
    ALOGE("%s in\n", __func__);
    do {
        int i;
        int ion_client = ion_open();
        int share_fd[20];
        VPUMemLinear_t vpumem, lnkmem;
        VPUMemLinear_t normem, cpymem, fnlmem;
        int cnt = 5;
        int len = 0x100000;

        ALOGE("%s %d\n", __func__, __LINE__);

        vpu_display_mem_pool *pool = open_vpu_memory_pool();

        ALOGE("%s %d\n", __func__, __LINE__);
#if 0
        for (i=0; i<20; i++) {
            ion_alloc_fd(ion_client, len, 0, 2, 0, &share_fd[i]);
            VPU_MEM_POOL()->commit_hdl(share_fd[i]);
        }
//#else
        ion_alloc_fd(ion_client, len, 0, 2, 0, &share_fd[0]);
        VPU_MEM_POOL()->commit_hdl(share_fd[0]);

        VPUMallocLinearFromRender(&vpumem, len);

        ALOGE("%s %d, phy_addr %x, vir_addr %x\n", __func__, __LINE__, vpumem.phy_addr, vpumem.vir_addr);

        VPUMemDuplicate(&lnkmem, &vpumem);

        VPUMemLink(&lnkmem);

        ALOGE("%s %d, phy_addr %x, vir_addr %x\n", __func__, __LINE__, lnkmem.phy_addr, lnkmem.vir_addr);

        VPUFreeLinear(&vpumem);

        ALOGE("%s %d\n", __func__, __LINE__);

        VPUFreeLinear(&lnkmem);
#endif

        while (cnt-- > 0) {

            ALOGE("cnt %d\n", cnt);
#if 0
            VPUMallocLinearFromRender(&vpumem, len);

            ALOGE("%s %d, phy_addr %x, vir_addr %x\n", __func__, __LINE__, vpumem.phy_addr, vpumem.vir_addr);

            VPUMemDuplicate(&lnkmem, &vpumem);

            VPUMemLink(&lnkmem);

            ALOGE("%s %d, phy_addr %x, vir_addr %x\n", __func__, __LINE__, lnkmem.phy_addr, lnkmem.vir_addr);

            VPUFreeLinear(&vpumem);

            ALOGE("%s %d\n", __func__, __LINE__);

            VPUFreeLinear(&lnkmem);
#else
            //////////////////////////////////////////////

            VPUMallocLinear(&normem, len);

            ALOGE("%s %d, phy_addr %x, vir_addr %x\n", __func__, __LINE__, normem.phy_addr, normem.vir_addr);

            VPUMemDuplicate(&cpymem, &normem);

            VPUMemLink(&cpymem);

            ALOGE("%s %d, phy_addr %x, vir_addr %x\n", __func__, __LINE__, cpymem.phy_addr, cpymem.vir_addr);

            VPUMemDuplicate(&fnlmem, &cpymem);
            VPUMemLink(&fnlmem);

            VPUFreeLinear(&cpymem);

            ALOGE("%s %d\n", __func__, __LINE__);
            VPUFreeLinear(&fnlmem);

            VPUFreeLinear(&normem);
#endif
        }

        close_vpu_memory_pool(pool);

        ion_close(ion_client);

        return 0;
    }
    while (0);

    return -1;
}

int prot = PROT_READ | PROT_WRITE;
int map_flags = MAP_SHARED;
int alloc_flags = 0;
size_t len = 1024*1024, align = 0;
int heap_mask = 2;

void ion_mytest1()
{
	int fd, map_fd, ret;
	size_t i;
	ion_user_handle_t handle;
    ion_user_handle_t handle2;
	unsigned long phys = 0;
	unsigned char *ptr;
	int count = 0;
    int share_fd;

	fd = ion_open();
	if (fd < 0)
		return;

    ret = ion_alloc(fd, len, align, heap_mask, alloc_flags, &handle);

    if (ret){
        printf("%s failed: %s\n", __func__, strerror(ret));
        return;
    }

    //ion_map(fd, handle, len, prot, map_flags, 0, &ptr, &map_fd);
    //close(map_fd);

    while(1) {
        unsigned char *ptrx;
        int map_fd;
    	printf("%s: TEST %d\n", __func__, count++);

        ion_share(fd, handle, &share_fd);

        ion_import(fd, share_fd, &handle2);

    	//ret = ion_map(fd, handle2, len, prot, map_flags, 0, &ptr, &map_fd);
        ptrx = mmap(NULL, len, prot, map_flags, share_fd, 0);
    	if (ret)
    		return;

    	/*unsigned int id;
    	if(ion_get_share_id(fd, map_fd, &id)) {
    		printf("%s failed ion_get_share_id", __func__);
    		return -1;
    	}
    	printf("%s: map_fd=%d, obj=%X\n", __func__, map_fd, id);*/

    	ion_get_phys(fd, handle2, &phys);
    	if(ret)
    		return;
    	printf("PHYS=%X\n", phys);

    	/* clean up properly */
    	munmap(ptrx, len);

        ret = ion_free(fd, handle2);
        close(share_fd);
    	//close(map_fd);
    }

    //munmap(ptr, len);
    ret = ion_free(fd, handle);
	ion_close(fd);
}

int main(int argc, char **argv)
{
    //ion_mytest1();
    //vpu_mem_share_test();
    vpu_mem_alloc_test();
    //vpu_mem_from_fd_test();

    return 0;
}
