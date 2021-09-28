#ifndef GEN8_RENDER_H
#define GEN8_RENDER_H

#include "gen7_render.h"

# define GEN8_WM_LEGACY_DIAMOND_LINE_RASTERIZATION	(1 << 26)

#define GEN8_3DSTATE_SCISSOR_STATE_POINTERS	GEN4_3D(3, 0, 0xf)
#define GEN8_3DSTATE_STENCIL_BUFFER		GEN4_3D(3, 0, 0x06)
#define GEN8_3DSTATE_HIER_DEPTH_BUFFER		GEN4_3D(3, 0, 0x07)
#define GEN8_3DSTATE_MULTISAMPLE		GEN4_3D(3, 0, 0x0d)
# define GEN8_3DSTATE_MULTISAMPLE_NUMSAMPLES_2			(1 << 1)

#define GEN8_3DSTATE_WM_HZ_OP			GEN4_3D(3, 0, 0x52)

#define GEN8_3DSTATE_VF_INSTANCING		GEN4_3D(3, 0, 0x49)
# define GEN8_SBE_FORCE_URB_ENTRY_READ_LENGTH	(1 << 29)
# define GEN8_SBE_FORCE_URB_ENTRY_READ_OFFSET	(1 << 28)
# define GEN8_SBE_URB_ENTRY_READ_OFFSET_SHIFT   5
#define GEN8_3DSTATE_SBE_SWIZ			GEN4_3D(3, 0, 0x51)
#define GEN8_3DSTATE_RASTER			GEN4_3D(3, 0, 0x50)
# define GEN8_RASTER_FRONT_WINDING_CCW			(1 << 21)
# define GEN8_RASTER_CULL_NONE                          (1 << 16)

# define GEN8_SF_POINT_WIDTH_FROM_SOURCE                (1 << 11)

# define GEN8_VS_FLOATING_POINT_MODE_ALTERNATE          (1 << 16)

#define GEN8_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP	\
						GEN4_3D(3, 0, 0x21)
#define GEN8_3DSTATE_PS_BLEND			GEN4_3D(3, 0, 0x4d)
# define GEN8_PS_BLEND_HAS_WRITEABLE_RT			(1 << 30)
#define GEN8_3DSTATE_WM_DEPTH_STENCIL		GEN4_3D(3, 0, 0x4e)
#define GEN8_3DSTATE_PS_EXTRA			GEN4_3D(3, 0, 0x4f)
# define GEN8_PSX_PIXEL_SHADER_VALID			(1 << 31)
# define GEN8_PSX_ATTRIBUTE_ENABLE			(1 << 8)

#define GEN8_3DSTATE_DS_STATE_POINTERS		GEN4_3D(3, 0, 0x25)

#define GEN8_3DSTATE_SAMPLER_STATE_POINTERS_HS	GEN4_3D(3, 0, 0x2c)
#define GEN8_3DSTATE_SAMPLER_STATE_POINTERS_DS	GEN4_3D(3, 0, 0x2d)

#define GEN8_3DSTATE_VF				GEN4_3D(3, 0, 0x0c)
#define GEN8_3DSTATE_VF_TOPOLOGY		GEN4_3D(3, 0, 0x4b)

#define GEN8_3DSTATE_BIND_TABLE_POOL_ALLOC	GEN4_3D(3, 1, 0x19)
#define GEN8_3DSTATE_GATHER_POOL_ALLOC		GEN4_3D(3, 1, 0x1a)
#define GEN8_3DSTATE_DX9_CONSTANT_BUFFER_POOL_ALLOC	GEN4_3D(3, 1, 0x1b)
#define GEN8_3DSTATE_PUSH_CONSTANT_ALLOC_HS	GEN4_3D(3, 1, 0x13)
#define GEN8_3DSTATE_PUSH_CONSTANT_ALLOC_DS	GEN4_3D(3, 1, 0x14)
#define GEN8_3DSTATE_PUSH_CONSTANT_ALLOC_GS	GEN4_3D(3, 1, 0x15)

#define GEN8_3DSTATE_VF_SGVS			GEN4_3D(3, 0, 0x4a)
#define GEN8_3DSTATE_SO_DECL_LIST		GEN4_3D(3, 1, 0x17)
#define GEN8_3DSTATE_SO_BUFFER			GEN4_3D(3, 1, 0x18)
#define GEN8_3DSTATE_SAMPLER_PALETTE_LOAD0	GEN4_3D(3, 1, 0x02)
#define GEN8_3DSTATE_SAMPLER_PALETTE_LOAD1	GEN4_3D(3, 1, 0x0c)

/* Some random bits that we care about */
#define GEN8_VB0_BUFFER_ADDR_MOD_EN		(1 << 14)
#define GEN8_3DSTATE_PS_PERSPECTIVE_PIXEL_BARYCENTRIC (1 << 11)
#define GEN8_3DSTATE_PS_ATTRIBUTE_ENABLED	 (1 << 10)

/* Random shifts */
#define GEN8_3DSTATE_PS_MAX_THREADS_SHIFT 23

/* STATE_BASE_ADDRESS state size in pages*/
#define GEN8_STATE_SIZE_PAGES(x) ((x) << 12)

#define BDW_MOCS_PTE		(0 << 5)
#define BDW_MOCS_UC		(1 << 5)
#define BDW_MOCS_WT		(2 << 5)
#define BDW_MOCS_WB		(3 << 5)
#define BDW_MOCS_TC_ELLC	(0 << 3)
#define BDW_MOCS_TC_LLC		(1 << 3)
#define BDW_MOCS_TC_LLC_ELLC	(2 << 3)
#define BDW_MOCS_TC_L3_PTE	(3 << 3)
#define BDW_MOCS_AGE(x)		((x) << 0)

#define CHV_MOCS_UC		(0 << 5)
#define CHV_MOCS_WB		(3 << 5)
#define CHV_MOCS_NO_CACHING	(0 << 3)
#define CHV_MOCS_L3		(3 << 3)

/* Shamelessly ripped from mesa */
struct gen8_surface_state
{
	struct {
		uint32_t cube_pos_z:1;
		uint32_t cube_neg_z:1;
		uint32_t cube_pos_y:1;
		uint32_t cube_neg_y:1;
		uint32_t cube_pos_x:1;
		uint32_t cube_neg_x:1;
		uint32_t media_boundary_pixel_mode:2;
		uint32_t render_cache_read_write:1;
		uint32_t smapler_l2_bypass:1;
		uint32_t vert_line_stride_ofs:1;
		uint32_t vert_line_stride:1;
		uint32_t tiled_mode:2;
		uint32_t horizontal_alignment:2;
		uint32_t vertical_alignment:2;
		uint32_t surface_format:9;     /**< BRW_SURFACEFORMAT_x */
		uint32_t pad0:1;
		uint32_t is_array:1;
		uint32_t surface_type:3;       /**< BRW_SURFACE_1D/2D/3D/CUBE */
	} ss0;

	struct {
		uint32_t qpitch:15;
		uint32_t pad1:4;
		uint32_t base_mip_level:5;
		uint32_t memory_object_control:7;
		uint32_t pad0:1;
	} ss1;

	struct {
		uint32_t width:14;
		uint32_t pad1:2;
		uint32_t height:14;
		uint32_t pad0:2;
	} ss2;

	struct {
		uint32_t pitch:18;
		uint32_t pad:3;
		uint32_t depth:11;
	} ss3;

	struct {
		uint32_t minimum_array_element:27;
		uint32_t pad0:5;
	} ss4;

	struct {
		uint32_t mip_count:4;
		uint32_t min_lod:4;
		uint32_t pad3:6;
		uint32_t coherency_type:1;
		uint32_t pad2:5;
		uint32_t ewa_disable_for_cube:1;
		uint32_t y_offset:3;
		uint32_t pad0:1;
		uint32_t x_offset:7;
	} ss5;

	struct {
		uint32_t aux_mode:3;
		uint32_t aux_pitch:9;
		uint32_t pad0:4;
		uint32_t aux_qpitch:15;
		uint32_t pad1:1;
	} ss6;

	struct {
		uint32_t resource_min_lod:12;

		/* Only on Haswell */
		uint32_t pad0:4;
		uint32_t shader_chanel_select_a:3;
		uint32_t shader_chanel_select_b:3;
		uint32_t shader_chanel_select_g:3;
		uint32_t shader_chanel_select_r:3;

		uint32_t alpha_clear_color:1;
		uint32_t blue_clear_color:1;
		uint32_t green_clear_color:1;
		uint32_t red_clear_color:1;
	} ss7;

	struct {
		uint32_t base_addr;
	} ss8;

	struct {
		uint32_t base_addr_hi;
	} ss9;

	struct {
		uint32_t aux_base_addr;
	} ss10;

	struct {
		uint32_t aux_base_addr_hi;
	} ss11;

	struct {
		uint32_t hiz_depth_clear_value;
	} ss12;

	struct {
		uint32_t reserved;
	} ss13;

	struct {
		uint32_t reserved;
	} ss14;

	struct {
		uint32_t reserved;
	} ss15;
};

struct gen8_sampler_state
{
	struct
	{
		uint32_t aniso_algorithm:1;
		uint32_t lod_bias:13;
		uint32_t min_filter:3;
		uint32_t mag_filter:3;
		uint32_t mip_filter:2;
		uint32_t base_level:5;
		uint32_t lod_preclamp:2;
		uint32_t default_color_mode:1;
		uint32_t pad0:1;
		uint32_t disable:1;
	} ss0;

	struct
	{
		uint32_t cube_control_mode:1;
		uint32_t shadow_function:3;
		uint32_t chromakey_mode:1;
		uint32_t chromakey_index:2;
		uint32_t chromakey_enable:1;
		uint32_t max_lod:12;
		uint32_t min_lod:12;
	} ss1;

	struct
	{
		uint32_t lod_clamp_mag_mode:1;
		uint32_t flexible_filter_valign:1;
		uint32_t flexible_filter_halign:1;
		uint32_t flexible_filter_coeff_size:1;
		uint32_t flexible_filter_mode:1;
		uint32_t pad1:1;
		uint32_t indirect_state_ptr:18;
		uint32_t pad0:2;
		uint32_t sep_filter_height:2;
		uint32_t sep_filter_width:2;
		uint32_t sep_filter_coeff_table_size:2;
	} ss2;

	struct
	{
		uint32_t r_wrap_mode:3;
		uint32_t t_wrap_mode:3;
		uint32_t s_wrap_mode:3;
		uint32_t pad:1;
		uint32_t non_normalized_coord:1;
		uint32_t trilinear_quality:2;
		uint32_t address_round:6;
		uint32_t max_aniso:3;
		uint32_t pad0:2;
		uint32_t non_sep_filter_footprint_mask:8;
	} ss3;
};

struct gen8_blend_state {
	struct {
		uint32_t pad0:19;
		uint32_t y_dither_offset:2;
		uint32_t x_dither_offset:2;
		uint32_t dither_enable:1;
		uint32_t alpha_test_func:3;
		uint32_t alpha_test:1;
		uint32_t alpha_to_coverage_dither:1;
		uint32_t alpha_to_one:1;
		uint32_t ia_blend:1;
		uint32_t alpha_to_coverage:1;
	} bs0;

	struct {
		uint32_t write_disable_blue:1;
		uint32_t write_disable_green:1;
		uint32_t write_disable_red:1;
		uint32_t write_disable_alpha:1;
		uint32_t pad1:1;
		uint32_t alpha_blend_func:3;
		uint32_t dest_alpha_blend_factor:5;
		uint32_t source_alpha_blend_factor:5;
		uint32_t color_blend_func:3;
		uint32_t dest_blend_factor:5;
		uint32_t source_blend_factor:5;
		uint32_t color_buffer_blend:1;
		uint32_t post_blend_color_clamp:1;
		uint32_t pre_blend_color_clamp:1;
		uint32_t color_clamp_range:2;
		uint32_t pre_blend_source_only_clamp:1;
		uint32_t pad0:22;
		uint32_t logic_op_func:4;
		uint32_t logic_op_enable:1;
	} bs[16];
};

struct gen7_sf_clip_viewport {
	struct {
		float m00;
		float m11;
		float m22;
		float m30;
		float m31;
		float m32;
	} viewport;

	uint32_t pad0[2];

	struct {
		float xmin;
		float xmax;
		float ymin;
		float ymax;
	} guardband;

	float pad1[4];
};

struct gen6_scissor_rect
{
	uint32_t xmin:16;
	uint32_t ymin:16;
	uint32_t xmax:16;
	uint32_t ymax:16;
};

#endif
