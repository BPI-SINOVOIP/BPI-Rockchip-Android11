#define LOG_TAG "alsa_pcm"
//#define LOG_NDEBUG 0
#include <cutils/log.h>
#include <cutils/config_utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <linux/ioctl.h>

#include "alsa_audio.h"

#define __force
#define __bitwise
#define __user
#include "../asound.h"

#define PARAM_MAX SNDRV_PCM_HW_PARAM_LAST_INTERVAL

int64_t last_read_time = 0;

static int64_t systemTime()
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec*1000000000LL + t.tv_nsec;
}

static inline int param_is_mask(int p)
{
    return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
        (p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline int param_is_interval(int p)
{
    return (p >= SNDRV_PCM_HW_PARAM_FIRST_INTERVAL) &&
        (p <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL);
}

static inline struct snd_interval *param_to_interval(struct snd_pcm_hw_params *p, int n)
{
    return &(p->intervals[n - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL]);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
    return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned bit)
{
    if (bit >= SNDRV_MASK_MAX)
        return;
    if (param_is_mask(n)) {
        struct snd_mask *m = param_to_mask(p, n);
        m->bits[0] = 0;
        m->bits[1] = 0;
        m->bits[bit >> 5] |= (1 << (bit & 31));
    }
}

static void param_set_min(struct snd_pcm_hw_params *p, int n, unsigned val)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        i->min = val;
    }
}

static void param_set_max(struct snd_pcm_hw_params *p, int n, unsigned val)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        i->max = val;
    }
}

static void param_set_int(struct snd_pcm_hw_params *p, int n, unsigned val)
{
    if (param_is_interval(n)) {
        struct snd_interval *i = param_to_interval(p, n);
        i->min = val;
        i->max = val;
        i->integer = 1;
    }
}

static void param_init(struct snd_pcm_hw_params *p)
{
    int n;
    memset(p, 0, sizeof(*p));
    for (n = SNDRV_PCM_HW_PARAM_FIRST_MASK;
         n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++) {
            struct snd_mask *m = param_to_mask(p, n);
            m->bits[0] = ~0;
            m->bits[1] = ~0;
    }
    for (n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
         n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++) {
            struct snd_interval *i = param_to_interval(p, n);
            i->min = 0;
            i->max = ~0;
    }
}

/* debugging gunk */

#if DEBUG
static const char *param_name[PARAM_MAX+1] = {
    [SNDRV_PCM_HW_PARAM_ACCESS] = "access",
    [SNDRV_PCM_HW_PARAM_FORMAT] = "format",
    [SNDRV_PCM_HW_PARAM_SUBFORMAT] = "subformat",

    [SNDRV_PCM_HW_PARAM_SAMPLE_BITS] = "sample_bits",
    [SNDRV_PCM_HW_PARAM_FRAME_BITS] = "frame_bits",
    [SNDRV_PCM_HW_PARAM_CHANNELS] = "channels",
    [SNDRV_PCM_HW_PARAM_RATE] = "rate",
    [SNDRV_PCM_HW_PARAM_PERIOD_TIME] = "period_time",
    [SNDRV_PCM_HW_PARAM_PERIOD_SIZE] = "period_size",
    [SNDRV_PCM_HW_PARAM_PERIOD_BYTES] = "period_bytes",
    [SNDRV_PCM_HW_PARAM_PERIODS] = "periods",
    [SNDRV_PCM_HW_PARAM_BUFFER_TIME] = "buffer_time",
    [SNDRV_PCM_HW_PARAM_BUFFER_SIZE] = "buffer_size",
    [SNDRV_PCM_HW_PARAM_BUFFER_BYTES] = "buffer_bytes",
    [SNDRV_PCM_HW_PARAM_TICK_TIME] = "tick_time",
};

static void param_dump(struct snd_pcm_hw_params *p)
{
    int n;

    for (n = SNDRV_PCM_HW_PARAM_FIRST_MASK;
         n <= SNDRV_PCM_HW_PARAM_LAST_MASK; n++) {
            struct snd_mask *m = param_to_mask(p, n);
            printf("%s = %08x%08x\n", param_name[n],
                   m->bits[1], m->bits[0]);
    }
    for (n = SNDRV_PCM_HW_PARAM_FIRST_INTERVAL;
         n <= SNDRV_PCM_HW_PARAM_LAST_INTERVAL; n++) {
            struct snd_interval *i = param_to_interval(p, n);
            printf("%s = (%d,%d) omin=%d omax=%d int=%d empty=%d\n",
                   param_name[n], i->min, i->max, i->openmin,
                   i->openmax, i->integer, i->empty);
    }
    printf("info = %08x\n", p->info);
    printf("msbits = %d\n", p->msbits);
    printf("rate = %d/%d\n", p->rate_num, p->rate_den);
    printf("fifo = %d\n", (int) p->fifo_size);
}

static void info_dump(struct snd_pcm_info *info)
{
    printf("device = %d\n", info->device);
    printf("subdevice = %d\n", info->subdevice);
    printf("stream = %d\n", info->stream);
    printf("card = %d\n", info->card);
    printf("id = '%s'\n", info->id);
    printf("name = '%s'\n", info->name);
    printf("subname = '%s'\n", info->subname);
    printf("dev_class = %d\n", info->dev_class);
    printf("dev_subclass = %d\n", info->dev_subclass);
    printf("subdevices_count = %d\n", info->subdevices_count);
    printf("subdevices_avail = %d\n", info->subdevices_avail);
}
#else
static void param_dump(struct snd_pcm_hw_params *p) {}
static void info_dump(struct snd_pcm_info *info) {}
#endif

unsigned pcm_buffer_size(struct pcm *pcm)
{
    return pcm->buffer_size;
}

const char* pcm_error(struct pcm *pcm)
{
    return pcm->error;
}

static int oops(struct pcm *pcm, int e, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static int oops(struct pcm *pcm, int e, const char *fmt, ...)
{
    va_list ap;
    int sz;

    va_start(ap, fmt);
    vsnprintf(pcm->error, PCM_ERROR_MAX, fmt, ap);
    va_end(ap);
    sz = strlen(pcm->error);

    if (errno)
        snprintf(pcm->error + sz, PCM_ERROR_MAX - sz,
                 ": %s", strerror(e));
    return -1;
}

int pcm_write(struct pcm *pcm, void *data, unsigned count)
{
    struct snd_xferi x;

    if (pcm->flags & PCM_IN)
        return -EINVAL;

    x.buf = data;
    x.frames = (pcm->flags & PCM_MONO) ? (count / 2) : (count / 4);

    for (;;) {
        if (!pcm->running) {
            if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_PREPARE))
                return oops(pcm, errno, "cannot prepare channel");
            if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x))
                return oops(pcm, errno, "cannot write initial data");
            pcm->running = 1;
            return 0;
        }
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x)) {
#ifdef SUPPORT_USB
            //usb sound card out, so sleep for data and return no error.
            unsigned int usleep_time = 0;
            unsigned int frames = (pcm->flags & PCM_MONO) ? (count / 2) : (count / 4);

            if ((pcm->flags & PCM_RATE_MASK) == PCM_8000HZ)
                usleep_time = frames * 1000 * 1000 / 8000;
            else if ((pcm->flags & PCM_RATE_MASK) == PCM_48000HZ)
                usleep_time = frames * 1000 * 1000 / 48000;
            else
                usleep_time = frames * 1000 * 1000 / 44100;
            usleep(usleep_time);

            return 0;
#endif
            pcm->running = 0;
            if (errno == EPIPE) {
                    /* we failed to make our window -- try to restart */
                pcm->underruns++;
                continue;
            }
            return oops(pcm, errno, "cannot write stream data");
        }
        return 0;
    }
}

/********************************
	author:charles chen
	data:2012.09.27
	parameter 
	data: the input data buf point
	len:   the input data len need consider the pcm_format
	ret: 0:Left and right channel is valid
		  1:Left      channel is valid
		  2:Right    channel is valid

defalt the input signal is like LRLRLR,default pcm_format is 16bit
*********************************/
#define SAMPLECOUNT 441*5*2*2
int channalFlags = -1;//mean the channel is not checked now

int startCheckCount = 0;

int channel_check(void * data, unsigned len)
{
	short * pcmLeftChannel = (short *)data;
	short * pcmRightChannel = pcmLeftChannel+1;
	unsigned index = 0;
	int leftValid = 0x0;
	int rightValid = 0x0;
	short checkValue = 0;
	
	checkValue = *pcmLeftChannel;

	//checkleft first
	for(index = 0; index < len; index += 2)
	{
		
		if((pcmLeftChannel[index] >= checkValue+50)||(pcmLeftChannel[index] <= checkValue-50))
		{
			leftValid++;// = 0x01;
		        //printf("-->pcmLeftChannel[%d] = %d checkValue %d leftValid %d",index,pcmLeftChannel[index],checkValue,leftValid);
			//break;
		}	
	}

	if(leftValid >20)
		leftValid = 0x01;
	else
		leftValid = 0;
	checkValue = *pcmRightChannel;

		//then check right 
	for(index = 0; index < len; index += 2)
	{
		
		if((pcmRightChannel[index] >= checkValue+50)||(pcmRightChannel[index] <= checkValue-50))
		{
			rightValid++;//= 0x02;
			//printf("-->pcmRightChannel[%d] = %d checkValue %d rightValid %d",index,pcmRightChannel[index],checkValue,rightValid);
			//break;
		}	
	}

	if(rightValid >20)
		rightValid = 0x02;
	else
		rightValid = 0;
	printf("leftValid %d rightValid %d \n",leftValid,rightValid);
	return leftValid|rightValid;
}

void channel_fixed(void * data, unsigned len, int chFlag)
{
	//we just fixed when chFlag is 1 or 2.
	if(chFlag <= 0 || chFlag > 2 )
		return;

	short * pcmValid = (short *)data;
	short * pcmInvalid = pcmValid;
	
	if(chFlag == 1)
		pcmInvalid += 1;
	else if (chFlag == 2)
		pcmValid += 1;
	
	unsigned index ;
	
	for(index = 0; index < len; index += 2)
	{
		pcmInvalid[index] = pcmValid[index];
	}
	return;
}

static int cont = 0;
int pcm_read(struct pcm *pcm, void *data, unsigned count)
{
    struct snd_xferi x;
    int ret = 0;
	char *recdata = NULL;
	int xy = 0;

    if (!(pcm->flags & PCM_IN))
        return -EINVAL;

    x.buf = data;
    x.frames = (pcm->flags & PCM_MONO) ? (count / 2) : (count / 4);

//    printf("read() %d frames", x.frames);
    for (;;) {
        if (!pcm->running) {
            if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_PREPARE))
                return oops(pcm, errno, "cannot prepare channel");
            if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_START))
                return oops(pcm, errno, "cannot start channel");
            pcm->running = 1;
        }
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_READI_FRAMES, &x)) {
//#ifdef SUPPORT_USB
            if (((pcm->flags & PCM_CARD_MASK) >> PCM_CARD_SHIFT) == PCM_CARD2) {
                //usb sound card out, so set data to 0, and sleep for data
                int need_usleep_time;
                unsigned int usleep_time = 0;
                unsigned int frames = (pcm->flags & PCM_MONO) ? (count / 2) : (count / 4);

                if ((pcm->flags & PCM_RATE_MASK) == PCM_8000HZ)
                    usleep_time = frames * 1000 * 1000 / 8000;
                else if ((pcm->flags & PCM_RATE_MASK) == PCM_48000HZ)
                    usleep_time = frames * 1000 * 1000 / 48000;
                else
                    usleep_time = frames * 1000 * 1000 / 44100;

                memset(data, 0, count);

                if (last_read_time == 0)
                    last_read_time = systemTime();

                need_usleep_time = usleep_time - (systemTime() - last_read_time) / 1000;
                if (need_usleep_time > 0)
                    usleep(need_usleep_time);

                last_read_time = systemTime();

                return 0;
            }
//#endif
            pcm->running = 0;
            if (errno == EPIPE) {
                    /* we failed to make our window -- try to restart */
                pcm->underruns++;
                continue;
            }
            return oops(pcm, errno, "cannot read stream data");
        }
        last_read_time = systemTime();
//        printf("read() got %d frames", x.frames);
		if(!(pcm->flags & PCM_MONO))
		{
				//LOGI("read() get %d", x.frames);
			if(channalFlags == -1 )	
			{
				if(startCheckCount < SAMPLECOUNT)
				{
					startCheckCount += count;
				}
				else
				{
					channalFlags = channel_check(data,count/2);
				}
			}//if(channalFlags == -1)

			channel_fixed(data,count/2, channalFlags);
		}

		if (0){
			cont ++;
			recdata = data;
			for (;xy <= 1000;xy++){
				if ((xy%22) == 21)
					printf("\n");
					printf("0x%02x ",*recdata++);
			}
		}
        return 0;
    }
}



static struct pcm bad_pcm = {
    .fd = -1,
};

int pcm_close(struct pcm *pcm)
{
    printf("pcm_close() card %u, device %u, %s \n",
        (pcm->flags & PCM_CARD_MASK) >> PCM_CARD_SHIFT,
        (pcm->flags & PCM_DEVICE_MASK) >> PCM_DEVICE_SHIFT,
        (pcm->flags & PCM_IN) ? "Capture" : "Playback");

    if (pcm == &bad_pcm)
        return 0;

    if (pcm->fd >= 0)
        close(pcm->fd);
    pcm->running = 0;
    pcm->buffer_size = 0;
    pcm->fd = -1;
    free(pcm);
    return 0;
}

struct pcm *pcm_open(unsigned flags)
{
    const char *dfmt = "/dev/snd/pcmC%uD%u%c";
    char dname[sizeof(dfmt) + 20];
    struct pcm *pcm;
    struct snd_pcm_info info;
    struct snd_pcm_hw_params params;
    struct snd_pcm_sw_params sparams;
    unsigned card;
    unsigned device;
    unsigned period_sz;
    unsigned period_cnt;

    printf("pcm_open(0x%08x) \n", flags);

    pcm = calloc(1, sizeof(struct pcm));
    if (!pcm)
        return &bad_pcm;

__open_again:

    card = (flags & PCM_CARD_MASK) >> PCM_CARD_SHIFT;
    device = (flags & PCM_DEVICE_MASK) >> PCM_DEVICE_SHIFT;

    sprintf(dname, dfmt, card, device, flags & PCM_IN ? 'c' : 'p');

    printf("pcm_open(%s) card %u, device %u, %s \n", dname,
        card, device, (flags & PCM_IN) ? "Capture" : "Playback");

    pcm->flags = flags;
    pcm->fd = open(dname, O_RDWR|O_CLOEXEC);
    if (pcm->fd < 0) {
        oops(pcm, errno, "cannot open device '%s'", dname);
        if ((flags & PCM_CARD_MASK) == PCM_CARD1) {
            printf("Open sound card1 for HDMI error, open sound card0");
            flags &= ~PCM_CARD_MASK;
            goto __open_again;
        }
        return pcm;
    }

    while(pcm->fd == 0 || pcm->fd == 1 || pcm->fd == 2)
    {
        printf("pcm_open old_fd=%d \n",pcm->fd);
        int tmp_fd = pcm->fd;
        pcm->fd = dup(tmp_fd);
        close(tmp_fd);
        printf("pcm_open new_fd=%d \n",pcm->fd);
    }

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_INFO, &info)) {
        oops(pcm, errno, "cannot get info - %s", dname);
        goto fail;
    }
    info_dump(&info);

    printf("pcm_open() period sz multiplier %d \n",
         ((flags & PCM_PERIOD_SZ_MASK) >> PCM_PERIOD_SZ_SHIFT) + 1);
    period_sz = PCM_PERIOD_SZ_MIN * (((flags & PCM_PERIOD_SZ_MASK) >> PCM_PERIOD_SZ_SHIFT) + 1);
    printf("pcm_open() period cnt %d \n",
         ((flags & PCM_PERIOD_CNT_MASK) >> PCM_PERIOD_CNT_SHIFT) + PCM_PERIOD_CNT_MIN);
    period_cnt = ((flags & PCM_PERIOD_CNT_MASK) >> PCM_PERIOD_CNT_SHIFT) + PCM_PERIOD_CNT_MIN;

    printf("pcm_open() period_cnt %d period_sz %d channels %d \n",
         period_cnt, period_sz, (flags & PCM_MONO) ? 1 : 2);

    param_init(&params);
    param_set_mask(&params, SNDRV_PCM_HW_PARAM_ACCESS,
                   SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    param_set_mask(&params, SNDRV_PCM_HW_PARAM_FORMAT,
                   SNDRV_PCM_FORMAT_S16_LE);
    param_set_mask(&params, SNDRV_PCM_HW_PARAM_SUBFORMAT,
                   SNDRV_PCM_SUBFORMAT_STD);
	
    param_set_min(&params, SNDRV_PCM_HW_PARAM_PERIOD_SIZE, period_sz);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 16);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_FRAME_BITS,
                  (flags & PCM_MONO) ? 16 : 32);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_CHANNELS,
                  (flags & PCM_MONO) ? 1 : 2);
    param_set_int(&params, SNDRV_PCM_HW_PARAM_PERIODS, period_cnt);
    if ((flags & PCM_RATE_MASK) == PCM_8000HZ) {
        printf("set audio rate 8KHz \n");
        param_set_int(&params, SNDRV_PCM_HW_PARAM_RATE, 8000);
    } else if ((flags & PCM_RATE_MASK) == PCM_48000HZ) {
        printf("set audio rate 48KHz \n");
        param_set_int(&params, SNDRV_PCM_HW_PARAM_RATE, 48000);
    } else
        param_set_int(&params, SNDRV_PCM_HW_PARAM_RATE, 44100);

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_HW_PARAMS, &params)) {
        oops(pcm, errno, "cannot set hw params");
        goto fail;
    }
    param_dump(&params);

    memset(&sparams, 0, sizeof(sparams));
    sparams.tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
    sparams.period_step = 1;
    sparams.avail_min = 1;
    sparams.start_threshold = period_cnt * period_sz;
    sparams.stop_threshold = period_cnt * period_sz;
    sparams.xfer_align = period_sz / 2; /* needed for old kernels */
    sparams.silence_size = 0;
    sparams.silence_threshold = 0;

    if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sparams)) {
        oops(pcm, errno, "cannot set sw params");
        goto fail;
    }

    //Set prepare for device 1/2 of codec
    if (device != 0 && card == 0) {
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_PREPARE))
            printf("pcm_open() cannot set prepare for card %d, device %d \n", card, device);
    }

    pcm->buffer_size = period_cnt * period_sz;
    pcm->underruns = 0;
    return pcm;

fail:
    close(pcm->fd);
    pcm->fd = -1;
    return pcm;
}

int pcm_ready(struct pcm *pcm)
{
    return pcm->fd >= 0;
}
