#include <thread>
#include <hardware/rga.h>
#define virtual vir
#include <drm.h>
#include <drm_mode.h>
#undef virtual

#include "camera_test.h"
extern "C" {
    #include "../script.h"
    #include "../test_case.h"
    #include "../script_parser.h"
}

static char iq_file[255] = "/etc/cam_iq.xml";
static char out_file[255];
static char dev_name[255];
static int width = 640;
static int height = 480;
static int format = V4L2_PIX_FMT_NV12;
static int fd = -1;
static int drm_fd = -1;
static int io = IO_METHOD_MMAP;
static enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
struct buffer *buffers;
static unsigned int n_buffers;
static int frame_count = 5;
static float mae_gain = 0.0f;
static float mae_expo = 0.0f;
FILE *fp;
static int silent;
static unsigned int drm_handle;

#define DBG(...) do { if(!silent) printf(__VA_ARGS__); } while(0)
#define ERR(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#ifdef ANDROID
#ifdef ANDROID_VERSION_ABOVE_8_X
#define LIBRKISP "/vendor/lib64/librkisp.so"
#else
#define LIBRKISP "/system/lib/librkisp.so"
#endif
#else
#define LIBRKISP "/usr/lib/librkisp.so"
#endif

#define VIDEO_DEV_NAME   "/dev/video0"
#define PMEM_DEV_NAME    "/dev/pmem_cam"
#define DISP_DEV_NAME    "/dev/graphics/fb1"
#define ION_DEVICE          "/dev/ion"

#define FBIOSET_ENABLE			0x5019	

#define CAM_OVERLAY_BUF_NEW  1
#define RK29_CAM_VERSION_CODE_1 KERNEL_VERSION(0, 0, 1)
#define RK29_CAM_VERSION_CODE_2 KERNEL_VERSION(0, 0, 2)

static void *m_v4l2Buffer[4];
static void *m_v4l2buffer_display[4];
static int v4l2Buffer_phy_addr[4] = {0};
static int v4l2Buffer_phy_addr_display;
static int SharedFd_display[4];

static int iCamFd = 0, iDispFd =-1;
static int preview_w,preview_h;

static char videodevice[20] ={0};
static struct v4l2_capability mCamDriverCapability;
static unsigned int pix_format;

static void* vaddr = NULL;
static volatile int isstoped = 0;
static int hasstoped = 1;
enum {
    FD_INIT = -1,
};

//#define RKISP_1 //for 3399/3288/3326/3368/3126c
#define RKISP_2 //for 356x

#define RK30_PLAT 1
#define RK29_PLAT 0
static int is_rk30_plat = RK30_PLAT;
#define  FB_NONSTAND ((is_rk30_plat == RK29_PLAT)?0x2:0x20)
static int cam_id = 0;

static int camera_x=0,camera_y=0,camera_w=0,camera_h=0,camera_num=0;
static struct testcase_info *tc_info = NULL;

static void errno_exit(const char *s)
{
    ERR("%s error %d, %s\n", s, errno, strerror(errno));
    //exit(EXIT_FAILURE);
}

static int init_drm()
{
    int drm_fd = drmOpen("rockchip", NULL);
    if (drm_fd < 0) {
        ERR("failed to open rockchip drm: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return drm_fd;
}

static void deinit_drm(int drm_fd)
{
    drmClose(drm_fd);
}

static void* get_drm_buf(int drm_fd, int width, int height, int bpp)
{
    struct drm_mode_create_dumb alloc_arg;
    struct drm_mode_map_dumb mmap_arg;
    struct drm_mode_destroy_dumb destory_arg;
    int ret;
    void *map;

    CLEAR(alloc_arg);
    alloc_arg.bpp = bpp;
    alloc_arg.width = width;
    alloc_arg.height = height;
    #define ROCKCHIP_BO_CONTIG  1            
    #define ROCKCHIP_BO_CACHABLE (1<<1)
    alloc_arg.flags = ROCKCHIP_BO_CONTIG | ROCKCHIP_BO_CACHABLE;

    ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &alloc_arg);
    if (ret) {
        ERR("failed to create dumb buffer: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    CLEAR(mmap_arg);
    mmap_arg.handle = alloc_arg.handle;

    ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &mmap_arg);
    if (ret) {
        ERR("failed to create map dumb: %s\n", strerror(errno));
        ret = -EINVAL;
        goto destory_dumb;
    }

    map = mmap(0, alloc_arg.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, mmap_arg.offset);
    if (map == MAP_FAILED) {
        ERR("failed to mmap buffer: %s\n", strerror(errno));
        ret = -EINVAL;
        goto destory_dumb;
    }

    assert(alloc_arg.size == width * height * bpp / 8);

    destory_dumb:
    CLEAR(destory_arg);
    destory_arg.handle = alloc_arg.handle;
    drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destory_arg);
    if (ret)
    return NULL;

    return map;
}

static void* get_drm_fd(int drm_fd, int width, int height, int bpp, int *export_dmafd)
{
    struct drm_mode_create_dumb alloc_arg;
    struct drm_mode_map_dumb mmap_arg;
    struct drm_mode_destroy_dumb destory_arg;
    int ret;
    void *map;

    CLEAR(alloc_arg);
    alloc_arg.bpp = bpp;
    alloc_arg.width = width;
    alloc_arg.height = height;
    #define ROCKCHIP_BO_CONTIG  1
    #define ROCKCHIP_BO_CACHABLE (1<<1)
    alloc_arg.flags = ROCKCHIP_BO_CONTIG | ROCKCHIP_BO_CACHABLE;

    ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &alloc_arg);
    if (ret) {
        ERR("failed to create dumb buffer: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    CLEAR(mmap_arg);
    mmap_arg.handle = alloc_arg.handle;
    ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &mmap_arg);
    if (ret) {
        ERR("failed to create map dumb: %s\n", strerror(errno));
        ret = -EINVAL;
        goto destory_dumb;
    }

    map = mmap(0, alloc_arg.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, mmap_arg.offset);
    if (map == MAP_FAILED) {
        ERR("failed to mmap buffer: %s\n", strerror(errno));
        ret = -EINVAL;
        goto destory_dumb;
    }

    assert(alloc_arg.size == width * height * bpp / 8);
    ret = drmPrimeHandleToFD(drm_fd, alloc_arg.handle, 0, export_dmafd);
    if (ret) {
        ERR("failed to export fd: %s\n", strerror(errno));
        ret = -EINVAL;
        goto destory_dumb;
    }

    drm_handle = alloc_arg.handle;
    return map;

destory_dumb:
    CLEAR(destory_arg);
    destory_arg.handle = alloc_arg.handle;
    drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destory_arg);

    return NULL;
}

static int xioctl(int fh, int request, void *arg)
{
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

static void process_image(const void *p, int size)
{
    DBG("process_image size: %d\n",size);
    fwrite(p, size, 1, fp);
    fflush(fp);
}

static int read_frame(FILE *fp)
{
    struct v4l2_buffer buf;
    int i, bytesused;

    CLEAR(buf);

    buf.type = buf_type;
    if (io == IO_METHOD_MMAP)
    buf.memory = V4L2_MEMORY_MMAP;
    else if (io == IO_METHOD_DMABUF)
    buf.memory = V4L2_MEMORY_DMABUF;
    else
    buf.memory = V4L2_MEMORY_USERPTR;

    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type) {
        struct v4l2_plane planes[FMT_NUM_PLANES];
        buf.m.planes = planes;
        buf.length = FMT_NUM_PLANES;
    }

    if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
        errno_exit("VIDIOC_DQBUF");
        return 1;
    }

    i = buf.index;

    if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type)
    bytesused = buf.m.planes[0].bytesused;
    else
    bytesused = buf.bytesused;
    process_image(buffers[i].start, bytesused);
    DBG("bytesused %d\n", bytesused);

    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
        errno_exit("VIDIOC_QBUF");
        return 1;
    }

    return 0;
}

static int mainloop(void)
{
    unsigned int count = frame_count;
    float exptime, expgain;
    int64_t frame_id, frame_sof;

    while (count-- > 0) {
        DBG("No.%d\n",frame_count - count);        //显示当前帧数目

        if (read_frame(fp) > 0)
        return 1;
    }
    DBG("\nREAD AND SAVE DONE!\n");
    return 0;
}

static int stop_capturing(void)
{
    enum v4l2_buf_type type;
    type = buf_type;
    if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type)) {
        errno_exit("VIDIOC_STREAMOFF");
        return 1;
    }
    return 0;
}

static int start_capturing(void)
{
    unsigned int i;
    enum v4l2_buf_type type;

    for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type = buf_type;
        if (io == IO_METHOD_MMAP)
        buf.memory = V4L2_MEMORY_MMAP;
        else if (io == IO_METHOD_DMABUF)
        buf.memory = V4L2_MEMORY_DMABUF;
        else
        buf.memory = V4L2_MEMORY_USERPTR;
        buf.index = i;

        if (io == IO_METHOD_USERPTR) {
            buf.m.userptr = (unsigned long)buffers[i].start;
            buf.length = buffers[i].length;
        } else if (io == IO_METHOD_DMABUF) {
            buf.m.fd = buffers[i].v4l2_buf.m.fd;
            buf.length = buffers[i].length;
        }

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type) {
            struct v4l2_plane planes[FMT_NUM_PLANES];

            if (io == IO_METHOD_USERPTR) {
                planes[0].m.userptr = (unsigned long)buffers[i].start;
                planes[0].length = buffers[i].length;
            } else if (io == IO_METHOD_DMABUF) {
                planes[0].m.fd = buffers[i].v4l2_buf.m.fd;
                planes[0].length = buffers[i].length;
            }
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }
        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
            errno_exit("VIDIOC_QBUF");
            return 1;
        }
    }
    type = buf_type;
    if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)) {
        errno_exit("VIDIOC_STREAMON");
        return 1;
    }
    return 0;
}

static int uninit_device(void)
{
    unsigned int i;
    struct drm_mode_destroy_dumb destory_arg;

    for (i = 0; i < n_buffers; ++i) {
        if (-1 == munmap(buffers[i].start, buffers[i].length)) {
            errno_exit("munmap");
            return 1;
        }

        if (io == IO_METHOD_DMABUF) {
            close(buffers[i].v4l2_buf.m.fd);
            CLEAR(destory_arg);
            destory_arg.handle = drm_handle;
            drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destory_arg);
        }
    }

    free(buffers);

    if (drm_fd != -1) deinit_drm(drm_fd);
    return 0;
}

static void init_mmap(void)
{
    struct v4l2_requestbuffers req;

    CLEAR(req);

    req.count = BUFFER_COUNT;
    req.type = buf_type;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            ERR("%s does not support "
                "memory mapping\n", dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        ERR("Insufficient buffer memory on %s\n",
            dev_name);
        exit(EXIT_FAILURE);
    }

    buffers = (struct buffer*)calloc(req.count, sizeof(*buffers));

    if (!buffers) {
        ERR("Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[FMT_NUM_PLANES];
        CLEAR(buf);
        CLEAR(planes);

        buf.type = buf_type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type) {
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }

        if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) errno_exit("VIDIOC_QUERYBUF");

        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type) {
            buffers[n_buffers].length = buf.m.planes[0].length;
            buffers[n_buffers].start =
            mmap(NULL /* start anywhere */,
                 buf.m.planes[0].length,
                 PROT_READ | PROT_WRITE /* required */,
                 MAP_SHARED /* recommended */,
                 fd, buf.m.planes[0].m.mem_offset);
        } else {
            buffers[n_buffers].length = buf.length;
            buffers[n_buffers].start =
            mmap(NULL /* start anywhere */,
                 buf.length,
                 PROT_READ | PROT_WRITE /* required */,
                 MAP_SHARED /* recommended */,
                 fd, buf.m.offset);
        }

        if (MAP_FAILED == buffers[n_buffers].start) errno_exit("mmap");
    }
}

static void init_dmabuf(int buffer_size, int width, int height)
{
    struct v4l2_requestbuffers req;
    int bpp;
    int export_dmafd;

    CLEAR(req);

    req.count  = BUFFER_COUNT;
    req.memory = V4L2_MEMORY_DMABUF;
    req.type = buf_type;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            ERR("%s does not support dmabuf i/on\n", dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }

    buffers = (struct buffer*)calloc(req.count, sizeof(*buffers));

    if (!buffers) {
        ERR("Out of memory\n");
        exit(EXIT_FAILURE);
    }

    drm_fd = init_drm();
    bpp = buffer_size * 8 / width / height;
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        buffers[n_buffers].length = buffer_size;
        buffers[n_buffers].start = get_drm_fd(drm_fd, width, height, bpp, &export_dmafd);

        buffers[n_buffers].v4l2_buf.m.fd = export_dmafd;
        buffers[n_buffers].v4l2_buf.length = buffer_size;
    }
}

static void init_userp(int buffer_size, int width, int height)
{
    struct v4l2_requestbuffers req;
    int bpp;

    CLEAR(req);

    req.count  = BUFFER_COUNT;
    req.memory = V4L2_MEMORY_USERPTR;
    req.type = buf_type;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            ERR("%s does not support user pointer i/on\n", dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }

    buffers = (struct buffer*)calloc(req.count, sizeof(*buffers));

    if (!buffers) {
        ERR("Out of memory\n");
        exit(EXIT_FAILURE);
    }

    drm_fd = init_drm();
    bpp = buffer_size * 8 / width / height;
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        buffers[n_buffers].length = buffer_size;
        buffers[n_buffers].start = get_drm_buf(drm_fd, width, height, bpp);

        if (!buffers[n_buffers].start) {
            exit(EXIT_FAILURE);
        }
    }
}

static int init_device(void)
{
    struct v4l2_capability cap;
    struct v4l2_format fmt;

    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            ERR("%s is no V4L2 device\n",
                dev_name);
            exit(EXIT_FAILURE);
            return 1;
        } else {
            errno_exit("VIDIOC_QUERYCAP");
            return 1;
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
        !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
            ERR("%s is not a video capture device, capabilities: %x\n",
                dev_name, cap.capabilities);
            exit(EXIT_FAILURE);
            return 1;
        }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        ERR("%s does not support streaming i/o\n",
            dev_name);
        exit(EXIT_FAILURE);
        return 1;
    }

    DBG(" %s capabilities driver: %s name:%s\n", dev_name, (char*) &cap.driver[0], (char*) &cap.card[0]);

    if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    else if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
        buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    }

    CLEAR(fmt);
    fmt.type = buf_type;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = format;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)) {
        errno_exit("VIDIOC_S_FMT");
        return 1;
    }

    if (io == IO_METHOD_MMAP) {
        init_mmap();
    } else if (io == IO_METHOD_USERPTR) {
        init_userp(fmt.fmt.pix.sizeimage, width, height);
    } else if (io == IO_METHOD_DMABUF) {
        init_dmabuf(fmt.fmt.pix.sizeimage, width, height);
    }
    return 0;
}

static int close_device(void)
{
    if (-1 == close(fd)) {
        errno_exit("close");
        return 1;
    }

    fd = -1;
    return 0;
}

static int open_device(void)
{
    fd = open(dev_name, O_RDWR /* required */ /*| O_NONBLOCK*/, 0);

    if (-1 == fd) {
        ERR("Cannot open '%s': %d, %s\n",
            dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
        return 1;
    }
    return 0;
}

void parse_args(int argc, char **argv)                                                                                                       
{
    int c;
    int digit_optind = 0;

    while (1) {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"width",    required_argument, 0, 'w' },
            {"height",   required_argument, 0, 'h' },
            {"memory",   required_argument, 0, 'm' },
            {"format",   required_argument, 0, 'f' },
            {"iqfile",   required_argument, 0, 'i' },
            {"device",   required_argument, 0, 'd' },
            {"output",   required_argument, 0, 'o' },
            {"count",    required_argument, 0, 'c' },
            {"expo",     required_argument, 0, 'e' },
            {"gain",     required_argument, 0, 'g' },
            {"help",     no_argument,       0, 'p' },
            {"silent",   no_argument,       0, 's' },
            {0,          0,                 0,  0  }
        };

        c = getopt_long(argc, argv, "w:h:m:f:i:d:o:c:e:g:ps",
                        long_options, &option_index);
        if (c == -1)
        break;

        switch (c) {
            case 'c':
                frame_count = atoi(optarg);
                break;
            case 'e':
                mae_expo = atof(optarg);
                DBG("target expo: %f\n", mae_expo);
                break;
            case 'm':
                if (!strcmp("drm", optarg))
                    io = IO_METHOD_USERPTR;
                else if(!strcmp("dmabuf", optarg))
                    io = IO_METHOD_DMABUF;
                else
                    io = IO_METHOD_MMAP;
                break;                                                                                                                            
            case 'g':
                mae_gain = atof(optarg);
                DBG("target gain: %f\n", mae_gain);
                break;
            case 'w':
                width = atoi(optarg);
                break;
            case 'h':
                height = atoi(optarg);
                break;
            case 'f':
                format = v4l2_fourcc(optarg[0], optarg[1], optarg[2], optarg[3]);
                break;
            case 'i':
                strcpy(iq_file, optarg);
                break;
            case 'd':
                strcpy(dev_name, optarg);
                break;
            case 'o':
                strcpy(out_file, optarg);
                break;
            case 's':
                silent = 1;
                break;
            case '?':
            case 'p':
                ERR("Usage: %s to capture rkisp1 frames\n"
                "         --width,  default 640,             optional, width of image\n"
                "         --height, default 480,             optional, height of image\n"
                "         --memory, default mmap,            optional, use 'mmap' or 'drm' to alloc buffers\n"
                "         --format, default NV12,            optional, fourcc of format\n"
                "         --count,  default    5,            optional, how many frames to capture\n"
                "         --iqfile, default /etc/cam_iq.xml, optional, camera IQ file\n"
                "         --device,                          required, path of video device\n"
                "         --output,                          required, output file path, if <file> is '-', then the data is written to     stdout\n"
                "         --gain,   default 0,               optional\n"
                "         --expo,   default 0,               optional\n"
                "                   Manually AE is enable only if --gain and --expo are not zero\n"
                "         --silent,                          optional, subpress debug log\n",
                argv[0]);
                exit(-1);
            default:
                ERR("?? getopt returned character code 0%o ??\n", c);
        }
    }
    if (strlen(out_file) == 0 || strlen(dev_name) == 0) {
        ERR("arguments --output and --device are required\n");
        exit(-1);
    }
}

void *camera_test(void *argc, display_callback *hook)
{
    int err[2] = {0,0};
    char msg[80];
    int ret, num;
    tc_info = (struct testcase_info *)argc; 

    if (script_fetch("camera", "number",&num, 1) == 0) {
        printf("camera_test num:%d\r\n",num);
        camera_num = num;	
    }
    
    snprintf(msg, sizeof(msg), "%s:[%s]", PCBA_CAMERA, PCBA_TESTING);
    hook->handle_refresh_screen(tc_info->y, msg);

    system("/system/bin/media-ctl -r");

    sleep(1);

    strcpy(out_file, "/data/1.yuv");
    strcpy(dev_name, "/dev/video0");
    width = 3264;
    height = 2448;

    if (!strcmp(out_file, "-")) {
        fp = stdout;
        silent = 1;
    } else if ((fp = fopen(out_file, "w")) == NULL) {
        perror("Creat file failed");
        exit(0);
    }

    //media-ctl setup links
#ifdef RKISP_1
    err[0] = system("/system/bin/media-ctl -l '\"m00_b_ov13850 1-0010\":0->\"rockchip-mipi-dphy-rx\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rockchip-mipi-dphy-rx\":1->\"rkisp1-isp-subdev\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp1-input-params\":0->\"rkisp1-isp-subdev\":1[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp1-isp-subdev\":2->\"rkisp1_selfpath\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp1-isp-subdev\":2->\"rkisp1_mainpath\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp1-isp-subdev\":3->\"rkisp1-statistics\":0[1]'");
#endif


#ifdef RKISP_2
    err[0] = system("/system/bin/media-ctl -l '\"m00_b_ov8858 2-0036\":0->\"rockchip-mipi-dphy-rx\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rockchip-mipi-dphy-rx\":1->\"rkisp-csi-subdev\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp-csi-subdev\":1->\"rkisp-isp-subdev\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp-csi-subdev\":2->\"rkisp_rawwr0\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp-csi-subdev\":4->\"rkisp_rawwr2\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp-csi-subdev\":5->\"rkisp_rawwr3\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp-input-params\":0->\"rkisp-isp-subdev\":1[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp-isp-subdev\":2->\"rkisp_selfpath\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp-isp-subdev\":2->\"rkisp_mainpath\":0[1]'");
    err[0] = system("/system/bin/media-ctl -l '\"rkisp-isp-subdev\":3->\"rkisp-statistics\":0[1]'");
#endif

    sleep(1);

    if (err[0]) goto end;

    //set v4l2 format / size and so on
#ifdef RKISP_1
    err[0] = system("/system/bin/media-ctl --set-v4l2 '\"m00_b_ov13850 1-0010\":0[fmt:SBGGR10/2112x1568]'");
    err[0] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp1-isp-subdev\":0[fmt:SBGGR10/2112x1568]'");
    err[0] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp1-isp-subdev\":0[fmt:SBGGR10/2112x1568]' --set-v4l2 '\"rkisp1-isp-subdev\":0[crop:(0,0)/2112x1568]'");
    err[0] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp1-isp-subdev\":2[fmt:YUYV2X8/2112x1568]'");
    err[0] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp1-isp-subdev\":2[fmt:YUYV2X8/2112x1568]' --set-v4l2 '\"rkisp1-isp-subdev\":2[crop:(0,0)/2112x1568]'");
#endif

#ifdef RKISP_2
    err[0] = system("/system/bin/media-ctl --set-v4l2 '\"m00_b_ov8858 2-0036\":0[fmt:SBGGR10/3264x2448]'"); 

    err[0] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp-isp-subdev\":0[fmt:SBGGR10/3264x2448]'");

    err[0] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp-isp-subdev\":0[crop:(0,0)/3264x2448]'"); 
#endif
    sleep(1);

    if (err[0]) goto end;

    err[0] = open_device();
    if (err[0]) goto end;

    err[0] = init_device();
    if (err[0]) goto end;

    err[0] = start_capturing();
    if (err[0]) goto end;

    err[0] = mainloop();
    if (err[0]) goto end;

    fclose(fp);
    stop_capturing();
    uninit_device();
    close_device();

    if (camera_num > 1) {
        system("/system/bin/media-ctl -r");

        sleep(1);

        strcpy(out_file, "/data/2.yuv");
        strcpy(dev_name, "/dev/video0");
        width = 1600;
        height = 1200;

        if (!strcmp(out_file, "-")) {
            fp = stdout;
            silent = 1;
        } else if ((fp = fopen(out_file, "w")) == NULL) {
            perror("Creat file failed");
            exit(0);
        }
        //media-ctl setup links
#ifdef RKISP_1
        err[1] = system("/system/bin/media-ctl -l '\"m00_b_ov13850 1-0010\":0->\"rockchip-mipi-dphy-rx\":0[1]'");
        err[1] = system("/system/bin/media-ctl -l '\"rockchip-mipi-dphy-rx\":1->\"rkisp1-isp-subdev\":0[1]'");
        err[1] = system("/system/bin/media-ctl -l '\"rkisp1-input-params\":0->\"rkisp1-isp-subdev\":1[1]'");
        err[1] = system("/system/bin/media-ctl -l '\"rkisp1-isp-subdev\":2->\"rkisp1_selfpath\":0[1]'");
        err[1] = system("/system/bin/media-ctl -l '\"rkisp1-isp-subdev\":2->\"rkisp1_mainpath\":0[1]'");
        err[1] = system("/system/bin/media-ctl -l '\"rkisp1-isp-subdev\":3->\"rkisp1-statistics\":0[1]'");
#endif

#ifdef RKISP_2
        err[0] = system("/system/bin/media-ctl -l '\"m01_f_gc2385 2-0037\":0->\"rockchip-mipi-dphy-rx\":0[1]'");
        err[0] = system("/system/bin/media-ctl -l '\"rockchip-mipi-dphy-rx\":1->\"rkisp-csi-subdev\":0[1]'");
        err[0] = system("/system/bin/media-ctl -l '\"rkisp-csi-subdev\":1->\"rkisp-isp-subdev\":0[1]'");
        err[0] = system("/system/bin/media-ctl -l '\"rkisp-csi-subdev\":2->\"rkisp_rawwr0\":0[1]'");
        err[0] = system("/system/bin/media-ctl -l '\"rkisp-csi-subdev\":4->\"rkisp_rawwr2\":0[1]'");
        err[0] = system("/system/bin/media-ctl -l '\"rkisp-csi-subdev\":5->\"rkisp_rawwr3\":0[1]'");
        err[0] = system("/system/bin/media-ctl -l '\"rkisp-input-params\":0->\"rkisp-isp-subdev\":1[1]'");
        err[0] = system("/system/bin/media-ctl -l '\"rkisp-isp-subdev\":2->\"rkisp_selfpath\":0[1]'");
        err[0] = system("/system/bin/media-ctl -l '\"rkisp-isp-subdev\":2->\"rkisp_mainpath\":0[1]'");
        err[0] = system("/system/bin/media-ctl -l '\"rkisp-isp-subdev\":3->\"rkisp-statistics\":0[1]'");
#endif
        sleep(1);

        if (err[1]) goto end;

        //set v4l2 format / size and so on
#ifdef RKISP_1
        err[1] = system("/system/bin/media-ctl --set-v4l2 '\"m00_b_ov13850 1-0010\":0[fmt:SBGGR10/2112x1568]'");
        err[1] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp1-isp-subdev\":0[fmt:SBGGR10/2112x1568]'");
        err[1] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp1-isp-subdev\":0[fmt:SBGGR10/2112x1568]' --set-v4l2 '\"rkisp1-isp-subdev\":0[crop:(0,0)/2112x1568]'");
        err[1] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp1-isp-subdev\":2[fmt:YUYV2X8/2112x1568]'");
        err[1] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp1-isp-subdev\":2[fmt:YUYV2X8/2112x1568]' --set-v4l2 '\"rkisp1-isp-subdev\":2[crop:(0,0)/2112x1568]'");
#endif

#ifdef RKISP_2
        err[0] = system("/system/bin/media-ctl --set-v4l2 '\"m01_f_gc2385 2-0037\":0[fmt:SBGGR10/1600x1200]'"); 

        err[0] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp-isp-subdev\":0[fmt:SBGGR10/1600x1200]'");

        err[0] = system("/system/bin/media-ctl --set-v4l2 '\"rkisp-isp-subdev\":0[crop:(0,0)/1600x1200]'");
#endif
        sleep(1);

        if (err[1]) goto end;

        err[1] = open_device();
        if (err[1]) goto end;

        err[1] = init_device();
        if (err[1]) goto end;

        err[1] = start_capturing();
        if (err[1]) goto end;

        err[1] = mainloop();
        if (err[1]) goto end;

        fclose(fp);
        stop_capturing();
        uninit_device();
        close_device();
    }

end:	
    if (camera_num > 1) {
        bool failed = (err[0] < 0) || (err[1] < 0);
        snprintf(msg, sizeof(msg), "Back Camera:[%s] { ID:0x%x } Front Camera:[%s] { ID:0x%x }",
                 err[0] < 0 ? PCBA_FAILED : PCBA_SECCESS, 0,
                 err[1] < 0 ? PCBA_FAILED : PCBA_SECCESS, 1);
        hook->handle_refresh_screen_hl(tc_info->y, msg, failed);
    } else {
        bool failed = err[0] < 0;
        snprintf(msg, sizeof(msg), "Back Camera:[%s] { ID:0x%x }",
                 failed?PCBA_FAILED:PCBA_SECCESS, 0, failed);
        hook->handle_refresh_screen_hl(tc_info->y, msg, failed);
    }
    return argc;
}
