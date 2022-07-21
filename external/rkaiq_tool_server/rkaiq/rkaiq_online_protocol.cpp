#include "rkaiq_online_protocol.h"
#include "rk-camera-module.h"
#include "rkaiq_protocol.h"
#include "tcp_client.h"
#include "rkaiq_media.h"
#include <regex>

#ifdef LOG_TAG
    #undef LOG_TAG
#endif
#define LOG_TAG "aiqtool"

extern int g_width;
extern int g_height;
extern std::string g_stream_dev_name;
extern std::shared_ptr<RKAiqMedia> rkaiq_media;
extern int g_device_id;
extern uint32_t g_sensorHdrMode;

static uint16_t capture_check_sum;
static int capture_status = READY;
static int capture_frames = 1;
static int capture_frames_index = 0;
static char captureDevNode[128] = {0};
static struct capture_info cap_info;

#define FMT_NUM_PLANES 1
#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct YuvCaptureBuffer {
    void* start;
    size_t length;
};

static void HexDump(const unsigned char* data, size_t size)
{
    printf("####\n");
    int i;
    size_t offset = 0;
    while (offset < size) {
        printf("%04x  ", offset);
        for (i = 0; i < 16; i++) {
            if (i % 8 == 0) {
                putchar(' ');
            }
            if (offset + i < size) {
                printf("%02x ", data[offset + i]);
            } else {
                printf("   ");
            }
        }
        printf("   ");
        for (i = 0; i < 16 && offset + i < size; i++) {
            if (isprint(data[offset + i])) {
                printf("%c", data[offset + i]);
            } else {
                putchar('.');
            }
        }
        putchar('\n');
        offset += 16;
    }
    printf("####\n\n");
}

static void DoAnswer(int sockfd, CommandData_t* cmd, int cmd_id, int ret_status)
{
    char send_data[MAXPACKETSIZE];
    LOG_DEBUG("enter\n");

    strncpy((char*)cmd->RKID, RKID_ISP_ON, sizeof(cmd->RKID));
    cmd->cmdType = CMD_TYPE_CAPTURE;
    cmd->cmdID = cmd_id;
    strncpy((char*)cmd->version, RKAIQ_TOOL_VERSION, sizeof(cmd->version));
    cmd->datLen = 4;
    memset(cmd->dat, 0, sizeof(cmd->dat));
    cmd->dat[0] = ret_status;
    cmd->checkSum = 0;
    for (int i = 0; i < cmd->datLen; i++) {
        cmd->checkSum += cmd->dat[i];
    }

    memcpy(send_data, cmd, sizeof(CommandData_t));
    send(sockfd, send_data, sizeof(CommandData_t), 0);
    LOG_DEBUG("exit\n");
}

static void DoAnswer2(int sockfd, CommandData_t* cmd, int cmd_id, uint16_t check_sum, uint32_t result)
{
    char send_data[MAXPACKETSIZE];
    LOG_DEBUG("enter\n");
    strncpy((char*)cmd->RKID, RKID_ISP_ON, sizeof(cmd->RKID));
    cmd->cmdType = CMD_TYPE_CAPTURE;
    cmd->cmdID = cmd_id;
    strncpy((char*)cmd->version, RKAIQ_TOOL_VERSION, sizeof(cmd->version));
    cmd->datLen = 4;
    memset(cmd->dat, 0, sizeof(cmd->dat));
    cmd->dat[0] = result;
    cmd->dat[1] = check_sum & 0xFF;
    cmd->dat[2] = (check_sum >> 8) & 0xFF;
    cmd->checkSum = 0;
    for (int i = 0; i < cmd->datLen; i++) {
        cmd->checkSum += cmd->dat[i];
    }

    memcpy(send_data, cmd, sizeof(CommandData_t));
    send(sockfd, send_data, sizeof(CommandData_t), 0);
    LOG_DEBUG("exit\n");
}

static int DoCheckSum(int sockfd, uint16_t check_sum)
{
    char recv_data[MAXPACKETSIZE];
    size_t recv_size = 0;
    int param_size = sizeof(CommandData_t);
    int remain_size = param_size;
    int try_count = 3;
    LOG_DEBUG("enter\n");

    struct timeval interval = {3, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&interval, sizeof(struct timeval));

    while (remain_size > 0) {
        int offset = param_size - remain_size;
        recv_size = recv(sockfd, recv_data + offset, remain_size, 0);
        if (recv_size < 0) {
            if (errno == EAGAIN) {
                LOG_DEBUG("recv size %zu, do try again, count %d\n", recv_size, try_count);
                try_count--;
                if (try_count < 0) {
                    break;
                }
                continue;
            } else {
                LOG_ERROR("Error socket recv failed %d\n", errno);
            }
        }
        remain_size = remain_size - recv_size;
    }
    LOG_DEBUG("recv_size: 0x%lx expect 0x%lx\n", recv_size, sizeof(CommandData_t));

    CommandData_t* cmd = (CommandData_t*)recv_data;
    uint16_t recv_check_sum = 0;
    recv_check_sum += cmd->dat[0] & 0xff;
    recv_check_sum += (cmd->dat[1] & 0xff) << 8;
    LOG_DEBUG("check_sum local: 0x%x pc: 0x%x\n", check_sum, recv_check_sum);

    if (check_sum != recv_check_sum) {
        LOG_DEBUG("check_sum fail!\n");
        return -1;
    }

    LOG_DEBUG("exit\n");
    return 0;
}

static void OnLineSet(int sockfd, CommandData_t* cmd, uint16_t& check_sum, uint32_t& result)
{
    int recv_size = 0;
    int param_size = *(int*)cmd->dat;
    int remain_size = param_size;

    LOG_DEBUG("enter\n");
    LOG_DEBUG("expect recv param_size 0x%x\n", param_size);
    char* param = (char*)malloc(param_size);
    while (remain_size > 0) {
        int offset = param_size - remain_size;
        recv_size = recv(sockfd, param + offset, remain_size, 0);
        remain_size = remain_size - recv_size;
    }

    LOG_DEBUG("recv ready\n");

    for (int i = 0; i < param_size; i++) {
        check_sum += param[i];
    }

    LOG_DEBUG("DO Sycn Setting, CmdId: 0x%x, expect ParamSize %d\n", cmd->cmdID, param_size);
#if 0
  if (rkaiq_manager) {
    result = rkaiq_manager->IoCtrl(cmd->cmdID, param, param_size);
  }
#endif
    if (param != NULL) {
        free(param);
    }
    LOG_DEBUG("exit\n");
}

static int OnLineGet(int sockfd, CommandData_t* cmd)
{
    int ret = 0;
    int ioRet = 0;
    int send_size = 0;
    int param_size = *(int*)cmd->dat;
    int remain_size = param_size;
    LOG_DEBUG("enter\n");
    LOG_DEBUG("ParamSize: 0x%x\n", param_size);

    uint8_t* param = (uint8_t*)malloc(param_size);

    LOG_INFO("DO Get Setting, CmdId: 0x%x, expect ParamSize %d\n", cmd->cmdID, param_size);
#if 0
  if (rkaiq_manager) {
    ioRet = rkaiq_manager->IoCtrl(cmd->cmdID, param, param_size);
    if (ioRet != 0) {
      LOG_ERROR("DO Get Setting, io get data failed. return\n");
      return 1;
    }
  }
#endif

    while (remain_size > 0) {
        int offset = param_size - remain_size;
        send_size = send(sockfd, param + offset, remain_size, 0);
        remain_size = param_size - send_size;
    }

    uint16_t check_sum = 0;
    for (int i = 0; i < param_size; i++) {
        check_sum += param[i];
    }

    ret = DoCheckSum(sockfd, check_sum);

    if (param != NULL) {
        free(param);
        param = NULL;
    }
    return ret;
}

static void SendYuvData(int socket, int index, void* buffer, int size)
{
    char* buf = NULL;
    int total = size;
    int packet_len = MAXPACKETSIZE;
    int send_size = 0;
    int ret_val;
    uint16_t check_sum = 0;

    // buf = (char*)buffer;
    // for (int i = 0; i < size; i++)
    // {
    //     check_sum += buf[i];
    // }
    // capture_check_sum = check_sum;

    buf = (char*)buffer;
    while (total > 0) {
        if (total < packet_len) {
            send_size = total;
        } else {
            send_size = packet_len;
        }
        ret_val = send(socket, buf, send_size, 0);
        total -= send_size;
        buf += ret_val;
    }
}

static void SendYuvDataResult(int sockfd, CommandData_t* cmd, CommandData_t* recv_cmd)
{
    unsigned short* checksum;
    char send_data[MAXPACKETSIZE];
    checksum = (unsigned short*)&recv_cmd->dat[1];
    strncpy((char*)cmd->RKID, RKID_ISP_ON, sizeof(cmd->RKID));
    cmd->cmdType = CMD_TYPE_CAPTURE;
    cmd->cmdID = CMD_ID_CAPTURE_YUV_CAPTURE;
    cmd->datLen = 2;
    memset(cmd->dat, 0, sizeof(cmd->dat));
    cmd->dat[0] = 0x04;
    LOG_DEBUG("capture_check_sum %d, recieve %d\n", capture_check_sum, *checksum);
    if (capture_check_sum == *checksum) {
        cmd->dat[1] = RES_SUCCESS;
    } else {
        cmd->dat[1] = RES_FAILED;
    }
    cmd->checkSum = 0;
    capture_check_sum = 0;
    for (int i = 0; i < cmd->datLen; i++) {
        cmd->checkSum += cmd->dat[i];
    }

    memcpy(send_data, cmd, sizeof(CommandData_t));
    send(sockfd, send_data, sizeof(CommandData_t), 0);
}

static void SendOnlineRawDataResult(int sockfd, CommandData_t* cmd, CommandData_t* recv_cmd)
{
    unsigned short* checksum;
    char send_data[MAXPACKETSIZE];
    checksum = (unsigned short*)&recv_cmd->dat[1];
    strncpy((char*)cmd->RKID, RKID_ISP_ON, sizeof(cmd->RKID));
    cmd->cmdType = CMD_TYPE_CAPTURE;
    cmd->cmdID = CMD_ID_CAPTURE_ONLINE_RAW_CAPTURE;
    cmd->datLen = 2;
    memset(cmd->dat, 0, sizeof(cmd->dat));
    cmd->dat[0] = 0x04;
    LOG_DEBUG("capture_check_sum %d, recieve %d\n", capture_check_sum, *checksum);
    if (capture_check_sum == *checksum) {
        cmd->dat[1] = RES_SUCCESS;
    } else {
        cmd->dat[1] = RES_FAILED;
    }
    cmd->checkSum = 0;
    capture_check_sum = 0;
    for (int i = 0; i < cmd->datLen; i++) {
        cmd->checkSum += cmd->dat[i];
    }

    memcpy(send_data, cmd, sizeof(CommandData_t));
    send(sockfd, send_data, sizeof(CommandData_t), 0);
}

#ifndef __ANDROID__
extern std::shared_ptr<easymedia::Flow> video_dump_flow;
#endif
static const uint32_t kSocket_fd = (1 << 0);
static const uint32_t kEnable_Link = (1 << 1);

void LinkCaptureCallBack(unsigned char* buffer, unsigned int buffer_size, int sockfd, uint32_t sequence)
{
    LOG_DEBUG("sockfd %d buffer %p, size %d, sequence:%u\n", sockfd, buffer, buffer_size, sequence);
    SendYuvData(sockfd, capture_frames_index++, buffer, buffer_size);
}

long GetFileSize(char* filename)
{
    struct stat stat_buf;
    int rc = stat(filename, &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void ExecuteCMD(const char* cmd, char* result)
{
    char buf_ps[1024];
    char ps[1024] = {0};
    FILE* ptr;
    strcpy(ps, cmd);
    if ((ptr = popen(ps, "r")) != NULL) {
        while (fgets(buf_ps, 1024, ptr) != NULL) {
            strcat(result, buf_ps);
            if (strlen(result) > 1024) {
                break;
            }
        }
        pclose(ptr);
        ptr = NULL;
    } else {
        printf("popen %s error\n", ps);
    }
}

static int DoCaptureYuv(int sockfd)
{
    LOG_DEBUG("DoCaptureYuv\n");
    bool videoDevNodeFindedFlag = false;
    char cmdResStr[128] = {0};
    memset(captureDevNode, 0, sizeof(captureDevNode));
    if (g_stream_dev_name.length() > 0) {
        videoDevNodeFindedFlag = true;
        memcpy(captureDevNode, g_stream_dev_name.c_str(), g_stream_dev_name.length());
        LOG_INFO("DoCaptureYuv,using specific dev node:%s\n", captureDevNode);
    } else {
        LOG_INFO("DoCaptureYuv,using rkisp_iqtool node.\n");
        if (videoDevNodeFindedFlag == false) {
            memset(cmdResStr, 0, sizeof(cmdResStr));
            ExecuteCMD("media-ctl -d /dev/media0 -e rkisp_iqtool", cmdResStr);
            if (strstr(cmdResStr, "/dev/video") != NULL) {
                videoDevNodeFindedFlag = true;
                memcpy(captureDevNode, cmdResStr, sizeof(cmdResStr));
            }
        }
        if (videoDevNodeFindedFlag == false) {
            memset(cmdResStr, 0, sizeof(cmdResStr));
            ExecuteCMD("media-ctl -d /dev/media1 -e rkisp_iqtool", cmdResStr);
            if (strstr(cmdResStr, "/dev/video") != NULL) {
                videoDevNodeFindedFlag = true;
                memcpy(captureDevNode, cmdResStr, sizeof(cmdResStr));
            }
        }
        if (videoDevNodeFindedFlag == false) {
            memset(cmdResStr, 0, sizeof(cmdResStr));
            ExecuteCMD("media-ctl -d /dev/media2 -e rkisp_iqtool", cmdResStr);
            if (strstr(cmdResStr, "/dev/video") != NULL) {
                videoDevNodeFindedFlag = true;
                memcpy(captureDevNode, cmdResStr, sizeof(cmdResStr));
            }
        }
    }

    if (videoDevNodeFindedFlag == false) {
        LOG_ERROR("Video capture device node not found.\n");
        return 1;
    } else {
        captureDevNode[strcspn(captureDevNode, "\n")] = '\0';
        LOG_DEBUG("Video capture device node:%s\n", captureDevNode);
    }

    //
    struct v4l2_capability capCapability;
    struct v4l2_format capFmt;

    int fd = open(captureDevNode, O_RDWR, 0);
    if (-1 == fd) {
        LOG_ERROR("Cannot open '%s': %d, %s\n", captureDevNode, errno, strerror(errno));
        return -1;
    }

    //
    CLEAR(capCapability);
    if (xioctl(fd, VIDIOC_QUERYCAP, &capCapability) < 0) {
        LOG_ERROR("Failed to ioctl(VIDIOC_QUERYCAP)\n");
        return -1;
    }
    // HexDump((unsigned char*)&capCapability, sizeof(v4l2_capability));
    // LOG_DEBUG("capCapability.capabilities:%u\n", capCapability.capabilities);
    // if (!((capCapability.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE))
    // {
    //     LOG_ERROR("%s, Not a video capture device.\n", captureDevNode);
    //     return -1;
    // }
    // if (!((capCapability.capabilities & V4L2_CAP_STREAMING) == V4L2_CAP_STREAMING))
    // {
    //     LOG_ERROR("%s does not support the streaming I/O method.\n", captureDevNode);
    //     return -1;
    // }

    CLEAR(capFmt);
    capFmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (capCapability.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
        capFmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    capFmt.fmt.pix.width = g_width;
    capFmt.fmt.pix.height = g_height;
    capFmt.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12;
    if (xioctl(fd, VIDIOC_S_FMT, &capFmt) < 0) {
        LOG_ERROR("Failed to ioctl(VIDIOC_S_FMT)\n");
        return -1;
    }
    if (xioctl(fd, VIDIOC_G_FMT, &capFmt) < 0) {
        LOG_ERROR("Failed to ioctl(VIDIOC_G_FMT)\n");
        return -1;
    }

    //
    uint32_t file_length = capFmt.fmt.pix.width * capFmt.fmt.pix.height * 1.5; // NV12:x1.5
    LOG_DEBUG("file_length:%u\n", file_length);

    struct v4l2_requestbuffers req;
    CLEAR(req);
    req.count = 4; // memory map buffer count
    req.type = (v4l2_buf_type)capFmt.type;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        LOG_ERROR("Failed to ioctl(VIDIOC_REQBUFS)\n");
        return -1;
    }
    if (req.count < 2) {
        LOG_ERROR("Insufficient buffer memory\n");
        return -1;
    }
    //

    struct YuvCaptureBuffer* buffers = (YuvCaptureBuffer*)calloc(req.count, sizeof(*buffers));
    unsigned int n_buffers = 0;
    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[FMT_NUM_PLANES];
        CLEAR(buf);
        CLEAR(planes);
        buf.type = (v4l2_buf_type)capFmt.type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;
        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == capFmt.type) {
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }
        if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) {
            LOG_ERROR("VIDIOC_QUERYBUF error\n");
        }
        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == capFmt.type) {
            buffers[n_buffers].length = buf.m.planes[0].length;
            buffers[n_buffers].start = mmap(NULL, buf.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                            buf.m.planes[0].m.mem_offset);
        } else {
            buffers[n_buffers].length = buf.length;
            buffers[n_buffers].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        }

        if (MAP_FAILED == buffers[n_buffers].start) {
            LOG_ERROR("memory map failed\n");
        }
    }

    for (uint i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;
        struct v4l2_plane planes[FMT_NUM_PLANES];
        CLEAR(buf);
        CLEAR(planes);
        buf.type = (v4l2_buf_type)capFmt.type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == (v4l2_buf_type)capFmt.type) {
            buf.m.planes = planes;
            buf.length = FMT_NUM_PLANES;
        }
        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
            LOG_ERROR("VIDIOC_QBUF failed\n");
        }
    }

    if (-1 == xioctl(fd, VIDIOC_STREAMON, &capFmt.type)) {
        LOG_ERROR("VIDIOC_STREAMON failed\n");
        return -1;
    }

    while (1) {
        fd_set fds;
        struct timeval tv;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        // 2 sec Timeout
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        int ret = select(fd + 1, &fds, NULL, NULL, &tv);
        if (-1 == ret) {
            if (EINTR == errno)
                continue;
            LOG_ERROR("select failed\n");
        } else if (0 == ret) {
            fprintf(stderr, "select timeout\n");
            LOG_ERROR("select timeout\n");
            return -1;
            // exit(EXIT_FAILURE);
        }

        //
        // FILE* file_fd = fopen("YUV_Frame.yuv", "w");
        while (capture_frames_index < capture_frames) {
            struct v4l2_buffer buf;
            struct v4l2_plane planes[FMT_NUM_PLANES];
            CLEAR(buf);
            CLEAR(planes);
            buf.type = (v4l2_buf_type)capFmt.type;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = capture_frames_index;
            if (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == (v4l2_buf_type)capFmt.type) {
                buf.m.planes = planes;
                buf.length = FMT_NUM_PLANES;
            }
            if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
                LOG_ERROR("VIDIOC_DQBUF failed\n");
            }
            assert(buf.index < n_buffers);
            LOG_INFO("YUV capture, sequence:%u\n", buf.sequence);
            // fwrite(buffers[buf.index].start, file_length, 1, file_fd);
            SendYuvData(sockfd, capture_frames_index, buffers[buf.index].start, file_length);
            if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) {
                LOG_ERROR("VIDIOC_QBUF failed\n");
            }

            capture_frames_index++;
        }
        // fclose(file_fd);
        break;
    }

    // unmap
    for (uint i = 0; i < n_buffers; ++i) {
        if (-1 == munmap(buffers[i].start, buffers[i].length)) {
            LOG_ERROR("munmap error\n");
        }
    }

    free(buffers);
    close(fd);
    return 0;
}

static void RawCaptureDeinit()
{
    if (cap_info.subdev_fd > 0) {
        device_close(cap_info.subdev_fd);
        cap_info.subdev_fd = -1;
        LOG_DEBUG("device_close(cap_info.subdev_fd)\n");
    }
    if (cap_info.dev_fd > 0) {
        device_close(cap_info.dev_fd);
        cap_info.dev_fd = -1;
        LOG_DEBUG("device_close(cap_info.dev_fd)\n");
    }
}

static void DumpCapinfo()
{
    LOG_INFO("DumpCapinfo: \n");
    LOG_INFO("    dev_name ------------- %s\n", cap_info.dev_name);
    LOG_INFO("    dev_fd --------------- %d\n", cap_info.dev_fd);
    LOG_INFO("    io ------------------- %d\n", cap_info.io);
    LOG_INFO("    width ---------------- %d\n", cap_info.width);
    LOG_INFO("    height --------------- %d\n", cap_info.height);
    LOG_INFO("    format --------------- %d\n", cap_info.format);
    LOG_INFO("    capture_buf_type ----- %d\n", cap_info.capture_buf_type);
    LOG_INFO("    out_file ------------- %s\n", cap_info.out_file);
    LOG_INFO("    frame_count ---------- %d\n", cap_info.frame_count);
}

static void SendRawData(int socket, int index, void* buffer, int size)
{
    LOG_DEBUG("SendRawData\n");
    assert(buffer);

    char* buf = NULL;
    int total = size;
    int packet_len = MAXPACKETSIZE;
    int send_size = 0;
    int ret_val;
    uint16_t check_sum = 0;
    buf = (char*)buffer;
    while (total > 0) {
        if (total < packet_len) {
            send_size = total;
        } else {
            send_size = packet_len;
        }
        ret_val = send(socket, buf, send_size, 0);
        total -= send_size;
        buf += ret_val;
    }

    buf = (char*)buffer;
    for (int i = 0; i < size; i++) {
        check_sum += buf[i];
    }

    LOG_INFO("capture raw index %d, check_sum %d capture_check_sum %d\n", index, check_sum, capture_check_sum);
    capture_check_sum = check_sum;
}

static void OnlineRawCaptureCallBack(int socket, int index, void* buffer, int size)
{
    LOG_DEBUG("OnlineRawCaptureCallBack size %d\n", size);
    int width = cap_info.width;
    int height = cap_info.height;
    LOG_DEBUG("cap_info.width %d\n", cap_info.width);
    LOG_DEBUG("cap_info.height %d\n", cap_info.height);
    if (g_sensorHdrMode == NO_HDR && size > (width * height * 2)) {
        LOG_ERROR("DoMultiFrameCallBack size error\n");
        return;
    }
    SendRawData(socket, index, buffer, size);
}

static int DoCaptureOnlineRaw(int sockfd)
{
    init_device(&cap_info);
    DumpCapinfo();
    start_capturing(&cap_info);

    LOG_DEBUG("DoCapture entry!!!!!\n");
    AutoDuration ad;
    int skip_frame = 5;

    if (capture_frames_index == 0) {
        for (int i = 0; i < skip_frame; i++) {
            read_frame(sockfd, i, &cap_info, nullptr);
            LOG_DEBUG("DoCapture skip frame %d ...\n", i);
        }
    }

    while (capture_frames_index < capture_frames) {
        read_frame(sockfd, capture_frames_index, &cap_info, OnlineRawCaptureCallBack);
        capture_frames_index++;
    }

    LOG_INFO("DoCapture %ld ms %ld us\n", ad.Get() / 1000, ad.Get() % 1000);
    LOG_DEBUG("DoCapture exit!!!!!\n");

    stop_capturing(&cap_info);
    uninit_device(&cap_info);
    RawCaptureDeinit();
    return 0;
}

static void ReplyStatus(int sockfd, CommandData_t* cmd, int ret_status)
{
    LOG_DEBUG("enter\n");
    char send_data[MAXPACKETSIZE];
    strncpy((char*)cmd->RKID, RKID_ISP_ON, sizeof(cmd->RKID));
    cmd->cmdType = CMD_TYPE_CAPTURE;
    cmd->cmdID = CMD_ID_CAPTURE_YUV_CAPTURE;
    cmd->datLen = 2;
    memset(cmd->dat, 0, sizeof(cmd->dat));
    cmd->dat[0] = DATA_ID_CAPTURE_RAW_STATUS; // ProcessID
    cmd->dat[1] = ret_status;
    cmd->checkSum = 0;
    for (int i = 0; i < cmd->datLen; i++) {
        cmd->checkSum += cmd->dat[i];
    }

    LOG_DEBUG("cmd->checkSum %d\n", cmd->checkSum);
    memcpy(send_data, cmd, sizeof(CommandData_t));
    send(sockfd, send_data, sizeof(CommandData_t), 0);
    LOG_DEBUG("exit\n");
}

static void ReplyOnlineRawStatus(int sockfd, CommandData_t* cmd, int ret_status)
{
    LOG_DEBUG("enter\n");
    char send_data[MAXPACKETSIZE];
    strncpy((char*)cmd->RKID, RKID_ISP_ON, sizeof(cmd->RKID));
    cmd->cmdType = CMD_TYPE_CAPTURE;
    cmd->cmdID = CMD_ID_CAPTURE_ONLINE_RAW_CAPTURE;
    cmd->datLen = 2;
    memset(cmd->dat, 0, sizeof(cmd->dat));
    cmd->dat[0] = DATA_ID_CAPTURE_ONLINE_RAW_STATUS; // ProcessID
    cmd->dat[1] = ret_status;
    cmd->checkSum = 0;
    for (int i = 0; i < cmd->datLen; i++) {
        cmd->checkSum += cmd->dat[i];
    }

    LOG_DEBUG("cmd->checkSum %d\n", cmd->checkSum);
    memcpy(send_data, cmd, sizeof(CommandData_t));
    send(sockfd, send_data, sizeof(CommandData_t), 0);
    LOG_DEBUG("exit\n");
}

static void ReplySensorPara(int sockfd, CommandData_t* cmd)
{
    LOG_DEBUG("enter\n");
    char cmdResStr[2048] = {0};
    bool videoDevNodeFindedFlag = false;
    memset(captureDevNode, 0, sizeof(captureDevNode));

    if (videoDevNodeFindedFlag == false) {
        memset(cmdResStr, 0, sizeof(cmdResStr));
        ExecuteCMD("media-ctl -d /dev/media0 -e rkisp_mainpath", cmdResStr);
        if (strstr(cmdResStr, "/dev/video") != NULL) {
            videoDevNodeFindedFlag = true;
            memcpy(captureDevNode, cmdResStr, strlen(cmdResStr) + 1);
        }
    }

    if (videoDevNodeFindedFlag == false) {
        memset(cmdResStr, 0, sizeof(cmdResStr));
        ExecuteCMD("media-ctl -d /dev/media1 -e rkisp_mainpath", cmdResStr);
        if (strstr(cmdResStr, "/dev/video") != NULL) {
            videoDevNodeFindedFlag = true;
            memcpy(captureDevNode, cmdResStr, strlen(cmdResStr) + 1);
        }
    }

    if (videoDevNodeFindedFlag == false) {
        memset(cmdResStr, 0, sizeof(cmdResStr));
        ExecuteCMD("media-ctl -d /dev/media2 -e rkisp_mainpath", cmdResStr);
        if (strstr(cmdResStr, "/dev/video") != NULL) {
            videoDevNodeFindedFlag = true;
            memcpy(captureDevNode, cmdResStr, strlen(cmdResStr) + 1);
        }
    }

    if (videoDevNodeFindedFlag == false) {
        LOG_ERROR("Video capture device node not found.\n");
        return;
    } else {
        captureDevNode[strcspn(captureDevNode, "\n")] = '\0';
        LOG_DEBUG("Video capture device node:%s\n", captureDevNode);
    }
    if (strlen(captureDevNode) == 0) {
        LOG_ERROR("Video capture device node not found.\n");
        return;
    }

    char tmpCmd[128] = {0};
    memset(tmpCmd, 0, sizeof(tmpCmd));
    sprintf(tmpCmd, "v4l2-ctl -d %s --set-fmt-video=width=%d,height=%d", captureDevNode, g_width, g_height);
    memset(cmdResStr, 0, sizeof(cmdResStr));
    ExecuteCMD(tmpCmd, cmdResStr);

    memset(tmpCmd, 0, sizeof(tmpCmd));
    sprintf(tmpCmd, "v4l2-ctl -d %s --get-fmt-video", captureDevNode);
    memset(cmdResStr, 0, sizeof(cmdResStr));
    ExecuteCMD(tmpCmd, cmdResStr);

    //
    std::string srcStr = cmdResStr;
    std::smatch results;
    // get width / height
    std::string resolutionStrPattern{"Width/Height.*: (.*)/(.*)"};
    std::regex reResolution(resolutionStrPattern);
    results.empty();
    std::regex_search(srcStr, results, reResolution);
    assert(results.length() >= 2);
    string width = results.str(1);
    string height = results.str(2);
    if (g_stream_dev_name.length() == 0) {
        g_width = atoi(width.c_str());
        g_height = atoi(height.c_str());
    }
    if (g_width == 0 || g_height == 0) {
        LOG_ERROR("Captrure YUV, get output resolution failed.\n");
    } else {
        LOG_ERROR("Captrure YUV, get resolution %d x %d\n", g_width, g_height);
    }
    // get pixel format
    std::string pixelStrPattern{"Pixel Format.*'(.*)'"};
    std::regex rePixelStrPattern(pixelStrPattern);
    results.empty();
    std::regex_search(srcStr, results, rePixelStrPattern);
    assert(results.length() >= 2);
    string pixelFormat = results.str(1);
    if (pixelFormat.length() == 0) {
        LOG_ERROR("Captrure YUV, get pixel format failed.\n");
    } else {
        LOG_ERROR("Captrure YUV, get pixel format:%s.\n", pixelFormat.c_str());
    }

    char send_data[MAXPACKETSIZE];
    memset(cmd, 0, sizeof(CommandData_t));
    strncpy((char*)cmd->RKID, RKID_ISP_ON, sizeof(cmd->RKID));
    cmd->cmdType = CMD_TYPE_CAPTURE;
    cmd->cmdID = CMD_ID_CAPTURE_YUV_CAPTURE;
    cmd->datLen = 3;
    Sensor_Yuv_Params_t* param = (Sensor_Yuv_Params_t*)(cmd->dat);
    param->data_id = DATA_ID_CAPTURE_YUV_GET_PARAM;
    param->width = g_width;
    param->height = g_height;
    if (pixelFormat == "YUYV") {
        param->format = RKISP_FORMAT_YUYV;
    } else {
        param->format = RKISP_FORMAT_NV12;
    }

    cmd->checkSum = 0;
    for (int i = 0; i < cmd->datLen; i++) {
        cmd->checkSum += cmd->dat[i];
    }

    LOG_DEBUG("cmd->checkSum %d\n", cmd->checkSum);
    memcpy(send_data, cmd, sizeof(CommandData_t));
    send(sockfd, send_data, sizeof(CommandData_t), 0);
    LOG_DEBUG("exit\n");
}

static void InitOnlineRawCapture()
{
    media_info_t mi = rkaiq_media->GetMediaInfoT(g_device_id);
    if (mi.cif.linked_sensor) {
        cap_info.link = link_to_vicap;
        strcpy(cap_info.sd_path.device_name, mi.cif.sensor_subdev_path.c_str());
        strcpy(cap_info.cif_path.cif_video_path, mi.cif.mipi_id0.c_str());
        cap_info.dev_name = cap_info.cif_path.cif_video_path;
    } else if (mi.dvp.linked_sensor) {
        cap_info.link = link_to_dvp;
        strcpy(cap_info.sd_path.device_name, mi.dvp.sensor_subdev_path.c_str());
        strcpy(cap_info.cif_path.cif_video_path, mi.dvp.dvp_id0.c_str());
        cap_info.dev_name = cap_info.cif_path.cif_video_path;
    } else {
        cap_info.link = link_to_isp;
        strcpy(cap_info.sd_path.device_name, mi.isp.sensor_subdev_path.c_str());
        strcpy(cap_info.vd_path.isp_main_path, mi.isp.main_path.c_str());
        cap_info.dev_name = cap_info.vd_path.isp_main_path;
    }
    strcpy(cap_info.vd_path.media_dev_path, mi.isp.media_dev_path.c_str());
    strcpy(cap_info.vd_path.isp_sd_path, mi.isp.isp_dev_path.c_str());
    if (mi.lens.module_lens_dev_name.length()) {
        strcpy(cap_info.lens_path.lens_device_name, mi.lens.module_lens_dev_name.c_str());
    } else {
        cap_info.lens_path.lens_device_name[0] = '\0';
    }
    cap_info.dev_fd = -1;
    cap_info.subdev_fd = -1;
    cap_info.lensdev_fd = -1;
    LOG_DEBUG("cap_info.link: %d \n", cap_info.link);
    LOG_DEBUG("cap_info.dev_name: %s \n", cap_info.dev_name);
    LOG_DEBUG("cap_info.isp_media_path: %s \n", cap_info.vd_path.media_dev_path);
    LOG_DEBUG("cap_info.vd_path.isp_sd_path: %s \n", cap_info.vd_path.isp_sd_path);
    LOG_DEBUG("cap_info.sd_path.device_name: %s \n", cap_info.sd_path.device_name);
    LOG_DEBUG("cap_info.lens_path.lens_dev_name: %s \n", cap_info.lens_path.lens_device_name);

    cap_info.io = IO_METHOD_MMAP;
    cap_info.height = g_height;
    cap_info.width = g_width;
    // cap_info.format = v4l2_fourcc('B', 'G', '1', '2');
    LOG_DEBUG("get ResW: %d  ResH: %d\n", cap_info.width, cap_info.height);

    //
    int fd = device_open(cap_info.sd_path.device_name);
    LOG_DEBUG("sensor subdev path: %s\n", cap_info.sd_path.device_name);
    LOG_DEBUG("cap_info.subdev_fd: %d\n", fd);
    if (fd < 0) {
        LOG_ERROR("Open %s failed.\n", cap_info.sd_path.device_name);
    } else {
        rkmodule_hdr_cfg hdrCfg;
        int ret = ioctl(fd, RKMODULE_GET_HDR_CFG, &hdrCfg);
        if (ret > 0) {
            g_sensorHdrMode = NO_HDR;
            LOG_ERROR("Get sensor hdr mode failed, use default, No HDR\n");
        } else {
            g_sensorHdrMode = hdrCfg.hdr_mode;
            LOG_INFO("Get sensor hdr mode:%u\n", g_sensorHdrMode);
        }
    }
    close(fd);
    if (mi.cif.linked_sensor) {
        if (g_sensorHdrMode == NO_HDR) {
            LOG_INFO("Get sensor mode: NO_HDR\n");
            strcpy(cap_info.cif_path.cif_video_path, mi.cif.mipi_id0.c_str());
            cap_info.dev_name = cap_info.cif_path.cif_video_path;
        } else if (g_sensorHdrMode == HDR_X2) {
            LOG_INFO("Get sensor mode: HDR_2\n");
            strcpy(cap_info.cif_path.cif_video_path, mi.cif.mipi_id1.c_str());
            cap_info.dev_name = cap_info.cif_path.cif_video_path;
        } else if (g_sensorHdrMode == HDR_X3) {
            LOG_INFO("Get sensor mode: HDR_3\n");
            strcpy(cap_info.cif_path.cif_video_path, mi.cif.mipi_id2.c_str());
            cap_info.dev_name = cap_info.cif_path.cif_video_path;
        }
    }

    // Get Sensor Para
    struct v4l2_queryctrl ctrl;
    struct v4l2_subdev_frame_interval finterval;
    struct v4l2_subdev_format fmt;
    struct v4l2_format format;

    int hblank, vblank;
    int vts, hts, ret;
    float fps;
    int endianness = 0;

    cap_info.subdev_fd = device_open(cap_info.sd_path.device_name);
    LOG_DEBUG("sensor subdev path: %s\n", cap_info.sd_path.device_name);

    uint16_t sensorFormat;
    // set capture image data format
    if (g_sensorHdrMode == NO_HDR) {
        sensorFormat = PROC_ID_CAPTURE_RAW_NON_COMPACT_LINEAR;
        LOG_INFO("NO_HDR | sensorFormat:%d\n", sensorFormat);
    } else if (g_sensorHdrMode == HDR_X2) {
        sensorFormat = PROC_ID_CAPTURE_RAW_COMPACT_HDR2_ALIGN256;
        LOG_INFO("HDR_X2 | sensorFormat:%d\n", sensorFormat);
    } else if (g_sensorHdrMode == HDR_X3) {
        sensorFormat = PROC_ID_CAPTURE_RAW_COMPACT_HDR3_ALIGN256;
        LOG_INFO("HDR_X3 | sensorFormat:%d\n", sensorFormat);
    } else {
        LOG_ERROR("Get sensor hdr mode failed, hdr mode:%u, use default.No HDR\n", g_sensorHdrMode);
        sensorFormat = PROC_ID_CAPTURE_RAW_NON_COMPACT_LINEAR;
        LOG_INFO("NO_HDR | sensorFormat:%d\n", sensorFormat);
    }

    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_HBLANK;
    if (device_getblank(cap_info.subdev_fd, &ctrl) < 0) {
        goto end;
    }
    hblank = ctrl.minimum;
    LOG_DEBUG("get hblank: %d\n", hblank);

    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_VBLANK;
    if (device_getblank(cap_info.subdev_fd, &ctrl) < 0) {
        goto end;
    }
    vblank = ctrl.minimum;
    LOG_DEBUG("get vblank: %d\n", vblank);

    memset(&fmt, 0, sizeof(fmt));
    fmt.pad = 0;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    if (device_getsubdevformat(cap_info.subdev_fd, &fmt) < 0) {
        goto end;
    }
    vts = vblank + fmt.format.height;
    hts = hblank + fmt.format.width;
    LOG_DEBUG("get hts: %d  vts: %d\n", hts, vts);
    cap_info.format = convert_to_v4l2fmt(&cap_info, fmt.format.code);
    cap_info.sd_path.sen_fmt = fmt.format.code;
    cap_info.sd_path.width = fmt.format.width;
    cap_info.sd_path.height = fmt.format.height;

    LOG_DEBUG("get sensor code: %d  bits: %d, cap_info.format:  %d\n", cap_info.sd_path.sen_fmt, cap_info.sd_path.bits,
              cap_info.format);

    /* set isp subdev fmt to bayer raw*/
    if (cap_info.link == link_to_isp) {
        ret = rkisp_set_ispsd_fmt(&cap_info, fmt.format.width, fmt.format.height, fmt.format.code, cap_info.width,
                                  cap_info.height, fmt.format.code);
        endianness = 1;
        LOG_DEBUG("rkisp_set_ispsd_fmt: %d endianness = %d\n", ret, endianness);

        if (ret) {
            LOG_ERROR("subdev choose the best fit fmt: %dx%d, 0x%08x\n", fmt.format.width, fmt.format.height,
                      fmt.format.code);
            goto end;
        }
    }

    memset(&finterval, 0, sizeof(finterval));
    finterval.pad = 0;
    if (device_getsensorfps(cap_info.subdev_fd, &finterval) < 0) {
        goto end;
    }
    fps = (float)(finterval.interval.denominator) / finterval.interval.numerator;
    LOG_DEBUG("get fps: %f\n", fps);

    if (cap_info.subdev_fd > 0) {
        device_close(cap_info.subdev_fd);
        cap_info.subdev_fd = -1;
    }
    if (cap_info.dev_fd > 0) {
        device_close(cap_info.dev_fd);
        cap_info.dev_fd = -1;
    }
    return;

end:
    if (cap_info.subdev_fd > 0) {
        device_close(cap_info.subdev_fd);
        cap_info.subdev_fd = -1;
    }
}

static void ReplyOnlineRawSensorPara(int sockfd, CommandData_t* cmd)
{
    LOG_DEBUG("enter\n");
    char cmdResStr[2048] = {0};
    bool videoDevNodeFindedFlag = false;
    memset(captureDevNode, 0, sizeof(captureDevNode));

    if (videoDevNodeFindedFlag == false) {
        if (g_device_id == 0) {
            memset(cmdResStr, 0, sizeof(cmdResStr));
            ExecuteCMD("media-ctl -d /dev/media0 -e stream_cif_mipi_id0", cmdResStr);
            if (strstr(cmdResStr, "/dev/video") != NULL) {
                videoDevNodeFindedFlag = true;
                memcpy(captureDevNode, cmdResStr, strlen(cmdResStr) + 1);
            }
        } else if (g_device_id == 1) {
            memset(cmdResStr, 0, sizeof(cmdResStr));
            ExecuteCMD("media-ctl -d /dev/media1 -e stream_cif_mipi_id0", cmdResStr);
            if (strstr(cmdResStr, "/dev/video") != NULL) {
                videoDevNodeFindedFlag = true;
                memcpy(captureDevNode, cmdResStr, strlen(cmdResStr) + 1);
            }
        }
    }

    if (videoDevNodeFindedFlag == false) {
        LOG_ERROR("Video capture device node not found.\n");
        return;
    } else {
        captureDevNode[strcspn(captureDevNode, "\n")] = '\0';
        LOG_DEBUG("Video capture device node:%s\n", captureDevNode);
    }
    if (strlen(captureDevNode) == 0) {
        LOG_ERROR("Video capture device node not found.\n");
        return;
    }

    char tmpCmd[128] = {0};
    memset(tmpCmd, 0, sizeof(tmpCmd));
    sprintf(tmpCmd, "v4l2-ctl -d %s --get-fmt-video", captureDevNode);
    memset(cmdResStr, 0, sizeof(cmdResStr));
    ExecuteCMD(tmpCmd, cmdResStr);

    //
    std::string srcStr = cmdResStr;
    std::smatch results;
    // get width / height
    std::string resolutionStrPattern{"Width/Height.*: (.*)/(.*)"};
    std::regex reResolution(resolutionStrPattern);
    results.empty();
    std::regex_search(srcStr, results, reResolution);
    assert(results.length() >= 2);
    string width = results.str(1);
    string height = results.str(2);
    g_width = atoi(width.c_str());
    g_height = atoi(height.c_str());
    if (g_width == 0 || g_height == 0) {
        LOG_ERROR("Captrure online raw, get output resolution failed.\n");
    } else {
        LOG_ERROR("Captrure online raw, get resolution %d x %d\n", g_width, g_height);
    }

    //
    InitOnlineRawCapture();

    // get pixel format
    std::string pixelStrPattern{"Pixel Format.*'(.*)'"};
    std::regex rePixelStrPattern(pixelStrPattern);
    results.empty();
    std::regex_search(srcStr, results, rePixelStrPattern);
    assert(results.length() >= 2);
    string pixelFormat = results.str(1);
    if (pixelFormat.length() == 0) {
        LOG_ERROR("Captrure online raw, get pixel format failed.\n");
    } else {
        LOG_ERROR("Captrure online raw, get pixel format:%s.\n", pixelFormat.c_str());
    }

    char send_data[MAXPACKETSIZE];
    memset(cmd, 0, sizeof(CommandData_t));
    strncpy((char*)cmd->RKID, RKID_ISP_ON, sizeof(cmd->RKID));
    cmd->cmdType = CMD_TYPE_CAPTURE;
    cmd->cmdID = CMD_ID_CAPTURE_ONLINE_RAW_CAPTURE;
    cmd->datLen = 3;
    Sensor_Online_Raw_Params_t* param = (Sensor_Online_Raw_Params_t*)(cmd->dat);
    param->data_id = DATA_ID_CAPTURE_ONLINE_RAW_GET_PARAM;
    param->width = g_width;
    param->height = g_height;
    param->bits = cap_info.sd_path.bits;

    uint16_t sensorFormat;
    // set capture image data format
    if (g_sensorHdrMode == NO_HDR) {
        sensorFormat = PROC_ID_CAPTURE_RAW_NON_COMPACT_LINEAR;
        LOG_INFO("NO_HDR | sensorFormat:%d\n", sensorFormat);
    } else if (g_sensorHdrMode == HDR_X2) {
        sensorFormat = PROC_ID_CAPTURE_RAW_COMPACT_HDR2_ALIGN256;
        LOG_INFO("HDR_X2 | sensorFormat:%d\n", sensorFormat);
    } else if (g_sensorHdrMode == HDR_X3) {
        sensorFormat = PROC_ID_CAPTURE_RAW_COMPACT_HDR3_ALIGN256;
        LOG_INFO("HDR_X3 | sensorFormat:%d\n", sensorFormat);
    } else {
        LOG_ERROR("Get sensor hdr mode failed, hdr mode:%u, use default.No HDR\n", g_sensorHdrMode);
        sensorFormat = PROC_ID_CAPTURE_RAW_NON_COMPACT_LINEAR;
        LOG_INFO("NO_HDR | sensorFormat:%d\n", sensorFormat);
    }
    param->sensorImageFormat = sensorFormat;

    cmd->checkSum = 0;
    for (int i = 0; i < cmd->datLen; i++) {
        cmd->checkSum += cmd->dat[i];
    }

    LOG_DEBUG("cmd->checkSum %d\n", cmd->checkSum);
    memcpy(send_data, cmd, sizeof(CommandData_t));
    send(sockfd, send_data, sizeof(CommandData_t), 0);
    LOG_DEBUG("exit\n");
}

static void SetSensorPara(int sockfd, CommandData_t* recv_cmd, CommandData_t* cmd)
{
    LOG_DEBUG("enter\n");
    char send_data[MAXPACKETSIZE];

    Capture_Yuv_Params_t* CapParam = (Capture_Yuv_Params_t*)(recv_cmd->dat + 1);
    LOG_DEBUG(" set gain        : %d\n", CapParam->gain);
    LOG_DEBUG(" set exposure    : %d\n", CapParam->time);
    LOG_DEBUG(" set fmt        : %d\n", CapParam->fmt);
    LOG_DEBUG(" set framenumber : %d\n", CapParam->framenumber);

    capture_frames = CapParam->framenumber;
    capture_frames_index = 0;
    capture_check_sum = 0;

    memset(cmd, 0, sizeof(CommandData_t));
    strncpy((char*)cmd->RKID, RKID_ISP_ON, sizeof(cmd->RKID));
    cmd->cmdType = PC_TO_DEVICE;
    cmd->cmdID = CMD_ID_CAPTURE_YUV_CAPTURE;
    cmd->datLen = 2;
    memset(cmd->dat, 0, sizeof(cmd->dat));
    cmd->dat[0] = DATA_ID_CAPTURE_RAW_SET_PARAM;
    cmd->dat[1] = RES_SUCCESS;
    cmd->checkSum = 0;
    for (int i = 0; i < cmd->datLen; i++) {
        cmd->checkSum += cmd->dat[i];
    }
    LOG_DEBUG("cmd->checkSum %d\n", cmd->checkSum);
    memcpy(send_data, cmd, sizeof(CommandData_t));
    send(sockfd, send_data, sizeof(CommandData_t), 0);
    LOG_DEBUG("exit\n");
}

static void SetOnlineRawSensorPara(int sockfd, CommandData_t* recv_cmd, CommandData_t* cmd)
{
    LOG_DEBUG("enter\n");
    char send_data[MAXPACKETSIZE];

    Capture_Yuv_Params_t* CapParam = (Capture_Yuv_Params_t*)(recv_cmd->dat + 1);
    // LOG_DEBUG(" set gain        : %d\n", CapParam->gain);
    // LOG_DEBUG(" set exposure    : %d\n", CapParam->time);
    // LOG_DEBUG(" set fmt        : %d\n", CapParam->fmt);
    LOG_DEBUG(" set framenumber : %d\n", CapParam->framenumber);

    capture_frames = CapParam->framenumber;
    capture_frames_index = 0;
    capture_check_sum = 0;

    memset(cmd, 0, sizeof(CommandData_t));
    strncpy((char*)cmd->RKID, RKID_ISP_ON, sizeof(cmd->RKID));
    cmd->cmdType = PC_TO_DEVICE;
    cmd->cmdID = CMD_ID_CAPTURE_ONLINE_RAW_CAPTURE;
    cmd->datLen = 2;
    memset(cmd->dat, 0, sizeof(cmd->dat));
    cmd->dat[0] = DATA_ID_CAPTURE_ONLINE_RAW_SET_PARAM;
    cmd->dat[1] = RES_SUCCESS;
    cmd->checkSum = 0;
    for (int i = 0; i < cmd->datLen; i++) {
        cmd->checkSum += cmd->dat[i];
    }
    LOG_DEBUG("cmd->checkSum %d\n", cmd->checkSum);
    memcpy(send_data, cmd, sizeof(CommandData_t));
    send(sockfd, send_data, sizeof(CommandData_t), 0);
    LOG_DEBUG("exit\n");
}

void RKAiqOLProtocol::HandlerOnLineMessage(int sockfd, char* buffer, int size)
{
    CommandData_t* common_cmd = (CommandData_t*)buffer;
    CommandData_t send_cmd;
    int ret_val, ret;

    LOG_DEBUG("HandlerOnLineMessage:\n");
    LOG_DEBUG("DATA datLen: 0x%x\n", common_cmd->datLen);

    if (strcmp((char*)common_cmd->RKID, TAG_OL_PC_TO_DEVICE) == 0) {
        LOG_DEBUG("RKID: %s\n", common_cmd->RKID);
    } else {
        LOG_DEBUG("RKID: Unknow\n");
        return;
    }

    LOG_DEBUG("cmdID: 0x%x cmdType: 0x%x\n", common_cmd->cmdID, common_cmd->cmdType);

    switch (common_cmd->cmdType) {
        case CMD_TYPE_STREAMING:
            RKAiqProtocol::DoChangeAppMode(APP_RUN_STATUS_TUNRING);
            if (common_cmd->cmdID == 0xffff) {
                uint16_t check_sum;
                uint32_t result;
                DoAnswer(sockfd, &send_cmd, common_cmd->cmdID, READY);
                OnLineSet(sockfd, common_cmd, check_sum, result);
                DoAnswer2(sockfd, &send_cmd, common_cmd->cmdID, check_sum, result ? RES_FAILED : RES_SUCCESS);
            }
            break;
        case CMD_TYPE_STATUS:
            DoAnswer(sockfd, &send_cmd, common_cmd->cmdID, READY);
            break;
        case CMD_TYPE_UAPI_SET: {
            uint16_t check_sum;
            uint32_t result;
            DoAnswer(sockfd, &send_cmd, common_cmd->cmdID, READY);
            OnLineSet(sockfd, common_cmd, check_sum, result);
            DoAnswer2(sockfd, &send_cmd, common_cmd->cmdID, check_sum, result ? RES_FAILED : RES_SUCCESS);
        } break;
        case CMD_TYPE_UAPI_GET: {
            ret = OnLineGet(sockfd, common_cmd);
            if (ret == 0) {
                DoAnswer(sockfd, &send_cmd, common_cmd->cmdID, RES_SUCCESS);
            } else {
                DoAnswer(sockfd, &send_cmd, common_cmd->cmdID, RES_FAILED);
            }
        } break;
        case CMD_TYPE_CAPTURE: {
            LOG_DEBUG("CMD_TYPE_CAPTURE in\n");
            // DoCaptureYuv(sockfd);
            char* datBuf = (char*)(common_cmd->dat);
            switch (datBuf[0]) {
                case DATA_ID_CAPTURE_YUV_STATUS:
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_YUV_STATUS in\n");
                    ReplyStatus(sockfd, &send_cmd, READY);
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_YUV_STATUS out\n");
                    break;
                case DATA_ID_CAPTURE_YUV_GET_PARAM:
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_YUV_GET_PARAM in\n");
                    ReplySensorPara(sockfd, &send_cmd);
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_YUV_GET_PARAM out\n");
                    break;
                case DATA_ID_CAPTURE_YUV_SET_PARAM:
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_YUV_SET_PARAM in\n");
                    SetSensorPara(sockfd, common_cmd, &send_cmd);
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_YUV_SET_PARAM out\n");
                    break;
                case DATA_ID_CAPTURE_YUV_START: {
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_YUV_START in\n");
                    capture_status = BUSY;
                    DoCaptureYuv(sockfd);
                    capture_status = READY;
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_YUV_START out\n");
                    break;
                }
                case DATA_ID_CAPTURE_YUV_CHECKSUM:
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_YUV_CHECKSUM in\n");
                    LOG_DEBUG("DATA_ID_CAPTURE_YUV_CHECKSUM SKIP\n");
                    // SendYuvDataResult(sockfd, &send_cmd, common_cmd);
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_YUV_CHECKSUM out\n");
                    break;

                // online raw
                case DATA_ID_CAPTURE_ONLINE_RAW_STATUS: {
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_ONLINE_RAW_STATUS in\n");
                    char result[2048] = {0};
                    std::string pattern{"Isp online"};
                    std::regex re(pattern);
                    std::smatch results;
                    ExecuteCMD("cat /proc/rkisp0-vir0", result);
                    std::string srcStr = result;
                    // LOG_INFO("#### srcStr:%s\n", srcStr.c_str());
                    if (!std::regex_search(srcStr, results, re)) // not found
                    {
                        char result[2048] = {0};
                        std::string pattern{"Isp online"};
                        std::regex re(pattern);
                        std::smatch results;
                        ExecuteCMD("cat /proc/rkisp1-vir0", result);
                        std::string srcStr = result;
                        // LOG_INFO("#### srcStr:%s\n", srcStr.c_str());
                        if (!std::regex_search(srcStr, results, re)) // not found
                        {
                            char result[2048] = {0};
                            std::string pattern{"Isp online"};
                            std::regex re(pattern);
                            std::smatch results;
                            ExecuteCMD("cat /proc/rkisp-vir0", result);
                            std::string srcStr = result;
                            // LOG_INFO("#### srcStr:%s\n", srcStr.c_str());
                            if (!std::regex_search(srcStr, results, re)) // not found
                            {
                                char result[2048] = {0};
                                std::string pattern{"Isp online"};
                                std::regex re(pattern);
                                std::smatch results;
                                ExecuteCMD("cat /proc/rkisp-unite", result);
                                std::string srcStr = result;
                                // LOG_INFO("#### srcStr:%s\n", srcStr.c_str());
                                if (!std::regex_search(srcStr, results, re)) // not found
                                {
                                    LOG_INFO("Isp not online, online raw capture not available.\n");
                                    return;
                                }
                            }
                        }
                    }

                    if (capture_status == READY) {
                        ReplyOnlineRawStatus(sockfd, &send_cmd, READY);
                    }
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_ONLINE_RAW_STATUS out\n");
                    break;
                }
                case DATA_ID_CAPTURE_ONLINE_RAW_GET_PARAM:
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_ONLINE_RAW_GET_PARAM in\n");
                    ReplyOnlineRawSensorPara(sockfd, &send_cmd);
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_ONLINE_RAW_GET_PARAM out\n");
                    break;
                case DATA_ID_CAPTURE_ONLINE_RAW_SET_PARAM:
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_ONLINE_RAW_SET_PARAM in\n");
                    SetOnlineRawSensorPara(sockfd, common_cmd, &send_cmd);
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_ONLINE_RAW_SET_PARAM out\n");
                    break;
                case DATA_ID_CAPTURE_ONLINE_RAW_START: {
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_ONLINE_RAW_START in\n");
                    capture_status = BUSY;
                    DoCaptureOnlineRaw(sockfd);
                    capture_status = READY;
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_ONLINE_RAW_START out\n");
                    break;
                }
                case DATA_ID_CAPTURE_ONLINE_RAW_CHECKSUM:
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_ONLINE_RAW_CHECKSUM in\n");
                    LOG_DEBUG("DATA_ID_CAPTURE_ONLINE_RAW_CHECKSUM SKIP\n");
                    // SendYuvDataResult(sockfd, &send_cmd, common_cmd);
                    LOG_DEBUG("ProcID DATA_ID_CAPTURE_ONLINE_RAW_CHECKSUM out\n");
                    break;
                default:
                    break;
            }
            LOG_DEBUG("CMD_TYPE_CAPTURE out\n\n");
        } break;
        default:
            LOG_INFO("cmdID: Unknow\n");
            break;
    }
}
