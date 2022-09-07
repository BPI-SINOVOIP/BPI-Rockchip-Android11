#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/types.h>

#include "codec_test.h"
#include "../test_case.h"
#include "alsa_audio.h"
#include "../Language/language.h"
#include "../common.h"
#include "../extra-functions.h"

#define AUDIO_HW_OUT_PERIOD_MULT 8 // (8 * 128 = 1024 frames)
#define AUDIO_HW_OUT_PERIOD_CNT 4
#define FILE_PATH "/res/codectest.pcm"
#define REC_DUR 3 //the unit is second
#define RECORD_FILE_PATH "/res/record.pcm"
static FILE * alsa_in_fp = NULL;


pthread_t rec_disp_tid;  
int codec_err = -1;
extern pid_t g_codec_pid;

static int maxRecPcm = 0;
static int maxRecPcmPeriod = 0;
static int nTime = 0;
static struct testcase_info  *tc_info = NULL;
static void calcAndDispRecAudioStrenth(short *pcm, int len)
{	
	short i, data;
	
	// calc mac rec value
	for(i = 0; i < len/2; i++) {
		data = abs(*pcm++);
		if(maxRecPcmPeriod < data) {
			maxRecPcmPeriod = data;
		}
	}
	
	//printf("---- maxRecPcmPeriod = %d\n", maxRecPcmPeriod);
	if(nTime++ >= 10) {
		nTime = 0;
		maxRecPcm = maxRecPcmPeriod;
		maxRecPcmPeriod = 0;
	}
}




// 边录边放模式,在使用耳机时选择此模式
void* rec_play_test_1(void *argv)
{		
	struct pcm* pcmIn;
	struct pcm* pcmOut;
	unsigned bufsize;
	char *data;
	FILE *fp = NULL;
	char recData[4*REC_DUR*44100];	
	unsigned inFlags = PCM_IN;
	unsigned outFlags = PCM_OUT;
	unsigned flags =  PCM_STEREO;

	struct mixer*   mMixer = NULL;
	struct mixer_ctl* mRouteCtl = NULL;
	char mString[10] = "";
	static FILE * HWFile;
	int headsetState = 0;
	unsigned isNeedChangeRate = 0;

	char *recdata = NULL;
	int xy = 0;
	//printf("<xiaoyao>\n");

	route_init();

	//read HP state
	HWFile = fopen("/sys/class/switch/h2w/state", "rt");
	if (!HWFile) {
		fprintf(stderr, "Open headset state error!\n");
	} else {
		fread(mString, sizeof(char), sizeof(mString), HWFile);
		fclose(HWFile);
		headsetState = mString[0] - '0';
		fprintf(stderr, "headsetState %d\n", headsetState);
	}
	printf("headsetState:%d.\n",headsetState);
	
	//read HDMI state
	HWFile = fopen("/sys/class/switch/hdmi/state", "rt");
	if (!HWFile) {
		fprintf(stderr, "Open hdmi state error!\n");
	} else {
		fread(mString, sizeof(char), sizeof(mString), HWFile);
		fclose(HWFile);
		if (mString[0] - '0' == 1) {
			fprintf(stderr, "HDMI is in\n");
			if (isNeedChangeRate)
				inFlags = (~PCM_RATE_MASK & inFlags) | PCM_8000HZ;
			outFlags = (~PCM_CARD_MASK & outFlags) | PCM_CARD1;
		} else {
			fprintf(stderr, "HDMI is out\n");
			if (isNeedChangeRate)
				inFlags = (~PCM_RATE_MASK & inFlags) | PCM_44100HZ;
			outFlags = (~PCM_CARD_MASK & outFlags) | PCM_CARD0;
		}
	}

	flags |= (AUDIO_HW_OUT_PERIOD_MULT - 1) << PCM_PERIOD_SZ_SHIFT;
	flags |= (AUDIO_HW_OUT_PERIOD_CNT - PCM_PERIOD_CNT_MIN)<< PCM_PERIOD_CNT_SHIFT;

	inFlags |= flags;
	outFlags |= flags;


	//0x10027000  0x1003f000
	inFlags = 0x1003f000;
	pcmIn = pcm_open(inFlags);
	if (!pcm_ready(pcmIn)) {
	pcm_close(pcmIn);
	goto fail;
	}

	//0x00027000  0x0001f000
	outFlags = 0x0001f000;	
	pcmOut = pcm_open(outFlags);
	if (!pcm_ready(pcmOut)) {
	pcm_close(pcmOut);
	goto fail;
	}

	//open route
    if (mMixerPlayback == NULL)
        mMixerPlayback = mixer_open(0);

    if (mMixerCapture == NULL)
        mMixerCapture = mixer_open(0);

 	route_set_controls(HEADSET_NORMAL_ROUTE);
	fp = fopen(FILE_PATH,"rb");
	if(NULL == fp)
	{
		fprintf(stderr,"could not open %s file, will go to fail\n", FILE_PATH);
		goto fail;
	}

	bufsize = pcm_buffer_size(pcmOut);
	usleep(10000);
	fseek(fp,0,SEEK_SET);
	
	while(bufsize == fread(recData,1,bufsize,fp)){
		if (pcm_write(pcmOut, recData, bufsize)) {
			fprintf(stderr,"the pcmOut could not write %d bytes file data,will go to fail\n", bufsize);
			goto fail;
		}
	}

	bufsize = pcm_buffer_size(pcmIn);
	data = malloc(bufsize);
	if (!data) {
		fprintf(stderr,"could not allocate %d bytes\n", bufsize);
		return 0;
	}

	//route_set_controls(MAIN_MIC_CAPTURE_ROUTE);
	route_set_controls(HANDS_FREE_MIC_CAPTURE_ROUTE);
	printf("###############################################\n");
	while (!pcm_read(pcmIn, data, bufsize)) {

		route_set_controls(HEADSET_NORMAL_ROUTE);
		calcAndDispRecAudioStrenth(data, bufsize);
		if (pcm_write(pcmOut, data, bufsize)) {
			fprintf(stderr,"could not write %d bytes\n", bufsize);
		        free(data);
			return 0;
		}
		
	}
	fail:
	//close route
	if (mMixer) {
		mRouteCtl = mixer_get_control(mMixer, "Playback Path", 0);
		if (mRouteCtl) {
			mixer_ctl_select(mRouteCtl, "OFF");
		}

		mRouteCtl = mixer_get_control(mMixer, "Capture MIC Path", 0);
		if (mRouteCtl) {
			mixer_ctl_select(mRouteCtl, "MIC OFF");
		}
	}
	mixer_close(mMixer);
	mMixer = NULL;
	pcm_close(pcmIn);
	pcm_close(pcmOut);
	return 0;
}



// 边录边放模式,使用喇叭时选择此模式
void* rec_play_test_2(void *argv)
{		
	struct pcm* pcmIn;
	struct pcm* pcmOut;
	unsigned bufsize;
	char *data;
	FILE *fp = NULL;
	FILE *outfp = NULL;
	char recData[4*REC_DUR*44100];	
	unsigned inFlags = PCM_IN;
	unsigned outFlags = PCM_OUT;
	unsigned flags =  PCM_STEREO;

	struct mixer*   mMixer = NULL;
	struct mixer_ctl* mRouteCtl = NULL;
	char mString[10] = "";
	static FILE * HWFile;
	int headsetState = 0;
	unsigned isNeedChangeRate = 0;
	static int count = 500;

	char *recdata = NULL;
	int xy = 0;
	
	//printf("<xiaoyao>\n");

	route_init();

	//read HP state
	HWFile = fopen("/sys/class/switch/h2w/state", "rt");
	if (!HWFile) {
		fprintf(stderr, "Open headset state error!\n");
	} else {
		fread(mString, sizeof(char), sizeof(mString), HWFile);
		fclose(HWFile);
		headsetState = mString[0] - '0';
		fprintf(stderr, "headsetState %d\n", headsetState);
	}
	printf("headsetState:%d.\n",headsetState);
	
	//read HDMI state
	HWFile = fopen("/sys/class/switch/hdmi/state", "rt");
	if (!HWFile) {
		fprintf(stderr, "Open hdmi state error!\n");
	} else {
		fread(mString, sizeof(char), sizeof(mString), HWFile);
		fclose(HWFile);
		if (mString[0] - '0' == 1) {
			fprintf(stderr, "HDMI is in\n");
			if (isNeedChangeRate)
				inFlags = (~PCM_RATE_MASK & inFlags) | PCM_8000HZ;
			outFlags = (~PCM_CARD_MASK & outFlags) | PCM_CARD1;
		} else {
			fprintf(stderr, "HDMI is out\n");
			if (isNeedChangeRate)
				inFlags = (~PCM_RATE_MASK & inFlags) | PCM_44100HZ;
			outFlags = (~PCM_CARD_MASK & outFlags) | PCM_CARD0;
		}
	}

	flags |= (AUDIO_HW_OUT_PERIOD_MULT - 1) << PCM_PERIOD_SZ_SHIFT;
	flags |= (AUDIO_HW_OUT_PERIOD_CNT - PCM_PERIOD_CNT_MIN)<< PCM_PERIOD_CNT_SHIFT;

	inFlags |= flags;
	outFlags |= flags;


	//0x10027000  0x1003f000
	inFlags = 0x1003f000;
	pcmIn = pcm_open(inFlags);
	if (!pcm_ready(pcmIn)) {
	pcm_close(pcmIn);
	goto fail;
	}

	//0x00027000  0x0001f000
	outFlags = 0x0001f000;	
	pcmOut = pcm_open(outFlags);
	if (!pcm_ready(pcmOut)) {
	pcm_close(pcmOut);
	goto fail;
	}

	//open route
    if (mMixerPlayback == NULL)
        mMixerPlayback = mixer_open(0);

    if (mMixerCapture == NULL)
        mMixerCapture = mixer_open(0);

	route_set_controls(SPEAKER_NORMAL_ROUTE);
	route_set_controls(MAIN_MIC_CAPTURE_ROUTE);
#if 1
	fp = fopen(FILE_PATH,"rb");
	if(NULL == fp)
	{
		fprintf(stderr,"could not open %s file, will go to fail\n", FILE_PATH);
		goto fail;
	}

	bufsize = pcm_buffer_size(pcmOut);
	usleep(10000);
	fseek(fp,0,SEEK_SET);
	
	while(bufsize == fread(recData,1,bufsize,fp)){
		if (pcm_write(pcmOut, recData, bufsize)) {
			fprintf(stderr,"the pcmOut could not write %d bytes file data,will go to fail\n", bufsize);
			goto fail;
		}
	}
#endif
	bufsize = pcm_buffer_size(pcmIn);
	data = malloc(bufsize);
	if (!data) {
		fprintf(stderr,"could not allocate %d bytes\n", bufsize);
		return 0;
	}
	alsa_in_fp = fopen(RECORD_FILE_PATH,"wrb");
	if(NULL == alsa_in_fp)
	{
		fprintf(stderr,"could not open %s file, will go to fail\n", RECORD_FILE_PATH);
		goto fail;
	}
	while (!pcm_read(pcmIn, data, bufsize) && count > 0) {	
			calcAndDispRecAudioStrenth(data, bufsize);
			fwrite(data,1,bufsize,alsa_in_fp);
			count--;
			fprintf(stderr,"read data\n");

	}
	fprintf(stderr,"read data ok\n");
	fclose(alsa_in_fp);
	
	usleep(100000);
	outfp = fopen(RECORD_FILE_PATH,"rb");
	if(NULL == outfp)
	{
		fprintf(stderr,"could not open %s file, will go to fail\n", RECORD_FILE_PATH);
		goto fail;
	}

	int freadsize = fread(data, 1, bufsize, outfp);
	fprintf(stderr,"freadsize = %d; bufsize = %d\n", freadsize, bufsize);

	while(bufsize == fread(data,1,bufsize,outfp)){
		fprintf(stderr,"write data\n");
			if (pcm_write(pcmOut, data, bufsize)) {
					fprintf(stderr,"the pcmOut could not write %d bytes file data,will go to fail\n", bufsize);
					free(data);

			}
	}
	return 0;		
	//}
	fail:
	//close route
	if (mMixer) {
		mRouteCtl = mixer_get_control(mMixer, "Playback Path", 0);
		if (mRouteCtl) {
			mixer_ctl_select(mRouteCtl, "OFF");
		}

		mRouteCtl = mixer_get_control(mMixer, "Capture MIC Path", 0);
		if (mRouteCtl) {
			mixer_ctl_select(mRouteCtl, "MIC OFF");
		}
	}
	mixer_close(mMixer);
	mMixer = NULL;
	pcm_close(pcmIn);
	pcm_close(pcmOut);
	return 0;
}

void rec_volum_display(void)
{
	int volume;
	int y_offset = tc_info->y;
	
	printf("enter rec_volum_display thread.\n");
	while(1) {
		usleep(300000);
		volume = 20 + ((maxRecPcm*100)/32768);
		if(volume > 100) volume = 100;
		//ui_print_xy_rgba(0,y_offset,0,255,0,255,"%s:[%s:%d%%]\n",PCBA_RECORD,PCBA_VOLUME,volume);
		ui_display_sync(0,y_offset,0,255,0,255,"%s:[%s:%d%%]\n",PCBA_RECORD,PCBA_VOLUME,volume);
		//printf("---- display maxRecPcm = %d\n", maxRecPcm);
	}
}

void* codec_test(void *argv)
{
    int ret = -1;
    char dt[32] = {0};

	tc_info = (struct testcase_info *)argv;
			
	if(tc_info->y <= 0)
		tc_info->y  = get_cur_print_y();	

	ui_print_xy_rgba(0,tc_info->y,255,255,0,255,"%s \n",PCBA_RECORD);
	sleep(3);    
	
	if(script_fetch("Codec", "program", (int *)dt, 8) == 0) {
		printf("script_fetch program = %s.\n", dt);
	}	
	
	codec_err = pthread_create(&rec_disp_tid, NULL, rec_volum_display,NULL); //
	if(codec_err != 0)
	{  
	   printf("create rec_volum_display thread error: %s/n",strerror(codec_err));  
	} 

	{
	    printf ("\r\nBEGIN CODEC TEST ---------------- \r\n");
	    if(strcmp(dt, "case2") == 0) {
	    	rec_play_test_2(NULL);
		} else {
			rec_play_test_1(NULL);
		}
	    printf ("\r\nEND CODEC TEST\r\n");
	}
	//printf("pcba-test codec pid %d\n",g_codec_pid);
	
    return NULL;
}
