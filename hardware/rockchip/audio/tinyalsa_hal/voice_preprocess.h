#ifndef VOICE_PREPROCESS_H_
#define VOICE_PREPROCESS_H_


#ifdef __cplusplus
extern "C" {
#endif

typedef struct rk_process_api_ {
    int (*start)();
    int (*quueCaputureBuffer)(void *buf, int size);
    int (*queuePlaybackBuffer)(void *buf, int size);
    int (*getCapureBuffer)(void *buf, int size);
    int (*getPlaybackBuffer)(void *buf, int size);
    int (*flush)();
} rk_process_api;

rk_process_api* rk_voiceprocess_create(int ply_sr, int ply_ch, int cap_sr, int cap_ch);
int rk_voiceprocess_destory();

#ifdef __cplusplus
}
#endif

#endif

