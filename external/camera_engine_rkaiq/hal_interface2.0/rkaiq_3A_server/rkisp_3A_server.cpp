#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h> /* getopt_long() */
#include <fcntl.h> /* low-level i/o */
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <mediactl.h>
#include <mediactl/mediactl-priv.h>
#include <linux/videodev2.h>
#include "rkisp_control_loop.h"
#include <utils/Log.h>
#include "rkisp_control_aiq.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define DBG(...) do { if(!silent) printf("DBG: " __VA_ARGS__);} while(0)
#define ERR(...) do { fprintf(stderr, "ERR: " __VA_ARGS__); } while (0)


/* Private v4l2 event */
#define CIFISP_V4L2_EVENT_STREAM_START  \
                    (V4L2_EVENT_PRIVATE_START + 1)
#define CIFISP_V4L2_EVENT_STREAM_STOP   \
                    (V4L2_EVENT_PRIVATE_START + 2)

#define FILE_PATH_LEN                       64
#define CAMS_NUM_MAX                        2
#define FLASH_NUM_MAX                       2

/* The media topology instance that describes video device and
 * sub-device informations.
 *
 * @sd_isp_path, the isp sub-device path, e.g. /dev/v4l-subdev0
 * @vd_params_path, the params video device path
 * @vd_stats_path, the stats video device path
 * @cams, multipe cameras can attache to isp, but only one can be active
 * @sd_sensor_path, the sensor sub-device path
 * @sd_lens_path, the lens sub-device path that attached to sensor
 * @sd_flash_path, the flash sub-device path that attached to sensor,
 *      can be two or more.
 * @link_enabled, the link status of this sensor
 */
#define MAX_MEDIA_DEV_NUM  10

struct rkisp_media_info {
    char sd_isp_path[FILE_PATH_LEN];
    char vd_params_path[FILE_PATH_LEN];
    char vd_stats_path[FILE_PATH_LEN];
    char sd_ispp_path[FILE_PATH_LEN];
    struct {
            char sd_sensor_path[FILE_PATH_LEN];
            char sd_lens_path[FILE_PATH_LEN];
            char sd_flash_path[FLASH_NUM_MAX][FILE_PATH_LEN];
            bool link_enabled;
            char sensor_entity_name[FILE_PATH_LEN];
    } cams[CAMS_NUM_MAX];
};

static struct rkisp_media_info media_info;
static void* rkisp_engine;
static int sensor_index = -1;
static int silent = 0;
static const char *hdrmode = "NORMAL";
static int width = 2688;
static int height = 1520;
static const char *mdev_path = NULL;
static int find_ispp = 0;
char cur_dev_path[FILE_PATH_LEN];

static void errno_exit(const char *s)
{
    ERR("%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

static int
rkisp_get_devname(struct media_device *device, const char *name, char *dev_name)
{
    const char *devname;

    media_entity *entity =  NULL;

    entity = media_get_entity_by_name(device, name, strlen(name));
    if (!entity)
        return -1;

    devname = media_entity_get_devname(entity);
    if (!devname) {
        ERR("can't find %s device path!", name);
        return -1;
    }

    strncpy(dev_name, devname, FILE_PATH_LEN);

    DBG("get %s devname: %s\n", name, dev_name);

    return 0;
}

static int
rkisp_enumrate_modules (struct media_device *device)
{
    uint32_t nents, i;
    const char* dev_name = NULL;
    int active_sensor = -1;

    nents = media_get_entities_count (device);
    for (i = 0; i < nents; ++i) {
        int module_idx = -1;
        struct media_entity *e;
        const struct media_entity_desc *ef;
        const struct media_link *link;

        e = media_get_entity(device, i);
        ef = media_entity_get_info(e);
        if (ef->type != MEDIA_ENT_T_V4L2_SUBDEV_SENSOR &&
            ef->type != MEDIA_ENT_T_V4L2_SUBDEV_FLASH &&
            ef->type != MEDIA_ENT_T_V4L2_SUBDEV_LENS)
            continue;

        if (ef->name[0] != 'm' && ef->name[3] != '_') {
            ERR("sensor/lens/flash entity name format is incorrect,"
                "pls check driver version !\n");
            return -1;
        }

        /* Retrive the sensor index from sensor name,
         * which is indicated by two characters after 'm',
         *   e.g.  m00_b_ov13850 1-0010
         *          ^^, 00 is the module index
         */
        module_idx = atoi(ef->name + 1);
        if (module_idx >= CAMS_NUM_MAX) {
            ERR("multiple sensors more than two not supported, %s\n", ef->name);
            continue;
        }

        if (sensor_index >= 0 && module_idx != sensor_index) {
            continue;
        }

        dev_name = media_entity_get_devname (e);

        switch (ef->type) {
        case MEDIA_ENT_T_V4L2_SUBDEV_SENSOR:
            strncpy(media_info.cams[module_idx].sd_sensor_path,
                    dev_name, FILE_PATH_LEN);
            link = media_entity_get_link(e, 0);
            if (link && (link->flags & MEDIA_LNK_FL_ENABLED)) {
                media_info.cams[module_idx].link_enabled = true;
                active_sensor = module_idx;
                strcpy(media_info.cams[module_idx].sensor_entity_name, ef->name);
                DBG("%s(%d) sensor_entity_name(%s) \n", __FUNCTION__, __LINE__, media_info.cams[module_idx].sensor_entity_name);
            }
            break;
        case MEDIA_ENT_T_V4L2_SUBDEV_FLASH:
            // TODO, support multiple flashes attached to one module
            strncpy(media_info.cams[module_idx].sd_flash_path[0],
                    dev_name, FILE_PATH_LEN);
            break;
        case MEDIA_ENT_T_V4L2_SUBDEV_LENS:
            strncpy(media_info.cams[module_idx].sd_lens_path,
                    dev_name, FILE_PATH_LEN);
            break;
        default:
            break;
        }
    }

    if (active_sensor < 0) {
        ERR("Not sensor link is enabled, does sensor probe correctly?\n");
        return -1;
    }

    return 0;
}

int rkaiq_get_media_info() {
  int ret = 0;
  unsigned int i, index = 0;
  char sys_path[64];
  int find_sensor = 0;
  int find_isp = 0;
  int linked_sensor = -1;
  struct media_device *device = NULL;
  const struct media_device_info *info = NULL;
  char model[64] = "\0";
  while (index < MAX_MEDIA_DEV_NUM) {
    info = NULL;
    snprintf(sys_path, 64, "/dev/media%d", index++);
    DBG("media get sys_path: %s\n", sys_path);
    if(access(sys_path,F_OK))
      continue;

    device = media_device_new(sys_path);
    if (device == NULL) {
      ERR("Failed to create media %s\n", sys_path);
      continue;
    }

    ret = media_device_enumerate(device);
    if (ret < 0) {
      ERR("Failed to enumerate %s (%d)\n", sys_path, ret);
      media_device_unref(device);
      continue;
    }

    info = media_get_info(device);

    /* Try Sensor */
    if (!find_sensor) {
      unsigned int nents = media_get_entities_count(device);
      for (i = 0; i < nents; ++i) {
        struct media_entity *entity = media_get_entity(device, i);
        const struct media_entity_desc *desc = media_entity_get_info(entity);
        unsigned int type = desc ->type;
        if (MEDIA_ENT_T_V4L2_SUBDEV == (type & MEDIA_ENT_TYPE_MASK)) {
           unsigned int subtype = type & MEDIA_ENT_SUBTYPE_MASK;
           if (subtype == 1) {
               ret = rkisp_enumrate_modules(device);
               if (!ret) { 
                   linked_sensor = index;
                   find_sensor = 1;
                   if (info && !strncmp(info->driver, "rkcif", strlen("rkcif"))) {
                       find_sensor = 2;
                       strncpy(model, info->model, 64);
                   }
               }
           }
        }
      }
    }

    if (find_sensor == 2 && strlen(model) > 0) {
        media_entity *entity =  NULL;
        entity = media_get_entity_by_name(device, model, strlen(model));
        if (!entity) {
            continue;
        }
    }

    /* Try rkisp */
    if (!find_isp) {
      ret = rkisp_get_devname(device, "rkisp-isp-subdev", media_info.sd_isp_path);
      ret |= rkisp_get_devname(device, "rkisp-input-params", media_info.vd_params_path);
      ret |= rkisp_get_devname(device, "rkisp-statistics", media_info.vd_stats_path);
      if (ret == 0) {
        find_isp = 1;
      }
    }

    /* Try rkispp */
    if (!find_ispp) {
      ret = rkisp_get_devname(device, "rkispp_input_params", media_info.sd_ispp_path);
      if (ret == 0) {
        find_ispp = 1;
      }
    }

    media_device_unref(device);
  }

  if (find_sensor && (find_isp || find_ispp))
    return 0;

  ERR("find_sensor %d find_isp %d find_ispp %d\n",
            find_sensor, find_isp, find_ispp);

  return ret;
}

#if 0
static int rkisp_get_media_info(const char *mdev_path) {
    struct media_device *device = NULL;
    int ret;

    CLEAR(media_info);

    device = media_device_new (mdev_path);
    /* Enumerate entities, pads and links. */
    media_device_enumerate (device);

    ret = rkisp_get_devname(device, "rkisp-isp-subdev",
                            media_info.sd_isp_path);
    ret |= rkisp_get_devname(device, "rkisp-input-params",
                             media_info.vd_params_path);
    ret |= rkisp_get_devname(device, "rkisp-statistics",
                             media_info.vd_stats_path);
    if (ret) {
        media_device_unref (device);
        return ret;
    }

    ret = rkisp_enumrate_modules(device);
    media_device_unref (device);

    return ret;
}
#endif

static void init_engine(void)
{
    int ret = 0;

    for (int i = 0; i < CAMS_NUM_MAX; i++) {
        if (!media_info.cams[i].link_enabled) {
            DBG("Link disabled, skipped\n");
            continue;
        }
        if (sensor_index >= 0 && i != sensor_index)
            continue;

        ret = rkisp_cl_rkaiq_init(&rkisp_engine, NULL, NULL, media_info.cams[i].sensor_entity_name);
        if (ret) {
            ERR("rkisp engine init failed !\n");
            exit(-1);
        }
        setMulCamConc(rkisp_engine,true);

        break;
    }

}

static void prepare_engine(void)
{
    struct rkisp_cl_prepare_params_s params = {0};
    int ret = 0;

    params.isp_sd_node_path = media_info.sd_isp_path;
    params.isp_vd_params_path = media_info.vd_params_path;
    params.isp_vd_stats_path = media_info.vd_stats_path;
    params.staticMeta = NULL;
    params.width = width;
    params.height = height;
    params.work_mode = hdrmode;
    DBG("%s--set workingmode(%s)\n", __FUNCTION__, params.work_mode);

    for (int i = 0; i < CAMS_NUM_MAX; i++) {
        if (!media_info.cams[i].link_enabled) {
            DBG("Link disabled, skipped\n");
            continue;
        }
        if (sensor_index >= 0 && i != sensor_index)
            continue;
        DBG("%s - %s: link enabled : %d\n", media_info.cams[i].sd_sensor_path,
            media_info.cams[i].sensor_entity_name,
            media_info.cams[i].link_enabled);

        params.sensor_sd_node_path = media_info.cams[i].sd_sensor_path;

        if (strlen(media_info.cams[i].sd_lens_path))
            params.lens_sd_node_path = media_info.cams[i].sd_lens_path;
        if (strlen(media_info.cams[i].sd_flash_path[0]))
            params.flashlight_sd_node_path[0] = media_info.cams[i].sd_flash_path[0];

        if (rkisp_cl_prepare(rkisp_engine, &params)) {
            ERR("rkisp engine prepare failed !\n");
            exit(-1);
        }

        break;
    }

}

static void start_engine(void)
{
    DBG("rkaiq start\n");
    rkisp_cl_start(rkisp_engine);
    if (rkisp_engine == NULL) {
        ERR("rkisp_init engine failed\n");
        exit(-1);
    } else {
        DBG("rkisp_init engine succeed\n");
    }
}

static void stop_engine(void)
{
    rkisp_cl_stop(rkisp_engine);
}

static void deinit_engine(void)
{
    rkisp_cl_deinit(rkisp_engine);
}

// blocked func
static int wait_stream_event(int fd, unsigned int event_type, int time_out_ms)
{
    int ret;
    struct v4l2_event event;

    CLEAR(event);

    do {
	/*
	 * xioctl instead of poll.
	 * Since poll() cannot wait for input before stream on,
	 * it will return an error directly. So, use ioctl to
	 * dequeue event and block until sucess.
	 */
	ret = xioctl(fd, VIDIOC_DQEVENT, &event);
	if (ret == 0 && event.type == event_type) {
		return 0;
	}
    } while (true);

    return -1;

}

static int subscrible_stream_event(int fd, bool subs)
{
    struct v4l2_event_subscription sub;
    int ret = 0;

    DBG("subscribe events from %s !\n", cur_dev_path);
    CLEAR(sub);
    sub.type = CIFISP_V4L2_EVENT_STREAM_START;
    ret = xioctl(fd,
                 subs ? VIDIOC_SUBSCRIBE_EVENT : VIDIOC_UNSUBSCRIBE_EVENT,
                 &sub);
    if (ret) {
        ERR("can't subscribe %s start event!\n", cur_dev_path);
        exit(EXIT_FAILURE);
    }

    CLEAR(sub);
    sub.type = CIFISP_V4L2_EVENT_STREAM_STOP;
    ret = xioctl(fd,
                 subs ? VIDIOC_SUBSCRIBE_EVENT : VIDIOC_UNSUBSCRIBE_EVENT,
                 &sub);
    if (ret) {
        ERR("can't subscribe %s stop event!\n", cur_dev_path);
        exit(EXIT_FAILURE);
    }

    DBG("subscribe events from %s success !\n", cur_dev_path);

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
           {"sensor_index",    optional_argument  , 0, 'd' },
           {"mmedia",    optional_argument  , 0, 'm' },
           {"silent",    no_argument,       0, 's' },
           {"help",      no_argument,       0, 'h' },
           {"hdrmode",   required_argument, 0, 'r' },
           {0,           0,                 0,  0  }
       };

       c = getopt_long(argc, argv, "m::shr:", long_options, &option_index);
       if (c == -1)
           break;

       switch (c) {
           case 'd':
               sensor_index = atoi(optarg);
               break;
           case 'm':
               mdev_path = optarg;
               break;
           case 'w':
               width = atoi(optarg);
               break;
           case 'h':
               height = atoi(optarg);
               break;
           case 's':
               silent = 1;
               break;
           case 'r':
               hdrmode = optarg;
               break;
           case '?':
               ERR("Usage: %s to start 3A engine\n"
                   "         --sensor_index,  optional, sendor index\n"
                   "         --silent,        optional, subpress debug log\n",
                   "         --hdrmode,       required, NORMAL/HDR2/HDR3 \n",
                   argv[0]);
               exit(-1);

           default:
               ERR("?? getopt returned character code %c ??\n", c);
       }
   }
}

int main(int argc, char **argv)
{
    int ret = 0;
    int isp_fd;
    unsigned int stream_event = -1;

    /* Line buffered so that printf can flash every line if redirected to
     * no-interactive device.
     */
    setlinebuf(stdout);

    parse_args(argc, argv);

    /* Refresh media info so that sensor link status updated */
    //if (rkisp_get_media_info(mdev_path)) //not used now
    if (rkaiq_get_media_info())
        errno_exit("Bad media topology\n");

    if(find_ispp) {
        strncpy(cur_dev_path, media_info.sd_ispp_path, FILE_PATH_LEN);
        isp_fd = open(cur_dev_path, O_RDWR);
    }
    else {
        strncpy(cur_dev_path, media_info.vd_params_path, FILE_PATH_LEN);
        isp_fd = open(cur_dev_path, O_RDWR);
    }
    if (isp_fd < 0) {
        ERR("open %s failed %s\n", cur_dev_path,
            strerror(errno));
        exit(-1);
    }
    subscrible_stream_event(isp_fd, true);
    init_engine();
    prepare_engine();

    for (;;) {
        DBG("wait stream start event...\n");
        wait_stream_event(isp_fd, CIFISP_V4L2_EVENT_STREAM_START, -1);
        DBG("wait stream start event success ...\n");

        start_engine();

        DBG("wait stream stop event...\n");
        wait_stream_event(isp_fd, CIFISP_V4L2_EVENT_STREAM_STOP, -1);
        DBG("wait stream stop event success ...\n");

        stop_engine();
    }
    deinit_engine();
    subscrible_stream_event(isp_fd, false);
    close(isp_fd);

    DBG("----------------------------------------------\n\n");


    return 0;
}
