#include <errno.h>
#include <fcntl.h>
#include <log/log.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define LOG_TAG "VENDOR_STORAGE_TEST"

#define SERIALNO_BUF_LEN 33
#define RKNAND_SYS_STORGAE_DATA_LEN 512
#define DEBUG_LOG 1   //open debug info

#define VENDOR_REQ_TAG		0x56524551
#define VENDOR_READ_IO		_IOW('v', 0x01, unsigned int)
#define VENDOR_WRITE_IO		_IOW('v', 0x02, unsigned int)

#define VENDOR_SN_ID		1
#define VENDOR_WIFI_MAC_ID	2
#define VENDOR_LAN_MAC_ID	3
#define VENDOR_BLUETOOTH_ID	4

typedef		unsigned short	  uint16;
typedef		unsigned long	    uint32;
typedef		unsigned char	    uint8;
static char sn_buf_idb[SERIALNO_BUF_LEN] = {0};

struct rk_vendor_req {
    uint32 tag;
    uint16 id;
    uint16 len;
    uint8 data[RKNAND_SYS_STORGAE_DATA_LEN];
};

void dump_hex_data(const char *s,uint8 *buf,uint32 len)
{
    SLOGE("%s",s);
    for(int i=0;i<len;i+=4)
    {
        SLOGE("0x%x 0x%x 0x%x 0x%x",buf[i],buf[i+1],buf[i+2],buf[i+3]);
    }
}

int vendor_storage_read_sn(void)
{
    int ret ;
    uint16 len;
    struct rk_vendor_req req;
    memset(sn_buf_idb,0,sizeof(sn_buf_idb));
    int sys_fd = open("/dev/vendor_storage",O_RDONLY,0);
    if(sys_fd < 0){
        SLOGE("vendor_storage open fail %s\n", strerror(errno));
        return -1;
    }

    req.tag = VENDOR_REQ_TAG;
    req.id = VENDOR_SN_ID;
    req.len = RKNAND_SYS_STORGAE_DATA_LEN; /* max read length to read*/
    ret = ioctl(sys_fd, VENDOR_READ_IO, &req);
    close(sys_fd);
    if (DEBUG_LOG) dump_hex_data("vendor read:", req.data, req.len/4 + 3);
    /* return req->len is the real data length stored in the NV-storage */
    if(ret){
        SLOGE("vendor read error\n");
        return -1;
    }
    //get the sn length
    len = req.len;
    if(len > 30)
    {
        len =30;
    }
    if(len <= 0)
    {
        SLOGE("vendor read error, len = 0\n");
    }
    memcpy(sn_buf_idb,req.data,len);
    if (DEBUG_LOG) SLOGD("vendor read sn_buf_idb:%s\n",sn_buf_idb);
    return 0;
}

int vendor_storage_write_sn(const char* sn)
{
    if (DEBUG_LOG) SLOGD("save SN: %s to IDB.\n", sn);
    int ret ;
    struct rk_vendor_req req;
    int sys_fd = open("/dev/vendor_storage",O_RDONLY,0);
    if(sys_fd < 0){
        SLOGE("vendor_storage open fail %s\n", strerror(errno));
        return -1;
    }
    memset(&req, 0, sizeof(req));
    req.tag = VENDOR_REQ_TAG;
    req.id = VENDOR_SN_ID;
    req.len = strlen(sn);
    memcpy(req.data, sn, strlen(sn));
    if (DEBUG_LOG) dump_hex_data("vendor write:", req.data, req.len/4+3);
    ret = ioctl(sys_fd, VENDOR_WRITE_IO, &req);
    close(sys_fd);
    if(ret){
        SLOGE("error in saving SN to IDB.\n");
        return -1;
    }
    return 0;
}

/** * Program entry pointer *
* @return 0 for success, -1 for SLOGE
*/
int main( int argc, char *argv[] )
{
    SLOGD("----------------running vendor storage test---------------");

    if (argc > 1) {
        const char* vendor_sn = argv[1];
        vendor_storage_write_sn(vendor_sn);
    }
    else vendor_storage_read_sn();
    return 0;
}
