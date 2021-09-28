/******************************************************************************
 *
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *****************************************************************************
 * Originally developed and contributed by Ittiam Systems Pvt. Ltd, Bangalore
*/
/*!
******************************************************************************
* \file hme_interface.h
*
* \brief
*    Interfaces exported by ME to the world outside of ME
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_INTERFACE_H_
#define _HME_INTERFACE_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief      Maximum number of layers allowed
******************************************************************************
 */
#define MAX_NUM_LAYERS 4

/**
******************************************************************************
 *  @brief      layer max dimensions
******************************************************************************
 */
#define HME_MAX_WIDTH 1920
#define HME_MAX_HEIGHT 1088

/**
******************************************************************************
 *  @brief      layer min dimensions
******************************************************************************
 */
#define MIN_WD_COARSE 16
#define MIN_HT_COARSE 16

/**
******************************************************************************
 *  @brief     HME COARSE LAYER STEP SIZE
******************************************************************************
 */

#define HME_COARSE_STEP_SIZE_HIGH_SPEED 4
#define HME_COARSE_STEP_SIZE_HIGH_QUALITY 2

/**
******************************************************************************
 *  @brief      Memtabs required by layer ctxt: each layer ctxt requires 1
 *               memtab for itslf, 1 for mv bank, 1 for ref idx bank, one
 *               for input bufffer and 1 for storing segmentation info in
 *               worst case
******************************************************************************
 */
#define HME_MEMTABS_COARSE_LAYER_CTXT (5 * (MAX_NUM_LAYERS - 1) * (MAX_NUM_REF + 1))

/**
******************************************************************************
 *  @brief      Total number of memtabs reuqired by HME. Atleast 22 memtabs
 *              for different search results structure, 2*MAX_NUM_REF memtabs
 *              for search nodes maintaining coarse layer results in prev
 *              row, and for histograms. Memtabs reqd for layer,me ctxt
 *              ctb node mgr and buf mgr plus some 8 for safety
 *              if multi threaded then some memtabs will be more
******************************************************************************
 */
#define HME_COARSE_TOT_MEMTABS                                                                     \
    (22 + HME_MEMTABS_COARSE_LAYER_CTXT + (3 * MAX_NUM_REF) + 8 * MAX_NUM_FRM_PROC_THRDS_PRE_ENC + \
     1)

/**
******************************************************************************
 *  @brief      Memtabs required by layer ctxt (enc): each layer ctxt requires 1
 *               memtab for itslf, 1 for mv bank, 1 for ref idx bank, one
 *               for input bufffer and 1 for storing segmentation info in
 *               worst case
******************************************************************************
 */
#define MIN_HME_MEMTABS_ENC_LAYER_CTXT (5 * 1 * (MAX_NUM_REF + 1))

#define MAX_HME_MEMTABS_ENC_LAYER_CTXT (5 * 1 * (MAX_NUM_REF + 1 + MAX_NUM_ME_PARALLEL))

/**
******************************************************************************
 *  @brief      Total number of memtabs reuqired by HME. Atleast 22 memtabs
 *              for different search results structure, 2*MAX_NUM_REF memtabs
 *              for search nodes maintaining coarse layer results in prev
 *              row, and for histograms. Memtabs reqd for layer,me ctxt
 *              ctb node mgr and buf mgr plus some 8 for safety
 *              if multi threaded then some memtabs will be more
******************************************************************************
 */

#define MIN_HME_ENC_TOT_MEMTABS                                                                    \
    (22 + MIN_HME_MEMTABS_ENC_LAYER_CTXT + (3 * MAX_NUM_REF) + 28 * MAX_NUM_FRM_PROC_THRDS_ENC +   \
     2 /* Clustering */ + 1 /*traqo*/ + 1 /* ME Optimised Function List */)

#define MAX_HME_ENC_TOT_MEMTABS                                                                    \
    ((22 * MAX_NUM_ME_PARALLEL) + MAX_HME_MEMTABS_ENC_LAYER_CTXT +                                 \
     (3 * MAX_NUM_REF * MAX_NUM_ME_PARALLEL) +                                                     \
     28 * MAX_NUM_FRM_PROC_THRDS_ENC * MAX_NUM_ME_PARALLEL + 2 /* Clustering */ + 1 /*traqo*/ +    \
     1 /* ME Optimised Function List */)

/*****************************************************************************/
/* Enumerations                                                              */
/*****************************************************************************/
/**
******************************************************************************
 *  @enum     HME_MEM_ATTRS_T
 *  @brief      Contains type of memory: scratch, scratch ovly, persistent
******************************************************************************
 */
typedef enum
{
    HME_SCRATCH_MEM,
    HME_SCRATCH_OVLY_MEM,
    HME_PERSISTENT_MEM
} HME_MEM_ATTRS_T;

/**
******************************************************************************
 *  @enum     ME_QUALITY_PRESETS_T
 *  @brief    Describes the source for values in me_quality_params_t struct
******************************************************************************
 */
typedef enum
{
    ME_PRISTINE_QUALITY = 0,
    ME_HIGH_QUALITY = 2,
    ME_MEDIUM_SPEED,
    ME_HIGH_SPEED,
    ME_XTREME_SPEED,
    ME_XTREME_SPEED_25,
    ME_USER_DEFINED
} ME_QUALITY_PRESETS_T;

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/**
******************************************************************************
 *  @struct     hme_ref_buf_info_t
 *  @brief      Contains all required information of a ref picture
 *              Valid for a given layer.
******************************************************************************
 */
typedef struct
{
    /** Amt of padding in X direction both sides. */
    U08 u1_pad_x;

    /** Amt of padding in Y direction both sides */
    U08 u1_pad_y;

    /** Recon stride, in pixels */
    S32 luma_stride;

    /** Offset w.r.t. actual start of the buffer */
    S32 luma_offset;

    /** Src ptrs of the reference pictures*/
    U08 *pu1_ref_src;

    /** Reference ptrs for fpel plane, needed for this layer closed loop ME */
    U08 *pu1_rec_fxfy;

    /** Reference ptrs for hxfy plane (x = k+0.5, y = m) */
    U08 *pu1_rec_hxfy;

    /** Reference ptrs for fxhy plane (x = k, y = m + 0.5 */
    U08 *pu1_rec_fxhy;

    /** Reference ptrs for hxhy plane (x = k + 0.5, y = m + 0.5 */
    U08 *pu1_rec_hxhy;

    /** Reference ptr for u plane */
    U08 *pu1_rec_u;

    /** Reference ptr for v plane */
    U08 *pu1_rec_v;

    /** chroma plane stride in pixels */
    S32 chroma_stride;

    S32 chroma_offset;

    /** Pointer to dependency manager of recon buffer */
    void *pv_dep_mngr;

} hme_ref_buf_info_t;

/**
******************************************************************************
 *  @struct     interp_prms_t
 *  @brief      All parameters for the interpolation function
******************************************************************************
 */
typedef struct
{
    /** Array of ptr of 4 planes in order fxfy, hxfy, fxhy, hxhy */
    U08 **ppu1_ref;

    /**
     *  Array of pointers for ping-pong buffers, used to store interp out
     *  Output during a call goes to any one of these buffers
     */
    U08 *apu1_interp_out[5];

    /**
     *  Working memory to store 16 bit intermediate output. This has to be
     *  of size i4_blk_wd * (i4_blk_ht + 7) * 2
     */
    U08 *pu1_wkg_mem;

    /** Stride of all 4 planes of ref buffers */
    S32 i4_ref_stride;

    /** Width of interpolated output blk desired */
    S32 i4_blk_wd;

    /** Ht of interpolated output blk desired */
    S32 i4_blk_ht;

    /**
     *  Stride of interpolated output bufers,
     *  applicable for both ping and pong
     */
    S32 i4_out_stride;

    /** Final output pointer, which may be one of ping-pong or hpel planes */
    U08 *pu1_final_out;

    /** STride of the output bfufer */
    S32 i4_final_out_stride;

} interp_prms_t;

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/
typedef void (*PF_EXT_UPDATE_FXN_T)(void *, void *, S32, S32);

//typedef void (*PF_GET_INTRA_CU_AND_COST)(void *, S32, S32, S32 *, S32*, double *, S32);

typedef void (*PF_INTERP_FXN_T)(interp_prms_t *ps_prms, S32 i4_mv_x, S32 i4_mv_y, S32 interp_buf_id);

typedef void (*PF_SCALE_FXN_T)(
    U08 *pu1_src, S32 src_stride, U08 *pu1_dst, S32 dst_stride, S32 wd, S32 ht, U08 *pu1_wkg_mem);

/**
******************************************************************************
 *  @struct     hme_ref_desc_t
 *  @brief      Contains all reqd information for ref pics across all layers
 *              but for a given POC/ref id
******************************************************************************
 */
typedef struct
{
    /**
     *  Reference id in LC list. This is a unified list containing both fwd
     *  and backward direction references. Having a unified list just does
     *  a unique mapping of frames to ref id and eases out addressing in the
     *  ME search.
     */
    S08 i1_ref_id_lc;

    /**
     *  Reference id in L0 list. Priority is given to temporally fwd dirn
     *  unless of a scene change like case
     */
    S08 i1_ref_id_l0;

    /**
     *  Reference id in L1 list. Priority to backward dirn unless scene change
     *  like case
    */
    S08 i1_ref_id_l1;

    /** Whether this ref is temporally forward w.r.t. current pic */
    U08 u1_is_fwd;

    /** POC of this ref pic. */
    S32 i4_poc;

    /** display_num of this ref pic. */
    S32 i4_display_num;
    /**
     *  Lambda to be used for S + lambda*bits style cost computations when
     *  using this ref pic. This is a function of ref dist and hence diff
     *  ref has diff lambda
     */
    S32 lambda;

    /** Ref buffer info for all layers */
    hme_ref_buf_info_t as_ref_info[MAX_NUM_LAYERS];

    /** Weights and offset of reference picture
     * used for weighted pred analysis
     */
    S16 i2_weight;

    S16 i2_offset;

    /*
    * IDR GOP number
    */

    WORD32 i4_GOP_num;

} hme_ref_desc_t;

/**
******************************************************************************
 *  @struct     hme_ref_map_t
 *  @brief      Complete ref information across all layers and POCs
 *              Information valid for a given inp frame with a given POC.
******************************************************************************
 */
typedef struct
{
    /** Number of active ref picturs in LC list */
    S32 i4_num_ref;

    /** Recon Pic buffer pointers for L0 list */
    recon_pic_buf_t **pps_rec_list_l0;

    /** Recon Pic buffer pointers for L0 list */
    recon_pic_buf_t **pps_rec_list_l1;

    /** Reference descriptors for all ref pics */
    hme_ref_desc_t as_ref_desc[MAX_NUM_REF];

} hme_ref_map_t;

/**
 ******************************************************************************
 *  @struct me_coding_params_t
 *  @param e_me_quality_presets : Quality preset value
 *  @brief  ME Parameters that affect quality depending on their state
 ******************************************************************************
*/
typedef struct
{
    ME_QUALITY_PRESETS_T e_me_quality_presets;

    S32 i4_num_steps_hpel_refine;

    S32 i4_num_steps_qpel_refine;

    U08 u1_l0_me_controlled_via_cmd_line;

    U08 u1_num_results_per_part_in_l0me;

    U08 u1_num_results_per_part_in_l1me;

    U08 u1_num_results_per_part_in_l2me;

    U08 u1_max_num_coloc_cands;

    U08 u1_max_2nx2n_tu_recur_cands;

    U08 u1_max_num_fpel_refine_centers;

    U08 u1_max_num_subpel_refine_centers;
} me_coding_params_t;

/**
 ******************************************************************************
 *  @struct hme_init_prms_t
 *  @brief  Initialization parameters used during HME instance creation
 ******************************************************************************
*/
typedef struct
{
    /** Pointer to widths of various simulcast layers,
     * starting with biggest resolution
     */
    S32 a_wd[MAX_NUM_LAYERS];

    /** Pointer to heights of various simulcast layers,
     *  starting with biggest resolution
     */
    S32 a_ht[MAX_NUM_LAYERS];

    /** Maximum number of reference frames that a frame ever has to search */
    S32 max_num_ref;

    /** Number of results to be stored in the coarsest layer */
    S32 max_num_results_coarse;

    /**
     *  Number of layers for which explicit ME is to be done
     *  0 or MAX_NUM_LAYERS: encoder will do explicit ME for all layers
     *  anything in between, explicit ME done for that many layers
     */
    S32 num_layers_explicit_search;

    /** Number of simulcast layers to be encoded */
    S32 num_simulcast_layers;

    /** Maximum number of results per reference per partition */
    S32 max_num_results;

    /**
     *  If enabled, all layers store segmentation info at 16x16 lvl
     *  If not enabled, then only finest layer stores this info
     */
    S32 segment_higher_layers;

    /**
     *  If enabled, the non enocde layers use 8x8 blks with 4x4 partial
     *  sads also being evaluated, which is more powerful but computationally
     *  less efficient
     */
    S32 use_4x4;

    /**
     *  Number of B frames allowed between P frames
     */
    S32 num_b_frms;

    /** CTB Size as passed by encoder */
    S32 log_ctb_size;

    /** number of threads created run time */
    S32 i4_num_proc_thrds;

    /* This struct contains fields corresponding to quality knobs for ME */
    me_coding_params_t s_me_coding_tools;

    S32 max_vert_search_range;

    S32 max_horz_search_range;

    S32 is_interlaced;

    U08 u1_max_tr_depth;

    U08 u1_is_stasino_enabled;

    IV_ARCH_T e_arch_type;
} hme_init_prms_t;

/**
 ******************************************************************************
 *  @struct hme_frm_prms_t
 *  @brief  Frame level prms for HME execution
 ******************************************************************************
*/
typedef struct
{
    /** Range of the Motion vector in fpel units at finest layer x dirn */
    S16 i2_mv_range_x;

    /** range of motion vector in fpel units at finest layer y dirn */
    S16 i2_mv_range_y;

    /** Context for computing the cost function */
    void *pv_mv_cost_ctxt;

    /** Interpolation function pointers */
    PF_INTERP_FXN_T pf_interp_fxn;

    U08 is_i_pic;

    S32 bidir_enabled;

    S32 i4_temporal_layer_id;

    /**
      * Lambda values in Q format. 4 values exist: Closed loop SATD/SAD
      * and open loop SATD/SAD
      */
    S32 i4_cl_sad_lambda_qf;
    S32 i4_cl_satd_lambda_qf;
    S32 i4_ol_sad_lambda_qf;
    S32 i4_ol_satd_lambda_qf;

    /** Shift for lambda QFormat */
    S32 lambda_q_shift;

    S32 qstep;
    S32 qstep_ls8;
    S32 i4_frame_qp;
    S32 is_pic_second_field;

    /**
     * Number of active references in l0
     */
    U08 u1_num_active_ref_l0;

    /**
     * Number of active references in l1
     */
    U08 u1_num_active_ref_l1;

    /* Flag that specifies whether CU level QP */
    /* modulation is enabled */
    U08 u1_is_cu_qp_delta_enabled;

} hme_frm_prms_t;

/**
 ******************************************************************************
 *  @struct hme_memtab_t
 *  @brief  Structure to return memory requirements for one buffer.
 ******************************************************************************
*/
typedef struct
{
    /** Base of the memtab. Filled by application */
    U08 *pu1_mem;

    /** Required size of the memtab. Filed by module */
    S32 size;

    /** Alignment required */
    S32 align;

    /** type of memory */
    HME_MEM_ATTRS_T e_mem_attr;

} hme_memtab_t;

/**
 ******************************************************************************
 *  @struct hme_inp_buf_attr_t
 *  @brief  Attributes of input buffer and planes
 ******************************************************************************
*/
typedef struct
{
    /** Luma ptr 0, 0 position */
    U08 *pu1_y;

    /** Cb component or U component, 0, 0 position */
    U08 *pu1_u;

    /** Cr component or V component, 0, 0 position */
    U08 *pu1_v;

    /** Stride of luma component in pixels */
    S32 luma_stride;

    /** Stride of chroma component in pixels */
    S32 chroma_stride;
} hme_inp_buf_attr_t;

/**
 ******************************************************************************
 *  @struct hme_inp_desc_t
 *  @brief  Descriptor of a complete input frames (all simulcast layers incl)
 ******************************************************************************
*/
typedef struct
{
    /** input attributes for all simulcast layers */
    hme_inp_buf_attr_t s_layer_desc[MAX_NUM_LAYERS];

    /** POC of the current input frame */
    S32 i4_poc;

    /** idr GOP number*/
    S32 i4_idr_gop_num;

    /** is refence picture */
    S32 i4_is_reference;

} hme_inp_desc_t;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

/**
********************************************************************************
*  @fn     hme_enc_num_alloc()
*
*  @brief  returns number of memtabs that is required by hme module
*
*  @return   Number of memtabs required
********************************************************************************
*/
S32 hme_enc_num_alloc(WORD32 i4_num_me_frm_pllel);

/**
********************************************************************************
*  @fn     hme_coarse_num_alloc()
*
*  @brief  returns number of memtabs that is required by hme module
*
*  @return   Number of memtabs required
********************************************************************************
*/
S32 hme_coarse_num_alloc();

/**
********************************************************************************
*  @fn     hme_coarse_dep_mngr_num_alloc()
*
*  @brief  returns number of memtabs that is required by Dep Mngr for hme module
*
*  @return   Number of memtabs required
********************************************************************************
*/
WORD32 hme_coarse_dep_mngr_num_alloc();

/**
********************************************************************************
*  @fn     S32 hme_coarse_alloc(hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms)
*
*  @brief  Fills up memtabs with memory information details required by HME
*
*  @param[out] ps_memtabs : Pointre to an array of memtabs where module fills
*              up its requirements of memory
*
*  @param[in] ps_prms : Input parameters to module crucial in calculating reqd
*                       amt of memory
*
*  @return   Number of memtabs required
*******************************************************************************
*/
S32 hme_coarse_alloc(hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms);

/**
*******************************************************************************
*  @fn hme_coarse_dep_mngr_alloc
*
*  @brief  Fills up memtabs with memory information details required by Coarse HME
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
* \param[in] i4_mem_space : memspace in whihc memory request should be done
*
*  @return   Number of memtabs required
*******************************************************************************
*/
WORD32 hme_coarse_dep_mngr_alloc(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_mem_space,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_resolution_id);

/**
********************************************************************************
*  @fn     S32 hme_enc_alloc(hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms)
*
*  @brief  Fills up memtabs with memory information details required by HME
*
*  @param[out] ps_memtabs : Pointer to an array of memtabs where module fills
*              up its requirements of memory
*
*  @param[in] ps_prms : Input parameters to module crucial in calculating reqd
*                       amt of memory
*
*  @return   Number of memtabs required
*******************************************************************************
*/
S32 hme_enc_alloc(hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms, WORD32 i4_num_me_frm_pllel);

/**
********************************************************************************
*  @fn     S32 hme_enc_init(void *pv_ctxt,
*                       hme_memtab_t *ps_memtabs,
*                       hme_init_prms_t *ps_prms);
*
*  @brief  Initialization (one time) of HME
*
*  @param[in,out] pv_ctxt : Pointer to context of HME
*
*  @param[in] ps_memtabs : updated memtabs by application (allocated memory)
*
*  @param[in] ps_prms : Initialization parametres
*
*  @return   0 : success, -1 : failure
*******************************************************************************
*/
S32 hme_enc_init(
    void *pv_ctxt,
    hme_memtab_t *ps_memtabs,
    hme_init_prms_t *ps_prms,
    rc_quant_t *ps_rc_quant_ctxt,
    WORD32 i4_num_me_frm_pllel);

/**
********************************************************************************
*  @fn     S32 hme_coarse_init(void *pv_ctxt,
*                       hme_memtab_t *ps_memtabs,
*                       hme_init_prms_t *ps_prms);
*
*  @brief  Initialization (one time) of HME
*
*  @param[in,out] pv_ctxt : Pointer to context of HME
*
*  @param[in] ps_memtabs : updated memtabs by application (allocated memory)
*
*  @param[in] ps_prms : Initialization parametres
*
*  @return   0 : success, -1 : failure
*******************************************************************************
*/
S32 hme_coarse_init(void *pv_ctxt, hme_memtab_t *ps_memtabs, hme_init_prms_t *ps_prms);

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_get_lyr_prms_dep_mngr \endif
*
* \brief Returns to the caller key attributes relevant for dependency manager,
*        ie, the number of vertical units in each layer
*
* \par Description:
*    This function requires the precondition that the width and ht of encode
*    layer is known.
*    The number of layers, number of vertical units in each layer, and for
*    each vertial unit in each layer, its dependency on previous layer's units
*    From ME's perspective, a vertical unit is one which is smallest min size
*    vertically (and spans the entire row horizontally). This is CTB for encode
*    layer, and 8x8 / 4x4 for non encode layers.
*
* \param[in] num_layers : Number of ME Layers
* \param[in] pai4_ht    : Array storing ht at each layer
* \param[in] pai4_wd    : Array storing wd at each layer
* \param[out] pi4_num_vert_units_in_lyr : Array of size N (num layers), each
*                     entry has num vertical units in that particular layer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_get_lyr_prms_dep_mngr(
    WORD32 num_layers, WORD32 *pai4_ht, WORD32 *pai4_wd, WORD32 *pai4_num_vert_units_in_lyr);

/**
********************************************************************************
*  @fn     hme_coarse_dep_mngr_alloc_mem()
*
*  @brief  Requests/ assign memory for HME Dep Mngr
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
* \param[in] i4_mem_space : memspace in whihc memory request should be done
*
*  @return  number of memtabs
********************************************************************************
*/
WORD32 hme_coarse_dep_mngr_alloc_mem(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_mem_space,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_resolution_id);

/**
********************************************************************************
*  @fn     hme_coarse_dep_mngr_init()
*
*  @brief  Assign memory for HME Dep Mngr
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
*  @param[in] pv_ctxt : ME ctxt
* \param[in] pv_osal_handle : Osal handle
*
*  @return  number of memtabs
********************************************************************************
*/
WORD32 hme_coarse_dep_mngr_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    void *pv_ctxt,
    void *pv_osal_handle,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_resolution_id);

/**
********************************************************************************
*  @fn     hme_coarse_dep_mngr_reg_sem()
*
*  @brief  Assign semaphores for HME Dep Mngr
*
* \param[in] pv_me_ctxt : pointer to Coarse ME ctxt
* \param[in] ppv_sem_hdls : Arry of semaphore handles
* \param[in] i4_num_proc_thrds : Number of processing threads
*
*  @return  number of memtabs
********************************************************************************
*/
void hme_coarse_dep_mngr_reg_sem(void *pv_ctxt, void **ppv_sem_hdls, WORD32 i4_num_proc_thrds);

/**
********************************************************************************
*  @fn     hme_coarse_dep_mngr_delete()
*
*    Destroy Coarse ME Dep Mngr module
*   Note : Only Destroys the resources allocated in the module like
*   semaphore,etc. Memory free is done Separately using memtabs
*
* \param[in] pv_me_ctxt : pointer to Coarse ME ctxt
* \param[in] ps_init_prms : Create time static parameters
*
*  @return  none
********************************************************************************
*/
void hme_coarse_dep_mngr_delete(
    void *pv_me_ctxt, ihevce_static_cfg_params_t *ps_init_prms, WORD32 i4_resolution_id);

void hme_coarse_get_layer1_mv_bank_ref_idx_size(
    S32 n_tot_layers,
    S32 *a_wd,
    S32 *a_ht,
    S32 max_num_ref,
    S32 *pi4_mv_bank_size,
    S32 *pi4_ref_idx_size);

/**
********************************************************************************
*  @fn     S32 hme_add_inp(void *pv_ctxt,
*                       hme_inp_desc_t *ps_inp_desc);
*
*  @brief  Updates the HME context with details of the input buffers and POC.
*          Layers that are not encoded are processed further in terms of
*          pyramid generation.
*
*  @param[in,out] pv_ctxt : Pointer to context of HME
*
*  @param[in] ps_inp_desc : Input descriptor containing information of all
*             simulcast layers of input.
*
*  @return   void
*******************************************************************************
*/
void hme_add_inp(void *pv_ctxt, hme_inp_desc_t *ps_inp_desc, S32 me_frm_id, WORD32 thrd_id);

void hme_coarse_add_inp(void *pv_me_ctxt, hme_inp_desc_t *ps_inp_desc, WORD32 i4_curr_idx);

/**
********************************************************************************
*  @fn     hme_process_frm_init
*
*  @brief  HME frame level initialsation processing function
*
*  @param[in] pv_me_ctxt : ME ctxt pointer
*
*  @param[in] ps_ref_map : Reference map prms pointer
*
*  @param[in] ps_frm_prms :Pointer to frame params
*
*  @return Scale factor in Q8 format
********************************************************************************
*/

void hme_process_frm_init(
    void *pv_me_ctxt,
    hme_ref_map_t *ps_ref_map,
    hme_frm_prms_t *ps_frm_prms,
    WORD32 me_frm_id,
    WORD32 i4_num_me_frm_pllel);

void hme_coarse_process_frm_init(
    void *pv_me_ctxt, hme_ref_map_t *ps_ref_map, hme_frm_prms_t *ps_frm_prms);

/**
********************************************************************************
*  @fn     void hme_process_frm(void *pv_ctxt,
*                    hme_ref_map_t *ps_ref_map,
*                    U16 **ppu2_intra_cost,
*                    hme_frm_prms_t *ps_frm_prms);
*
*  @brief  Processes all the layers of the input, and updates the MV Banks.
*          Note that this function is not to be called if processing of a single
*          layer is desired.
*
*  @param[in,out] pv_ctxt : Pointer to context of HME
*
*  @param[in] ps_ref_map : Map structure that has for current input, lists of
*             ref pics (POC) mapping to LC, L0 and L1, and buffer ptrs as well
*             Informatino for all simulcast layers present.
*
*  @param[in] ppu2_intra_cost : array of Pointer to intra cost evaluated at an
*              8x8 level, stored in raster order. At each layer, the
*              corresponding ptr points to raster ordered array of wdxht/64,
*              wd and ht are layer width and ht respectively. Also, note that
*              ppu2_intra_cost[0] points to biggest resolution layer,
*              and from there on in decreasing order of size.
*
* @param[in]  ps_frm_prms : input frame parameters (excluding ref info) that
*             control the search complexity. Refer to hme_frm_prms_t for more
*              info regards the same.
*
*  @return   void
*******************************************************************************
*/

void hme_process_frm(
    void *pv_me_ctxt,
    pre_enc_L0_ipe_encloop_ctxt_t *ps_l0_ipe_input,
    hme_ref_map_t *ps_ref_map,
    double **ppd_intra_costs,
    hme_frm_prms_t *ps_frm_prms,
    PF_EXT_UPDATE_FXN_T pf_ext_update_fxn,
    //PF_GET_INTRA_CU_AND_COST pf_get_intra_cu_and_cost,
    void *pv_coarse_layer,
    void *pv_multi_thrd_ctxt,
    WORD32 i4_frame_parallelism_level,
    S32 thrd_id,
    S32 i4_me_frm_id);

void hme_coarse_process_frm(
    void *pv_me_ctxt,
    hme_ref_map_t *ps_ref_map,
    hme_frm_prms_t *ps_frm_prms,
    void *pv_multi_thrd_ctxt,
    WORD32 i4_ping_pong,
    void **ppv_dep_mngr_hme_sync);

void hme_discard_frm(
    void *pv_ctxt, S32 *p_pocs_to_remove, S32 i4_idr_gop_num, S32 i4_num_me_frm_pllel);

void hme_coarse_discard_frm(void *pv_me_ctxt, S32 *p_pocs_to_remove);

/**
*******************************************************************************
*  @fn     S32 hme_set_resolution(void *pv_me_ctxt,
*                                   S32 n_enc_layers,
*                                   S32 *p_wd,
*                                   S32 *p_ht
*
*  @brief  Sets up the layers based on resolution information.
*
*  @param[in, out] pv_me_ctxt : ME handle, updated with the resolution info
*
*  @param[in] n_enc_layers : Number of layers encoded
*
*  @param[in] p_wd : Pointer to an array having widths for each encode layer
*
*  @param[in] p_ht : Pointer to an array having heights for each encode layer
*
*  @return   void
*******************************************************************************
*/

void hme_set_resolution(void *pv_me_ctxt, S32 n_enc_layers, S32 *p_wd, S32 *p_ht, S32 me_frm_id);

void hme_coarse_set_resolution(void *pv_me_ctxt, S32 n_enc_layers, S32 *p_wd, S32 *p_ht);

/**
*******************************************************************************
*  @fn     WORD32 hme_get_active_pocs_list(void *pv_me_ctxt)
*
*  @brief  Returns the list of active POCs in ME ctxt
*
*  @param[in] pv_me_ctxt : handle to ME context
*
*  @param[out] p_pocs_buffered_in_me : pointer to an array which this fxn
*                                      populates with pocs active
*
*  @return   void
*******************************************************************************
*/
WORD32 hme_get_active_pocs_list(void *pv_me_ctxt, S32 i4_num_me_frm_pllel);

void hme_coarse_get_active_pocs_list(void *pv_me_ctxt, S32 *p_pocs_buffered_in_me);

S32 hme_get_blk_size(S32 use_4x4, S32 layer_id, S32 n_layers, S32 encode);

/**
********************************************************************************
*  @fn     hme_get_mv_blk_size()
*
*  @brief  returns whether blk uses 4x4 size or something else.
*
*  @param[in] enable_4x4 : input param from application to enable 4x4
*
*  @param[in] layer_id : id of current layer (0 finest)
*
*  @param[in] num_layeers : total num layers
*
*  @param[in] is_enc : Whether encoding enabled for layer
*
*  @return   1 for 4x4 blks, 0 for 8x8
********************************************************************************
*/
S32 hme_get_mv_blk_size(S32 enable_4x4, S32 layer_id, S32 num_layers, S32 is_enc);

void hme_set_refine_prms(
    void *pv_refine_prms,
    U08 u1_encode,
    S32 num_ref,
    S32 layer_id,
    S32 num_layers,
    S32 num_layers_explicit_search,
    S32 use_4x4,
    hme_frm_prms_t *ps_frm_prms,
    double **ppd_intra_costs,
    me_coding_params_t *ps_me_coding_tools);

S32 hme_coarse_find_free_descr_idx(void *pv_ctxt);

S32 hme_derive_num_layers(S32 n_enc_layers, S32 *p_wd, S32 *p_ht, S32 *p_disp_wd, S32 *p_disp_ht);

#endif /* #ifndef _HME_INTERFACE_H_ */
