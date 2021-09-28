#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termio.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <cutils/properties.h>
#include <sys/utsname.h>
#include <cutils/list.h>
#include <cutils/log.h>
#include <cutils/sockets.h>
#include <cutils/iosched_policy.h>

#include <dirent.h>
#include <sys/stat.h>
#include <regex.h>

#define LOG_TAG "DrmService"

#define WIFI_CHIP_TYPE_PATH         "/sys/class/rkwifi/chip"
#define WIFI_MAC_FILENAME   "/sys/class/net/wlan0/address"
#define DRIVER_MODULE_PATH  "/system/lib/modules/wlan.ko"
#define DRIVER_MODULE_PATH_SECOND  "/system/lib/modules/rkwifi.ko"
#define DRIVER_MODULE_NAME "wlan"
#define DEVICE_SERIALNO "/data/vendor/serialno"
#define USB_SERIAL_PATH "/sys/class/android_usb/android0/iSerial"
#define USB_SERIAL_PATH1 "/config/usb_gadget/g1/strings/0x409/serialnumber"

#define SERIALNO_PATTERN "^[A-Za-z0-9]+$"
#define SERIALNO_COUNT 1

#define SERIALNO_BUF_LEN 33

extern int init_module(void *, unsigned long, const char *);
extern int delete_module(const char *, unsigned int);

static char sn_buf_auto[SERIALNO_BUF_LEN] = {0};
static char sn_buf_idb[SERIALNO_BUF_LEN] = {0};
static char hid_buf_idb[SERIALNO_BUF_LEN] = {0};

//add by xzj to support DRM,including read SN,read userdefine data, auto SN,detect keybox
typedef		unsigned short	  uint16;
typedef		unsigned long	    uint32;
typedef		unsigned char	    uint8;

#define RKNAND_SYS_STORGAE_DATA_LEN 512

#define RKNAND_DIASBLE_SECURE_BOOT _IOW('d', 127, unsigned int)
#define RKNAND_ENASBLE_SECURE_BOOT _IOW('d', 126, unsigned int)
#define RKNAND_GET_SN_SECTOR       _IOW('d', 3, unsigned int)//读取SN
#define RKNAND_GET_DRM_KEY         _IOW('d', 1, unsigned int)


#define RKNAND_GET_VENDOR_SECTOR0       _IOW('v', 16, unsigned int)
#define RKNAND_STORE_VENDOR_SECTOR0     _IOW('v', 17, unsigned int)

#define RKNAND_GET_VENDOR_SECTOR1       _IOW('v', 18, unsigned int)
#define RKNAND_STORE_VENDOR_SECTOR1     _IOW('v', 19, unsigned int)

#define VENDOR_REQ_TAG		0x56524551
#define VENDOR_READ_IO		_IOW('v', 0x01, unsigned int)
#define VENDOR_WRITE_IO		_IOW('v', 0x02, unsigned int)


#define RKNAND_LOADER_LOCK         _IOW('l', 40, unsigned int)
#define RKNAND_LOADER_UNLOCK        _IOW('l', 50, unsigned int)
#define RKNAND_LOADER_STATUS       _IOW('l', 60, unsigned int)
#define RKNAND_DEV_CACHE_FLUSH     _IOW('c', 20, unsigned int)


#define DRM_KEY_OP_TAG              0x4B4D5244 // "DRMK" 
#define SN_SECTOR_OP_TAG            0x41444E53 // "SNDA"
#define DIASBLE_SECURE_BOOT_OP_TAG  0x42534444 // "DDSB"
#define ENASBLE_SECURE_BOOT_OP_TAG  0x42534E45 // "ENSB"
#define VENDOR_SECTOR_OP_TAG        0x444E4556 // "VEND"
#define LOADER_LOCK_UNLOCK_TAG      0x4C4F434B // "LOCK"

#define VENDOR_SN_ID		1
#define VENDOR_WIFI_MAC_ID	2
#define VENDOR_LAN_MAC_ID	3
#define VENDOR_BLUETOOTH_ID	4

#define DEBUG_LOG 0   //open debug info

#define SERIALNO_FROM_IDB 1  //if 1 read sn from idb3;  if 0 generate sn auto

#define SET_IFACE_DELAY                 300000
#define SET_IFACE_POLLING_LOOP          20

extern int ifc_init();
extern void ifc_close();
extern int ifc_up(const char *name);
extern int ifc_down(const char *name);

extern void *load_file(const char *fn, unsigned *sz);
int get_serialno_cached(char * result,int len);

struct rk_vendor_req {
    uint32 tag;
    uint16 id;
    uint16 len;
    uint8 data[RKNAND_SYS_STORGAE_DATA_LEN];
};

void rknand_print_hex_data(uint8 *s,uint32 * buf,uint32 len)
{
    uint32 i,j,count;
    SLOGE("%s",s);
    for(i=0;i<len;i+=4)
    {
        SLOGE("%x %x %x %x",buf[i],buf[i+1],buf[i+2],buf[i+3]);
    }
}

typedef struct tagRKNAND_SYS_STORGAE
{
    unsigned long tag;
    unsigned long len;
    unsigned char data[RKNAND_SYS_STORGAE_DATA_LEN];
}RKNAND_SYS_STORGAE;

/*
disable secureboot/keybox
*/
int rknand_sys_storage_secure_boot_disable(void)
{
    uint32 i;
    int ret ;
    RKNAND_SYS_STORGAE sysData;

    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        return -1;
    }
    sysData.tag = DIASBLE_SECURE_BOOT_OP_TAG;
    sysData.len = RKNAND_SYS_STORGAE_DATA_LEN;

    ret = ioctl(sys_fd, RKNAND_DIASBLE_SECURE_BOOT, &sysData);
    if(ret){
        SLOGE("disable secure boot error\n");
        return -1;
    }
    return 0;
}

/*
enable secureboot/keybox
*/
int rknand_sys_storage_secure_boot_enable(void)
{
    uint32 i;
    int ret ;
    RKNAND_SYS_STORGAE sysData;

    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        return -1;
    }
    sysData.tag = ENASBLE_SECURE_BOOT_OP_TAG;
    sysData.len = RKNAND_SYS_STORGAE_DATA_LEN;

    ret = ioctl(sys_fd, RKNAND_ENASBLE_SECURE_BOOT, &sysData);
    if(ret){
        SLOGE("enable secure boot error\n");
        return -1;
    }
    return 0;
}


/*
demo for load data in vendor sector
*/
int rknand_sys_storage_vendor_sector_load(void)
{
    uint32 i;
    int ret ;
    RKNAND_SYS_STORGAE sysData;

    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        return -1;
    }

    sysData.tag = VENDOR_SECTOR_OP_TAG;
    sysData.len = RKNAND_SYS_STORGAE_DATA_LEN-8;

    ret = ioctl(sys_fd, RKNAND_GET_VENDOR_SECTOR0, &sysData);
    rknand_print_hex_data("vendor_sector load:",(uint32*)sysData.data,32);
    if(ret){
        SLOGE("get vendor_sector error\n");
        return -1;
    }
    return 0;
}


/*
demo for store data in vendor sector
*/
int rknand_sys_storage_vendor_sector_store(void)
{
    uint32 i;
    int ret ;
    RKNAND_SYS_STORGAE sysData;

    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        return -1;
    }
    sysData.tag = VENDOR_SECTOR_OP_TAG;
    sysData.len = RKNAND_SYS_STORGAE_DATA_LEN - 8;
    for(i=0;i<126;i++)
    {
        sysData.data[i] = i;
    }
    rknand_print_hex_data("vendor_sector save:",(uint32*)sysData.data,32);
    ret = ioctl(sys_fd, RKNAND_STORE_VENDOR_SECTOR0, &sysData);
    close(sys_fd);
    if(ret){
        SLOGE("save vendor_sector error\n");
        return -1;
    }
    return 0;
}

/*
flush flash cache
*/
int rknand_sys_storage_dev_cache_flush(void)
{
    uint32 i;
    int ret ;
    RKNAND_SYS_STORGAE sysData;

    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        return -1;
    }
    sysData.tag = RKNAND_DEV_CACHE_FLUSH;
    sysData.len = 504;
    ret = ioctl(sys_fd, RKNAND_DEV_CACHE_FLUSH, &sysData);
    close(sys_fd);
    if(ret){
        SLOGE("dev cache flush error\n");
        return -1;
    }
    return 0;
}


int rknand_sys_storage_lock_loader(void)
{
    uint32 i;
    int ret ;
    RKNAND_SYS_STORGAE sysData;

    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        return -1;
    }
    sysData.tag = LOADER_LOCK_UNLOCK_TAG;
    sysData.len = 0; //这个值是一个密码，用户自己设置，默认这个值为0，没有密码，第一次设置时，这个值会保存，unlock时这个值要匹配
    ret = ioctl(sys_fd, RKNAND_LOADER_LOCK, &sysData);
    close(sys_fd);
    if(ret){
        SLOGE("dev cache flush error\n");
        return -1;
    }
    return 0;
}

int rknand_sys_storage_unlock_loader(void)
{
    uint32 i;
    int ret ;
    RKNAND_SYS_STORGAE sysData;

    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        return -1;
    }
    sysData.tag = LOADER_LOCK_UNLOCK_TAG;
    sysData.len = 0; //这个值是一个密码，unlock时要匹配密码，密码不匹配不能unlock，成功unlock后，密码会清除
    ret = ioctl(sys_fd, RKNAND_LOADER_UNLOCK, &sysData);
    close(sys_fd);
    if(ret){
        SLOGE("dev cache flush error\n");
        return -1;
    }
    return 0;
}

int rknand_sys_storage_get_loader_status(int * plock_status)
{
    uint32 i;
    int ret ;
    RKNAND_SYS_STORGAE sysData;


    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        return -1;
    }
    sysData.tag = LOADER_LOCK_UNLOCK_TAG;
    sysData.len = 0;
    ret = ioctl(sys_fd, RKNAND_LOADER_STATUS, &sysData);
    close(sys_fd);
    if(ret){
        SLOGE("rknand_sys_storage_get_loader_status error\n");
        return -1;
    }
    *plock_status =  sysData.len;
    SLOGE("lock_status = %d\n",sysData.len);
    return 0;
}

/*
read SN from IDB3,from 0-31bit
*/

int rknand_sys_storage_test_sn(void)
{
    uint32 i;
    int ret;
    uint16 len;
    RKNAND_SYS_STORGAE sysData;
    memset(sn_buf_idb,0,sizeof(sn_buf_idb));
    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        return -1;
    }
    //sn
    sysData.tag = SN_SECTOR_OP_TAG;
    sysData.len = RKNAND_SYS_STORGAE_DATA_LEN;
    ret = ioctl(sys_fd, RKNAND_GET_SN_SECTOR, &sysData);
    close(sys_fd);
    rknand_print_hex_data("sndata:",(uint32*)sysData.data,8);
    if(ret){
        SLOGE("get sn SLOGE\n");
        return -1;
    }
    //get the sn length
    len =*((uint16*)sysData.data);
    if(len > 30)
    {
        len =30;
    }
    if(len < 0)
    {
        len =0;
    }
    memcpy(sn_buf_idb,(sysData.data)+2,len);
    //property_set("vendor.serialno",sn_buf_idb);
    return 0;
}

/*
read HID from IDB3,from 0-31bit
*/

int rknand_sys_storage_test_hid(void)
{
    uint32 i;
    int ret;
    uint16 len;
    RKNAND_SYS_STORGAE sysData;
    memset(hid_buf_idb,0,sizeof(hid_buf_idb));
    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        return -1;
    }
    //sn
    sysData.tag = SN_SECTOR_OP_TAG;
    sysData.len = RKNAND_SYS_STORGAE_DATA_LEN;
    ret = ioctl(sys_fd, RKNAND_GET_SN_SECTOR, &sysData);
    close(sys_fd);
    rknand_print_hex_data("hiddata:",(uint32*)sysData.data,8);
    if(ret){
        SLOGE("get hid SLOGE\n");
        return -1;
    }
    //get the sn length
    len =*((uint16*)sysData.data);
    if(len > 32)
    {
        len =32;
    }
    if(len < 0)
    {
        len =0;
    }
    memcpy(hid_buf_idb,(sysData.data)+192,len);
    //property_set("vendor.serialno",sn_buf_idb);
    return 0;
}



int vendor_storage_read_sn(void)
{
    uint32 i;
    int ret ;
    uint16 len;
    struct rk_vendor_req req;
    memset(sn_buf_idb,0,sizeof(sn_buf_idb));
    int sys_fd = open("/dev/vendor_storage",O_RDONLY,0);
    if(sys_fd < 0){
        SLOGE("vendor_storage open fail %s\n", strerror(errno));
        goto try_drmboot;
    }

    req.tag = VENDOR_REQ_TAG;
    req.id = VENDOR_SN_ID;
    req.len = RKNAND_SYS_STORGAE_DATA_LEN; /* max read length to read*/
    ret = ioctl(sys_fd, VENDOR_READ_IO, &req);
    close(sys_fd);
    if (DEBUG_LOG) rknand_print_hex_data("vendor read:", (uint32*)req.data, req.len/4 + 3);
    /* return req->len is the real data length stored in the NV-storage */
    if(ret){
        SLOGE("vendor read error\n");
        goto try_drmboot;
    }
    //get the sn length
    len = req.len;
    if(len > 30)
    {
        len =30;
    }
    if(len <= 0)
    {
        goto try_drmboot;
    }
    memcpy(sn_buf_idb,req.data,len);
    if (DEBUG_LOG) SLOGD("vendor read sn_buf_idb:%s\n",sn_buf_idb);
    return 0;

try_drmboot:
    SLOGE("----vendor read sn error,try drmboot----");
    rknand_sys_storage_test_sn();
    return 0;
}

int vendor_storage_write_sn(const char* sn)
{
    if (DEBUG_LOG) SLOGD("save SN: %s to IDB.\n", sn);
    uint32 i;
    int ret ;
    uint16 len;
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
    if (DEBUG_LOG) rknand_print_hex_data("vendor write:", (uint32*)req.data, req.len/4+3);
    ret = ioctl(sys_fd, VENDOR_WRITE_IO, &req);
    close(sys_fd);
    if(ret){
        SLOGE("error in saving SN to IDB.\n");
        return -1;
    }
    return 0;
}

/*
read user defined data from  IDB3, from 32-512bit
*/
void read_region_tag()
{
    int ret,i,temp;
    char region[20];
    RKNAND_SYS_STORGAE sysData;
    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        property_set("ro.vendor.board.zone","0");
        SLOGE("open file failed,ro.board.zone set default value 0\n");
        return;
    }
    sysData.tag = VENDOR_SECTOR_OP_TAG;
    sysData.len = RKNAND_SYS_STORGAE_DATA_LEN-8;
    ret = ioctl(sys_fd, RKNAND_GET_VENDOR_SECTOR0, &sysData);
    strncpy(region,sysData.data,20);
    region[19]='\0';
    SLOGE("-----read_region_tag,str=%s",region);
    if(strstr(region,"Archos_Region")!=NULL)
    {
        char region_tag[2];
        region_tag[0]=region[14];
        region_tag[1]='\0';
        SLOGE("------get region=%c",region_tag[0]);
        if(region_tag[0]>='0'&&region_tag[0]<='5')
        {
            property_set("ro.vendor.board.zone",region_tag);
            SLOGE("we set ro.vendor.board.zone to %c\n",region_tag[0]);
        }
        else
        {
            property_set("ro.vendor.board.zone","0");
            SLOGE("get SLOGE region tag from flash,not between 0-4,ro.board.zone set default value 0\n");
        }
    }
    else
    {
        property_set("ro.vendor.board.zone","0");
        SLOGE("get SLOGE region tag from flash,not between 0-4,ro.board.zone set default value 0\n");
    }
}

int insmod(const char *filename)
{
    void *module = NULL;
    unsigned int size;
    int ret;
    struct utsname name;
    char filename_release[PATH_MAX];
    memset(&name, 0, sizeof(name));
    ret = uname(&name);
    if (ret == 0) {
        // try insmod filename.x.x.x
        strncat(filename_release, filename, sizeof(filename_release) - 1);
        strncat(filename_release, ".", sizeof(filename_release) - 1);
        strncat(filename_release, name.release, sizeof(filename_release) - 1);
        module = load_file(filename_release, &size);
    }
    if (!module)
    module = load_file(filename, &size);
    if (!module)
    return -1;
    ret = init_module(module, size, "");
    free(module);
    return ret;
}



static int rmmod(const char *modname)
{
    int ret = -1;
    int maxtry = 10;
    while (maxtry-- > 0)
    {
        ret = delete_module(modname, O_NONBLOCK | O_EXCL);
        if (ret < 0 && errno == EAGAIN)
        usleep(500000);
        else
        break;
    }
    if (ret != 0)
    SLOGE("Unable to unload driver module \"%s\": %s\n", modname, strerror(errno));
    return ret;
}

// return 0, which means invalid
int is_serialno_valid(char* serialno)
{
#ifdef ENABLE_SN_VERIFY
    if ((strlen(serialno) < 6) || (strlen(serialno) > 14)) {
        SLOGE("serialno is too short or too long, please check!");
        return 0;
    }
    regex_t regex;
    int ret = regcomp(&regex, SERIALNO_PATTERN, REG_EXTENDED);
    if (ret != 0) {
        regfree(&regex);
        SLOGE("regex init failed!");
        return 0;
    }

    regmatch_t pm[SERIALNO_COUNT];
    memset(pm, 0, sizeof(regmatch_t) * SERIALNO_COUNT);
    ret = regexec(&regex, serialno, SERIALNO_COUNT, pm, 0);
    regfree(&regex);
    return !ret == REG_NOMATCH;
#else
    return 1;
#endif
}

int store_serialno(char* serialno)
{
    FILE *mac = NULL;
    char buf[32];

    if(get_serialno_cached(buf,strlen(serialno))==0)
    {
        if(DEBUG_LOG) SLOGE("store_serialno =%s,len = %d,cached =%s",serialno,strlen(serialno),buf);
        if(0 == strncmp(buf,serialno, strlen(serialno)))
        {
            if(DEBUG_LOG) SLOGE("store_serialno,skip write same serialno =%s",serialno);
            return 0;
        }
    }

    mac = fopen(DEVICE_SERIALNO, "w+");
    if (mac == NULL)
    {
        if(DEBUG_LOG) SLOGE("open %s failed.", DEVICE_SERIALNO);
        return -1;
    }
    fputs(serialno, mac);
    fclose(mac);
    if(DEBUG_LOG) SLOGE("buffer serialno =%s in %s done",serialno,DEVICE_SERIALNO);
    return 0;
}

int get_serialno_cached(char * result,int len)
{
    int fd,readlen;
    char buf[32];
    fd = open(DEVICE_SERIALNO, O_RDONLY);
    if (fd < 0)
    {
        if(DEBUG_LOG) SLOGE("[%s] has not been created", DEVICE_SERIALNO);
        return -1;
    }
    readlen=read(fd, buf, sizeof(buf) - 1);
    if(readlen != len)
    {
        if(DEBUG_LOG) SLOGE("get_serialno_cached,wanted len =%d,but cached len =%d",len,readlen);
        return -1;
    }
    memcpy(result,buf,readlen);
    buf[readlen]='\0';
    close(fd);
    return 0;
}



void generate_device_serialno(int len,char*result)
{
    int temp=0,rand_bit=0,times =0;
    int fd,type;
    char buf[32];
    char value[6][2];
    const char *bufp;
    ssize_t nbytes;
    char path[64];
    unsigned int seed[2]={0,0};

    #ifdef DEBUG_RANDOM
    SLOGE("-------DEBUG_RANDOM mode-------");
    goto bail;
    #endif

    if(!get_serialno_cached(result,len))
    {
        SLOGE("serianno =%s",result);
        return;
    }

bail:
    for(times=0;times<2;times++)
    {
        if(seed[times]!=0)
        {
            if(DEBUG_LOG) SLOGE("using seed[%d]=%d---",times,seed[times]);
            srand(seed[times]);
        }
        else if(times == 0)
        {
            if(DEBUG_LOG) SLOGE("-----using time as seed----");
            srand(time(0));
        }

        for(temp=0;temp<len/2;temp++)
        {
            rand_bit =rand()%36;
            if(rand_bit>=0&&rand_bit<26)//A-Z
            {
                *(result+temp+(len/2*times))=rand_bit+'A';
            }
            else if(rand_bit>=26 && rand_bit <36)//0-9
            {
                *(result+temp+(len/2*times))=(rand_bit-26)+'0';
            }
            if(DEBUG_LOG) SLOGE("generate_device_serialno, temp =%d,rand_bit=%d,char=%c",temp,rand_bit,*(result+temp+(len/2*times)));
        }
    }
    result[len]='\0';
    store_serialno(result);
    SLOGE("generate_device_serialno,len =%d,result=%s",len,result);
}

int write_serialno2kernel(char*result)
{
    int fd;
    if ((fd = open(USB_SERIAL_PATH, O_WRONLY)) < 0) {
        SLOGE("Unable to open path (%s),error is(%s)",USB_SERIAL_PATH,strerror(errno));
        goto try_next;
    }
    if (write(fd,result,strlen(result)) < 0) {
        SLOGE("Unable to write path (%s),error is(%s)",USB_SERIAL_PATH,strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return 0;

    try_next:
    SLOGE("try %s", USB_SERIAL_PATH1);
    if ((fd = open(USB_SERIAL_PATH1, O_WRONLY)) < 0) {
        SLOGE("Unable to open path (%s),error is(%s)",USB_SERIAL_PATH1,strerror(errno));
        return -1;
    }
    if (write(fd,result,strlen(result)) < 0) {
        SLOGE("Unable to write path (%s),error is(%s)",USB_SERIAL_PATH1,strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

/*
detect if secure boot is enabled
*/
bool detect_keybox()
{
    typedef struct tagDRM_KEY_INFO
    {
        uint32 drmtag;           // "DRMK" 0x4B4D5244
        uint32 drmLen;           // 504
        uint32 keyBoxEnable;     // 0:disable 1 enable
        uint32 drmKeyLen;        //0 disable , 1~N : part 1~N
        uint32 publicKeyLen;     //0 disable , 1:enable
        uint32 reserved0[(0x40-0x14)/4];
        uint8  drmKey[0x80];      // key data
        uint32 reserved2[(0x100-0xC0)/4];
        uint8  publicKey[0x100];      // key data
    } DRM_KEY_INFO,*pDRM_KEY_INFO;

    RKNAND_SYS_STORGAE sysData;
    int ret ;
    DRM_KEY_INFO * pdrmKey = (DRM_KEY_INFO*)&sysData;

    int sys_fd = open("/dev/rknand_sys_storage",O_RDWR,0);
    if(sys_fd < 0){
        SLOGE("rknand_sys_storage open fail\n");
        return false;
    }
    sysData.tag = DRM_KEY_OP_TAG;
    sysData.len = RKNAND_SYS_STORGAE_DATA_LEN;
    pdrmKey->drmKeyLen = 128;
    ret = ioctl(sys_fd, RKNAND_GET_DRM_KEY, &sysData);

    if(ret){
        SLOGE("get drm key ioctl SLOGE\n");
        close(sys_fd);
        return false;
    }

    if(!pdrmKey->keyBoxEnable){
        SLOGE("drm keybox disable!!");
        close(sys_fd);
        return false;
    }
    close(sys_fd);
    return true;
}

void detect_secure_boot()
{
    int fd;
    char buf[2048];
    fd = open("/proc/cmdline", O_RDONLY);
    if (fd < 0)
    {
        if(DEBUG_LOG) SLOGE("------detect_secure_boot() open /proc/cmdline failed!\n");
        return;
    }
    read(fd, buf, sizeof(buf) - 1);
    if(strstr(buf,"SecureBootCheckOk=1")!=NULL){
        if(DEBUG_LOG) SLOGE("------detect SecureBoot-----");
        property_set("vendor.secureboot","true");
    }else{
        if(DEBUG_LOG) SLOGE("------detect not SecureBoot---");
        property_set("vendor.secureboot","false");
    }
    close(fd);
}

void change_path(const char *path)
{
    SLOGE("Leave %s Successed . . .\n",getcwd(NULL,0));
    if(chdir(path)==-1)
    {
        SLOGE("chdir %s error",path);
        return;
    }
    SLOGE("Entry %s Successed . . .\n",getcwd(NULL,0));
}


void copy_file(const char *old_path,const char *new_path)
{
    FILE *in,*out;
    size_t len;
    char buf[64];
    char *p=getcwd(NULL,0);
    SLOGE("start copy file,from %s to %s\n",old_path,new_path);

    if((in=fopen(old_path,"rb"))==NULL)
    {
        SLOGE("fopen %s error\n",old_path);
        return;
    }
    //change_path(new_path);

    if((out=fopen(new_path,"wb"))==NULL)
    {
        SLOGE("fopen %s error\n",new_path);
        return;
    }

    while(!feof(in))
    {
        bzero(buf,sizeof(buf));
        len=fread(&buf,1,sizeof(buf)-1,in);
        fwrite(&buf,len,1,out);
    }
    fclose(in);
    fclose(out);
    //change_path(p);
}

char *get_abs_path(const char *dir,const char *path)
{
    char *rel_path;
    unsigned long d_len,p_len;

    d_len=strlen(dir);
    p_len=strlen(path);
    if((rel_path=malloc(d_len+p_len+2))==NULL)
    {
        SLOGE("malloc fail\n");
        return NULL;
    }
    bzero(rel_path,d_len+p_len+2);

    strncpy(rel_path,dir,d_len);
    strncat(rel_path,"/",sizeof(char));
    strncat(rel_path,path,p_len);

    return rel_path;
}

void copy_dir_at(const char *root_path, const char *old_path, const char *new_path, int can_del)
{
    DIR *dir;
    struct stat buf;
    struct dirent *dirp;
    char *p=getcwd(NULL,0);
    if((dir=opendir(old_path))==NULL)
    {
        SLOGE("opendir %s fail\n",old_path);
        return;
    }
    mkdir(root_path,0755);//in case root_path not created
    char *root_dir_abs_path = get_abs_path(root_path, new_path);
    SLOGE("--root_dir_abs_path =%s--\n",root_dir_abs_path);
    if((mkdir(root_dir_abs_path,0777)==-1) && errno != EEXIST)
    {
        SLOGE("mkdir %s fail, %s \n",root_dir_abs_path, strerror(errno));
        free(root_dir_abs_path);
        return;
    }
    change_path(old_path);
    while((dirp=readdir(dir)))
    {
        if(strcmp(dirp->d_name,".")==0 || strcmp(dirp->d_name,"..")==0)
        continue;
        if(stat(dirp->d_name,&buf)==-1)
        {
            SLOGE("stat %s fail\n",dirp->d_name);
            return;
        }
        if(S_ISDIR(buf.st_mode))
        {
            char * sub_dir_abs_path = get_abs_path(new_path,dirp->d_name);
            SLOGE("--subdir abs path =%s\n",sub_dir_abs_path);
            copy_dir_at(root_path, dirp->d_name, sub_dir_abs_path, can_del);
            free(sub_dir_abs_path);
            continue;
        }
        char* file_abs_path = get_abs_path(root_dir_abs_path,dirp->d_name);
        SLOGE("--file abs path =%s\n",file_abs_path);
        copy_file(dirp->d_name,file_abs_path);
        chmod(file_abs_path,S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IXOTH);
        if (can_del)
        {
            chown(file_abs_path,1023,1023);//if want to deleteable,open this
        }
        free(file_abs_path);
    }

    closedir(dir);
    change_path(p);
    chmod(root_dir_abs_path,S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IXOTH);
if (can_del)
{
    SLOGD("set files can be deleted.");
    chown(root_dir_abs_path,1023,1023);//if want to deleteable,open this
}
free(root_dir_abs_path);
}

int value_in_cmdline(char *value) {
    int fd;
    char buf[2048]={0};
    fd = open("/proc/cmdline", O_RDONLY);
    if (fd < 0) {
        SLOGE("open /proc/cmdline failed!");
        close(fd);
        return -1;
    }
    read(fd, buf, sizeof(buf) - 1);
    if (DEBUG_LOG) {
        SLOGE("cmdline: %s", &buf);
        SLOGE("serialno: %s", value);
    }
    char *ret = strstr(&buf, value);
    if (ret != NULL) {
        close(fd);
        return 0;
    } else {
        close(fd);
        return -1;
    }
}

/**
 * ro.serialno is exported by cmdline which from cpu_id or vendor_storage,
 * if it is empty or mismatch with sn_buf, update it.
 */
void update_serialno(char *sn_buf)
{
#ifdef ENABLE_CMDLINE_VERIFY
    // serialno is empty or different with vendor_storage,
    // save new sn_buf to vendor_storage
    int32_t len_sn_buf = strlen(sn_buf);
    len_sn_buf += strlen("androidboot.serialno=");
    char serialno_cmdline[len_sn_buf];
    snprintf(serialno_cmdline, len_sn_buf, "androidboot.serialno=%s", sn_buf);
    if (value_in_cmdline(serialno_cmdline) != 0) {
        SLOGD("verify: save serialno: %s (%d)", sn_buf, strlen(sn_buf));
        const char *vendor_sn = sn_buf;
        vendor_storage_write_sn(vendor_sn);
        property_set("vendor.serialno", sn_buf);
        write_serialno2kernel(sn_buf);
    } else {
        // keep the SN read from idb is same as from cmdline.
        // skip property_set if they are same.
        // otherwish adbd will restart and cause adb offline.
        SLOGI("new sn is same as old, skip prop_set and update!");
    }
#else
    SLOGD("verify: save serialno: %s (%d)", sn_buf, strlen(sn_buf));
    const char *vendor_sn = sn_buf;
    vendor_storage_write_sn(vendor_sn);
    property_set("vendor.serialno", sn_buf);
    write_serialno2kernel(sn_buf);
#endif
}

/**
*  User cannot del or modify the content that copyed form '/oem/pre_set',
*  only '/oem/pre_set_del' can do that.
*/
void copy_oem()
{
    if(DEBUG_LOG) SLOGE("---do bootup copy oem---");

    copy_dir_at("", "/oem/pre_set_del", "data", 1);
    copy_dir_at("", "/oem/pre_set", "data", 0);

    if(DEBUG_LOG) SLOGE("---do bootup copy oem---");
}

/** * Program entry pointer *
* @return 0 for success, -1 for SLOGE
*/
int main( int argc, char *argv[] )
{
    SLOGD("----------------running drmservice---------------");
    char prop_board_platform[PROPERTY_VALUE_MAX];
    char propbuf_copy_oem[PROPERTY_VALUE_MAX];
    property_get("ro.board.platform", prop_board_platform, "");
    property_get("ro.boot.copy_oem", propbuf_copy_oem, "");

    //get hid data
    rknand_sys_storage_test_hid();
    SLOGD("Get HID data:%s", hid_buf_idb);
    property_set("persist.vendor.sys.hid", hid_buf_idb[0] ? hid_buf_idb : "");

    if(SERIALNO_FROM_IDB)//read serialno form idb
    {
        vendor_storage_read_sn();
        if (is_serialno_valid(sn_buf_idb)) {
#ifdef ENABLE_CMDLINE_VERIFY
            update_serialno(sn_buf_idb);
#else
            property_set("vendor.serialno", sn_buf_idb);
            write_serialno2kernel(sn_buf_idb);
#endif
        } else {
            goto RANDOM_SN;
        }
    }
    else//auto generate serialno
    {
RANDOM_SN:
        generate_device_serialno(10, sn_buf_auto);
        update_serialno(sn_buf_auto);
    }
    /*bool keybox=detect_keybox();
    if(keybox==true)
    {
    property_set("drm.service.enabled","true");
    SLOGE("detect keybox enabled");
}
    else
    {
    property_set("drm.service.enabled","false");
    SLOGE("detect keybox disabled");
}*/
    //detect_secure_boot();
    //Only set 'ro.boot.copy_oem = true', run this.
    if (strcmp(propbuf_copy_oem, "true") == 0) {
        char prop_buf[PROPERTY_VALUE_MAX];
        property_get("persist.sys.first_booting", prop_buf, "");
        if(strcmp(prop_buf,"false"))
        {
            //if want to only copy after recovery,open this
            copy_oem();
        }
    }
//store_metadata_forgpu();

    //read_region_tag();//add by xzj to add property ro.board.zone read from flash
    //rknand_sys_storage_vendor_sector_store();
    //rknand_sys_storage_vendor_sector_load();
    //rknand_sys_storage_get_loader_status(&status);
    //rknand_sys_storage_lock_loader();
    //rknand_sys_storage_get_loader_status(&status);
    //rknand_sys_storage_unlock_loader();
    //rknand_sys_storage_get_loader_status(&status);
    return 0;
}
