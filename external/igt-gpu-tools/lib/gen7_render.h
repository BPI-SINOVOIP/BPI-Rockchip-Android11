#ifndef GEN7_RENDER_H
#define GEN7_RENDER_H

#include <stdint.h>
#include "surfaceformat.h"
#include "gen6_render.h"

#define INTEL_MASK(high, low) (((1 << ((high) - (low) + 1)) - 1) << (low))


/* GEN7_3DSTATE_WM */
/* DW1 */
# define GEN7_WM_STATISTICS_ENABLE                              (1 << 31)
# define GEN7_WM_DEPTH_CLEAR                                    (1 << 30)
# define GEN7_WM_DISPATCH_ENABLE                                (1 << 29)
# define GEN7_WM_DEPTH_RESOLVE                                  (1 << 28)
# define GEN7_WM_HIERARCHICAL_DEPTH_RESOLVE                     (1 << 27)
# define GEN7_WM_KILL_ENABLE                                    (1 << 25)
# define GEN7_WM_PSCDEPTH_OFF                                   (0 << 23)
# define GEN7_WM_PSCDEPTH_ON                                    (1 << 23)
# define GEN7_WM_PSCDEPTH_ON_GE                                 (2 << 23)
# define GEN7_WM_PSCDEPTH_ON_LE                                 (3 << 23)
# define GEN7_WM_USES_SOURCE_DEPTH                              (1 << 20)
# define GEN7_WM_USES_SOURCE_W                                  (1 << 19)
# define GEN7_WM_POSITION_ZW_PIXEL                              (0 << 17)
# define GEN7_WM_POSITION_ZW_CENTROID                           (2 << 17)
# define GEN7_WM_POSITION_ZW_SAMPLE                             (3 << 17)
# define GEN7_WM_NONPERSPECTIVE_SAMPLE_BARYCENTRIC              (1 << 16)
# define GEN7_WM_NONPERSPECTIVE_CENTROID_BARYCENTRIC            (1 << 15)
# define GEN7_WM_NONPERSPECTIVE_PIXEL_BARYCENTRIC               (1 << 14)
# define GEN7_WM_PERSPECTIVE_SAMPLE_BARYCENTRIC                 (1 << 13)
# define GEN7_WM_PERSPECTIVE_CENTROID_BARYCENTRIC               (1 << 12)
# define GEN7_WM_PERSPECTIVE_PIXEL_BARYCENTRIC                  (1 << 11)
# define GEN7_WM_USES_INPUT_COVERAGE_MASK                       (1 << 10)
# define GEN7_WM_LINE_END_CAP_AA_WIDTH_0_5                      (0 << 8)
# define GEN7_WM_LINE_END_CAP_AA_WIDTH_1_0                      (1 << 8)
# define GEN7_WM_LINE_END_CAP_AA_WIDTH_2_0                      (2 << 8)
# define GEN7_WM_LINE_END_CAP_AA_WIDTH_4_0                      (3 << 8)
# define GEN7_WM_LINE_AA_WIDTH_0_5                              (0 << 6)
# define GEN7_WM_LINE_AA_WIDTH_1_0                              (1 << 6)
# define GEN7_WM_LINE_AA_WIDTH_2_0                              (2 << 6)
# define GEN7_WM_LINE_AA_WIDTH_4_0                              (3 << 6)
# define GEN7_WM_POLYGON_STIPPLE_ENABLE                         (1 << 4)
# define GEN7_WM_LINE_STIPPLE_ENABLE                            (1 << 3)
# define GEN7_WM_POINT_RASTRULE_UPPER_RIGHT                     (1 << 2)
# define GEN7_WM_MSRAST_OFF_PIXEL                               (0 << 0)
# define GEN7_WM_MSRAST_OFF_PATTERN                             (1 << 0)
# define GEN7_WM_MSRAST_ON_PIXEL                                (2 << 0)
# define GEN7_WM_MSRAST_ON_PATTERN                              (3 << 0)
/* DW2 */
# define GEN7_WM_MSDISPMODE_PERPIXEL                            (1 << 31)

/* Surface state DW0 */
#define GEN7_SURFACE_RC_READ_WRITE	(1 << 8)
#define GEN7_SURFACE_TILED		(1 << 14)
#define GEN7_SURFACE_TILED_Y		(1 << 13)
#define GEN7_SURFACE_FORMAT_SHIFT	18
#define GEN7_SURFACE_TYPE_SHIFT		29

/* Surface state DW2 */
#define GEN7_SURFACE_HEIGHT_SHIFT        16
#define GEN7_SURFACE_WIDTH_SHIFT         0

/* Surface state DW3 */
#define GEN7_SURFACE_DEPTH_SHIFT         21
#define GEN7_SURFACE_PITCH_SHIFT         0

#define HSW_SWIZZLE_ZERO		0
#define HSW_SWIZZLE_ONE			1
#define HSW_SWIZZLE_RED			4
#define HSW_SWIZZLE_GREEN		5
#define HSW_SWIZZLE_BLUE		6
#define HSW_SWIZZLE_ALPHA		7
#define __HSW_SURFACE_SWIZZLE(r,g,b,a) \
	((a) << 16 | (b) << 19 | (g) << 22 | (r) << 25)
#define HSW_SURFACE_SWIZZLE(r,g,b,a) \
	__HSW_SURFACE_SWIZZLE(HSW_SWIZZLE_##r, HSW_SWIZZLE_##g, HSW_SWIZZLE_##b, HSW_SWIZZLE_##a)

/* _3DSTATE_VERTEX_BUFFERS on GEN7*/
/* DW1 */
#define GEN7_VB0_ADDRESS_MODIFYENABLE   (1 << 14)

/* _3DPRIMITIVE on GEN7 */
/* DW1 */
# define GEN7_3DPRIM_VERTEXBUFFER_ACCESS_SEQUENTIAL     (0 << 8)
# define GEN7_3DPRIM_VERTEXBUFFER_ACCESS_RANDOM         (1 << 8)

#define GEN7_3DSTATE_CLEAR_PARAMS               GEN4_3D(3, 0, 0x04)
#define GEN7_3DSTATE_DEPTH_BUFFER               GEN4_3D(3, 0, 0x05)
# define GEN7_3DSTATE_DEPTH_BUFFER_TYPE_SHIFT	29
# define GEN7_3DSTATE_DEPTH_BUFFER_FORMAT_SHIFT	18
/* DW1 */
# define GEN7_3DSTATE_DEPTH_CLEAR_VALID		(1 << 15)

#define GEN7_3DSTATE_CONSTANT_HS                GEN4_3D(3, 0, 0x19)
#define GEN7_3DSTATE_CONSTANT_DS                GEN4_3D(3, 0, 0x1a)

#define GEN7_3DSTATE_HS                         GEN4_3D(3, 0, 0x1b)
#define GEN7_3DSTATE_TE                         GEN4_3D(3, 0, 0x1c)
#define GEN7_3DSTATE_DS                         GEN4_3D(3, 0, 0x1d)
#define GEN7_3DSTATE_STREAMOUT                  GEN4_3D(3, 0, 0x1e)
#define GEN7_3DSTATE_SBE                        GEN4_3D(3, 0, 0x1f)

/* DW1 */
# define GEN7_SBE_SWIZZLE_CONTROL_MODE          (1 << 28)
# define GEN7_SBE_NUM_OUTPUTS_SHIFT             22
# define GEN7_SBE_SWIZZLE_ENABLE                (1 << 21)
# define GEN7_SBE_POINT_SPRITE_LOWERLEFT        (1 << 20)
# define GEN7_SBE_URB_ENTRY_READ_LENGTH_SHIFT   11
# define GEN7_SBE_URB_ENTRY_READ_OFFSET_SHIFT   4

#define GEN7_3DSTATE_PS                                 GEN4_3D(3, 0, 0x20)
/* DW1: kernel pointer */
/* DW2 */
# define GEN7_PS_SPF_MODE                               (1 << 31)
# define GEN7_PS_VECTOR_MASK_ENABLE                     (1 << 30)
# define GEN7_PS_SAMPLER_COUNT_SHIFT                    27
# define GEN7_PS_BINDING_TABLE_ENTRY_COUNT_SHIFT        18
# define GEN7_PS_FLOATING_POINT_MODE_IEEE_754           (0 << 16)
# define GEN7_PS_FLOATING_POINT_MODE_ALT                (1 << 16)
/* DW3: scratch space */
/* DW4 */
# define IVB_PS_MAX_THREADS_SHIFT                      24
# define HSW_PS_MAX_THREADS_SHIFT                      23
# define HSW_PS_SAMPLE_MASK_SHIFT                      12
# define GEN7_PS_PUSH_CONSTANT_ENABLE                   (1 << 11)
# define GEN7_PS_ATTRIBUTE_ENABLE                       (1 << 10)
# define GEN7_PS_OMASK_TO_RENDER_TARGET                 (1 << 9)
# define GEN7_PS_DUAL_SOURCE_BLEND_ENABLE               (1 << 7)
# define GEN7_PS_POSOFFSET_NONE                         (0 << 3)
# define GEN7_PS_POSOFFSET_CENTROID                     (2 << 3)
# define GEN7_PS_POSOFFSET_SAMPLE                       (3 << 3)
# define GEN7_PS_32_DISPATCH_ENABLE                     (1 << 2)
# define GEN7_PS_16_DISPATCH_ENABLE                     (1 << 1)
# define GEN7_PS_8_DISPATCH_ENABLE                      (1 << 0)
/* DW5 */
# define GEN7_PS_DISPATCH_START_GRF_SHIFT_0             16
# define GEN7_PS_DISPATCH_START_GRF_SHIFT_1             8
# define GEN7_PS_DISPATCH_START_GRF_SHIFT_2             0
/* DW6: kernel 1 pointer */
/* DW7: kernel 2 pointer */

#define GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_SF_CL      GEN4_3D(3, 0, 0x21)
#define GEN7_3DSTATE_VIEWPORT_STATE_POINTERS_CC         GEN4_3D(3, 0, 0x23)

#define GEN7_3DSTATE_BLEND_STATE_POINTERS               GEN4_3D(3, 0, 0x24)
#define GEN7_3DSTATE_DEPTH_STENCIL_STATE_POINTERS       GEN4_3D(3, 0, 0x25)

#define GEN7_3DSTATE_BINDING_TABLE_POINTERS_VS          GEN4_3D(3, 0, 0x26)
#define GEN7_3DSTATE_BINDING_TABLE_POINTERS_HS          GEN4_3D(3, 0, 0x27)
#define GEN7_3DSTATE_BINDING_TABLE_POINTERS_DS          GEN4_3D(3, 0, 0x28)
#define GEN7_3DSTATE_BINDING_TABLE_POINTERS_GS          GEN4_3D(3, 0, 0x29)
#define GEN7_3DSTATE_BINDING_TABLE_POINTERS_PS          GEN4_3D(3, 0, 0x2a)

#define GEN7_3DSTATE_SAMPLER_STATE_POINTERS_VS          GEN4_3D(3, 0, 0x2b)
#define GEN7_3DSTATE_SAMPLER_STATE_POINTERS_GS          GEN4_3D(3, 0, 0x2e)
#define GEN7_3DSTATE_SAMPLER_STATE_POINTERS_PS          GEN4_3D(3, 0, 0x2f)

#define GEN7_3DSTATE_URB_VS                             GEN4_3D(3, 0, 0x30)
#define GEN7_3DSTATE_URB_HS                             GEN4_3D(3, 0, 0x31)
#define GEN7_3DSTATE_URB_DS                             GEN4_3D(3, 0, 0x32)
#define GEN7_3DSTATE_URB_GS                             GEN4_3D(3, 0, 0x33)
/* DW1 */
# define GEN7_URB_ENTRY_NUMBER_SHIFT            0
# define GEN7_URB_ENTRY_SIZE_SHIFT              16
# define GEN7_URB_STARTING_ADDRESS_SHIFT        25

#define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_VS             GEN4_3D(3, 1, 0x12)
#define GEN7_3DSTATE_PUSH_CONSTANT_ALLOC_PS             GEN4_3D(3, 1, 0x16)
/* DW1 */
# define GEN7_PUSH_CONSTANT_BUFFER_OFFSET_SHIFT 16

/* for GEN7_STATE_BASE_ADDRESS */
#define BASE_ADDRESS_MODIFY		(1 << 0)

/* for GEN7_PIPE_CONTROL */
#define GEN7_PIPE_CONTROL_CS_STALL      (1 << 20)
#define GEN7_PIPE_CONTROL_STALL_AT_SCOREBOARD   (1 << 1)

/* VERTEX_BUFFER_STATE Structure */
#define GEN7_VB0_ADDRESS_MODIFY_ENABLE	(1 << 14)

#define GEN7_TD_CTL		       0x8000

/* Execution Unit (EU) defines */

#define GEN7_ARF_IP                    0xA0

#define VLV_MOCS_SNOOP		(2 << 1)
#define VLV_MOCS_L3		(1 << 0)

#define IVB_MOCS_GFDT		(1 << 2)
#define IVB_MOCS_PTE		(0 << 1)
#define IVB_MOCS_LLC		(1 << 1)
#define IVB_MOCS_L3		(1 << 0)

/* The hardware supports two different modes for border color. The
 * default (OpenGL) mode uses floating-point color channels, while the
 * legacy mode uses 4 bytes.
 *
 * More significantly, the legacy mode respects the components of the
 * border color for channels not present in the source, (whereas the
 * default mode will ignore the border color's alpha channel and use
 * alpha==1 for an RGB source, for example).
 *
 * The legacy mode matches the semantics specified by the Render
 * extension.
 */
struct gen7_surface_state {
	struct {
		unsigned int cube_pos_z:1;
		unsigned int cube_neg_z:1;
		unsigned int cube_pos_y:1;
		unsigned int cube_neg_y:1;
		unsigned int cube_pos_x:1;
		unsigned int cube_neg_x:1;
		unsigned int pad2:2;
		unsigned int render_cache_read_write:1;
		unsigned int pad1:1;
		unsigned int surface_array_spacing:1;
		unsigned int vert_line_stride_ofs:1;
		unsigned int vert_line_stride:1;
		unsigned int tile_walk:1;
		unsigned int tiled_surface:1;
		unsigned int horizontal_alignment:1;
		unsigned int vertical_alignment:2;
		unsigned int surface_format:9;     /**< BRW_SURFACEFORMAT_x */
		unsigned int pad0:1;
		unsigned int is_array:1;
		unsigned int surface_type:3;       /**< BRW_SURFACE_1D/2D/3D/CUBE */
	} ss0;

	struct {
		unsigned int base_addr;
	} ss1;

	struct {
		unsigned int width:14;
		unsigned int pad1:2;
		unsigned int height:14;
		unsigned int pad0:2;
	} ss2;

	struct {
		unsigned int pitch:18;
		unsigned int pad:3;
		unsigned int depth:11;
	} ss3;

	struct {
		unsigned int multisample_position_palette_index:3;
		unsigned int num_multisamples:3;
		unsigned int multisampled_surface_storage_format:1;
		unsigned int render_target_view_extent:11;
		unsigned int min_array_elt:11;
		unsigned int rotation:2;
		unsigned int pad0:1;
	} ss4;

	struct {
		unsigned int mip_count:4;
		unsigned int min_lod:4;
		unsigned int pad1:8;
		unsigned int memory_object_control:4;
		unsigned int y_offset:4;
		unsigned int pad0:1;
		unsigned int x_offset:7;
	} ss5;

	struct {
		unsigned int pad; /* Multisample Control Surface stuff */
	} ss6;

	struct {
		unsigned int resource_min_lod:12;
		unsigned int pad0:16;
		unsigned int alpha_clear_color:1;
		unsigned int blue_clear_color:1;
		unsigned int green_clear_color:1;
		unsigned int red_clear_color:1;
	} ss7;
};

struct gen7_sampler_state {
	struct {
		unsigned int aniso_algorithm:1;
		unsigned int lod_bias:13;
		unsigned int min_filter:3;
		unsigned int mag_filter:3;
		unsigned int mip_filter:2;
		unsigned int base_level:5;
		unsigned int pad1:1;
		unsigned int lod_preclamp:1;
		unsigned int default_color_mode:1;
		unsigned int pad0:1;
		unsigned int disable:1;
	} ss0;

	struct {
		unsigned int cube_control_mode:1;
		unsigned int shadow_function:3;
		unsigned int pad:4;
		unsigned int max_lod:12;
		unsigned int min_lod:12;
	} ss1;

	struct {
		unsigned int pad:5;
		unsigned int default_color_pointer:27;
	} ss2;

	struct {
		unsigned int r_wrap_mode:3;
		unsigned int t_wrap_mode:3;
		unsigned int s_wrap_mode:3;
		unsigned int pad:1;
		unsigned int non_normalized_coord:1;
		unsigned int trilinear_quality:2;
		unsigned int address_round:6;
		unsigned int max_aniso:3;
		unsigned int chroma_key_mode:1;
		unsigned int chroma_key_index:2;
		unsigned int chroma_key_enable:1;
		unsigned int pad0:6;
	} ss3;
};

#endif
