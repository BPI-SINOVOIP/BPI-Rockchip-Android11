#ifndef MY_VPX_DEFS_H_
#define MY_VPX_DEFS_H_


#define VPX_IMG_FMT_PLANAR 0x100       /**< Image is a planar format. */
#define VPX_IMG_FMT_UV_FLIP 0x200      /**< V plane precedes U in memory. */
#define VPX_IMG_FMT_HAS_ALPHA 0x400    /**< Image has an alpha channel. */
#define VPX_IMG_FMT_HIGHBITDEPTH 0x800 /**< Image uses 16bit framebuffer. */

typedef unsigned char uint8_t;

enum class RenderMode {
    RENDER_BY_HOST_GPU = 1,
    RENDER_BY_GUEST_CPU = 2,
};

enum vpx_img_fmt_t {
  VPX_IMG_FMT_NONE,
  VPX_IMG_FMT_YV12 =
      VPX_IMG_FMT_PLANAR | VPX_IMG_FMT_UV_FLIP | 1, /**< planar YVU */
  VPX_IMG_FMT_I420 = VPX_IMG_FMT_PLANAR | 2,
  VPX_IMG_FMT_I422 = VPX_IMG_FMT_PLANAR | 5,
  VPX_IMG_FMT_I444 = VPX_IMG_FMT_PLANAR | 6,
  VPX_IMG_FMT_I440 = VPX_IMG_FMT_PLANAR | 7,
  VPX_IMG_FMT_I42016 = VPX_IMG_FMT_I420 | VPX_IMG_FMT_HIGHBITDEPTH,
  VPX_IMG_FMT_I42216 = VPX_IMG_FMT_I422 | VPX_IMG_FMT_HIGHBITDEPTH,
  VPX_IMG_FMT_I44416 = VPX_IMG_FMT_I444 | VPX_IMG_FMT_HIGHBITDEPTH,
  VPX_IMG_FMT_I44016 = VPX_IMG_FMT_I440 | VPX_IMG_FMT_HIGHBITDEPTH
};
    
struct vpx_image_t {
    vpx_img_fmt_t fmt;       /**< Image Format */
    unsigned int d_w; /**< Displayed image width */
    unsigned int d_h; /**< Displayed image height */
    void *user_priv;
};

#define VPX_CODEC_OK 0

struct vpx_codec_ctx_t {
    int vpversion; //8: vp8 or 9: vp9
    int version;   // 100: return decoded frame to guest; 200: render on host
    int hostColorBufferId;
    uint64_t id;  // >= 1, unique
    int memory_slot;
    uint64_t address_offset = 0;
    size_t outputBufferWidth;
    size_t outputBufferHeight;
    size_t width;
    size_t height;
    size_t bpp;
    uint8_t *data;
    uint8_t *dst;
    vpx_image_t myImg;
};

int vpx_codec_destroy(vpx_codec_ctx_t*);
int vpx_codec_dec_init(vpx_codec_ctx_t*);
vpx_image_t* vpx_codec_get_frame(vpx_codec_ctx_t*, int hostColorBufferId = -1);
int vpx_codec_flush(vpx_codec_ctx_t *ctx);
int vpx_codec_decode(vpx_codec_ctx_t *ctx, const uint8_t *data,
                                 unsigned int data_sz, void *user_priv,
                                 long deadline);

#endif  // MY_VPX_DEFS_H_
