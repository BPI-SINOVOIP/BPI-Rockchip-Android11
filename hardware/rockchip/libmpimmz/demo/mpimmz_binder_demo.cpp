#include <stdlib.h>
#include <utils/RefBase.h>
#include <binder/TextOutput.h>
#include <binder/IInterface.h>
#include <binder/IBinder.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>

#include "rk_mpi_mmz.h"

using namespace android;

#define BINDER_SERV_NAME        "MPI_MMZ_DEMO"

void assert_fail(const char *file, int line, const char *func, const char *expr) {
    printf("assertion failed at file %s, line %d, function %s:\n",
            file, line, func);
    printf("%s\n", expr);
    abort();
}

#define ASSERT(e) \
    do { \
        if (!(e)) \
            assert_fail(__FILE__, __LINE__, __func__, #e); \
    } while(0)

// Interface (our AIDL) - Shared by server and client
class IDemo : public IInterface {
    public:
        enum {
            GET_BUFFER = IBinder::FIRST_CALL_TRANSACTION,
            SET_BUFFER,
            FREE_BUFFER,
        };
        virtual void        setBuffer(int32_t fd, uint32_t len) = 0;

        virtual void        getBuffer(int32_t* fd, uint32_t* len) = 0;

        virtual void        freeBuffer() = 0;

        DECLARE_META_INTERFACE(Demo);
};

// Client
class BpDemo : public BpInterface<IDemo> {
    public:
        BpDemo(const sp<IBinder>& impl) : BpInterface<IDemo>(impl) {
            printf("BpDemo::BpDemo()\n");
        }

        virtual void setBuffer(int32_t fd, uint32_t len)  {
            Parcel data, reply;
            data.writeInterfaceToken(IDemo::getInterfaceDescriptor());
            data.writeFileDescriptor(fd);
            data.writeUint32(len);

            remote()->transact(SET_BUFFER, data, &reply);

            printf("BpDemo::setBuffer(fd=%d, len=%d)\n", fd, len);
        }

        virtual void getBuffer(int32_t* fd, uint32_t* len) {
            Parcel data, reply;

            data.writeInterfaceToken(IDemo::getInterfaceDescriptor());
            remote()->transact(GET_BUFFER, data, &reply);

            // 注意：这里有次dup fd
            *fd = dup(reply.readFileDescriptor());
            *len = reply.readUint32();

            printf("BpDemo::getBuffer(fd=%d, len=%d)\n", *fd, *len);
        }

        virtual void freeBuffer() {
            Parcel data;
            data.writeInterfaceToken(IDemo::getInterfaceDescriptor());
            remote()->transact(FREE_BUFFER, data, NULL);

            printf("BpDemo::freeBuffer()\n");
        }
};
IMPLEMENT_META_INTERFACE(Demo, BINDER_SERV_NAME);


// Server
class BnDemo : public BnInterface<IDemo> {
    virtual status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags = 0);
};

status_t BnDemo::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
    data.checkInterface(this);

    switch(code) {
        case SET_BUFFER: {
            int32_t fd = data.readFileDescriptor();
            uint32_t len = data.readUint32();

            // 注意：这里有次dup fd
            setBuffer(dup(fd), len);

            return NO_ERROR;
        } break;
        case GET_BUFFER: {
            int32_t fd;
            uint32_t len;

            getBuffer(&fd, &len);
            ASSERT(reply != 0);
            reply->writeFileDescriptor(fd);
            reply->writeUint32(len);

            return NO_ERROR;
        } break;
        case FREE_BUFFER: {
            freeBuffer();
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

/*
  相同返回  0
  不同返回  -1
*/
int check_data(uint8_t *data, uint8_t value, uint32_t len)
{
    for(uint32_t i=0; i<len; i++) {
        if (data[i] != value) {
            printf("unmatch in data[%d]=0x%x\n", i, data[i]);
            return -1;
        }
    }

    return 0;
}

class Server : public BnDemo {
    MB_BLK m_blk = NULL;

    virtual void setBuffer(int32_t fd, uint32_t len) {
        printf("Server::setBuffer(%d, %d)\n", fd, len);
        m_blk = RK_MPI_MMZ_ImportFD(fd, len);
        void* vaddr = RK_MPI_MMZ_Handle2VirAddr(m_blk);

        if (check_data((uint8_t*)vaddr, 0x3C, len) == 0) {
            printf("check okay.\n");
        } else {
            printf("check fail.\n");
        }
    }

    virtual void getBuffer(int32_t* fd, uint32_t* len) {
        if (m_blk == NULL) {
            if (RK_MPI_MMZ_Alloc(&m_blk, 1024*1024, 0) < 0) {
                printf("alloc buffer fail\n");
                return;
            }
        }
        void* vaddr = RK_MPI_MMZ_Handle2VirAddr(m_blk);
        *fd = RK_MPI_MMZ_Handle2Fd(m_blk);
        *len = RK_MPI_MMZ_GetSize(m_blk);

        printf("Fill 0x5A\n");
        memset(vaddr, 0x5A, *len);

        printf("Server::getBuffer(%d, %d)\n", *fd, *len);
    }

    virtual void freeBuffer() {
        printf("Server::freeBuffer()\n");
        RK_MPI_MMZ_Free(m_blk);
        m_blk = NULL;
    }
};

// Helper function to get a hold of the "Demo" service.
sp<IDemo> getDemoServ() {
    sp<IServiceManager> sm = defaultServiceManager();
    ASSERT(sm != 0);
    sp<IBinder> binder = sm->getService(String16(BINDER_SERV_NAME));
    // TODO: If the "Demo" service is not running, getService times out and binder == 0.
    ASSERT(binder != 0);
    sp<IDemo> demo = interface_cast<IDemo>(binder);
    ASSERT(demo != 0);
    return demo;
}

void startDemoServ()
{
    defaultServiceManager()->addService(String16(BINDER_SERV_NAME), new Server());
    android::ProcessState::self()->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
    printf("service is now ready\n");
}

sp<IDemo> demo;

MB_BLK dequeue()
{
    int fd = -1;
    uint32_t len = 0;

    demo->getBuffer(&fd, &len);
    if (fd < 0)
        return (MB_BLK)NULL;

    demo->freeBuffer();

    MB_BLK blk = RK_MPI_MMZ_ImportFD(fd, len);

    return blk;
}

void queue(MB_BLK blk)
{
    uint32_t len = RK_MPI_MMZ_GetSize(blk);
    int fd = RK_MPI_MMZ_Handle2Fd(blk);

    // 把buffer送入server
    demo->setBuffer(fd, len);

    // 释放client端内存
    RK_MPI_MMZ_Free(blk);
}

int main(int argc, char **argv)
{
    (void)argv;
    if (argc == 1) {
        printf("=== server PID[%d] ===\n", getpid());
        startDemoServ();
    } else if (argc == 2) {
        printf("=== client PID[%d] ===\n", getpid());
        demo = getDemoServ();

        // 从Server取出一块buffer
        MB_BLK blk = dequeue();
        if (blk == NULL) {
            printf("dequeue buffer fail!\n");
            return -1;
        }

        // 检查数据正确性，预期是全部 0x5A
        void* vaddr = RK_MPI_MMZ_Handle2VirAddr(blk);
        uint32_t len = RK_MPI_MMZ_GetSize(blk);
        if (check_data((uint8_t*)vaddr, 0x5A, len) == 0) {
            printf("check okay.\n");
        } else {
            printf("check fail.\n");
        }

        printf("Press Enter key to continue...\n");getchar();

        // 更新数据
        printf("Fill 0x3C\n");
        memset(vaddr, 0x3C, len);

        // 把内存还给server
        queue(blk);

        printf("Press Enter key to quit...\n");getchar();
    }

    return 0;
}